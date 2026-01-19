#include "LookAndFeel.h"
#include <BinaryData.h>

void EQProLookAndFeel::setTheme(const ThemeColors& newTheme)
{
    theme = newTheme;
}

void EQProLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPosProportional, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider& slider)
{
    if (! knobFilmstrip.isValid())
    {
        knobFilmstrip = juce::ImageCache::getFromMemory(BinaryData::knob86Filmstrip_png,
                                                        BinaryData::knob86Filmstrip_pngSize);
        if (knobFilmstrip.isValid())
        {
            const int frameSize = knobFilmstrip.getWidth();
            knobFrames = frameSize > 0 ? (knobFilmstrip.getHeight() / frameSize) : 0;
        }
    }

    const float size = static_cast<float>(juce::jmin(width, height)) - 8.0f;
    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, size, size)
                            .withCentre(juce::Point<float>(static_cast<float>(x + width / 2),
                                                          static_cast<float>(y + height / 2)));
    const auto radius = size * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle
        + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    if (knobFilmstrip.isValid() && knobFrames > 0)
    {
        const int frame = juce::jlimit(0, knobFrames - 1,
                                       static_cast<int>(std::round(sliderPosProportional * (knobFrames - 1))));
        const int frameSize = knobFilmstrip.getWidth();
        const int srcY = frame * frameSize;
        g.drawImage(knobFilmstrip,
                    bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                    0, srcY, frameSize, frameSize);
    }
    else
    {
        g.setColour(theme.panel.withAlpha(0.9f));
        g.fillEllipse(bounds);
    }

    const auto tint = slider.findColour(juce::Slider::trackColourId);
    const float dotRadius = 1.8f;
    const int dotCount = 24;
    for (int i = 0; i < dotCount; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(dotCount - 1);
        const float dotAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        const float dotX = centre.x + std::cos(dotAngle) * (radius - 6.0f);
        const float dotY = centre.y + std::sin(dotAngle) * (radius - 6.0f);
        const float alpha = (t <= sliderPosProportional + 0.001f) ? 0.9f : 0.25f;
        g.setColour(tint.withAlpha(slider.isEnabled() ? alpha : 0.15f));
        g.fillEllipse(dotX - dotRadius, dotY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
    }

    juce::Path pointer;
    const float pointerLength = radius - 10.0f;
    const float pointerThickness = 2.0f;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength,
                                pointerThickness, pointerLength * 0.7f, 1.0f);
    g.setColour(theme.text.withAlpha(slider.isEnabled() ? 0.9f : 0.4f));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}
