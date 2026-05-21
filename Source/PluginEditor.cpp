#include "PluginEditor.h"

static const juce::Colour kBg     { 0xff1a1a2e };
static const juce::Colour kAccent { 0xff00d4aa };
static const juce::Colour kText   { 0xffdadada };
static const juce::Colour kDim    { 0xff555577 };

WaverEditor::WaverEditor (WaverProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      delayAtt     (p.apvts, "delay_ms",     delaySlider),
      pitchAtt     (p.apvts, "pitch_cents",  pitchSlider),
      driftAtt     (p.apvts, "drift_ms",     driftSlider),
      levelAtt     (p.apvts, "level_db",     levelSlider),
      crossoverAtt (p.apvts, "crossover_hz", crossoverSlider),
      irMixAtt     (p.apvts, "ir_mix",       irMixSlider),
      eqAtt        (p.apvts, "eq_enabled",   eqButton),
      swapAtt      (p.apvts, "swap_lr",      swapButton),
      irAtt        (p.apvts, "ir_enabled",   irButton)
{
    setupSlider (delaySlider,     delayLabel,     "Delay",     " ms");
    setupSlider (pitchSlider,     pitchLabel,     "Pitch",     " ct");
    setupSlider (driftSlider,     driftLabel,     "Drift",     " ms");
    setupSlider (levelSlider,     levelLabel,     "Level",     " dB");
    setupSlider (crossoverSlider, crossoverLabel, "Crossover", " Hz");

    // IR mix: compact horizontal slider
    irMixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    irMixSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    irMixSlider.setColour (juce::Slider::trackColourId,      kAccent);
    irMixSlider.setColour (juce::Slider::backgroundColourId, kDim);
    addAndMakeVisible (irMixSlider);

    for (auto* btn : { &eqButton, &swapButton, &irButton })
    {
        btn->setColour (juce::ToggleButton::textColourId,          kText);
        btn->setColour (juce::ToggleButton::tickColourId,          kAccent);
        btn->setColour (juce::ToggleButton::tickDisabledColourId,  kText.darker());
        addAndMakeVisible (btn);
    }

    // IR file load button
    loadIRButton.setColour (juce::TextButton::buttonColourId,   kDim);
    loadIRButton.setColour (juce::TextButton::textColourOnId,   kText);
    loadIRButton.setColour (juce::TextButton::textColourOffId,  kText);
    loadIRButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load IR File", juce::File::getSpecialLocation (juce::File::userHomeDirectory),
            "*.wav;*.aif;*.aiff");

        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto result = fc.getResult();
                if (result.existsAsFile())
                {
                    processor.loadIR (result);
                    irFileLabel.setText (result.getFileName(), juce::dontSendNotification);
                }
            });
    };
    addAndMakeVisible (loadIRButton);

    irFileLabel.setText (p.getIRFileName().isNotEmpty() ? p.getIRFileName() : "No IR loaded",
                         juce::dontSendNotification);
    irFileLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
    irFileLabel.setColour (juce::Label::textColourId, kText.darker (0.3f));
    irFileLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (irFileLabel);

    setSize (500, 320);
}

void WaverEditor::setupSlider (juce::Slider& s, juce::Label& l,
                                const juce::String& text,
                                const juce::String& suffix)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    s.setTextValueSuffix (suffix);
    s.setColour (juce::Slider::rotarySliderFillColourId,    kAccent);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, kText.darker (0.6f));
    s.setColour (juce::Slider::textBoxTextColourId,         kText);
    s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    addAndMakeVisible (s);

    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f)));
    l.setColour (juce::Label::textColourId, kText);
    l.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (l);
}

//==============================================================================
void WaverEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    g.setColour (kAccent);
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (20.0f).withStyle ("Bold")));
    g.drawText ("WAVER", getLocalBounds().removeFromTop (36), juce::Justification::centred);

    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
    g.setColour (kText.darker (0.4f));
    g.drawText ("double-track simulator", getLocalBounds().withTrimmedTop (26).removeFromTop (18),
                juce::Justification::centred);

    // Separator above IR section
    g.setColour (kDim);
    g.fillRect (getLocalBounds().withTrimmedTop (242).removeFromTop (1).reduced (12, 0));
}

void WaverEditor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (48).reduced (12, 4);

    // Row 1: five knobs
    const int knobW = area.getWidth() / 5;
    const int knobH = 140;
    auto knobRow = area.removeFromTop (knobH);

    for (auto [slider, label] : { std::pair {&delaySlider,     &delayLabel},
                                   std::pair {&pitchSlider,     &pitchLabel},
                                   std::pair {&driftSlider,     &driftLabel},
                                   std::pair {&levelSlider,     &levelLabel},
                                   std::pair {&crossoverSlider, &crossoverLabel} })
    {
        auto cell = knobRow.removeFromLeft (knobW);
        label->setBounds  (cell.removeFromBottom (18));
        slider->setBounds (cell);
    }

    // Row 2: toggles
    area.removeFromTop (8);
    auto toggleRow = area.removeFromTop (28);
    const int toggleW = toggleRow.getWidth() / 3;
    eqButton.setBounds   (toggleRow.removeFromLeft (toggleW).reduced (4, 2));
    swapButton.setBounds (toggleRow.removeFromLeft (toggleW).reduced (4, 2));
    irButton.setBounds   (toggleRow.reduced (4, 2));

    // Row 3: IR section (load button + mix slider + filename)
    area.removeFromTop (14);
    auto irRow = area.removeFromTop (28);
    loadIRButton.setBounds (irRow.removeFromLeft (90).reduced (0, 2));
    irRow.removeFromLeft (6);
    irMixSlider.setBounds  (irRow.removeFromLeft (120).reduced (0, 4));
    irRow.removeFromLeft (6);
    irFileLabel.setBounds  (irRow);
}
