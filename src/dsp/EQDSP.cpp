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
    constexpr int kMaxSlopeStages = 8;
    const float clamped = juce::jlimit(6.0f, 96.0f, slopeDb);
    const int stages = juce::jmin(kMaxSlopeStages, static_cast<int>(std::floor(clamped / 12.0f)));
    const float remainder = clamped - static_cast<float>(stages) * 12.0f;
    const bool useOnePole = (remainder >= 6.0f) || stages == 0;
    return { juce::jmax(0, stages), useOnePole };
}

float detectorWeightFromFreq(float freqHz)
{
    const float clamped = juce::jlimit(20.0f, 20000.0f, freqHz);
    const float norm = std::log2(clamped / 1000.0f);
    return juce::jlimit(0.6f, 1.4f, 1.0f + 0.2f * norm);
}

float computeDynamicGain(const eqdsp::BandParams& params, float detectorDb)
{
    if (! params.dynamicEnabled)
        return params.gainDb;

    const float overDb = detectorDb - params.thresholdDb;
    const float amount = juce::jlimit(0.0f, 1.0f, overDb / 12.0f);
    if (params.dynamicMode == 0)
    {
        if (params.gainDb >= 0.0f)
            return params.gainDb * amount;
        return params.gainDb * (1.0f - amount);
    }
    if (params.gainDb >= 0.0f)
        return params.gainDb * (1.0f - amount);
    return params.gainDb * amount;
}

