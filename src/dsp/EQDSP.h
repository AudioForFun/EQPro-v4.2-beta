#pragma once

#include <array>
#include <JuceHeader.h>
#include "Biquad.h"
#include "OnePole.h"
#include "../util/ParamIDs.h"
#include <atomic>

namespace eqdsp
{
class EQDSP
{
public:
    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setGlobalBypass(bool shouldBypass);
    void setSmartSoloEnabled(bool enabled);
    void setQMode(int mode);
    void setQModeAmount(float amount);
    void updateBandParams(int channelIndex, int bandIndex, const BandParams& params);
    void updateMsBandParams(int bandIndex, const BandParams& params);
    void setMsTargets(const std::array<int, ParamIDs::kBandsPerChannel>& targets);
    void setBandChannelMasks(const std::array<uint32_t, ParamIDs::kBandsPerChannel>& masks);
    float getDetectorDb(int channelIndex, int bandIndex) const;
    float getDynamicGainDb(int channelIndex, int bandIndex) const;
    void process(juce::AudioBuffer<float>& buffer,
                 const juce::AudioBuffer<float>* detectorBuffer = nullptr);

private:
    double sampleRateHz = 48000.0;
    int numChannels = 0;
    static constexpr int kMaxStages = 8;
    std::array<std::array<std::array<Biquad, kMaxStages>, ParamIDs::kBandsPerChannel>,
               ParamIDs::kMaxChannels>
        filters {};
    std::array<std::array<OnePole, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        onePoles {};
    std::array<std::array<juce::SmoothedValue<float>, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        smoothFreq {};
    std::array<std::array<juce::SmoothedValue<float>, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        smoothGain {};
    std::array<std::array<juce::SmoothedValue<float>, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        smoothQ {};
    std::array<std::array<juce::SmoothedValue<float>, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        smoothMix {};
    std::array<std::array<juce::SmoothedValue<float>, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        smoothDynThresh {};
    std::array<std::array<Biquad, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        soloFilters {};
    juce::AudioBuffer<float> scratchBuffer;
    std::array<std::array<BandParams, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels> cachedParams {};
    std::array<std::array<std::array<Biquad, kMaxStages>, ParamIDs::kBandsPerChannel>, 2>
        msFilters {};
    std::array<std::array<OnePole, ParamIDs::kBandsPerChannel>, 2> msOnePoles {};
    std::array<int, ParamIDs::kBandsPerChannel> msTargets {};
    std::array<uint32_t, ParamIDs::kBandsPerChannel> bandChannelMasks {};
    std::array<std::array<Biquad, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        detectorFilters {};
    std::array<std::array<float, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        detectorEnv {};
    std::array<std::array<float, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        detectorEnvRms {};
    std::array<std::array<std::atomic<float>, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        detectorDb {};
    std::array<std::array<std::atomic<float>, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        dynamicGainDb {};
    juce::AudioBuffer<float> msBuffer;
    juce::AudioBuffer<float> msDryBuffer;
    juce::AudioBuffer<float> detectorMsBuffer;
    juce::AudioBuffer<float> detectorTemp;
    bool globalBypass = false;
    bool smartSoloEnabled = false;
    int qMode = 0;
    float qModeAmount = 50.0f;

    float applyQMode(const BandParams& params) const;
};
} // namespace eqdsp
