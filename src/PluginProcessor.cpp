#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "util/ParamIDs.h"
#include "util/ChannelLayoutUtils.h"
#include <cmath>
#include <complex>
#include <cstring>

namespace
{
constexpr const char* kParamFreqSuffix = "freq";
constexpr const char* kParamGainSuffix = "gain";
constexpr const char* kParamQSuffix = "q";
constexpr const char* kParamTypeSuffix = "type";
constexpr const char* kParamBypassSuffix = "bypass";
constexpr const char* kParamMsSuffix = "ms";
constexpr const char* kParamSlopeSuffix = "slope";
constexpr const char* kParamSoloSuffix = "solo";
constexpr const char* kParamMixSuffix = "mix";
constexpr const char* kParamDynEnableSuffix = "dynEnable";
constexpr const char* kParamDynModeSuffix = "dynMode";
constexpr const char* kParamDynThreshSuffix = "dynThresh";
constexpr const char* kParamDynAttackSuffix = "dynAttack";
constexpr const char* kParamDynReleaseSuffix = "dynRelease";
constexpr const char* kParamDynAutoSuffix = "dynAuto";

const juce::StringArray kFilterTypeChoices {
    "Bell",
    "Low Shelf",
    "High Shelf",
    "Low Pass",
    "High Pass",
    "Notch",
    "Band Pass",
    "All Pass",
    "Tilt",
    "Flat Tilt"
};

const juce::StringArray kMsChoices {
    "All",
    "Mid",
    "Side",
    "Left",
    "Right",
    "L/R",
    "Mono",
    "C",
    "LFE",
    "Ls",
    "Rs",
    "Lrs",
    "Rrs",
    "Lc",
    "Rc",
    "Ltf",
    "Rtf",
    "Tfc",
    "Tm",
    "Ltr",
    "Rtr",
    "Trc",
    "Lts",
    "Rts",
    "Lw",
    "Rw",
    "LFE2",
    "Bfl",
    "Bfr",
    "Bfc",
    "W",
    "X",
    "Y",
    "Z",
    "Ls/Rs",
    "Lrs/Rrs",
    "Lc/Rc",
    "Ltf/Rtf",
    "Ltr/Rtr",
    "Lts/Rts",
    "Lw/Rw",
    "Bfl/Bfr"
};


const juce::StringArray kPhaseModeChoices {
    "Real-time",
    "Natural",
    "Linear"
};

const juce::StringArray kLinearQualityChoices {
    "Low",
    "Medium",
    "High",
    "Very High",
    "Intensive"
};

const juce::StringArray kLinearWindowChoices {
    "Hann",
    "Blackman",
    "Kaiser"
};

const juce::StringArray kOversamplingChoices {
    "Off",
    "2x",
    "4x"
};
} // namespace

juce::String EQProAudioProcessor::sharedStateClipboard;

EQProAudioProcessor::EQProAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, &undoManager, "PARAMETERS", createParameterLayout())
{
    initializeParamPointers();
    startTimerHz(10);
}

EQProAudioProcessor::~EQProAudioProcessor()
{
    stopTimer();
}

void EQProAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    eqDsp.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
    eqDsp.reset();
    meteringDsp.prepare(sampleRate);
    spectralDsp.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
    spectralDsp.reset();
    linearPhaseEq.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
    linearPhaseEq.reset();
    linearPhaseMsEq.prepare(sampleRate, samplesPerBlock, 2);
    linearPhaseMsEq.reset();
    lastSampleRate = sampleRate;
    lastMaxBlockSize = samplesPerBlock;
    updateOversampling();

    outputTrimGainSmoothed.reset(sampleRate, 0.02);
    outputTrimGainSmoothed.setCurrentAndTargetValue(1.0f);
    globalMixSmoothed.reset(sampleRate, 0.02);
    globalMixSmoothed.setCurrentAndTargetValue(1.0f);

    dryBuffer.setSize(getTotalNumInputChannels(), samplesPerBlock);
    dryBuffer.clear();

    constexpr int analyzerBufferSize = 16384;
    analyzerPreFifo.prepare(analyzerBufferSize);
    analyzerPostFifo.prepare(analyzerBufferSize);
    analyzerExternalFifo.prepare(analyzerBufferSize);
}

void EQProAudioProcessor::releaseResources()
{
}

bool EQProAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainInput = layouts.getChannelSet(true, 0);
    const auto mainOutput = layouts.getChannelSet(false, 0);

    if (mainInput.isDisabled() || mainOutput.isDisabled())
        return false;

    if (mainInput != mainOutput)
        return false;

    const int channels = mainInput.size();
    if (channels < 1 || channels > ParamIDs::kMaxChannels)
        return false;

    const auto sidechain = layouts.getChannelSet(true, 1);
    if (! sidechain.isDisabled())
    {
        const int scChannels = sidechain.size();
        if (scChannels < 1 || scChannels > 2)
            return false;
    }

    return true;
}

void EQProAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numChannels = juce::jmin(buffer.getNumChannels(), ParamIDs::kMaxChannels);
    const bool globalBypass = globalBypassParam != nullptr && globalBypassParam->load() > 0.5f;
    eqDsp.setGlobalBypass(globalBypass);
    eqDsp.setSmartSoloEnabled(smartSoloParam != nullptr && smartSoloParam->load() > 0.5f);
    eqDspOversampled.setGlobalBypass(globalBypass);
    eqDspOversampled.setSmartSoloEnabled(smartSoloParam != nullptr && smartSoloParam->load() > 0.5f);
    const int qMode = qModeParam != nullptr ? static_cast<int>(qModeParam->load()) : 0;
    const float qAmount = qModeAmountParam != nullptr ? qModeAmountParam->load() : 50.0f;
    eqDsp.setQMode(qMode);
    eqDsp.setQModeAmount(qAmount);
    eqDspOversampled.setQMode(qMode);
    eqDspOversampled.setQModeAmount(qAmount);

    const bool midiLearnEnabled = midiLearnParam != nullptr && midiLearnParam->load() > 0.5f;
    if (midiLearnEnabled || learnedMidiCC.load() >= 0)
    {
        for (const auto metadata : midiMessages)
        {
            const auto msg = metadata.getMessage();
            if (! msg.isController())
                continue;

            const int cc = msg.getControllerNumber();
            if (midiLearnEnabled && learnedMidiCC.load() < 0)
                learnedMidiCC.store(cc);

            if (cc != learnedMidiCC.load())
                continue;

            const float value = msg.getControllerValue() / 127.0f;
            const int channel = juce::jlimit(0, ParamIDs::kMaxChannels - 1, selectedChannelIndex.load());
            const int band = juce::jlimit(0, ParamIDs::kBandsPerChannel - 1, selectedBandIndex.load());
            const int target = midiTargetParam != nullptr ? static_cast<int>(midiTargetParam->load()) : 0;
            const char* suffix = (target == 1 ? kParamFreqSuffix : (target == 2 ? kParamQSuffix : kParamGainSuffix));
            if (auto* param = parameters.getParameter(ParamIDs::bandParamId(channel, band, suffix)))
                param->setValueNotifyingHost(value);
        }
    }

    const bool sidechainEnabled = getBusCount(true) > 1 && getBus(true, 1) != nullptr
        && getBus(true, 1)->isEnabled();
    const juce::AudioBuffer<float>* detectorBuffer = nullptr;
    if (sidechainEnabled)
        detectorBuffer = &getBusBuffer(buffer, true, 1);

    if (correlationPairs.empty())
    {
        if (numChannels >= 2)
            meteringDsp.setCorrelationPair(0, 1);
    }
    else
    {
        const int safeIndex = juce::jlimit(0, static_cast<int>(correlationPairs.size() - 1),
                                           correlationPairIndex);
        const auto pair = correlationPairs[static_cast<size_t>(safeIndex)];
        meteringDsp.setCorrelationPair(pair.first, pair.second);
    }

    if (buffer.getNumChannels() > 0)
        analyzerPreFifo.push(buffer.getReadPointer(0), buffer.getNumSamples());

    const float globalMixTarget = globalMixParam != nullptr ? (globalMixParam->load() / 100.0f) : 1.0f;
    globalMixSmoothed.setTargetValue(globalMixTarget);
    const bool applyGlobalMix = globalMixParam != nullptr
        && (globalMixSmoothed.isSmoothing() || std::abs(globalMixTarget - 1.0f) > 0.0001f);
    if (applyGlobalMix)
    {
        const int copyChannels = juce::jmin(numChannels, dryBuffer.getNumChannels());
        for (int ch = 0; ch < copyChannels; ++ch)
            juce::FloatVectorOperations::copy(dryBuffer.getWritePointer(ch),
                                              buffer.getReadPointer(ch),
                                              buffer.getNumSamples());
    }

    std::array<int, ParamIDs::kBandsPerChannel> msTargets {};
    std::array<uint32_t, ParamIDs::kBandsPerChannel> bandChannelMasks {};
    const uint32_t maskAll = (numChannels >= 32)
        ? 0xFFFFFFFFu
        : (numChannels > 0 ? ((1u << static_cast<uint32_t>(numChannels)) - 1u) : 0u);
    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
    {
        msTargets[band] = 0;
        bandChannelMasks[band] = maskAll;
    }

    const auto& channelNames = cachedChannelNames;
    auto findIndex = [&channelNames](const juce::String& name) -> int
    {
        for (int i = 0; i < static_cast<int>(channelNames.size()); ++i)
            if (channelNames[static_cast<size_t>(i)] == name)
                return i;
        return -1;
    };
    auto maskFor = [&](const juce::String& name) -> uint32_t
    {
        const int idx = findIndex(name);
        return idx >= 0 ? (1u << static_cast<uint32_t>(idx)) : 0u;
    };
    auto maskForPair = [&](const juce::String& left, const juce::String& right) -> uint32_t
    {
        return maskFor(left) | maskFor(right);
    };

    for (int ch = 0; ch < numChannels; ++ch)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            const auto& ptrs = bandParamPointers[ch][band];
            if (ptrs.frequency == nullptr)
                continue;

            eqdsp::BandParams params;
            params.frequencyHz = ptrs.frequency->load();
            params.gainDb = ptrs.gain->load();
            params.q = ptrs.q->load();
            params.type = static_cast<eqdsp::FilterType>(static_cast<int>(ptrs.type->load()));
            params.slopeDb = ptrs.slope != nullptr ? ptrs.slope->load() : 12.0f;
            params.bypassed = ptrs.bypass->load() > 0.5f;
            params.solo = ptrs.solo != nullptr && ptrs.solo->load() > 0.5f;
            params.mix = ptrs.mix != nullptr ? (ptrs.mix->load() / 100.0f) : 1.0f;
            params.dynamicEnabled = ptrs.dynEnable != nullptr && ptrs.dynEnable->load() > 0.5f;
            params.dynamicMode = ptrs.dynMode != nullptr ? static_cast<int>(ptrs.dynMode->load()) : 0;
            params.thresholdDb = ptrs.dynThreshold != nullptr ? ptrs.dynThreshold->load() : -24.0f;
            params.attackMs = ptrs.dynAttack != nullptr ? ptrs.dynAttack->load() : 20.0f;
            params.releaseMs = ptrs.dynRelease != nullptr ? ptrs.dynRelease->load() : 200.0f;
            params.autoScale = ptrs.dynAuto != nullptr && ptrs.dynAuto->load() > 0.5f;

            eqDsp.updateBandParams(ch, band, params);

            if (ch == 0)
            {
                eqDsp.updateMsBandParams(band, params);
            }
        }
    }

    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
    {
        const auto& ptrs = bandParamPointers[0][band];
        const int target = ptrs.msTarget != nullptr ? static_cast<int>(ptrs.msTarget->load()) : 0;
        int msTarget = 0;
        uint32_t mask = maskAll;

        switch (target)
        {
            case 0: // All
                mask = maskAll;
                break;
            case 1: // Mid
                msTarget = 1;
                mask = maskForPair("L", "R");
                break;
            case 2: // Side
                msTarget = 2;
                mask = maskForPair("L", "R");
                break;
            case 3: // Left
                mask = maskFor("L");
                break;
            case 4: // Right
                mask = maskFor("R");
                break;
            case 5: // L/R
                mask = maskForPair("L", "R");
                break;
            case 6: // Mono
                msTarget = 1;
                mask = maskForPair("L", "R");
                break;
            case 7: mask = maskFor("C"); break;
            case 8: mask = maskFor("LFE"); break;
            case 9: mask = maskFor("Ls"); break;
            case 10: mask = maskFor("Rs"); break;
            case 11: mask = maskFor("Lrs"); break;
            case 12: mask = maskFor("Rrs"); break;
            case 13: mask = maskFor("Lc"); break;
            case 14: mask = maskFor("Rc"); break;
            case 15: mask = maskFor("Ltf"); break;
            case 16: mask = maskFor("Rtf"); break;
            case 17: mask = maskFor("Tfc"); break;
            case 18: mask = maskFor("Tm"); break;
            case 19: mask = maskFor("Ltr"); break;
            case 20: mask = maskFor("Rtr"); break;
            case 21: mask = maskFor("Trc"); break;
            case 22: mask = maskFor("Lts"); break;
            case 23: mask = maskFor("Rts"); break;
            case 24: mask = maskFor("Lw"); break;
            case 25: mask = maskFor("Rw"); break;
            case 26: mask = maskFor("LFE2"); break;
            case 27: mask = maskFor("Bfl"); break;
            case 28: mask = maskFor("Bfr"); break;
            case 29: mask = maskFor("Bfc"); break;
            case 30: mask = maskFor("W"); break;
            case 31: mask = maskFor("X"); break;
            case 32: mask = maskFor("Y"); break;
            case 33: mask = maskFor("Z"); break;
            case 34: mask = maskForPair("Ls", "Rs"); break;
            case 35: mask = maskForPair("Lrs", "Rrs"); break;
            case 36: mask = maskForPair("Lc", "Rc"); break;
            case 37: mask = maskForPair("Ltf", "Rtf"); break;
            case 38: mask = maskForPair("Ltr", "Rtr"); break;
            case 39: mask = maskForPair("Lts", "Rts"); break;
            case 40: mask = maskForPair("Lw", "Rw"); break;
            case 41: mask = maskForPair("Bfl", "Bfr"); break;
            default:
                mask = maskAll;
                break;
        }

        msTargets[band] = msTarget;
        bandChannelMasks[band] = mask;
    }

    const int phaseMode = phaseModeParam != nullptr
        ? static_cast<int>(phaseModeParam->load())
        : 0;

    const bool useOversampling = (phaseMode == 0 && oversamplingIndex > 0 && oversampler != nullptr);
    bool characterApplied = false;
    if (useOversampling)
    {
        auto block = juce::dsp::AudioBlock<float>(buffer);
        auto upBlock = oversampler->processSamplesUp(block);
        const int upSamples = static_cast<int>(upBlock.getNumSamples());
        const int channels = juce::jmin(buffer.getNumChannels(), oversampledBuffer.getNumChannels());

        for (int ch = 0; ch < channels; ++ch)
            juce::FloatVectorOperations::copy(oversampledBuffer.getWritePointer(ch),
                                              upBlock.getChannelPointer(ch), upSamples);

        eqDspOversampled.setMsTargets(msTargets);
        eqDspOversampled.setBandChannelMasks(bandChannelMasks);
        eqDspOversampled.process(oversampledBuffer, nullptr);


        const int characterMode = characterModeParam != nullptr
            ? static_cast<int>(characterModeParam->load())
            : 0;
        if (characterMode > 0)
        {
            const float drive = (characterMode == 1) ? 1.5f : 2.5f;
            const float norm = std::tanh(drive);
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* data = oversampledBuffer.getWritePointer(ch);
                for (int i = 0; i < upSamples; ++i)
                {
                    const float x = data[i] * drive;
                    data[i] = std::tanh(x) / norm;
                }
            }
            characterApplied = true;
        }

        for (int ch = 0; ch < channels; ++ch)
            juce::FloatVectorOperations::copy(upBlock.getChannelPointer(ch),
                                              oversampledBuffer.getReadPointer(ch), upSamples);

        oversampler->processSamplesDown(block);
    }
    else if (phaseMode == 0)
    {
        eqDsp.setMsTargets(msTargets);
        eqDsp.setBandChannelMasks(bandChannelMasks);
        eqDsp.process(buffer, detectorBuffer);
    }
    else
    {
        const bool useMs = numChannels >= 2
            && std::any_of(msTargets.begin(), msTargets.end(), [](int v) { return v != 0; });

        if (useMs)
        {
            auto* left = buffer.getWritePointer(0);
            auto* right = buffer.getWritePointer(1);
            const int samples = buffer.getNumSamples();

            for (int i = 0; i < samples; ++i)
            {
                const float mid = 0.5f * (left[i] + right[i]);
                const float side = 0.5f * (left[i] - right[i]);
                left[i] = mid;
                right[i] = side;
            }

            linearPhaseMsEq.processRange(buffer, 0, 2);

            for (int i = 0; i < samples; ++i)
            {
                const float mid = left[i];
                const float side = right[i];
                left[i] = mid + side;
                right[i] = mid - side;
            }

            linearPhaseEq.processRange(buffer, 0, 2);

            if (numChannels > 2)
                linearPhaseEq.processRange(buffer, 2, numChannels - 2);
        }
        else
        {
            linearPhaseEq.process(buffer);
        }
    }


    const bool spectralEnabled = spectralEnableParam != nullptr
        && spectralEnableParam->load() > 0.5f;
    spectralDsp.setEnabled(spectralEnabled);
    if (spectralEnabled)
    {
        const float threshold = spectralThresholdParam != nullptr ? spectralThresholdParam->load() : -24.0f;
        const float ratio = spectralRatioParam != nullptr ? spectralRatioParam->load() : 2.0f;
        const float attack = spectralAttackParam != nullptr ? spectralAttackParam->load() : 20.0f;
        const float release = spectralReleaseParam != nullptr ? spectralReleaseParam->load() : 200.0f;
        const float mix = spectralMixParam != nullptr ? (spectralMixParam->load() / 100.0f) : 1.0f;
        spectralDsp.setParams(threshold, ratio, attack, release, mix);
        spectralDsp.process(buffer);
    }

    const int characterMode = characterModeParam != nullptr
        ? static_cast<int>(characterModeParam->load())
        : 0;
    if (characterMode > 0 && ! characterApplied)
    {
        const float drive = (characterMode == 1) ? 1.5f : 2.5f;
        const float norm = std::tanh(drive);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float x = data[i] * drive;
                data[i] = std::tanh(x) / norm;
            }
        }
    }

    if (autoGainEnableParam != nullptr && autoGainEnableParam->load() > 0.5f)
    {
        float sumDb = 0.0f;
        int count = 0;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
            {
                const auto& ptrs = bandParamPointers[ch][band];
                if (ptrs.gain == nullptr || ptrs.bypass == nullptr || ptrs.type == nullptr)
                    continue;
                if (ptrs.bypass->load() > 0.5f)
                    continue;
                const int type = static_cast<int>(ptrs.type->load());
                if (type == static_cast<int>(eqdsp::FilterType::lowPass)
                    || type == static_cast<int>(eqdsp::FilterType::highPass)
                    || type == static_cast<int>(eqdsp::FilterType::allPass))
                    continue;
                sumDb += ptrs.gain->load();
                ++count;
            }
        }

        if (count > 0)
        {
            const float avgDb = sumDb / static_cast<float>(count);
            const float scale = gainScaleParam != nullptr ? (gainScaleParam->load() / 100.0f) : 1.0f;
            const float autoGainDb = juce::jlimit(-12.0f, 12.0f, -avgDb * scale);
            buffer.applyGain(juce::Decibels::decibelsToGain(autoGainDb));
        }
    }

    if (applyGlobalMix)
    {
        const int mixChannels = juce::jmin(numChannels, dryBuffer.getNumChannels());
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float wet = globalMixSmoothed.getNextValue();
            const float dryGain = 1.0f - wet;
            for (int ch = 0; ch < mixChannels; ++ch)
            {
                auto* data = buffer.getWritePointer(ch);
                const float dry = dryBuffer.getReadPointer(ch)[i];
                data[i] = dry * dryGain + data[i] * wet;
            }
        }
    }

    if (phaseInvertParam != nullptr && phaseInvertParam->load() > 0.5f)
        buffer.applyGain(-1.0f);

    if (outputTrimParam != nullptr)
    {
        const float trimDb = outputTrimParam->load();
        const float targetGain = juce::Decibels::decibelsToGain(trimDb);
        outputTrimGainSmoothed.setTargetValue(targetGain);
        if (outputTrimGainSmoothed.isSmoothing()
            || std::abs(targetGain - 1.0f) > 0.0001f)
        {
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float gain = outputTrimGainSmoothed.getNextValue();
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.getWritePointer(ch)[i] *= gain;
            }
        }
    }

    meteringDsp.process(buffer, numChannels);

    if (buffer.getNumChannels() > 0)
        analyzerPostFifo.push(buffer.getReadPointer(0), buffer.getNumSamples());

    if (analyzerExternalParam != nullptr && analyzerExternalParam->load() > 0.5f && detectorBuffer != nullptr
        && detectorBuffer->getNumChannels() > 0)
    {
        analyzerExternalFifo.push(detectorBuffer->getReadPointer(0), detectorBuffer->getNumSamples());
    }
}

