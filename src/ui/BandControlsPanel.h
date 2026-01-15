#pragma once

#include <JuceHeader.h>
#include <optional>
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
    void setLinkPairs(const juce::StringArray& names,
                      const std::vector<std::pair<int, int>>& pairs);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void updateAttachments();
    void updateTypeUi();
    void copyBandState();
    void pasteBandState();
    void mirrorToLinkedChannel(const juce::String& suffix, float value);
    void timerCallback() override;

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
        float dynEnable = 0.0f;
        float dynMode = 0.0f;
        float dynThresh = -24.0f;
        float dynAttack = 20.0f;
        float dynRelease = 200.0f;
        float dynMix = 100.0f;
        float dynSource = 0.0f;
        float dynFilter = 1.0f;
    };

    EQProAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& parameters;
    int selectedChannel = 0;
    int selectedBand = 0;

    juce::Label titleLabel;
    juce::Slider freqSlider;
    juce::Slider gainSlider;
    juce::Slider qSlider;
    juce::ComboBox typeBox;
    juce::ComboBox msBox;
    juce::Slider slopeSlider;
    juce::ToggleButton bypassButton;
    juce::TextButton copyButton;
    juce::TextButton pasteButton;
    juce::ToggleButton linkButton;
    juce::Label linkPairLabel;
    juce::ComboBox linkPairBox;
    juce::ToggleButton soloButton;
    juce::ToggleButton dynEnableButton;
    juce::ComboBox dynModeBox;
    juce::ComboBox dynSourceBox;
    juce::ToggleButton dynFilterButton;
    juce::Slider thresholdSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider dynMixSlider;
    float dynMeterDb = 0.0f;
    juce::StringArray linkPairNames;
    std::vector<std::pair<int, int>> linkPairIndices;

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
    std::unique_ptr<ButtonAttachment> dynEnableAttachment;
    std::unique_ptr<ComboBoxAttachment> dynModeAttachment;
    std::unique_ptr<ComboBoxAttachment> dynSourceAttachment;
    std::unique_ptr<ButtonAttachment> dynFilterAttachment;
    std::unique_ptr<SliderAttachment> thresholdAttachment;
    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<SliderAttachment> dynMixAttachment;

    ThemeColors theme = makeDarkTheme();
    bool msEnabled = true;
    bool linkEnabled = false;
    std::optional<BandState> clipboard;
};
