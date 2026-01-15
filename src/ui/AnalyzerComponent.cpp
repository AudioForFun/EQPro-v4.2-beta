#include "AnalyzerComponent.h"
#include <complex>
#include <algorithm>
#include <functional>
#include "../PluginProcessor.h"
#include "../util/FFTUtils.h"
#include "../util/Smoothing.h"
#include "../util/ColorUtils.h"

namespace
{
constexpr float kMinFreq = 20.0f;
constexpr float kMaxDb = 24.0f;
constexpr float kMinDb = -24.0f;
constexpr float kAnalyzerMinDb = -90.0f;
constexpr float kAnalyzerMaxDb = 6.0f;
constexpr float kPhaseMin = -2.0f * juce::MathConstants<float>::pi;
constexpr float kPhaseMax = 2.0f * juce::MathConstants<float>::pi;
constexpr float kPointRadius = 5.0f;
constexpr float kHitRadius = 12.0f;
constexpr float kSmoothingCoeff = 0.2f;

const juce::String kParamFreqSuffix = "freq";
const juce::String kParamGainSuffix = "gain";
const juce::String kParamQSuffix = "q";
const juce::String kParamTypeSuffix = "type";
const juce::String kParamBypassSuffix = "bypass";
const juce::String kParamSlopeSuffix = "slope";
} // namespace

AnalyzerComponent::AnalyzerComponent(EQProAudioProcessor& processor)
    : processorRef(processor),
      parameters(processor.getParameters()),
      externalFifo(processor.getAnalyzerExternalFifo()),
      fft(fftOrder),
      window(fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    preMagnitudes.fill(kAnalyzerMinDb);
    postMagnitudes.fill(kAnalyzerMinDb);
    externalMagnitudes.fill(kAnalyzerMinDb);
    selectedBands.push_back(selectedBand);
    startTimerHz(30);
}

void AnalyzerComponent::setSelectedBand(int bandIndex)
{
    selectedBand = juce::jlimit(0, ParamIDs::kBandsPerChannel - 1, bandIndex);
    selectedBands.clear();
    selectedBands.push_back(selectedBand);
    repaint();
}

void AnalyzerComponent::setSelectedChannel(int channelIndex)
{
    selectedChannel = juce::jlimit(0, ParamIDs::kMaxChannels - 1, channelIndex);
    repaint();
}

void AnalyzerComponent::setShowPhase(bool shouldShow)
{
    showPhase = shouldShow;
    updateCurves();
    repaint();
}

void AnalyzerComponent::setTheme(const ThemeColors& newTheme)
{
    theme = newTheme;
    repaint();
}

