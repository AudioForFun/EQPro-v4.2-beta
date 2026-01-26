#pragma once

#include "MeteringDSP.h"

namespace eqdsp
{
// Meter tap for RMS/peak/correlation/goniometer data.
class MeterTap
{
public:
    // Prepare metering for sample rate.
    void prepare(double sampleRate);
    // Reset meter state.
    void reset();
    // Process buffer and update meters.
    void process(const juce::AudioBuffer<float>& buffer, int numChannels);
    // Retrieve per-channel meter state.
    ChannelMeterState getState(int channel) const;
    // Correlation and scope points.
    float getCorrelation() const;
    int copyScopePoints(juce::Point<float>* dest, int maxPoints, int& writePos) const;
    // Choose correlation channel pair.
    void setCorrelationPair(int channelA, int channelB);

private:
    MeteringDSP meters;
};
} // namespace eqdsp
