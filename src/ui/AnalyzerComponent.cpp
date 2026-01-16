#include "AnalyzerComponent.h"
#include <complex>
#include <cmath>
#include <algorithm>
#include <functional>
#include "../PluginProcessor.h"
#include "../util/FFTUtils.h"
#include "../util/Smoothing.h"
#include "../util/ColorUtils.h"

namespace
{
constexpr float kMinFreq = 10.0f;
constexpr float kMaxDb = 60.0f;
constexpr float kMinDb = -60.0f;
constexpr float kAnalyzerMinDb = -60.0f;
constexpr float kAnalyzerMaxDb = 60.0f;
constexpr float kPointRadius = 6.5f;
constexpr float kHitRadius = 10.0f;
constexpr float kSmoothingCoeff = 0.2f;

const juce::String kParamFreqSuffix = "freq";
const juce::String kParamGainSuffix = "gain";
const juce::String kParamQSuffix = "q";
const juce::String kParamTypeSuffix = "type";
const juce::String kParamBypassSuffix = "bypass";
const juce::String kParamSlopeSuffix = "slope";
const juce::String kParamMsSuffix = "ms";
const juce::String kParamSoloSuffix = "solo";
const juce::String kParamMixSuffix = "mix";
const juce::String kParamDynEnableSuffix = "dynEnable";
const juce::String kParamDynModeSuffix = "dynMode";
const juce::String kParamDynThreshSuffix = "dynThresh";
const juce::String kParamDynAttackSuffix = "dynAttack";
const juce::String kParamDynReleaseSuffix = "dynRelease";
const juce::String kParamDynAutoSuffix = "dynAuto";
const juce::StringArray kFilterTypeLabels {
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
    lastTimerHz = 30;
    startTimerHz(30);
    perBandCurveHash.assign(ParamIDs::kBandsPerChannel, 0);
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


void AnalyzerComponent::setTheme(const ThemeColors& newTheme)
{
    theme = newTheme;
    repaint();
}

void AnalyzerComponent::setUiScale(float scale)
{
    uiScale = juce::jlimit(0.75f, 2.5f, scale);
    repaint();
}

void AnalyzerComponent::setInteractive(bool shouldAllow)
{
    allowInteraction = shouldAllow;
}

void AnalyzerComponent::paint(juce::Graphics& g)
{
    auto plotArea = getPlotArea();
    auto magnitudeArea = getMagnitudeArea();
    const float scale = uiScale;
    juce::ColourGradient grad(theme.analyzerBg, plotArea.getTopLeft().toFloat(),
                              theme.panel, plotArea.getBottomLeft().toFloat(), false);
    g.setGradientFill(grad);
    g.fillRect(plotArea);

    g.setColour(theme.panelOutline);
    g.drawRect(plotArea);

    g.setColour(theme.grid);


    drawLabels(g, magnitudeArea);

    const float maxFreq = getMaxFreq();

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

    bool hasPre = false;
    bool hasPost = false;
    for (int i = 0; i < fftBins; ++i)
    {
        if (preMagnitudes[i] > kAnalyzerMinDb + 2.0f)
            hasPre = true;
        if (postMagnitudes[i] > kAnalyzerMinDb + 2.0f)
            hasPost = true;
        if (hasPre && hasPost)
            break;
    }

    if (hasPre)
    {
        g.setColour(theme.accent.withAlpha(0.2f));
        g.strokePath(prePath, juce::PathStrokeType(3.0f * scale));
        g.setColour(theme.accent.withAlpha(0.75f));
        g.strokePath(prePath, juce::PathStrokeType(1.4f * scale));
    }

    if (hasPost)
    {
        g.setColour(theme.accentAlt.withAlpha(0.25f));
        g.strokePath(postPath, juce::PathStrokeType(3.0f * scale));
        g.setColour(theme.accentAlt.withAlpha(0.85f));
        g.strokePath(postPath, juce::PathStrokeType(1.5f * scale));
    }

    const bool showExternal = parameters.getRawParameterValue(ParamIDs::analyzerExternal) != nullptr
        && parameters.getRawParameterValue(ParamIDs::analyzerExternal)->load() > 0.5f;
    if (showExternal && hasPost)
    {
        juce::Path extPath;
        extPath.startNewSubPath(plotArea.getX(), gainToY(externalMagnitudes.front()));
        for (int x = 1; x < static_cast<int>(externalMagnitudes.size()); ++x)
            extPath.lineTo(plotArea.getX() + x,
                           gainToY(externalMagnitudes[static_cast<size_t>(x)]));
        g.setColour(theme.accentAlt.withAlpha(0.4f));
        g.strokePath(extPath, juce::PathStrokeType(1.0f * scale));
    }

    if (! eqCurveDb.empty())
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            if (band >= static_cast<int>(perBandCurveDb.size()))
                break;
            if (band >= static_cast<int>(perBandActive.size()) || ! perBandActive[static_cast<size_t>(band)])
                continue;

            const auto& curve = perBandCurveDb[static_cast<size_t>(band)];
            if (curve.empty())
                continue;

            juce::Path bandPath;
            bandPath.startNewSubPath(plotArea.getX(),
                                     gainToY(curve.front()));
            for (int x = 1; x < static_cast<int>(curve.size()); ++x)
                bandPath.lineTo(plotArea.getX() + x,
                                gainToY(curve[static_cast<size_t>(x)]));

            const auto bandColour = ColorUtils::bandColour(band);
            const bool isSelected = band == selectedBand;
            g.setColour(bandColour.withAlpha(isSelected ? 0.9f : 0.65f));
            g.strokePath(bandPath, juce::PathStrokeType((isSelected ? 2.0f : 1.6f) * scale));
        }

        juce::Path eqPath;
        eqPath.startNewSubPath(plotArea.getX(),
                               gainToY(eqCurveDb.front()));

        for (int x = 1; x < static_cast<int>(eqCurveDb.size()); ++x)
            eqPath.lineTo(plotArea.getX() + x,
                          gainToY(eqCurveDb[static_cast<size_t>(x)]));

        g.setColour(theme.text.withAlpha(0.25f));
        g.strokePath(eqPath, juce::PathStrokeType(3.2f * scale));
        g.setColour(theme.text.withAlpha(0.85f));
        g.strokePath(eqPath, juce::PathStrokeType(1.8f * scale));
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
    bypassIcons.clear();
    bypassIcons.reserve(ParamIDs::kBandsPerChannel);
    hasQHandles = false;

    std::vector<juce::Rectangle<float>> labelRects;
    labelRects.reserve(ParamIDs::kBandsPerChannel);
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

        const bool isSelected = std::find(selectedBands.begin(), selectedBands.end(), band)
            != selectedBands.end();
        const float radius = (isSelected ? kPointRadius + 2.5f : kPointRadius) * scale;
        g.setColour(colour.withAlpha(0.35f));
        g.fillEllipse(point.x - radius - 3.0f * scale, point.y - radius - 3.0f * scale,
                      (radius + 3.0f * scale) * 2.0f, (radius + 3.0f * scale) * 2.0f);
        g.setColour(colour);
        g.fillEllipse(point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f);

        if (isSelected)
        {
        g.setColour(theme.text.withAlpha(0.7f));
        g.drawEllipse(point.x - radius - 2.0f * scale, point.y - radius - 2.0f * scale,
                      (radius + 2.0f * scale) * 2.0f, (radius + 2.0f * scale) * 2.0f, 1.0f * scale);
        }

        if (getBandType(band) == static_cast<int>(eqdsp::FilterType::tilt)
            || getBandType(band) == static_cast<int>(eqdsp::FilterType::flatTilt))
        {
            g.setColour(theme.text.withAlpha(0.85f));
            g.drawLine(point.x - radius * 0.7f, point.y + radius * 0.7f,
                       point.x + radius * 0.7f, point.y - radius * 0.7f, 1.2f);
        }

        const float iconSize = 12.0f * uiScale;
        const juce::Rectangle<float> iconRect(point.x - iconSize * 0.5f,
                                              point.y + radius + 4.0f * uiScale,
                                              iconSize, iconSize);
        bypassIcons.push_back(iconRect);
        const auto iconColour = bypassed ? theme.textMuted.withAlpha(0.5f)
                                         : colour.withAlpha(0.9f);
        g.setColour(iconColour);
        g.drawEllipse(iconRect, 1.2f * uiScale);
        g.drawLine(iconRect.getCentreX(),
                   iconRect.getY() + 2.0f * uiScale,
                   iconRect.getCentreX(),
                   iconRect.getCentreY(),
                   1.2f * uiScale);

        if (band == selectedBand && ! bypassed)
        {
            const int type = getBandType(band);
            const bool supportsQ = type == static_cast<int>(eqdsp::FilterType::bell)
                || type == static_cast<int>(eqdsp::FilterType::notch)
                || type == static_cast<int>(eqdsp::FilterType::bandPass)
                || type == static_cast<int>(eqdsp::FilterType::lowShelf)
                || type == static_cast<int>(eqdsp::FilterType::highShelf);

            if (supportsQ)
            {
                const float q = std::max(0.11f, getBandParameter(band, kParamQSuffix));
                const float ratio = std::pow(2.0f, 1.0f / (2.0f * q));
                const float leftFreq = freq / ratio;
                const float rightFreq = freq * ratio;
                const float leftX = frequencyToX(leftFreq);
                const float rightX = frequencyToX(rightFreq);
                const float handleSize = 8.0f * uiScale;
                qHandleRects[0] = juce::Rectangle<float>(leftX - handleSize * 0.5f,
                                                         point.y - handleSize * 0.5f,
                                                         handleSize, handleSize);
                qHandleRects[1] = juce::Rectangle<float>(rightX - handleSize * 0.5f,
                                                         point.y - handleSize * 0.5f,
                                                         handleSize, handleSize);
                hasQHandles = true;
                g.setColour(colour.withAlpha(0.9f));
                g.fillEllipse(qHandleRects[0]);
                g.fillEllipse(qHandleRects[1]);
            }
        }

        if (isSelected && ! bypassed)
        {
            g.setColour(colour.withAlpha(0.9f));
            g.setFont(12.0f * scale);
            const float labelW = 20.0f * scale;
            const float labelH = 14.0f * scale;
            juce::Rectangle<float> labelRect(point.x + radius + 2.0f * scale,
                                             point.y - 7.0f * scale,
                                             labelW, labelH);
            const float offsetStep = labelH;
            const int offsets[] { 0, -1, 1, -2, 2, -3, 3 };
            for (int offset : offsets)
            {
                auto candidate = labelRect;
                candidate.setY(labelRect.getY() + offset * offsetStep);
                candidate.setY(juce::jlimit(static_cast<float>(plotArea.getY()),
                                            static_cast<float>(plotArea.getBottom()) - labelH,
                                            candidate.getY()));
                const bool overlaps = std::any_of(labelRects.begin(), labelRects.end(),
                                                  [&candidate](const juce::Rectangle<float>& other)
                                                  {
                                                      return candidate.intersects(other);
                                                  });
                if (! overlaps)
                {
                    labelRect = candidate;
                    break;
                }
            }
            labelRects.push_back(labelRect);
            g.drawFittedText(juce::String(band + 1),
                             labelRect.toNearestInt(),
                             juce::Justification::left, 1);
        }
    }

