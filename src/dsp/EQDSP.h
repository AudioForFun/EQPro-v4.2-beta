#pragma once

#include <array>
#include <JuceHeader.h>
#include "Biquad.h"
#include "OnePole.h"
#include "../util/ParamIDs.h"

namespace eqdsp
{
class EQDSP
{
public:
    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setGlobalBypass(bool shouldBypass);
    void setSmartSoloEnabled(bool enabled);
    void updateBandParams(int channelIndex, int bandIndex, const BandParams& params);
    void updateMsBandParams(int bandIndex, const BandParams& params);
    void setMsTargets(const std::array<int, ParamIDs::kBandsPerChannel>& targets);
    void process(juce::AudioBuffer<float>& buffer,
                 const juce::AudioBuffer<float>* detectorBuffer = nullptr);
    float getDynamicGainDb(int channelIndex, int bandIndex) const;

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
    std::array<std::array<Biquad, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        soloFilters {};
    std::array<std::array<Biquad, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        detectorFilters {};
    std::array<std::array<float, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        detectorEnv {};
    std::array<std::array<juce::SmoothedValue<float>, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        dynGainDb {};
    std::array<std::array<float, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        lastDynGainDb {};
    juce::AudioBuffer<float> scratchBuffer;
    std::array<std::array<BandParams, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels> cachedParams {};
    std::array<std::array<std::array<Biquad, kMaxStages>, ParamIDs::kBandsPerChannel>, 2>
        msFilters {};
    std::array<std::array<OnePole, ParamIDs::kBandsPerChannel>, 2> msOnePoles {};
    std::array<int, ParamIDs::kBandsPerChannel> msTargets {};
    juce::AudioBuffer<float> msBuffer;
    bool globalBypass = false;
    bool smartSoloEnabled = false;
};
} // namespace eqdsp
