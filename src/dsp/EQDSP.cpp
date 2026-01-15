#include "EQDSP.h"
#include <algorithm>
#include <cmath>

namespace
{
struct SlopeConfig
{
    int stages = 1;
    bool useOnePole = false;
};

SlopeConfig slopeFromDb(float slopeDb)
{
    const float clamped = juce::jlimit(6.0f, 96.0f, slopeDb);
    const int stages = static_cast<int>(std::floor(clamped / 12.0f));
    const float remainder = clamped - static_cast<float>(stages) * 12.0f;
    const bool useOnePole = (remainder >= 6.0f) || stages == 0;
    return { juce::jmax(0, stages), useOnePole };
}

eqdsp::BandParams makeTiltParams(const eqdsp::BandParams& params, bool highShelf, float qOverride = -1.0f)
{
    auto tiltParams = params;
    tiltParams.type = highShelf ? eqdsp::FilterType::highShelf : eqdsp::FilterType::lowShelf;
    tiltParams.gainDb = params.gainDb * (highShelf ? -0.5f : 0.5f);
    if (qOverride > 0.0f)
        tiltParams.q = qOverride;
    return tiltParams;
}
} // namespace

namespace eqdsp
{
void EQDSP::prepare(double sampleRate, int maxBlockSize, int channels)
{
    sampleRateHz = sampleRate;
    numChannels = juce::jlimit(0, ParamIDs::kMaxChannels, channels);

    msBuffer.setSize(2, maxBlockSize);
    msBuffer.clear();
    scratchBuffer.setSize(numChannels, maxBlockSize);
    scratchBuffer.clear();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            for (int stage = 0; stage < kMaxStages; ++stage)
                filters[ch][band][stage].prepare(sampleRateHz);
            onePoles[ch][band].prepare(sampleRateHz);
            cachedParams[ch][band] = {};
            cachedParams[ch][band].frequencyHz = 1000.0f;
            cachedParams[ch][band].gainDb = 0.0f;
            cachedParams[ch][band].q = 0.707f;
            cachedParams[ch][band].type = FilterType::bell;
            cachedParams[ch][band].slopeDb = 12.0f;
            cachedParams[ch][band].bypassed = false;
            smoothFreq[ch][band].reset(sampleRateHz, 0.02);
            smoothGain[ch][band].reset(sampleRateHz, 0.02);
            smoothQ[ch][band].reset(sampleRateHz, 0.02);
            smoothFreq[ch][band].setCurrentAndTargetValue(cachedParams[ch][band].frequencyHz);
            smoothGain[ch][band].setCurrentAndTargetValue(cachedParams[ch][band].gainDb);
            smoothQ[ch][band].setCurrentAndTargetValue(cachedParams[ch][band].q);
            soloFilters[ch][band].prepare(sampleRateHz);
            detectorFilters[ch][band].prepare(sampleRateHz);
            detectorEnv[ch][band] = 0.0f;
            dynGainDb[ch][band].reset(sampleRateHz, 0.02);
            dynGainDb[ch][band].setCurrentAndTargetValue(0.0f);
            lastDynGainDb[ch][band] = 0.0f;
        }
    }

    for (int channel = 0; channel < 2; ++channel)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            for (int stage = 0; stage < kMaxStages; ++stage)
                msFilters[channel][band][stage].prepare(sampleRateHz);
            msOnePoles[channel][band].prepare(sampleRateHz);
        }
    }
}

void EQDSP::reset()
{
    for (int ch = 0; ch < numChannels; ++ch)
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            for (int stage = 0; stage < kMaxStages; ++stage)
                filters[ch][band][stage].reset();
            onePoles[ch][band].reset();
            soloFilters[ch][band].reset();
            detectorFilters[ch][band].reset();
            detectorEnv[ch][band] = 0.0f;
            lastDynGainDb[ch][band] = 0.0f;
        }
}

void EQDSP::setGlobalBypass(bool shouldBypass)
{
    globalBypass = shouldBypass;
}

void EQDSP::setSmartSoloEnabled(bool enabled)
{
    smartSoloEnabled = enabled;
}

void EQDSP::updateBandParams(int channelIndex, int bandIndex, const BandParams& params)
{
    if (channelIndex < 0 || channelIndex >= numChannels)
        return;
    if (bandIndex < 0 || bandIndex >= ParamIDs::kBandsPerChannel)
        return;

    cachedParams[channelIndex][bandIndex] = params;
    smoothFreq[channelIndex][bandIndex].setTargetValue(params.frequencyHz);
    smoothGain[channelIndex][bandIndex].setTargetValue(params.gainDb);
    smoothQ[channelIndex][bandIndex].setTargetValue(params.q);
}

