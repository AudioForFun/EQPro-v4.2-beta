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

    if (slider.isMouseOverOrDragging())
    {
        g.setColour(theme.accent.withAlpha(0.25f));
        g.drawEllipse(bounds.expanded(3.0f), 2.0f);
    }
    if (slider.hasKeyboardFocus(true))
    {
        g.setColour(theme.accent.withAlpha(0.55f));
        g.drawEllipse(bounds.expanded(4.0f), 2.0f);
    }

    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillEllipse(bounds.translated(0.0f, 2.0f));

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

    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawEllipse(bounds.reduced(1.5f), 1.5f);

    // Draw per-band colored LED dots over the filmstrip.
    const auto tint = slider.findColour(juce::Slider::trackColourId);
    const float dotRadius = 1.6f;
    const int dotCount = 24;
    for (int i = 0; i < dotCount; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(dotCount - 1);
        const float dotAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        const float dotX = centre.x + std::cos(dotAngle) * (radius - 6.0f);
        const float dotY = centre.y + std::sin(dotAngle) * (radius - 6.0f);
        const float alpha = (t <= sliderPosProportional + 0.001f) ? 0.95f : 0.45f;
        g.setColour(tint.withAlpha(slider.isEnabled() ? alpha : 0.15f));
        g.fillEllipse(dotX - dotRadius, dotY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
    }

    // Smaller inner LED ring to cover the filmstrip's inner dots.
    const float innerRadius = radius - 16.0f;
    const float innerDotRadius = 1.1f;
    const int innerDotCount = 16;
    for (int i = 0; i < innerDotCount; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(innerDotCount - 1);
        const float dotAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        const float dotX = centre.x + std::cos(dotAngle) * innerRadius;
        const float dotY = centre.y + std::sin(dotAngle) * innerRadius;
        const float alpha = (t <= sliderPosProportional + 0.001f) ? 0.85f : 0.35f;
        g.setColour(tint.withAlpha(slider.isEnabled() ? alpha : 0.15f));
        g.fillEllipse(dotX - innerDotRadius, dotY - innerDotRadius,
                      innerDotRadius * 2.0f, innerDotRadius * 2.0f);
    }

    juce::Path pointer;
    const float pointerLength = radius - 10.0f;
    const float pointerThickness = 2.0f;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength,
                                pointerThickness, pointerLength * 0.7f, 1.0f);
    g.setColour(theme.text.withAlpha(slider.isEnabled() ? 0.9f : 0.4f));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

    const float capRadius = 2.6f;
    g.setColour(theme.text.withAlpha(0.8f));
    g.fillEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);

    if (slider.isDoubleClickReturnEnabled())
    {
        const double defaultValue = slider.getDoubleClickReturnValue();
        const double range = slider.getRange().getLength();
        const double epsilon = (range > 0.0) ? range * 0.001 : 0.0001;
        if (std::abs(slider.getValue() - defaultValue) <= epsilon)
        {
            const float snapAngle = rotaryStartAngle;
            const float tickX = centre.x + std::cos(snapAngle) * (radius - 8.0f);
            const float tickY = centre.y + std::sin(snapAngle) * (radius - 8.0f);
            g.setColour(theme.accent.withAlpha(0.85f));
            g.fillEllipse(tickX - 1.8f, tickY - 1.8f, 3.6f, 3.6f);
        }
    }
}

void EQProLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const bool isOn = button.getToggleState();
    const bool isEnabled = button.isEnabled();
    const bool isOver = button.isMouseOver();
    const bool isDown = shouldDrawButtonAsDown || button.isMouseButtonDown();

    // Get colors from component properties (set via setColour with custom IDs or use theme defaults).
    // For custom toggle buttons styled like text buttons, we use theme colors directly.
    auto bgColour = theme.panel.withAlpha(0.2f);
    if (isOn)
        bgColour = theme.accent.withAlpha(0.55f);
    if (!isEnabled)
        bgColour = bgColour.withMultipliedAlpha(0.5f);
    else if (isDown)
        bgColour = bgColour.brighter(0.1f);
    else if (isOver)
        bgColour = bgColour.brighter(0.05f);

    // Draw rounded rectangle background.
    g.setColour(bgColour);
    g.fillRoundedRectangle(bounds.reduced(0.5f), 4.0f);

    // Draw outline.
    auto outlineColour = theme.panelOutline.withAlpha(isEnabled ? 0.6f : 0.3f);
    if (isOver && isEnabled)
        outlineColour = theme.accent.withAlpha(0.4f);
    g.setColour(outlineColour);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

    // Draw text centered inside.
    auto textColour = button.findColour(juce::ToggleButton::textColourId);
    if (isOn)
        textColour = theme.text;  // Brighter text when on
    if (!isEnabled)
        textColour = textColour.withMultipliedAlpha(0.5f);

    g.setColour(textColour);
    g.setFont(juce::Font(12.0f).boldened());
    g.drawFittedText(button.getButtonText(), bounds.toNearestInt(),
                     juce::Justification::centred, 1);
}
