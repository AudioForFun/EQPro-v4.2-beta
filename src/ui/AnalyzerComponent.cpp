#include "AnalyzerComponent.h"
#include <complex>
#include <cmath>
#include <algorithm>
#include <functional>
#include "../PluginProcessor.h"
#include "../util/FFTUtils.h"
#include "../util/Smoothing.h"
#include "../util/ColorUtils.h"

// FFT display + EQ curve rendering + interactive band editing.

namespace
{
// Lower bound so the curve renders across the full spectrum (no low-end gap).
constexpr float kMinFreq = 5.0f;
constexpr float kMaxDb = 60.0f;
constexpr float kMinDb = -60.0f;
constexpr float kAnalyzerMinDb = -60.0f;
constexpr float kAnalyzerMaxDb = 60.0f;
constexpr float kPointRadius = 6.5f;
constexpr float kHitRadius = 4.0f;
// v4.4 beta: Faster smoothing for more reactive analyzer (was 0.2f, now 0.3f for quicker response)
constexpr float kSmoothingCoeff = 0.3f;

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
      harmonicFifo(processor.getAnalyzerHarmonicFifo()),  // v4.5 beta: FIFO for harmonic-only curve (red)
      fft(fftOrder),
      window(fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    preMagnitudes.fill(kAnalyzerMinDb);
    postMagnitudes.fill(kAnalyzerMinDb);
    harmonicMagnitudes.fill(kAnalyzerMinDb);  // v4.5 beta: Magnitudes for harmonic-only curve (red)
    externalMagnitudes.fill(kAnalyzerMinDb);
    selectedBands.push_back(selectedBand);
    lastTimerHz = 30;
    // v4.4 beta: Defer timer start - will start after first resize
    // This prevents expensive FFT updates before component is properly laid out
    hasBeenResized = false;
    perBandCurveHash.assign(ParamIDs::kBandsPerChannel, 0);
    // v4.4 beta: Use buffered rendering for better performance on initial load
    // Reduces repaint overhead and ensures controls appear immediately
    setBufferedToImage(true);
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

void AnalyzerComponent::invalidateCaches()
{
    lastCurveWidth = 0;
    lastCurveHash = 0;
    lastCurveBand = -1;
    lastCurveChannel = -1;
    perBandCurveHash.assign(ParamIDs::kBandsPerChannel, 0);
    eqCurveDb.clear();
    selectedBandCurveDb.clear();
    perBandCurveDb.clear();
    perBandActive.clear();
}

void AnalyzerComponent::paint(juce::Graphics& g)
{
    auto plotArea = getPlotArea();
    auto magnitudeArea = getMagnitudeArea();
    const float scale = uiScale;
    const float cornerRadius = 8.0f * scale;
    
    // v4.4 beta: Enhanced beautiful gradient background for depth and polish
    // More pronounced gradient for better visual appeal
    juce::ColourGradient bgGradient(theme.analyzerBg.brighter(0.04f), plotArea.toFloat().getTopLeft(),
                                    theme.analyzerBg.darker(0.06f), plotArea.toFloat().getBottomLeft(), false);
    g.setGradientFill(bgGradient);
    g.fillRoundedRectangle(plotArea.toFloat(), cornerRadius);
    
    // Enhanced inner glow for more beautiful, modern look
    g.setColour(theme.accent.withAlpha(0.05f));
    g.fillRoundedRectangle(plotArea.toFloat().reduced(2.0f), cornerRadius - 2.0f);

    // Modern border with layered outlines for depth and polish.
    g.setColour(theme.panelOutline.withAlpha(0.8f));
    g.drawRoundedRectangle(plotArea.toFloat(), cornerRadius, 1.2f);
    g.setColour(theme.panelOutline.withAlpha(0.4f));
    g.drawRoundedRectangle(plotArea.toFloat().reduced(1.0f), cornerRadius - 1.0f, 1.0f);
    g.setColour(theme.accent.withAlpha(0.15f));
    g.drawRoundedRectangle(plotArea.toFloat().reduced(2.0f), cornerRadius - 2.0f, 0.8f);

    g.saveState();
    g.reduceClipRegion(plotArea);

    // Draw grid lines early (background elements).
    drawGridLines(g, magnitudeArea);

    const float maxFreq = getMaxFreq();

    // v4.4 beta: Smoother, more beautiful curves using quadratic interpolation
    juce::Path prePath;
    juce::Path postPath;
    bool started = false;
    float prevPreX = 0.0f, prevPreY = 0.0f;
    float prevPostX = 0.0f, prevPostY = 0.0f;

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
            prevPreX = x;
            prevPreY = preY;
            prevPostX = x;
            prevPostY = postY;
            started = true;
        }
        else
        {
            // v4.4 beta: Use quadratic curves for smoother, more beautiful lines
            const float midX = (prevPreX + x) * 0.5f;
            const float midPreY = (prevPreY + preY) * 0.5f;
            const float midPostY = (prevPostY + postY) * 0.5f;
            prePath.quadraticTo(midX, midPreY, x, preY);
            postPath.quadraticTo(midX, midPostY, x, postY);
            prevPreX = x;
            prevPreY = preY;
            prevPostX = x;
            prevPostY = postY;
        }
    }

    const int viewIndex = parameters.getRawParameterValue(ParamIDs::analyzerView) != nullptr
        ? static_cast<int>(parameters.getRawParameterValue(ParamIDs::analyzerView)->load())
        : 0;
    const bool drawPre = viewIndex != 2;
    const bool drawPost = viewIndex != 1;

    // Option 1: Classic Pro-Q Style - Two different grey tones for pre/post curves.
    // Pre-EQ: Light grey (medium brightness, clearly visible).
    juce::Colour preColour = juce::Colour(0xffC0C0C0);  // Light grey #C0C0C0
    // Post-EQ: Darker grey (darker, still distinct).
    juce::Colour postColour = juce::Colour(0xff808080);  // Darker grey #808080

    // Classic Pro-Q style: Clean, thin lines with subtle fill, no glow effects.
    if (drawPre && !prePath.isEmpty())
    {
        // v4.4 beta: Beautiful gradient fill under curve for depth
        juce::Path fillPath = prePath;
        fillPath.lineTo(plotArea.getRight(), magnitudeArea.getBottom());
        fillPath.lineTo(plotArea.getX(), magnitudeArea.getBottom());
        fillPath.closeSubPath();
        
        // Gradient fill from top (brighter) to bottom (darker) for beautiful depth
        juce::ColourGradient fillGradient(
            preColour.withAlpha(0.12f), fillPath.getBounds().getTopLeft(),
            preColour.withAlpha(0.04f), fillPath.getBounds().getBottomLeft(), false);
        g.setGradientFill(fillGradient);
        g.fillPath(fillPath);
        
        // Enhanced line with subtle glow for beautiful appearance
        g.setColour(preColour.withAlpha(0.95f));
        g.strokePath(prePath, juce::PathStrokeType(2.0f * scale));
        // Subtle outer glow
        g.setColour(preColour.withAlpha(0.15f));
        g.strokePath(prePath, juce::PathStrokeType(3.5f * scale));
    }

