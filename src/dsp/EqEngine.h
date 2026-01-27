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
// Central DSP engine: routes snapshots to IIR/FIR processing, meters, and taps.
class EqEngine
{
public:
    // Prepare all internal DSP state for the given format.
    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    // Reset state to defaults.
    void reset();
    // Process a block using the provided snapshot and taps.
    void process(juce::AudioBuffer<float>& buffer,
                 const ParamSnapshot& snapshot,
                 const juce::AudioBuffer<float>* detectorBuffer,
                 AnalyzerTap& preTap,
                 AnalyzerTap& postTap,
                 AnalyzerTap& harmonicTap,  // v4.5 beta: Tap for harmonic-only curve (red)
                 MeterTap& meterTap);
    // Rebuild FIR paths when parameters change.
    void updateLinearPhase(const ParamSnapshot& snapshot, double sampleRate);

    void setOversampling(int index);
    int getLatencySamples() const;
    void setDebugToneEnabled(bool enabled);
    void setDebugToneFrequency(float frequencyHz);

    EQDSP& getEqDsp();
    const EQDSP& getEqDsp() const;
    LinearPhaseEQ& getLinearPhaseEq();
    float getLastPreRmsDb() const;
    float getLastPostRmsDb() const;
    int getLastRmsPhaseMode() const;
    int getLastRmsQuality() const;

private:
    // Hash helper to detect snapshot changes.
    uint64_t computeParamsHash(const ParamSnapshot& snapshot) const;
    // FIR rebuild path for linear phase processing.
    void rebuildLinearPhase(const ParamSnapshot& snapshot, int taps, int headSize, double sampleRate);
    // Oversampling setup for non-realtime modes.
    void updateOversampling(const ParamSnapshot& snapshot, double sampleRate, int maxBlockSize, int channels);
    EQDSP eqDsp;
    EQDSP eqDspOversampled;
    LinearPhaseEQ linearPhaseEq;
    LinearPhaseEQ linearPhaseMsEq;
    SpectralDynamicsDSP spectralDsp;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> dryDelayBuffer;
    int dryDelayWritePos = 0;
    int mixDelaySamples = 0;
    int maxDelaySamples = 8192;
    juce::AudioBuffer<float> minPhaseBuffer;
    juce::AudioBuffer<float> minPhaseDelayBuffer;
    int minPhaseDelayWritePos = 0;
    int minPhaseDelaySamples = 0;
    juce::AudioBuffer<float> calibBuffer;
    juce::AudioBuffer<float> oversampledBuffer;
    juce::AudioBuffer<float> harmonicTapBuffer;
    juce::AudioBuffer<float> harmonicTapOversampledBuffer;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    juce::SmoothedValue<float> globalMixSmoothed;
    juce::SmoothedValue<float> outputTrimGainSmoothed;
    juce::SmoothedValue<float> autoGainSmoothed;

    int oversamplingIndex = 0;
    int maxPreparedBlockSize = 0;
    double sampleRateHz = 48000.0;
    int meterSkipFactor = 1;
    int meterSkipCounter = 0;
    std::atomic<bool> debugToneEnabled { false };
    double debugPhase = 0.0;
    double debugPhaseDelta = 0.0;
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
    std::atomic<float> lastPreRmsDb { -120.0f };
    std::atomic<float> lastPostRmsDb { -120.0f };
    std::atomic<int> lastRmsPhaseMode { 0 };
    std::atomic<int> lastRmsQuality { 0 };

    void updateDryDelay(int latencySamples, int maxBlockSize, int numChannels);
    void applyDryDelay(juce::AudioBuffer<float>& dry, int numSamples, int delaySamples);
    void updateMinPhaseDelay(int latencySamples, int maxBlockSize, int numChannels);
    void applyMinPhaseDelay(juce::AudioBuffer<float>& buffer, int numSamples, int delaySamples);
};
} // namespace eqdsp