    if (hoverBand >= 0 && hoverBand < ParamIDs::kBandsPerChannel)
    {
        const float hoverFreq = getBandParameter(hoverBand, kParamFreqSuffix);
        const float hoverGain = getBandParameter(hoverBand, kParamGainSuffix);
        const float hoverQ = getBandParameter(hoverBand, kParamQSuffix);
        const int typeIndex = getBandType(hoverBand);
        const juce::String typeLabel = (typeIndex >= 0 && typeIndex < kFilterTypeLabels.size())
            ? kFilterTypeLabels[typeIndex]
            : "Filter";
        const juce::String text = typeLabel + "  "
            + (hoverFreq >= 1000.0f ? juce::String(hoverFreq / 1000.0f, 2) + "kHz"
                                    : juce::String(hoverFreq, 0) + "Hz")
            + "  " + juce::String(hoverGain, 1) + "dB"
            + "  Q " + juce::String(hoverQ, 2);

        const int pad = static_cast<int>(6 * uiScale);
        g.setFont(12.0f * uiScale);
        const int textW = static_cast<int>(g.getCurrentFont().getStringWidthFloat(text) + pad * 2);
        const int textH = static_cast<int>(18 * uiScale);
        juce::Rectangle<int> hudRect(static_cast<int>(hoverPos.x) + pad,
                                     static_cast<int>(hoverPos.y) - textH - pad,
                                     textW, textH);
        if (! getLocalBounds().contains(hudRect))
            hudRect.setPosition(static_cast<int>(hoverPos.x) - textW - pad,
                                static_cast<int>(hoverPos.y) - textH - pad);

        g.setColour(theme.panel.darker(0.2f).withAlpha(0.9f));
        g.fillRoundedRectangle(hudRect.toFloat(), 6.0f * uiScale);
        g.setColour(theme.panelOutline.withAlpha(0.8f));
        g.drawRoundedRectangle(hudRect.toFloat(), 6.0f * uiScale, 1.0f);
        g.setColour(theme.text);
        g.drawFittedText(text, hudRect, juce::Justification::centredLeft, 1);
    }
}

