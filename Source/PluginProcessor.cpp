#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
static juce::NormalisableRange<float> msRange  (5.0f,   40.0f,  0.1f);
static juce::NormalisableRange<float> centsRange(-25.0f, 25.0f, 0.1f);
static juce::NormalisableRange<float> driftRange(0.0f,   5.0f,  0.01f);
static juce::NormalisableRange<float> levelRange(-12.0f, 6.0f,  0.1f);

juce::AudioProcessorValueTreeState::ParameterLayout WaverProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> ("delay_ms",    "Delay",       msRange,    22.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("pitch_cents", "Pitch",       centsRange,  8.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("drift_ms",    "Drift",       driftRange,  1.8f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("level_db",    "Level",       levelRange, -1.5f));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("eq_enabled",  "EQ",          true));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("swap_lr",     "Swap L/R",    false));

    return layout;
}

//==============================================================================
WaverProcessor::WaverProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::mono(),   true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

bool WaverProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    const auto& in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() ||
           in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void WaverProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    pitchShifter.prepare (sampleRate);
    variableDelay.prepare (sampleRate, 60.0f);
    wetBuffer.resize (size_t (samplesPerBlock), 0.0f);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = uint32_t (samplesPerBlock);
    spec.numChannels      = 1;
    eqFilter.prepare (spec);

    // Tell Cubase how much latency we introduce so PDC keeps tracks aligned.
    // The granular pitch shifter pre-fills half its circular buffer as safety margin.
    setLatencySamples (PitchShifter::kBufSize / 2);

    updateDsp();
}

void WaverProcessor::releaseResources()
{
    wetBuffer.clear();
    wetBuffer.shrink_to_fit();
}

void WaverProcessor::reset()
{
    // Called by Cubase on transport stop, loop restart, and project load.
    // Clears all DSP buffers so stale audio doesn't bleed into the next play.
    pitchShifter.reset();
    variableDelay.reset();
    eqFilter.reset();
}

double WaverProcessor::getTailLengthSeconds() const
{
    // Report the maximum possible delay so Cubase flushes the tail at clip end.
    const float maxDelayMs = 40.0f + 5.0f; // max delay + max drift headroom
    return double (PitchShifter::kBufSize / 2) / currentSampleRate
         + maxDelayMs / 1000.0;
}

void WaverProcessor::updateDsp()
{
    const float pitchCents = apvts.getRawParameterValue ("pitch_cents")->load();
    const float delayMs    = apvts.getRawParameterValue ("delay_ms")->load();
    const float driftMs    = apvts.getRawParameterValue ("drift_ms")->load();
    const bool  eqEnabled  = apvts.getRawParameterValue ("eq_enabled")->load() > 0.5f;

    pitchShifter.setCents  (pitchCents);
    variableDelay.setParameters (delayMs, driftMs);

    if (eqEnabled)
    {
        // High-shelf cut at 4 kHz: simulates slightly different mic placement
        *eqFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            getSampleRate(), 4000.0f, 0.707f, juce::Decibels::decibelsToGain (-2.5f));
    }
}

//==============================================================================
void WaverProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (wetBuffer.size() < size_t (numSamples))
        wetBuffer.resize (size_t (numSamples));

    updateDsp();

    // Source is always channel 0 (handles both mono and stereo input)
    const float* dryData = buffer.getReadPointer (0);

    // --- Build wet (B) track ---
    pitchShifter.processBlock (dryData, wetBuffer.data(), numSamples);
    variableDelay.processBlock (wetBuffer.data(), wetBuffer.data(), numSamples);

    // Level offset
    const float levelGain = juce::Decibels::decibelsToGain (
        apvts.getRawParameterValue ("level_db")->load());
    juce::FloatVectorOperations::multiply (wetBuffer.data(), levelGain, numSamples);

    // EQ
    if (apvts.getRawParameterValue ("eq_enabled")->load() > 0.5f)
    {
        float* wetPtr = wetBuffer.data();
        juce::dsp::AudioBlock<float> block (&wetPtr, 1, size_t (numSamples));
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        eqFilter.process (ctx);
    }

    // --- Route to stereo output ---
    const bool swapLR = apvts.getRawParameterValue ("swap_lr")->load() > 0.5f;

    if (!swapLR)
    {
        // Dry → L (ch 0 already has dry), Wet → R (ch 1)
        buffer.copyFromWithRamp (1, 0, wetBuffer.data(), numSamples, 1.0f, 1.0f);
    }
    else
    {
        // Wet → L (ch 0), Dry → R (ch 1)
        buffer.copyFromWithRamp (1, 0, dryData,          numSamples, 1.0f, 1.0f);
        buffer.copyFromWithRamp (0, 0, wetBuffer.data(), numSamples, 1.0f, 1.0f);
    }
}

//==============================================================================
void WaverProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void WaverProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessorEditor* WaverProcessor::createEditor()
{
    return new WaverEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WaverProcessor();
}
