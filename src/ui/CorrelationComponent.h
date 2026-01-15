#pragma once

#include <JuceHeader.h>
#include <array>
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
    static constexpr int kHistorySize = 120;
    std::array<float, kHistorySize> history {};
    int historyIndex = 0;
    ThemeColors theme = makeDarkTheme();
};
