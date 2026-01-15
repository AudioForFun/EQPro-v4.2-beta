#pragma once

#include <JuceHeader.h>
#include <vector>

namespace eqdsp
{
class SpectralDynamicsDSP
{
public:
    void prepare(double sampleRate, int maxBlockSize, int channels);
    void reset();
    void setEnabled(bool enabled);
    void setParams(float thresholdDb, float ratio, float attackMs, float releaseMs, float mix);
    void process(juce::AudioBuffer<float>& buffer);

private:
    struct ChannelState
    {
        std::vector<float> circular;
        std::vector<float> ola;
        std::vector<float> fftData;
        std::vector<float> gainDb;
        int writeIndex = 0;
        int hopCounter = 0;
    };

    double sampleRateHz = 44100.0;
    int fftOrder = 11;
    int fftSize = 1 << 11;
    int hopSize = 1 << 10;
    float normalization = 1.0f;
    bool enabled = false;

    float thresholdDb = -24.0f;
    float ratio = 2.0f;
    float attackMs = 20.0f;
    float releaseMs = 200.0f;
    float mix = 1.0f;

    std::vector<float> window;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<ChannelState> states;
};
} // namespace eqdsp
