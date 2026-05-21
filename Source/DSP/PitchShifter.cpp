#include "PitchShifter.h"
#include <cstring>

static constexpr float kPi     = 3.14159265358979f;
static constexpr float kTwoPi  = 6.28318530717959f;

void PitchShifter::prepare (double /*sampleRate*/)
{
    reset();
}

void PitchShifter::setCents (float cents)
{
    speedRatio = std::pow (2.0f, cents / 1200.0f);
}

void PitchShifter::reset()
{
    std::memset (buf, 0, sizeof (buf));
    writePos = kBufSize / 2; // pre-fill half buffer as silence so readPos1
    readPos1 = 0.0f;         // starts kBufSize/2 samples behind the write head
    readPos2 = float (kBufSize / 2);
}

void PitchShifter::processBlock (const float* input, float* output, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        buf[writePos & kMask] = input[i];
        writePos = (writePos + 1) & kMask;

        // Distance of each read pointer behind the write head, normalised [0,1).
        // phi = 0   → at write head (unsafe, weight = 0)
        // phi = 0.5 → half-buffer behind (safe zone, weight = 1)
        auto phi = [&] (float rp) -> float
        {
            float d = float (writePos) - rp;
            if (d < 0.0f)           d += float (kBufSize);
            if (d >= float(kBufSize)) d -= float (kBufSize);
            return d / float (kBufSize);
        };

        const float p1 = phi (readPos1);
        const float p2 = phi (readPos2);

        // sin²(π·φ): peaks at φ=0.5, zero at φ=0 and φ=1.
        // Since readPos2 = readPos1 + kBufSize/2, p2 = (p1+0.5) mod 1,
        // and sin²(π·p1) + sin²(π·p2) = 1 — guaranteed unity gain sum.
        const float w1 = 0.5f * (1.0f - std::cos (kTwoPi * p1));
        const float w2 = 0.5f * (1.0f - std::cos (kTwoPi * p2));

        output[i] = readInterp (readPos1) * w1 + readInterp (readPos2) * w2;

        readPos1 += speedRatio;
        if (readPos1 >= float (kBufSize)) readPos1 -= float (kBufSize);

        readPos2 += speedRatio;
        if (readPos2 >= float (kBufSize)) readPos2 -= float (kBufSize);
    }
}

float PitchShifter::readInterp (float pos) const noexcept
{
    const int   idx  = int (pos) & kMask;
    const int   idx1 = (idx + 1) & kMask;
    const float frac = pos - std::floor (pos);
    return buf[idx] * (1.0f - frac) + buf[idx1] * frac;
}
