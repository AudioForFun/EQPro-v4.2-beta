#include "EqEngine.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace eqdsp
{
void EqEngine::prepare(double sampleRate, int maxBlockSize, int numChannels)
{
    sampleRateHz = sampleRate;
    maxPreparedBlockSize = maxBlockSize;
    debugPhaseDelta = 2.0 * juce::MathConstants<double>::pi * 1000.0 / sampleRateHz;
    eqDsp.prepare(sampleRate, maxBlockSize, numChannels);
    eqDsp.reset();
    linearPhaseEq.prepare(sampleRate, maxBlockSize, numChannels);
    linearPhaseEq.reset();
    linearPhaseMsEq.prepare(sampleRate, maxBlockSize, 2);
    linearPhaseMsEq.reset();
    spectralDsp.prepare(sampleRate, maxBlockSize, numChannels);
    spectralDsp.reset();

    dryBuffer.setSize(numChannels, maxBlockSize);
    dryBuffer.clear();
    dryDelayBuffer.setSize(numChannels, maxPreparedBlockSize + maxDelaySamples + 1);
    dryDelayBuffer.clear();
    dryDelayWritePos = 0;
    mixDelaySamples = 0;
    minPhaseBuffer.setSize(numChannels, maxBlockSize);
    minPhaseBuffer.clear();
    minPhaseDelayBuffer.setSize(numChannels, maxPreparedBlockSize + maxDelaySamples + 1);
    minPhaseDelayBuffer.clear();
    minPhaseDelayWritePos = 0;
    minPhaseDelaySamples = 0;
    oversampledBuffer.setSize(numChannels, maxBlockSize * 4);
    oversampledBuffer.clear();

    globalMixSmoothed.reset(sampleRate, 0.02);
    globalMixSmoothed.setCurrentAndTargetValue(1.0f);
    outputTrimGainSmoothed.reset(sampleRate, 0.02);
    outputTrimGainSmoothed.setCurrentAndTargetValue(1.0f);
    autoGainSmoothed.reset(sampleRate, 0.08);
    autoGainSmoothed.setCurrentAndTargetValue(0.0f);

    meterSkipFactor = 1;
    if (sampleRateHz >= 256000.0)
        meterSkipFactor = 3;
    else if (sampleRateHz >= 192000.0)
        meterSkipFactor = 2;
    meterSkipCounter = 0;
}

void EqEngine::reset()
{
    eqDsp.reset();
    linearPhaseEq.reset();
    linearPhaseMsEq.reset();
    spectralDsp.reset();
    dryDelayBuffer.clear();
    dryDelayWritePos = 0;
    mixDelaySamples = 0;
    minPhaseDelayBuffer.clear();
    minPhaseDelayWritePos = 0;
    minPhaseDelaySamples = 0;
    autoGainSmoothed.setCurrentAndTargetValue(0.0f);
}

void EqEngine::process(juce::AudioBuffer<float>& buffer,
                       const ParamSnapshot& snapshot,
                       const juce::AudioBuffer<float>* detectorBuffer,
                       AnalyzerTap& preTap,
                       AnalyzerTap& postTap,
                       AnalyzerTap& harmonicTap,  // v4.5 beta: Tap for program + harmonics (red curve)
                       MeterTap& meterTap)
{
    const int numChannels = juce::jmin(buffer.getNumChannels(), snapshot.numChannels);
    auto computeRms = [](const juce::AudioBuffer<float>& buf, int channels) -> double
    {
        const int n = buf.getNumSamples();
        double sum = 0.0;
        for (int ch = 0; ch < channels; ++ch)
        {
            const float* data = buf.getReadPointer(ch);
            for (int i = 0; i < n; ++i)
                sum += static_cast<double>(data[i]) * static_cast<double>(data[i]);
        }
        return std::sqrt(sum / static_cast<double>(n * juce::jmax(1, channels)));
    };
    if (debugToneEnabled.load())
    {
        const int samples = buffer.getNumSamples();
        for (int i = 0; i < samples; ++i)
        {
            const float tone = 0.25f * std::sin(debugPhase);
            debugPhase += debugPhaseDelta;
            if (debugPhase >= 2.0 * juce::MathConstants<double>::pi)
                debugPhase -= 2.0 * juce::MathConstants<double>::pi;
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer(ch)[i] = tone;
        }
    }
    eqDsp.setGlobalBypass(snapshot.globalBypass);
    eqDsp.setSmartSoloEnabled(snapshot.smartSolo);
    eqDsp.setQMode(snapshot.qMode);
    eqDsp.setQModeAmount(snapshot.qModeAmount);
    eqDspOversampled.setGlobalBypass(snapshot.globalBypass);
    eqDspOversampled.setSmartSoloEnabled(snapshot.smartSolo);
    eqDspOversampled.setQMode(snapshot.qMode);
    eqDspOversampled.setQModeAmount(snapshot.qModeAmount);

    const int preChannels = juce::jmax(1, buffer.getNumChannels());
    const double preRms = computeRms(buffer, preChannels);
    lastPreRmsDb.store(juce::Decibels::gainToDecibels(static_cast<float>(preRms), -120.0f),
                       std::memory_order_relaxed);
    lastRmsPhaseMode.store(snapshot.phaseMode, std::memory_order_relaxed);
    lastRmsQuality.store(snapshot.linearQuality, std::memory_order_relaxed);

    if (buffer.getNumChannels() > 0)
    {
        const float* data = buffer.getReadPointer(0);
        const int samples = buffer.getNumSamples();
        int stride = 1;
        if (sampleRateHz >= 192000.0)
            stride = 4;
        else if (sampleRateHz >= 96000.0)
            stride = 2;
        if (samples > 4096)
            stride = juce::jmax(stride, samples / 2048);

        if (stride == 1)
        {
            preTap.push(data, samples);
        }
        else
        {
            constexpr int kChunk = 512;
            float temp[kChunk];
            int idx = 0;
            for (int i = 0; i < samples; i += stride)
            {
                temp[idx++] = data[i];
                if (idx == kChunk)
                {
                    preTap.push(temp, idx);
                    idx = 0;
                }
            }
            if (idx > 0)
                preTap.push(temp, idx);
        }
    }

    const bool bypassed = snapshot.globalBypass;
    if (bypassed)
        autoGainSmoothed.setTargetValue(0.0f);

    if (! bypassed)
    {
    globalMixSmoothed.setTargetValue(snapshot.globalMix);
    const bool applyGlobalMix = globalMixSmoothed.isSmoothing()
        || std::abs(snapshot.globalMix - 1.0f) > 0.0001f;
    if (applyGlobalMix)
    {
        const int latencySamples = getLatencySamples();
        if (latencySamples > 0)
            updateDryDelay(latencySamples, buffer.getNumSamples(), numChannels);
        const int copyChannels = juce::jmin(numChannels, dryBuffer.getNumChannels());
        for (int ch = 0; ch < copyChannels; ++ch)
            juce::FloatVectorOperations::copy(dryBuffer.getWritePointer(ch),
                                              buffer.getReadPointer(ch),
                                              buffer.getNumSamples());
        if (latencySamples > 0)
            applyDryDelay(dryBuffer, buffer.getNumSamples(), latencySamples);
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            const auto& src = snapshot.bands[ch][band];
            eqdsp::BandParams params;
            params.frequencyHz = src.frequencyHz;
            params.gainDb = src.gainDb;
            params.q = src.q;
            params.type = static_cast<eqdsp::FilterType>(src.type);
            params.slopeDb = src.slopeDb;
            params.bypassed = src.bypassed;
            params.solo = src.solo;
            params.mix = src.mix;
            params.dynamicEnabled = src.dynEnabled;
            params.dynamicMode = src.dynMode;
            // v4.5 beta: Harmonic parameters (per-band, independent for each of 12 bands)
            // Each band can have its own odd/even harmonic settings and bypass state
            params.oddHarmonicDb = src.oddHarmonicDb;
            params.mixOdd = src.mixOdd;
            params.evenHarmonicDb = src.evenHarmonicDb;
            params.mixEven = src.mixEven;
            params.harmonicBypassed = src.harmonicBypassed;
            // v4.5 beta: Global harmonic layer oversampling (applies to all bands uniformly)
            // This is a global parameter - when changed, all bands' harmonic processing uses the same oversampling factor
            // Only available in Natural Phase and Linear Phase modes (disabled in Real-time)
            params.harmonicOversampling = snapshot.harmonicLayerOversampling;
            params.thresholdDb = src.dynThresholdDb;
            params.attackMs = src.dynAttackMs;
            params.releaseMs = src.dynReleaseMs;
            params.autoScale = src.dynAuto;
            params.useExternalDetector = src.dynExternal;

            eqDsp.updateBandParams(ch, band, params);
            if (ch == 0)
                eqDsp.updateMsBandParams(band, params);
        }
    }

    eqDsp.setMsTargets(snapshot.msTargets);
    eqDsp.setBandChannelMasks(snapshot.bandChannelMasks);
    eqDspOversampled.setMsTargets(snapshot.msTargets);
    eqDspOversampled.setBandChannelMasks(snapshot.bandChannelMasks);

    const int phaseMode = snapshot.phaseMode;
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

        eqDspOversampled.process(oversampledBuffer, detectorBuffer);

        if (snapshot.characterMode > 0)
        {
            characterApplied = true;
            const float drive = (snapshot.characterMode == 1) ? 1.5f : 2.5f;
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
        }

        oversampler->processSamplesDown(block);
        
        // v4.5 beta: Tap signal after harmonic processing for oversampled path
        // Harmonics are processed inside eqDspOversampled, so tap after downsample
        bool hasActiveHarmonics = false;
        for (int ch = 0; ch < numChannels && !hasActiveHarmonics; ++ch)
        {
            for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
            {
                const auto& b = snapshot.bands[ch][band];
                if (!b.harmonicBypassed && 
                    ((b.oddHarmonicDb != 0.0f && b.mixOdd > 0.0f) || 
                     (b.evenHarmonicDb != 0.0f && b.mixEven > 0.0f)))
                {
                    hasActiveHarmonics = true;
                    break;
                }
            }
        }
        
        if (hasActiveHarmonics && buffer.getNumChannels() > 0)
        {
            const float* data = buffer.getReadPointer(0);
            const int samples = buffer.getNumSamples();
            int stride = 1;
            if (sampleRateHz >= 192000.0)
                stride = 4;
            else if (sampleRateHz >= 96000.0)
                stride = 2;
            if (samples > 4096)
                stride = juce::jmax(stride, samples / 2048);

            if (stride == 1)
            {
                harmonicTap.push(data, samples);
            }
            else
            {
                constexpr int kChunk = 512;
                float temp[kChunk];
                int idx = 0;
                for (int i = 0; i < samples; i += stride)
                {
                    temp[idx++] = data[i];
                    if (idx == kChunk)
                    {
                        harmonicTap.push(temp, idx);
                        idx = 0;
                    }
                }
                if (idx > 0)
                    harmonicTap.push(temp, idx);
            }
        }
    }
    else if (phaseMode == 0)
    {
        eqDsp.process(buffer, detectorBuffer);
        
        // v4.5 beta: Tap signal after harmonic processing for realtime path
        // Harmonics are processed inside eqDsp, so tap right after
        bool hasActiveHarmonics = false;
        for (int ch = 0; ch < numChannels && !hasActiveHarmonics; ++ch)
        {
            for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
            {
                const auto& b = snapshot.bands[ch][band];
                if (!b.harmonicBypassed && 
                    ((b.oddHarmonicDb != 0.0f && b.mixOdd > 0.0f) || 
                     (b.evenHarmonicDb != 0.0f && b.mixEven > 0.0f)))
                {
                    hasActiveHarmonics = true;
                    break;
                }
            }
        }
        
        if (hasActiveHarmonics && buffer.getNumChannels() > 0)
        {
            const float* data = buffer.getReadPointer(0);
            const int samples = buffer.getNumSamples();
            int stride = 1;
            if (sampleRateHz >= 192000.0)
                stride = 4;
            else if (sampleRateHz >= 96000.0)
                stride = 2;
            if (samples > 4096)
                stride = juce::jmax(stride, samples / 2048);

            if (stride == 1)
            {
                harmonicTap.push(data, samples);
            }
            else
            {
                constexpr int kChunk = 512;
                float temp[kChunk];
                int idx = 0;
                for (int i = 0; i < samples; i += stride)
                {
                    temp[idx++] = data[i];
                    if (idx == kChunk)
                    {
                        harmonicTap.push(temp, idx);
                        idx = 0;
                    }
                }
                if (idx > 0)
                    harmonicTap.push(temp, idx);
            }
        }
    }
    else
    {
        const int samples = buffer.getNumSamples();
        const int latencySamples = getLatencySamples();
        if (calibBuffer.getNumChannels() != numChannels
            || calibBuffer.getNumSamples() < samples)
        {
            calibBuffer.setSize(numChannels, samples);
        }
        for (int ch = 0; ch < numChannels; ++ch)
            juce::FloatVectorOperations::copy(calibBuffer.getWritePointer(ch),
                                              buffer.getReadPointer(ch), samples);
        float mixedPhaseAmount = 0.0f;
            bool hasSubtractive = false;
            for (int ch = 0; ch < numChannels && ! hasSubtractive; ++ch)
            {
                for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
                {
                    const auto& b = snapshot.bands[ch][band];
                    if (b.bypassed || b.mix <= 0.0005f)
                        continue;
                    const auto filterType = static_cast<eqdsp::FilterType>(b.type);
                    if (filterType == eqdsp::FilterType::lowPass
                        || filterType == eqdsp::FilterType::highPass
                        || filterType == eqdsp::FilterType::allPass)
                        continue;
                    if (b.gainDb < -0.001f)
                    {
                        hasSubtractive = true;
                        break;
                    }
                }
            }
            if (hasSubtractive)
                mixedPhaseAmount = 0.0f;

            if (mixedPhaseAmount > 0.0f)
            {
                if (minPhaseBuffer.getNumChannels() != numChannels
                    || minPhaseBuffer.getNumSamples() < samples)
                {
                    minPhaseBuffer.setSize(numChannels, samples);
                }
                for (int ch = 0; ch < numChannels; ++ch)
                    juce::FloatVectorOperations::copy(minPhaseBuffer.getWritePointer(ch),
                                                      buffer.getReadPointer(ch), samples);
                // Minimum-phase pass for transient preservation in linear modes.
                eqDsp.process(minPhaseBuffer, detectorBuffer);
                if (latencySamples > 0)
                {
                    updateMinPhaseDelay(latencySamples, samples, numChannels);
                    applyMinPhaseDelay(minPhaseBuffer, samples, latencySamples);
                }
            }

            // Linear-phase M/S is only applied to the front L/R pair.
            bool useMs = false;
            if (numChannels >= 2)
            {
                for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
                {
                    const int target = snapshot.msTargets[band];
                    if ((target == 1 || target == 2)
                        && (snapshot.bandChannelMasks[band] & 0x3u) == 0x3u)
                    {
                        useMs = true;
                        break;
                    }
                }
            }

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
            
            // v4.5 beta: Tap signal after harmonic processing for linear phase path
            // Note: In linear phase mode, harmonics are processed in the reference pass (calibBuffer)
            // but we want to tap the linear phase output which doesn't have harmonics directly
            // Actually, harmonics are processed per-band in EQDSP, so they're in the linear phase output
            // We need to create a buffer that has harmonics applied to the linear phase output
            // For now, tap after linear phase processing - harmonics will be in the final mix
            // TODO: This might need adjustment if harmonics are only in the reference path
            
            if (mixedPhaseAmount > 0.0f)
            {
                const float dryMix = 1.0f - mixedPhaseAmount;
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    auto* wet = buffer.getWritePointer(ch);
                    const auto* minPhase = minPhaseBuffer.getReadPointer(ch);
                    for (int i = 0; i < samples; ++i)
                        wet[i] = wet[i] * dryMix + minPhase[i] * mixedPhaseAmount;
                }
            }

            // Realtime reference pass for post-mode RMS calibration.
            eqDsp.process(calibBuffer, detectorBuffer);
            
            // v4.5 beta: For linear phase mode, tap from calibBuffer which has harmonics from eqDsp
            // The calibBuffer contains the realtime reference with harmonics, which is what we want to show
            bool hasActiveHarmonics = false;
            for (int ch = 0; ch < numChannels && !hasActiveHarmonics; ++ch)
            {
                for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
                {
                    const auto& b = snapshot.bands[ch][band];
                    if (!b.harmonicBypassed && 
                        ((b.oddHarmonicDb != 0.0f && b.mixOdd > 0.0f) || 
                         (b.evenHarmonicDb != 0.0f && b.mixEven > 0.0f)))
                    {
                        hasActiveHarmonics = true;
                        break;
                    }
                }
            }
            
            if (hasActiveHarmonics && calibBuffer.getNumChannels() > 0)
            {
                const float* data = calibBuffer.getReadPointer(0);
                const int samples = calibBuffer.getNumSamples();
                int stride = 1;
                if (sampleRateHz >= 192000.0)
                    stride = 4;
                else if (sampleRateHz >= 96000.0)
                    stride = 2;
                if (samples > 4096)
                    stride = juce::jmax(stride, samples / 2048);

                if (stride == 1)
                {
                    harmonicTap.push(data, samples);
                }
                else
                {
                    constexpr int kChunk = 512;
                    float temp[kChunk];
                    int idx = 0;
                    for (int i = 0; i < samples; i += stride)
                    {
                        temp[idx++] = data[i];
                        if (idx == kChunk)
                        {
                            harmonicTap.push(temp, idx);
                            idx = 0;
                        }
                    }
                    if (idx > 0)
                        harmonicTap.push(temp, idx);
                }
            }

            // Fallback: if the linear output collapses, keep realtime EQ so audio never drops.
            const double linRms = computeRms(buffer, numChannels);
            const double refRms = computeRms(calibBuffer, numChannels);
            if (linRms < 1.0e-9 && refRms > 1.0e-6)
            {
                for (int ch = 0; ch < numChannels; ++ch)
                    juce::FloatVectorOperations::copy(buffer.getWritePointer(ch),
                                                      calibBuffer.getReadPointer(ch), samples);
            }
    }

    spectralDsp.setEnabled(snapshot.spectralEnabled);
    if (snapshot.spectralEnabled)
    {
        spectralDsp.setParams(snapshot.spectralThresholdDb,
                              snapshot.spectralRatio,
                              snapshot.spectralAttackMs,
                              snapshot.spectralReleaseMs,
                              snapshot.spectralMix);
        spectralDsp.process(buffer);
    }

    if (snapshot.characterMode > 0 && ! characterApplied)
    {
        const float drive = (snapshot.characterMode == 1) ? 1.5f : 2.5f;
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

    if (applyGlobalMix)
    {
        const int mixChannels = juce::jmin(numChannels, dryBuffer.getNumChannels());
        const int numSamples = buffer.getNumSamples();
        const float wetStart = globalMixSmoothed.getCurrentValue();
        globalMixSmoothed.skip(numSamples);
        const float wetEnd = globalMixSmoothed.getCurrentValue();
        const float wetStep = (wetEnd - wetStart) / static_cast<float>(numSamples);

        float wet = wetStart;
        for (int i = 0; i < numSamples; ++i)
        {
            const float dryGain = 1.0f - wet;
            for (int ch = 0; ch < mixChannels; ++ch)
            {
                auto* data = buffer.getWritePointer(ch);
                const float dry = dryBuffer.getReadPointer(ch)[i];
                data[i] = dry * dryGain + data[i] * wet;
            }
            wet += wetStep;
        }
    }

    if (snapshot.phaseInvert)
        buffer.applyGain(-1.0f);

    float autoGainDb = 0.0f;
    if (snapshot.autoGainEnabled)
    {
        const double postRms = computeRms(buffer, numChannels);
        if (preRms > 1.0e-9 && postRms > 1.0e-9)
        {
            const float preDb = juce::Decibels::gainToDecibels(static_cast<float>(preRms), -120.0f);
            const float postDb = juce::Decibels::gainToDecibels(static_cast<float>(postRms), -120.0f);
            const float deltaDb = preDb - postDb;
            autoGainDb = juce::jlimit(-12.0f, 12.0f, deltaDb) * snapshot.gainScale;
        }
    }
    autoGainSmoothed.setTargetValue(autoGainDb);
    autoGainSmoothed.skip(buffer.getNumSamples());
    const float autoGainSmoothedDb = autoGainSmoothed.getCurrentValue();

    outputTrimGainSmoothed.setTargetValue(
        juce::Decibels::decibelsToGain(snapshot.outputTrimDb + autoGainSmoothedDb));
    if (outputTrimGainSmoothed.isSmoothing()
        || std::abs(snapshot.outputTrimDb) > 0.001f)
    {
        const int numSamples = buffer.getNumSamples();
        const float startGain = outputTrimGainSmoothed.getCurrentValue();
        outputTrimGainSmoothed.skip(numSamples);
        const float endGain = outputTrimGainSmoothed.getCurrentValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.applyGainRamp(ch, 0, numSamples, startGain, endGain);
    }

    if (snapshot.phaseMode != 0)
    {
        // Match linear/natural output level to realtime RMS for consistent meters.
        const int numSamples = buffer.getNumSamples();
        const int refChannels = juce::jmin(numChannels, calibBuffer.getNumChannels());
        auto mixSmoothed = globalMixSmoothed;
        if (applyGlobalMix)
        {
            const float wetStart = mixSmoothed.getCurrentValue();
            mixSmoothed.skip(numSamples);
            const float wetEnd = mixSmoothed.getCurrentValue();
            const float wetStep = (wetEnd - wetStart) / static_cast<float>(numSamples);
            float wet = wetStart;
            for (int i = 0; i < numSamples; ++i)
            {
                const float dryGain = 1.0f - wet;
                for (int ch = 0; ch < refChannels; ++ch)
                {
                    auto* data = calibBuffer.getWritePointer(ch);
                    const float dry = dryBuffer.getReadPointer(ch)[i];
                    data[i] = dry * dryGain + data[i] * wet;
                }
                wet += wetStep;
            }
        }

        auto trimSmoothed = outputTrimGainSmoothed;
        const float startGain = trimSmoothed.getCurrentValue();
        trimSmoothed.skip(numSamples);
        const float endGain = trimSmoothed.getCurrentValue();
        for (int ch = 0; ch < refChannels; ++ch)
            calibBuffer.applyGainRamp(ch, 0, numSamples, startGain, endGain);

        auto computeRms = [](const juce::AudioBuffer<float>& buf, int channels) -> double
        {
            const int n = buf.getNumSamples();
            double sum = 0.0;
            for (int ch = 0; ch < channels; ++ch)
            {
                const float* data = buf.getReadPointer(ch);
                for (int i = 0; i < n; ++i)
                    sum += static_cast<double>(data[i]) * static_cast<double>(data[i]);
            }
            return std::sqrt(sum / static_cast<double>(n * juce::jmax(1, channels)));
        };
        const double refRms = computeRms(calibBuffer, refChannels);
        const double linRms = computeRms(buffer, numChannels);
        if (refRms > 1.0e-9 && linRms > 1.0e-9)
            buffer.applyGain(static_cast<float>(refRms / linRms));
    }
    }

    if (++meterSkipCounter >= meterSkipFactor)
    {
        meterTap.process(buffer, numChannels);
        meterSkipCounter = 0;
    }

    const int postSamples = buffer.getNumSamples();
    const int postChannels = juce::jmax(1, buffer.getNumChannels());
    double postSum = 0.0;
    for (int ch = 0; ch < postChannels; ++ch)
    {
        const float* data = buffer.getReadPointer(ch);
        for (int i = 0; i < postSamples; ++i)
            postSum += static_cast<double>(data[i]) * static_cast<double>(data[i]);
    }
    const double postRms = std::sqrt(postSum / static_cast<double>(postSamples * postChannels));
    lastPostRmsDb.store(juce::Decibels::gainToDecibels(static_cast<float>(postRms), -120.0f),
                        std::memory_order_relaxed);

    if (buffer.getNumChannels() > 0)
    {
        const float* data = buffer.getReadPointer(0);
        const int samples = buffer.getNumSamples();
        int stride = 1;
        if (sampleRateHz >= 192000.0)
            stride = 4;
        else if (sampleRateHz >= 96000.0)
            stride = 2;
        if (samples > 4096)
            stride = juce::jmax(stride, samples / 2048);

        if (stride == 1)
        {
            postTap.push(data, samples);
        }
        else
        {
            constexpr int kChunk = 512;
            float temp[kChunk];
            int idx = 0;
            for (int i = 0; i < samples; i += stride)
            {
                temp[idx++] = data[i];
                if (idx == kChunk)
                {
                    postTap.push(temp, idx);
                    idx = 0;
                }
            }
            if (idx > 0)
                postTap.push(temp, idx);
        }
    }
}

