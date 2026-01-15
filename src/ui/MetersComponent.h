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

    float leftRms = -120.0f;
    float leftPeak = -120.0f;
    float rightRms = -120.0f;
    float rightPeak = -120.0f;
    float phaseValue = 0.0f;
    bool dualMode = true;
    ThemeColors theme = makeDarkTheme();
};
