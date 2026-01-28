#include "OnePole.h"
#include <cmath>

namespace eqdsp
{
void OnePole::prepare(double sampleRate)
{
    sampleRateHz = sampleRate;
    reset();
}

void OnePole::reset()
{
    z1 = 0.0;
}

void OnePole::setLowPass(float cutoffHz)
{
    highPass = false;
    updateCoeff(cutoffHz);
}

void OnePole::setHighPass(float cutoffHz)
{
    highPass = true;
    updateCoeff(cutoffHz);
}

float OnePole::processSample(float x)
{
    if (! highPass)
    {
        const double y = (1.0 - alpha) * x + alpha * z1;
        z1 = y;
        return static_cast<float>(y);
    }

    const double y = (1.0 + alpha) * 0.5 * (x - z1) + alpha * z1;
    z1 = y;
    return static_cast<float>(y);
}

void OnePole::processBlock(float* data, int numSamples)
{
    if (data == nullptr || numSamples <= 0)
        return;

    double z = z1;
    const double a = alpha;
    const bool hp = highPass;

    if (! hp)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const double y = (1.0 - a) * data[i] + a * z;
            z = y;
            data[i] = static_cast<float>(y);
        }
    }
    else
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const double y = (1.0 + a) * 0.5 * (data[i] - z) + a * z;
            z = y;
            data[i] = static_cast<float>(y);
        }
    }

    z1 = z;
}

void OnePole::updateCoeff(float cutoffHz)
{
    if (cutoffHz == lastCutoff)
        return;

    const double nyquist = sampleRateHz * 0.5;
    const double clamped = juce::jlimit(10.0, nyquist * 0.99, static_cast<double>(cutoffHz));
    alpha = std::exp(-2.0 * juce::MathConstants<double>::pi * clamped / sampleRateHz);
    lastCutoff = cutoffHz;
}
} // namespace eqdsp
