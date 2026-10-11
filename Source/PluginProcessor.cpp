#include "PluginProcessor.h"
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout DoesitPitchProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "pitch", 1 }, "Pitch",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("st")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "grain", 1 }, "Grain",
        juce::NormalisableRange<float>(10.0f, 100.0f, 0.1f), 40.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

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
    pitchParam = apvts.getRawParameterValue("pitch");
    grainParam = apvts.getRawParameterValue("grain");
    mixParam   = apvts.getRawParameterValue("mix");
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
    writePos = 0;
    phase = 0.0f;
    reportedLatency = -1;
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

    const float semis = pitchParam->load();
    const float grainMs = grainParam->load();
    const float mix = mixParam->load();

    const float ratio = std::pow(2.0f, semis / 12.0f);
    const float grain = (float) (grainMs * 0.001 * currentSampleRate);
    const int latency = (int) (grain * 0.5f);

    if (latency != reportedLatency)
    {
        setLatencySamples(latency);
        reportedLatency = latency;
    }

    const bool pitchActive = std::abs(semis) >= 0.001f;
    const float phaseInc = (1.0f - ratio) / grain;
    const float pi = 3.14159265358979f;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numCh; ++ch)
            delayBuf[(size_t) ch][(size_t) writePos] = buffer.getReadPointer(ch)[i];

        const float pB = (phase + 0.5f >= 1.0f) ? phase - 0.5f : phase + 0.5f;
        const float sA = std::sin(pi * phase);
        const float wA = sA * sA;
        const float wB = 1.0f - wA;
        const float dA = phase * grain;
        const float dB = pB * grain;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const auto& b = delayBuf[(size_t) ch];
            const float dry = readAt(b, (float) latency);
            const float wet = pitchActive ? (readAt(b, dA) * wA + readAt(b, dB) * wB) : dry;
            buffer.getWritePointer(ch)[i] = dry * (1.0f - mix) + wet * mix;
        }

        phase += phaseInc;
        while (phase >= 1.0f) phase -= 1.0f;
        while (phase < 0.0f)  phase += 1.0f;

        writePos = (writePos + 1) & bufMask;
    }
}

juce::AudioProcessorEditor* DoesitPitchProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
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