void EQDSP::updateMsBandParams(int bandIndex, const BandParams& params)
{
    if (bandIndex < 0 || bandIndex >= ParamIDs::kBandsPerChannel)
        return;
    juce::ignoreUnused(params);
}

void EQDSP::setMsTargets(const std::array<int, ParamIDs::kBandsPerChannel>& targets)
{
    msTargets = targets;
}

void EQDSP::process(juce::AudioBuffer<float>& buffer,
                    const juce::AudioBuffer<float>* detectorBuffer)
{
    if (globalBypass)
        return;

    const int samples = buffer.getNumSamples();
    const bool hasDetectorBuffer = detectorBuffer != nullptr
        && detectorBuffer->getNumSamples() >= samples
        && detectorBuffer->getNumChannels() > 0;

    auto getDetectorPointer = [detectorBuffer, hasDetectorBuffer](int channel) -> const float*
    {
        if (! hasDetectorBuffer || detectorBuffer == nullptr)
            return nullptr;
        const int index = juce::jlimit(0, detectorBuffer->getNumChannels() - 1, channel);
        return detectorBuffer->getReadPointer(index);
    };
    bool anySolo = false;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            if (cachedParams[ch][band].solo)
            {
                anySolo = true;
                break;
            }
        }
        if (anySolo)
            break;
    }

    if (anySolo)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            scratchBuffer.copyFrom(ch, 0, buffer, ch, 0, samples);

        buffer.clear();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* out = buffer.getWritePointer(ch);
            const auto* in = scratchBuffer.getReadPointer(ch);
            for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
            {
                if (! cachedParams[ch][band].solo)
                    continue;

                auto params = cachedParams[ch][band];
                smoothFreq[ch][band].skip(samples);
                smoothGain[ch][band].skip(samples);
                smoothQ[ch][band].skip(samples);
                params.frequencyHz = smoothFreq[ch][band].getCurrentValue();
                params.gainDb = smoothGain[ch][band].getCurrentValue();
                params.q = smoothQ[ch][band].getCurrentValue();
                params.type = FilterType::bandPass;
                params.gainDb = smartSoloEnabled ? 6.0f : 0.0f;
                if (smartSoloEnabled)
                    params.q = juce::jlimit(0.2f, 18.0f, params.q * 2.5f);
                params.bypassed = false;
                soloFilters[ch][band].update(params);

                for (int i = 0; i < samples; ++i)
                    out[i] += soloFilters[ch][band].processSample(in[i]);
            }
        }

        return;
    }

    bool anyActive = false;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            if (! cachedParams[ch][band].bypassed)
            {
                anyActive = true;
                break;
            }
        }
        if (anyActive)
            break;
    }
    if (! anyActive)
        return;
    const bool useMs = numChannels >= 2
        && std::any_of(msTargets.begin(), msTargets.end(), [](int v) { return v == 1 || v == 2; });

    if (useMs)
    {
        auto* mid = msBuffer.getWritePointer(0);
        auto* side = msBuffer.getWritePointer(1);
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(1);

        for (int i = 0; i < samples; ++i)
        {
            mid[i] = 0.5f * (left[i] + right[i]);
            side[i] = 0.5f * (left[i] - right[i]);
        }

        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            if (cachedParams[0][band].bypassed)
                continue;

            const int target = msTargets[band];
            if (target == 3 || target == 4)
                continue;
            auto params = cachedParams[0][band];
            const bool isHpLp = params.type == FilterType::lowPass || params.type == FilterType::highPass;
            const bool isTilt = params.type == FilterType::tilt || params.type == FilterType::flatTilt;
            const float tiltQ = (params.type == FilterType::flatTilt) ? 0.5f : -1.0f;
            const auto slopeConfig = slopeFromDb(params.slopeDb);
            smoothFreq[0][band].skip(samples);
            smoothGain[0][band].skip(samples);
            smoothQ[0][band].skip(samples);
            params.frequencyHz = smoothFreq[0][band].getCurrentValue();
            params.gainDb = smoothGain[0][band].getCurrentValue();
            params.q = smoothQ[0][band].getCurrentValue();

            if (params.dynamicEnabled
                && params.type != FilterType::lowPass
                && params.type != FilterType::highPass
                && params.type != FilterType::allPass)
            {
                float env = detectorEnv[0][band];
                const float attackCoeff = std::exp(-1.0f / (0.001f * params.attackMs * sampleRateHz));
                const float releaseCoeff = std::exp(-1.0f / (0.001f * params.releaseMs * sampleRateHz));
                const float* detectorSource = (target == 2) ? side : mid;
                if (params.dynamicSource == 1)
                {
                    const float* external = getDetectorPointer(target == 2 ? 1 : 0);
                    if (external != nullptr)
                        detectorSource = external;
                }
                if (params.dynamicFilter)
                {
                    BandParams detectorParams = params;
                    detectorParams.type = FilterType::bandPass;
                    detectorFilters[0][band].update(detectorParams);
                }

                for (int i = 0; i < samples; ++i)
                {
                    float level = std::abs(detectorSource[i]);
                    if (params.dynamicFilter)
                        level = std::abs(detectorFilters[0][band].processSample(detectorSource[i]));
                    if (level > env)
                        env = attackCoeff * env + (1.0f - attackCoeff) * level;
                    else
                        env = releaseCoeff * env + (1.0f - releaseCoeff) * level;
                }
                detectorEnv[0][band] = env;

                const float envDb = juce::Decibels::gainToDecibels(env, -120.0f);
                const float over = envDb - params.thresholdDb;
                float delta = 0.0f;
                if (params.dynamicMode == 0 && over > 0.0f)
                    delta = -over;
                else if (params.dynamicMode == 1 && over < 0.0f)
                    delta = -over;

                dynGainDb[0][band].setTargetValue(delta);
                const float gainDelta = dynGainDb[0][band].getNextValue() * params.dynamicMix;
                lastDynGainDb[0][band] = gainDelta;
                params.gainDb += gainDelta;
            }
            else
            {
                lastDynGainDb[0][band] = 0.0f;
            }

            const int stages = isTilt ? 2 : (isHpLp ? slopeConfig.stages : 1);

            if (isTilt)
            {
                const auto lowParams = makeTiltParams(params, false, tiltQ);
                const auto highParams = makeTiltParams(params, true, tiltQ);
                msFilters[0][band][0].update(lowParams);
                msFilters[0][band][1].update(highParams);
                msFilters[1][band][0].update(lowParams);
                msFilters[1][band][1].update(highParams);
            }
            else
            {
                for (int stage = 0; stage < stages; ++stage)
                {
                    msFilters[0][band][stage].update(params);
                    msFilters[1][band][stage].update(params);
                }
            }

            if (target == 0)
            {
                if (isHpLp && slopeConfig.useOnePole)
                {
                    msOnePoles[0][band].setLowPass(params.frequencyHz);
                    msOnePoles[1][band].setLowPass(params.frequencyHz);
                    if (params.type == FilterType::highPass)
                    {
                        msOnePoles[0][band].setHighPass(params.frequencyHz);
                        msOnePoles[1][band].setHighPass(params.frequencyHz);
                    }
                }

                for (int i = 0; i < samples; ++i)
                {
                    float m = mid[i];
                    float s = side[i];

                    if (isHpLp && slopeConfig.useOnePole)
                    {
                        m = msOnePoles[0][band].processSample(m);
                        s = msOnePoles[1][band].processSample(s);
                    }

                    for (int stage = 0; stage < stages; ++stage)
                    {
                        m = msFilters[0][band][stage].processSample(m);
                        s = msFilters[1][band][stage].processSample(s);
                    }

                    mid[i] = m;
                    side[i] = s;
                }
            }
            else if (target == 1)
            {
                if (isHpLp && slopeConfig.useOnePole)
                {
                    if (params.type == FilterType::lowPass)
                        msOnePoles[0][band].setLowPass(params.frequencyHz);
                    else
                        msOnePoles[0][band].setHighPass(params.frequencyHz);
                }

                for (int i = 0; i < samples; ++i)
                {
                    float m = mid[i];
                    if (isHpLp && slopeConfig.useOnePole)
                        m = msOnePoles[0][band].processSample(m);

                    for (int stage = 0; stage < stages; ++stage)
                    {
                        m = msFilters[0][band][stage].processSample(m);
                    }

                    mid[i] = m;
                }
            }
            else if (target == 2)
            {
                if (isHpLp && slopeConfig.useOnePole)
                {
                    if (params.type == FilterType::lowPass)
                        msOnePoles[1][band].setLowPass(params.frequencyHz);
                    else
                        msOnePoles[1][band].setHighPass(params.frequencyHz);
                }

                for (int i = 0; i < samples; ++i)
                {
                    float s = side[i];
                    if (isHpLp && slopeConfig.useOnePole)
                        s = msOnePoles[1][band].processSample(s);

                    for (int stage = 0; stage < stages; ++stage)
                    {
                        s = msFilters[1][band][stage].processSample(s);
                    }

                    side[i] = s;
                }
            }
        }

        for (int i = 0; i < samples; ++i)
        {
            left[i] = mid[i] + side[i];
            right[i] = mid[i] - side[i];
        }
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* channelData = buffer.getWritePointer(ch);
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            const int target = (ch < 2) ? msTargets[band] : 0;
            if (useMs && ch < 2 && (target == 0 || target == 1 || target == 2))
                continue;
            if (ch == 0 && target == 4)
                continue;
            if (ch == 1 && target == 3)
                continue;

            if (cachedParams[ch][band].bypassed)
                continue;

            auto params = cachedParams[ch][band];
            const bool isHpLp = params.type == FilterType::lowPass || params.type == FilterType::highPass;
            const bool isTilt = params.type == FilterType::tilt || params.type == FilterType::flatTilt;
            const float tiltQ = (params.type == FilterType::flatTilt) ? 0.5f : -1.0f;
            const auto slopeConfig = slopeFromDb(params.slopeDb);
            smoothFreq[ch][band].skip(samples);
            smoothGain[ch][band].skip(samples);
            smoothQ[ch][band].skip(samples);
            params.frequencyHz = smoothFreq[ch][band].getCurrentValue();
            params.gainDb = smoothGain[ch][band].getCurrentValue();
            params.q = smoothQ[ch][band].getCurrentValue();

            if (params.dynamicEnabled
                && params.type != FilterType::lowPass
                && params.type != FilterType::highPass
                && params.type != FilterType::allPass)
            {
                float env = detectorEnv[ch][band];
                const float attackCoeff = std::exp(-1.0f / (0.001f * params.attackMs * sampleRateHz));
                const float releaseCoeff = std::exp(-1.0f / (0.001f * params.releaseMs * sampleRateHz));
                const float* detectorSource = channelData;
                if (params.dynamicSource == 1)
                {
                    const float* external = getDetectorPointer(ch);
                    if (external != nullptr)
                        detectorSource = external;
                }
                if (params.dynamicFilter)
                {
                    BandParams detectorParams = params;
                    detectorParams.type = FilterType::bandPass;
                    detectorFilters[ch][band].update(detectorParams);
                }

                for (int i = 0; i < samples; ++i)
                {
                    float level = std::abs(detectorSource[i]);
                    if (params.dynamicFilter)
                        level = std::abs(detectorFilters[ch][band].processSample(detectorSource[i]));
                    if (level > env)
                        env = attackCoeff * env + (1.0f - attackCoeff) * level;
                    else
                        env = releaseCoeff * env + (1.0f - releaseCoeff) * level;
                }
                detectorEnv[ch][band] = env;
                const float envDb = juce::Decibels::gainToDecibels(env, -120.0f);
                const float over = envDb - params.thresholdDb;
                float delta = 0.0f;
                if (params.dynamicMode == 0 && over > 0.0f)
                    delta = -over;
                else if (params.dynamicMode == 1 && over < 0.0f)
                    delta = -over;

                dynGainDb[ch][band].setTargetValue(delta);
                const float gainDelta = dynGainDb[ch][band].getNextValue() * params.dynamicMix;
                lastDynGainDb[ch][band] = gainDelta;
                params.gainDb += gainDelta;
            }
            else
            {
                lastDynGainDb[ch][band] = 0.0f;
            }

            const int stages = isTilt ? 2 : (isHpLp ? slopeConfig.stages : 1);
            if (isTilt)
            {
                const auto lowParams = makeTiltParams(params, false, tiltQ);
                const auto highParams = makeTiltParams(params, true, tiltQ);
                filters[ch][band][0].update(lowParams);
                filters[ch][band][1].update(highParams);
            }
            else
            {
                for (int stage = 0; stage < stages; ++stage)
                    filters[ch][band][stage].update(params);
            }

            if (isHpLp && slopeConfig.useOnePole)
            {
                if (params.type == FilterType::lowPass)
                    onePoles[ch][band].setLowPass(params.frequencyHz);
                else
                    onePoles[ch][band].setHighPass(params.frequencyHz);
            }

            for (int i = 0; i < samples; ++i)
            {
                float sample = channelData[i];

                if (isHpLp && slopeConfig.useOnePole)
                    sample = onePoles[ch][band].processSample(sample);

                for (int stage = 0; stage < stages; ++stage)
                    sample = filters[ch][band][stage].processSample(sample);

                channelData[i] = sample;
            }
        }
    }
}

float EQDSP::getDynamicGainDb(int channelIndex, int bandIndex) const
{
    if (channelIndex < 0 || channelIndex >= numChannels)
        return 0.0f;
    if (bandIndex < 0 || bandIndex >= ParamIDs::kBandsPerChannel)
        return 0.0f;
    return lastDynGainDb[channelIndex][bandIndex];
}
} // namespace eqdsp
