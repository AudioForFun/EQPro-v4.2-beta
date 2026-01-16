#include "MeterTap.h"

namespace eqdsp
{
void MeterTap::prepare(double sampleRate)
{
    meters.prepare(sampleRate);
}

void MeterTap::reset()
{
    meters.reset();
}

void MeterTap::process(const juce::AudioBuffer<float>& buffer, int numChannels)
{
    meters.process(buffer, numChannels);
}

ChannelMeterState MeterTap::getState(int channel) const
{
    return meters.getChannelState(channel);
}

float MeterTap::getCorrelation() const
{
    return meters.getCorrelation();
}

void MeterTap::setCorrelationPair(int channelA, int channelB)
{
    meters.setCorrelationPair(channelA, channelB);
}
} // namespace eqdsp
