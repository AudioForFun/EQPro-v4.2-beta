#include "MeteringDSP.h"

namespace
{
constexpr float kMinDb = -120.0f;
constexpr float kEpsilon = 1.0e-12f;
constexpr int kScopeDecim = 4;
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
    scopeWritePos.store(0, std::memory_order_release);
    scopeDecimCounter = 0;
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

        const auto* left = buffer.getReadPointer(a);
        const auto* right = buffer.getReadPointer(b);
        double sumLR = 0.0;
        double sumL2 = 0.0;
        double sumR2 = 0.0;
        int writePos = scopeWritePos.load(std::memory_order_relaxed);

        for (int i = 0; i < samples; ++i)
        {
            const double l = left[i];
            const double r = right[i];
            sumLR += l * r;
            sumL2 += l * l;
            sumR2 += r * r;

            scopeDecimCounter++;
            if (scopeDecimCounter >= kScopeDecim)
            {
                scopeDecimCounter = 0;
                float x = static_cast<float>(0.5 * (l + r));
                float y = static_cast<float>(0.5 * (l - r));
                x = juce::jlimit(-1.0f, 1.0f, x);
                y = juce::jlimit(-1.0f, 1.0f, y);
                scopeX[static_cast<size_t>(writePos)] = x;
                scopeY[static_cast<size_t>(writePos)] = y;
                writePos = (writePos + 1) % kScopePoints;
            }
        }

        const double denom = std::sqrt(sumL2 * sumR2) + kEpsilon;
        const float target = static_cast<float>(sumLR / denom);
        correlation = smooth(correlation, target, correlationSmooth);
        scopeWritePos.store(writePos, std::memory_order_release);
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

int MeteringDSP::copyScopePoints(juce::Point<float>* dest, int maxPoints, int& writePos) const
{
    if (dest == nullptr || maxPoints <= 0)
    {
        writePos = 0;
        return 0;
    }

    const int count = juce::jmin(kScopePoints, maxPoints);
    const int end = scopeWritePos.load(std::memory_order_acquire);
    writePos = end;

    for (int i = 0; i < count; ++i)
    {
        const int idx = (end - count + i + kScopePoints) % kScopePoints;
        dest[i] = { scopeX[static_cast<size_t>(idx)], scopeY[static_cast<size_t>(idx)] };
    }
    return count;
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
