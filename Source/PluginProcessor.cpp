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

    layout.add (std::make_unique<juce::AudioParameterFloat> ("delay_ms",      "Delay",     msRange,    22.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("pitch_cents",   "Pitch",     centsRange,  8.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("drift_ms",      "Drift",     driftRange,  1.8f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("level_db",      "Level",     levelRange, -1.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("crossover_hz",  "Crossover",
                    juce::NormalisableRange<float> (60.0f, 300.0f, 1.0f), 150.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("ir_mix",        "IR Mix",
                    juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("eq_enabled",  "EQ",          true));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("ir_enabled",  "IR",          false));
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
    lowBandBuffer.setSize (1, samplesPerBlock);
    highBandBuffer.setSize (1, samplesPerBlock);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = uint32_t (samplesPerBlock);
    spec.numChannels      = 1;
    eqFilter.prepare (spec);

    lowpassFilter.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    highpassFilter.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    lowpassFilter.prepare (spec);
    highpassFilter.prepare (spec);

    irConvolution.prepare (spec);
    irWetBuffer.setSize (1, samplesPerBlock);

    // Tell Cubase how much latency we introduce so PDC keeps tracks aligned.
    // The granular pitch shifter pre-fills half its circular buffer as safety margin.
    setLatencySamples (PitchShifter::kBufSize / 2);

    updateDsp();
}

void WaverProcessor::releaseResources()
{
    wetBuffer.clear();
    wetBuffer.shrink_to_fit();
    lowBandBuffer.setSize (0, 0);
    highBandBuffer.setSize (0, 0);
    irWetBuffer.setSize    (0, 0);
}

void WaverProcessor::reset()
{
    // Called by Cubase on transport stop, loop restart, and project load.
    // Clears all DSP buffers so stale audio doesn't bleed into the next play.
    pitchShifter.reset();
    variableDelay.reset();
    eqFilter.reset();
    lowpassFilter.reset();
    highpassFilter.reset();
    irConvolution.reset();
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

    const float crossoverHz = apvts.getRawParameterValue ("crossover_hz")->load();
    lowpassFilter.setCutoffFrequency (crossoverHz);
    highpassFilter.setCutoffFrequency (crossoverHz);

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
    lowBandBuffer.setSize  (1, numSamples, false, false, true);
    highBandBuffer.setSize (1, numSamples, false, false, true);

    updateDsp();

    // Source is always channel 0 (handles both mono and stereo input)
    const float* inputData = buffer.getReadPointer (0);

    // --- Split into low band (mono, no processing) and high band (double-tracked)
    // LR4 crossover sums to flat, so low + high = original with no phase cancellation.
    lowBandBuffer.copyFrom  (0, 0, inputData, numSamples);
    highBandBuffer.copyFrom (0, 0, inputData, numSamples);

    {
        juce::dsp::AudioBlock<float> lowBlock  (lowBandBuffer);
        juce::dsp::AudioBlock<float> highBlock (highBandBuffer);
        lowpassFilter .process (juce::dsp::ProcessContextReplacing<float> (lowBlock));
        highpassFilter.process (juce::dsp::ProcessContextReplacing<float> (highBlock));
    }

    // --- Build wet (B) high band: pitch shift + variable delay + level + EQ ---
    const float* dryHigh = highBandBuffer.getReadPointer (0);
    pitchShifter.processBlock  (dryHigh,        wetBuffer.data(), numSamples);
    variableDelay.processBlock (wetBuffer.data(), wetBuffer.data(), numSamples);

    const float levelGain = juce::Decibels::decibelsToGain (
        apvts.getRawParameterValue ("level_db")->load());
    juce::FloatVectorOperations::multiply (wetBuffer.data(), levelGain, numSamples);

    if (apvts.getRawParameterValue ("eq_enabled")->load() > 0.5f)
    {
        float* wetPtr = wetBuffer.data();
        juce::dsp::AudioBlock<float> block (&wetPtr, 1, size_t (numSamples));
        eqFilter.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    // --- IR convolution (optional): simulates different mic/room on B track ---
    if (apvts.getRawParameterValue ("ir_enabled")->load() > 0.5f)
    {
        irWetBuffer.setSize (1, numSamples, false, false, true);
        irWetBuffer.copyFrom (0, 0, wetBuffer.data(), numSamples);

        juce::dsp::AudioBlock<float> irBlock (irWetBuffer);
        irConvolution.process (juce::dsp::ProcessContextReplacing<float> (irBlock));

        const float irMix = apvts.getRawParameterValue ("ir_mix")->load();
        const float dryGain = 1.0f - irMix;
        for (int i = 0; i < numSamples; ++i)
            wetBuffer[size_t (i)] = wetBuffer[size_t (i)] * dryGain
                                  + irWetBuffer.getSample (0, i) * irMix;
    }

    // --- Assemble stereo output ---
    // Both channels share the same mono low band → zero phase difference below crossover.
    // Double-tracking lives only in the high band.
    const bool swapLR        = apvts.getRawParameterValue ("swap_lr")->load() > 0.5f;
    const float* lowData     = lowBandBuffer.getReadPointer (0);
    const float* dryHighData = highBandBuffer.getReadPointer (0);
    const float* wetHighData = wetBuffer.data();

    float*       outL = buffer.getWritePointer (swapLR ? 1 : 0);
    float*       outR = buffer.getWritePointer (swapLR ? 0 : 1);

    // Soft clipper: tanh-based limiter prevents hard clipping on hot recordings.
    // Tanh saturates gracefully — starts acting around -6dBFS, hard limit at 0dBFS.
    // Factor 1.5 boosts before tanh so it starts compressing earlier on hot material,
    // then divides back so unity gain is preserved on normal levels.
    constexpr float kSoftClipDrive = 1.5f;
    constexpr float kSoftClipGain  = 1.0f / kSoftClipDrive;
    for (int i = 0; i < numSamples; ++i)
    {
        outL[i] = std::tanh (kSoftClipDrive * (lowData[i] + dryHighData[i])) * kSoftClipGain;
        outR[i] = std::tanh (kSoftClipDrive * (lowData[i] + wetHighData[i])) * kSoftClipGain;
    }
}

//==============================================================================
void WaverProcessor::loadIR (const juce::File& file)
{
    if (! file.existsAsFile()) return;

    irConvolution.loadImpulseResponse (
        file,
        juce::dsp::Convolution::Stereo::no,    // treat IR as mono
        juce::dsp::Convolution::Trim::yes,      // trim leading/trailing silence
        0,                                      // max IR length (0 = no limit)
        juce::dsp::Convolution::Normalise::yes); // normalise IR energy — prevents level spikes on hot signals

    irFilePath = file.getFullPathName();
    irFileName = file.getFileName();
}

//==============================================================================
void WaverProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (irFilePath.isNotEmpty())
        state.setProperty ("irFilePath", irFilePath, nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void WaverProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml (*xmlState);
        apvts.replaceState (tree);

        // Reload IR if this project had one saved
        const juce::String savedPath = tree.getProperty ("irFilePath", "");
        if (savedPath.isNotEmpty())
            loadIR (juce::File (savedPath));
    }
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
