#include "LookAndFeel.h"

void EQProLookAndFeel::setTheme(const ThemeColors& newTheme)
{
    theme = newTheme;
}

void EQProLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPosProportional, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                               static_cast<float>(width), static_cast<float>(height))
                            .reduced(6.0f);
    const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle
        + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    g.setColour(theme.panel);
    g.fillEllipse(bounds);
    g.setColour(theme.panelOutline);
    g.drawEllipse(bounds, 1.0f);

    juce::Path arc;
    arc.addCentredArc(centre.x, centre.y, radius - 2.0f, radius - 2.0f,
                      0.0f, rotaryStartAngle, angle, true);
    g.setColour(theme.accent.withAlpha(slider.isEnabled() ? 0.9f : 0.4f));
    g.strokePath(arc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved));

    juce::Path pointer;
    const float pointerLength = radius - 6.0f;
    const float pointerThickness = 2.0f;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength,
                                pointerThickness, pointerLength * 0.7f, 1.0f);
    g.setColour(theme.text);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}