    if (drawPost && !postPath.isEmpty())
    {
        // v4.4 beta: Beautiful gradient fill under curve for depth
        juce::Path fillPath = postPath;
        fillPath.lineTo(plotArea.getRight(), magnitudeArea.getBottom());
        fillPath.lineTo(plotArea.getX(), magnitudeArea.getBottom());
        fillPath.closeSubPath();
        
        // Gradient fill from top (brighter) to bottom (darker) for beautiful depth
        juce::ColourGradient fillGradient(
            postColour.withAlpha(0.15f), fillPath.getBounds().getTopLeft(),
            postColour.withAlpha(0.05f), fillPath.getBounds().getBottomLeft(), false);
        g.setGradientFill(fillGradient);
        g.fillPath(fillPath);
        
        // Enhanced line with subtle glow for beautiful appearance
        g.setColour(postColour.withAlpha(0.95f));
        g.strokePath(postPath, juce::PathStrokeType(2.0f * scale));
        // Subtle outer glow
        g.setColour(postColour.withAlpha(0.15f));
        g.strokePath(postPath, juce::PathStrokeType(3.5f * scale));
    }

    // v4.5 beta: Draw harmonic curve (harmonic-only content) in RED
    // This third analyzer curve provides visual feedback for the harmonic processing layer.
    // It displays only the harmonic contribution (no dry/program signal), so changes are clearer.
    //
    // Visual Design:
    // - Bright red color (0xffff4444) for clear distinction from grey pre/post curves
    // - Same rendering style as pre/post curves: gradient fill + enhanced line with subtle glow
    // - Uses quadratic interpolation for smooth, beautiful curves
    // - Automatically appears when harmonics are active, disappears when all harmonics are bypassed
    //
    // Visibility Logic:
    // - Checks all channels and bands to determine if any harmonic processing is active
    // - Only draws the curve if at least one band has active harmonics (same logic as updateFft)
    bool hasActiveHarmonics = false;
    for (int ch = 0; ch < ParamIDs::kMaxChannels && !hasActiveHarmonics; ++ch)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            const auto oddParam = parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, "odd"));
            const auto evenParam = parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, "even"));
            const auto mixOddParam = parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, "mixOdd"));
            const auto mixEvenParam = parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, "mixEven"));
            const auto bypassParam = parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, "harmonicBypass"));
            
            if (bypassParam != nullptr && bypassParam->load() > 0.5f)
                continue;  // Bypassed - skip this band
            
            const float odd = oddParam != nullptr ? oddParam->load() : 0.0f;
            const float even = evenParam != nullptr ? evenParam->load() : 0.0f;
            const float mixOdd = mixOddParam != nullptr ? (mixOddParam->load() / 100.0f) : 0.0f;
            const float mixEven = mixEvenParam != nullptr ? (mixEvenParam->load() / 100.0f) : 0.0f;
            
            // Check if this band has active harmonics (non-zero amount with mix > 0)
            if ((std::abs(odd) > 0.001f && mixOdd > 0.001f) || (std::abs(even) > 0.001f && mixEven > 0.001f))
            {
                hasActiveHarmonics = true;
                break;
            }
        }
    }
    
    if (hasActiveHarmonics)
    {
        // Build harmonic path using same frequency mapping as pre/post curves
        // This ensures the harmonic curve aligns perfectly with the frequency axis
        juce::Path harmonicPath;
        bool harmonicStarted = false;
        float prevHarmonicX = 0.0f, prevHarmonicY = 0.0f;
        
        // Iterate through FFT bins and map to screen coordinates
        for (int bin = 1; bin < fftBins; ++bin)
        {
            // Calculate frequency for this bin
            const float freq = (lastSampleRate * bin) / static_cast<float>(fftSize);
            if (freq < kMinFreq || freq > maxFreq)
                continue;  // Skip frequencies outside display range

            // Map frequency to normalized position (logarithmic scale)
            const float xNorm = FFTUtils::freqToNorm(freq, kMinFreq, maxFreq);
            const float x = plotArea.getX() + xNorm * plotArea.getWidth();
            // Map magnitude (dB) to screen Y coordinate
            const float harmonicDb = harmonicMagnitudes[bin];
            const float harmonicY = juce::jmap(harmonicDb, kAnalyzerMinDb, kAnalyzerMaxDb,
                                               static_cast<float>(magnitudeArea.getBottom()),
                                               static_cast<float>(magnitudeArea.getY()));

            if (!harmonicStarted)
            {
                // Start new path at first valid point
                harmonicPath.startNewSubPath(x, harmonicY);
                prevHarmonicX = x;
                prevHarmonicY = harmonicY;
                harmonicStarted = true;
            }
            else
            {
                // Use quadratic curves for smoother, more beautiful lines (same as pre/post curves)
                // This creates smooth transitions between points instead of sharp linear segments
                const float midX = (prevHarmonicX + x) * 0.5f;
                const float midHarmonicY = (prevHarmonicY + harmonicY) * 0.5f;
                harmonicPath.quadraticTo(midX, midHarmonicY, x, harmonicY);
                prevHarmonicX = x;
                prevHarmonicY = harmonicY;
            }
        }
        
        if (!harmonicPath.isEmpty())
        {
            // Red color for harmonic curve - distinct from grey pre/post curves
            // Bright red (0xffff4444) provides excellent visibility and clear distinction
            const juce::Colour harmonicColour = juce::Colour(0xffff4444);
            
            // Create gradient fill under curve for visual depth
            // Fill extends from curve to bottom of plot area
            juce::Path fillPath = harmonicPath;
            fillPath.lineTo(plotArea.getRight(), magnitudeArea.getBottom());
            fillPath.lineTo(plotArea.getX(), magnitudeArea.getBottom());
            fillPath.closeSubPath();
            
            // Gradient from brighter (top) to darker (bottom) for beautiful depth effect
            juce::ColourGradient fillGradient(
                harmonicColour.withAlpha(0.12f), fillPath.getBounds().getTopLeft(),
                harmonicColour.withAlpha(0.04f), fillPath.getBounds().getBottomLeft(), false);
            g.setGradientFill(fillGradient);
            g.fillPath(fillPath);
            
            // Draw main curve line with enhanced styling
            // Primary line: bright red with high opacity for clear visibility
            g.setColour(harmonicColour.withAlpha(0.95f));
            g.strokePath(harmonicPath, juce::PathStrokeType(2.0f * scale));
            // Subtle outer glow: softer red with lower opacity for depth
            g.setColour(harmonicColour.withAlpha(0.15f));
            g.strokePath(harmonicPath, juce::PathStrokeType(3.5f * scale));
        }
    }

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
        g.strokePath(extPath, juce::PathStrokeType(1.0f * scale));
    }

    // Removed "Pre" and "Post" labels per user request - users can identify curves by their grey tones.
    // Only show legend if external analyzer is enabled.
    const bool showLegend = showExternal;
    if (showLegend)
    {
        const float pad = 6.0f * scale;
        const float swatch = 10.0f * scale;
        const float rowH = 14.0f * scale;
        juce::StringArray items;
        if (showExternal) items.add("Ext");

        // Modern legend with better styling.
        const float legendW = 70.0f * scale;
        const float legendH = rowH * items.size() + pad * 2.0f;
        auto legend = juce::Rectangle<float>(plotArea.getRight() - legendW - pad,
                                             plotArea.getY() + pad,
                                             legendW, legendH);
        // Modern legend background with subtle gradient.
        juce::ColourGradient legendGradient(theme.panel.withAlpha(0.9f), legend.getTopLeft(),
                                            theme.panel.darker(0.1f).withAlpha(0.85f), legend.getBottomLeft(), false);
        g.setGradientFill(legendGradient);
        g.fillRoundedRectangle(legend, 6.0f * scale);
        g.setColour(theme.panelOutline.withAlpha(0.9f));
        g.drawRoundedRectangle(legend, 6.0f * scale, 1.2f);
        g.setColour(theme.accent.withAlpha(0.2f));
        g.drawRoundedRectangle(legend.reduced(1.0f), 5.0f * scale, 0.8f);

        g.setFont(juce::Font(11.0f * scale, juce::Font::plain));
        auto row = legend.reduced(pad);
        for (int i = 0; i < items.size(); ++i)
        {
            auto line = row.removeFromTop(rowH);
            juce::Colour swatchColour = postColour.withAlpha(0.6f);  // Only "Ext" remains
            // Clean swatch matching Pro-Q style (no glow, simple fill).
            const auto swatchRect = line.removeFromLeft(swatch).toFloat();
            g.setColour(swatchColour);
            g.fillRoundedRectangle(swatchRect, 2.5f);
            g.setColour(theme.textMuted.withAlpha(0.95f));
            g.drawFittedText(items[i], line.toNearestInt(),
                             juce::Justification::centredLeft, 1);
        }

    g.restoreState();
    }

    if (! eqCurveDb.empty())
    {
        const float curveFloor = kAnalyzerMinDb + 2.0f;
        auto buildCurvePath = [&](const std::vector<float>& curve, juce::Path& path)
        {
            bool started = false;
            for (int x = 0; x < static_cast<int>(curve.size()); ++x)
            {
                const float db = curve[static_cast<size_t>(x)];
                if (db <= curveFloor)
                {
                    started = false;
                    continue;
                }
                const float px = plotArea.getX() + static_cast<float>(x);
                const float py = gainToY(db);
                if (! started)
                {
                    path.startNewSubPath(px, py);
                    started = true;
                }
                else
                {
                    path.lineTo(px, py);
                }
            }
        };

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
            buildCurvePath(curve, bandPath);
            if (bandPath.isEmpty())
                continue;

            const auto bandColour = ColorUtils::bandColour(band);
            const bool isSelected = band == selectedBand;
            const float baseline = gainToY(kAnalyzerMinDb);
            juce::Path bandFill;
            bool fillStarted = false;
            float lastX = plotArea.getX();
            for (int x = 0; x < static_cast<int>(curve.size()); ++x)
            {
                const float db = std::max(curveFloor, curve[static_cast<size_t>(x)]);
                const float px = plotArea.getX() + static_cast<float>(x);
                const float py = gainToY(db);
                if (! fillStarted)
                {
                    bandFill.startNewSubPath(px, baseline);
                    bandFill.lineTo(px, py);
                    fillStarted = true;
                }
                else
                {
                    bandFill.lineTo(px, py);
                }
                lastX = px;
            }
            if (fillStarted)
            {
                bandFill.lineTo(lastX, baseline);
                bandFill.closeSubPath();
                g.setColour(bandColour.withAlpha(isSelected ? 0.22f : 0.14f));
                g.fillPath(bandFill);
            }
            g.setColour(bandColour.withAlpha(isSelected ? 0.9f : 0.65f));
            g.strokePath(bandPath, juce::PathStrokeType((isSelected ? 2.0f : 1.6f) * scale));
        }

        if (hoverBand >= 0 && hoverBand < static_cast<int>(perBandCurveDb.size())
            && hoverBand != selectedBand
            && hoverBand < static_cast<int>(perBandActive.size())
            && perBandActive[static_cast<size_t>(hoverBand)])
        {
            const auto& curve = perBandCurveDb[static_cast<size_t>(hoverBand)];
            if (! curve.empty())
            {
                juce::Path hoverPath;
                buildCurvePath(curve, hoverPath);
                if (! hoverPath.isEmpty())
                {
                    const auto hoverColour = ColorUtils::bandColour(hoverBand);
                    g.setColour(hoverColour.withAlpha(0.25f));
                    g.strokePath(hoverPath, juce::PathStrokeType(1.2f * scale));
                }
            }
        }

        juce::Path eqPath;
        buildCurvePath(eqCurveDb, eqPath);
        if (! eqPath.isEmpty())
        {
            if (lastGlobalMix < 0.999f)
            {
                g.setColour(theme.accent.withAlpha(0.28f));
                g.strokePath(eqPath, juce::PathStrokeType(3.6f * scale));
                g.setColour(theme.accent.withAlpha(0.75f));
                g.strokePath(eqPath, juce::PathStrokeType(2.1f * scale));
            }
            g.setColour(theme.text.withAlpha(0.25f));
            g.strokePath(eqPath, juce::PathStrokeType(3.2f * scale));
            g.setColour(theme.text.withAlpha(0.85f));
            g.strokePath(eqPath, juce::PathStrokeType(1.8f * scale));
        }
    }

    if (! selectedBandCurveDb.empty())
    {
        const float curveFloor = kAnalyzerMinDb + 2.0f;
        juce::Path bandPath;
        bool started = false;
        for (int x = 0; x < static_cast<int>(selectedBandCurveDb.size()); ++x)
        {
            const float db = selectedBandCurveDb[static_cast<size_t>(x)];
            if (db <= curveFloor)
            {
                started = false;
                continue;
            }
            const float px = plotArea.getX() + static_cast<float>(x);
            const float py = gainToY(db);
            if (! started)
            {
                bandPath.startNewSubPath(px, py);
                started = true;
            }
            else
            {
                bandPath.lineTo(px, py);
            }
        }

        if (! bandPath.isEmpty())
        {
            const auto bandColour = ColorUtils::bandColour(selectedBand);
            if (lastSelectedMix < 0.999f)
            {
                g.setColour(bandColour.withAlpha(0.35f));
                g.strokePath(bandPath, juce::PathStrokeType(3.0f));
            }
            const float mixAlpha = (lastSelectedMix < 0.999f ? 0.95f : 0.75f);
            g.setColour(bandColour.withAlpha(mixAlpha));
            g.strokePath(bandPath, juce::PathStrokeType(lastSelectedMix < 0.999f ? 2.2f : 1.5f));
        }
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
        const float mix = getBandParameter(band, kParamMixSuffix) / 100.0f;
        const bool isActive = ! bypassed && mix > 0.0005f;
        if (! isActive)
            continue;

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

        if (isSelected || isActive)
        {
            const float labelAlpha = bypassed ? 0.35f : (isSelected ? 0.98f : 0.7f);
            g.setColour(colour.withAlpha(labelAlpha));
            g.setFont(juce::Font(12.0f * scale, isSelected ? juce::Font::bold : juce::Font::plain));
            const float labelW = 26.0f * scale;
            const float labelH = 16.0f * scale;
            const float yJitter = (static_cast<float>((band % 5) - 2) * 0.35f) * labelH;

            juce::Rectangle<float> labelRect(point.x + radius + 2.0f * scale,
                                             point.y - labelH * 0.5f + yJitter,
                                             labelW, labelH);

            if (labelRect.getRight() > plotArea.getRight())
                labelRect.setX(point.x - radius - 2.0f * scale - labelW);

            labelRect.setX(juce::jlimit(static_cast<float>(plotArea.getX()),
                                        static_cast<float>(plotArea.getRight()) - labelW,
                                        labelRect.getX()));
            labelRect.setY(juce::jlimit(static_cast<float>(plotArea.getY()),
                                        static_cast<float>(plotArea.getBottom()) - labelH,
                                        labelRect.getY()));

            labelRects.push_back(labelRect);
            g.drawFittedText(juce::String(band + 1),
                             labelRect.toNearestInt(),
                             juce::Justification::left, 1);
        }
    }

    auto drawPointValue = [&](int bandIndex)
    {
        if (bandIndex < 0 || bandIndex >= ParamIDs::kBandsPerChannel)
            return;
        const bool bypassed = getBandBypassed(bandIndex);
        const float mix = getBandParameter(bandIndex, kParamMixSuffix) / 100.0f;
        if (bypassed || mix <= 0.0005f)
            return;
        const float freq = getBandParameter(bandIndex, kParamFreqSuffix);
        const float gain = getBandParameter(bandIndex, kParamGainSuffix);
        const auto point = juce::Point<float>(frequencyToX(freq), gainToY(gain));
        const juce::String text =
            (freq >= 1000.0f ? juce::String(freq / 1000.0f, freq >= 10000.0f ? 1 : 2) + "kHz"
                             : juce::String(freq, 0) + "Hz")
            + "  " + juce::String(gain, 1) + "dB";
        g.setFont(11.0f * uiScale);
        const float pad = 6.0f * uiScale;
        const float textW = g.getCurrentFont().getStringWidthFloat(text);
        const float textH = 16.0f * uiScale;
        
        // Calculate initial Y position (above the point).
        float pillY = point.y - textH - pad;
        
        // Check for overlap with amplitude scale labels and adjust Y position if needed.
        // Amplitude labels are at specific Y positions for major grid lines (-60 to +60, every 12dB).
        const float spectrumMinDb = kAnalyzerMinDb;  // -60 dB
        const float spectrumMaxDb = kAnalyzerMaxDb;   // +60 dB
        const float spectrumStep = 6.0f;
        const float amplitudeLabelHeight = 14.0f * uiScale;
        const float leftGutter = 52.0f * uiScale;  // Updated to match reduced leftGutter
        const float amplitudeLabelX = magnitudeArea.getX() + 8.0f * uiScale;  // Updated to match reduced margin
        const float amplitudeLabelWidth = 36.0f * uiScale;  // Updated to match reduced labelWidth
        
        // Check if dynamic label would overlap with any amplitude scale label.
        for (float db = spectrumMinDb; db <= spectrumMaxDb + 0.01f; db += spectrumStep)
        {
            const bool major = (static_cast<int>(db) % 12 == 0);
            if (!major)
                continue;
            
            const float amplitudeLabelY = gainToY(db) - amplitudeLabelHeight * 0.5f;
            const float amplitudeLabelTop = amplitudeLabelY;
            const float amplitudeLabelBottom = amplitudeLabelY + amplitudeLabelHeight;
            
            // Check if dynamic label's Y position overlaps with this amplitude label.
            const float pillTop = pillY;
            const float pillBottom = pillY + textH;
            
            // If there's vertical overlap, adjust the dynamic label position.
            if (pillBottom > amplitudeLabelTop && pillTop < amplitudeLabelBottom)
            {
                // Check if they're also horizontally close (amplitude labels are on the left).
                // If the dynamic label is on the left side, move it down to avoid overlap.
                if (point.x < magnitudeArea.getX() + leftGutter + amplitudeLabelWidth + 20.0f * uiScale)
                {
                    // Move dynamic label below the amplitude label.
                    pillY = amplitudeLabelBottom + pad;
                    break;
                }
            }
        }
        
        auto pill = juce::Rectangle<float>(point.x + pad,
                                           pillY,
                                           textW + pad * 2.0f,
                                           textH);
        
        // If label would go outside bounds, try positioning on the other side.
        if (! getLocalBounds().toFloat().contains(pill))
        {
            pill.setPosition(point.x - pill.getWidth() - pad, pillY);
            // Re-check bounds after repositioning.
            if (! getLocalBounds().toFloat().contains(pill))
            {
                // If still outside, try below the point instead.
                pill.setPosition(point.x + pad, point.y + pad);
            }
        }
        
        g.setColour(theme.panel.darker(0.25f).withAlpha(0.9f));
        g.fillRoundedRectangle(pill, 5.0f * uiScale);
        g.setColour(theme.panelOutline.withAlpha(0.85f));
        g.drawRoundedRectangle(pill, 5.0f * uiScale, 1.0f);
        g.setColour(theme.text);
        g.drawFittedText(text, pill.toNearestInt(), juce::Justification::centredLeft, 1);
    };
    if (draggingBand >= 0)
        drawPointValue(draggingBand);

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
    
    // Draw amplitude labels LAST, after all band points and curves, so they're always visible on top.
    // This ensures labels from -60 to +60 dB are never overlapped by graphical elements.
    drawAmplitudeLabels(g, magnitudeArea);
}