void AnalyzerComponent::resized()
{
    updateCurves();
}

void AnalyzerComponent::mouseDown(const juce::MouseEvent& event)
{
    if (! allowInteraction)
        return;
    draggingBand = -1;
    draggingQ = false;
    const auto plotArea = getMagnitudeArea().toFloat();
    if (! plotArea.contains(event.position))
        return;

    for (int i = 0; i < static_cast<int>(bypassIcons.size()); ++i)
    {
        if (bypassIcons[static_cast<size_t>(i)].contains(event.position))
        {
            const bool bypassed = getBandBypassed(i);
            setBandParameter(i, kParamBypassSuffix, bypassed ? 0.0f : 1.0f);
            setSelectedBand(i);
            if (onBandSelected)
                onBandSelected(i);
            repaint();
            return;
        }
    }

    float closest = kHitRadius * uiScale;
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

    if (event.mods.isAltDown() && event.mods.isLeftButtonDown())
    {
        if (closestBand >= 0)
        {
            resetBandToDefaults(closestBand, true);
            setSelectedBand(closestBand);
            if (onBandSelected)
                onBandSelected(closestBand);
            repaint();
            return;
        }
        startAltSolo(event.position);
        return;
    }

    if (closestBand >= 0)
    {
        if (hasQHandles)
        {
            for (int i = 0; i < 2; ++i)
            {
                if (qHandleRects[static_cast<size_t>(i)].contains(event.position))
                {
                    draggingQ = true;
                    qDragSide = i;
                    qDragStart = getBandParameter(selectedBand, kParamQSuffix);
                    return;
                }
            }
        }

        if (event.mods.isRightButtonDown())
        {
            juce::PopupMenu menu;
            menu.addItem("Reset to Default", [this, closestBand]()
            {
                resetBandToDefaults(closestBand, false);
                setSelectedBand(closestBand);
                if (onBandSelected)
                    onBandSelected(closestBand);
            });
            menu.addItem("Delete Band", [this, closestBand]()
            {
                resetBandToDefaults(closestBand, true);
                setSelectedBand(closestBand);
                if (onBandSelected)
                    onBandSelected(closestBand);
            });
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(*this));
            return;
        }

        if (event.mods.isAltDown() && event.mods.isLeftButtonDown())
        {
            tempSoloBand = closestBand;
            const auto soloId = ParamIDs::bandParamId(selectedChannel, tempSoloBand, kParamSoloSuffix);
            if (auto* soloParam = parameters.getParameter(soloId))
            {
                tempSoloWasEnabled = soloParam->getValue() > 0.5f;
                soloParam->setValueNotifyingHost(1.0f);
            }
        }

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
    else if (event.mods.isLeftButtonDown())
    {
        createBandAtPosition(event.position);
    }
}

void AnalyzerComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (! allowInteraction)
        return;
    if (isAltSoloing)
    {
        updateAltSolo(event.position);
        return;
    }

    if (draggingQ)
    {
        const float centerFreq = getBandParameter(selectedBand, kParamFreqSuffix);
        const float sideFreq = xToFrequency(event.position.x);
        const float ratio = (qDragSide == 0)
            ? (centerFreq / std::max(20.0f, sideFreq))
            : (std::max(sideFreq, 20.0f) / centerFreq);
        const float safeRatio = juce::jlimit(1.001f, 64.0f, ratio);
        const float qValue = 1.0f / (safeRatio - 1.0f / safeRatio);
        setBandParameter(selectedBand, kParamQSuffix, juce::jlimit(0.1f, 18.0f, qValue));
        repaint();
        return;
    }

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
    if (! allowInteraction)
        return;
    juce::ignoreUnused(event);
    if (isAltSoloing)
        stopAltSolo();
    draggingQ = false;
    if (tempSoloBand >= 0)
    {
        const auto soloId = ParamIDs::bandParamId(selectedChannel, tempSoloBand, kParamSoloSuffix);
        if (auto* soloParam = parameters.getParameter(soloId))
            soloParam->setValueNotifyingHost(tempSoloWasEnabled ? 1.0f : 0.0f);
        tempSoloBand = -1;
        tempSoloWasEnabled = false;
    }
    draggingBand = -1;
    dragBands.clear();
}

void AnalyzerComponent::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (! allowInteraction)
        return;
    const auto plotArea = getMagnitudeArea().toFloat();
    if (! plotArea.contains(event.position))
        return;
    createBandAtPosition(event.position);
}