juce::AudioProcessorEditor* EQProAudioProcessor::createEditor()
{
    return new EQProAudioProcessorEditor(*this);
}

bool EQProAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String EQProAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EQProAudioProcessor::acceptsMidi() const
{
    return true;
}

bool EQProAudioProcessor::producesMidi() const
{
    return false;
}

bool EQProAudioProcessor::isMidiEffect() const
{
    return false;
}

double EQProAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EQProAudioProcessor::getNumPrograms()
{
    return 1;
}

int EQProAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EQProAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String EQProAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void EQProAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void EQProAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    parameters.state.setProperty("showPhase", showPhasePreference, nullptr);
    parameters.state.setProperty("presetSelection", presetSelection, nullptr);
    parameters.state.setProperty("presetApplyTarget", presetApplyTarget, nullptr);
    parameters.state.setProperty("snapshotA", snapshotA, nullptr);
    parameters.state.setProperty("snapshotB", snapshotB, nullptr);
    parameters.state.setProperty("snapshotC", snapshotC, nullptr);
    parameters.state.setProperty("snapshotD", snapshotD, nullptr);
    parameters.state.setProperty("darkTheme", darkTheme, nullptr);
    parameters.state.setProperty("themeMode", themeMode, nullptr);
    parameters.state.setProperty("correlationPairIndex", correlationPairIndex, nullptr);
    parameters.state.setProperty("favoritePresets", favoritePresets, nullptr);
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void EQProAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));

    showPhasePreference = parameters.state.getProperty("showPhase", true);
    presetSelection = static_cast<int>(parameters.state.getProperty("presetSelection", 0));
    presetApplyTarget = static_cast<int>(parameters.state.getProperty("presetApplyTarget", 0));
    snapshotA = parameters.state.getProperty("snapshotA", "");
    snapshotB = parameters.state.getProperty("snapshotB", "");
    snapshotC = parameters.state.getProperty("snapshotC", "");
    snapshotD = parameters.state.getProperty("snapshotD", "");
    darkTheme = parameters.state.getProperty("darkTheme", true);
    themeMode = static_cast<int>(parameters.state.getProperty("themeMode", darkTheme ? 0 : 1));
    darkTheme = (themeMode == 0);
    correlationPairIndex = static_cast<int>(parameters.state.getProperty("correlationPairIndex", 0));
    favoritePresets = parameters.state.getProperty("favoritePresets", "");
}

