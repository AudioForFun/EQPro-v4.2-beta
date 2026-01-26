#pragma once

#include <JuceHeader.h>

struct ThemeColors
{
    juce::Colour background;
    juce::Colour panel;
    juce::Colour panelOutline;
    juce::Colour text;
    juce::Colour textMuted;
    juce::Colour accent;
    juce::Colour accentAlt;
    juce::Colour grid;
    juce::Colour analyzerBg;
    juce::Colour analyzerPhaseBg;
    juce::Colour meterFill;
    juce::Colour meterPeak;
};

inline ThemeColors makeDarkTheme()
{
    return {
        juce::Colour(0xff0b0f14),
        juce::Colour(0xff111827),
        juce::Colour(0xff1f2937),
        juce::Colour(0xffe6edf3),
        juce::Colour(0xff94a3b8),
        juce::Colour(0xff22d3ee),
        juce::Colour(0xffa78bfa),
        juce::Colour(0xff1f2937),
        juce::Colour(0xff0b0f14),
        juce::Colour(0xff0b0f14),
        juce::Colour(0xff22c55e),
        juce::Colour(0xfff97316),
    };
}

inline ThemeColors makeLightTheme()
{
    return {
        juce::Colour(0xfff5f7fb),
        juce::Colour(0xffeef2f7),
        juce::Colour(0xffcbd5e1),
        juce::Colour(0xff0f172a),
        juce::Colour(0xff64748b),
        juce::Colour(0xff0891b2),
        juce::Colour(0xff8b5cf6),
        juce::Colour(0xffd7dde6),
        juce::Colour(0xfff1f5f9),
        juce::Colour(0xffe2e8f0),
        juce::Colour(0xff16a34a),
        juce::Colour(0xffdc2626),
    };
}