void AnalyzerComponent::mouseMove(const juce::MouseEvent& event)
{
    hoverPos = event.position;
    const auto plotArea = getMagnitudeArea().toFloat();
    if (! plotArea.contains(event.position))
    {
        hoverBand = -1;
        repaint();
        return;
    }

    float closest = kHitRadius * uiScale;
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
    hoverBand = closestBand;
    repaint();
}

void AnalyzerComponent::mouseExit(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    hoverBand = -1;
    repaint();
}

void AnalyzerComponent::mouseWheelMove(const juce::MouseEvent& event,
                                       const juce::MouseWheelDetails& wheel)
{
    if (! allowInteraction)
        return;
    juce::ignoreUnused(event);
    const float delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
    if (delta == 0.0f)
        return;

    const auto qId = ParamIDs::bandParamId(selectedChannel, selectedBand, kParamQSuffix);
    if (auto* qParam = parameters.getParameter(qId))
    {
        const float current = qParam->getValue();
        const float step = 0.04f;
        qParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, current + delta * step));
    }
}

void AnalyzerComponent::createBandAtPosition(const juce::Point<float>& position)
{
    const float freq = xToFrequency(position.x);
    const float gain = yToGain(position.y);

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

    setBandParameter(targetBand, kParamFreqSuffix, freq);
    setBandParameter(targetBand, kParamGainSuffix, gain);
    setBandParameter(targetBand, kParamBypassSuffix, 0.0f);
    setBandParameter(targetBand, kParamMixSuffix, 100.0f);
    setBandParameter(targetBand, kParamSoloSuffix, 0.0f);
    setSelectedBand(targetBand);
    if (onBandSelected)
        onBandSelected(targetBand);
}

void AnalyzerComponent::resetBandToDefaults(int bandIndex, bool shouldBypass)
{
    auto resetParam = [this, bandIndex](const juce::String& suffix)
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, bandIndex, suffix)))
            param->setValueNotifyingHost(param->getDefaultValue());
    };

    resetParam(kParamFreqSuffix);
    resetParam(kParamGainSuffix);
    resetParam(kParamQSuffix);
    resetParam(kParamTypeSuffix);
    resetParam(kParamMsSuffix);
    resetParam(kParamSlopeSuffix);
    resetParam(kParamSoloSuffix);
    resetParam(kParamMixSuffix);
    resetParam(kParamDynEnableSuffix);
    resetParam(kParamDynModeSuffix);
    resetParam(kParamDynThreshSuffix);
    resetParam(kParamDynAttackSuffix);
    resetParam(kParamDynReleaseSuffix);
    resetParam(kParamDynAutoSuffix);

    if (auto* bypassParam = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, bandIndex, kParamBypassSuffix)))
        bypassParam->setValueNotifyingHost(shouldBypass ? 1.0f : 0.0f);
}

void AnalyzerComponent::startAltSolo(const juce::Point<float>& position)
{
    if (isAltSoloing)
        return;

    altSoloBand = selectedBand;
    auto storeParam = [this](const juce::String& suffix, float& dest)
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, altSoloBand, suffix)))
            dest = param->getValue();
    };
    storeParam(kParamFreqSuffix, altSoloState.freqNorm);
    storeParam(kParamGainSuffix, altSoloState.gainNorm);
    storeParam(kParamQSuffix, altSoloState.qNorm);
    storeParam(kParamTypeSuffix, altSoloState.typeNorm);
    storeParam(kParamBypassSuffix, altSoloState.bypassNorm);
    storeParam(kParamSoloSuffix, altSoloState.soloNorm);

    auto setParamValue = [this](const juce::String& suffix, float value)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*>(
                parameters.getParameter(ParamIDs::bandParamId(selectedChannel, altSoloBand, suffix))))
        {
            param->setValueNotifyingHost(param->convertTo0to1(value));
        }
    };

    const float freq = xToFrequency(position.x);
    setParamValue(kParamFreqSuffix, freq);
    setParamValue(kParamGainSuffix, 0.0f);
    setParamValue(kParamQSuffix, 6.0f);
    setParamValue(kParamTypeSuffix, 6.0f);
    setParamValue(kParamBypassSuffix, 0.0f);
    setParamValue(kParamSoloSuffix, 1.0f);
    isAltSoloing = true;
}

void AnalyzerComponent::updateAltSolo(const juce::Point<float>& position)
{
    if (! isAltSoloing)
        return;

    const float freq = xToFrequency(position.x);
    if (auto* param = dynamic_cast<juce::RangedAudioParameter*>(
            parameters.getParameter(ParamIDs::bandParamId(selectedChannel, altSoloBand, kParamFreqSuffix))))
    {
        param->setValueNotifyingHost(param->convertTo0to1(freq));
    }
}