juce::AudioProcessorValueTreeState& EQProAudioProcessor::getParameters()
{
    return parameters;
}

AudioFifo& EQProAudioProcessor::getAnalyzerPreFifo()
{
    return analyzerPreFifo;
}

AudioFifo& EQProAudioProcessor::getAnalyzerPostFifo()
{
    return analyzerPostFifo;
}

AudioFifo& EQProAudioProcessor::getAnalyzerExternalFifo()
{
    return analyzerExternalFifo;
}

std::vector<juce::String> EQProAudioProcessor::getCurrentChannelNames() const
{
    const auto* bus = getBus(true, 0);
    if (bus == nullptr)
        bus = getBus(false, 0);

    if (bus != nullptr)
        return ChannelLayoutUtils::getChannelNames(bus->getCurrentLayout());

    return { "Ch 1" };
}

juce::String EQProAudioProcessor::getCurrentLayoutDescription() const
{
    const auto* bus = getBus(true, 0);
    if (bus == nullptr)
        bus = getBus(false, 0);

    if (bus != nullptr)
        return bus->getCurrentLayout().getDescription();

    return "Unknown";
}

eqdsp::ChannelMeterState EQProAudioProcessor::getMeterState(int channelIndex) const
{
    return meteringDsp.getChannelState(channelIndex);
}

float EQProAudioProcessor::getCorrelation() const
{
    return meteringDsp.getCorrelation();
}

juce::StringArray EQProAudioProcessor::getCorrelationPairNames()
{
    const int channelCount = juce::jlimit(0, ParamIDs::kMaxChannels, getTotalNumInputChannels());
    if (channelCount != correlationChannelCount || correlationPairs.empty())
    {
        correlationPairs.clear();
        correlationChannelCount = channelCount;
        const auto names = getCurrentChannelNames();
        for (int i = 0; i < channelCount; ++i)
        {
            for (int j = i + 1; j < channelCount; ++j)
                correlationPairs.emplace_back(i, j);
        }
    }

    juce::StringArray labels;
    const auto names = getCurrentChannelNames();
    for (const auto& pair : correlationPairs)
    {
        const auto left = pair.first < static_cast<int>(names.size())
            ? names[static_cast<size_t>(pair.first)]
            : ("Ch " + juce::String(pair.first + 1));
        const auto right = pair.second < static_cast<int>(names.size())
            ? names[static_cast<size_t>(pair.second)]
            : ("Ch " + juce::String(pair.second + 1));
        labels.add(left + "/" + right);
    }

    if (labels.isEmpty())
        labels.add("L/R");

    return labels;
}

void EQProAudioProcessor::setCorrelationPairIndex(int index)
{
    correlationPairIndex = index;
    parameters.state.setProperty("correlationPairIndex", correlationPairIndex, nullptr);
}

int EQProAudioProcessor::getCorrelationPairIndex() const
{
    return correlationPairIndex;
}

void EQProAudioProcessor::setShowPhasePreference(bool enabled)
{
    showPhasePreference = enabled;
    parameters.state.setProperty("showPhase", showPhasePreference, nullptr);
}

bool EQProAudioProcessor::getShowPhasePreference() const
{
    return showPhasePreference;
}

juce::UndoManager* EQProAudioProcessor::getUndoManager()
{
    return &undoManager;
}

void EQProAudioProcessor::setPresetSelection(int index)
{
    presetSelection = index;
    parameters.state.setProperty("presetSelection", presetSelection, nullptr);
}

int EQProAudioProcessor::getPresetSelection() const
{
    return presetSelection;
}

void EQProAudioProcessor::setPresetApplyTarget(int index)
{
    presetApplyTarget = index;
    parameters.state.setProperty("presetApplyTarget", presetApplyTarget, nullptr);
}

int EQProAudioProcessor::getPresetApplyTarget() const
{
    return presetApplyTarget;
}

void EQProAudioProcessor::storeSnapshotA()
{
    if (auto xml = parameters.copyState().createXml())
        snapshotA = xml->toString();
    parameters.state.setProperty("snapshotA", snapshotA, nullptr);
}

void EQProAudioProcessor::storeSnapshotB()
{
    if (auto xml = parameters.copyState().createXml())
        snapshotB = xml->toString();
    parameters.state.setProperty("snapshotB", snapshotB, nullptr);
}

void EQProAudioProcessor::recallSnapshotA()
{
    if (snapshotA.isNotEmpty())
        parameters.replaceState(juce::ValueTree::fromXml(snapshotA));
}

void EQProAudioProcessor::recallSnapshotB()
{
    if (snapshotB.isNotEmpty())
        parameters.replaceState(juce::ValueTree::fromXml(snapshotB));
}

void EQProAudioProcessor::storeSnapshotC()
{
    if (auto xml = parameters.copyState().createXml())
        snapshotC = xml->toString();
    parameters.state.setProperty("snapshotC", snapshotC, nullptr);
}

void EQProAudioProcessor::storeSnapshotD()
{
    if (auto xml = parameters.copyState().createXml())
        snapshotD = xml->toString();
    parameters.state.setProperty("snapshotD", snapshotD, nullptr);
}

void EQProAudioProcessor::recallSnapshotC()
{
    if (snapshotC.isNotEmpty())
        parameters.replaceState(juce::ValueTree::fromXml(snapshotC));
}

void EQProAudioProcessor::recallSnapshotD()
{
    if (snapshotD.isNotEmpty())
        parameters.replaceState(juce::ValueTree::fromXml(snapshotD));
}

void EQProAudioProcessor::setDarkTheme(bool enabled)
{
    darkTheme = enabled;
    parameters.state.setProperty("darkTheme", darkTheme, nullptr);
    themeMode = darkTheme ? 0 : 1;
    parameters.state.setProperty("themeMode", themeMode, nullptr);
}

bool EQProAudioProcessor::getDarkTheme() const
{
    return darkTheme;
}

void EQProAudioProcessor::setThemeMode(int mode)
{
    themeMode = juce::jlimit(0, 1, mode);
    darkTheme = (themeMode == 0);
    parameters.state.setProperty("themeMode", themeMode, nullptr);
    parameters.state.setProperty("darkTheme", darkTheme, nullptr);
}

int EQProAudioProcessor::getThemeMode() const
{
    return themeMode;
}

void EQProAudioProcessor::setFavoritePresets(const juce::String& names)
{
    favoritePresets = names;
    parameters.state.setProperty("favoritePresets", favoritePresets, nullptr);
}

