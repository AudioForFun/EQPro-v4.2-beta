#pragma once

#include <JuceHeader.h>
#include "Theme.h"

class EQProAudioProcessor;

class CorrelationComponent final : public juce::Component,
                                   private juce::Timer
{
public:
    explicit CorrelationComponent(EQProAudioProcessor& processor);

    void paint(juce::Graphics&) override;
    void resized() override;
    void setTheme(const ThemeColors& newTheme);

private:
    void timerCallback() override;

    EQProAudioProcessor& processorRef;
    float correlation = 0.0f;
    ThemeColors theme = makeDarkTheme();
};
