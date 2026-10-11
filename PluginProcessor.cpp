#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
    const float kPi = 3.14159265358979f;

    // Index 0 = Off. Bittiefe und maximale Samplerate laut Herstellerangaben.
    struct MachineSpec { int bits; float maxRate; };
    const MachineSpec kMachines[] = {
        { 24, 0.0f },        // Off
        { 16, 32000.0f },    // S20
        { 12, 48000.0f },    // S950
        { 16, 44100.0f }     // S3000
    };

    // Rauschboden: theoretisches Quantisierungsrauschen plus dieser Aufschlag in dB.
    // Das ist meine Schaetzung, damit der Unterschied zwischen 12 und 16 Bit hoerbar wird.
    const float kNoiseBoostDb = 18.0f;

    // Index 0 = Auto (Maximalrate der gewaehlten Maschine)
    const float kRates[] = { 0.0f, 48000.0f, 44100.0f, 40000.0f, 32000.0f, 22050.0f,
                             16000.0f, 11025.0f, 8000.0f, 7500.0f, 4000.0f };
}

juce::AudioProcessorValueTreeState::ParameterLayout DoesitPitchProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ "pitch", 1 }, "Pitch", -24, 24, 0,
        juce::AudioParameterIntAttributes().withLabel("st")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "fine", 1 }, "Fine",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("ct")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "grain", 1 }, "Grain",
        juce::NormalisableRange<float>(10.0f, 100.0f, 0.1f), 40.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "machine", 1 }, "Machine",
        juce::StringArray{ "Off", "S20", "S950", "S3000" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "rate", 1 }, "Rate",
        juce::StringArray{ "Auto", "48 kHz", "44.1 kHz", "40 kHz", "32 kHz", "22.05 kHz",
                           "16 kHz", "11.025 kHz", "8 kHz", "7.5 kHz", "4 kHz" }, 0));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "warm", 1 }, "Warm",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "mix", 1 }, "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    return layout;
}

DoesitPitchProcessor::DoesitPitchProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", createLayout())
{
    pitchParam   = apvts.getRawParameterValue("pitch");
    fineParam    = apvts.getRawParameterValue("fine");
    grainParam   = apvts.getRawParameterValue("grain");
    mixParam     = apvts.getRawParameterValue("mix");
    machineParam = apvts.getRawParameterValue("machine");
    rateParam    = apvts.getRawParameterValue("rate");
    warmParam    = apvts.getRawParameterValue("warm");
}

bool DoesitPitchProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void DoesitPitchProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;
    bufSize = juce::nextPowerOfTwo((int) (sampleRate * 0.25));
    bufMask = bufSize - 1;
    const int channels = juce::jmax(1, getTotalNumInputChannels());
    delayBuf.assign((size_t) channels, std::vector<float>((size_t) bufSize, 0.0f));
    chState.assign((size_t) channels, ChannelState());
    writePos = 0;
    phase = 0.0f;
    srAcc = 0.0f;
    reportedLatency = -1;

    // Feste Latenz (halbe maximale Grain-Groesse). Sie aendert sich nie waehrend des Spielens,
    // damit FL die Latenzkompensation nicht neu starten muss.
    latencyFixed = (int) std::ceil(0.05 * sampleRate);
    setLatencySamples(latencyFixed);

    grainSmooth = grainParam->load() * 0.001f * (float) sampleRate;
    grainCoef = 1.0f - std::exp(-1.0f / (0.03f * (float) sampleRate));
}

float DoesitPitchProcessor::readAt(const std::vector<float>& b, float delay) const
{
    float rp = (float) writePos - delay;
    while (rp < 0.0f)
        rp += (float) bufSize;

    int i0 = (int) rp;
    const float frac = rp - (float) i0;
    const int i1 = (i0 + 1) & bufMask;
    i0 &= bufMask;
    return b[(size_t) i0] * (1.0f - frac) + b[(size_t) i1] * frac;
}

void DoesitPitchProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin(getTotalNumInputChannels(), (int) delayBuf.size());
    const int numSamples = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);

    const float semis = pitchParam->load() + fineParam->load() / 100.0f;
    const float grainMs = grainParam->load();
    const float mix = mixParam->load();
    const float warm = warmParam->load();
    const int machine = juce::jlimit(0, 3, (int) (machineParam->load() + 0.5f));
    const int rateIdx = juce::jlimit(0, 10, (int) (rateParam->load() + 0.5f));
    const float sr = (float) currentSampleRate;

    // Pitch
    const float ratio = std::pow(2.0f, semis / 12.0f);
    const float grainTarget = juce::jmin(grainMs * 0.001f * sr, 2.0f * (float) latencyFixed);
    const bool pitchActive = std::abs(semis) >= 0.001f;

    // Sampler-Charakter
    auto onePole = [sr](float fc)
    {
        fc = juce::jmin(fc, sr * 0.45f);
        return 1.0f - std::exp(-2.0f * kPi * fc / sr);
    };
    const bool machineActive = machine > 0;
    float targetRate = kMachines[machine].maxRate;
    if (machineActive && rateIdx > 0)
        targetRate = kRates[rateIdx];
    const bool rateActive = machineActive && targetRate > 0.0f && targetRate < sr * 0.999f;
    const float rateStep = rateActive ? targetRate / sr : 0.0f;
    const float levels = (float) (1 << (kMachines[machine].bits - 1));
    const float aaA = rateActive ? onePole(0.40f * targetRate) : 0.0f;
    const float rcA = rateActive ? onePole(0.45f * targetRate) : 0.0f;
    const float noiseAmp = machineActive
        ? std::pow(10.0f, (-(6.02f * (float) kMachines[machine].bits + 1.76f) + kNoiseBoostDb) / 20.0f) * 1.7320508f
        : 0.0f;

    // Warm
    const bool warmActive = warm > 0.001f;
    const float drive = 1.0f + 8.0f * warm;
    const float bias = 0.20f * warm;
    const float tb = std::tanh(bias);
    const float norm = 1.0f / (drive * (1.0f - tb * tb));
    const float dcR = 1.0f - 2.0f * kPi * 20.0f / sr;
    const float warmA = onePole(18000.0f * std::pow(8000.0f / 18000.0f, warm));

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numCh; ++ch)
            delayBuf[(size_t) ch][(size_t) writePos] = buffer.getReadPointer(ch)[i];

        // Grain-Groesse weich nachfuehren, damit Automation nicht ruckelt
        grainSmooth += grainCoef * (grainTarget - grainSmooth);
        const float grain = juce::jmax(1.0f, grainSmooth);
        const float offset = juce::jmax(0.0f, (float) latencyFixed - grain * 0.5f);
        const float phaseInc = (1.0f - ratio) / grain;

        const float pB = (phase + 0.5f >= 1.0f) ? phase - 0.5f : phase + 0.5f;
        const float sA = std::sin(kPi * phase);
        const float gA = sA * sA;
        const float gB = 1.0f - gA;
        const float dA = phase * grain + offset;
        const float dB = pB * grain + offset;

        bool tick = false;
        if (rateActive)
        {
            srAcc += rateStep;
            if (srAcc >= 1.0f)
            {
                srAcc -= 1.0f;
                tick = true;
            }
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            const auto& b = delayBuf[(size_t) ch];
            auto& st = chState[(size_t) ch];

            const float dry = readAt(b, (float) latencyFixed);
            float s = pitchActive ? (readAt(b, dA) * gA + readAt(b, dB) * gB) : dry;

            if (machineActive)
            {
                if (rateActive)
                {
                    st.aa1 += aaA * (s - st.aa1);
                    st.aa2 += aaA * (st.aa1 - st.aa2);
                    if (tick)
                        st.hold = st.aa2;
                    s = st.hold;
                }

                if (noiseAmp > 0.0f)
                {
                    rng = rng * 1664525u + 1013904223u;
                    const float n = (float) (rng >> 8) * (1.0f / 8388608.0f) - 1.0f;
                    s += n * noiseAmp;
                }

                s = juce::jlimit(-1.0f, 1.0f, s);
                s = std::round(s * levels) / levels;

                if (rateActive)
                {
                    st.rc1 += rcA * (s - st.rc1);
                    st.rc2 += rcA * (st.rc1 - st.rc2);
                    s = st.rc2;
                }
            }

            if (warmActive)
            {
                const float y = (std::tanh(s * drive + bias) - tb) * norm;
                const float d = y - st.dcX + dcR * st.dcY;
                st.dcX = y;
                st.dcY = d;
                st.warmLP += warmA * (d - st.warmLP);
                s = st.warmLP;
            }

            buffer.getWritePointer(ch)[i] = dry * (1.0f - mix) + s * mix;
        }

        phase += phaseInc;
        while (phase >= 1.0f) phase -= 1.0f;
        while (phase < 0.0f)  phase += 1.0f;

        writePos = (writePos + 1) & bufMask;
    }
}

juce::AudioProcessorEditor* DoesitPitchProcessor::createEditor()
{
    return new DoesitPitchEditor(*this);
}

void DoesitPitchProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void DoesitPitchProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DoesitPitchProcessor();
}