void AnalyzerComponent::stopAltSolo()
{
    if (! isAltSoloing)
        return;

    auto restoreParam = [this](const juce::String& suffix, float value)
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, altSoloBand, suffix)))
            param->setValueNotifyingHost(value);
    };
    restoreParam(kParamFreqSuffix, altSoloState.freqNorm);
    restoreParam(kParamGainSuffix, altSoloState.gainNorm);
    restoreParam(kParamQSuffix, altSoloState.qNorm);
    restoreParam(kParamTypeSuffix, altSoloState.typeNorm);
    restoreParam(kParamBypassSuffix, altSoloState.bypassNorm);
    restoreParam(kParamSoloSuffix, altSoloState.soloNorm);

    isAltSoloing = false;
    altSoloBand = -1;
}

void AnalyzerComponent::timerCallback()
{
    if (! isShowing())
        return;

    minDb = kMinDb;
    maxDb = kMaxDb;

    const int speedIndex = parameters.getRawParameterValue(ParamIDs::analyzerSpeed) != nullptr
        ? static_cast<int>(parameters.getRawParameterValue(ParamIDs::analyzerSpeed)->load())
        : 1;
    if (speedIndex != analyzerSpeedIndex)
    {
        analyzerSpeedIndex = speedIndex;
    }

    const float sr = static_cast<float>(processorRef.getSampleRate());
    const float effectiveSr = sr > 0.0f ? sr : lastSampleRate;
    int hz = (analyzerSpeedIndex == 0 ? 15 : (analyzerSpeedIndex == 1 ? 30 : 60));
    if (effectiveSr >= 192000.0f)
        hz = juce::jmax(10, hz / 2);
    if (effectiveSr >= 384000.0f)
        hz = juce::jmax(10, hz / 3);
    if (hz != lastTimerHz)
    {
        lastTimerHz = hz;
        startTimerHz(hz);
    }

    const bool freeze = parameters.getRawParameterValue(ParamIDs::analyzerFreeze) != nullptr
        && parameters.getRawParameterValue(ParamIDs::analyzerFreeze)->load() > 0.5f;

    bool didUpdate = false;
    if (! freeze)
    {
        updateFft();
        didUpdate = true;
        if (++frameCounter % 2 == 0)
        {
            updateCurves();
            didUpdate = true;
        }
    }
    if (didUpdate)
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

    if (lastCurveWidth != magnitudeArea.getWidth())
    {
        lastCurveWidth = magnitudeArea.getWidth();
        perBandCurveDb.assign(ParamIDs::kBandsPerChannel,
                              std::vector<float>(static_cast<size_t>(lastCurveWidth), 0.0f));
        perBandCurveHash.assign(ParamIDs::kBandsPerChannel, 0);
    }

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
        hashValue(getBandParameter(band, kParamMixSuffix));
    }

    if (hash == lastCurveHash && selectedBand == lastCurveBand
        && selectedChannel == lastCurveChannel)
        return;

    lastCurveHash = hash;
    lastCurveBand = selectedBand;
    lastCurveChannel = selectedChannel;

    const int width = magnitudeArea.getWidth();
    eqCurveDb.assign(static_cast<size_t>(width), 0.0f);
    selectedBandCurveDb.assign(static_cast<size_t>(width), 0.0f);
    perBandActive.assign(ParamIDs::kBandsPerChannel, false);
    // Phase view removed.

    const float maxFreq = getMaxFreq();
    std::array<bool, ParamIDs::kBandsPerChannel> bandActive {};
    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
    {
        const float mix = getBandParameter(band, kParamMixSuffix) / 100.0f;
        bandActive[band] = ! getBandBypassed(band) && mix > 0.0005f;
        perBandActive[band] = bandActive[band];
    }

    std::vector<bool> bandDirty(ParamIDs::kBandsPerChannel, false);
    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
    {
        uint64_t bandHash = 1469598103934665603ull;
        auto hashBand = [&bandHash](float value)
        {
            const auto bits = static_cast<uint64_t>(std::hash<float>{}(value));
            bandHash ^= bits + 0x9e3779b97f4a7c15ull + (bandHash << 6) + (bandHash >> 2);
        };
        hashBand(getBandParameter(band, kParamFreqSuffix));
        hashBand(getBandParameter(band, kParamGainSuffix));
        hashBand(getBandParameter(band, kParamQSuffix));
        hashBand(getBandParameter(band, kParamTypeSuffix));
        hashBand(getBandParameter(band, kParamBypassSuffix));
        hashBand(getBandParameter(band, kParamSlopeSuffix));
        hashBand(getBandParameter(band, kParamMixSuffix));
        bandDirty[static_cast<size_t>(band)] = bandHash != perBandCurveHash[static_cast<size_t>(band)];
        perBandCurveHash[static_cast<size_t>(band)] = bandHash;
    }

    for (int x = 0; x < width; ++x)
    {
        const float norm = static_cast<float>(x) / static_cast<float>(width);
        const float freq = FFTUtils::normToFreq(norm, kMinFreq, maxFreq);

        std::complex<double> total = { 1.0, 0.0 };
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            std::complex<double> response { 1.0, 0.0 };
            if (bandActive[band])
            {
                if (bandDirty[static_cast<size_t>(band)])
                {
                    response = computeBandResponse(band, freq);
                    perBandCurveDb[static_cast<size_t>(band)][static_cast<size_t>(x)] =
                        juce::Decibels::gainToDecibels(static_cast<float>(std::abs(response)), minDb);
                }
                else
                {
                    const float db = perBandCurveDb[static_cast<size_t>(band)][static_cast<size_t>(x)];
                    response = std::complex<double>(juce::Decibels::decibelsToGain(db), 0.0);
                }
            }
            else
            {
                perBandCurveDb[static_cast<size_t>(band)][static_cast<size_t>(x)] = minDb;
            }

            total *= response;
        }

        const auto bandResp = computeBandResponse(selectedBand, freq);
        const double mag = std::abs(total);
        eqCurveDb[static_cast<size_t>(x)] =
            juce::Decibels::gainToDecibels(static_cast<float>(mag), minDb);
        selectedBandCurveDb[static_cast<size_t>(x)] =
            juce::Decibels::gainToDecibels(static_cast<float>(std::abs(bandResp)), minDb);
    }
}

