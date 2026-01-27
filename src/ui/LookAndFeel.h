#pragma once

#include <JuceHeader.h>
#include "Theme.h"

// Shared look-and-feel for EQ Pro UI widgets.
class EQProLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    // Update palette used by all custom draw calls.
    void setTheme(const ThemeColors& newTheme);

    // Filmstrip-based rotary knob with hover/focus cues.
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    // Custom toggle button drawing to match text button style (text inside, no checkbox).
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    ThemeColors theme = makeDarkTheme();
    juce::Image knobFilmstrip;
    int knobFrames = 0;
};
