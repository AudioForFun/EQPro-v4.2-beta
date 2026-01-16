#include "MetersComponent.h"
#include "../PluginProcessor.h"

namespace
{
constexpr float kMinDb = -60.0f;
constexpr float kMaxDb = 6.0f;
}

MetersComponent::MetersComponent(EQProAudioProcessor& processor)
    : processorRef(processor)
{
    startTimerHz(30);
}

void MetersComponent::setSelectedChannel(int channelIndex)
{
    selectedChannel = channelIndex;
}

void MetersComponent::setChannelLabels(const juce::StringArray& labels)
{
    channelLabels = labels;
}

void MetersComponent::setTheme(const ThemeColors& newTheme)
{
    theme = newTheme;
    repaint();
}

void MetersComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(theme.analyzerBg);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(theme.panelOutline);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    auto meterArea = bounds.reduced(8.0f, 12.0f);
    const auto phaseArea = meterArea.removeFromBottom(14.0f);
    const auto peakArea = meterArea.removeFromBottom(18.0f);
    const int channels = juce::jmax(1, static_cast<int>(rmsDb.size()));
    const float gap = 4.0f;
    const float meterWidth = (meterArea.getWidth() - gap * static_cast<float>(channels - 1))
        / static_cast<float>(channels);
    const float fontSize = channels > 8 ? 9.5f : 11.0f;

    auto drawMeter = [&](float x, float rmsDbValue, float peakDbValue, const juce::String& label)
    {
        const auto meterBounds = juce::Rectangle<float>(x, meterArea.getY(), meterWidth,
                                                        meterArea.getHeight());
        g.setColour(theme.panel);
        g.fillRoundedRectangle(meterBounds, 4.0f);

        const auto mapDbToY = [&meterArea](float db)
        {
            const float clamped = juce::jlimit(kMinDb, kMaxDb, db);
            return juce::jmap(clamped, kMinDb, kMaxDb, meterArea.getBottom(), meterArea.getY());
        };
        const float rmsY = mapDbToY(rmsDbValue);
        const float peakY = mapDbToY(peakDbValue);
        const auto fill = juce::Rectangle<float>(meterBounds.getX(), rmsY,
                                                 meterBounds.getWidth(),
                                                 meterBounds.getBottom() - rmsY);
        g.setColour(theme.meterFill);
        g.fillRoundedRectangle(fill, 3.0f);

        g.setColour(theme.meterPeak);
        g.drawLine(meterBounds.getX(), peakY, meterBounds.getRight(), peakY, 1.5f);

        g.setColour(theme.textMuted);
        g.setFont(fontSize);
        g.drawFittedText(label, meterBounds.toNearestInt().withHeight(16),
                         juce::Justification::centred, 1);
    };

    for (int ch = 0; ch < channels; ++ch)
    {
        const juce::String label =
            channelLabels.size() > ch
                ? channelLabels[ch]
                : ("Ch " + juce::String(ch + 1));
        const float x = meterArea.getX() + ch * (meterWidth + gap);
        drawMeter(x,
                  rmsDb[static_cast<size_t>(ch)],
                  peakDb[static_cast<size_t>(ch)],
                  label);
    }

    float peakDbValue = kMinDb;
    for (const auto value : peakDb)
        peakDbValue = juce::jmax(peakDbValue, value);
    g.setColour(theme.panel);
    g.fillRoundedRectangle(peakArea, 4.0f);
    const float peakNorm = juce::jmap(juce::jlimit(kMinDb, kMaxDb, peakDbValue),
                                      kMinDb, kMaxDb, 0.0f, 1.0f);
    const auto peakFill = peakArea.withWidth(peakArea.getWidth() * peakNorm);
    g.setColour(theme.meterPeak);
    g.fillRoundedRectangle(peakFill, 4.0f);
    g.setColour(theme.text);
    g.setFont(12.0f);
    g.drawFittedText("Peak " + juce::String(peakDbValue, 1) + " dB",
                     peakArea.toNearestInt(), juce::Justification::centred, 1);

    g.setColour(theme.panel);
    g.fillRoundedRectangle(phaseArea, 4.0f);
    const float midX = phaseArea.getCentreX();
    const float phaseNorm = juce::jlimit(-1.0f, 1.0f, phaseValue);
    const float phaseX = juce::jmap(phaseNorm, -1.0f, 1.0f, phaseArea.getX(), phaseArea.getRight());
    g.setColour(theme.grid.withAlpha(0.5f));
    g.drawLine(midX, phaseArea.getY() + 2.0f, midX, phaseArea.getBottom() - 2.0f, 1.0f);
    g.setColour(theme.accentAlt.withAlpha(0.4f));
    g.drawLine(midX, phaseArea.getCentreY(), phaseX, phaseArea.getCentreY(), 4.0f);
    g.setColour(theme.accentAlt);
    g.drawLine(midX, phaseArea.getCentreY(), phaseX, phaseArea.getCentreY(), 2.0f);
    g.setColour(theme.textMuted);
    g.setFont(11.0f);
    g.drawFittedText("Phase", phaseArea.toNearestInt(), juce::Justification::centred, 1);
}

void MetersComponent::resized()
{
}

void MetersComponent::timerCallback()
{
    const int totalChannels = juce::jmax(1, processorRef.getTotalNumInputChannels());
    if (static_cast<int>(rmsDb.size()) != totalChannels)
    {
        rmsDb.assign(static_cast<size_t>(totalChannels), kMinDb);
        peakDb.assign(static_cast<size_t>(totalChannels), kMinDb);
    }

    for (int ch = 0; ch < totalChannels; ++ch)
    {
        const auto state = processorRef.getMeterState(ch);
        rmsDb[static_cast<size_t>(ch)] = state.rmsDb;
        peakDb[static_cast<size_t>(ch)] = state.peakDb;
    }

    phaseValue = processorRef.getCorrelation();
    repaint();
}

float MetersComponent::dbToY(float db) const
{
    const auto bounds = getLocalBounds().toFloat().reduced(8.0f, 12.0f);
    const float clamped = juce::jlimit(kMinDb, kMaxDb, db);
    return juce::jmap(clamped, kMinDb, kMaxDb, bounds.getBottom(), bounds.getY());
}
