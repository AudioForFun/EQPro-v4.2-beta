#include "SpectralDynamicsPanel.h"

namespace
{
constexpr float kLabelFontSize = 12.0f;
}

SpectralDynamicsPanel::SpectralDynamicsPanel(juce::AudioProcessorValueTreeState& state)
    : parameters(state)
{
    titleLabel.setText("Spectral Dynamics", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, theme.text);
    addAndMakeVisible(titleLabel);

    enableButton.setButtonText("Enable");
    enableButton.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    addAndMakeVisible(enableButton);
    enableAttachment = std::make_unique<ButtonAttachment>(parameters, ParamIDs::spectralEnable, enableButton);

    auto setupSlider = [this](juce::Slider& slider, const juce::String& suffix)
    {
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
        slider.setTextBoxIsEditable(true);
        addAndMakeVisible(slider);
        return std::make_unique<SliderAttachment>(parameters, suffix, slider);
    };

    thresholdSlider.setTextValueSuffix(" dB");
    ratioSlider.setTextValueSuffix(":1");
    attackSlider.setTextValueSuffix(" ms");
    releaseSlider.setTextValueSuffix(" ms");
    mixSlider.setTextValueSuffix(" %");

    thresholdAttachment = setupSlider(thresholdSlider, ParamIDs::spectralThreshold);
    ratioAttachment = setupSlider(ratioSlider, ParamIDs::spectralRatio);
    attackAttachment = setupSlider(attackSlider, ParamIDs::spectralAttack);
    releaseAttachment = setupSlider(releaseSlider, ParamIDs::spectralRelease);
    mixAttachment = setupSlider(mixSlider, ParamIDs::spectralMix);
}

void SpectralDynamicsPanel::setTheme(const ThemeColors& newTheme)
{
    theme = newTheme;
    titleLabel.setColour(juce::Label::textColourId, theme.text);
    enableButton.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    repaint();
}

void SpectralDynamicsPanel::paint(juce::Graphics& g)
{
    g.setColour(theme.panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    g.setColour(theme.panelOutline);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);
}

void SpectralDynamicsPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    auto header = bounds.removeFromTop(20);
    titleLabel.setBounds(header.removeFromLeft(150));
    enableButton.setBounds(header);

    bounds.removeFromTop(6);
    auto row = bounds.removeFromTop(90);
    const int knobWidth = (row.getWidth() - 16) / 5;
    thresholdSlider.setBounds(row.removeFromLeft(knobWidth));
    row.removeFromLeft(4);
    ratioSlider.setBounds(row.removeFromLeft(knobWidth));
    row.removeFromLeft(4);
    attackSlider.setBounds(row.removeFromLeft(knobWidth));
    row.removeFromLeft(4);
    releaseSlider.setBounds(row.removeFromLeft(knobWidth));
    row.removeFromLeft(4);
    mixSlider.setBounds(row.removeFromLeft(knobWidth));
}
