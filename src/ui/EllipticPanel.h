#pragma once

#include <JuceHeader.h>
#include "Theme.h"

class EllipticPanel final : public juce::Component
{
public:
    explicit EllipticPanel(juce::AudioProcessorValueTreeState& state);
    void setTheme(const ThemeColors& newTheme);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    float getParamValue(const juce::String& id, float fallback) const;
    bool isBypassed() const;

    juce::AudioProcessorValueTreeState& parameters;
    juce::Label titleLabel;
    juce::Slider freqSlider;
    juce::Slider amountSlider;
    juce::ToggleButton bypassButton;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> freqAttachment;
    std::unique_ptr<SliderAttachment> amountAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    ThemeColors theme = makeDarkTheme();
};
