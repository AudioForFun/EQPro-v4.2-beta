#pragma once

#include <algorithm>

namespace FFTUtils
{
inline float freqToNorm(float freq, float minFreq, float maxFreq)
{
    const float clamped = std::max(minFreq, std::min(freq, maxFreq));
    const float logMin = std::log(minFreq);
    const float logMax = std::log(maxFreq);
    return (std::log(clamped) - logMin) / (logMax - logMin);
}

inline float normToFreq(float norm, float minFreq, float maxFreq)
{
    const float logMin = std::log(minFreq);
    const float logMax = std::log(maxFreq);
    const float logFreq = logMin + (logMax - logMin) * std::clamp(norm, 0.0f, 1.0f);
    return std::exp(logFreq);
}
} // namespace FFTUtils
