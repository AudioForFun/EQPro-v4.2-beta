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

    // Optimized 3D beveled effect: Single gradient for performance
    // Shadow/drop shadow for 3D effect (simplified)
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillEllipse(bounds.translated(0.0f, 2.0f));

    // Main knob body: Single optimized gradient (light top-left, dark bottom-right)
    juce::ColourGradient baseGradient(
        theme.panel.brighter(0.12f), bounds.getTopLeft().toFloat(),
        theme.panel.darker(0.25f), bounds.getBottomRight().toFloat(), false);
    g.setGradientFill(baseGradient);
    g.fillEllipse(bounds);

    // Single subtle border (reduced operations)
    g.setColour(theme.panelOutline.withAlpha(0.5f));
    g.drawEllipse(bounds.reduced(0.5f), 1.0f);

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
        
        // Optimized LED fill: Single color fill (gradient removed for performance)
        g.setColour(tint.withAlpha(isEnabled ? 0.85f : 0.3f));
        g.fillPath(activeTrack);
        
        // Simplified LED glow (single stroke instead of separate glow)
        g.setColour(tint.withAlpha(isEnabled ? 0.25f : 0.1f));
        g.strokePath(activeTrack, juce::PathStrokeType(trackWidth + 1.5f));
    }

    // Optimized pointer: Simplified shadow and single-color fill
    const float pointerLength = radius - 12.0f;
    const float pointerThickness = 2.5f;
    
    // Simplified pointer shadow (reduced opacity for performance)
    juce::Path pointerShadow;
    pointerShadow.addRoundedRectangle(-pointerThickness * 0.5f - 0.5f, -pointerLength - 0.5f,
                                       pointerThickness + 1.0f, pointerLength * 0.75f, 1.5f);
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillPath(pointerShadow, juce::AffineTransform::rotation(angle).translated(centre.x + 0.8f, centre.y + 0.8f));
    
    // Main pointer: Single color (gradient removed for performance)
    juce::Path pointer;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength,
                                pointerThickness, pointerLength * 0.75f, 1.0f);
    g.setColour(theme.text.withAlpha(isEnabled ? 0.9f : 0.4f));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

    // Optimized center cap: Simplified (single fill, no gradient)
    const float capRadius = 3.0f;
    const auto capBounds = juce::Rectangle<float>(centre.x - capRadius, centre.y - capRadius,
                                                   capRadius * 2.0f, capRadius * 2.0f);
    
    // Simplified cap shadow
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.fillEllipse(capBounds.translated(0.4f, 0.4f));
    
    // Single color cap (gradient removed for performance)
    g.setColour(theme.text.withAlpha(isEnabled ? 0.85f : 0.4f));
    g.fillEllipse(capBounds);

    // Default value indicator (snap point) - removed per user request (residual dot not wanted).
    // Removed the dot that was appearing at the default value position.
}

void EQProLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    // Option 1: Soft 3D Beveled Pill - harmonizes with modern 3D knobs
    const auto bounds = button.getLocalBounds().toFloat();
    const bool isOn = button.getToggleState();
    const bool isEnabled = button.isEnabled();
    const bool isOver = button.isMouseOver();
    const bool isDown = shouldDrawButtonAsDown || button.isMouseButtonDown();
    
    // Adjust bounds for pressed state (subtle "pushed in" effect)
    auto drawBounds = bounds.reduced(0.5f);
    if (isDown && isEnabled)
        drawBounds = drawBounds.translated(0.0f, 1.0f);  // Slight downward shift when pressed
    
    const float cornerRadius = 4.0f;
    
    // Background gradient: light top, dark bottom (same style as knobs)
    juce::ColourGradient bgGradient(
        isOn ? theme.accent.brighter(0.15f).withAlpha(isEnabled ? 0.7f : 0.35f)
             : theme.panel.brighter(0.1f).withAlpha(isEnabled ? 0.3f : 0.15f),
        drawBounds.getTopLeft().toFloat(),
        isOn ? theme.accent.darker(0.15f).withAlpha(isEnabled ? 0.6f : 0.3f)
             : theme.panel.darker(0.15f).withAlpha(isEnabled ? 0.25f : 0.12f),
        drawBounds.getBottomRight().toFloat(), false);
    
    // Apply hover/down state adjustments
    if (isDown && isEnabled)
    {
        // Darker when pressed
        bgGradient = juce::ColourGradient(
            bgGradient.getColour(0).darker(0.1f),
            drawBounds.getTopLeft().toFloat(),
            bgGradient.getColour(1).darker(0.1f),
            drawBounds.getBottomRight().toFloat(), false);
    }
    else if (isOver && isEnabled && !isOn)
    {
        // Slightly brighter on hover (OFF state only)
        bgGradient = juce::ColourGradient(
            bgGradient.getColour(0).brighter(0.05f),
            drawBounds.getTopLeft().toFloat(),
            bgGradient.getColour(1).brighter(0.05f),
            drawBounds.getBottomRight().toFloat(), false);
    }
    
    g.setGradientFill(bgGradient);
    g.fillRoundedRectangle(drawBounds, cornerRadius);
    
    // Optimized 3D effect: Single subtle highlight overlay (reduced operations)
    // Simplified top-left highlight (single overlay instead of two gradients)
    g.setColour(juce::Colours::white.withAlpha(isEnabled ? 0.06f : 0.03f));
    g.fillRoundedRectangle(drawBounds.reduced(1.5f, 1.5f).withTrimmedBottom(drawBounds.getHeight() * 0.5f), cornerRadius - 1.5f);
    
    // Single clean border (theme.panelOutline or theme.accent when ON)
    auto borderColour = theme.panelOutline;
    if (!isEnabled)
        borderColour = borderColour.withAlpha(0.5f);
    else if (isOn)
        borderColour = theme.accent.withAlpha(0.9f);
    else if (isOver)
        borderColour = theme.panelOutline.brighter(0.15f);
    
    g.setColour(borderColour);
    g.drawRoundedRectangle(drawBounds, cornerRadius, 1.2f);

    // Draw text centered inside.
    auto textColour = button.findColour(juce::ToggleButton::textColourId);
    if (isOn)
        textColour = theme.text;  // Brighter text when on
    if (!isEnabled)
        textColour = textColour.withMultipliedAlpha(0.5f);

    g.setColour(textColour);
    g.setFont(juce::Font(12.0f).boldened());
    g.drawFittedText(button.getButtonText(), drawBounds.toNearestInt(),
                     juce::Justification::centred, 1);
}
