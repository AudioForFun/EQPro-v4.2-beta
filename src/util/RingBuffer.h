#pragma once

#include <JuceHeader.h>

class AudioFifo
{
public:
    void prepare(int bufferSize);
    void push(const float* data, int numSamples);
    int pull(float* dest, int numSamples);

private:
    juce::AbstractFifo fifo { 1 };
    juce::AudioBuffer<float> buffer;
};
