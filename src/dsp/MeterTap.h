#pragma once

#include "MeteringDSP.h"

namespace eqdsp
{
class MeterTap
{
public:
    void prepare(double sampleRate);
    void reset();
    void process(const juce::AudioBuffer<float>& buffer, int numChannels);
    ChannelMeterState getState(int channel) const;
    float getCorrelation() const;
    int copyScopePoints(juce::Point<float>* dest, int maxPoints, int& writePos) const;
    void setCorrelationPair(int channelA, int channelB);

private:
    MeteringDSP meters;
};
} // namespace eqdsp
