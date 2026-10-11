#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <cstdint>
#include <vector>

// Doesit Pitch v0.2: koerniger Pitch-Shifter (+/-24 Halbtoene plus Fine in Cent),
// Tempo bleibt gleich. Dazu Sampler-Charakter (Bittiefe, Samplerate, Filter) und Warm-Saturation.
class DoesitPitchProcessor : public juce::AudioProcessor
{
public:
    DoesitPitchProcessor();
    ~DoesitPitchProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Doesit Pitch"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getApvts() { return apvts; }

private:
    struct ChannelState
    {
        float aa1 = 0.0f, aa2 = 0.0f;   // Anti-Alias-Filter vor der Abtastung
        float hold = 0.0f;              // Sample and Hold
        float rc1 = 0.0f, rc2 = 0.0f;   // Rekonstruktionsfilter nach der Wandlung
        float dcX = 0.0f, dcY = 0.0f;   // DC-Blocker (Warm)
        float warmLP = 0.0f;            // Hoehendaempfung (Warm)
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float readAt(const std::vector<float>& b, float delay) const;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>* pitchParam = nullptr;
    std::atomic<float>* fineParam = nullptr;
    std::atomic<float>* grainParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* machineParam = nullptr;
    std::atomic<float>* rateParam = nullptr;
    std::atomic<float>* warmParam = nullptr;

    std::vector<std::vector<float>> delayBuf;
    std::vector<ChannelState> chState;
    int bufSize = 0;
    int bufMask = 0;
    int writePos = 0;
    float phase = 0.0f;
    float srAcc = 0.0f;
    std::uint32_t rng = 22222u;
    double currentSampleRate = 44100.0;
    int reportedLatency = -1;
    int latencyFixed = 0;      // feste Latenz in Samples, wird einmal gemeldet
    float grainSmooth = 0.0f;  // geglaettete Grain-Groesse in Samples
    float grainCoef = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DoesitPitchProcessor)
};
