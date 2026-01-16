#pragma once

#include <JuceHeader.h>

namespace eqdsp
{
class OnePole
{
public:
    void prepare(double sampleRate);
    void reset();
    void setLowPass(float cutoffHz);
    void setHighPass(float cutoffHz);
    float processSample(float x);
    void processBlock(float* data, int numSamples);

private:
    void updateCoeff(float cutoffHz);

    double sampleRateHz = 48000.0;
    double alpha = 0.0;
    double z1 = 0.0;
    bool highPass = false;
    double lastCutoff = 0.0;
};
} // namespace eqdsp