juce::String EQProAudioProcessor::getFavoritePresets() const
{
    return favoritePresets;
}

void EQProAudioProcessor::copyStateToClipboard()
{
    if (auto xml = parameters.copyState().createXml())
        sharedStateClipboard = xml->toString();
}

void EQProAudioProcessor::pasteStateFromClipboard()
{
    if (sharedStateClipboard.isNotEmpty())
        parameters.replaceState(juce::ValueTree::fromXml(sharedStateClipboard));
}

void EQProAudioProcessor::setSelectedBandIndex(int index)
{
    selectedBandIndex.store(juce::jlimit(0, ParamIDs::kBandsPerChannel - 1, index));
}

void EQProAudioProcessor::setSelectedChannelIndex(int index)
{
    selectedChannelIndex.store(juce::jlimit(0, ParamIDs::kMaxChannels - 1, index));
}

int EQProAudioProcessor::getSelectedBandIndex() const
{
    return selectedBandIndex.load();
}

int EQProAudioProcessor::getSelectedChannelIndex() const
{
    return selectedChannelIndex.load();
}

float EQProAudioProcessor::getBandDetectorDb(int channelIndex, int bandIndex) const
{
    return eqDsp.getDetectorDb(channelIndex, bandIndex);
}

void EQProAudioProcessor::initializeParamPointers()
{
    globalBypassParam = parameters.getRawParameterValue(ParamIDs::globalBypass);
    globalMixParam = parameters.getRawParameterValue(ParamIDs::globalMix);
    phaseModeParam = parameters.getRawParameterValue(ParamIDs::phaseMode);
    linearQualityParam = parameters.getRawParameterValue(ParamIDs::linearQuality);
    linearWindowParam = parameters.getRawParameterValue(ParamIDs::linearWindow);
    oversamplingParam = parameters.getRawParameterValue(ParamIDs::oversampling);
    outputTrimParam = parameters.getRawParameterValue(ParamIDs::outputTrim);
    spectralEnableParam = parameters.getRawParameterValue(ParamIDs::spectralEnable);
    spectralThresholdParam = parameters.getRawParameterValue(ParamIDs::spectralThreshold);
    spectralRatioParam = parameters.getRawParameterValue(ParamIDs::spectralRatio);
    spectralAttackParam = parameters.getRawParameterValue(ParamIDs::spectralAttack);
    spectralReleaseParam = parameters.getRawParameterValue(ParamIDs::spectralRelease);
    spectralMixParam = parameters.getRawParameterValue(ParamIDs::spectralMix);
    characterModeParam = parameters.getRawParameterValue(ParamIDs::characterMode);
    qModeParam = parameters.getRawParameterValue(ParamIDs::qMode);
    qModeAmountParam = parameters.getRawParameterValue(ParamIDs::qModeAmount);
    analyzerExternalParam = parameters.getRawParameterValue(ParamIDs::analyzerExternal);
    autoGainEnableParam = parameters.getRawParameterValue(ParamIDs::autoGainEnable);
    gainScaleParam = parameters.getRawParameterValue(ParamIDs::gainScale);
    phaseInvertParam = parameters.getRawParameterValue(ParamIDs::phaseInvert);
    midiLearnParam = parameters.getRawParameterValue(ParamIDs::midiLearn);
    midiTargetParam = parameters.getRawParameterValue(ParamIDs::midiTarget);
    smartSoloParam = parameters.getRawParameterValue(ParamIDs::smartSolo);

    for (int ch = 0; ch < ParamIDs::kMaxChannels; ++ch)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            bandParamPointers[ch][band].frequency =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamFreqSuffix));
            bandParamPointers[ch][band].gain =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamGainSuffix));
            bandParamPointers[ch][band].q =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamQSuffix));
            bandParamPointers[ch][band].type =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamTypeSuffix));
            bandParamPointers[ch][band].bypass =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamBypassSuffix));
            bandParamPointers[ch][band].msTarget =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamMsSuffix));
            bandParamPointers[ch][band].slope =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamSlopeSuffix));
            bandParamPointers[ch][band].solo =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamSoloSuffix));
            bandParamPointers[ch][band].mix =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamMixSuffix));
            bandParamPointers[ch][band].dynEnable =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamDynEnableSuffix));
            bandParamPointers[ch][band].dynMode =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamDynModeSuffix));
            bandParamPointers[ch][band].dynThreshold =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamDynThreshSuffix));
            bandParamPointers[ch][band].dynAttack =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamDynAttackSuffix));
            bandParamPointers[ch][band].dynRelease =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamDynReleaseSuffix));
            bandParamPointers[ch][band].dynAuto =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamDynAutoSuffix));
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout EQProAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.reserve(ParamIDs::kMaxChannels * ParamIDs::kBandsPerChannel * 21 + 8);

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::globalBypass, "Global Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::globalMix, "Global Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        100.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::phaseMode, "Phase Mode",
        kPhaseModeChoices, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::linearQuality, "Linear Quality",
        kLinearQualityChoices, 1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::linearWindow, "Linear Window",
        kLinearWindowChoices, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::oversampling, "Oversampling",
        kOversamplingChoices, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::outputTrim, "Output Trim",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f),
        0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::spectralEnable, "Spectral Enable",
        false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::spectralThreshold, "Spectral Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f),
        -24.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::spectralRatio, "Spectral Ratio",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.01f),
        2.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::spectralAttack, "Spectral Attack",
        juce::NormalisableRange<float>(1.0f, 200.0f, 0.1f),
        20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::spectralRelease, "Spectral Release",
        juce::NormalisableRange<float>(5.0f, 1000.0f, 0.1f),
        200.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::spectralMix, "Spectral Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        100.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::characterMode, "Character Mode",
        juce::StringArray("Off", "Gentle", "Warm"),
        0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::qMode, "Q Mode",
        juce::StringArray("Constant", "Proportional"),
        0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::qModeAmount, "Q Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::analyzerRange, "Analyzer Range",
        juce::StringArray("3 dB", "6 dB", "12 dB", "30 dB"),
        2));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::analyzerSpeed, "Analyzer Speed",
        juce::StringArray("Slow", "Normal", "Fast"),
        1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::analyzerView, "Analyzer View",
        juce::StringArray("Both", "Pre", "Post"),
        0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::analyzerFreeze, "Analyzer Freeze",
        false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::analyzerExternal, "Analyzer External",
        false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::autoGainEnable, "Auto Gain",
        false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::gainScale, "Gain Scale",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        100.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::phaseInvert, "Phase Invert",
        false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::midiLearn, "MIDI Learn",
        false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::midiTarget, "MIDI Target",
        juce::StringArray("Gain", "Freq", "Q"),
        0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::smartSolo, "Smart Solo",
        false));

    const juce::NormalisableRange<float> freqRange(20.0f, 192000.0f, 0.01f, 0.5f);
    const juce::NormalisableRange<float> gainRange(-48.0f, 48.0f, 0.01f);
    const juce::NormalisableRange<float> qRange(0.1f, 18.0f, 0.01f, 0.5f);

    for (int ch = 0; ch < ParamIDs::kMaxChannels; ++ch)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamFreqSuffix),
                ParamIDs::bandParamName(ch, band, "Freq"),
                freqRange,
                1000.0f));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamGainSuffix),
                ParamIDs::bandParamName(ch, band, "Gain"),
                gainRange,
                0.0f));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamQSuffix),
                ParamIDs::bandParamName(ch, band, "Q"),
                qRange,
                0.707f));

            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                ParamIDs::bandParamId(ch, band, kParamTypeSuffix),
                ParamIDs::bandParamName(ch, band, "Type"),
                kFilterTypeChoices,
                0));

            params.push_back(std::make_unique<juce::AudioParameterBool>(
                ParamIDs::bandParamId(ch, band, kParamBypassSuffix),
                ParamIDs::bandParamName(ch, band, "Bypass"),
                true));

            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                ParamIDs::bandParamId(ch, band, kParamMsSuffix),
                ParamIDs::bandParamName(ch, band, "M/S"),
                kMsChoices,
                0));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamSlopeSuffix),
                ParamIDs::bandParamName(ch, band, "Slope"),
                juce::NormalisableRange<float>(6.0f, 96.0f, 0.1f),
                12.0f));

            params.push_back(std::make_unique<juce::AudioParameterBool>(
                ParamIDs::bandParamId(ch, band, kParamSoloSuffix),
                ParamIDs::bandParamName(ch, band, "Solo"),
                false));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamMixSuffix),
                ParamIDs::bandParamName(ch, band, "Mix"),
                juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
                100.0f));

            params.push_back(std::make_unique<juce::AudioParameterBool>(
                ParamIDs::bandParamId(ch, band, kParamDynEnableSuffix),
                ParamIDs::bandParamName(ch, band, "Dyn Enable"),
                false));

            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                ParamIDs::bandParamId(ch, band, kParamDynModeSuffix),
                ParamIDs::bandParamName(ch, band, "Dyn Mode"),
                juce::StringArray("Up", "Down"),
                0));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamDynThreshSuffix),
                ParamIDs::bandParamName(ch, band, "Dyn Threshold"),
                juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f),
                -24.0f));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamDynAttackSuffix),
                ParamIDs::bandParamName(ch, band, "Dyn Attack"),
                juce::NormalisableRange<float>(1.0f, 200.0f, 0.1f),
                20.0f));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamDynReleaseSuffix),
                ParamIDs::bandParamName(ch, band, "Dyn Release"),
                juce::NormalisableRange<float>(5.0f, 1000.0f, 0.1f),
                200.0f));

            params.push_back(std::make_unique<juce::AudioParameterBool>(
                ParamIDs::bandParamId(ch, band, kParamDynAutoSuffix),
                ParamIDs::bandParamName(ch, band, "Dyn Auto Scale"),
                true));

        }
    }

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQProAudioProcessor();
}