float EqEngine::getLastPreRmsDb() const
{
    return lastPreRmsDb.load(std::memory_order_relaxed);
}

float EqEngine::getLastPostRmsDb() const
{
    return lastPostRmsDb.load(std::memory_order_relaxed);
}

int EqEngine::getLastRmsPhaseMode() const
{
    return lastRmsPhaseMode.load(std::memory_order_relaxed);
}

int EqEngine::getLastRmsQuality() const
{
    return lastRmsQuality.load(std::memory_order_relaxed);
}

void EqEngine::setOversampling(int index)
{
    oversamplingIndex = index;
}

void EqEngine::setDebugToneEnabled(bool enabled)
{
    debugToneEnabled.store(enabled);
}

void EqEngine::setDebugToneFrequency(float frequencyHz)
{
    const double freq = juce::jmax(10.0, static_cast<double>(frequencyHz));
    debugPhaseDelta = 2.0 * juce::MathConstants<double>::pi * freq / sampleRateHz;
}

int EqEngine::getLatencySamples() const
{
    return linearPhaseEq.getLatencySamples();
}

void EqEngine::updateLinearPhase(const ParamSnapshot& snapshot, double sampleRate)
{
    if (snapshot.phaseMode == 0)
    {
        linearPhaseEq.setLatencySamples(0);
        lastPhaseMode = snapshot.phaseMode;
        return;
    }

    // Adaptive taps: increase FIR length for complex band settings.
    float maxQ = 0.0f;
    float maxGain = 0.0f;
    float maxSlope = 0.0f;
    int activeBands = 0;
    for (int ch = 0; ch < snapshot.numChannels; ++ch)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            const auto& b = snapshot.bands[ch][band];
            if (b.bypassed || b.mix <= 0.0005f)
                continue;
            ++activeBands;
            maxQ = std::max(maxQ, b.q);
            maxGain = std::max(maxGain, std::abs(b.gainDb));
            maxSlope = std::max(maxSlope, b.slopeDb);
        }
    }

    int complexityBoost = 0;
    if (maxQ >= 10.0f || maxGain >= 18.0f || maxSlope >= 48.0f || activeBands >= 8)
        complexityBoost = 1;
    if (maxQ >= 14.0f || maxGain >= 30.0f || maxSlope >= 72.0f || activeBands >= 10)
        complexityBoost = 2;

    const int quality = juce::jlimit(0, 4, snapshot.linearQuality);
    const int index = juce::jmin(4, quality + complexityBoost);
    const std::array<int, 5> naturalTaps { 128, 256, 512, 1024, 2048 };
    const std::array<int, 5> linearTaps { 512, 1024, 2048, 4096, 8192 };
    const int taps = snapshot.phaseMode == 1 ? naturalTaps[static_cast<size_t>(index)]
                                             : linearTaps[static_cast<size_t>(index)];

    const uint64_t hash = computeParamsHash(snapshot);
    if (hash == lastParamHash && taps == lastTaps && snapshot.phaseMode == lastPhaseMode
        && snapshot.linearQuality == lastLinearQuality && snapshot.linearWindow == lastWindowIndex)
        return;

    // Use uniform convolution for now to avoid dropouts in lower quality modes.
    const int headSize = 0;

    rebuildLinearPhase(snapshot, taps, headSize, sampleRate);
    juce::Logger::writeToLog("LinearPhase rebuild: mode=" + juce::String(snapshot.phaseMode)
                             + " quality=" + juce::String(quality)
                             + " taps=" + juce::String(taps)
                             + " window=" + juce::String(snapshot.linearWindow));
    lastParamHash = hash;
    lastTaps = taps;
    lastPhaseMode = snapshot.phaseMode;
    lastLinearQuality = snapshot.linearQuality;
    lastWindowIndex = snapshot.linearWindow;
}

