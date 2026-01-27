#pragma once

#include <array>
#include <JuceHeader.h>
#include "Biquad.h"
#include "OnePole.h"
#include "../util/ParamIDs.h"
#include <atomic>

namespace eqdsp
{
// Minimum-phase IIR EQ engine (per-band, per-channel).
class EQDSP
{
public:
    // Prepare internal filters and buffers.
    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    // Reset filter state.
    void reset();
    // Global bypass toggle for IIR path.
    void setGlobalBypass(bool shouldBypass);
    // Smart solo logic.
    void setSmartSoloEnabled(bool enabled);
    // Q scaling mode and amount.
    void setQMode(int mode);
    void setQModeAmount(float amount);
    // Update parameters for a band on a channel.
    void updateBandParams(int channelIndex, int bandIndex, const BandParams& params);
    // Update parameters for a band in MS processing.
    void updateMsBandParams(int bandIndex, const BandParams& params);
    // MS target routing.
    void setMsTargets(const std::array<int, ParamIDs::kBandsPerChannel>& targets);
    // Per-band channel masks for immersive layouts.
    void setBandChannelMasks(const std::array<uint32_t, ParamIDs::kBandsPerChannel>& masks);
    // Detector and dynamic gain readbacks.
    float getDetectorDb(int channelIndex, int bandIndex) const;
    float getDynamicGainDb(int channelIndex, int bandIndex) const;
    // Process buffer in-place (optional detector source).
    // If harmonicOnlyBuffer is provided, it is filled with harmonic-only content (harmonicSample - sample).
    void process(juce::AudioBuffer<float>& buffer,
                 const juce::AudioBuffer<float>* detectorBuffer = nullptr,
                 juce::AudioBuffer<float>* harmonicOnlyBuffer = nullptr);

private:
    double sampleRateHz = 48000.0;
    int numChannels = 0;
    int maxBlockSize = 512;  // v4.4 beta: Store max block size for oversampler initialization
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
    
    // v4.4 beta: Per-band harmonic oversampling infrastructure
    // Oversamplers for each band (per-channel) - only created when needed
    std::array<std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        harmonicOversamplers {};
    std::array<std::array<juce::AudioBuffer<float>, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        harmonicOversampledBuffers {};
    std::array<std::array<juce::AudioBuffer<float>, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        harmonicDryBuffers {};  // For latency compensation - stores dry samples before harmonic processing
    std::array<std::array<juce::AudioBuffer<float>, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        harmonicDryDelayBuffers {};  // Delay buffers for sample-accurate mixing
    std::array<std::array<juce::AudioBuffer<float>, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        harmonicProcessBuffers {};  // Buffers to collect samples for block processing
    std::array<std::array<int, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        harmonicOversamplingFactors {};  // Current oversampling factor per band (0=NONE, 1=2x, 2=4x, 3=8x, 4=16x)
    std::array<std::array<int, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        harmonicLatencySamples {};  // Latency in samples for each band's oversampling
    std::array<std::array<int, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        harmonicDryDelayWritePos {};  // Write position for dry delay buffers
    std::array<std::array<int, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels>
        harmonicBufferWritePos {};  // Write position for harmonic process buffers

    // Applies Q mode scaling (constant/proportional).
    float applyQMode(const BandParams& params) const;
    // v4.4 beta: Process harmonics with oversampling and latency compensation (block-based)
    void processHarmonicsBlock(int ch, int band, const BandParams& params, 
                               juce::AudioBuffer<float>& harmonicBuffer, 
                               const juce::AudioBuffer<float>& dryBuffer,
                               int numSamples);
};
} // namespace eqdsp
