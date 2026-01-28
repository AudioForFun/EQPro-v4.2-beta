#pragma once

#include <JuceHeader.h>
#include "Theme.h"

// Shared look-and-feel for EQ Pro UI widgets.
class EQProLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    // Update palette used by all custom draw calls.
    void setTheme(const ThemeColors& newTheme);

    // Modern 3D beveled rotary knob with LED layer and per-band color support.
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    // Custom toggle button drawing to match text button style (text inside, no checkbox).
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    // Modern 3D beveled text button - harmonizes with toggle buttons and knobs.
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

private:
    ThemeColors theme = makeDarkTheme();
};
