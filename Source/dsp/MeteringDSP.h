#pragma once

#include <array>
#include <atomic>
#include <JuceHeader.h>
#include "../util/ParamIDs.h"

namespace eqdsp
{
// Simple RMS/peak metering state for one channel.
struct ChannelMeterState
{
    float rmsDb = -120.0f;
    float peakDb = -120.0f;
};

// Metering DSP for RMS, peak, correlation, and goniometer points.
class MeteringDSP
{
public:
    static constexpr int kScopePoints = 512;

    // Prepare internal buffers and smoothers.
    void prepare(double sampleRate);
    // Reset state.
    void reset();
    // Process audio and update states.
    void process(const juce::AudioBuffer<float>& buffer, int numChannels);
    // Set which channels feed correlation/phase scope.
    void setCorrelationPair(int channelA, int channelB);

    // Readback current meter values.
    ChannelMeterState getChannelState(int channelIndex) const;
    float getCorrelation() const;
    int copyScopePoints(juce::Point<float>* dest, int maxPoints, int& writePos) const;

private:
    // Level computations.
    float computeRmsDb(const float* data, int numSamples) const;
    float computePeakDb(const float* data, int numSamples) const;

    // Smoothing helper.
    float smooth(float current, float target, float coeff) const;

    double sampleRateHz = 48000.0;
    std::array<ChannelMeterState, ParamIDs::kMaxChannels> channelStates {};

    float correlation = 0.0f;
    float correlationSmooth = 0.2f;
    float rmsSmooth = 0.2f;
    float peakSmooth = 0.2f;
    int corrA = 0;
    int corrB = 1;
    std::array<float, kScopePoints> scopeX {};
    std::array<float, kScopePoints> scopeY {};
    std::atomic<int> scopeWritePos { 0 };
    int scopeDecimCounter = 0;
};
} // namespace eqdsp
