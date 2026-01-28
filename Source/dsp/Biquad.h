#pragma once

#include <cmath>
#include <JuceHeader.h>
#include "EQBand.h"

namespace eqdsp
{
// Standard biquad filter with cached coefficients.
class Biquad
{
public:
    // Initialize sampling rate.
    void prepare(double sampleRate);
    // Reset state.
    void reset();
    // Update coefficients from band params.
    void update(const BandParams& params);

    // Process a single sample or a block.
    float processSample(float x);
    void processBlock(float* data, int numSamples);
    // Debug accessors for coefficients and state.
    void getCoefficients(float& b0Out, float& b1Out, float& b2Out, float& a1Out, float& a2Out) const;
    void getState(float& z1Out, float& z2Out) const;
    void setState(float z1In, float z2In);

private:
    // Compute coefficients for current band params.
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
