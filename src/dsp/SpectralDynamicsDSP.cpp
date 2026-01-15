#include "SpectralDynamicsDSP.h"
#include <cmath>

namespace eqdsp
{
void SpectralDynamicsDSP::prepare(double sampleRate, int maxBlockSize, int channels)
{
    juce::ignoreUnused(maxBlockSize);
    sampleRateHz = sampleRate;
    fftOrder = 11;
    fftSize = 1 << fftOrder;
    hopSize = fftSize / 2;
    fft = std::make_unique<juce::dsp::FFT>(fftOrder);

    window.assign(static_cast<size_t>(fftSize), 1.0f);
    juce::dsp::WindowingFunction<float> windowFn(static_cast<size_t>(fftSize),
                                                 juce::dsp::WindowingFunction<float>::hann, false);
    windowFn.multiplyWithWindowingTable(window.data(), static_cast<size_t>(fftSize));
    normalization = 1.0f / static_cast<float>(fftSize * 0.5f);

    const int numChannels = juce::jmax(1, channels);
    states.clear();
    states.resize(static_cast<size_t>(numChannels));
    for (auto& state : states)
    {
        state.circular.assign(static_cast<size_t>(fftSize), 0.0f);
        state.ola.assign(static_cast<size_t>(fftSize), 0.0f);
        state.fftData.assign(static_cast<size_t>(fftSize * 2), 0.0f);
        state.gainDb.assign(static_cast<size_t>(fftSize / 2 + 1), 0.0f);
        state.writeIndex = 0;
        state.hopCounter = 0;
    }
}

void SpectralDynamicsDSP::reset()
{
    for (auto& state : states)
    {
        std::fill(state.circular.begin(), state.circular.end(), 0.0f);
        std::fill(state.ola.begin(), state.ola.end(), 0.0f);
        std::fill(state.fftData.begin(), state.fftData.end(), 0.0f);
        std::fill(state.gainDb.begin(), state.gainDb.end(), 0.0f);
        state.writeIndex = 0;
        state.hopCounter = 0;
    }
}

void SpectralDynamicsDSP::setEnabled(bool shouldEnable)
{
    enabled = shouldEnable;
}

void SpectralDynamicsDSP::setParams(float threshDb, float ratioIn, float attack, float release, float mixIn)
{
    thresholdDb = threshDb;
    ratio = juce::jmax(1.0f, ratioIn);
    attackMs = juce::jmax(1.0f, attack);
    releaseMs = juce::jmax(5.0f, release);
    mix = juce::jlimit(0.0f, 1.0f, mixIn);
}

void SpectralDynamicsDSP::process(juce::AudioBuffer<float>& buffer)
{
    if (! enabled || fft == nullptr || states.empty())
        return;

    const int numSamples = buffer.getNumSamples();
    if (mix <= 0.0f || numSamples <= 0)
        return;
    const int numChannels = juce::jmin(buffer.getNumChannels(), static_cast<int>(states.size()));

    const float attackCoeff = std::exp(-1.0f / (0.001f * attackMs * sampleRateHz));
    const float releaseCoeff = std::exp(-1.0f / (0.001f * releaseMs * sampleRateHz));

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& state = states[static_cast<size_t>(ch)];
        auto* channelData = buffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float input = channelData[i];
            const float processed = state.ola[static_cast<size_t>(state.writeIndex)];
            state.ola[static_cast<size_t>(state.writeIndex)] = 0.0f;

            channelData[i] = input * (1.0f - mix) + processed * mix;
            state.circular[static_cast<size_t>(state.writeIndex)] = input;

            state.writeIndex = (state.writeIndex + 1) % fftSize;
            if (++state.hopCounter < hopSize)
                continue;

            state.hopCounter = 0;
            int readIndex = state.writeIndex;
            for (int n = 0; n < fftSize; ++n)
            {
                const float sample = state.circular[static_cast<size_t>(readIndex)];
                state.fftData[static_cast<size_t>(n)] = sample * window[static_cast<size_t>(n)];
                readIndex = (readIndex + 1) % fftSize;
            }

            std::fill(state.fftData.begin() + fftSize, state.fftData.end(), 0.0f);
            fft->performRealOnlyForwardTransform(state.fftData.data());

            const int bins = fftSize / 2;
            for (int bin = 0; bin <= bins; ++bin)
            {
                const int index = bin * 2;
                const float re = state.fftData[static_cast<size_t>(index)];
                const float im = state.fftData[static_cast<size_t>(index + 1)];
                const float mag = std::sqrt(re * re + im * im) + 1.0e-8f;
                const float magDb = juce::Decibels::gainToDecibels(mag, -120.0f);

                float targetGainDb = 0.0f;
                if (magDb > thresholdDb)
                    targetGainDb = (thresholdDb + (magDb - thresholdDb) / ratio) - magDb;

                float& gainDb = state.gainDb[static_cast<size_t>(bin)];
                if (targetGainDb < gainDb)
                    gainDb = attackCoeff * gainDb + (1.0f - attackCoeff) * targetGainDb;
                else
                    gainDb = releaseCoeff * gainDb + (1.0f - releaseCoeff) * targetGainDb;

                const float gain = juce::Decibels::decibelsToGain(gainDb);
                state.fftData[static_cast<size_t>(index)] = re * gain;
                state.fftData[static_cast<size_t>(index + 1)] = im * gain;
            }

            fft->performRealOnlyInverseTransform(state.fftData.data());

            for (int n = 0; n < fftSize; ++n)
            {
                const float value = state.fftData[static_cast<size_t>(n)]
                    * window[static_cast<size_t>(n)] * normalization;
                const int outIndex = (state.writeIndex + n) % fftSize;
                state.ola[static_cast<size_t>(outIndex)] += value;
            }
        }
    }
}
} // namespace eqdsp
