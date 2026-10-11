#pragma once
#include "PluginProcessor.h"

class DoesitPitchEditor : public juce::AudioProcessorEditor
{
public:
    explicit DoesitPitchEditor(DoesitPitchProcessor& p);
    ~DoesitPitchEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void setupSlider(juce::Slider& s, juce::Label& l, const juce::String& text);
    void setupCombo(juce::ComboBox& c, juce::Label& l, const juce::String& text, const juce::String& paramId);

    DoesitPitchProcessor& processor;
    juce::LookAndFeel_V4 laf;

    juce::Label lPitch, lFine, lGrain, lMachine, lRate, lWarm, lMix;
    juce::Slider sPitch, sFine, sGrain, sWarm, sMix;
    juce::ComboBox cMachine, cRate;

    std::unique_ptr<SliderAttachment> aPitch, aFine, aGrain, aWarm, aMix;
    std::unique_ptr<ComboAttachment> aMachine, aRate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DoesitPitchEditor)
};
