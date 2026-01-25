#pragma once

#include <array>
#include <JuceHeader.h>
#include "../util/ParamIDs.h"

namespace eqdsp
{
class LinearPhaseEQ
{
public:
    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void beginImpulseUpdate(int headSize, int expectedLoads);
    void loadImpulse(int channelIndex, juce::AudioBuffer<float>&& impulse, double sampleRate);
    void endImpulseUpdate();
    void process(juce::AudioBuffer<float>& buffer);
    void processRange(juce::AudioBuffer<float>& buffer, int startChannel, int count);
    void configurePartitioning(int headSize);

    int getLatencySamples() const;
    void setLatencySamples(int samples);

private:
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
