#pragma once

#include <JuceHeader.h>
#include <optional>
#include <atomic>
#include "../util/ParamIDs.h"
#include "Theme.h"

class EQProAudioProcessor;

class BandControlsPanel final : public juce::Component,
                                private juce::Timer
{
public:
    explicit BandControlsPanel(EQProAudioProcessor& processor);

    void setSelectedBand(int channelIndex, int bandIndex);
    void setTheme(const ThemeColors& newTheme);
    void setMsEnabled(bool enabled);
    std::function<void(int)> onBandNavigate;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    void updateAttachments();
    void updateTypeUi();
    int getCurrentTypeIndex() const;
    void updateFilterButtonsFromType(int typeIndex);
    void copyBandState();
    void pasteBandState();
    void mirrorToLinkedChannel(const juce::String& suffix, float value);
    bool isBandExisting(int bandIndex) const;
    int findNextExisting(int startIndex, int direction) const;

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
        float mix = 100.0f;
        float dynEnable = 0.0f;
        float dynMode = 0.0f;
        float dynThresh = -24.0f;
        float dynAttack = 20.0f;
        float dynRelease = 200.0f;
        float dynAuto = 1.0f;
    };

    EQProAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& parameters;
    int selectedChannel = 0;
    int selectedBand = 0;

    juce::Label titleLabel;
    juce::TextButton resetButton;
    juce::TextButton defaultButton;
    juce::TextButton deleteButton;
    juce::TextButton prevBandButton;
    juce::TextButton nextBandButton;
    juce::Label freqLabel;
    juce::Label gainLabel;
    juce::Label qLabel;
    juce::Slider freqSlider;
    juce::Slider gainSlider;
    juce::Slider qSlider;
    juce::Label typeLabel;
    std::array<juce::ToggleButton, 10> filterButtons;
    juce::Label msLabel;
    juce::ComboBox msBox;
    juce::Label slopeLabel;
    juce::Slider slopeSlider;
    juce::Label mixLabel;
    juce::Slider mixSlider;
    juce::ToggleButton bypassButton;
    juce::TextButton copyButton;
    juce::TextButton pasteButton;
    juce::ToggleButton soloButton;
    juce::ToggleButton tiltDirToggle;
    juce::Label dynamicLabel;
    juce::ToggleButton dynEnableToggle;
    juce::TextButton dynUpButton;
    juce::TextButton dynDownButton;
    juce::Label thresholdLabel;
    juce::Slider thresholdSlider;
    juce::Label attackLabel;
    juce::Slider attackSlider;
    juce::Label releaseLabel;
    juce::Slider releaseSlider;
    juce::ToggleButton autoScaleToggle;
    juce::Rectangle<float> detectorMeterBounds;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> freqAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment;
    std::unique_ptr<SliderAttachment> qAttachment;
    std::unique_ptr<ComboBoxAttachment> msAttachment;
    std::unique_ptr<SliderAttachment> slopeAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<ButtonAttachment> soloAttachment;
    std::unique_ptr<ButtonAttachment> dynEnableAttachment;
    std::unique_ptr<ComboBoxAttachment> dynModeAttachment;
    std::unique_ptr<SliderAttachment> dynThresholdAttachment;
    std::unique_ptr<SliderAttachment> dynAttackAttachment;
    std::unique_ptr<SliderAttachment> dynReleaseAttachment;
    std::unique_ptr<ButtonAttachment> dynAutoAttachment;

    ThemeColors theme = makeDarkTheme();
    bool msEnabled = true;
    std::optional<BandState> clipboard;
    float detectorDb = -60.0f;

    void resetSelectedBand(bool shouldBypass);
};
