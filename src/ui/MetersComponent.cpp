#include "MetersComponent.h"
#include "../PluginProcessor.h"

namespace
{
constexpr float kMinDb = -60.0f;
constexpr float kMaxDb = 6.0f;

juce::String formatDolbyLabel(const juce::String& label)
{
    auto key = label.toUpperCase().removeCharacters(" /");
    if (key == "L") return "L";
    if (key == "R") return "R";
    if (key == "C") return "C";
    if (key == "LFE") return "LFE";
    if (key == "LFE2") return "LFE2";
    if (key == "LS") return "Ls";
    if (key == "RS") return "Rs";
    if (key == "LRS") return "Lrs";
    if (key == "RRS") return "Rrs";
    if (key == "LC") return "Lc";
    if (key == "RC") return "Rc";
    if (key == "LTF") return "Ltf";
    if (key == "RTF") return "Rtf";
    if (key == "TFC") return "Tfc";
    if (key == "TM") return "Tm";
    if (key == "LTR") return "Ltr";
    if (key == "RTR") return "Rtr";
    if (key == "TRC") return "Trc";
    if (key == "LTS") return "Lts";
    if (key == "RTS") return "Rts";
    if (key == "LW") return "Lw";
    if (key == "RW") return "Rw";
    if (key == "BFL") return "Bfl";
    if (key == "BFR") return "Bfr";
    if (key == "BFC") return "Bfc";
    return label;
}
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
    g.setColour(theme.panel.darker(0.5f).withAlpha(0.6f));
    g.drawRoundedRectangle(bounds.reduced(1.5f), 6.0f, 1.0f);
    g.setColour(theme.panel.brighter(0.3f).withAlpha(0.2f));
    g.drawRoundedRectangle(bounds.reduced(2.5f), 6.0f, 1.0f);

    auto meterArea = bounds.reduced(8.0f, 12.0f);
    const auto phaseArea = meterArea.removeFromBottom(14.0f);
    const auto peakArea = meterArea.removeFromBottom(18.0f);
    const int channels = juce::jmax(1, static_cast<int>(rmsDb.size()));
    const float gap = channels > 10 ? 2.0f : 4.0f;
    const float meterWidthRaw = (meterArea.getWidth() - gap * static_cast<float>(channels - 1))
        / static_cast<float>(channels);
    float meterWidth = meterWidthRaw;
    const float maxWidth = channels > 10 ? 12.0f : 16.0f;
    if (meterWidth > maxWidth)
        meterWidth = maxWidth;
    float totalWidth = meterWidth * static_cast<float>(channels)
        + gap * static_cast<float>(channels - 1);
    if (totalWidth > meterArea.getWidth())
    {
        meterWidth = meterWidthRaw;
        totalWidth = meterArea.getWidth();
    }
    const float startX = meterArea.getX() + (meterArea.getWidth() - totalWidth) * 0.5f;

    auto drawMeter = [&](float x, float rmsDbValue, float peakDbValue, float holdDbValue,
                         const juce::String& label)
    {
        const auto meterBounds = juce::Rectangle<float>(x, meterArea.getY(), meterWidth,
                                                        meterArea.getHeight());
        g.setColour(theme.panel);
        g.fillRoundedRectangle(meterBounds, 4.0f);
        g.setColour(theme.panelOutline.withAlpha(0.6f));
        g.drawRoundedRectangle(meterBounds.reduced(0.6f), 3.5f, 1.0f);

        const auto mapDbToY = [&meterArea](float db)
        {
            const float clamped = juce::jlimit(kMinDb, kMaxDb, db);
            return juce::jmap(clamped, kMinDb, kMaxDb, meterArea.getBottom(), meterArea.getY());
        };
        const float tickDb[] { -60.0f, -48.0f, -36.0f, -24.0f, -12.0f, -6.0f, 0.0f, 6.0f };
        g.setColour(theme.grid.withAlpha(0.35f));
        for (float tick : tickDb)
        {
            const float y = mapDbToY(tick);
            g.drawLine(meterBounds.getX() + 2.0f, y, meterBounds.getRight() - 2.0f, y, 1.0f);
        }
        const float rmsY = mapDbToY(rmsDbValue);
        const float peakY = mapDbToY(peakDbValue);
        const auto fill = juce::Rectangle<float>(meterBounds.getX(), rmsY,
                                                 meterBounds.getWidth(),
                                                 meterBounds.getBottom() - rmsY);
        juce::ColourGradient fillGrad(theme.meterFill.brighter(0.2f), fill.getTopLeft(),
                                      theme.meterFill.darker(0.25f), fill.getBottomLeft(), false);
        g.setGradientFill(fillGrad);
        g.fillRoundedRectangle(fill, 3.0f);

        g.setColour(theme.meterPeak);
        g.drawLine(meterBounds.getX(), peakY, meterBounds.getRight(), peakY, 1.5f);

        const float holdY = mapDbToY(holdDbValue);
        g.setColour(theme.meterPeak.withAlpha(0.75f));
        g.drawLine(meterBounds.getX(), holdY, meterBounds.getRight(), holdY, 1.0f);

        juce::String labelText = formatDolbyLabel(label);
        const float labelScale = labelText.length() <= 2 ? 0.9f : 0.75f;
        const float labelFont = juce::jlimit(6.0f, 11.0f, meterWidth * labelScale);
        g.setColour(theme.textMuted);
        g.setFont(labelFont);
        g.drawFittedText(labelText, meterBounds.toNearestInt().withHeight(14),
                         juce::Justification::centred, 1);
    };

    for (int ch = 0; ch < channels; ++ch)
    {
        const juce::String label =
            channelLabels.size() > ch
                ? channelLabels[ch]
                : ("Ch " + juce::String(ch + 1));
        const float x = startX + ch * (meterWidth + gap);
        const float holdValue = peakHoldDb.size() > static_cast<size_t>(ch)
            ? peakHoldDb[static_cast<size_t>(ch)]
            : peakDb[static_cast<size_t>(ch)];
        drawMeter(x,
                  rmsDb[static_cast<size_t>(ch)],
                  peakDb[static_cast<size_t>(ch)],
                  holdValue,
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
        peakHoldDb.assign(static_cast<size_t>(totalChannels), kMinDb);
    }

    for (int ch = 0; ch < totalChannels; ++ch)
    {
        const auto state = processorRef.getMeterState(ch);
        rmsDb[static_cast<size_t>(ch)] = state.rmsDb;
        peakDb[static_cast<size_t>(ch)] = state.peakDb;
        const float currentPeak = peakDb[static_cast<size_t>(ch)];
        float& hold = peakHoldDb[static_cast<size_t>(ch)];
        if (currentPeak >= hold || hold <= kMinDb + 0.1f)
            hold = currentPeak;
        else
            hold = juce::jmax(currentPeak, hold - 0.7f);
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
