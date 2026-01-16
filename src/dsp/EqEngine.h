#pragma once

#include "EQDSP.h"
#include "LinearPhaseEQ.h"
#include "SpectralDynamicsDSP.h"
#include "ParamSnapshot.h"
#include "AnalyzerTap.h"
#include "MeterTap.h"
#include <vector>

namespace eqdsp
{
class EqEngine
{
public:
    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void process(juce::AudioBuffer<float>& buffer,
                 const ParamSnapshot& snapshot,
                 const juce::AudioBuffer<float>* detectorBuffer,
                 AnalyzerTap& preTap,
                 AnalyzerTap& postTap,
                 MeterTap& meterTap);
    void updateLinearPhase(const ParamSnapshot& snapshot, double sampleRate);

    void setOversampling(int index);
    int getLatencySamples() const;

    EQDSP& getEqDsp();
    const EQDSP& getEqDsp() const;
    LinearPhaseEQ& getLinearPhaseEq();

private:
    uint64_t computeParamsHash(const ParamSnapshot& snapshot) const;
    void rebuildLinearPhase(const ParamSnapshot& snapshot, int taps, double sampleRate);
    void updateOversampling(const ParamSnapshot& snapshot, double sampleRate, int maxBlockSize, int channels);
    EQDSP eqDsp;
    EQDSP eqDspOversampled;
    LinearPhaseEQ linearPhaseEq;
    LinearPhaseEQ linearPhaseMsEq;
    SpectralDynamicsDSP spectralDsp;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> oversampledBuffer;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    juce::SmoothedValue<float> globalMixSmoothed;
    juce::SmoothedValue<float> outputTrimGainSmoothed;

    int oversamplingIndex = 0;
    double sampleRateHz = 48000.0;
    int meterSkipFactor = 1;
    int meterSkipCounter = 0;
    int lastPhaseMode = 0;
    int lastLinearQuality = 0;
    int lastTaps = 0;
    int lastWindowIndex = 0;
    uint64_t lastParamHash = 0;
    int firFftSize = 0;
    int firFftOrder = 0;
    std::unique_ptr<juce::dsp::FFT> firFft;
    std::vector<float> firData;
    std::vector<float> firImpulse;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> firWindow;
    int firWindowMethod = -1;
};
} // namespace eqdsp
