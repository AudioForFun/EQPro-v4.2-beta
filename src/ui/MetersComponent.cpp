#include "MetersComponent.h"
#include "../PluginProcessor.h"

// Output meter drawing and sampling.

namespace
{
// v4.4 beta: Digital domain - 0 dBFS is maximum (not +6 dB)
constexpr float kMinDb = -60.0f;
constexpr float kMaxDb = 0.0f;

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
    if (key == "LTF") return "TFL";
    if (key == "RTF") return "TFR";
    if (key == "TFL") return "TFL";
    if (key == "TFR") return "TFR";
    if (key == "TFC") return "TFC";
    if (key == "TM") return "TM";
    if (key == "TML") return "TML";
    if (key == "TMR") return "TMR";
    if (key == "LTR") return "TRL";
    if (key == "RTR") return "TRR";
    if (key == "TRL") return "TRL";
    if (key == "TRR") return "TRR";
    if (key == "TRC") return "TRC";
    if (key == "LTS") return "TML";
    if (key == "RTS") return "TMR";
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

void MetersComponent::setMeterMode(bool usePeak)
{
    // Controls whether RMS or Peak drives the filled bar.
    showPeakAsFill = usePeak;
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
    // v4.4 beta: Removed peakArea box at bottom - peak values now shown at top of bars
    
    // v4.4 beta: Add dB value labels on the left side of the meter scale
    const float labelWidth = 28.0f;
    const auto labelArea = meterArea.removeFromLeft(labelWidth);
    const int channels = juce::jmax(1, static_cast<int>(rmsDb.size()));
    auto getSafeValue = [](const std::vector<float>& values, int index, float fallback)
    {
        if (index >= 0 && index < static_cast<int>(values.size()))
            return values[static_cast<size_t>(index)];
        return fallback;
    };
    const float gap = channels > 12 ? 2.0f : 4.0f;
    const float minWidth = channels > 12 ? 4.0f : 6.0f;
    const float maxWidth = channels > 12 ? 10.0f : 16.0f;
    const float meterWidthRaw = (meterArea.getWidth() - gap * static_cast<float>(channels - 1))
        / static_cast<float>(channels);
    const float meterWidth = juce::jlimit(minWidth, maxWidth, meterWidthRaw);
    const float totalWidth = meterWidth * static_cast<float>(channels)
        + gap * static_cast<float>(channels - 1);
    const float startX = meterArea.getX() + (meterArea.getWidth() - totalWidth) * 0.5f;

    auto drawMeter = [&](const juce::Rectangle<float>& meterBounds, float rmsDbValue,
                         float peakDbValue, float holdDbValue, const juce::String& label)
    {
        g.setColour(theme.panel);
        g.fillRoundedRectangle(meterBounds, 4.0f);
        g.setColour(theme.panelOutline.withAlpha(0.6f));
        g.drawRoundedRectangle(meterBounds.reduced(0.6f), 3.5f, 1.0f);

        const auto mapDbToY = [&meterBounds](float db)
        {
            const float clamped = juce::jlimit(kMinDb, kMaxDb, db);
            return juce::jmap(clamped, kMinDb, kMaxDb,
                              meterBounds.getBottom(), meterBounds.getY());
        };
        // v4.4 beta: Digital domain graduations - maximum is 0 dBFS
        // Major ticks (every 12 dB)
        const float majorTickDb[] { -60.0f, -48.0f, -36.0f, -24.0f, -12.0f, 0.0f };
        g.setColour(theme.grid.withAlpha(0.5f));
        for (float tick : majorTickDb)
        {
            const float y = mapDbToY(tick);
            // Full width major tick marks
            g.drawLine(meterBounds.getX() + 1.0f, y, meterBounds.getRight() - 1.0f, y, 1.2f);
        }
        
        // Minor ticks (every 6 dB) for better graduation
        const float minorTickDb[] { -54.0f, -42.0f, -30.0f, -18.0f, -6.0f };
        g.setColour(theme.grid.withAlpha(0.25f));
        for (float tick : minorTickDb)
        {
            const float y = mapDbToY(tick);
            // Shorter minor tick marks (half width)
            const float tickWidth = (meterBounds.getWidth() - 2.0f) * 0.5f;
            g.drawLine(meterBounds.getX() + 1.0f, y, meterBounds.getX() + 1.0f + tickWidth, y, 0.8f);
        }
        const float mainDb = showPeakAsFill ? peakDbValue : rmsDbValue;
        const float rmsY = mapDbToY(rmsDbValue);
        const float peakY = mapDbToY(peakDbValue);
        const float mainY = mapDbToY(mainDb);
        const auto fill = juce::Rectangle<float>(meterBounds.getX(), mainY,
                                                 meterBounds.getWidth(),
                                                 meterBounds.getBottom() - mainY);
        
        // v4.4 beta: Color-coded meter bars based on level (green/yellow/red)
        juce::Colour meterColour;
        if (mainDb >= -3.0f)
            meterColour = juce::Colour(0xffff4444);  // Red: near clipping (0 to -3 dB)
        else if (mainDb >= -12.0f)
            meterColour = juce::Colour(0xffffaa44);  // Yellow/Orange: moderate level (-3 to -12 dB)
        else
            meterColour = juce::Colour(0xff44ff44);  // Green: safe level (below -12 dB)
        
        // Gradient fill with color based on level
        juce::ColourGradient fillGrad(meterColour.brighter(0.2f), fill.getTopLeft(),
                                      meterColour.darker(0.25f), fill.getBottomLeft(), false);
        g.setGradientFill(fillGrad);
        g.fillRoundedRectangle(fill, 3.0f);

        g.setColour(theme.meterPeak);
        g.drawLine(meterBounds.getX(), peakY, meterBounds.getRight(), peakY, 1.4f);

        const float holdY = mapDbToY(holdDbValue);
        g.setColour(theme.meterPeak.withAlpha(0.75f));
        g.drawLine(meterBounds.getX(), holdY, meterBounds.getRight(), holdY, 1.0f);
        g.setColour(theme.meterPeak.withAlpha(0.4f));
        g.drawLine(meterBounds.getRight() - 2.0f, holdY, meterBounds.getRight() - 2.0f,
                   holdY + 8.0f, 1.0f);

        if (showPeakAsFill)
        {
            g.setColour(theme.textMuted.withAlpha(0.7f));
            g.drawLine(meterBounds.getX(), rmsY, meterBounds.getRight(), rmsY, 1.0f);
        }

        // v4.4 beta: Channel label at bottom
        juce::String labelText = formatDolbyLabel(label);
        const float labelScale = labelText.length() <= 2 ? 0.9f : 0.75f;
        const float labelFont = juce::jlimit(6.0f, 11.0f, meterBounds.getWidth() * labelScale);
        g.setColour(theme.textMuted);
        g.setFont(labelFont);
        g.drawFittedText(labelText, meterBounds.toNearestInt().removeFromBottom(14),
                         juce::Justification::centred, 1);
        
        // v4.4 beta: Peak value displayed at top of meter bar with background for visibility
        if (peakDbValue > kMinDb + 0.5f)  // Only show if there's a meaningful peak
        {
            const juce::String peakText = juce::String(peakDbValue, 1);
            const auto peakLabelRect = meterBounds.toNearestInt().removeFromTop(14);
            
            // Background for peak value label
            g.setColour(theme.panel.darker(0.3f).withAlpha(0.85f));
            g.fillRoundedRectangle(peakLabelRect.toFloat().reduced(1.0f), 2.0f);
            g.setColour(theme.panelOutline.withAlpha(0.6f));
            g.drawRoundedRectangle(peakLabelRect.toFloat().reduced(1.0f), 2.0f, 0.8f);
            
            // Peak value text
            g.setColour(theme.text.withAlpha(0.95f));
            g.setFont(juce::Font(8.5f, juce::Font::bold));
            g.drawFittedText(peakText, peakLabelRect, juce::Justification::centred, 1);
        }
    };

    // v4.4 beta: Draw dB value labels on the left side of the meter scale
    // Digital domain: maximum is 0 dBFS (not +6 dB)
    // Better graduations: major ticks every 12 dB, show labels for major ticks only
    const float majorTickDb[] { -60.0f, -48.0f, -36.0f, -24.0f, -12.0f, 0.0f };
    const auto mapDbToY = [&meterArea](float db)
    {
        const float clamped = juce::jlimit(kMinDb, kMaxDb, db);
        return juce::jmap(clamped, kMinDb, kMaxDb,
                          meterArea.getBottom(), meterArea.getY());
    };
    
    // v4.4 beta: Better looking labels with improved spacing and visibility
    g.setColour(theme.textMuted.withAlpha(0.9f));
    g.setFont(juce::Font(9.5f, juce::Font::plain));
    for (float tick : majorTickDb)
    {
        const float y = mapDbToY(tick);
        // Format label: whole numbers without decimals, negative sign included
        const juce::String labelText = juce::String(static_cast<int>(tick));
        const auto labelRect = juce::Rectangle<float>(
            labelArea.getX(),
            y - 7.0f,
            labelArea.getWidth() - 4.0f,
            14.0f
        );
        g.drawFittedText(labelText, labelRect.toNearestInt(),
                         juce::Justification::centredRight, 1);
    }
    
    for (int ch = 0; ch < channels; ++ch)
    {
        const juce::String label =
            channelLabels.size() > ch
                ? channelLabels[ch]
                : ("Ch " + juce::String(ch + 1));
        const float x = startX + ch * (meterWidth + gap);
        const auto meterBounds = juce::Rectangle<float>(x, meterArea.getY(),
                                                        meterWidth, meterArea.getHeight());
        const float rmsValue = getSafeValue(rmsDb, ch, kMinDb);
        const float peakValue = getSafeValue(peakDb, ch, kMinDb);
        const float holdValue = getSafeValue(peakHoldDb, ch, peakValue);
        drawMeter(meterBounds, rmsValue, peakValue, holdValue, label);
    }

    // v4.4 beta: Peak meter box removed - peak values now shown at top of each meter bar

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

    repaint();
}

float MetersComponent::dbToY(float db) const
{
    const auto bounds = getLocalBounds().toFloat().reduced(8.0f, 12.0f);
    const float clamped = juce::jlimit(kMinDb, kMaxDb, db);
    return juce::jmap(clamped, kMinDb, kMaxDb, bounds.getBottom(), bounds.getY());
}