void AnalyzerComponent::paint(juce::Graphics& g)
{
    auto plotArea = getPlotArea();
    auto magnitudeArea = getMagnitudeArea();
    auto phaseArea = getPhaseArea();
    juce::ColourGradient grad(theme.analyzerBg, plotArea.getTopLeft().toFloat(),
                              theme.panel, plotArea.getBottomLeft().toFloat(), false);
    g.setGradientFill(grad);
    g.fillRect(plotArea);

    g.setColour(theme.panelOutline);
    g.drawRect(plotArea);

    g.setColour(theme.grid);
    for (int i = 1; i < 4; ++i)
    {
        const int y = magnitudeArea.getY() + magnitudeArea.getHeight() * i / 4;
        g.drawHorizontalLine(y, static_cast<float>(plotArea.getX()),
                             static_cast<float>(plotArea.getRight()));
    }

    if (showPhase)
    {
        g.setColour(theme.analyzerPhaseBg);
        g.fillRect(phaseArea);
        g.setColour(theme.grid);
        g.drawRect(phaseArea);
    }

    drawLabels(g, magnitudeArea);

    const float maxFreq = std::min(20000.0f, lastSampleRate * 0.5f);

    juce::Path prePath;
    juce::Path postPath;
    bool started = false;

    for (int bin = 1; bin < fftBins; ++bin)
    {
        const float freq = (lastSampleRate * bin) / static_cast<float>(fftSize);
        if (freq < kMinFreq || freq > maxFreq)
            continue;

        const float xNorm = FFTUtils::freqToNorm(freq, kMinFreq, maxFreq);
        const float x = plotArea.getX() + xNorm * plotArea.getWidth();
        const float preDb = preMagnitudes[bin];
        const float postDb = postMagnitudes[bin];
        const float preY = juce::jmap(preDb, kAnalyzerMinDb, kAnalyzerMaxDb,
                                      static_cast<float>(magnitudeArea.getBottom()),
                                      static_cast<float>(magnitudeArea.getY()));
        const float postY = juce::jmap(postDb, kAnalyzerMinDb, kAnalyzerMaxDb,
                                       static_cast<float>(magnitudeArea.getBottom()),
                                       static_cast<float>(magnitudeArea.getY()));

        if (! started)
        {
            prePath.startNewSubPath(x, preY);
            postPath.startNewSubPath(x, postY);
            started = true;
        }
        else
        {
            prePath.lineTo(x, preY);
            postPath.lineTo(x, postY);
        }
    }

    g.setColour(theme.accent.withAlpha(0.6f));
    g.strokePath(prePath, juce::PathStrokeType(1.2f));

    g.setColour(theme.accentAlt.withAlpha(0.7f));
    g.strokePath(postPath, juce::PathStrokeType(1.2f));

    const bool showExternal = parameters.getRawParameterValue(ParamIDs::analyzerExternal) != nullptr
        && parameters.getRawParameterValue(ParamIDs::analyzerExternal)->load() > 0.5f;
    if (showExternal)
    {
        juce::Path extPath;
        extPath.startNewSubPath(plotArea.getX(), gainToY(externalMagnitudes.front()));
        for (int x = 1; x < static_cast<int>(externalMagnitudes.size()); ++x)
            extPath.lineTo(plotArea.getX() + x,
                           gainToY(externalMagnitudes[static_cast<size_t>(x)]));
        g.setColour(theme.accentAlt.withAlpha(0.4f));
        g.strokePath(extPath, juce::PathStrokeType(1.0f));
    }

    if (! eqCurveDb.empty())
    {
        juce::Path eqPath;
        eqPath.startNewSubPath(plotArea.getX(),
                               gainToY(eqCurveDb.front()));

        for (int x = 1; x < static_cast<int>(eqCurveDb.size()); ++x)
            eqPath.lineTo(plotArea.getX() + x,
                          gainToY(eqCurveDb[static_cast<size_t>(x)]));

        g.setColour(theme.text.withAlpha(0.8f));
        g.strokePath(eqPath, juce::PathStrokeType(2.0f));
    }

    if (! selectedBandCurveDb.empty())
    {
        juce::Path bandPath;
        bandPath.startNewSubPath(plotArea.getX(),
                                 gainToY(selectedBandCurveDb.front()));
        for (int x = 1; x < static_cast<int>(selectedBandCurveDb.size()); ++x)
            bandPath.lineTo(plotArea.getX() + x,
                            gainToY(selectedBandCurveDb[static_cast<size_t>(x)]));

        const auto bandColour = ColorUtils::bandColour(selectedBand);
        g.setColour(bandColour.withAlpha(0.85f));
        g.strokePath(bandPath, juce::PathStrokeType(1.5f));
    }

    bandPoints.clear();
    bandPoints.reserve(ParamIDs::kBandsPerChannel);

    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
    {
        const float freq = getBandParameter(band, kParamFreqSuffix);
        const float gain = getBandParameter(band, kParamGainSuffix);
        const bool bypassed = getBandBypassed(band);

        const float x = frequencyToX(freq);
        const float y = gainToY(gain);
        const juce::Point<float> point(x, y);
        bandPoints.push_back(point);

        auto colour = ColorUtils::bandColour(band);
        if (bypassed)
            colour = colour.withAlpha(0.25f);

        g.setColour(colour);
        const bool isSelected = std::find(selectedBands.begin(), selectedBands.end(), band)
            != selectedBands.end();
        const float radius = isSelected ? kPointRadius + 2.0f : kPointRadius;
        g.fillEllipse(point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f);

        if (isSelected)
        {
            g.setColour(theme.text.withAlpha(0.6f));
            g.drawEllipse(point.x - radius - 2.0f, point.y - radius - 2.0f,
                          (radius + 2.0f) * 2.0f, (radius + 2.0f) * 2.0f, 1.0f);
        }

        if (getBandType(band) == static_cast<int>(eqdsp::FilterType::tilt)
            || getBandType(band) == static_cast<int>(eqdsp::FilterType::flatTilt))
        {
            g.setColour(theme.text.withAlpha(0.85f));
            g.drawLine(point.x - radius * 0.7f, point.y + radius * 0.7f,
                       point.x + radius * 0.7f, point.y - radius * 0.7f, 1.2f);
        }

        g.setColour(theme.textMuted);
        g.setFont(10.0f);
        g.drawFittedText(juce::String(band + 1),
                         juce::Rectangle<int>(static_cast<int>(point.x + radius + 2.0f),
                                              static_cast<int>(point.y - 7.0f),
                                              20, 14),
                         juce::Justification::left, 1);
    }
    if (showPhase && ! phaseCurve.empty())
    {
        juce::Path phasePath;
        phasePath.startNewSubPath(phaseArea.getX(),
                                  phaseToY(phaseCurve.front()));
        for (int x = 1; x < static_cast<int>(phaseCurve.size()); ++x)
            phasePath.lineTo(phaseArea.getX() + x,
                             phaseToY(phaseCurve[static_cast<size_t>(x)]));

        g.setColour(theme.accent.withAlpha(0.6f));
        g.strokePath(phasePath, juce::PathStrokeType(1.0f));
    }
}