void AnalyzerComponent::drawLabels(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    g.setColour(theme.textMuted);
    const float scale = uiScale;
    g.setFont(12.5f * scale);

    const int leftGutter = static_cast<int>(44 * scale);
    const int rightGutter = static_cast<int>(44 * scale);
    const int bottomGutter = static_cast<int>(18 * scale);
    const auto labelArea = area.withTrimmedLeft(leftGutter).withTrimmedRight(rightGutter)
        .withTrimmedBottom(bottomGutter);

    const float spectrumMinDb = kAnalyzerMinDb;
    const float spectrumMaxDb = kAnalyzerMaxDb;
    const float spectrumStep = 6.0f;
    for (float db = spectrumMinDb; db <= spectrumMaxDb + 0.01f; db += spectrumStep)
    {
        const float y = juce::jmap(db, spectrumMinDb, spectrumMaxDb,
                                   static_cast<float>(labelArea.getBottom()),
                                   static_cast<float>(labelArea.getY()));
        const bool major = (static_cast<int>(db) % 12 == 0);
        g.setColour(theme.grid.withAlpha(major ? 0.7f : 0.45f));
        g.drawLine(static_cast<float>(area.getX()), y,
                   static_cast<float>(area.getRight()), y, major ? 1.4f : 1.0f);
        if (major)
        {
            g.setColour(theme.textMuted.withAlpha(0.8f));
            g.drawFittedText(juce::String(db, 0),
                             juce::Rectangle<int>(static_cast<int>(area.getX() + 2 * scale),
                                                  static_cast<int>(y - 7 * scale),
                                                  static_cast<int>(36 * scale),
                                                  static_cast<int>(14 * scale)),
                             juce::Justification::left, 1);
        }
    }

    const float maxFreq = getMaxFreq();
    const float freqs[] { 10.0f, 12.5f, 16.0f, 20.0f, 25.0f, 31.5f, 40.0f, 50.0f, 63.0f, 80.0f, 100.0f, 125.0f,
                          160.0f, 200.0f, 250.0f, 315.0f, 400.0f, 500.0f, 630.0f, 800.0f, 1000.0f, 1250.0f, 1600.0f,
                          2000.0f, 2500.0f, 3150.0f, 4000.0f, 5000.0f, 6300.0f, 8000.0f, 10000.0f, 12500.0f, 16000.0f,
                          20000.0f, 25000.0f, 31500.0f, 40000.0f, 50000.0f, 60000.0f };
    float lastLabelX = -1.0e6f;
    const float minLabelSpacing = 28.0f * scale;
    const int labelWidth = static_cast<int>(42 * scale);
    const int labelHeight = static_cast<int>(14 * scale);
    for (float f : freqs)
    {
        if (f < kMinFreq || f > maxFreq)
            continue;
        const float x = frequencyToX(f);
        const bool major = (f == 10.0f || f == 20.0f || f == 50.0f || f == 100.0f || f == 200.0f || f == 500.0f
            || f == 1000.0f || f == 2000.0f || f == 5000.0f || f == 10000.0f || f == 20000.0f || f == 40000.0f
            || f == 60000.0f);
        g.setColour(major ? theme.grid.withAlpha(0.8f) : theme.grid.withAlpha(0.45f));
        g.drawVerticalLine(static_cast<int>(x), static_cast<float>(area.getY()),
                           static_cast<float>(area.getBottom()));
        if (major && x + labelWidth <= area.getRight() && (x - lastLabelX) >= minLabelSpacing)
        {
            lastLabelX = x;
            g.setColour(theme.textMuted);
            const juce::String text = f >= 1000.0f
                ? juce::String(f / 1000.0f, (f >= 10000.0f ? 1 : 2)) + "k"
                : juce::String(f, f < 100.0f ? 1 : 0);
            g.drawFittedText(text,
                             juce::Rectangle<int>(static_cast<int>(x + 3.0f * scale),
                                                  static_cast<int>(area.getBottom() - bottomGutter),
                                                  labelWidth,
                                                  labelHeight),
                             juce::Justification::left, 1);
        }
    }

    const float db = 0.0f;
    const int y = static_cast<int>(gainToY(db));
    g.setColour(theme.grid.withAlpha(0.6f));
    g.drawHorizontalLine(y, static_cast<float>(area.getX()), static_cast<float>(area.getRight()));
    g.setColour(theme.textMuted);
    g.drawFittedText("0 dB",
                     juce::Rectangle<int>(static_cast<int>(area.getRight() - rightGutter + 4 * scale),
                                          static_cast<int>(y - 9 * scale),
                                          static_cast<int>(rightGutter - 6 * scale),
                                          static_cast<int>(14 * scale)),
                     juce::Justification::left, 1);

    g.setColour(theme.textMuted.withAlpha(0.6f));
    g.drawFittedText("EQ",
                     juce::Rectangle<int>(area.getRight() - static_cast<int>(rightGutter),
                                          area.getY() + static_cast<int>(2 * scale),
                                          static_cast<int>(rightGutter - 4 * scale),
                                          static_cast<int>(12 * scale)),
                     juce::Justification::right, 1);
    g.drawFittedText("SP",
                     juce::Rectangle<int>(area.getX() + static_cast<int>(2 * scale),
                                          area.getY() + static_cast<int>(2 * scale),
                                          static_cast<int>(leftGutter - 4 * scale),
                                          static_cast<int>(12 * scale)),
                     juce::Justification::left, 1);
}

