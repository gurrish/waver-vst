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
    layout.add (std::make_unique<juce::AudioParameterFloat> ("ir_mix",        "IR Mix",
                    juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("eq_enabled",  "EQ",          true));
    layout.add (std::make_unique<juce::AudioParameterBool>  ("ir_enabled",  "IR",          false));

    return layout;
}

//==============================================================================
WaverProcessor::WaverProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::mono(),   true)
                          .withOutput ("Output", juce::AudioChannelSet::mono(),   true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

bool WaverProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
        return false;

    const auto& in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono();
}

//==============================================================================
void WaverProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    pitchShifter.prepare (sampleRate);
    variableDelay.prepare (sampleRate, 60.0f);
    // Limit random drift to the absolute sample window [200000, 400000)
    variableDelay.setDriftWindow (200000, 400000);
    variableDelay.enableDriftWindow (true);

    wetBuffer.resize (size_t (samplesPerBlock), 0.0f);

    // Internal short IR coefficients (FIR-style). Small, short impulse response.
    internalIRCoeffs = { 0.6f, -0.35f, 0.2f, -0.12f, 0.06f };
    internalIRHistory.clear();

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = uint32_t (samplesPerBlock);
    spec.numChannels      = 1;
    eqFilter.prepare (spec);

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
    irWetBuffer.setSize    (0, 0);
    internalIRHistory.clear();
}

void WaverProcessor::reset()
{
    // Called by Cubase on transport stop, loop restart, and project load.
    // Clears all DSP buffers so stale audio doesn't bleed into the next play.
    pitchShifter.reset();
    variableDelay.reset();
    eqFilter.reset();
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

    // Source is always channel 0 (plugin supports only mono in/out)
    const float* inputData = buffer.getReadPointer (0);

    // Build wet buffer by processing the entire input (no dry/original mixed in)
    pitchShifter.processBlock  (inputData,        wetBuffer.data(), numSamples);
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

    // --- Internal short IR (always applied) ---
    if (! internalIRCoeffs.empty())
    {
        const float mix = internalIRMix;
        auto &hist = internalIRHistory;
        const int L = int (internalIRCoeffs.size());
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = -wetBuffer[size_t (i)]; // flip polarity before IR
            float y = internalIRCoeffs[0] * x;
            const int available = int (hist.size());
            for (int j = 1; j < L; ++j)
            {
                if (j - 1 < available)
                    y += internalIRCoeffs[size_t (j)] * hist[size_t (j - 1)];
            }

            // update history (most recent at front)
            hist.push_front (x);
            if (int (hist.size()) > L - 1)
                hist.pop_back();

            // mix IR output back into wet buffer
            wetBuffer[size_t (i)] = wetBuffer[size_t (i)] * (1.0f - mix) + y * mix;
        }
    }

    // IR convolution (optional) — mix IR only with processed wet signal (no dry/original)
    if (apvts.getRawParameterValue ("ir_enabled")->load() > 0.5f)
    {
        irWetBuffer.setSize (1, numSamples, false, false, true);
        irWetBuffer.copyFrom (0, 0, wetBuffer.data(), numSamples);

        juce::dsp::AudioBlock<float> irBlock (irWetBuffer);
        irConvolution.process (juce::dsp::ProcessContextReplacing<float> (irBlock));

        const float irMix = apvts.getRawParameterValue ("ir_mix")->load();
        const float wetGain = 1.0f - irMix;
        for (int i = 0; i < numSamples; ++i)
            wetBuffer[size_t (i)] = wetBuffer[size_t (i)] * wetGain
                                  + irWetBuffer.getSample (0, i) * irMix;
    }

    // Write mono output (do not mix in original dry signal)
    float* out = buffer.getWritePointer (0);

    // Soft clipper: tanh-based limiter prevents hard clipping on hot recordings.
    constexpr float kSoftClipDrive = 1.5f;
    constexpr float kSoftClipGain  = 1.0f / kSoftClipDrive;
    for (int i = 0; i < numSamples; ++i)
        out[i] = std::tanh (kSoftClipDrive * wetBuffer[size_t (i)]) * kSoftClipGain;
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
