#pragma once

#include <JuceHeader.h>
#include <array>
#include "Theme.h"

class EQProAudioProcessor;

// Goniometer/phase scope with correlation readout.
class CorrelationComponent final : public juce::Component,
                                   private juce::Timer
{
public:
    explicit CorrelationComponent(EQProAudioProcessor& processor);

    // Draws scope, grid, and trace.
    void paint(juce::Graphics&) override;
    void resized() override;
    // Apply theme palette.
    void setTheme(const ThemeColors& newTheme);

private:
    void timerCallback() override;

    EQProAudioProcessor& processorRef;
    // Ring of pre-decimated scope points.
    static constexpr int kScopePoints = 512;
    std::array<juce::Point<float>, kScopePoints> scopePoints {};
    int scopePointCount = 0;
    // Smoothed auto-gain for consistent scope size.
    juce::SmoothedValue<float> scopeGainSmoothed { 1.0f };
    ThemeColors theme = makeDarkTheme();
};