void EQProAudioProcessor::timerCallback()
{
    cachedChannelNames = getCurrentChannelNames();
    const int phaseMode = phaseModeParam != nullptr
        ? static_cast<int>(phaseModeParam->load())
        : 0;

    updateOversampling();

    if (phaseMode == 0)
    {
        setLatencySamples(0);
        lastPhaseMode = phaseMode;
        return;
    }

    const int quality = linearQualityParam != nullptr
        ? static_cast<int>(linearQualityParam->load())
        : 1;
    const int windowIndex = linearWindowParam != nullptr
        ? static_cast<int>(linearWindowParam->load())
        : 0;

    int taps = 256;
    if (phaseMode == 1)
    {
        switch (quality)
        {
            case 0: taps = 128; break;
            case 1: taps = 256; break;
            case 2: taps = 512; break;
            case 3: taps = 1024; break;
            case 4: taps = 2048; break;
            default: taps = 256; break;
        }
    }
    else
    {
        switch (quality)
        {
            case 0: taps = 512; break;
            case 1: taps = 1024; break;
            case 2: taps = 2048; break;
            case 3: taps = 4096; break;
            case 4: taps = 8192; break;
            default: taps = 1024; break;
        }
    }

    const int channels = getTotalNumInputChannels();
    const auto sampleRate = getSampleRate();
    if (sampleRate <= 0.0)
        return;

    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
    {
        if (bandParamPointers[0][band].msTarget != nullptr)
            lastMsTargets[band] = static_cast<int>(bandParamPointers[0][band].msTarget->load());
        else
            lastMsTargets[band] = 0;
    }

    const uint64_t hash = computeParamsHash(channels);
    if (hash == lastParamHash && taps == lastTaps && phaseMode == lastPhaseMode
        && quality == lastLinearQuality && windowIndex == lastWindowIndex)
        return;

    int headSize = 0;
    if (phaseMode == 1)
        headSize = taps / 2;
    else
    {
        switch (quality)
        {
            case 0: headSize = 128; break;
            case 1: headSize = 256; break;
            case 2: headSize = 512; break;
            case 3: headSize = 1024; break;
            case 4: headSize = 2048; break;
            default: headSize = 256; break;
        }
    }

    linearPhaseEq.configurePartitioning(headSize);
    linearPhaseMsEq.configurePartitioning(headSize);

    rebuildLinearPhase(taps, sampleRate, channels);
    lastParamHash = hash;
    lastTaps = taps;
    lastPhaseMode = phaseMode;
    lastLinearQuality = quality;
    lastWindowIndex = windowIndex;
}

uint64_t EQProAudioProcessor::computeParamsHash(int channels) const
{
    auto hash = uint64_t { 1469598103934665603ull };
    const auto hashFloat = [&hash](float value)
    {
        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        hash ^= bits;
        hash *= 1099511628211ull;
    };

    for (int ch = 0; ch < channels; ++ch)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            const auto& ptrs = bandParamPointers[ch][band];
            if (ptrs.frequency == nullptr)
                continue;

            hashFloat(ptrs.frequency->load());
            hashFloat(ptrs.gain->load());
            hashFloat(ptrs.q->load());
            hashFloat(ptrs.type->load());
            hashFloat(ptrs.bypass->load());
            if (ptrs.mix != nullptr)
                hashFloat(ptrs.mix->load());
            if (ptrs.slope != nullptr)
                hashFloat(ptrs.slope->load());

            if (ch == 0 && ptrs.msTarget != nullptr)
                hashFloat(ptrs.msTarget->load());
        }
    }

    return hash;
}

