#include "LookAndFeel.h"

void EQProLookAndFeel::setTheme(const ThemeColors& newTheme)
{
    theme = newTheme;
}

void EQProLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPosProportional, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider& slider)
{
    // Option 1: Minimalist 3D Beveled Knob with LED layer
    const float size = static_cast<float>(juce::jmin(width, height)) - 8.0f;
    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, size, size)
                            .withCentre(juce::Point<float>(static_cast<float>(x + width / 2),
                                                          static_cast<float>(y + height / 2)));
    const auto radius = size * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle
        + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Hover and focus indicators (subtle outer glow)
    if (slider.isMouseOverOrDragging())
    {
        g.setColour(theme.accent.withAlpha(0.2f));
        g.drawEllipse(bounds.expanded(3.0f), 2.0f);
    }
    if (slider.hasKeyboardFocus(true))
    {
        g.setColour(theme.accent.withAlpha(0.4f));
        g.drawEllipse(bounds.expanded(4.0f), 2.0f);
    }

    // Get per-band color (or default accent color)
    const auto tint = slider.findColour(juce::Slider::trackColourId);
    const bool isEnabled = slider.isEnabled();

    // Shadow/drop shadow for 3D effect
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillEllipse(bounds.translated(0.0f, 2.5f));

    // Main knob body with 3D beveled effect using gradients
    // Outer edge (darker)
    const float bevelWidth = 3.0f;
    const auto outerBounds = bounds;
    const auto innerBounds = bounds.reduced(bevelWidth);
    
    // Base gradient: light at top-left, dark at bottom-right for 3D bevel
    juce::ColourGradient baseGradient(
        theme.panel.brighter(0.15f), outerBounds.getTopLeft().toFloat(),
        theme.panel.darker(0.3f), outerBounds.getBottomRight().toFloat(), false);
    g.setGradientFill(baseGradient);
    g.fillEllipse(outerBounds);

    // Inner highlight for depth (lighter top section)
    juce::ColourGradient highlightGradient(
        theme.panel.brighter(0.25f).withAlpha(0.6f), 
        juce::Point<float>(centre.x - radius * 0.3f, centre.y - radius * 0.3f),
        theme.panel.withAlpha(0.0f), 
        juce::Point<float>(centre.x, centre.y), false);
    g.setGradientFill(highlightGradient);
    g.fillEllipse(innerBounds);

    // Subtle inner shadow (darker bottom section)
    juce::ColourGradient shadowGradient(
        theme.panel.withAlpha(0.0f),
        juce::Point<float>(centre.x, centre.y),
        juce::Colours::black.withAlpha(0.2f),
        juce::Point<float>(centre.x + radius * 0.3f, centre.y + radius * 0.3f), false);
    g.setGradientFill(shadowGradient);
    g.fillEllipse(innerBounds);

    // Outer border ring (subtle)
    g.setColour(theme.panelOutline.withAlpha(0.4f));
    g.drawEllipse(outerBounds.reduced(0.5f), 1.0f);

    // Inner border ring (subtle highlight)
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawEllipse(innerBounds.reduced(0.5f), 0.8f);

    // LED Layer: Colored arc track showing active range (smooth, no dots)
    const float trackRadius = radius - 8.0f;
    const float trackWidth = 3.5f;
    const float trackInnerRadius = trackRadius - trackWidth;
    
    // Draw inactive track (subtle background)
    juce::Path inactiveTrack;
    inactiveTrack.addCentredArc(centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                                rotaryStartAngle, rotaryEndAngle, true);
    inactiveTrack.addCentredArc(centre.x, centre.y, trackInnerRadius, trackInnerRadius, 0.0f,
                                rotaryEndAngle, rotaryStartAngle, false);
    inactiveTrack.closeSubPath();
    g.setColour(theme.panelOutline.withAlpha(isEnabled ? 0.15f : 0.08f));
    g.fillPath(inactiveTrack);

    // Draw active LED arc (only the portion up to current value)
    if (sliderPosProportional > 0.001f)
    {
        juce::Path activeTrack;
        const float activeEndAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        activeTrack.addCentredArc(centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                                  rotaryStartAngle, activeEndAngle, true);
        activeTrack.addCentredArc(centre.x, centre.y, trackInnerRadius, trackInnerRadius, 0.0f,
                                  activeEndAngle, rotaryStartAngle, false);
        activeTrack.closeSubPath();
        
        // Colored fill with gradient for LED effect
        juce::ColourGradient ledGradient(
            tint.brighter(0.3f).withAlpha(isEnabled ? 0.95f : 0.3f),
            juce::Point<float>(centre.x - trackRadius * 0.5f, centre.y - trackRadius * 0.5f),
            tint.withAlpha(isEnabled ? 0.75f : 0.25f),
            juce::Point<float>(centre.x + trackRadius * 0.5f, centre.y + trackRadius * 0.5f), false);
        g.setGradientFill(ledGradient);
        g.fillPath(activeTrack);
        
        // LED glow effect (outer glow on active arc)
        g.setColour(tint.withAlpha(isEnabled ? 0.3f : 0.1f));
        g.strokePath(activeTrack, juce::PathStrokeType(trackWidth + 2.0f));
    }

    // Pointer with 3D effect and shadow
    const float pointerLength = radius - 12.0f;
    const float pointerThickness = 2.5f;
    
    // Pointer shadow
    juce::Path pointerShadow;
    pointerShadow.addRoundedRectangle(-pointerThickness * 0.5f - 0.5f, -pointerLength - 0.5f,
                                       pointerThickness + 1.0f, pointerLength * 0.75f, 1.5f);
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillPath(pointerShadow, juce::AffineTransform::rotation(angle).translated(centre.x + 1.0f, centre.y + 1.0f));
    
    // Main pointer
    juce::Path pointer;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength,
                                pointerThickness, pointerLength * 0.75f, 1.0f);
    
    // Pointer gradient (lighter at tip)
    juce::ColourGradient pointerGradient(
        theme.text.brighter(0.2f).withAlpha(isEnabled ? 0.95f : 0.4f),
        juce::Point<float>(0.0f, -pointerLength),
        theme.text.withAlpha(isEnabled ? 0.85f : 0.3f),
        juce::Point<float>(0.0f, 0.0f), false);
    g.setGradientFill(pointerGradient);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

    // Center cap with 3D effect
    const float capRadius = 3.0f;
    const auto capBounds = juce::Rectangle<float>(centre.x - capRadius, centre.y - capRadius,
                                                   capRadius * 2.0f, capRadius * 2.0f);
    
    // Cap shadow
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillEllipse(capBounds.translated(0.5f, 0.5f));
    
    // Cap gradient (3D beveled)
    juce::ColourGradient capGradient(
        theme.text.brighter(0.15f).withAlpha(isEnabled ? 0.9f : 0.4f),
        capBounds.getTopLeft().toFloat(),
        theme.text.darker(0.2f).withAlpha(isEnabled ? 0.7f : 0.3f),
        capBounds.getBottomRight().toFloat(), false);
    g.setGradientFill(capGradient);
    g.fillEllipse(capBounds);
    
    // Cap highlight
    g.setColour(juce::Colours::white.withAlpha(isEnabled ? 0.2f : 0.1f));
    g.drawEllipse(capBounds.reduced(0.5f), 0.5f);

    // Default value indicator (snap point) - removed per user request (residual dot not wanted).
    // Removed the dot that was appearing at the default value position.
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

    // v4.2: Clear, well-defined borders for solo toggles to match rest of GUI.
    // Dual-layer border system ensures visibility even with shading/gradients behind.
    // Outer border for definition (always visible).
    auto outerBorderColour = theme.panelOutline;
    if (!isEnabled)
        outerBorderColour = outerBorderColour.withAlpha(0.5f);
    else if (isOn)
        outerBorderColour = theme.accent;
    else if (isOver)
        outerBorderColour = theme.panelOutline.brighter(0.15f);
    
    g.setColour(outerBorderColour);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.2f);
    
    // Inner border for depth and better definition (especially with shading behind).
    auto innerBorderColour = theme.panelOutline.withAlpha(0.3f);
    if (isOn)
        innerBorderColour = theme.accent.withAlpha(0.4f);
    else if (isOver && isEnabled)
        innerBorderColour = theme.accent.withAlpha(0.25f);
    
    g.setColour(innerBorderColour);
    g.drawRoundedRectangle(bounds.reduced(1.5f), 3.0f, 0.8f);

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
