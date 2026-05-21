#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class WaverEditor  : public juce::AudioProcessorEditor
{
public:
    explicit WaverEditor (WaverProcessor&);
    ~WaverEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    WaverProcessor& processor;

    // Sliders
    juce::Slider delaySlider, pitchSlider, driftSlider, levelSlider;
    juce::Label  delayLabel, pitchLabel, driftLabel, levelLabel;

    // Toggles
    juce::ToggleButton eqButton  { "EQ tilt" };
    juce::ToggleButton swapButton { "Swap L/R" };

    // APVTS attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    SliderAttachment delayAtt, pitchAtt, driftAtt, levelAtt;
    ButtonAttachment eqAtt, swapAtt;

    void setupSlider (juce::Slider& s, juce::Label& l,
                      const juce::String& text, const juce::String& suffix);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaverEditor)
};
