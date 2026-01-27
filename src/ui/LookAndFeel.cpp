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

    // v4.4 beta: Fast flat rendering for rotary knobs - performance optimized
    // Simplified from gradient to flat color for instant rendering
    // Main knob body: Flat color (no gradient for performance)
    g.setColour(theme.panel);
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
                                         bool shouldDrawButtonAsHighlighted,
                                         bool shouldDrawButtonAsDown)
{
    // v4.4 beta: Fast flat rendering for toggle buttons - performance optimized
    // CRITICAL: Removed expensive ColourGradient objects and highlight overlays that were blocking UI thread
    // Simplified from 3D gradients to flat colors for instant rendering
    // Applied to all toggle buttons: solo toggles, bypass toggles, RMS/Peak toggles, etc.
    const auto bounds = button.getLocalBounds().toFloat();
    const bool isOn = button.getToggleState();
    const bool isEnabled = button.isEnabled();
    const bool isOver = button.isMouseOver();
    const bool isDown = shouldDrawButtonAsDown || button.isMouseButtonDown();
    
    auto drawBounds = bounds.reduced(0.5f);
    const float cornerRadius = 4.0f;
    
    // Fast flat background color (no gradient for performance)
    auto bgColour = isOn ? theme.accent.withAlpha(isEnabled ? 0.7f : 0.35f)
                         : theme.panel.withAlpha(isEnabled ? 0.3f : 0.15f);
    if (isDown && isEnabled)
        bgColour = bgColour.darker(0.15f);
    else if (isOver && isEnabled && !isOn)
        bgColour = bgColour.brighter(0.1f);
    
    g.setColour(bgColour);
    g.fillRoundedRectangle(drawBounds, cornerRadius);
    
    // Simple border (no complex color calculations)
    auto borderColour = theme.panelOutline;
    if (!isEnabled)
        borderColour = borderColour.withAlpha(0.5f);
    else if (isOn)
        borderColour = theme.accent.withAlpha(0.9f);
    else if (isOver)
        borderColour = borderColour.brighter(0.1f);
    
    g.setColour(borderColour);
    g.drawRoundedRectangle(drawBounds, cornerRadius, 1.0f);

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

void EQProLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                             const juce::Colour& backgroundColour,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown)
{
    // v4.4 beta: Fast flat rendering for text buttons - performance optimized
    // CRITICAL: Removed expensive ColourGradient objects that were blocking UI thread
    // Simplified from 3D gradients to flat colors for instant rendering
    // Applied to all text buttons: preset section, EQ control section, snapshot buttons, undo/redo
    const auto bounds = button.getLocalBounds().toFloat();
    const bool isEnabled = button.isEnabled();
    const bool isOver = button.isMouseOver();
    const bool isDown = shouldDrawButtonAsDown || button.isMouseButtonDown();
    
    auto drawBounds = bounds.reduced(0.5f);
    const float cornerRadius = 4.0f;
    
    // Fast flat background color (no gradient for performance)
    auto bgColour = theme.panel.withAlpha(isEnabled ? 0.3f : 0.15f);
    if (isDown && isEnabled)
        bgColour = bgColour.darker(0.15f);
    else if (isOver && isEnabled)
        bgColour = bgColour.brighter(0.1f);
    
    g.setColour(bgColour);
    g.fillRoundedRectangle(drawBounds, cornerRadius);
    
    // Simple border (no complex color calculations)
    auto borderColour = theme.panelOutline;
    if (!isEnabled)
        borderColour = borderColour.withAlpha(0.5f);
    else if (isOver)
        borderColour = borderColour.brighter(0.1f);
    
    g.setColour(borderColour);
    g.drawRoundedRectangle(drawBounds, cornerRadius, 1.0f);
}
