#pragma once

#include "../util/RingBuffer.h"

namespace eqdsp
{
// Analyzer tap to capture audio into a FIFO for UI FFT.
class AnalyzerTap
{
public:
    // Prepare FIFO size.
    void prepare(int fifoSize);
    // Push audio samples (audio thread).
    void push(const float* data, int numSamples);
    // Get FIFO for UI reads.
    AudioFifo& getFifo();

private:
    AudioFifo fifo;
};
} // namespace eqdsp
