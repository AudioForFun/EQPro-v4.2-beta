#pragma once

namespace Smoothing
{
// One-pole smoothing step for UI/DSP values.
inline float smooth(float previous, float target, float coefficient)
{
    return previous + coefficient * (target - previous);
}
} // namespace Smoothing
