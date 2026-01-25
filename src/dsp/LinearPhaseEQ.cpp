#include "LinearPhaseEQ.h"

namespace eqdsp
{
namespace
{
void prepareConvolutionSet(std::array<std::unique_ptr<juce::dsp::Convolution>, ParamIDs::kMaxChannels>& set,
                           int numChannels,
                           int headSize,
                           bool hasSpec,
                           const juce::dsp::ProcessSpec& spec)
{
    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (headSize > 0)
            set[static_cast<size_t>(ch)] = std::make_unique<juce::dsp::Convolution>(
                juce::dsp::Convolution::NonUniform{ headSize });
        else
            set[static_cast<size_t>(ch)] = std::make_unique<juce::dsp::Convolution>();

        if (hasSpec)
            set[static_cast<size_t>(ch)]->prepare(spec);
    }
}
} // namespace

void LinearPhaseEQ::prepare(double sampleRate, int maxBlockSize, int channels)
{
    const juce::SpinLock::ScopedLockType lock(convolverLock);
    sampleRateHz = sampleRate;
    this->maxBlockSize = maxBlockSize;
    numChannels = juce::jlimit(0, ParamIDs::kMaxChannels, channels);
    activeSet.store(0, std::memory_order_relaxed);
    stagingSet = 1;
    pendingLoads = 0;

    juce::dsp::ProcessSpec spec {};
    spec.sampleRate = sampleRateHz;
    spec.maximumBlockSize = static_cast<uint32>(maxBlockSize);
    spec.numChannels = 1;
    lastSpec = spec;
    hasSpec = true;

    prepareConvolutionSet(convolutions[0], numChannels, headSize, hasSpec, lastSpec);
    prepareConvolutionSet(convolutions[1], numChannels, headSize, hasSpec, lastSpec);
}

void LinearPhaseEQ::reset()
{
    const juce::SpinLock::ScopedLockType lock(convolverLock);
    for (int setIndex = 0; setIndex < 2; ++setIndex)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            if (convolutions[setIndex][static_cast<size_t>(ch)] != nullptr)
                convolutions[setIndex][static_cast<size_t>(ch)]->reset();
    }
    pendingLoads = 0;
}

void LinearPhaseEQ::beginImpulseUpdate(int headSize, int expectedLoads)
{
    const juce::SpinLock::ScopedLockType lock(convolverLock);
    const int clampedHead = juce::jlimit(0, maxBlockSize, headSize);
    this->headSize = clampedHead;
    stagingSet = 1 - activeSet.load(std::memory_order_relaxed);
    pendingLoads = juce::jmax(0, expectedLoads);
    if (hasSpec)
        prepareConvolutionSet(convolutions[stagingSet], numChannels, clampedHead, hasSpec, lastSpec);
}

void LinearPhaseEQ::loadImpulse(int channelIndex, juce::AudioBuffer<float>&& impulse, double sampleRate)
{
    const juce::SpinLock::ScopedLockType lock(convolverLock);
    if (channelIndex < 0 || channelIndex >= numChannels)
        return;

    auto& convolver = convolutions[stagingSet][static_cast<size_t>(channelIndex)];
    if (convolver == nullptr)
        return;

    convolver->loadImpulseResponse(
        std::move(impulse),
        sampleRate,
        juce::dsp::Convolution::Stereo::no,
        juce::dsp::Convolution::Trim::no,
        juce::dsp::Convolution::Normalise::yes);
    if (pendingLoads > 0)
        --pendingLoads;
}

void LinearPhaseEQ::endImpulseUpdate()
{
    const juce::SpinLock::ScopedLockType lock(convolverLock);
    if (pendingLoads <= 0)
    {
        if (hasSpec)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                if (convolutions[stagingSet][static_cast<size_t>(ch)] != nullptr)
                    convolutions[stagingSet][static_cast<size_t>(ch)]->prepare(lastSpec);
        }
        activeSet.store(stagingSet, std::memory_order_release);
    }
    pendingLoads = 0;
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

    const int setIndex = activeSet.load(std::memory_order_acquire);
    juce::dsp::AudioBlock<float> block(buffer);
    for (int ch = startChannel; ch < endChannel; ++ch)
    {
        auto channelBlock = block.getSingleChannelBlock(ch);
        juce::dsp::ProcessContextReplacing<float> context(channelBlock);
        auto& convolver = convolutions[setIndex][static_cast<size_t>(ch)];
        if (convolver != nullptr)
            convolver->process(context);
    }
}

void LinearPhaseEQ::configurePartitioning(int headSize)
{
    const juce::SpinLock::ScopedLockType lock(convolverLock);
    const int clampedHead = juce::jlimit(0, maxBlockSize, headSize);
    this->headSize = clampedHead;
    if (hasSpec)
    {
        prepareConvolutionSet(convolutions[0], numChannels, clampedHead, hasSpec, lastSpec);
        prepareConvolutionSet(convolutions[1], numChannels, clampedHead, hasSpec, lastSpec);
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