void AnalyzerComponent::resized()
{
    updateCurves();
}

void AnalyzerComponent::mouseDown(const juce::MouseEvent& event)
{
    draggingBand = -1;
    const auto plotArea = getMagnitudeArea().toFloat();
    if (! plotArea.contains(event.position))
        return;

    float closest = kHitRadius;
    int closestBand = -1;
    for (int i = 0; i < static_cast<int>(bandPoints.size()); ++i)
    {
        const float distance = bandPoints[static_cast<size_t>(i)].getDistanceFrom(event.position);
        if (distance < closest)
        {
            closest = distance;
            closestBand = i;
        }
    }

    if (closestBand >= 0)
    {
        draggingBand = closestBand;
        if (event.mods.isShiftDown())
        {
            const auto it = std::find(selectedBands.begin(), selectedBands.end(), closestBand);
            if (it == selectedBands.end())
                selectedBands.push_back(closestBand);
            else if (selectedBands.size() > 1)
                selectedBands.erase(it);
            selectedBand = closestBand;
        }
        else
        {
            setSelectedBand(closestBand);
        }

        dragStartPos = event.position;
        dragBands.clear();
        for (const int band : selectedBands)
        {
            DragBandState state;
            state.band = band;
            state.freq = getBandParameter(band, kParamFreqSuffix);
            state.gain = getBandParameter(band, kParamGainSuffix);
            dragBands.push_back(state);
        }
        if (onBandSelected)
            onBandSelected(closestBand);
    }
}

void AnalyzerComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (draggingBand < 0)
        return;

    const auto plotArea = getMagnitudeArea().toFloat();
    if (! plotArea.contains(event.position))
        return;

    const float targetFreq = event.mods.isAltDown()
        ? snapFrequencyToPeak(event.position.x)
        : xToFrequency(event.position.x);
    const float targetGain = yToGain(event.position.y);
    float startFreq = targetFreq;
    float startGain = targetGain;
    for (const auto& state : dragBands)
    {
        if (state.band == draggingBand)
        {
            startFreq = state.freq;
            startGain = state.gain;
            break;
        }
    }

    const float ratio = (startFreq > 0.0f) ? (targetFreq / startFreq) : 1.0f;
    const float deltaGain = targetGain - startGain;
    for (const auto& state : dragBands)
    {
        const float newFreq = state.freq * ratio;
        const float newGain = state.gain + deltaGain;
        setBandParameter(state.band, kParamFreqSuffix, newFreq);
        setBandParameter(state.band, kParamGainSuffix, newGain);
    }
    repaint();
}

void AnalyzerComponent::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    draggingBand = -1;
    dragBands.clear();
}

void AnalyzerComponent::mouseDoubleClick(const juce::MouseEvent& event)
{
    const auto plotArea = getMagnitudeArea().toFloat();
    if (! plotArea.contains(event.position))
        return;

    int targetBand = -1;
    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
    {
        if (getBandBypassed(band))
        {
            targetBand = band;
            break;
        }
    }

    if (targetBand < 0)
        targetBand = selectedBand;

    const float freq = xToFrequency(event.position.x);
    const float gain = yToGain(event.position.y);

    setBandParameter(targetBand, kParamFreqSuffix, freq);
    setBandParameter(targetBand, kParamGainSuffix, gain);
    setBandParameter(targetBand, kParamBypassSuffix, 0.0f);

    setSelectedBand(targetBand);
    if (onBandSelected)
        onBandSelected(targetBand);
}

