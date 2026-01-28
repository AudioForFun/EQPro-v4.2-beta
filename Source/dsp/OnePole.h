#pragma once

#include <JuceHeader.h>

namespace eqdsp
{
// Simple one-pole filter for smoothing/detector paths.
class OnePole
{
public:
    // Initialize sampling rate.
    void prepare(double sampleRate);
    // Reset filter state.
    void reset();
    // Configure low/high-pass mode.
    void setLowPass(float cutoffHz);
    void setHighPass(float cutoffHz);
    // Process sample or block.
    float processSample(float x);
    void processBlock(float* data, int numSamples);

private:
    // Update coefficient for cutoff.
    void updateCoeff(float cutoffHz);

    double sampleRateHz = 48000.0;
    double alpha = 0.0;
    double z1 = 0.0;
    bool highPass = false;
    double lastCutoff = 0.0;
};
} // namespace eqdsp
