#pragma once

#include <JuceHeader.h>
#include <optional>
#include "../util/ParamIDs.h"
#include "Theme.h"

class EQProAudioProcessor;

class BandControlsPanel final : public juce::Component
{
public:
    explicit BandControlsPanel(EQProAudioProcessor& processor);

    void setSelectedBand(int channelIndex, int bandIndex);
    void setTheme(const ThemeColors& newTheme);
    void setMsEnabled(bool enabled);
    std::function<void(int)> onBandNavigate;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void updateAttachments();
    void updateTypeUi();
    void copyBandState();
    void pasteBandState();
    void mirrorToLinkedChannel(const juce::String& suffix, float value);

    struct BandState
    {
        float freq = 1000.0f;
        float gain = 0.0f;
        float q = 0.707f;
        float type = 0.0f;
        float bypass = 0.0f;
        float ms = 0.0f;
        float slope = 1.0f;
        float solo = 0.0f;
    };

    EQProAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& parameters;
    int selectedChannel = 0;
    int selectedBand = 0;

    juce::Label titleLabel;
    juce::TextButton resetButton;
    juce::TextButton deleteButton;
    juce::TextButton prevBandButton;
    juce::TextButton nextBandButton;
    juce::Label freqLabel;
    juce::Label gainLabel;
    juce::Label qLabel;
    juce::Slider freqSlider;
    juce::Slider gainSlider;
    juce::Slider qSlider;
    juce::Label qModeLabel;
    juce::ComboBox qModeBox;
    juce::Label qAmountLabel;
    juce::Slider qAmountSlider;
    juce::Label typeLabel;
    juce::ComboBox typeBox;
    juce::Label msLabel;
    juce::ComboBox msBox;
    juce::Label slopeLabel;
    juce::Slider slopeSlider;
    juce::ToggleButton bypassButton;
    juce::TextButton copyButton;
    juce::TextButton pasteButton;
    juce::ToggleButton soloButton;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> freqAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment;
    std::unique_ptr<SliderAttachment> qAttachment;
    std::unique_ptr<ComboBoxAttachment> typeAttachment;
    std::unique_ptr<ComboBoxAttachment> msAttachment;
    std::unique_ptr<SliderAttachment> slopeAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<ButtonAttachment> soloAttachment;
    std::unique_ptr<ComboBoxAttachment> qModeAttachment;
    std::unique_ptr<SliderAttachment> qAmountAttachment;

    ThemeColors theme = makeDarkTheme();
    bool msEnabled = true;
    std::optional<BandState> clipboard;

    void resetSelectedBand(bool shouldBypass);
};
