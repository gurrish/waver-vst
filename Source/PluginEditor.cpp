#include "PluginEditor.h"

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
    setLookAndFeel (&laf);

    setupSlider (delaySlider,     delayLabel,     "Delay",     " ms");
    setupSlider (pitchSlider,     pitchLabel,     "Pitch",     " ct");
    setupSlider (driftSlider,     driftLabel,     "Drift",     " ms");
    setupSlider (levelSlider,     levelLabel,     "Level",     " dB");
    setupSlider (crossoverSlider, crossoverLabel, "Crossover", " Hz");

    // IR mix: compact horizontal slider
    irMixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    irMixSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (irMixSlider);

    for (auto* btn : { &eqButton, &swapButton, &irButton })
        addAndMakeVisible (btn);

    // Load IR button
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
    irFileLabel.setColour (juce::Label::textColourId, WaverColours::text3);
    irFileLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (irFileLabel);

    setSize (520, 310);
}

WaverEditor::~WaverEditor()
{
    setLookAndFeel (nullptr);
}

void WaverEditor::setupSlider (juce::Slider& s, juce::Label& l,
                                const juce::String& text,
                                const juce::String& suffix)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    s.setTextValueSuffix (suffix);
    addAndMakeVisible (s);

    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f).withStyle ("SemiBold")));
    l.setColour (juce::Label::textColourId, WaverColours::text3);
    l.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (l);
}

//==============================================================================
void WaverEditor::paint (juce::Graphics& g)
{
    using namespace WaverColours;

    // Background
    g.fillAll (bg);

    // Header panel
    const auto header = getLocalBounds().removeFromTop (44).toFloat();
    g.setColour (surface1);
    g.fillRect (header);
    g.setColour (border);
    g.fillRect (juce::Rectangle<float> (header.getX(), header.getBottom() - 1, header.getWidth(), 1.0f));

    // Logo waveform (mirrors website SVG)
    {
        const float cx = 22.0f;
        const float cy = header.getCentreY();
        g.setColour (accent);
        juce::Path wave;
        const float pts[][2] = {
            {0,0},{3,-7},{6,0},{9,7},{12,0},{15,-7},{18,0},{19.5f,3},{21,0}
        };
        wave.startNewSubPath (cx + pts[0][0], cy + pts[0][1]);
        for (int i = 1; i < 9; ++i)
            wave.lineTo (cx + pts[i][0], cy + pts[i][1]);
        g.strokePath (wave, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Plugin name
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (14.0f).withStyle ("Bold")));
    g.setColour (text1);
    g.drawText ("WAVER", 48, 0, 120, 44, juce::Justification::centredLeft);

    // Tagline (right-aligned in header)
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (10.5f)));
    g.setColour (text3);
    g.drawText ("double-track simulator",
                getWidth() - 160, 0, 150, 44, juce::Justification::centredRight);

    // Knobs section panel
    const auto kPanel = juce::Rectangle<float> (10, 48, getWidth() - 20.0f, 148);
    g.setColour (surface1);
    g.fillRoundedRectangle (kPanel, 8.0f);
    g.setColour (border);
    g.drawRoundedRectangle (kPanel, 8.0f, 1.0f);

    // Separator above IR section
    const float sepY = getHeight() - 60.0f;
    g.setColour (border);
    g.fillRect (juce::Rectangle<float> (14, sepY, getWidth() - 28.0f, 1.0f));
}

void WaverEditor::resized()
{
    const int pad = 14;
    auto area = getLocalBounds().withTrimmedTop (52).reduced (pad, 0);

    // Row 1: five knobs inside the panel
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
        label->setBounds  (cell.removeFromBottom (16));
        slider->setBounds (cell.reduced (2, 4));
    }

    // Row 2: toggles
    area.removeFromTop (10);
    auto toggleRow = area.removeFromTop (26);
    const int toggleW = toggleRow.getWidth() / 3;
    eqButton.setBounds   (toggleRow.removeFromLeft (toggleW).reduced (4, 0));
    swapButton.setBounds (toggleRow.removeFromLeft (toggleW).reduced (4, 0));
    irButton.setBounds   (toggleRow.reduced (4, 0));

    // Row 3: IR section
    area.removeFromTop (16);
    auto irRow = area.removeFromTop (28);
    loadIRButton.setBounds (irRow.removeFromLeft (84).reduced (0, 3));
    irRow.removeFromLeft (8);
    irMixSlider.setBounds  (irRow.removeFromLeft (110).reduced (0, 6));
    irRow.removeFromLeft (8);
    irFileLabel.setBounds  (irRow);
}
