#pragma once

namespace eqdsp
{
enum class FilterType
{
    bell = 0,
    lowShelf,
    highShelf,
    lowPass,
    highPass,
    notch,
    bandPass,
    allPass,
    tilt,
    flatTilt
};

struct BandParams
{
    float frequencyHz = 1000.0f;
    float gainDb = 0.0f;
    float q = 0.707f;
    FilterType type = FilterType::bell;
    float slopeDb = 12.0f;
    bool bypassed = false;
    bool solo = false;
};
} // namespace eqdsp