uint64_t EqEngine::computeParamsHash(const ParamSnapshot& snapshot) const
{
    auto hash = uint64_t { 1469598103934665603ull };
    const auto hashFloat = [&hash](float value)
    {
        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        hash ^= bits;
        hash *= 1099511628211ull;
    };

    for (int ch = 0; ch < snapshot.numChannels; ++ch)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            const auto& b = snapshot.bands[ch][band];
            hashFloat(b.frequencyHz);
            hashFloat(b.gainDb);
            hashFloat(b.q);
            hashFloat(static_cast<float>(b.type));
            hashFloat(b.bypassed ? 1.0f : 0.0f);
            hashFloat(b.mix);
            hashFloat(b.slopeDb);
            if (ch == 0)
                hashFloat(static_cast<float>(b.msTarget));
        }
    }
    return hash;
}

void EqEngine::rebuildLinearPhase(const ParamSnapshot& snapshot, int taps, int headSize, double sampleRate)
{
    linearPhaseEq.beginImpulseUpdate(headSize, snapshot.numChannels);
    linearPhaseMsEq.beginImpulseUpdate(headSize, snapshot.numChannels >= 2 ? 2 : 0);
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

    int windowIndex = snapshot.linearWindow;
    if (windowIndex == 0)
    {
        if (snapshot.linearQuality >= 4)
            windowIndex = 2; // Kaiser for maximum stop-band attenuation.
        else if (snapshot.linearQuality >= 2)
            windowIndex = 1; // Blackman for smoother bands.
        else
            windowIndex = 0; // Hann for lowest latency.
    }
    const auto method = windowIndex == 1
        ? juce::dsp::WindowingFunction<float>::blackman
        : (windowIndex == 2 ? juce::dsp::WindowingFunction<float>::kaiser
                                       : juce::dsp::WindowingFunction<float>::hann);

    if (firWindow == nullptr || static_cast<int>(firImpulse.size()) != taps
        || firWindowMethod != static_cast<int>(method))
    {
        firWindow = std::make_unique<juce::dsp::WindowingFunction<float>>(taps, method);
        firWindowMethod = static_cast<int>(method);
    }

    auto buildImpulse = [&](int channel, std::function<bool(int)> includeBand) -> juce::AudioBuffer<float>
    {
        std::fill(firData.begin(), firData.end(), 0.0f);
        const double nyquist = sampleRate * 0.5;
        std::vector<float> desiredMag;
        desiredMag.resize(static_cast<size_t>(fftSize / 2 + 1), 1.0f);

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

                const auto& b = snapshot.bands[channel][band];
                if (b.bypassed || b.mix <= 0.0001f)
                    continue;

                const double mix = b.mix;
                const double gainDb = b.gainDb;
                const double q = std::max(0.1f, b.q);
                const double freqParam = b.frequencyHz;
                const int type = b.type;
                const float slopeDb = b.slopeDb;

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
                        default:
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

                const double mixedMag = 1.0 + mix * (mag - 1.0);
                totalMag *= mixedMag;
            }

            totalMag = std::max(1.0e-4, totalMag);
            if (bin <= fftSize / 2)
                desiredMag[static_cast<size_t>(bin)] = static_cast<float>(totalMag);

            firData[static_cast<size_t>(bin) * 2] = static_cast<float>(totalMag);
            firData[static_cast<size_t>(bin) * 2 + 1] = 0.0f;
        }

        firFft->performRealOnlyInverseTransform(firData.data());
        for (int i = 0; i < taps; ++i)
            firImpulse[static_cast<size_t>(i)] = firData[static_cast<size_t>(i)] / static_cast<float>(fftSize);

        firWindow->multiplyWithWindowingTable(firImpulse.data(), taps);

        std::fill(firData.begin(), firData.end(), 0.0f);
        for (int i = 0; i < taps; ++i)
            firData[static_cast<size_t>(i)] = firImpulse[static_cast<size_t>(i)];
        firFft->performRealOnlyForwardTransform(firData.data());

        double numerator = 0.0;
        double denominator = 0.0;
        for (int bin = 0; bin <= fftSize / 2; ++bin)
        {
            const double re = firData[static_cast<size_t>(bin) * 2];
            const double im = firData[static_cast<size_t>(bin) * 2 + 1];
            const double actualMag = std::sqrt(re * re + im * im);
            const double targetMag = desiredMag[static_cast<size_t>(bin)];
            numerator += targetMag * actualMag;
            denominator += actualMag * actualMag;
        }
        const double scale = (denominator > 1.0e-9) ? (numerator / denominator) : 1.0;
        if (std::abs(scale - 1.0) > 1.0e-6)
        {
            for (int i = 0; i < taps; ++i)
                firImpulse[static_cast<size_t>(i)] = static_cast<float>(firImpulse[static_cast<size_t>(i)] * scale);
        }
        juce::AudioBuffer<float> impulse(1, taps);
        impulse.copyFrom(0, 0, firImpulse.data(), taps);
        return impulse;
    };

    auto ensureImpulseValid = [](juce::AudioBuffer<float>& impulse, const juce::String& tag)
    {
        const int samples = impulse.getNumSamples();
        if (samples <= 0)
            return;
        const float* data = impulse.getReadPointer(0);
        double sum = 0.0;
        for (int i = 0; i < samples; ++i)
            sum += static_cast<double>(data[i]) * static_cast<double>(data[i]);
        const double rms = std::sqrt(sum / static_cast<double>(samples));
        if (rms < 1.0e-7)
        {
            impulse.clear();
            impulse.setSample(0, 0, 1.0f);
            juce::Logger::writeToLog("LinearPhase: impulse fallback -> delta (" + tag + ")");
        }
    };

    for (int ch = 0; ch < snapshot.numChannels; ++ch)
    {
        auto impulse = buildImpulse(ch, [&](int band)
        {
            if ((snapshot.bandChannelMasks[band] & (1u << static_cast<uint32_t>(ch))) == 0)
                return false;
            const int target = snapshot.msTargets[band];
            const bool isMs = target == 1 || target == 2;
            const bool isFrontPair = (snapshot.bandChannelMasks[band] & 0x3u) == 0x3u;
            return ! isMs || ! isFrontPair;
        });
        ensureImpulseValid(impulse, "ch=" + juce::String(ch));
        linearPhaseEq.loadImpulse(ch, std::move(impulse), sampleRate);
    }

    if (snapshot.numChannels >= 2)
    {
        auto includeMid = [&](int band)
        {
        const int target = snapshot.msTargets[band];
        const bool isFrontPair = (snapshot.bandChannelMasks[band] & 0x3u) == 0x3u;
        return isFrontPair && target == 1;
        };
        auto includeSide = [&](int band)
        {
        const int target = snapshot.msTargets[band];
        const bool isFrontPair = (snapshot.bandChannelMasks[band] & 0x3u) == 0x3u;
        return isFrontPair && target == 2;
        };
        auto midImpulse = buildImpulse(0, includeMid);
        auto sideImpulse = buildImpulse(0, includeSide);
        ensureImpulseValid(midImpulse, "mid");
        ensureImpulseValid(sideImpulse, "side");
        linearPhaseMsEq.loadImpulse(0, std::move(midImpulse), sampleRate);
        linearPhaseMsEq.loadImpulse(1, std::move(sideImpulse), sampleRate);
    }
    linearPhaseEq.endImpulseUpdate();
    linearPhaseMsEq.endImpulseUpdate();

    const int latency = (taps - 1) / 2;
    linearPhaseEq.setLatencySamples(latency);
}