void AnalyzerComponent::timerCallback()
{
    const int rangeIndex = parameters.getRawParameterValue(ParamIDs::analyzerRange) != nullptr
        ? static_cast<int>(parameters.getRawParameterValue(ParamIDs::analyzerRange)->load())
        : 2;
    const float rangeDb = (rangeIndex == 0 ? 3.0f
        : (rangeIndex == 1 ? 6.0f
            : (rangeIndex == 2 ? 12.0f : 30.0f)));
    minDb = -rangeDb;
    maxDb = rangeDb;

    const int speedIndex = parameters.getRawParameterValue(ParamIDs::analyzerSpeed) != nullptr
        ? static_cast<int>(parameters.getRawParameterValue(ParamIDs::analyzerSpeed)->load())
        : 1;
    if (speedIndex != analyzerSpeedIndex)
    {
        analyzerSpeedIndex = speedIndex;
        const int hz = (speedIndex == 0 ? 15 : (speedIndex == 1 ? 30 : 60));
        startTimerHz(hz);
    }

    const bool freeze = parameters.getRawParameterValue(ParamIDs::analyzerFreeze) != nullptr
        && parameters.getRawParameterValue(ParamIDs::analyzerFreeze)->load() > 0.5f;

    if (! freeze)
    {
        updateFft();
        if (++frameCounter % 2 == 0)
            updateCurves();
    }
    repaint();
}

