#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <vector>

// Doesit Pitch: koerniger Pitch-Shifter (+/-24 Halbtoene), Tempo bleibt gleich.
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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float readAt(const std::vector<float>& b, float delay) const;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>* pitchParam = nullptr;
    std::atomic<float>* grainParam = nullptr;
    std::atomic<float>* mixParam = nullptr;

    std::vector<std::vector<float>> delayBuf;
    int bufSize = 0;
    int bufMask = 0;
    int writePos = 0;
    float phase = 0.0f;
    double currentSampleRate = 44100.0;
    int reportedLatency = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DoesitPitchProcessor)
};
