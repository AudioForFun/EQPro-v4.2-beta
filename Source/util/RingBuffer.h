#pragma once

#include <JuceHeader.h>

// Lock-free audio FIFO for analyzer/meter taps.
class AudioFifo
{
public:
    // Preallocate buffer for expected size.
    void prepare(int bufferSize);
    // Push samples into the FIFO (audio thread).
    void push(const float* data, int numSamples);
    // Pull samples out of the FIFO (UI thread).
    int pull(float* dest, int numSamples);

private:
    juce::AbstractFifo fifo { 1 };
    juce::AudioBuffer<float> buffer;
};
