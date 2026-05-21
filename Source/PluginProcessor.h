#pragma once
#include <JuceHeader.h>
#include "DSP/PitchShifter.h"
#include "DSP/VariableDelay.h"

class WaverProcessor  : public juce::AudioProcessor
{
public:
    WaverProcessor();
    ~WaverProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported (const BusesLayout&) const override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Waver"; }

    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    double currentSampleRate = 44100.0;

    PitchShifter  pitchShifter;
    VariableDelay variableDelay;

    juce::dsp::IIR::Filter<float> eqFilter;

    // Linkwitz-Riley crossover: low band stays mono (no phase cancellation),
    // double-tracking is applied only to the high band.
    juce::dsp::LinkwitzRileyFilter<float> lowpassFilter, highpassFilter;
    juce::AudioBuffer<float> lowBandBuffer, highBandBuffer;

    std::vector<float> wetBuffer;

    void updateDsp();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaverProcessor)
};
