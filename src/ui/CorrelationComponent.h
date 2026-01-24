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
    static constexpr int kScopePoints = 512;
    std::array<juce::Point<float>, kScopePoints> scopePoints {};
    int scopePointCount = 0;
    ThemeColors theme = makeDarkTheme();
};
