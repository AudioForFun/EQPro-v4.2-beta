#pragma once

#include <JuceHeader.h>
#include "../util/ParamIDs.h"
#include "Theme.h"

class SpectralDynamicsPanel final : public juce::Component
{
public:
    explicit SpectralDynamicsPanel(juce::AudioProcessorValueTreeState& state);
    void setTheme(const ThemeColors& newTheme);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& parameters;

    juce::Label titleLabel;
    juce::ToggleButton enableButton;
    juce::Slider thresholdSlider;
    juce::Slider ratioSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ButtonAttachment> enableAttachment;
    std::unique_ptr<SliderAttachment> thresholdAttachment;
    std::unique_ptr<SliderAttachment> ratioAttachment;
    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;

    ThemeColors theme = makeDarkTheme();
};
