#include "CorrelationComponent.h"
#include "../PluginProcessor.h"

CorrelationComponent::CorrelationComponent(EQProAudioProcessor& processor)
    : processorRef(processor)
{
    startTimerHz(30);
}

void CorrelationComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(8.0f, 8.0f);
    g.setColour(theme.panel);
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(theme.panelOutline);
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    const float midX = bounds.getCentreX();
    g.setColour(theme.textMuted.withAlpha(0.5f));
    g.drawLine(midX, bounds.getY() + 6.0f, midX, bounds.getBottom() - 6.0f, 1.0f);

    const float norm = (correlation + 1.0f) * 0.5f;
    const float posX = bounds.getX() + norm * bounds.getWidth();
    g.setColour(theme.accent);
    g.drawLine(posX, bounds.getY() + 10.0f, posX, bounds.getBottom() - 10.0f, 3.0f);

    g.setColour(theme.textMuted);
    g.setFont(12.0f);
    g.drawFittedText("Correlation", bounds.toNearestInt().withHeight(16),
                     juce::Justification::centred, 1);

    const juce::String valueText = juce::String(correlation, 2);
    g.drawFittedText(valueText, bounds.toNearestInt().withTrimmedTop(16),
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
    repaint();
}
