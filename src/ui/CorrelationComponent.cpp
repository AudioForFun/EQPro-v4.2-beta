#include "CorrelationComponent.h"
#include "../PluginProcessor.h"

CorrelationComponent::CorrelationComponent(EQProAudioProcessor& processor)
    : processorRef(processor)
{
    history.fill(0.0f);
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
    const auto meterArea = bounds.withTrimmedBottom(bounds.getHeight() * 0.45f);
    const auto historyArea = bounds.removeFromBottom(bounds.getHeight() * 0.45f);

    const float midX = meterArea.getCentreX();
    g.setColour(theme.grid.withAlpha(0.5f));
    g.drawLine(midX, meterArea.getY() + 4.0f, midX, meterArea.getBottom() - 4.0f, 1.0f);
    g.setColour(theme.grid.withAlpha(0.35f));
    g.drawLine(meterArea.getX() + meterArea.getWidth() * 0.25f,
               meterArea.getY() + 6.0f,
               meterArea.getX() + meterArea.getWidth() * 0.25f,
               meterArea.getBottom() - 6.0f, 1.0f);
    g.drawLine(meterArea.getX() + meterArea.getWidth() * 0.75f,
               meterArea.getY() + 6.0f,
               meterArea.getX() + meterArea.getWidth() * 0.75f,
               meterArea.getBottom() - 6.0f, 1.0f);

    const float norm = (correlation + 1.0f) * 0.5f;
    const float posX = meterArea.getX() + norm * meterArea.getWidth();
    g.setColour(theme.accent.withAlpha(0.3f));
    g.drawLine(posX, meterArea.getY() + 5.0f, posX, meterArea.getBottom() - 5.0f, 6.0f);
    g.setColour(theme.accent);
    g.drawLine(posX, meterArea.getY() + 6.0f, posX, meterArea.getBottom() - 6.0f, 3.0f);

    juce::Path historyPath;
    for (int i = 0; i < kHistorySize; ++i)
    {
        const int idx = (historyIndex + i) % kHistorySize;
        const float x = historyArea.getX() + (static_cast<float>(i) / (kHistorySize - 1)) * historyArea.getWidth();
        const float y = juce::jmap(history[static_cast<size_t>(idx)], -1.0f, 1.0f,
                                   historyArea.getBottom(), historyArea.getY());
        if (i == 0)
            historyPath.startNewSubPath(x, y);
        else
            historyPath.lineTo(x, y);
    }
    g.setColour(theme.accentAlt.withAlpha(0.25f));
    g.strokePath(historyPath, juce::PathStrokeType(4.0f));
    g.setColour(theme.accentAlt.withAlpha(0.8f));
    g.strokePath(historyPath, juce::PathStrokeType(1.8f));

    g.setColour(theme.textMuted);
    g.setFont(12.0f);
    g.drawFittedText("Correlation", titleArea.toNearestInt(),
                     juce::Justification::centred, 1);

    const juce::String valueText = juce::String(correlation, 2);
    g.drawFittedText(valueText, meterArea.toNearestInt().withTrimmedTop(16),
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
    correlation = processorRef.getCorrelation();
    history[static_cast<size_t>(historyIndex)] = correlation;
    historyIndex = (historyIndex + 1) % kHistorySize;
    repaint();
}
