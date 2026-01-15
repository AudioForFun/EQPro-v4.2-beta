#pragma once

namespace Smoothing
{
inline float smooth(float previous, float target, float coefficient)
{
    return previous + coefficient * (target - previous);
}
} // namespace Smoothing
