#pragma once

#include <JuceHeader.h>
#include "Biquad.h"

namespace eqdsp
{
class EllipticDSP
{
public:
    void prepare(double sampleRate);
    void reset();
    void setParams(bool enabled, float cutoffHz, float amount);
    void process(juce::AudioBuffer<float>& buffer);

private:
    bool enabled = false;
    float cutoff = 120.0f;
    float amount = 1.0f;
    Biquad sideFilter;
    BandParams params;
    bool needsUpdate = true;
};
} // namespace eqdsp
