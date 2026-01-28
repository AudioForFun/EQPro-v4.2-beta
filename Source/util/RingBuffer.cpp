#include "RingBuffer.h"

void AudioFifo::prepare(int bufferSize)
{
    if (bufferSize <= 0)
    {
        buffer.setSize(0, 0);
        fifo.setTotalSize(1);
        return;
    }

    buffer.setSize(1, bufferSize);
    buffer.clear();
    fifo.setTotalSize(bufferSize);
}

void AudioFifo::push(const float* data, int numSamples)
{
    if (numSamples <= 0)
        return;
    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
        return;

    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;

    fifo.prepareToWrite(numSamples, start1, size1, start2, size2);
    const int total = size1 + size2;
    if (total == 0)
        return;

    const int skip = juce::jmax(0, numSamples - total);
    const float* src = data + skip;

    if (size1 > 0)
        buffer.copyFrom(0, start1, src, size1);
    if (size2 > 0)
        buffer.copyFrom(0, start2, src + size1, size2);

    fifo.finishedWrite(total);
}

int AudioFifo::pull(float* dest, int numSamples)
{
    if (numSamples <= 0)
        return 0;
    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
        return 0;

    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;

    fifo.prepareToRead(numSamples, start1, size1, start2, size2);
    const int total = size1 + size2;
    if (total == 0)
        return 0;

    if (size1 > 0)
        juce::FloatVectorOperations::copy(dest, buffer.getReadPointer(0, start1), size1);
    if (size2 > 0)
        juce::FloatVectorOperations::copy(dest + size1, buffer.getReadPointer(0, start2), size2);

    fifo.finishedRead(total);
    return total;
}
