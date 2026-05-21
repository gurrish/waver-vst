#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ── Design tokens (mirror website) ───────────────────────────────────────────
namespace WaverColours
{
    inline const juce::Colour bg       { 0xff080808 };
    inline const juce::Colour surface1 { 0xff111111 };
    inline const juce::Colour surface2 { 0xff1a1a1a };
    inline const juce::Colour surface3 { 0xff222222 };
    inline const juce::Colour accent   { 0xffff5e14 };
    inline const juce::Colour text1    { 0xfff0f0f0 };
    inline const juce::Colour text2    { 0xff9a9a9a };
    inline const juce::Colour text3    { 0xff555555 };
    inline const juce::Colour border   { 0xff1e1e1e };
}

// ── Custom LookAndFeel ────────────────────────────────────────────────────────
class WaverLookAndFeel : public juce::LookAndFeel_V4
{
public:
    WaverLookAndFeel()
    {
        // Global text colours
        setColour (juce::Label::textColourId,               WaverColours::text2);
        setColour (juce::Slider::textBoxTextColourId,        WaverColours::text2);
        setColour (juce::Slider::textBoxOutlineColourId,     juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId,  juce::Colours::transparentBlack);
        setColour (juce::ToggleButton::textColourId,         WaverColours::text2);
        setColour (juce::ToggleButton::tickColourId,         WaverColours::accent);
        setColour (juce::ToggleButton::tickDisabledColourId, WaverColours::text3);
    }

    // ── Rotary knob ───────────────────────────────────────────────────────────
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        using namespace juce;
        const float r      = jmin (width * 0.5f, height * 0.5f) - 7.0f;
        const float cx     = x + width  * 0.5f;
        const float cy     = y + height * 0.5f;
        const float angle  = startAngle + sliderPos * (endAngle - startAngle);
        const float trackW = 2.2f;
        const float arcX   = cx - r; const float arcY = cy - r;
        const float arcD   = r * 2.0f;

        // Track arc (background)
        Path track;
        track.addArc (arcX, arcY, arcD, arcD, startAngle, endAngle, true);
        g.setColour (WaverColours::surface3);
        g.strokePath (track, PathStrokeType (trackW, PathStrokeType::curved, PathStrokeType::rounded));

        // Value arc (orange)
        if (sliderPos > 0.0f)
        {
            Path fill;
            fill.addArc (arcX, arcY, arcD, arcD, startAngle, angle, true);
            g.setColour (WaverColours::accent);
            g.strokePath (fill, PathStrokeType (trackW, PathStrokeType::curved, PathStrokeType::rounded));
        }

        // Knob body — two layered circles for depth
        const float kr = r - 5.5f;
        g.setColour (WaverColours::surface1);
        g.fillEllipse (cx - kr, cy - kr, kr * 2.0f, kr * 2.0f);
        g.setColour (WaverColours::surface2);
        g.fillEllipse (cx - kr + 1.0f, cy - kr + 0.5f, (kr - 0.5f) * 2.0f, (kr - 1.5f) * 2.0f);