void AnalyzerComponent::resized()
{
    // v4.4 beta: Start timer only after first resize to ensure proper initialization
    // Prevents expensive FFT updates and repaints before component is properly laid out
    // This ensures all controls are visible immediately on plugin load
    if (!hasBeenResized)
    {
        hasBeenResized = true;
        startTimerHz(30);
    }
    updateCurves();
}

void AnalyzerComponent::mouseDown(const juce::MouseEvent& event)
{
    if (! allowInteraction)
    {
        if (event.mods.isRightButtonDown())
        {
            const float maxHit = kPointRadius * 0.5f * uiScale;
            float closest = maxHit;
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
            if (closestBand >= 0 && closest <= maxHit)
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
            }
        }
        return;
    }
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

    const float maxHit = kPointRadius * 0.5f * uiScale;
    float closest = maxHit;
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
        if (closestBand >= 0 && closest <= maxHit)
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

    if (closestBand >= 0 && closest <= maxHit)
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

    const float maxHit = kPointRadius * 0.5f * uiScale;
    float closest = maxHit;
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
    hoverBand = (closestBand >= 0 && closest <= maxHit) ? closestBand : -1;
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
    const int channel = selectedChannel;
    auto resetParam = [this, channel, bandIndex](const juce::String& suffix)
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(channel, bandIndex, suffix)))
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
        resetParam("dynExternal");

    if (auto* bypassParam = parameters.getParameter(ParamIDs::bandParamId(channel, bandIndex, kParamBypassSuffix)))
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
    if (! isShowing() || getWidth() <= 0 || getHeight() <= 0)
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
    // v4.4 beta: Higher default update rates for more reactive analyzer
    int hz = (analyzerSpeedIndex == 0 ? 20 : (analyzerSpeedIndex == 1 ? 40 : 70));
    if (effectiveSr >= 192000.0f)
        hz = juce::jmax(10, hz / 2);
    if (effectiveSr >= 384000.0f)
        hz = juce::jmax(10, hz / 3);
    const int viewIndex = parameters.getRawParameterValue(ParamIDs::analyzerView) != nullptr
        ? static_cast<int>(parameters.getRawParameterValue(ParamIDs::analyzerView)->load())
        : 0;
    if (viewIndex != 0)
        hz = juce::jmax(10, static_cast<int>(hz * 0.8f));
    if (hz != lastTimerHz)
    {
        lastTimerHz = hz;
        startTimerHz(hz);
    }

    const bool freeze = parameters.getRawParameterValue(ParamIDs::analyzerFreeze) != nullptr
        && parameters.getRawParameterValue(ParamIDs::analyzerFreeze)->load() > 0.5f;
    if (freeze)
        hz = juce::jmax(8, hz / 2);

    // v4.4 beta: More reactive - update FFT every frame for instant response
    bool didUpdate = false;
    const int phaseMode = processorRef.getLastRmsPhaseMode();
    // Throttle analyzer updates in linear/natural modes to protect audio CPU headroom.
    const int throttleDiv = phaseMode != 0 ? 2 : 1;
    if (throttleDiv == 1)
        throttleCounter = 0;
    bool shouldUpdateFft = ! freeze;
    if (shouldUpdateFft && throttleDiv > 1)
        shouldUpdateFft = (++throttleCounter % throttleDiv) == 0;
    if (shouldUpdateFft)
    {
        updateFft();
        didUpdate = true;
    }
    // v4.4 beta: Update curves every frame for smoother, more reactive display
    updateCurves();
    didUpdate = true;
    
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

    // Stabilize the very low-frequency bins (sub-20 Hz region) so the curve remains readable
    // while still showing energy below 20 Hz.
    const int lowBinLimit = juce::jlimit(1, fftBins - 1,
                                         static_cast<int>((25.0f * fftSize) / lastSampleRate));
    auto stabilizeLowBins = [lowBinLimit](std::array<float, fftBins>& mags)
    {
        if (lowBinLimit <= 1)
            return;

        float sum = 0.0f;
        for (int i = 1; i <= lowBinLimit; ++i)
            sum += mags[i];
        const float avg = sum / static_cast<float>(lowBinLimit);

        // Flatten the lowest bins to avoid a visual slope at the left edge.
        for (int i = 1; i <= lowBinLimit; ++i)
            mags[i] = avg;
    };

    const int viewIndex = parameters.getRawParameterValue(ParamIDs::analyzerView) != nullptr
        ? static_cast<int>(parameters.getRawParameterValue(ParamIDs::analyzerView)->load())
        : 0;
    const bool wantPre = viewIndex != 2;
    const bool wantPost = viewIndex != 1;

    if (wantPre)
    {
        const int pulled = preFifo.pull(timeBuffer.data(), fftSize);
        if (pulled > 0)
        {
            std::fill(fftDataPre.begin(), fftDataPre.end(), 0.0f);
            juce::FloatVectorOperations::copy(fftDataPre.data(), timeBuffer.data(),
                                              juce::jmin(pulled, fftSize));
        window.multiplyWithWindowingTable(fftDataPre.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform(fftDataPre.data());

        for (int i = 0; i < fftBins; ++i)
        {
            const float mag = juce::Decibels::gainToDecibels(fftDataPre[static_cast<size_t>(i)],
                                                            minDb);
            preMagnitudes[i] = Smoothing::smooth(preMagnitudes[i], mag, kSmoothingCoeff);
        }
        stabilizeLowBins(preMagnitudes);
    }
    }

    if (wantPost)
    {
        const int pulled = postFifo.pull(timeBuffer.data(), fftSize);
        if (pulled > 0)
        {
            std::fill(fftDataPost.begin(), fftDataPost.end(), 0.0f);
            juce::FloatVectorOperations::copy(fftDataPost.data(), timeBuffer.data(),
                                              juce::jmin(pulled, fftSize));
        window.multiplyWithWindowingTable(fftDataPost.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform(fftDataPost.data());

        for (int i = 0; i < fftBins; ++i)
        {
            const float mag = juce::Decibels::gainToDecibels(fftDataPost[static_cast<size_t>(i)],
                                                            minDb);
            postMagnitudes[i] = Smoothing::smooth(postMagnitudes[i], mag, kSmoothingCoeff);
        }
        stabilizeLowBins(postMagnitudes);
    }
    }

    // v4.5 beta: Process harmonic curve (harmonic-only content) - red analyzer curve
    // This third analyzer curve displays only harmonic content (no dry/program signal), providing clear feedback
    // for the harmonic processing layer. The curve is only visible when harmonics are active on at least one band.
    //
    // Visibility Logic:
    // - Scans all channels and bands to determine if any harmonic processing is active
    // - A band is considered active if:
    //   1. Harmonic bypass is OFF (harmonicBypass < 0.5)
    //   2. At least one harmonic type (odd or even) has non-zero amount AND mix > 0
    // - If no active harmonics are found, the curve is hidden and magnitudes are reset
    //
    // Processing:
    // - Pulls audio data from the harmonic analyzer tap FIFO (tapped after harmonic processing in DSP)
    // - Applies windowing function (Hann) for spectral leakage reduction
    // - Performs FFT to convert time-domain to frequency-domain
    // - Converts magnitude to decibels and applies smoothing for visual stability
    // - Respects all analyzer settings: range, speed, freeze (via timerCallback and updateFft call)
    bool hasActiveHarmonics = false;
    for (int ch = 0; ch < ParamIDs::kMaxChannels && !hasActiveHarmonics; ++ch)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            const auto oddParam = parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, "odd"));
            const auto evenParam = parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, "even"));
            const auto mixOddParam = parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, "mixOdd"));
            const auto mixEvenParam = parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, "mixEven"));
            const auto bypassParam = parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, "harmonicBypass"));
            
            if (bypassParam != nullptr && bypassParam->load() > 0.5f)
                continue;  // Bypassed - skip this band
            
            const float odd = oddParam != nullptr ? oddParam->load() : 0.0f;
            const float even = evenParam != nullptr ? evenParam->load() : 0.0f;
            const float mixOdd = mixOddParam != nullptr ? (mixOddParam->load() / 100.0f) : 0.0f;
            const float mixEven = mixEvenParam != nullptr ? (mixEvenParam->load() / 100.0f) : 0.0f;
            
            // Check if this band has active harmonics (non-zero amount with mix > 0)
            if ((std::abs(odd) > 0.001f && mixOdd > 0.001f) || (std::abs(even) > 0.001f && mixEven > 0.001f))
            {
                hasActiveHarmonics = true;
                break;
            }
        }
    }
    
    if (hasActiveHarmonics)
    {
        // Pull audio samples from harmonic analyzer tap FIFO
        // This FIFO is fed by the DSP engine after harmonic processing is applied
        auto& harmonicFifoRef = harmonicFifo;
        const int pulled = harmonicFifoRef.pull(timeBuffer.data(), fftSize);
        if (pulled > 0)
        {
            // Clear FFT data buffer and copy time-domain samples
            std::fill(fftDataHarmonic.begin(), fftDataHarmonic.end(), 0.0f);
            juce::FloatVectorOperations::copy(fftDataHarmonic.data(), timeBuffer.data(),
                                              juce::jmin(pulled, fftSize));
            // Apply Hann windowing to reduce spectral leakage
            window.multiplyWithWindowingTable(fftDataHarmonic.data(), fftSize);
            // Perform FFT: convert time-domain to frequency-domain
            fft.performFrequencyOnlyForwardTransform(fftDataHarmonic.data());

            // Convert magnitude to decibels and apply exponential smoothing
            // Smoothing coefficient (kSmoothingCoeff) provides visual stability while maintaining responsiveness
            for (int i = 0; i < fftBins; ++i)
            {
                const float mag = juce::Decibels::gainToDecibels(fftDataHarmonic[static_cast<size_t>(i)],
                                                                minDb);
                harmonicMagnitudes[i] = Smoothing::smooth(harmonicMagnitudes[i], mag, kSmoothingCoeff);
            }
            stabilizeLowBins(harmonicMagnitudes);
        }
    }
    else
    {
        // Reset harmonic magnitudes when no harmonics are active
        // This ensures the curve disappears cleanly when all harmonics are bypassed or set to zero
        harmonicMagnitudes.fill(kAnalyzerMinDb);
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
        stabilizeLowBins(externalMagnitudes);
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
    const float globalMixParam = parameters.getRawParameterValue(ParamIDs::globalMix) != nullptr
        ? parameters.getRawParameterValue(ParamIDs::globalMix)->load()
        : 100.0f;
    hashValue(globalMixParam);
    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
    {
        hashValue(getBandParameter(band, kParamFreqSuffix));
        hashValue(getBandParameter(band, kParamGainSuffix));
        hashValue(getBandParameter(band, kParamQSuffix));
        hashValue(getBandParameter(band, kParamTypeSuffix));
        hashValue(getBandParameter(band, kParamBypassSuffix));
        hashValue(getBandParameter(band, kParamSlopeSuffix));
        hashValue(getBandParameter(band, kParamMixSuffix));
        hashValue(getBandDynamicGainDb(band));
    }

    const bool paramsUnchanged = (hash == lastCurveHash
                                  && selectedChannel == lastCurveChannel
                                  && lastCurveWidth == magnitudeArea.getWidth());
    if (paramsUnchanged && selectedBand != lastCurveBand
        && selectedBand >= 0
        && selectedBand < static_cast<int>(perBandCurveDb.size())
        && ! perBandCurveDb[static_cast<size_t>(selectedBand)].empty())
    {
        selectedBandCurveDb = perBandCurveDb[static_cast<size_t>(selectedBand)];
        lastCurveBand = selectedBand;
        return;
    }

    if (paramsUnchanged && selectedBand == lastCurveBand)
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
        const float gainDb = getBandParameter(band, kParamGainSuffix);
        const int type = getBandType(band);
        const bool dynEnabled = getBandParameter(band, kParamDynEnableSuffix) > 0.5f;
        const bool isBell = type == static_cast<int>(eqdsp::FilterType::bell);
        const bool isShelf = type == static_cast<int>(eqdsp::FilterType::lowShelf)
            || type == static_cast<int>(eqdsp::FilterType::highShelf);
        const bool isTilt = type == static_cast<int>(eqdsp::FilterType::tilt)
            || type == static_cast<int>(eqdsp::FilterType::flatTilt);
        const bool skipZeroGain = ! dynEnabled && (isBell || isShelf || isTilt)
            && std::abs(gainDb) < 0.0001f;

        bandActive[band] = ! getBandBypassed(band)
            && ! skipZeroGain;
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
        hashBand(globalMixParam);
        hashBand(getBandParameter(band, kParamFreqSuffix));
        hashBand(getBandParameter(band, kParamGainSuffix));
        hashBand(getBandParameter(band, kParamQSuffix));
        hashBand(getBandParameter(band, kParamTypeSuffix));
        hashBand(getBandParameter(band, kParamBypassSuffix));
        hashBand(getBandParameter(band, kParamSlopeSuffix));
        hashBand(getBandParameter(band, kParamMixSuffix));
        hashBand(getBandParameter(band, kParamDynEnableSuffix));
        hashBand(getBandDynamicGainDb(band));
        bandDirty[static_cast<size_t>(band)] = bandHash != perBandCurveHash[static_cast<size_t>(band)];
        perBandCurveHash[static_cast<size_t>(band)] = bandHash;
    }

    const bool selectedValid = selectedBand >= 0 && selectedBand < ParamIDs::kBandsPerChannel;
    float selectedMix = 0.0f;
    float selectedGainDb = 0.0f;
    int selectedType = 0;
    bool selectedDynEnabled = false;
    if (selectedValid)
    {
        selectedMix = getBandParameter(selectedBand, kParamMixSuffix) / 100.0f;
        selectedGainDb = getBandParameter(selectedBand, kParamGainSuffix);
        selectedType = getBandType(selectedBand);
        selectedDynEnabled = getBandParameter(selectedBand, kParamDynEnableSuffix) > 0.5f;
    }
    lastSelectedMix = selectedMix;
    const bool selectedIsBell = selectedType == static_cast<int>(eqdsp::FilterType::bell);
    const bool selectedIsShelf = selectedType == static_cast<int>(eqdsp::FilterType::lowShelf)
        || selectedType == static_cast<int>(eqdsp::FilterType::highShelf);
    const bool selectedIsTilt = selectedType == static_cast<int>(eqdsp::FilterType::tilt)
        || selectedType == static_cast<int>(eqdsp::FilterType::flatTilt);
    const bool selectedSkipZeroGain = selectedValid && ! selectedDynEnabled
        && (selectedIsBell || selectedIsShelf || selectedIsTilt)
        && std::abs(selectedGainDb) < 0.0001f;
    const bool selectedActive = selectedValid
        && ! getBandBypassed(selectedBand)
        && ! selectedSkipZeroGain;

    const float globalMix = parameters.getRawParameterValue(ParamIDs::globalMix) != nullptr
        ? juce::jlimit(0.0f, 1.0f,
                       parameters.getRawParameterValue(ParamIDs::globalMix)->load() / 100.0f)
        : 1.0f;
    lastGlobalMix = globalMix;

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
                const float dynamicDeltaDb = getBandDynamicGainDb(band);
                const float mix = juce::jlimit(0.0f, 1.0f,
                                               getBandParameter(band, kParamMixSuffix) / 100.0f);
                if (bandDirty[static_cast<size_t>(band)])
                {
                    response = computeBandResponse(band, freq);
                    if (std::abs(dynamicDeltaDb) > 0.0001f)
                    {
                        const double deltaGain = juce::Decibels::decibelsToGain(dynamicDeltaDb);
                        response = std::complex<double>(1.0, 0.0)
                            + (response - std::complex<double>(1.0, 0.0)) * deltaGain;
                    }
                    auto mixed = std::complex<double>(1.0, 0.0)
                        + static_cast<double>(mix) * (response - std::complex<double>(1.0, 0.0));
                    mixed = std::complex<double>(1.0, 0.0)
                        + static_cast<double>(globalMix) * (mixed - std::complex<double>(1.0, 0.0));
                    const float magnitude = static_cast<float>(std::abs(mixed));
                    perBandCurveDb[static_cast<size_t>(band)][static_cast<size_t>(x)] =
                        juce::Decibels::gainToDecibels(magnitude, minDb);
                    response = mixed;
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

            total += (response - std::complex<double>(1.0, 0.0));
        }

        total = std::complex<double>(1.0, 0.0)
            + static_cast<double>(globalMix) * (total - std::complex<double>(1.0, 0.0));
        const double mag = std::abs(total);
        eqCurveDb[static_cast<size_t>(x)] =
            juce::Decibels::gainToDecibels(static_cast<float>(mag), minDb);
        if (selectedActive)
        {
            std::complex<double> selectedResponse = computeBandResponse(selectedBand, freq);
            const float selectedDynamicDb = getBandDynamicGainDb(selectedBand);
            if (std::abs(selectedDynamicDb) > 0.0001f)
            {
                const double deltaGain = juce::Decibels::decibelsToGain(selectedDynamicDb);
                selectedResponse = std::complex<double>(1.0, 0.0)
                    + (selectedResponse - std::complex<double>(1.0, 0.0)) * deltaGain;
            }
            const float mix = juce::jlimit(0.0f, 1.0f, selectedMix);
            selectedResponse = std::complex<double>(1.0, 0.0)
                + static_cast<double>(mix) * (selectedResponse - std::complex<double>(1.0, 0.0));
            selectedResponse = std::complex<double>(1.0, 0.0)
                + static_cast<double>(globalMix) * (selectedResponse - std::complex<double>(1.0, 0.0));
            selectedBandCurveDb[static_cast<size_t>(x)] =
                juce::Decibels::gainToDecibels(static_cast<float>(std::abs(selectedResponse)), minDb);
        }
        else
        {
            selectedBandCurveDb[static_cast<size_t>(x)] = minDb;
        }
    }
}