void EqEngine::updateOversampling(const ParamSnapshot& snapshot, double sampleRate, int maxBlockSize, int channels)
{
    oversamplingIndex = 0;
    oversampler.reset();
    oversampledBuffer.setSize(0, 0);
    juce::ignoreUnused(snapshot, sampleRate, maxBlockSize, channels);
    return;
}

void EqEngine::updateDryDelay(int latencySamples, int maxBlockSize, int numChannels)
{
    const int targetDelay = juce::jmax(0, latencySamples);
    maxPreparedBlockSize = juce::jmax(maxPreparedBlockSize, maxBlockSize);
    const int neededSize = maxPreparedBlockSize + maxDelaySamples + 1;
    if (dryDelayBuffer.getNumChannels() != numChannels
        || dryDelayBuffer.getNumSamples() != neededSize)
    {
        dryDelayBuffer.setSize(numChannels, neededSize);
        dryDelayBuffer.clear();
        dryDelayWritePos = 0;
    }

    if (targetDelay != mixDelaySamples)
    {
        mixDelaySamples = targetDelay;
        dryDelayBuffer.clear();
        dryDelayWritePos = 0;
    }
}

void EqEngine::applyDryDelay(juce::AudioBuffer<float>& dry, int numSamples, int delaySamples)
{
    if (delaySamples <= 0)
        return;

    const int bufferSize = dryDelayBuffer.getNumSamples();
    if (bufferSize <= 1)
        return;
    delaySamples = juce::jmin(delaySamples, bufferSize - 1);

    int writePos = dryDelayWritePos;
    const int channels = juce::jmin(dry.getNumChannels(), dryDelayBuffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* delayData = dryDelayBuffer.getWritePointer(ch);
        auto* dryData = dry.getWritePointer(ch);
        int localWrite = writePos;
        for (int i = 0; i < numSamples; ++i)
        {
            delayData[localWrite] = dryData[i];
            int readPos = localWrite - delaySamples;
            if (readPos < 0)
                readPos += bufferSize;
            dryData[i] = delayData[readPos];
            if (++localWrite >= bufferSize)
                localWrite = 0;
        }
    }
    dryDelayWritePos = (writePos + numSamples) % bufferSize;
}

