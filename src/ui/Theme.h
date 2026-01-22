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
        juce::Colour(0xff000000),
        juce::Colour(0xff000000),
        juce::Colour(0xff0c0c0c),
        juce::Colour(0xffe6edf3),
        juce::Colour(0xffb6c2cf),
        juce::Colour(0xff22d3ee),
        juce::Colour(0xffa78bfa),
        juce::Colour(0xff000000),
        juce::Colour(0xff000000),
        juce::Colour(0xff000000),
        juce::Colour(0xff22c55e),
        juce::Colour(0xfff97316),
    };
}

inline ThemeColors makeLightTheme()
{
    return {
        juce::Colour(0xfff7f8fa),
        juce::Colour(0xfff0f2f5),
        juce::Colour(0xffcbd5e1),
        juce::Colour(0xff111827),
        juce::Colour(0xff475569),
        juce::Colour(0xff1d4ed8),
        juce::Colour(0xffea580c),
        juce::Colour(0xffd7dce3),
        juce::Colour(0xffeef2f7),
        juce::Colour(0xffe2e8f0),
        juce::Colour(0xff16a34a),
        juce::Colour(0xffdc2626),
    };
}
