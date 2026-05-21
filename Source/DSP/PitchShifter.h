#pragma once
#include <cmath>

/**
 * Granular pitch shifter using a dual-pointer circular buffer with sin²
 * crossfade. Works correctly at any pitch ratio without accumulated drift —
 * suitable for both real-time and offline (Direct Offline Processing) use.
 *
 * For the small shifts used in double-tracking (< ±25 cents) the crossfade
 * is inaudible: at 8 cents the grain boundary occurs only every ~20 seconds.
 */
class PitchShifter
{
public:
    static constexpr int kBufSize = 16384; // power-of-2; ~372 ms at 44100 Hz
    static constexpr int kMask    = kBufSize - 1;

    void prepare (double sampleRate);
    void setCents (float cents);
    void reset();

    /** Process numSamples of mono audio in-place (input == output is allowed). */
    void processBlock (const float* input, float* output, int numSamples);

private:
    float  buf[kBufSize] = {};
    int    writePos  = 0;
    float  readPos1  = 0.0f;
    float  readPos2  = float (kBufSize / 2);
    float  speedRatio = 1.0f;

    float readInterp (float pos) const noexcept;
};
