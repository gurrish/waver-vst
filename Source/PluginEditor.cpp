#include "PluginEditor.h"

static const juce::Colour kBg     { 0xff1a1a2e };
static const juce::Colour kAccent { 0xff00d4aa };
static const juce::Colour kText   { 0xffdadada };

WaverEditor::WaverEditor (WaverProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      delayAtt (p.apvts, "delay_ms",    delaySlider),
      pitchAtt (p.apvts, "pitch_cents", pitchSlider),
      driftAtt (p.apvts, "drift_ms",    driftSlider),
      levelAtt (p.apvts, "level_db",    levelSlider),
      eqAtt    (p.apvts, "eq_enabled",  eqButton),
      swapAtt  (p.apvts, "swap_lr",     swapButton)
{
    setupSlider (delaySlider, delayLabel, "Delay",  " ms");
    setupSlider (pitchSlider, pitchLabel, "Pitch",  " ct");
    setupSlider (driftSlider, driftLabel, "Drift",  " ms");
    setupSlider (levelSlider, levelLabel, "Level",  " dB");

    for (auto* btn : { &eqButton, &swapButton })
    {
        btn->setColour (juce::ToggleButton::textColourId,   kText);
        btn->setColour (juce::ToggleButton::tickColourId,   kAccent);
        btn->setColour (juce::ToggleButton::tickDisabledColourId, kText.darker());
        addAndMakeVisible (btn);
    }

    setSize (420, 240);
}

void WaverEditor::setupSlider (juce::Slider& s, juce::Label& l,
                                const juce::String& text,
                                const juce::String& suffix)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    s.setTextValueSuffix (suffix);
    s.setColour (juce::Slider::rotarySliderFillColourId,  kAccent);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, kText.darker (0.6f));
    s.setColour (juce::Slider::textBoxTextColourId, kText);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (s);

    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::Font (12.0f));
    l.setColour (juce::Label::textColourId, kText);
    l.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (l);
}

//==============================================================================
void WaverEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    g.setColour (kAccent);
    g.setFont (juce::Font (20.0f, juce::Font::bold));
    g.drawText ("WAVER", getLocalBounds().removeFromTop (36), juce::Justification::centred);

    g.setFont (juce::Font (11.0f));
    g.setColour (kText.darker (0.4f));
    g.drawText ("double-track simulator", getLocalBounds().withTrimmedTop (26).removeFromTop (18),
                juce::Justification::centred);
}

void WaverEditor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (48).reduced (12, 4);

    // Four knobs side by side
    const int knobW = area.getWidth() / 4;
    const int knobH = 140;
    auto knobRow = area.removeFromTop (knobH);

    for (auto [slider, label] : { std::pair {&delaySlider, &delayLabel},
                                   std::pair {&pitchSlider, &pitchLabel},
                                   std::pair {&driftSlider, &driftLabel},
                                   std::pair {&levelSlider, &levelLabel} })
    {
        auto cell = knobRow.removeFromLeft (knobW);
        label->setBounds  (cell.removeFromBottom (18));
        slider->setBounds (cell);
    }

    // Toggle buttons
    area.removeFromTop (8);
    const int btnW = area.getWidth() / 2;
    eqButton.setBounds   (area.removeFromLeft (btnW).reduced (8, 4));
    swapButton.setBounds (area.reduced (8, 4));
}
