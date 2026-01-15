#include "MeteringDSP.h"

namespace
{
constexpr float kMinDb = -120.0f;
constexpr float kEpsilon = 1.0e-12f;
}

namespace eqdsp
{
void MeteringDSP::prepare(double sampleRate)
{
    sampleRateHz = sampleRate;
    reset();
}

void MeteringDSP::reset()
{
    for (auto& state : channelStates)
    {
        state.rmsDb = kMinDb;
        state.peakDb = kMinDb;
    }

    correlation = 0.0f;
}

void MeteringDSP::process(const juce::AudioBuffer<float>& buffer, int numChannels)
{
    const int channels = juce::jlimit(0, ParamIDs::kMaxChannels, numChannels);
    const int samples = buffer.getNumSamples();
    if (channels == 0 || samples == 0)
        return;

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* data = buffer.getReadPointer(ch);
        const float rmsDb = computeRmsDb(data, samples);
        const float peakDb = computePeakDb(data, samples);
        channelStates[ch].rmsDb = smooth(channelStates[ch].rmsDb, rmsDb, rmsSmooth);
        channelStates[ch].peakDb = smooth(channelStates[ch].peakDb, peakDb, peakSmooth);
    }

    if (channels >= 2)
    {
        const int a = juce::jlimit(0, channels - 1, corrA);
        const int b = juce::jlimit(0, channels - 1, corrB);
        if (a == b)
            return;

        const auto* left = buffer.getReadPointer(a);
        const auto* right = buffer.getReadPointer(b);
        double sumLR = 0.0;
        double sumL2 = 0.0;
        double sumR2 = 0.0;

        for (int i = 0; i < samples; ++i)
        {
            const double l = left[i];
            const double r = right[i];
            sumLR += l * r;
            sumL2 += l * l;
            sumR2 += r * r;
        }

        const double denom = std::sqrt(sumL2 * sumR2) + kEpsilon;
        const float target = static_cast<float>(sumLR / denom);
        correlation = smooth(correlation, target, correlationSmooth);
    }
}

ChannelMeterState MeteringDSP::getChannelState(int channelIndex) const
{
    const int index = juce::jlimit(0, ParamIDs::kMaxChannels - 1, channelIndex);
    return channelStates[static_cast<size_t>(index)];
}

float MeteringDSP::getCorrelation() const
{
    return correlation;
}

void MeteringDSP::setCorrelationPair(int channelA, int channelB)
{
    corrA = channelA;
    corrB = channelB;
}

float MeteringDSP::computeRmsDb(const float* data, int numSamples) const
{
    double sum = 0.0;
    for (int i = 0; i < numSamples; ++i)
        sum += data[i] * data[i];

    const double rms = std::sqrt(sum / std::max(1, numSamples));
    return juce::Decibels::gainToDecibels(static_cast<float>(rms), kMinDb);
}

float MeteringDSP::computePeakDb(const float* data, int numSamples) const
{
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        peak = std::max(peak, std::abs(data[i]));

    return juce::Decibels::gainToDecibels(peak, kMinDb);
}

float MeteringDSP::smooth(float current, float target, float coeff) const
{
    return current + coeff * (target - current);
}
} // namespace eqdsp
