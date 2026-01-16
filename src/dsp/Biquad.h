#pragma once

#include <cmath>
#include "EQBand.h"

namespace eqdsp
{
class Biquad
{
public:
    void prepare(double sampleRate);
    void reset();
    void update(const BandParams& params);

    float processSample(float x);
    void processBlock(float* data, int numSamples);

private:
    void setCoefficients(const BandParams& params);

    double sampleRateHz = 48000.0;
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
    double z1 = 0.0;
    double z2 = 0.0;
    BandParams lastParams;
    bool firstUpdate = true;
};
} // namespace eqdsp