void EqEngine::updateMinPhaseDelay(int latencySamples, int maxBlockSize, int numChannels)
{
    const int targetDelay = juce::jmax(0, latencySamples);
    maxPreparedBlockSize = juce::jmax(maxPreparedBlockSize, maxBlockSize);
    const int neededSize = maxPreparedBlockSize + maxDelaySamples + 1;
    if (minPhaseDelayBuffer.getNumChannels() != numChannels
        || minPhaseDelayBuffer.getNumSamples() != neededSize)
    {
        minPhaseDelayBuffer.setSize(numChannels, neededSize);
        minPhaseDelayBuffer.clear();
        minPhaseDelayWritePos = 0;
    }

    if (targetDelay != minPhaseDelaySamples)
    {
        minPhaseDelaySamples = targetDelay;
        minPhaseDelayBuffer.clear();
        minPhaseDelayWritePos = 0;
    }
}

void EqEngine::applyMinPhaseDelay(juce::AudioBuffer<float>& buffer, int numSamples, int delaySamples)
{
    if (delaySamples <= 0)
        return;

    const int bufferSize = minPhaseDelayBuffer.getNumSamples();
    if (bufferSize <= 1)
        return;
    delaySamples = juce::jmin(delaySamples, bufferSize - 1);

    int writePos = minPhaseDelayWritePos;
    const int channels = juce::jmin(buffer.getNumChannels(), minPhaseDelayBuffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* delayData = minPhaseDelayBuffer.getWritePointer(ch);
        auto* data = buffer.getWritePointer(ch);
        int localWrite = writePos;
        for (int i = 0; i < numSamples; ++i)
        {
            delayData[localWrite] = data[i];
            int readPos = localWrite - delaySamples;
            if (readPos < 0)
                readPos += bufferSize;
            data[i] = delayData[readPos];
            if (++localWrite >= bufferSize)
                localWrite = 0;
        }
    }
    minPhaseDelayWritePos = (writePos + numSamples) % bufferSize;
}

EQDSP& EqEngine::getEqDsp()
{
    return eqDsp;
}

const EQDSP& EqEngine::getEqDsp() const
{
    return eqDsp;
}

LinearPhaseEQ& EqEngine::getLinearPhaseEq()
{
    return linearPhaseEq;
}
} // namespace eqdsp
