#include "LinearPhaseEQ.h"

namespace eqdsp
{
void LinearPhaseEQ::prepare(double sampleRate, int maxBlockSize, int channels)
{
    sampleRateHz = sampleRate;
    numChannels = juce::jlimit(0, ParamIDs::kMaxChannels, channels);

    juce::dsp::ProcessSpec spec {};
    spec.sampleRate = sampleRateHz;
    spec.maximumBlockSize = static_cast<uint32>(maxBlockSize);
    spec.numChannels = 1;
    lastSpec = spec;
    hasSpec = true;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (convolutions[ch] == nullptr)
        {
            if (headSize > 0)
                convolutions[ch] = std::make_unique<juce::dsp::Convolution>(
                    juce::dsp::Convolution::NonUniform{ headSize });
            else
                convolutions[ch] = std::make_unique<juce::dsp::Convolution>();
        }

        convolutions[ch]->prepare(spec);
    }
}

void LinearPhaseEQ::reset()
{
    for (int ch = 0; ch < numChannels; ++ch)
        if (convolutions[ch] != nullptr)
            convolutions[ch]->reset();
}

void LinearPhaseEQ::loadImpulse(int channelIndex, juce::AudioBuffer<float>&& impulse, double sampleRate)
{
    if (channelIndex < 0 || channelIndex >= numChannels)
        return;

    if (convolutions[channelIndex] == nullptr)
        return;

    convolutions[channelIndex]->loadImpulseResponse(
        std::move(impulse),
        sampleRate,
        juce::dsp::Convolution::Stereo::no,
        juce::dsp::Convolution::Trim::yes,
        juce::dsp::Convolution::Normalise::yes);
}

void LinearPhaseEQ::process(juce::AudioBuffer<float>& buffer)
{
    processRange(buffer, 0, juce::jmin(numChannels, buffer.getNumChannels()));
}

void LinearPhaseEQ::processRange(juce::AudioBuffer<float>& buffer, int startChannel, int count)
{
    if (count <= 0 || startChannel < 0)
        return;

    const int channels = juce::jmin(numChannels, buffer.getNumChannels());
    const int endChannel = juce::jmin(channels, startChannel + count);
    if (startChannel >= endChannel)
        return;

    juce::dsp::AudioBlock<float> block(buffer);
    for (int ch = startChannel; ch < endChannel; ++ch)
    {
        auto channelBlock = block.getSingleChannelBlock(ch);
        juce::dsp::ProcessContextReplacing<float> context(channelBlock);
        if (convolutions[ch] != nullptr)
            convolutions[ch]->process(context);
    }
}

void LinearPhaseEQ::configurePartitioning(int headSize)
{
    if (this->headSize == headSize && hasSpec)
        return;

    this->headSize = headSize;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (headSize > 0)
            convolutions[ch] = std::make_unique<juce::dsp::Convolution>(
                juce::dsp::Convolution::NonUniform{ headSize });
        else
            convolutions[ch] = std::make_unique<juce::dsp::Convolution>();

        if (hasSpec)
            convolutions[ch]->prepare(lastSpec);
    }
}

int LinearPhaseEQ::getLatencySamples() const
{
    return latencySamples;
}

void LinearPhaseEQ::setLatencySamples(int samples)
{
    latencySamples = samples;
}
} // namespace eqdsp
