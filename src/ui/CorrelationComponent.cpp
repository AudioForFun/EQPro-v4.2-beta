#include "CorrelationComponent.h"
#include "../PluginProcessor.h"
#include <cmath>

CorrelationComponent::CorrelationComponent(EQProAudioProcessor& processor)
    : processorRef(processor)
{
    startTimerHz(30);
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
        juce::Path scopePath;
        constexpr float kBaseGain = 0.6f;
        constexpr float kSoftClip = 2.0f;
        const float clipNorm = std::tanh(kSoftClip);
        for (int i = 0; i < scopePointCount; ++i)
        {
            const auto& p = scopePoints[static_cast<size_t>(i)];
            const float sx = std::tanh(p.x * kBaseGain * kSoftClip) / clipNorm;
            const float sy = std::tanh(p.y * kBaseGain * kSoftClip) / clipNorm;
            const float x = centre.x + sx * radius;
            const float y = centre.y - sy * radius;
            if (i == 0)
                scopePath.startNewSubPath(x, y);
            else
                scopePath.lineTo(x, y);
        }
        g.setColour(theme.accent.withAlpha(0.7f));
        g.strokePath(scopePath, juce::PathStrokeType(1.1f));
    }

    g.setColour(theme.textMuted);
    g.setFont(12.0f);
    g.drawFittedText("Goniometer", titleArea.toNearestInt(),
                     juce::Justification::centred, 1);
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
