#pragma once

#include "../util/RingBuffer.h"

namespace eqdsp
{
class AnalyzerTap
{
public:
    void prepare(int fifoSize);
    void push(const float* data, int numSamples);
    AudioFifo& getFifo();

private:
    AudioFifo fifo;
};
} // namespace eqdsp