void AnalyzerComponent::drawGridLines(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    // Modern grid using theme colors for better harmony.
    const auto gridColour = theme.grid.withAlpha(1.0f);
    const float scale = uiScale;
    const float spectrumMinDb = kAnalyzerMinDb;  // -60 dB
    const float spectrumMaxDb = kAnalyzerMaxDb;   // +60 dB
    const float spectrumStep = 6.0f;
    const int rightGutter = static_cast<int>(44 * scale);
    const int bottomGutter = static_cast<int>(18 * scale);
    
    // Draw amplitude grid lines (background elements, drawn early).
    for (float db = spectrumMinDb; db <= spectrumMaxDb + 0.01f; db += spectrumStep)
    {
        const float y = gainToY(db);
        const bool major = (static_cast<int>(db) % 12 == 0);
        const bool isZero = std::abs(db) < 0.1f;
        
        // Zero dB line gets special treatment.
        if (isZero)
        {
            g.setColour(theme.accent.withAlpha(0.4f));
            g.drawLine(static_cast<float>(area.getX()), y,
                       static_cast<float>(area.getRight()), y, 2.0f);
        }
        else
        {
            // Modern subtle grid lines.
            g.setColour(gridColour.withAlpha(major ? 0.2f : 0.08f));
            g.drawLine(static_cast<float>(area.getX()), y,
                       static_cast<float>(area.getRight()), y, major ? 1.0f : 0.8f);
        }
    }
    
    // Draw frequency grid lines and labels.
    const float maxFreq = getMaxFreq();
    const float majorFreqs[] { 5.0f, 10.0f, 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
    const float minorFreqs[] { 31.5f, 40.0f, 63.0f, 80.0f, 125.0f, 160.0f, 250.0f, 315.0f, 400.0f,
                               630.0f, 800.0f, 1250.0f, 1600.0f, 2500.0f, 3150.0f, 4000.0f,
                               6300.0f, 8000.0f, 12500.0f, 16000.0f };
    float lastLabelX = -1.0e6f;
    const float minLabelSpacing = 30.0f * scale;
    const int labelWidth = static_cast<int>(42 * scale);
    const int labelHeight = static_cast<int>(14 * scale);
    
    // Modern frequency grid lines.
    for (float f : minorFreqs)
    {
        if (f < kMinFreq || f > maxFreq)
            continue;
        const float x = frequencyToX(f);
        g.setColour(gridColour.withAlpha(0.1f));
        g.drawLine(x, static_cast<float>(area.getY()),
                   x, static_cast<float>(area.getBottom()), 0.8f);
    }
    for (float f : majorFreqs)
    {
        if (f < kMinFreq || f > maxFreq)
            continue;
        const float x = frequencyToX(f);
        g.setColour(gridColour.withAlpha(0.25f));
        g.drawLine(x, static_cast<float>(area.getY()),
                   x, static_cast<float>(area.getBottom()), 1.0f);
        if (x + labelWidth <= area.getRight() && (x - lastLabelX) >= minLabelSpacing)
        {
            lastLabelX = x;
            // v4.4 beta: Plain text frequency labels (no background boxes).
            // Removed rounded rectangle backgrounds and borders for minimal, clean appearance.
            const auto labelRect = juce::Rectangle<int>(static_cast<int>(x + 3.0f * scale),
                                                         static_cast<int>(area.getBottom() - bottomGutter),
                                                         labelWidth,
                                                         labelHeight);
            g.setColour(theme.textMuted.withAlpha(0.9f));
            g.setFont(juce::Font(10.0f * scale, juce::Font::plain));
            const juce::String text = f >= 1000.0f
                ? juce::String(f / 1000.0f, (f >= 10000.0f ? 1 : 2)) + "k"
                : juce::String(f, f < 100.0f ? 1 : 0);
            g.drawFittedText(text, labelRect, juce::Justification::centred, 1);
        }
    }

    // Ensure high-end label visibility (20k/40k) even if spacing hides it.
    auto drawHighLabel = [&](float freq)
    {
        if (freq < kMinFreq || freq > maxFreq)
            return;
        const float x = frequencyToX(freq);
        if (x < area.getX() || x > area.getRight())
            return;
        g.setColour(theme.textMuted);
        const juce::String text = freq >= 1000.0f
            ? juce::String(freq / 1000.0f, (freq >= 10000.0f ? 1 : 2)) + "k"
            : juce::String(freq, freq < 100.0f ? 1 : 0);
        const int xPos = static_cast<int>(std::min(x + 3.0f * scale,
                                                   static_cast<float>(area.getRight() - labelWidth)));
        g.drawFittedText(text,
                         juce::Rectangle<int>(xPos,
                                              static_cast<int>(area.getBottom() - bottomGutter),
                                              labelWidth,
                                              labelHeight),
                         juce::Justification::left, 1);
    };
    drawHighLabel(20000.0f);

    // 0 dB reference line (without label - label removed per user request).
    const float db = 0.0f;
    const float y = gainToY(db);
    // Subtle 0 dB line for reference.
    g.setColour(gridColour.withAlpha(0.85f));
    g.drawLine(static_cast<float>(area.getX()), y,
               static_cast<float>(area.getRight()), y, 1.6f);

    // v4.4 beta: Removed "EQ" label from top right corner - user requested removal
}

void AnalyzerComponent::drawAmplitudeLabels(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    // Draw amplitude labels LAST, after all band points and curves, so they're always visible on top.
    // This ensures labels from -60 to +60 dB are never overlapped by graphical elements.
    const float scale = uiScale;
    g.setFont(juce::Font(10.0f * scale, juce::Font::plain));
    
    const float spectrumMinDb = kAnalyzerMinDb;  // -60 dB
    const float spectrumMaxDb = kAnalyzerMaxDb;   // +60 dB
    const float spectrumStep = 6.0f;
    
    // Calculate spacing to determine if labels should be shown.
    // Reduced space usage: smaller left gutter and more compact labels.
    const int leftGutter = static_cast<int>(52 * scale);  // Reduced from 70 to 52 for less space usage
    const int rightGutter = static_cast<int>(44 * scale);
    const int bottomGutter = static_cast<int>(18 * scale);
    const auto labelArea = area.withTrimmedLeft(leftGutter).withTrimmedRight(rightGutter)
        .withTrimmedBottom(bottomGutter);
    const float majorSpacing = labelArea.getHeight() * (12.0f / (spectrumMaxDb - spectrumMinDb));
    const bool showDbLabels = majorSpacing >= 14.0f * scale;
    
    // Draw amplitude labels for all major grid lines from -60 to +60 dB.
    // More compact labels: smaller width, reduced margin, no background boxes for minimal space usage.
    for (float db = spectrumMinDb; db <= spectrumMaxDb + 0.01f; db += spectrumStep)
    {
        const bool major = (static_cast<int>(db) % 12 == 0);
        if (!major || !showDbLabels)
            continue;
            
        const float y = gainToY(db);
        
        // Compact label positioning: reduced margin and width.
        const int labelX = static_cast<int>(area.getX() + 8 * scale);  // Reduced from 18 to 8 for minimal space
        const int labelHeight = static_cast<int>(12 * scale);  // Reduced from 14 to 12
        const int labelWidth = static_cast<int>(36 * scale);  // Reduced from 48 to 36 (still fits "-60")
        
        // Calculate Y position, ensuring label stays within visible area bounds.
        const int labelY = static_cast<int>(y - labelHeight * 0.5f);
        const int labelTop = labelY;
        const int labelBottom = labelY + labelHeight;
        
        // Ensure label doesn't go outside plot area (top and bottom bounds).
        if (labelTop < area.getY() || labelBottom > area.getBottom())
            continue;  // Skip label if it would be clipped at top or bottom
        
        // Ensure label doesn't go outside plot area (right side).
        if (labelX + labelWidth > area.getRight() - static_cast<int>(rightGutter))
            continue;  // Skip label if it would overlap with right side
        
        const auto labelRect = juce::Rectangle<int>(labelX, labelY, labelWidth, labelHeight);
        
        // v4.4 beta: Compact labels - No background boxes, just text for minimal space usage.
        // Text only (no rounded rectangle background) to reduce visual footprint.
        // Reduced leftGutter from 70px to 52px, labelWidth from 48px to 36px for more FFT display space.
        g.setColour(theme.textMuted.withAlpha(0.9f));  // High contrast text, no background
        g.setFont(juce::Font(9.5f * scale, juce::Font::plain));  // Slightly smaller font
        g.drawFittedText(juce::String(db, 0),
                         labelRect,
                         juce::Justification::left, 1);  // Left-aligned for compactness
    }
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
    const float nyquist = lastSampleRate * 0.5f;
    const float preferredMax = 20000.0f;
    const float maxFreq = std::min(preferredMax, nyquist);
    return std::max(kMinFreq * 1.1f, maxFreq);
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

float AnalyzerComponent::getBandDynamicGainDb(int bandIndex) const
{
    return processorRef.getBandDynamicGainDb(selectedChannel, bandIndex);
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
        {
            response = std::pow(response, stages);
        }
        else
        {
            // 6 dB/oct uses only the one-pole stage (no biquad contribution).
            response = { 1.0, 0.0 };
        }
        if (useOnePole)
            response *= onePoleResponse(freq, frequency);
        if (stages == 0 && useOnePole)
        {
            const float resonanceMix = juce::jlimit(0.0f, 0.8f, (q - 0.707f) / 6.0f);
            if (resonanceMix > 0.0f)
            {
                const auto bandPass = computeResponseForType(eqdsp::FilterType::bandPass, 0.0, -1.0);
                response += bandPass * static_cast<double>(resonanceMix);
            }
        }
    }

    return response;
}