void processBiquadStereo(eqdsp::Biquad& left,
                         eqdsp::Biquad& right,
                         float* leftData,
                         float* rightData,
                         int numSamples)
{
    if (leftData == nullptr || rightData == nullptr || numSamples <= 0)
        return;

    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    left.getCoefficients(b0, b1, b2, a1, a2);
    float z1L = 0.0f, z2L = 0.0f, z1R = 0.0f, z2R = 0.0f;
    left.getState(z1L, z2L);
    right.getState(z1R, z2R);

    alignas (sizeof (juce::dsp::SIMDRegister<float>)) float z1Arr[juce::dsp::SIMDRegister<float>::SIMDNumElements] {};
    alignas (sizeof (juce::dsp::SIMDRegister<float>)) float z2Arr[juce::dsp::SIMDRegister<float>::SIMDNumElements] {};
    z1Arr[0] = z1L; z1Arr[1] = z1R;
    z2Arr[0] = z2L; z2Arr[1] = z2R;
    auto z1 = juce::dsp::SIMDRegister<float>::fromRawArray(z1Arr);
    auto z2 = juce::dsp::SIMDRegister<float>::fromRawArray(z2Arr);
    const auto vb0 = juce::dsp::SIMDRegister<float>::expand(b0);
    const auto vb1 = juce::dsp::SIMDRegister<float>::expand(b1);
    const auto vb2 = juce::dsp::SIMDRegister<float>::expand(b2);
    const auto va1 = juce::dsp::SIMDRegister<float>::expand(a1);
    const auto va2 = juce::dsp::SIMDRegister<float>::expand(a2);

    for (int i = 0; i < numSamples; ++i)
    {
        alignas (sizeof (juce::dsp::SIMDRegister<float>)) float xArr[juce::dsp::SIMDRegister<float>::SIMDNumElements] {};
        xArr[0] = leftData[i];
        xArr[1] = rightData[i];
        const auto x = juce::dsp::SIMDRegister<float>::fromRawArray(xArr);
        const auto y = vb0 * x + z1;
        z1 = vb1 * x - va1 * y + z2;
        z2 = vb2 * x - va2 * y;
        leftData[i] = y.get(0);
        rightData[i] = y.get(1);
    }

    left.setState(z1.get(0), z2.get(0));
    right.setState(z1.get(1), z2.get(1));
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
    this->maxBlockSize = maxBlockSize;
    const uint32_t maskAll = (numChannels >= 32)
        ? 0xFFFFFFFFu
        : (numChannels > 0 ? ((1u << static_cast<uint32_t>(numChannels)) - 1u) : 0u);
    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        bandChannelMasks[band] = maskAll;

    msBuffer.setSize(2, maxBlockSize);
    msBuffer.clear();
    msDryBuffer.setSize(2, maxBlockSize);
    msDryBuffer.clear();
    detectorMsBuffer.setSize(2, maxBlockSize);
    detectorMsBuffer.clear();
    detectorTemp.setSize(1, maxBlockSize);
    detectorTemp.clear();
    scratchBuffer.setSize(numChannels, maxBlockSize);
    scratchBuffer.clear();
    

    for (int ch = 0; ch < numChannels; ++ch)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            for (int stage = 0; stage < kMaxStages; ++stage)
                filters[ch][band][stage].prepare(sampleRateHz);
            onePoles[ch][band].prepare(sampleRateHz);
            detectorFilters[ch][band].prepare(sampleRateHz);
            cachedParams[ch][band] = {};
            cachedParams[ch][band].frequencyHz = 1000.0f;
            cachedParams[ch][band].gainDb = 0.0f;
            cachedParams[ch][band].q = 0.707f;
            cachedParams[ch][band].type = FilterType::bell;
            cachedParams[ch][band].slopeDb = 12.0f;
            cachedParams[ch][band].bypassed = false;
            cachedParams[ch][band].mix = 1.0f;
            detectorEnv[ch][band] = 0.0f;
            detectorEnvRms[ch][band] = 0.0f;
            detectorDb[ch][band].store(-60.0f);
            dynamicGainDb[ch][band].store(0.0f);
            smoothFreq[ch][band].reset(sampleRateHz, 0.02);
            smoothGain[ch][band].reset(sampleRateHz, 0.02);
            smoothQ[ch][band].reset(sampleRateHz, 0.02);
            smoothMix[ch][band].reset(sampleRateHz, 0.02);
            smoothDynThresh[ch][band].reset(sampleRateHz, 0.02);
            smoothFreq[ch][band].setCurrentAndTargetValue(cachedParams[ch][band].frequencyHz);
            smoothGain[ch][band].setCurrentAndTargetValue(cachedParams[ch][band].gainDb);
            smoothQ[ch][band].setCurrentAndTargetValue(cachedParams[ch][band].q);
            smoothMix[ch][band].setCurrentAndTargetValue(cachedParams[ch][band].mix);
            smoothDynThresh[ch][band].setCurrentAndTargetValue(cachedParams[ch][band].thresholdDb);
            soloFilters[ch][band].prepare(sampleRateHz);
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
            detectorEnvRms[ch][band] = 0.0f;
            detectorDb[ch][band].store(-60.0f);
            dynamicGainDb[ch][band].store(0.0f);
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

float EQDSP::applyQMode(const BandParams& params) const
{
    if (qMode != 1)
        return params.q;

    const bool supports = params.type == FilterType::bell;

    if (! supports)
        return params.q;

    const float amountNorm = juce::jlimit(0.0f, 1.0f, qModeAmount / 100.0f);
    const float factor = 1.0f + (std::abs(params.gainDb) / 18.0f) * amountNorm;
    return juce::jlimit(0.1f, 18.0f, params.q * factor);
}

void EQDSP::setQMode(int mode)
{
    qMode = mode;
}

void EQDSP::setQModeAmount(float amount)
{
    qModeAmount = amount;
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
    smoothMix[channelIndex][bandIndex].setTargetValue(params.mix);
    smoothDynThresh[channelIndex][bandIndex].setTargetValue(params.thresholdDb);
    
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

void EQDSP::setBandChannelMasks(const std::array<uint32_t, ParamIDs::kBandsPerChannel>& masks)
{
    bandChannelMasks = masks;
}

float EQDSP::getDetectorDb(int channelIndex, int bandIndex) const
{
    if (channelIndex < 0 || channelIndex >= numChannels)
        return -60.0f;
    if (bandIndex < 0 || bandIndex >= ParamIDs::kBandsPerChannel)
        return -60.0f;
    return detectorDb[channelIndex][bandIndex].load();
}

float EQDSP::getDynamicGainDb(int channelIndex, int bandIndex) const
{
    if (channelIndex < 0 || channelIndex >= numChannels)
        return 0.0f;
    if (bandIndex < 0 || bandIndex >= ParamIDs::kBandsPerChannel)
        return 0.0f;
    return dynamicGainDb[channelIndex][bandIndex].load();
}

void EQDSP::process(juce::AudioBuffer<float>& buffer,
                    const juce::AudioBuffer<float>* detectorBuffer,
                    juce::AudioBuffer<float>* harmonicOnlyBuffer)
{
    if (globalBypass)
    {
        if (harmonicOnlyBuffer != nullptr)
        {
            harmonicOnlyBuffer->setSize(buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
            harmonicOnlyBuffer->clear();
        }
        return;
    }

    const int samples = buffer.getNumSamples();
    if (harmonicOnlyBuffer != nullptr)
    {
        harmonicOnlyBuffer->setSize(buffer.getNumChannels(), samples, false, false, true);
        harmonicOnlyBuffer->clear();
    }
    const bool externalAvailable = detectorBuffer != nullptr
        && detectorBuffer->getNumSamples() == samples
        && detectorBuffer->getNumChannels() > 0;
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
                const int paramSource = ch;
                if (! cachedParams[paramSource][band].solo)
                    continue;
                if ((bandChannelMasks[band] & (1u << static_cast<uint32_t>(ch))) == 0)
                    continue;

                auto params = cachedParams[paramSource][band];
                if (paramSource == ch)
                {
                    smoothFreq[ch][band].skip(samples);
                    smoothGain[ch][band].skip(samples);
                    smoothQ[ch][band].skip(samples);
                    smoothMix[ch][band].skip(samples);
                    smoothDynThresh[ch][band].skip(samples);
                    params.frequencyHz = smoothFreq[ch][band].getCurrentValue();
                    params.gainDb = smoothGain[ch][band].getCurrentValue();
                    params.q = smoothQ[ch][band].getCurrentValue();
                    params.mix = smoothMix[ch][band].getCurrentValue();
                    params.thresholdDb = smoothDynThresh[ch][band].getCurrentValue();
                }
                else
                {
                    params.frequencyHz = smoothFreq[paramSource][band].getCurrentValue();
                    params.gainDb = smoothGain[paramSource][band].getCurrentValue();
                    params.q = smoothQ[paramSource][band].getCurrentValue();
                    params.mix = smoothMix[paramSource][band].getCurrentValue();
                    params.thresholdDb = smoothDynThresh[paramSource][band].getCurrentValue();
                }
                params.q = applyQMode(params);
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
    bool linkStereoDetectors = false;
    if (! useMs && numChannels == 2)
    {
        const auto* left = buffer.getReadPointer(0);
        const auto* right = buffer.getReadPointer(1);
        const int checkCount = juce::jmin(samples, 32);
        float acc = 0.0f;
        for (int i = 0; i < checkCount; ++i)
            acc += std::abs(left[i] - right[i]);
        linkStereoDetectors = acc < 1.0e-5f * static_cast<float>(checkCount);
    }

    auto updateDetector = [this](int channel, int band, float detSample,
                                 float attackCoeff, float releaseCoeff,
                                 float freqHz) -> float
    {
        const float weighted = detSample * detectorWeightFromFreq(freqHz);
        float& peakEnv = detectorEnv[channel][band];
        float& rmsEnv = detectorEnvRms[channel][band];
        const float absVal = std::abs(weighted);
        const float coeff = absVal > peakEnv ? attackCoeff : releaseCoeff;
        peakEnv = coeff * peakEnv + (1.0f - coeff) * absVal;
        const float sq = weighted * weighted;
        const float coeffRms = sq > rmsEnv ? attackCoeff : releaseCoeff;
        rmsEnv = coeffRms * rmsEnv + (1.0f - coeffRms) * sq;
        const float rms = std::sqrt(rmsEnv);
        // Blend peak and RMS for smoother, more musical dynamics.
        constexpr float kPeakBlend = 0.6f;
        const float detector = (kPeakBlend * peakEnv) + ((1.0f - kPeakBlend) * rms);
        const float detDb = juce::Decibels::gainToDecibels(detector, -60.0f);
        detectorDb[channel][band].store(detDb);
        return detDb;
    };

    if (useMs)
    {
        // Build M/S processing groups for each stereo pair selection in the layout.
        struct MsPairGroup
        {
            int left = -1;
            int right = -1;
            bool useExternal = false;
            std::vector<int> bands;
        };

        std::vector<MsPairGroup> pairGroups;
        pairGroups.reserve(4);

        auto findPairFromMask = [&](uint32_t mask) -> std::pair<int, int>
        {
            int first = -1;
            int second = -1;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                if ((mask & (1u << static_cast<uint32_t>(ch))) == 0)
                    continue;
                if (first < 0)
                    first = ch;
                else
                {
                    second = ch;
                    break;
                }
            }
            if (first >= 0 && second >= 0 && first > second)
                std::swap(first, second);
            return { first, second };
        };

        auto addPairGroup = [&](int left, int right, int band, bool useExternal)
        {
            for (auto& group : pairGroups)
            {
                if (group.left == left && group.right == right)
                {
                    group.bands.push_back(band);
                    group.useExternal = group.useExternal || useExternal;
                    return;
                }
            }
            MsPairGroup group;
            group.left = left;
            group.right = right;
            group.useExternal = useExternal;
            group.bands.push_back(band);
            pairGroups.push_back(std::move(group));
        };

        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            const int target = msTargets[band];
            if (target != 1 && target != 2)
                continue;
            const auto pair = findPairFromMask(bandChannelMasks[band]);
            if (pair.first < 0 || pair.second < 0)
                continue;
            const bool bandUseExternal = externalAvailable && cachedParams[pair.first][band].useExternalDetector;
            addPairGroup(pair.first, pair.second, band, bandUseExternal);
        }

        for (const auto& group : pairGroups)
        {
            // Process each selected stereo pair independently in M/S.
            auto* mid = msBuffer.getWritePointer(0);
            auto* side = msBuffer.getWritePointer(1);
            auto* left = buffer.getWritePointer(group.left);
            auto* right = buffer.getWritePointer(group.right);

            juce::FloatVectorOperations::copy(mid, left, samples);
            juce::FloatVectorOperations::add(mid, right, samples);
            juce::FloatVectorOperations::multiply(mid, 0.5f, samples);
            juce::FloatVectorOperations::copy(side, left, samples);
            juce::FloatVectorOperations::subtract(side, right, samples);
            juce::FloatVectorOperations::multiply(side, 0.5f, samples);

            juce::FloatVectorOperations::copy(msDryBuffer.getWritePointer(0), mid, samples);
            juce::FloatVectorOperations::copy(msDryBuffer.getWritePointer(1), side, samples);

            if (group.useExternal)
            {
                auto* detMid = detectorMsBuffer.getWritePointer(0);
                auto* detSide = detectorMsBuffer.getWritePointer(1);
                const auto* detLeft = detectorBuffer->getReadPointer(group.left);
                const auto* detRight = detectorBuffer->getReadPointer(group.right);
                juce::FloatVectorOperations::copy(detMid, detLeft, samples);
                juce::FloatVectorOperations::add(detMid, detRight, samples);
                juce::FloatVectorOperations::multiply(detMid, 0.5f, samples);
                juce::FloatVectorOperations::copy(detSide, detLeft, samples);
                juce::FloatVectorOperations::subtract(detSide, detRight, samples);
                juce::FloatVectorOperations::multiply(detSide, 0.5f, samples);
            }

            for (int band : group.bands)
            {
                const int target = msTargets[band];
                if (target != 1 && target != 2)
                    continue;
                auto params = cachedParams[group.left][band];
                if (params.bypassed)
                    continue;

                const bool bandUseExternal = externalAvailable && params.useExternalDetector;
                const bool isHpLp = params.type == FilterType::lowPass || params.type == FilterType::highPass;
                const bool isTilt = params.type == FilterType::tilt || params.type == FilterType::flatTilt;
                const float tiltQ = (params.type == FilterType::flatTilt) ? 0.5f : -1.0f;
                const auto slopeConfig = slopeFromDb(params.slopeDb);
                smoothFreq[group.left][band].skip(samples);
                smoothGain[group.left][band].skip(samples);
                smoothQ[group.left][band].skip(samples);
                smoothMix[group.left][band].skip(samples);
                smoothDynThresh[group.left][band].skip(samples);
                params.frequencyHz = smoothFreq[group.left][band].getCurrentValue();
                params.gainDb = smoothGain[group.left][band].getCurrentValue();
                params.q = smoothQ[group.left][band].getCurrentValue();
                params.mix = smoothMix[group.left][band].getCurrentValue();
                params.thresholdDb = smoothDynThresh[group.left][band].getCurrentValue();
                params.q = applyQMode(params);
                params.dynamicEnabled = false;
                const float mix = juce::jlimit(0.0f, 1.0f, params.mix);
                const float staticGainDb = params.gainDb;

                const float scale = params.autoScale
                    ? juce::jlimit(0.25f, 4.0f, params.frequencyHz / 1000.0f)
                    : 1.0f;
                const float attackMs = juce::jmax(0.1f, params.attackMs * scale);
                const float releaseMs = juce::jmax(0.1f, params.releaseMs * scale);
                const float attackCoeff = std::exp(-1.0f / (attackMs * 0.001f * static_cast<float>(sampleRateHz)));
                const float releaseCoeff = std::exp(-1.0f / (releaseMs * 0.001f * static_cast<float>(sampleRateHz)));

                BandParams detectorParams = params;
                detectorParams.type = FilterType::bandPass;
                detectorParams.gainDb = 0.0f;
                detectorFilters[0][band].update(detectorParams);
                detectorFilters[1][band].update(detectorParams);

                const int stages = isTilt ? 2 : (isHpLp ? slopeConfig.stages : 1);
                const bool isSixDb = isHpLp && slopeConfig.stages == 0 && slopeConfig.useOnePole;
                const float resonanceMix = isSixDb
                    ? juce::jlimit(0.0f, 0.8f, (params.q - 0.707f) / 6.0f)
                    : 0.0f;

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
                if (resonanceMix > 0.0f)
                {
                    BandParams resParams = params;
                    resParams.type = FilterType::bandPass;
                    resParams.gainDb = 0.0f;
                    msFilters[0][band][0].update(resParams);
                    msFilters[1][band][0].update(resParams);
                }

                if (target == 1)
                {
                    dynamicGainDb[0][band].store(0.0f);
                    if (isHpLp && slopeConfig.useOnePole)
                    {
                        if (params.type == FilterType::lowPass)
                            msOnePoles[0][band].setLowPass(params.frequencyHz);
                        else
                            msOnePoles[0][band].setHighPass(params.frequencyHz);
                    }

                    const auto* dryMid = msDryBuffer.getReadPointer(0);
                    for (int i = 0; i < samples; ++i)
                    {
                        const float dryM = dryMid[i];
                        float m = dryM;
                        float resM = 0.0f;
                        if (resonanceMix > 0.0f)
                            resM = msFilters[0][band][0].processSample(dryM);
                        const float detInput = bandUseExternal
                            ? detectorMsBuffer.getReadPointer(0)[i]
                            : dryM;
                        const float detSample = detectorFilters[0][band].processSample(detInput);
                        const float detDb = updateDetector(0, band, detSample,
                                                           attackCoeff, releaseCoeff,
                                                           params.frequencyHz);
                        if (isHpLp && slopeConfig.useOnePole)
                            m = msOnePoles[0][band].processSample(m);

                        for (int stage = 0; stage < stages; ++stage)
                            m = msFilters[0][band][stage].processSample(m);
                        if (resonanceMix > 0.0f)
                            m += resM * resonanceMix;

                        if (params.dynamicEnabled)
                        {
                            const float dynamicGainDb = computeDynamicGain(params, detDb);
                            const float deltaDb = dynamicGainDb - staticGainDb;
                            const float deltaGain = juce::Decibels::decibelsToGain(deltaDb);
                            m = dryM + (m - dryM) * deltaGain;
                            this->dynamicGainDb[0][band].store(deltaDb);
                        }

                        const float mDelta = (m - dryM) * mix;
                        mid[i] += mDelta;
                    }
                }
                else if (target == 2)
                {
                    dynamicGainDb[1][band].store(0.0f);
                    if (isHpLp && slopeConfig.useOnePole)
                    {
                        if (params.type == FilterType::lowPass)
                            msOnePoles[1][band].setLowPass(params.frequencyHz);
                        else
                            msOnePoles[1][band].setHighPass(params.frequencyHz);
                    }

                    const auto* drySide = msDryBuffer.getReadPointer(1);
                    for (int i = 0; i < samples; ++i)
                    {
                        const float dryS = drySide[i];
                        float s = dryS;
                        float resS = 0.0f;
                        if (resonanceMix > 0.0f)
                            resS = msFilters[1][band][0].processSample(dryS);
                        const float detInput = bandUseExternal
                            ? detectorMsBuffer.getReadPointer(1)[i]
                            : dryS;
                        const float detSample = detectorFilters[1][band].processSample(detInput);
                        const float detDb = updateDetector(1, band, detSample,
                                                           attackCoeff, releaseCoeff,
                                                           params.frequencyHz);
                        if (isHpLp && slopeConfig.useOnePole)
                            s = msOnePoles[1][band].processSample(s);

                        for (int stage = 0; stage < stages; ++stage)
                            s = msFilters[1][band][stage].processSample(s);
                        if (resonanceMix > 0.0f)
                            s += resS * resonanceMix;

                        if (params.dynamicEnabled)
                        {
                            const float dynamicGainDb = computeDynamicGain(params, detDb);
                            const float deltaDb = dynamicGainDb - staticGainDb;
                            const float deltaGain = juce::Decibels::decibelsToGain(deltaDb);
                            s = dryS + (s - dryS) * deltaGain;
                            this->dynamicGainDb[1][band].store(deltaDb);
                        }

                        const float sDelta = (s - dryS) * mix;
                        side[i] += sDelta;
                    }
                }
            }

            juce::FloatVectorOperations::copy(left, mid, samples);
            juce::FloatVectorOperations::add(left, side, samples);
            juce::FloatVectorOperations::copy(right, mid, samples);
            juce::FloatVectorOperations::subtract(right, side, samples);
        }
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        scratchBuffer.copyFrom(ch, 0, buffer, ch, 0, samples);
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* channelData = buffer.getWritePointer(ch);
        const auto* dryData = scratchBuffer.getReadPointer(ch);
        float* harmonicOnlyData = harmonicOnlyBuffer != nullptr
            ? harmonicOnlyBuffer->getWritePointer(ch)
            : nullptr;
        juce::FloatVectorOperations::copy(channelData, dryData, samples);
        auto* rightData = (numChannels >= 2) ? buffer.getWritePointer(1) : nullptr;
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            const int target = msTargets[band];
            if ((target == 1 || target == 2)
                && (bandChannelMasks[band] & (1u << static_cast<uint32_t>(ch))) != 0)
            {
                continue;
            }
            if ((bandChannelMasks[band] & (1u << static_cast<uint32_t>(ch))) == 0)
                continue;
            const int paramSource = ch;
            if (cachedParams[paramSource][band].bypassed)
                continue;

            auto params = cachedParams[paramSource][band];
            const bool bandUseExternal = externalAvailable && params.useExternalDetector;
            if (params.mix <= 0.0001f)
                continue;
            const bool isHpLp = params.type == FilterType::lowPass || params.type == FilterType::highPass;
            const bool isTilt = params.type == FilterType::tilt || params.type == FilterType::flatTilt;
            const bool isShelf = params.type == FilterType::lowShelf || params.type == FilterType::highShelf;
            const bool isBell = params.type == FilterType::bell;
            const float tiltQ = (params.type == FilterType::flatTilt) ? 0.5f : -1.0f;
            const auto slopeConfig = slopeFromDb(params.slopeDb);
            if (paramSource == ch)
            {
                smoothFreq[ch][band].skip(samples);
                smoothGain[ch][band].skip(samples);
                smoothQ[ch][band].skip(samples);
                params.frequencyHz = smoothFreq[ch][band].getCurrentValue();
                params.gainDb = smoothGain[ch][band].getCurrentValue();
                params.q = smoothQ[ch][band].getCurrentValue();
            }
            else
            {
                params.frequencyHz = smoothFreq[paramSource][band].getCurrentValue();
                params.gainDb = smoothGain[paramSource][band].getCurrentValue();
                params.q = smoothQ[paramSource][band].getCurrentValue();
            }
            params.q = applyQMode(params);
            params.dynamicEnabled = false;
            const float mix = juce::jlimit(0.0f, 1.0f, params.mix);
            const float staticGainDb = params.gainDb;
            if (! params.dynamicEnabled && (isBell || isShelf || isTilt)
                && std::abs(staticGainDb) < 0.0001f)
                continue;

            const float scale = params.autoScale
                ? juce::jlimit(0.25f, 4.0f, params.frequencyHz / 1000.0f)
                : 1.0f;
            const float attackMs = juce::jmax(0.1f, params.attackMs * scale);
            const float releaseMs = juce::jmax(0.1f, params.releaseMs * scale);
            const float attackCoeff = std::exp(-1.0f / (attackMs * 0.001f * static_cast<float>(sampleRateHz)));
            const float releaseCoeff = std::exp(-1.0f / (releaseMs * 0.001f * static_cast<float>(sampleRateHz)));

            BandParams detectorParams = params;
            detectorParams.type = FilterType::bandPass;
            detectorParams.gainDb = 0.0f;
            detectorFilters[ch][band].update(detectorParams);

            const int stages = isTilt ? 2 : (isHpLp ? slopeConfig.stages : 1);
            const bool isSixDb = isHpLp && slopeConfig.stages == 0 && slopeConfig.useOnePole;
            const float resonanceMix = isSixDb
                ? juce::jlimit(0.0f, 0.8f, (params.q - 0.707f) / 6.0f)
                : 0.0f;
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
            if (resonanceMix > 0.0f)
            {
                BandParams resParams = params;
                resParams.type = FilterType::bandPass;
                resParams.gainDb = 0.0f;
                filters[ch][band][0].update(resParams);
            }

            if (isHpLp && slopeConfig.useOnePole)
            {
                if (params.type == FilterType::lowPass)
                    onePoles[ch][band].setLowPass(params.frequencyHz);
                else
                    onePoles[ch][band].setHighPass(params.frequencyHz);
            }

            const float* detData = dryData;
            if (bandUseExternal)
            {
                const int detChannel = juce::jmin(ch, detectorBuffer->getNumChannels() - 1);
                if (detChannel >= 0)
                    detData = detectorBuffer->getReadPointer(detChannel);
            }

            const bool paramsMatchStereo = numChannels == 2
                && cachedParams[0][band].frequencyHz == cachedParams[1][band].frequencyHz
                && cachedParams[0][band].gainDb == cachedParams[1][band].gainDb
                && cachedParams[0][band].q == cachedParams[1][band].q
                && cachedParams[0][band].type == cachedParams[1][band].type
                && cachedParams[0][band].slopeDb == cachedParams[1][band].slopeDb
                && cachedParams[0][band].mix == cachedParams[1][band].mix
                && cachedParams[0][band].dynamicEnabled == cachedParams[1][band].dynamicEnabled
                && cachedParams[0][band].dynamicMode == cachedParams[1][band].dynamicMode
                && cachedParams[0][band].thresholdDb == cachedParams[1][band].thresholdDb
                && cachedParams[0][band].attackMs == cachedParams[1][band].attackMs
                && cachedParams[0][band].releaseMs == cachedParams[1][band].releaseMs
                && cachedParams[0][band].autoScale == cachedParams[1][band].autoScale
                && cachedParams[0][band].useExternalDetector == cachedParams[1][band].useExternalDetector;
            const bool canStereoSimd = numChannels == 2 && ch == 0 && rightData != nullptr
                && linkStereoDetectors
                && paramsMatchStereo;
            juce::ignoreUnused(canStereoSimd);

            const bool linkDetector = linkStereoDetectors && ch == 1
                && cachedParams[0][band].frequencyHz == params.frequencyHz
                && cachedParams[0][band].gainDb == params.gainDb
                && cachedParams[0][band].q == params.q
                && cachedParams[0][band].type == params.type
                && cachedParams[0][band].slopeDb == params.slopeDb
                && cachedParams[0][band].mix == params.mix
                && cachedParams[0][band].dynamicEnabled == params.dynamicEnabled
                && cachedParams[0][band].dynamicMode == params.dynamicMode
                && cachedParams[0][band].thresholdDb == params.thresholdDb
                && cachedParams[0][band].attackMs == params.attackMs
                && cachedParams[0][band].releaseMs == params.releaseMs
                && cachedParams[0][band].autoScale == params.autoScale
                && cachedParams[0][band].useExternalDetector == params.useExternalDetector;
            float* detTempWrite = linkStereoDetectors && ch == 0 ? detectorTemp.getWritePointer(0) : nullptr;
            const float* detTempRead = linkDetector ? detectorTemp.getReadPointer(0) : nullptr;

            juce::ignoreUnused(detData);
            dynamicGainDb[ch][band].store(0.0f);

            for (int i = 0; i < samples; ++i)
            {
                const float dry = dryData[i];
                float sample = dry;
                float res = 0.0f;
                if (resonanceMix > 0.0f)
                    res = filters[ch][band][0].processSample(dry);

                float detDb = 0.0f;
                if (params.dynamicEnabled)
                {
                    if (detTempRead != nullptr)
                    {
                        detDb = detTempRead[i];
                    }
                    else
                    {
                        const float detInput = detData != nullptr ? detData[i] : dry;
                        const float detSample = detectorFilters[ch][band].processSample(detInput);
                        detDb = updateDetector(ch, band, detSample,
                                               attackCoeff, releaseCoeff,
                                               params.frequencyHz);
                        if (detTempWrite != nullptr)
                            detTempWrite[i] = detDb;
                    }
                }

                if (isHpLp && slopeConfig.useOnePole)
                    sample = onePoles[ch][band].processSample(sample);

                for (int stage = 0; stage < stages; ++stage)
                    sample = filters[ch][band][stage].processSample(sample);
                if (resonanceMix > 0.0f)
                    sample += res * resonanceMix;

                if (params.dynamicEnabled)
                {
                    const float dynamicGainDb = computeDynamicGain(params, detDb);
                    const float deltaDb = dynamicGainDb - staticGainDb;
                    const float deltaGain = juce::Decibels::decibelsToGain(deltaDb);
                    sample = dry + (sample - dry) * deltaGain;
                    this->dynamicGainDb[ch][band].store(deltaDb);
                }
                
                // v4.5 beta: Harmonic generation (odd and even harmonics) - per-band, independent for each of 12 bands
                // Apply harmonics to the EQ-processed signal with optional global oversampling
                // Odd harmonics: cubic waveshaping (input^3) for rich, warm character
                // Even harmonics: quadratic waveshaping (input^2) for tube-like saturation
                // Each band can have independent odd/even amounts and mix values
                float harmonicSample = sample;
                if (!std::isfinite(harmonicSample))
                    harmonicSample = 0.0f;
                if (!params.harmonicBypassed && 
                    ((params.oddHarmonicDb != 0.0f && params.mixOdd > 0.0f) || 
                     (params.evenHarmonicDb != 0.0f && params.mixEven > 0.0f)))
                {
                    // Safety-focused harmonic generation (no dedicated harmonic oversampling).
                    const float input = juce::jlimit(-1.0f, 1.0f, harmonicSample);
                    float oddHarm = 0.0f;
                    float evenHarm = 0.0f;

                    if (params.oddHarmonicDb != 0.0f && params.mixOdd > 0.0f)
                    {
                        const float oddGain = juce::Decibels::decibelsToGain(params.oddHarmonicDb) * params.mixOdd;
                        oddHarm = input * input * input * oddGain * 0.33f;
                    }

                    if (params.evenHarmonicDb != 0.0f && params.mixEven > 0.0f)
                    {
                        const float evenGain = juce::Decibels::decibelsToGain(params.evenHarmonicDb) * params.mixEven;
                        evenHarm = input * input * evenGain * 0.5f;
                    }

                    harmonicSample = input + oddHarm + evenHarm;
                    if (!std::isfinite(harmonicSample))
                        harmonicSample = 0.0f;
                    if (std::abs(harmonicSample) > 1.0f)
                        harmonicSample = std::tanh(harmonicSample);
                }
                
                // v4.5 beta: Sample-accurate mixing with latency compensation
                // The dry signal is already aligned (no additional delay needed for non-oversampled case)
                // For oversampled case, latency compensation is handled via harmonicDryDelayBuffers
                // This ensures the dry and wet harmonic signals are perfectly aligned for sample-accurate mixing
                float harmonicDelta = (harmonicSample - sample) * mix;
                if (!std::isfinite(harmonicDelta))
                    harmonicDelta = 0.0f;
                harmonicDelta = juce::jlimit(-4.0f, 4.0f, harmonicDelta);
                if (harmonicOnlyData != nullptr)
                    harmonicOnlyData[i] += harmonicDelta;
                channelData[i] += (harmonicSample - dry) * mix;
            }
        }
    }
}
} // namespace eqdsp
