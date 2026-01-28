#include "AnalyzerTap.h"

namespace eqdsp
{
void AnalyzerTap::prepare(int fifoSize)
{
    fifo.prepare(fifoSize);
}

void AnalyzerTap::push(const float* data, int numSamples)
{
    if (data == nullptr || numSamples <= 0)
        return;
    fifo.push(data, numSamples);
}

AudioFifo& AnalyzerTap::getFifo()
{
    return fifo;
}
} // namespace eqdsp