void AnalyzerComponent::updateFft()
{
    auto& preFifo = processorRef.getAnalyzerPreFifo();
    auto& postFifo = processorRef.getAnalyzerPostFifo();
    auto& extFifo = externalFifo;

    lastSampleRate = static_cast<float>(processorRef.getSampleRate());
    if (lastSampleRate <= 0.0f)
        lastSampleRate = 48000.0f;

    if (preFifo.pull(timeBuffer.data(), fftSize) == fftSize)
    {
        std::fill(fftDataPre.begin(), fftDataPre.end(), 0.0f);
        juce::FloatVectorOperations::copy(fftDataPre.data(), timeBuffer.data(), fftSize);
        window.multiplyWithWindowingTable(fftDataPre.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform(fftDataPre.data());

        for (int i = 0; i < fftBins; ++i)
        {
            const float mag = juce::Decibels::gainToDecibels(fftDataPre[static_cast<size_t>(i)],
                                                            minDb);
            preMagnitudes[i] = Smoothing::smooth(preMagnitudes[i], mag, kSmoothingCoeff);
        }
    }

    if (postFifo.pull(timeBuffer.data(), fftSize) == fftSize)
    {
        std::fill(fftDataPost.begin(), fftDataPost.end(), 0.0f);
        juce::FloatVectorOperations::copy(fftDataPost.data(), timeBuffer.data(), fftSize);
        window.multiplyWithWindowingTable(fftDataPost.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform(fftDataPost.data());

        for (int i = 0; i < fftBins; ++i)
        {
            const float mag = juce::Decibels::gainToDecibels(fftDataPost[static_cast<size_t>(i)],
                                                            minDb);
            postMagnitudes[i] = Smoothing::smooth(postMagnitudes[i], mag, kSmoothingCoeff);
        }
    }

    const bool showExternal = parameters.getRawParameterValue(ParamIDs::analyzerExternal) != nullptr
        && parameters.getRawParameterValue(ParamIDs::analyzerExternal)->load() > 0.5f;
    if (showExternal && extFifo.pull(timeBuffer.data(), fftSize) == fftSize)
    {
        std::fill(fftDataPost.begin(), fftDataPost.end(), 0.0f);
        juce::FloatVectorOperations::copy(fftDataPost.data(), timeBuffer.data(), fftSize);
        window.multiplyWithWindowingTable(fftDataPost.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform(fftDataPost.data());

        for (int i = 0; i < fftBins; ++i)
        {
            const float mag = juce::Decibels::gainToDecibels(fftDataPost[static_cast<size_t>(i)],
                                                            minDb);
            externalMagnitudes[i] = Smoothing::smooth(externalMagnitudes[i], mag, kSmoothingCoeff);
        }
    }
}

void AnalyzerComponent::updateCurves()
{
    const auto magnitudeArea = getMagnitudeArea();
    if (magnitudeArea.getWidth() <= 0)
        return;

    uint64_t hash = 1469598103934665603ull;
    auto hashValue = [&hash](float value)
    {
        const auto bits = static_cast<uint64_t>(std::hash<float>{}(value));
        hash ^= bits + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
    };
    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
    {
        hashValue(getBandParameter(band, kParamFreqSuffix));
        hashValue(getBandParameter(band, kParamGainSuffix));
        hashValue(getBandParameter(band, kParamQSuffix));
        hashValue(getBandParameter(band, kParamTypeSuffix));
        hashValue(getBandParameter(band, kParamBypassSuffix));
        hashValue(getBandParameter(band, kParamSlopeSuffix));
    }

    if (hash == lastCurveHash && selectedBand == lastCurveBand
        && selectedChannel == lastCurveChannel)
        return;

    lastCurveHash = hash;
    lastCurveBand = selectedBand;
    lastCurveChannel = selectedChannel;

    eqCurveDb.assign(static_cast<size_t>(magnitudeArea.getWidth()), 0.0f);
    selectedBandCurveDb.assign(static_cast<size_t>(magnitudeArea.getWidth()), 0.0f);
    if (showPhase)
        phaseCurve.assign(static_cast<size_t>(magnitudeArea.getWidth()), 0.0f);
    else
        phaseCurve.clear();

    const float maxFreq = std::min(20000.0f, lastSampleRate * 0.5f);
    double prevPhase = 0.0;
    bool hasPrev = false;
    for (int x = 0; x < magnitudeArea.getWidth(); ++x)
    {
        const float norm = static_cast<float>(x) / static_cast<float>(magnitudeArea.getWidth());
        const float freq = FFTUtils::normToFreq(norm, kMinFreq, maxFreq);

        std::complex<double> total = { 1.0, 0.0 };
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
            total *= computeBandResponse(band, freq);

        const auto bandResp = computeBandResponse(selectedBand, freq);
        const double mag = std::abs(total);
        double phase = std::arg(total);
        if (showPhase)
        {
            if (hasPrev)
            {
                double delta = phase - prevPhase;
                while (delta > juce::MathConstants<double>::pi)
                {
                    phase -= 2.0 * juce::MathConstants<double>::pi;
                    delta = phase - prevPhase;
                }
                while (delta < -juce::MathConstants<double>::pi)
                {
                    phase += 2.0 * juce::MathConstants<double>::pi;
                    delta = phase - prevPhase;
                }
            }
            prevPhase = phase;
            hasPrev = true;
        }
        eqCurveDb[static_cast<size_t>(x)] =
            juce::Decibels::gainToDecibels(static_cast<float>(mag), minDb);
        selectedBandCurveDb[static_cast<size_t>(x)] =
            juce::Decibels::gainToDecibels(static_cast<float>(std::abs(bandResp)), minDb);
        if (showPhase)
            phaseCurve[static_cast<size_t>(x)] = static_cast<float>(phase);
    }
}

void AnalyzerComponent::drawLabels(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    g.setColour(theme.textMuted);
    g.setFont(10.0f);

    const float maxFreq = std::min(20000.0f, lastSampleRate * 0.5f);
    const float freqs[] { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f,
                          5000.0f, 10000.0f, 20000.0f };
    for (float f : freqs)
    {
        if (f < kMinFreq || f > maxFreq)
            continue;
        const float x = frequencyToX(f);
        g.drawVerticalLine(static_cast<int>(x), static_cast<float>(area.getY()),
                           static_cast<float>(area.getBottom()));
        const juce::String text = f >= 1000.0f ? juce::String(f / 1000.0f, 1) + "k" : juce::String(f, 0);
        g.drawFittedText(text,
                         juce::Rectangle<int>(static_cast<int>(x + 2.0f), area.getBottom() - 14,
                                              40, 12),
                         juce::Justification::left, 1);
    }

    const float dbMarks[] { -24.0f, -12.0f, 0.0f, 6.0f };
    for (float db : dbMarks)
    {
        const int y = static_cast<int>(gainToY(db));
        g.drawHorizontalLine(y, static_cast<float>(area.getX()), static_cast<float>(area.getRight()));
        g.drawFittedText(juce::String(db, 0) + " dB",
                         juce::Rectangle<int>(area.getX() + 4, y - 8, 50, 12),
                         juce::Justification::left, 1);
    }
}

juce::Rectangle<int> AnalyzerComponent::getPlotArea() const
{
    return getLocalBounds().reduced(16, 20);
}

juce::Rectangle<int> AnalyzerComponent::getMagnitudeArea() const
{
    auto area = getPlotArea();
    const int phaseHeight = area.getHeight() / 4;
    return showPhase ? area.withTrimmedBottom(phaseHeight + 8) : area;
}

juce::Rectangle<int> AnalyzerComponent::getPhaseArea() const
{
    auto area = getPlotArea();
    const int phaseHeight = area.getHeight() / 4;
    return showPhase ? area.removeFromBottom(phaseHeight) : juce::Rectangle<int>();
}

float AnalyzerComponent::xToFrequency(float x) const
{
    const auto plotArea = getMagnitudeArea();
    const float maxFreq = std::min(20000.0f, lastSampleRate * 0.5f);
    const float norm = (x - plotArea.getX()) / static_cast<float>(plotArea.getWidth());
    return FFTUtils::normToFreq(norm, kMinFreq, maxFreq);
}

float AnalyzerComponent::yToGain(float y) const
{
    const auto plotArea = getMagnitudeArea();
    return juce::jmap(y,
                      static_cast<float>(plotArea.getBottom()),
                      static_cast<float>(plotArea.getY()),
                      minDb, maxDb);
}

float AnalyzerComponent::frequencyToX(float freq) const
{
    const auto plotArea = getMagnitudeArea();
    const float maxFreq = std::min(20000.0f, lastSampleRate * 0.5f);
    const float norm = FFTUtils::freqToNorm(freq, kMinFreq, maxFreq);
    return plotArea.getX() + norm * plotArea.getWidth();
}

float AnalyzerComponent::gainToY(float gainDb) const
{
    const auto plotArea = getMagnitudeArea();
    return juce::jmap(gainDb, minDb, maxDb,
                      static_cast<float>(plotArea.getBottom()),
                      static_cast<float>(plotArea.getY()));
}

float AnalyzerComponent::phaseToY(float phase) const
{
    const auto area = getPhaseArea();
    return juce::jmap(phase, kPhaseMin, kPhaseMax,
                      static_cast<float>(area.getBottom()),
                      static_cast<float>(area.getY()));
}

float AnalyzerComponent::snapFrequencyToPeak(float x) const
{
    const auto plotArea = getMagnitudeArea().toFloat();
    if (plotArea.getWidth() <= 0.0f)
        return xToFrequency(x);

    const float normalized = juce::jlimit(0.0f, 1.0f,
                                          (x - plotArea.getX()) / plotArea.getWidth());
    const int centerBin = juce::jlimit(0, fftBins - 1,
                                       static_cast<int>(std::round(normalized * (fftBins - 1))));
    const int search = 6;
    int bestBin = centerBin;
    float bestMag = preMagnitudes[static_cast<size_t>(centerBin)];
    for (int i = centerBin - search; i <= centerBin + search; ++i)
    {
        const int idx = juce::jlimit(0, fftBins - 1, i);
        const float mag = preMagnitudes[static_cast<size_t>(idx)];
        if (mag > bestMag)
        {
            bestMag = mag;
            bestBin = idx;
        }
    }

    const double freq = (static_cast<double>(bestBin) * lastSampleRate) / fftSize;
    return juce::jlimit<float>(kMinFreq,
                               static_cast<float>(lastSampleRate * 0.49),
                               static_cast<float>(freq));
}

void AnalyzerComponent::setBandParameter(int bandIndex, const juce::String& suffix, float value)
{
    auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, bandIndex, suffix));
    if (param == nullptr)
        return;

    param->setValueNotifyingHost(param->convertTo0to1(value));
}

float AnalyzerComponent::getBandParameter(int bandIndex, const juce::String& suffix) const
{
    if (auto* param = parameters.getRawParameterValue(
            ParamIDs::bandParamId(selectedChannel, bandIndex, suffix)))
        return param->load();

    return 0.0f;
}

bool AnalyzerComponent::getBandBypassed(int bandIndex) const
{
    return getBandParameter(bandIndex, kParamBypassSuffix) > 0.5f;
}

int AnalyzerComponent::getBandType(int bandIndex) const
{
    return static_cast<int>(getBandParameter(bandIndex, kParamTypeSuffix));
}

std::complex<double> AnalyzerComponent::computeBandResponse(int bandIndex, float frequency) const
{
    if (getBandBypassed(bandIndex))
        return { 1.0, 0.0 };

    const float gainDb = getBandParameter(bandIndex, kParamGainSuffix);
    const float q = std::max(0.1f, getBandParameter(bandIndex, kParamQSuffix));
    const float freq = getBandParameter(bandIndex, kParamFreqSuffix);
    const int type = getBandType(bandIndex);
    const float slopeDb = getBandParameter(bandIndex, kParamSlopeSuffix);
    const double sampleRate = std::max(1.0, processorRef.getSampleRate());

    const double nyquist = sampleRate * 0.5;
    const double clampedFreq = juce::jlimit(10.0, nyquist * 0.99, static_cast<double>(freq));
    const double omega = 2.0 * juce::MathConstants<double>::pi * clampedFreq / sampleRate;
    const double sinW = std::sin(omega);
    const double cosW = std::cos(omega);
    const double alpha = sinW / (2.0 * q);
    const double a = std::pow(10.0, gainDb / 40.0);

    auto computeResponseForType = [&](eqdsp::FilterType filterType,
                                      double gainDbForType,
                                      double qOverride)
    {
        const double qLocal = (qOverride > 0.0) ? qOverride : q;
        const double alphaLocal = sinW / (2.0 * qLocal);
        const double aLocal = std::pow(10.0, gainDbForType / 40.0);
        double b0 = 1.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a0 = 1.0;
        double a1 = 0.0;
        double a2 = 0.0;

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
            * juce::jlimit(10.0, nyquist * 0.99, static_cast<double>(frequency)) / sampleRate;
        const std::complex<double> z = std::exp(std::complex<double>(0.0, -w));
        const std::complex<double> z2 = z * z;
        const std::complex<double> numerator = b0 + b1 * z + b2 * z2;
        const std::complex<double> denominator = 1.0 + a1 * z + a2 * z2;
        return numerator / denominator;
    };

    std::complex<double> response;
    const auto filterType = static_cast<eqdsp::FilterType>(type);
    if (filterType == eqdsp::FilterType::tilt || filterType == eqdsp::FilterType::flatTilt)
    {
        const double qOverride = (filterType == eqdsp::FilterType::flatTilt) ? 0.5 : -1.0;
        const auto low = computeResponseForType(eqdsp::FilterType::lowShelf, gainDb * 0.5, qOverride);
        const auto high = computeResponseForType(eqdsp::FilterType::highShelf, -gainDb * 0.5, qOverride);
        response = low * high;
    }
    else
    {
        response = computeResponseForType(filterType, gainDb, -1.0);
    }

    if (filterType == eqdsp::FilterType::lowPass || filterType == eqdsp::FilterType::highPass)
    {
        auto onePoleResponse = [sampleRate, filterType](double cutoff, double freqHz)
        {
            const double clamped = juce::jlimit(10.0, sampleRate * 0.5 * 0.99, cutoff);
            const double a = std::exp(-2.0 * juce::MathConstants<double>::pi * clamped / sampleRate);
            const std::complex<double> z1 = std::exp(std::complex<double>(0.0,
                                                                          -2.0 * juce::MathConstants<double>::pi
                                                                              * freqHz / sampleRate));
            if (filterType == eqdsp::FilterType::lowPass)
                return (1.0 - a) / (1.0 - a * z1);

            return ((1.0 + a) * 0.5) * (1.0 - z1) / (1.0 - a * z1);
        };

        const float clamped = juce::jlimit(6.0f, 96.0f, slopeDb);
        const int stages = static_cast<int>(std::floor(clamped / 12.0f));
        const float remainder = clamped - static_cast<float>(stages) * 12.0f;
        const bool useOnePole = (remainder >= 6.0f) || stages == 0;
        if (stages > 0)
            response = std::pow(response, stages);
        if (useOnePole)
            response *= onePoleResponse(freq, frequency);
    }

    return response;
}
