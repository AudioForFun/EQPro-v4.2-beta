#pragma once

#include <JuceHeader.h>
#include "Theme.h"

class EQProLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void setTheme(const ThemeColors& newTheme);

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

private:
    ThemeColors theme = makeDarkTheme();
};
