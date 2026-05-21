#pragma once
#include <vector>

/**
 * Variable fractional delay line with IIR-smoothed random-walk modulation.
 * Matches the Python waver.py approach: slow organic drift, not a periodic LFO
 * (which would sound like chorus/flanger).
 */
class VariableDelay
{
public:
    void prepare (double sampleRate, float maxDelayMs = 60.0f);
    void setParameters (float baseDelayMs, float driftDepthMs);
    void reset();

    /** Process mono audio in-place (input == output is allowed). */
    void processBlock (const float* input, float* output, int numSamples);

    // Limit the random drift modulation to an absolute sample window [start, end)
    void setDriftWindow (uint64_t startSample, uint64_t endSample);
    void enableDriftWindow (bool shouldEnable);

private:
    std::vector<float> buf;
    int    writePos         = 0;
    double sampleRate       = 44100.0;

    float  baseDelaySamples = 0.0f;
    float  driftScaled      = 0.0f; // driftDepthSamples * normScale

    // IIR low-pass state (500 ms time constant → very slow, organic drift)
    float  alpha            = 0.0f; // IIR coefficient
    float  normScale        = 0.0f; // scales smoothed output to unit RMS
    float  smoothed         = 0.0f; // running IIR state
    float  lcg              = 0.3f; // cheap deterministic noise state

    // Absolute sample position tracking (incremented by processed samples)
    uint64_t absolutePos    = 0;
    uint64_t driftWindowStart = 200000;
    uint64_t driftWindowEnd   = 400000;
    bool     useDriftWindow   = true;

    float  readInterp (float delaySamples) const noexcept;
    float  nextNoise() noexcept;
};
