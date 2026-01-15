#include "EllipticPanel.h"
#include "../util/ParamIDs.h"

EllipticPanel::EllipticPanel(juce::AudioProcessorValueTreeState& state)
    : parameters(state)
{
    titleLabel.setText("Elliptic", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, theme.text);
    addAndMakeVisible(titleLabel);

    freqSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    freqSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    freqSlider.setTextBoxIsEditable(true);
    freqSlider.setSkewFactorFromMidPoint(120.0);
    freqSlider.setTextValueSuffix(" Hz");
    addAndMakeVisible(freqSlider);

    amountSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    amountSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    amountSlider.setTextBoxIsEditable(true);
    amountSlider.setTextValueSuffix(" amt");
    addAndMakeVisible(amountSlider);

    bypassButton.setButtonText("Bypass");
    bypassButton.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    addAndMakeVisible(bypassButton);

    freqAttachment = std::make_unique<SliderAttachment>(parameters, ParamIDs::ellipticFreq,
                                                        freqSlider);
    amountAttachment = std::make_unique<SliderAttachment>(parameters, ParamIDs::ellipticAmount,
                                                          amountSlider);
    bypassAttachment = std::make_unique<ButtonAttachment>(parameters, ParamIDs::ellipticBypass,
                                                          bypassButton);
}

void EllipticPanel::paint(juce::Graphics& g)
{
    g.setColour(theme.panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    g.setColour(theme.panelOutline);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);

    const bool bypassed = isBypassed();
    g.setColour(bypassed ? theme.textMuted : theme.accent);
    g.setFont(10.0f);
    g.drawFittedText(bypassed ? "Bypassed" : "Active",
                     getLocalBounds().withTrimmedTop(4).withHeight(14),
                     juce::Justification::centredRight, 1);

    auto curveArea = getLocalBounds().reduced(12, 8).withHeight(40).withTrimmedTop(18);
    const float freq = getParamValue(ParamIDs::ellipticFreq, 120.0f);
    const float amount = getParamValue(ParamIDs::ellipticAmount, 1.0f);
    const float maxFreq = 20000.0f;

    juce::Path curve;
    for (int x = 0; x < curveArea.getWidth(); ++x)
    {
        const float norm = static_cast<float>(x) / static_cast<float>(curveArea.getWidth());
        const float f = 20.0f * std::pow(maxFreq / 20.0f, norm);
        const float response = 1.0f / std::sqrt(1.0f + std::pow(f / freq, 4.0f));
        const float y = juce::jmap(response * (1.0f - amount * 0.7f),
                                   0.0f, 1.0f,
                                   static_cast<float>(curveArea.getBottom()),
                                   static_cast<float>(curveArea.getY()));
        if (x == 0)
            curve.startNewSubPath(curveArea.getX() + x, y);
        else
            curve.lineTo(curveArea.getX() + x, y);
    }

    g.setColour(theme.accentAlt.withAlpha(bypassed ? 0.3f : 0.8f));
    g.strokePath(curve, juce::PathStrokeType(1.2f));
}

void EllipticPanel::setTheme(const ThemeColors& newTheme)
{
    theme = newTheme;
    titleLabel.setColour(juce::Label::textColourId, theme.text);
    bypassButton.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    repaint();
}

float EllipticPanel::getParamValue(const juce::String& id, float fallback) const
{
    if (auto* param = parameters.getRawParameterValue(id))
        return param->load();
    return fallback;
}

bool EllipticPanel::isBypassed() const
{
    return getParamValue(ParamIDs::ellipticBypass, 1.0f) > 0.5f;
}

void EllipticPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    titleLabel.setBounds(bounds.removeFromTop(20));

    auto sliders = bounds.removeFromTop(110);
    const int knobWidth = (sliders.getWidth() - 8) / 2;
    freqSlider.setBounds(sliders.removeFromLeft(knobWidth));
    sliders.removeFromLeft(8);
    amountSlider.setBounds(sliders.removeFromLeft(knobWidth));

    bounds.removeFromTop(6);
    bypassButton.setBounds(bounds.removeFromTop(20));
}
