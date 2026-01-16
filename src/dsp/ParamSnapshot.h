#pragma once

#include <array>
#include <cstdint>
#include "../util/ParamIDs.h"

namespace eqdsp
{
struct BandSnapshot
{
    float frequencyHz = 1000.0f;
    float gainDb = 0.0f;
    float q = 0.707f;
    int type = 0;
    bool bypassed = false;
    int msTarget = 0;
    float slopeDb = 12.0f;
    bool solo = false;
    float mix = 1.0f;
    bool dynEnabled = false;
    int dynMode = 0;
    float dynThresholdDb = -24.0f;
    float dynAttackMs = 20.0f;
    float dynReleaseMs = 200.0f;
    bool dynAuto = true;
    bool dynExternal = false;
};

struct ParamSnapshot
{
    int numChannels = 0;
    bool globalBypass = false;
    float globalMix = 1.0f;
    int phaseMode = 0;
    int linearQuality = 1;
    int linearWindow = 0;
    int oversampling = 0;
    float outputTrimDb = 0.0f;
    int characterMode = 0;
    bool smartSolo = false;
    int qMode = 0;
    float qModeAmount = 50.0f;
    bool spectralEnabled = false;
    float spectralThresholdDb = -24.0f;
    float spectralRatio = 2.0f;
    float spectralAttackMs = 20.0f;
    float spectralReleaseMs = 200.0f;
    float spectralMix = 1.0f;
    bool autoGainEnabled = false;
    float gainScale = 1.0f;
    bool phaseInvert = false;

    std::array<std::array<BandSnapshot, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        bands {};
    std::array<int, ParamIDs::kBandsPerChannel> msTargets {};
    std::array<uint32_t, ParamIDs::kBandsPerChannel> bandChannelMasks {};
};
} // namespace eqdsp
