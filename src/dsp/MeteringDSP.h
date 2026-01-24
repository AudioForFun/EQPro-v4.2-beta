#pragma once

#include <array>
#include <atomic>
#include <JuceHeader.h>
#include "../util/ParamIDs.h"

namespace eqdsp
{
struct ChannelMeterState
{
    float rmsDb = -120.0f;
    float peakDb = -120.0f;
};

class MeteringDSP
{
public:
    static constexpr int kScopePoints = 512;

    void prepare(double sampleRate);
    void reset();
    void process(const juce::AudioBuffer<float>& buffer, int numChannels);
    void setCorrelationPair(int channelA, int channelB);

    ChannelMeterState getChannelState(int channelIndex) const;
    float getCorrelation() const;
    int copyScopePoints(juce::Point<float>* dest, int maxPoints, int& writePos) const;

private:
    float computeRmsDb(const float* data, int numSamples) const;
    float computePeakDb(const float* data, int numSamples) const;

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
