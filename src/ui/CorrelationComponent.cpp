#include "CorrelationComponent.h"
#include "../PluginProcessor.h"
#include <cmath>

// Goniometer rendering and scope point capture.

CorrelationComponent::CorrelationComponent(EQProAudioProcessor& processor)
    : processorRef(processor)
{
    startTimerHz(30);
    scopeGainSmoothed.reset(30.0, 0.15);
    scopeGainSmoothed.setCurrentAndTargetValue(1.0f);
}

void CorrelationComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(8.0f, 8.0f);
    juce::ColourGradient bg(theme.panel, bounds.getTopLeft(),
                            theme.panel.darker(0.2f), bounds.getBottomLeft(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(bounds, 8.0f);

    g.setColour(theme.panelOutline);
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    const auto titleArea = bounds.removeFromTop(18.0f);
    // Correlation bar lives under the goniometer.
    const float corrLabelHeight = 14.0f;
    const float corrBarHeight = 10.0f;
    const float corrPad = 6.0f;
    auto corrArea = bounds.removeFromBottom(corrLabelHeight + corrBarHeight + corrPad);
    const auto corrLabelArea = corrArea.removeFromTop(corrLabelHeight);
    corrArea.removeFromTop(2.0f);
    const auto corrBarArea = corrArea.withHeight(corrBarHeight);

    auto scopeArea = bounds.reduced(6.0f, 6.0f);
    const float size = juce::jmin(scopeArea.getWidth(), scopeArea.getHeight());
    scopeArea = scopeArea.withSizeKeepingCentre(size, size);

    g.setColour(theme.panel.darker(0.1f));
    g.fillRect(scopeArea);
    g.setColour(juce::Colour(0xff6b7280));
    g.drawRect(scopeArea, 1.2f);

    const auto centre = scopeArea.getCentre();
    const float radius = size * 0.46f;
    g.setColour(theme.grid.withAlpha(0.5f));
    g.drawLine(centre.x, scopeArea.getY() + 4.0f, centre.x, scopeArea.getBottom() - 4.0f, 1.0f);
    g.drawLine(scopeArea.getX() + 4.0f, centre.y, scopeArea.getRight() - 4.0f, centre.y, 1.0f);
    g.setColour(theme.grid.withAlpha(0.35f));
    g.drawLine(scopeArea.getX() + 6.0f, scopeArea.getY() + 6.0f,
               scopeArea.getRight() - 6.0f, scopeArea.getBottom() - 6.0f, 1.0f);
    g.drawLine(scopeArea.getRight() - 6.0f, scopeArea.getY() + 6.0f,
               scopeArea.getX() + 6.0f, scopeArea.getBottom() - 6.0f, 1.0f);
    g.drawEllipse(scopeArea.reduced(size * 0.07f), 1.0f);

    if (scopePointCount > 1)
    {
        double sumSq = 0.0;
        for (int i = 0; i < scopePointCount; ++i)
        {
            const auto& p = scopePoints[static_cast<size_t>(i)];
            sumSq += static_cast<double>(p.x) * static_cast<double>(p.x);
            sumSq += static_cast<double>(p.y) * static_cast<double>(p.y);
        }
        const double rms = std::sqrt(sumSq / static_cast<double>(scopePointCount * 2));
        const float targetGain = rms > 1.0e-6
            ? juce::jlimit(0.35f, 1.6f, static_cast<float>(0.6 / rms))
            : 1.0f;
        scopeGainSmoothed.setTargetValue(targetGain);
        scopeGainSmoothed.skip(scopePointCount);
        const float autoGain = scopeGainSmoothed.getCurrentValue();

        juce::Path scopePath;
        constexpr float kBaseGain = 0.75f;
        constexpr float kSoftClip = 1.6f;
        const float clipNorm = std::tanh(kSoftClip);
        for (int i = 0; i < scopePointCount; ++i)
        {
            const auto& p = scopePoints[static_cast<size_t>(i)];
            const float sx = std::tanh(p.x * kBaseGain * autoGain * kSoftClip) / clipNorm;
            const float sy = std::tanh(p.y * kBaseGain * autoGain * kSoftClip) / clipNorm;
            const float x = centre.x + sx * radius;
            const float y = centre.y - sy * radius;
            if (i == 0)
                scopePath.startNewSubPath(x, y);
            else
                scopePath.lineTo(x, y);
        }
        g.setColour(theme.accent.withAlpha(0.35f));
        g.strokePath(scopePath, juce::PathStrokeType(2.0f));
        g.setColour(theme.accent.withAlpha(0.8f));
        g.strokePath(scopePath, juce::PathStrokeType(1.1f));
    }

    g.setColour(theme.textMuted);
    g.setFont(12.0f);
    g.drawFittedText("Goniometer", titleArea.toNearestInt(),
                     juce::Justification::centred, 1);

    // Correlation meter (centered, -1..1).
    const float correlation = juce::jlimit(-1.0f, 1.0f, processorRef.getCorrelation());
    g.setColour(theme.textMuted);
    g.setFont(11.0f);
    g.drawFittedText("Correlation", corrLabelArea.toNearestInt(),
                     juce::Justification::centredLeft, 1);

    g.setColour(theme.panel.darker(0.2f));
    g.fillRoundedRectangle(corrBarArea, 3.0f);
    g.setColour(theme.panelOutline.withAlpha(0.7f));
    g.drawRoundedRectangle(corrBarArea, 3.0f, 1.0f);

    const float midX = corrBarArea.getCentreX();
    g.setColour(theme.grid.withAlpha(0.5f));
    g.drawLine(midX, corrBarArea.getY(), midX, corrBarArea.getBottom(), 1.0f);

    const float fillWidth = (corrBarArea.getWidth() * 0.5f) * std::abs(correlation);
    juce::Rectangle<float> fillRect;
    if (correlation >= 0.0f)
        fillRect = { midX, corrBarArea.getY(), fillWidth, corrBarArea.getHeight() };
    else
        fillRect = { midX - fillWidth, corrBarArea.getY(), fillWidth, corrBarArea.getHeight() };

    const auto fillColour = correlation >= 0.0f ? theme.meterFill : theme.meterPeak;
    g.setColour(fillColour.withAlpha(0.85f));
    g.fillRoundedRectangle(fillRect, 3.0f);
}

void CorrelationComponent::resized()
{
}

void CorrelationComponent::setTheme(const ThemeColors& newTheme)
{
    theme = newTheme;
    repaint();
}

void CorrelationComponent::timerCallback()
{
    int writePos = 0;
    scopePointCount = processorRef.getGoniometerPoints(scopePoints.data(),
                                                       static_cast<int>(scopePoints.size()),
                                                       writePos);
    juce::ignoreUnused(writePos);
    repaint();
}
