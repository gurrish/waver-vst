#include "VariableDelay.h"
#include <cmath>
#include <cstring>
#include <algorithm>

void VariableDelay::prepare (double sr, float maxDelayMs)
{
    sampleRate = sr;

    const int maxSamples = int (std::ceil (maxDelayMs * sr / 1000.0)) + 4;
    buf.assign (maxSamples, 0.0f);
    writePos = 0;

    // IIR coefficient for 500 ms time constant
    alpha = std::exp (-1.0f / float (sr * 0.5));

    // Theoretical RMS of IIR-filtered uniform [-1,1] noise:
    //   RMS_in = 1/sqrt(3),  RMS_out = RMS_in * sqrt((1-alpha)/(1+alpha))
    const float rmsIn  = 1.0f / std::sqrt (3.0f);
    const float rmsOut = rmsIn * std::sqrt ((1.0f - alpha) / (1.0f + alpha));
    normScale = (rmsOut > 1e-9f) ? (1.0f / rmsOut) : 1.0f;

    reset();
}

void VariableDelay::setParameters (float baseDelayMs, float driftDepthMs)
{
    baseDelaySamples = baseDelayMs  * float (sampleRate) / 1000.0f;
    driftScaled      = driftDepthMs * float (sampleRate) / 1000.0f * normScale;
}

void VariableDelay::reset()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    writePos = 0;
    smoothed = 0.0f;
    lcg      = 0.3f;
}

void VariableDelay::processBlock (const float* input, float* output, int numSamples)
{
    const int bufSize = int (buf.size());

    for (int i = 0; i < numSamples; ++i)
    {
        buf[writePos] = input[i];
        writePos      = (writePos + 1) % bufSize;

        // IIR-smoothed noise: very slow random walk
        smoothed = alpha * smoothed + (1.0f - alpha) * nextNoise();

        const float delaySamples = baseDelaySamples + smoothed * driftScaled;
        output[i] = readInterp (std::max (delaySamples, 1.0f));
    }
}

float VariableDelay::readInterp (float delaySamples) const noexcept
{
    const int   bufSize = int (buf.size());
    float       rp      = float (writePos) - delaySamples;

    while (rp < 0.0f)           rp += float (bufSize);
    while (rp >= float(bufSize)) rp -= float (bufSize);

    const int   idx  = int (rp);
    const float frac = rp - float (idx);
    const int   idx1 = (idx + 1) % bufSize;

    return buf[idx] * (1.0f - frac) + buf[idx1] * frac;
}

// Cheap LCG producing values in [-1, 1] — deterministic and allocation-free
float VariableDelay::nextNoise() noexcept
{
    // Park-Miller LCG
    lcg = lcg * 16807.0f;
    lcg -= std::floor (lcg / 2147483647.0f) * 2147483647.0f;
    return (lcg / 2147483647.0f) * 2.0f - 1.0f;
}
