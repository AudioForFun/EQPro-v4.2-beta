#pragma once

#include <JuceHeader.h>
#include "Theme.h"

class EQProAudioProcessor;

class MetersComponent final : public juce::Component,
                              private juce::Timer
{
public:
    explicit MetersComponent(EQProAudioProcessor& processor);

    void setSelectedChannel(int channelIndex);
    void setChannelLabels(const juce::StringArray& labels);
    void setTheme(const ThemeColors& newTheme);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    float dbToY(float db) const;

    EQProAudioProcessor& processorRef;
    int selectedChannel = 0;
    juce::StringArray channelLabels;
    std::vector<float> rmsDb;
    std::vector<float> peakDb;
    std::vector<float> peakHoldDb;
    ThemeColors theme = makeDarkTheme();
};