juce::Rectangle<int> AnalyzerComponent::getPlotArea() const
{
    const int marginX = static_cast<int>(16 * uiScale);
    const int marginY = static_cast<int>(20 * uiScale);
    return getLocalBounds().reduced(marginX, marginY);
}

juce::Rectangle<int> AnalyzerComponent::getMagnitudeArea() const
{
    auto area = getPlotArea();
    return area;
}

float AnalyzerComponent::xToFrequency(float x) const
{
    const auto plotArea = getMagnitudeArea();
    const float maxFreq = getMaxFreq();
    const float norm = (x - plotArea.getX()) / static_cast<float>(plotArea.getWidth());
    return FFTUtils::normToFreq(norm, kMinFreq, maxFreq);
}

float AnalyzerComponent::yToGain(float y) const
{
    const auto plotArea = getMagnitudeArea();
    const float mapped = juce::jmap(y,
                                    static_cast<float>(plotArea.getBottom()),
                                    static_cast<float>(plotArea.getY()),
                                    minDb, maxDb);
    return juce::jlimit(-48.0f, 48.0f, mapped);
}

float AnalyzerComponent::frequencyToX(float freq) const
{
    const auto plotArea = getMagnitudeArea();
    const float maxFreq = getMaxFreq();
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

float AnalyzerComponent::getMaxFreq() const
{
    return std::max(kMinFreq * 1.1f, std::min(60000.0f, lastSampleRate * 0.5f));
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
    const float mix = getBandParameter(bandIndex, kParamMixSuffix) / 100.0f;
    if (mix <= 0.0001f)
        return { 1.0, 0.0 };
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

    if (mix < 1.0f)
    {
        const double wet = mix;
        response = (1.0 - wet) + response * wet;
    }

    return response;
}
