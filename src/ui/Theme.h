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
        juce::Colour(0xff111418),
        juce::Colour(0xff131820),
        juce::Colour(0xff1f2937),
        juce::Colour(0xffe5e7eb),
        juce::Colour(0xffcbd5e1),
        juce::Colour(0xff4fc3f7),
        juce::Colour(0xffff8a65),
        juce::Colour(0xff1f2a33),
        juce::Colour(0xff0b0e12),
        juce::Colour(0xff141a21),
        juce::Colour(0xff43a047),
        juce::Colour(0xffff7043),
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