        // Indicator line — white, from ~40% radius to ~85% radius
        const float lineInR  = kr * 0.38f;
        const float lineOutR = kr * 0.82f;
        const float sinA = std::sin (angle);
        const float cosA = std::cos (angle);
        g.setColour (WaverColours::text1);
        g.drawLine (cx + lineInR * sinA, cy - lineInR * cosA,
                    cx + lineOutR * sinA, cy - lineOutR * cosA, 1.8f);
    }

    // ── Toggle button — pill style ────────────────────────────────────────────
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& btn,
                           bool highlighted, bool /*down*/) override
    {
        using namespace juce;
        const bool on = btn.getToggleState();
        const auto bounds = btn.getLocalBounds().toFloat().reduced (0.5f);
        const float r = bounds.getHeight() * 0.5f;

        // Background
        g.setColour (on ? WaverColours::accent.withAlpha (0.18f) : WaverColours::surface2);
        g.fillRoundedRectangle (bounds, r);

        // Border
        g.setColour (on ? WaverColours::accent.withAlpha (0.55f)
                        : (highlighted ? WaverColours::text3 : WaverColours::border));
        g.drawRoundedRectangle (bounds, r, 1.0f);

        // Label
        g.setColour (on ? WaverColours::accent : (highlighted ? WaverColours::text2 : WaverColours::text3));
        g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.5f).withStyle ("SemiBold")));
        g.drawText (btn.getButtonText(), bounds.toNearestInt(), Justification::centred, false);
    }

    // ── Text button (Load IR) ─────────────────────────────────────────────────
    void drawButtonBackground (juce::Graphics& g, juce::Button& btn,
                               const juce::Colour& /*bgColour*/,
                               bool highlighted, bool down) override
    {
        using namespace juce;
        const auto bounds = btn.getLocalBounds().toFloat().reduced (0.5f);
        const float r = 5.0f;

        g.setColour (down      ? WaverColours::accent.withAlpha (0.25f)
                   : highlighted ? WaverColours::surface3
                                 : WaverColours::surface2);
        g.fillRoundedRectangle (bounds, r);

        g.setColour (highlighted ? WaverColours::accent.withAlpha (0.55f)
                                 : WaverColours::border);
        g.drawRoundedRectangle (bounds, r, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                         bool highlighted, bool /*down*/) override
    {
        g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.5f).withStyle ("SemiBold")));
        g.setColour (highlighted ? WaverColours::accent : WaverColours::text2);
        g.drawText (btn.getButtonText(), btn.getLocalBounds(), juce::Justification::centred, false);
    }

    // ── Horizontal slider (IR mix) ────────────────────────────────────────────
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float /*minPos*/, float /*maxPos*/,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal) {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height,
                                              sliderPos, 0, 0, style, slider);
            return;
        }
        const float cy   = y + height * 0.5f;
        const float trackH = 3.0f;
        // Track background
        g.setColour (WaverColours::surface3);
        g.fillRoundedRectangle ((float)x, cy - trackH * 0.5f, (float)width, trackH, trackH * 0.5f);
        // Track fill
        g.setColour (WaverColours::accent);
        g.fillRoundedRectangle ((float)x, cy - trackH * 0.5f, sliderPos - x, trackH, trackH * 0.5f);
        // Thumb
        const float tr = 6.0f;
        g.setColour (WaverColours::text1);
        g.fillEllipse (sliderPos - tr, cy - tr, tr * 2.0f, tr * 2.0f);
        g.setColour (WaverColours::accent);
        g.fillEllipse (sliderPos - tr + 2.0f, cy - tr + 2.0f, (tr - 2.0f) * 2.0f, (tr - 2.0f) * 2.0f);
    }
};

// ── Editor ────────────────────────────────────────────────────────────────────
class WaverEditor  : public juce::AudioProcessorEditor
{
public:
    explicit WaverEditor (WaverProcessor&);
    ~WaverEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    WaverProcessor& processor;
    WaverLookAndFeel laf;

    // Sliders
    juce::Slider delaySlider, pitchSlider, driftSlider, levelSlider, irMixSlider;
    juce::Label  delayLabel, pitchLabel, driftLabel, levelLabel, irMixLabel;

    // Toggles
    juce::ToggleButton eqButton   { "EQ tilt" };
    juce::ToggleButton irButton   { "IR" };

    // IR section
    juce::TextButton  loadIRButton { "Load IR..." };
    juce::Label       irFileLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // APVTS attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    SliderAttachment delayAtt, pitchAtt, driftAtt, levelAtt, irMixAtt;
    ButtonAttachment eqAtt, irAtt;

    void setupSlider (juce::Slider& s, juce::Label& l,
                      const juce::String& text, const juce::String& suffix);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaverEditor)
};
