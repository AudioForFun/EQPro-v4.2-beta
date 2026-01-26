#pragma once

#include <array>
#include <JuceHeader.h>
#include "../util/ParamIDs.h"

namespace eqdsp
{
// Linear phase FIR engine with double-buffered convolution sets.
class LinearPhaseEQ
{
public:
    // Prepares convolution sets for the given format.
    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    // Clears convolution state.
    void reset();

    // Begin an impulse update for a staged set.
    void beginImpulseUpdate(int headSize, int expectedLoads);
    // Load impulse response for a given channel.
    void loadImpulse(int channelIndex, juce::AudioBuffer<float>&& impulse, double sampleRate);
    // Commit staged set once all impulses are loaded.
    void endImpulseUpdate();
    void process(juce::AudioBuffer<float>& buffer);
    void processRange(juce::AudioBuffer<float>& buffer, int startChannel, int count);
    // Update partitioning for new head size.
    void configurePartitioning(int headSize);

    int getLatencySamples() const;
    void setLatencySamples(int samples);

private:
    // DSP format + state tracking.
    double sampleRateHz = 48000.0;
    int numChannels = 0;
    int latencySamples = 0;
    int maxBlockSize = 0;
    std::array<std::array<std::unique_ptr<juce::dsp::Convolution>, ParamIDs::kMaxChannels>, 2> convolutions {};
    std::atomic<int> activeSet { 0 };
    int stagingSet = 1;
    int pendingLoads = 0;
    juce::dsp::ProcessSpec lastSpec {};
    bool hasSpec = false;
    int headSize = 0;
    juce::SpinLock convolverLock;
};
} // namespace eqdsp
