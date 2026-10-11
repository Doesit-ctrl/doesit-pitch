#include "PluginEditor.h"

namespace
{
    const juce::Colour kBg      { 0xff101c36 };
    const juce::Colour kPanel   { 0xff1b2a4d };
    const juce::Colour kText    { 0xfff3efe4 };
    const juce::Colour kAccent  { 0xff8fb8ff };
}

DoesitPitchEditor::DoesitPitchEditor(DoesitPitchProcessor& p)
    : juce::AudioProcessorEditor(&p), processor(p)
{
    laf.setColour(juce::PopupMenu::backgroundColourId, kPanel);
    laf.setColour(juce::PopupMenu::textColourId, kText);
    laf.setColour(juce::PopupMenu::highlightedBackgroundColourId, kAccent);
    laf.setColour(juce::PopupMenu::highlightedTextColourId, kBg);
    laf.setColour(juce::TextEditor::backgroundColourId, kPanel);
    laf.setColour(juce::TextEditor::textColourId, kText);
    laf.setColour(juce::TextEditor::highlightColourId, kAccent);
    laf.setColour(juce::TextEditor::highlightedTextColourId, kBg);
    laf.setColour(juce::CaretComponent::caretColourId, kText);
    setLookAndFeel(&laf);

    setWantsKeyboardFocus(true);

    auto& apvts = processor.getApvts();

    setupSlider(sPitch, lPitch, "Pitch (st)");
    setupSlider(sFine,  lFine,  "Fine (ct)");
    setupSlider(sGrain, lGrain, "Grain (ms)");
    setupSlider(sWarm,  lWarm,  "Warm");
    setupSlider(sMix,   lMix,   "Mix");

    setupCombo(cMachine, lMachine, "Machine", "machine");
    setupCombo(cRate,    lRate,    "Rate",    "rate");

    aPitch = std::make_unique<SliderAttachment>(apvts, "pitch", sPitch);
    aFine  = std::make_unique<SliderAttachment>(apvts, "fine",  sFine);
    aGrain = std::make_unique<SliderAttachment>(apvts, "grain", sGrain);
    aWarm  = std::make_unique<SliderAttachment>(apvts, "warm",  sWarm);
    aMix   = std::make_unique<SliderAttachment>(apvts, "mix",   sMix);
    aMachine = std::make_unique<ComboAttachment>(apvts, "machine", cMachine);
    aRate    = std::make_unique<ComboAttachment>(apvts, "rate",    cRate);

    sPitch.setDoubleClickReturnValue(true, 0.0);
    sFine.setDoubleClickReturnValue(true, 0.0);
    sGrain.setDoubleClickReturnValue(true, 40.0);
    sWarm.setDoubleClickReturnValue(true, 0.0);
    sMix.setDoubleClickReturnValue(true, 1.0);

    setSize(480, 400);
}

DoesitPitchEditor::~DoesitPitchEditor()
{
    setLookAndFeel(nullptr);
}

void DoesitPitchEditor::setupSlider(juce::Slider& s, juce::Label& l, const juce::String& text)
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 84, 26);
    s.setTextBoxIsEditable(true);
    s.setColour(juce::Slider::trackColourId, kAccent);
    s.setColour(juce::Slider::backgroundColourId, kPanel);
    s.setColour(juce::Slider::thumbColourId, kText);
    s.setColour(juce::Slider::textBoxTextColourId, kText);
    s.setColour(juce::Slider::textBoxBackgroundColourId, kPanel);
    s.setColour(juce::Slider::textBoxOutlineColourId, kAccent.withAlpha(0.4f));
    addAndMakeVisible(s);

    l.setText(text, juce::dontSendNotification);
    l.setColour(juce::Label::textColourId, kText);
    addAndMakeVisible(l);
}

void DoesitPitchEditor::setupCombo(juce::ComboBox& c, juce::Label& l, const juce::String& text,
                                   const juce::String& paramId)
{
    c.addItemList(processor.getApvts().getParameter(paramId)->getAllValueStrings(), 1);
    c.setColour(juce::ComboBox::backgroundColourId, kPanel);
    c.setColour(juce::ComboBox::textColourId, kText);
    c.setColour(juce::ComboBox::outlineColourId, kAccent.withAlpha(0.4f));
    c.setColour(juce::ComboBox::arrowColourId, kText);
    addAndMakeVisible(c);

    l.setText(text, juce::dontSendNotification);
    l.setColour(juce::Label::textColourId, kText);
    addAndMakeVisible(l);
}

void DoesitPitchEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
    g.setColour(kText);
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.drawText("DOESIT PITCH", 20, 10, getWidth() - 40, 30, juce::Justification::centredLeft);
}

void DoesitPitchEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(36);

    const int rowH = 40;
    const int labelW = 90;

    auto addRow = [&](juce::Label& l, juce::Component& c)
    {
        auto row = area.removeFromTop(rowH);
        l.setBounds(row.removeFromLeft(labelW));
        c.setBounds(row.reduced(0, 6));
    };

    addRow(lPitch, sPitch);
    addRow(lFine, sFine);
    addRow(lGrain, sGrain);
    addRow(lMachine, cMachine);
    addRow(lRate, cRate);
    addRow(lWarm, sWarm);
    addRow(lMix, sMix);
}
