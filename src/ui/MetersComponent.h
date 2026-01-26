#pragma once

#include <JuceHeader.h>
#include "Theme.h"

class EQProAudioProcessor;

// Output meter panel with RMS/peak display and peak hold.
class MetersComponent final : public juce::Component,
                              private juce::Timer
{
public:
    explicit MetersComponent(EQProAudioProcessor& processor);

    // Keep the meter aligned with the selected channel.
    void setSelectedChannel(int channelIndex);
    // Updates channel labels for multi-channel layouts.
    void setChannelLabels(const juce::StringArray& labels);
    // Apply theme palette.
    void setTheme(const ThemeColors& newTheme);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    // Utility for mapping dB to meter Y space.
    float dbToY(float db) const;

    EQProAudioProcessor& processorRef;
    // Small toggle in the meter header (RMS/Peak focus).
    juce::TextButton meterModeButton;
    // When true, the filled bar follows peak instead of RMS.
    bool showPeakAsFill = false;
    int selectedChannel = 0;
    juce::StringArray channelLabels;
    std::vector<float> rmsDb;
    std::vector<float> peakDb;
    std::vector<float> peakHoldDb;
    ThemeColors theme = makeDarkTheme();
};
