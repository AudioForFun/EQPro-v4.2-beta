#include "EllipticDSP.h"

namespace eqdsp
{
void EllipticDSP::prepare(double sampleRate)
{
    sideFilter.prepare(sampleRate);
    reset();
}

void EllipticDSP::reset()
{
    sideFilter.reset();
}

void EllipticDSP::setParams(bool shouldEnable, float cutoffHz, float amountIn)
{
    const float newAmount = juce::jlimit(0.0f, 1.0f, amountIn);
    if (enabled != shouldEnable || cutoff != cutoffHz || amount != newAmount)
    {
        enabled = shouldEnable;
        cutoff = cutoffHz;
        amount = newAmount;
        params.frequencyHz = cutoff;
        params.gainDb = 0.0f;
        params.q = 0.707f;
        params.type = FilterType::lowPass;
        params.bypassed = false;
        needsUpdate = true;
    }
}

void EllipticDSP::process(juce::AudioBuffer<float>& buffer)
{
    if (! enabled || buffer.getNumChannels() < 2)
        return;

    if (needsUpdate)
    {
        sideFilter.update(params);
        needsUpdate = false;
    }

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    const int samples = buffer.getNumSamples();

    for (int i = 0; i < samples; ++i)
    {
        const float mid = 0.5f * (left[i] + right[i]);
        float side = 0.5f * (left[i] - right[i]);
        const float lowSide = sideFilter.processSample(side);
        side -= amount * lowSide;
        left[i] = mid + side;
        right[i] = mid - side;
    }
}
} // namespace eqdsp