void EQProAudioProcessor::rebuildLinearPhase(int taps, double sampleRate, int channels)
{
    const int fftSize = juce::nextPowerOfTwo(taps * 2);
    const int fftOrder = static_cast<int>(std::log2(fftSize));
    if (fftSize != firFftSize || fftOrder != firFftOrder)
    {
        firFftSize = fftSize;
        firFftOrder = fftOrder;
        firFft = std::make_unique<juce::dsp::FFT>(firFftOrder);
        firData.assign(static_cast<size_t>(firFftSize) * 2, 0.0f);
    }

    if (static_cast<int>(firImpulse.size()) != taps)
        firImpulse.assign(static_cast<size_t>(taps), 0.0f);

    const int windowIndex = linearWindowParam != nullptr
        ? static_cast<int>(linearWindowParam->load())
        : 0;
    const auto method = windowIndex == 1
        ? juce::dsp::WindowingFunction<float>::blackman
        : (windowIndex == 2 ? juce::dsp::WindowingFunction<float>::kaiser : juce::dsp::WindowingFunction<float>::hann);

    if (firWindow == nullptr || static_cast<int>(firImpulse.size()) != taps
        || firWindowMethod != static_cast<int>(method))
    {
        firWindow = std::make_unique<juce::dsp::WindowingFunction<float>>(taps, method);
        firWindowMethod = static_cast<int>(method);
    }

    const auto& channelNames = cachedChannelNames.empty() ? getCurrentChannelNames() : cachedChannelNames;
    auto findIndex = [&channelNames](const juce::String& name) -> int
    {
        for (int i = 0; i < static_cast<int>(channelNames.size()); ++i)
            if (channelNames[static_cast<size_t>(i)] == name)
                return i;
        return -1;
    };
    auto maskFor = [&](const juce::String& name) -> uint32_t
    {
        const int idx = findIndex(name);
        return idx >= 0 ? (1u << static_cast<uint32_t>(idx)) : 0u;
    };
    auto maskForPair = [&](const juce::String& left, const juce::String& right) -> uint32_t
    {
        return maskFor(left) | maskFor(right);
    };
    std::array<uint32_t, ParamIDs::kBandsPerChannel> bandChannelMasks {};
    const uint32_t maskAll = (channels >= 32)
        ? 0xFFFFFFFFu
        : (channels > 0 ? ((1u << static_cast<uint32_t>(channels)) - 1u) : 0u);
    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
    {
        const int target = lastMsTargets[band];
        uint32_t mask = maskAll;
        switch (target)
        {
            case 0: mask = maskAll; break;
            case 1: mask = maskForPair("L", "R"); break;
            case 2: mask = maskForPair("L", "R"); break;
            case 3: mask = maskFor("L"); break;
            case 4: mask = maskFor("R"); break;
            case 5: mask = maskForPair("L", "R"); break;
            case 6: mask = maskForPair("L", "R"); break;
            case 7: mask = maskFor("C"); break;
            case 8: mask = maskFor("LFE"); break;
            case 9: mask = maskFor("Ls"); break;
            case 10: mask = maskFor("Rs"); break;
            case 11: mask = maskFor("Lrs"); break;
            case 12: mask = maskFor("Rrs"); break;
            case 13: mask = maskFor("Lc"); break;
            case 14: mask = maskFor("Rc"); break;
            case 15: mask = maskFor("Ltf"); break;
            case 16: mask = maskFor("Rtf"); break;
            case 17: mask = maskFor("Tfc"); break;
            case 18: mask = maskFor("Tm"); break;
            case 19: mask = maskFor("Ltr"); break;
            case 20: mask = maskFor("Rtr"); break;
            case 21: mask = maskFor("Trc"); break;
            case 22: mask = maskFor("Lts"); break;
            case 23: mask = maskFor("Rts"); break;
            case 24: mask = maskFor("Lw"); break;
            case 25: mask = maskFor("Rw"); break;
            case 26: mask = maskFor("LFE2"); break;
            case 27: mask = maskFor("Bfl"); break;
            case 28: mask = maskFor("Bfr"); break;
            case 29: mask = maskFor("Bfc"); break;
            case 30: mask = maskFor("W"); break;
            case 31: mask = maskFor("X"); break;
            case 32: mask = maskFor("Y"); break;
            case 33: mask = maskFor("Z"); break;
            case 34: mask = maskForPair("Ls", "Rs"); break;
            case 35: mask = maskForPair("Lrs", "Rrs"); break;
            case 36: mask = maskForPair("Lc", "Rc"); break;
            case 37: mask = maskForPair("Ltf", "Rtf"); break;
            case 38: mask = maskForPair("Ltr", "Rtr"); break;
            case 39: mask = maskForPair("Lts", "Rts"); break;
            case 40: mask = maskForPair("Lw", "Rw"); break;
            case 41: mask = maskForPair("Bfl", "Bfr"); break;
            default: mask = maskAll; break;
        }
        bandChannelMasks[band] = mask;
    }

    auto buildImpulse = [&](int channel, std::function<bool(int)> includeBand) -> juce::AudioBuffer<float>
    {
        std::fill(firData.begin(), firData.end(), 0.0f);
        const double nyquist = sampleRate * 0.5;

        for (int bin = 0; bin <= fftSize / 2; ++bin)
        {
            const double freq = (sampleRate * bin) / static_cast<double>(fftSize);
            if (freq > nyquist)
                continue;

            double totalMag = 1.0;
            for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
            {
                if (! includeBand(band))
                    continue;

                const auto& ptrs = bandParamPointers[channel][band];
                if (ptrs.frequency == nullptr)
                    continue;

                if (ptrs.bypass->load() > 0.5f)
                    continue;

                const double mix = ptrs.mix != nullptr ? juce::jlimit(0.0f, 100.0f, ptrs.mix->load()) / 100.0f : 1.0;
                if (mix <= 0.0001)
                    continue;
                const double gainDb = ptrs.gain->load() * mix;
                const double q = std::max(0.1f, ptrs.q->load());
                const double freqParam = ptrs.frequency->load();
                const int type = static_cast<int>(ptrs.type->load());
                const float slopeDb = ptrs.slope != nullptr ? ptrs.slope->load() : 12.0f;

                const double clampedFreq = juce::jlimit(10.0, nyquist * 0.99, freqParam);
                const double omega = 2.0 * juce::MathConstants<double>::pi
                    * clampedFreq / sampleRate;
                const double sinW = std::sin(omega);
                const double cosW = std::cos(omega);
                const double alpha = sinW / (2.0 * q);
                const double a = std::pow(10.0, gainDb / 40.0);

                auto computeMagForType = [&](eqdsp::FilterType filterType,
                                             double gainDbLocal,
                                             double qOverride)
                {
                    const double qLocal = (qOverride > 0.0) ? qOverride : q;
                    const double alphaLocal = sinW / (2.0 * qLocal);
                    const double aLocal = std::pow(10.0, gainDbLocal / 40.0);
                    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a0 = 1.0, a1 = 0.0, a2 = 0.0;

                    switch (filterType)
                    {
                        case eqdsp::FilterType::bell:
                            b0 = 1.0 + alphaLocal * aLocal;
                            b1 = -2.0 * cosW;
                            b2 = 1.0 - alphaLocal * aLocal;
                            a0 = 1.0 + alphaLocal / aLocal;
                            a1 = -2.0 * cosW;
                            a2 = 1.0 - alphaLocal / aLocal;
                            break;
                        case eqdsp::FilterType::lowShelf:
                        {
                            const double beta = std::sqrt(aLocal) / qLocal;
                            b0 = aLocal * ((aLocal + 1.0) - (aLocal - 1.0) * cosW + beta * sinW);
                            b1 = 2.0 * aLocal * ((aLocal - 1.0) - (aLocal + 1.0) * cosW);
                            b2 = aLocal * ((aLocal + 1.0) - (aLocal - 1.0) * cosW - beta * sinW);
                            a0 = (aLocal + 1.0) + (aLocal - 1.0) * cosW + beta * sinW;
                            a1 = -2.0 * ((aLocal - 1.0) + (aLocal + 1.0) * cosW);
                            a2 = (aLocal + 1.0) + (aLocal - 1.0) * cosW - beta * sinW;
                            break;
                        }
                        case eqdsp::FilterType::highShelf:
                        {
                            const double beta = std::sqrt(aLocal) / qLocal;
                            b0 = aLocal * ((aLocal + 1.0) + (aLocal - 1.0) * cosW + beta * sinW);
                            b1 = -2.0 * aLocal * ((aLocal - 1.0) + (aLocal + 1.0) * cosW);
                            b2 = aLocal * ((aLocal + 1.0) + (aLocal - 1.0) * cosW - beta * sinW);
                            a0 = (aLocal + 1.0) - (aLocal - 1.0) * cosW + beta * sinW;
                            a1 = 2.0 * ((aLocal - 1.0) - (aLocal + 1.0) * cosW);
                            a2 = (aLocal + 1.0) - (aLocal - 1.0) * cosW - beta * sinW;
                            break;
                        }
                        case eqdsp::FilterType::lowPass:
                            b0 = (1.0 - cosW) * 0.5;
                            b1 = 1.0 - cosW;
                            b2 = (1.0 - cosW) * 0.5;
                            a0 = 1.0 + alphaLocal;
                            a1 = -2.0 * cosW;
                            a2 = 1.0 - alphaLocal;
                            break;
                        case eqdsp::FilterType::highPass:
                            b0 = (1.0 + cosW) * 0.5;
                            b1 = -(1.0 + cosW);
                            b2 = (1.0 + cosW) * 0.5;
                            a0 = 1.0 + alphaLocal;
                            a1 = -2.0 * cosW;
                            a2 = 1.0 - alphaLocal;
                            break;
                        case eqdsp::FilterType::notch:
                            b0 = 1.0;
                            b1 = -2.0 * cosW;
                            b2 = 1.0;
                            a0 = 1.0 + alphaLocal;
                            a1 = -2.0 * cosW;
                            a2 = 1.0 - alphaLocal;
                            break;
                        case eqdsp::FilterType::bandPass:
                            b0 = alphaLocal;
                            b1 = 0.0;
                            b2 = -alphaLocal;
                            a0 = 1.0 + alphaLocal;
                            a1 = -2.0 * cosW;
                            a2 = 1.0 - alphaLocal;
                            break;
                        case eqdsp::FilterType::allPass:
                            b0 = 1.0 - alphaLocal;
                            b1 = -2.0 * cosW;
                            b2 = 1.0 + alphaLocal;
                            a0 = 1.0 + alphaLocal;
                            a1 = -2.0 * cosW;
                            a2 = 1.0 - alphaLocal;
                            break;
                        case eqdsp::FilterType::tilt:
                        case eqdsp::FilterType::flatTilt:
                            break;
                    }

                    const double invA0 = 1.0 / a0;
                    b0 *= invA0;
                    b1 *= invA0;
                    b2 *= invA0;
                    a1 *= invA0;
                    a2 *= invA0;

                    const double w = 2.0 * juce::MathConstants<double>::pi
                        * juce::jlimit(10.0, nyquist * 0.99, freq) / sampleRate;
                    const std::complex<double> z = std::exp(std::complex<double>(0.0, -w));
                    const std::complex<double> z2 = z * z;
                    const std::complex<double> numerator = b0 + b1 * z + b2 * z2;
                    const std::complex<double> denominator = 1.0 + a1 * z + a2 * z2;
                    return std::abs(numerator / denominator);
                };

                double mag = 1.0;
                const auto filterType = static_cast<eqdsp::FilterType>(type);
                if (filterType == eqdsp::FilterType::tilt || filterType == eqdsp::FilterType::flatTilt)
                {
                    const double qOverride = (filterType == eqdsp::FilterType::flatTilt) ? 0.5 : -1.0;
                    mag = computeMagForType(eqdsp::FilterType::lowShelf, gainDb * 0.5, qOverride)
                        * computeMagForType(eqdsp::FilterType::highShelf, -gainDb * 0.5, qOverride);
                }
                else
                {
                    mag = computeMagForType(filterType, gainDb, -1.0);
                }

                if (filterType == eqdsp::FilterType::allPass)
                    continue;
                if (filterType == eqdsp::FilterType::lowPass || filterType == eqdsp::FilterType::highPass)
                {
                    auto onePoleMag = [&](double cutoff, double freqHz)
                    {
                        const double clamped = juce::jlimit(10.0, nyquist * 0.99, cutoff);
                        const double a1p = std::exp(-2.0 * juce::MathConstants<double>::pi * clamped / sampleRate);
                        const std::complex<double> z1p = std::exp(std::complex<double>(0.0,
                                                                                      -2.0 * juce::MathConstants<double>::pi
                                                                                          * freqHz / sampleRate));
                        if (filterType == eqdsp::FilterType::lowPass)
                            return std::abs((1.0 - a1p) / (1.0 - a1p * z1p));

                        return std::abs(((1.0 + a1p) * 0.5) * (1.0 - z1p) / (1.0 - a1p * z1p));
                    };

                    const float clamped = juce::jlimit(6.0f, 96.0f, slopeDb);
                    const int stages = static_cast<int>(std::floor(clamped / 12.0f));
                    const float remainder = clamped - static_cast<float>(stages) * 12.0f;
                    const bool useOnePole = (remainder >= 6.0f) || stages == 0;
                    if (stages > 0)
                        mag = std::pow(mag, stages);
                    if (useOnePole)
                        mag *= onePoleMag(freqParam, freq);
                }

                totalMag *= mag;
            }

            const int index = bin * 2;
            firData[static_cast<size_t>(index)] = static_cast<float>(totalMag);
            firData[static_cast<size_t>(index + 1)] = 0.0f;

            if (bin > 0 && bin < fftSize / 2)
            {
                const int mirror = (fftSize - bin) * 2;
                firData[static_cast<size_t>(mirror)] = static_cast<float>(totalMag);
                firData[static_cast<size_t>(mirror + 1)] = 0.0f;
            }
        }

        firFft->performRealOnlyInverseTransform(firData.data());

        const int offset = (fftSize - taps) / 2;
        for (int i = 0; i < taps; ++i)
            firImpulse[static_cast<size_t>(i)] = firData[static_cast<size_t>(offset + i)] / fftSize;

        firWindow->multiplyWithWindowingTable(firImpulse.data(), taps);

        juce::AudioBuffer<float> buffer(1, taps);
        buffer.copyFrom(0, 0, firImpulse.data(), taps);
        return buffer;
    };

    auto includeForChannel = [this, &bandChannelMasks](int channel)
    {
        return [this, channel, &bandChannelMasks](int band)
        {
            const int target = lastMsTargets[band];
            if ((bandChannelMasks[band] & (1u << static_cast<uint32_t>(channel))) == 0)
                return false;
            if (channel < 2 && (target == 1 || target == 2 || target == 6))
                return false;
            return true;
        };
    };
    for (int ch = 0; ch < channels; ++ch)
    {
        auto impulse = buildImpulse(ch, includeForChannel(ch));
        linearPhaseEq.loadImpulse(ch, std::move(impulse), sampleRate);
    }

    const bool useMs = channels >= 2
        && std::any_of(lastMsTargets.begin(), lastMsTargets.end(), [](int v) { return v != 0; });

    if (useMs)
    {
        auto includeMid = [this](int band)
        {
            return lastMsTargets[band] == 0 || lastMsTargets[band] == 1 || lastMsTargets[band] == 6;
        };
        auto includeSide = [this](int band)
        {
            return lastMsTargets[band] == 0 || lastMsTargets[band] == 2;
        };

        auto midImpulse = buildImpulse(0, includeMid);
        auto sideImpulse = buildImpulse(0, includeSide);
        linearPhaseMsEq.loadImpulse(0, std::move(midImpulse), sampleRate);
        linearPhaseMsEq.loadImpulse(1, std::move(sideImpulse), sampleRate);
    }

    const int latency = (taps - 1) / 2;
    linearPhaseEq.setLatencySamples(latency);
    setLatencySamples(latency);
}

void EQProAudioProcessor::updateOversampling()
{
    oversamplingIndex = 0;
    oversampler.reset();
    oversampledBuffer.setSize(0, 0);
    return;

    const int factor = oversamplingIndex;
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        getTotalNumInputChannels(),
        factor,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true);
    oversampler->initProcessing(static_cast<size_t>(lastMaxBlockSize));

    const int channels = juce::jmin(getTotalNumInputChannels(), ParamIDs::kMaxChannels);
    const int upFactor = 1 << factor;
    oversampledBuffer.setSize(channels, lastMaxBlockSize * upFactor);
    oversampledBuffer.clear();

    eqDspOversampled.prepare(lastSampleRate * upFactor, lastMaxBlockSize * upFactor, channels);
    eqDspOversampled.reset();
}
