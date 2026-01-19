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
    ~BandControlsPanel() override;

    void setSelectedBand(int channelIndex, int bandIndex);
    void setChannelNames(const std::vector<juce::String>& names);
    void setTheme(const ThemeColors& newTheme);
    void setMsEnabled(bool enabled);
    std::function<void(int)> onBandNavigate;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;

private:
    struct BandSelectButton final : public juce::TextButton
    {
        std::function<void()> onDoubleClick;

        void mouseDoubleClick(const juce::MouseEvent& event) override
        {
            juce::TextButton::mouseDoubleClick(event);
            if (onDoubleClick)
                onDoubleClick();
        }
    };

    struct BandKnob final : public juce::Slider
    {
        std::function<void()> onDoubleClick;

        void mouseDoubleClick(const juce::MouseEvent& event) override
        {
            juce::Slider::mouseDoubleClick(event);
            if (onDoubleClick)
                onDoubleClick();
        }
    };

    struct SoloToggleButton final : public juce::ToggleButton
    {
        std::function<void()> onDoubleClick;

        void mouseDoubleClick(const juce::MouseEvent& event) override
        {
            juce::ToggleButton::mouseDoubleClick(event);
            if (onDoubleClick)
                onDoubleClick();
        }
    };
    struct CompactComboLookAndFeel final : public juce::LookAndFeel_V4
    {
        juce::Font getComboBoxFont(juce::ComboBox&) override
        {
            return juce::Font(11.0f);
        }

        juce::Font getPopupMenuFont() override
        {
            return juce::Font(11.0f);
        }
    };

    void updateAttachments();
    void updateTypeUi();
    int getCurrentTypeIndex() const;
    void copyBandState();
    void pasteBandState();
    void mirrorToLinkedChannel(const juce::String& suffix, float value);
    bool isBandExisting(int bandIndex) const;
    int findNextExisting(int startIndex, int direction) const;
    void updateMsChoices();
    void syncMsSelectionFromParam();
    int getMsParamValue() const;
    void updateBandKnobColours();
    void ensureBandActiveFromEdit();
    void pushUiStateToParams();

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
        float dynExternal = 0.0f;
    };

    EQProAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& parameters;
    int selectedChannel = 0;
    int selectedBand = 0;

    juce::Label titleLabel;
    juce::Label eqSectionLabel;
    juce::TextButton defaultButton;
    std::array<BandSelectButton, ParamIDs::kBandsPerChannel> bandSelectButtons;
    std::array<SoloToggleButton, ParamIDs::kBandsPerChannel> bandSoloButtons;
    juce::Label freqLabel;
    juce::Label gainLabel;
    juce::Label qLabel;
    BandKnob freqSlider;
    BandKnob gainSlider;
    BandKnob qSlider;
    juce::Label typeLabel;
    juce::ComboBox typeBox;
    juce::Label msLabel;
    juce::ComboBox msBox;
    juce::Label slopeLabel;
    juce::ComboBox slopeBox;
    juce::Label mixLabel;
    BandKnob mixSlider;
    juce::TextButton copyButton;
    juce::TextButton pasteButton;
    juce::ToggleButton dynEnableToggle;
    juce::TextButton dynUpButton;
    juce::TextButton dynDownButton;
    juce::ToggleButton dynExternalToggle;
    juce::Label thresholdLabel;
    BandKnob thresholdSlider;
    juce::Label attackLabel;
    BandKnob attackSlider;
    juce::Label releaseLabel;
    BandKnob releaseSlider;
    juce::ToggleButton autoScaleToggle;
    juce::Rectangle<float> detectorMeterBounds;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> freqAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment;
    std::unique_ptr<SliderAttachment> qAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<ButtonAttachment> dynEnableAttachment;
    std::unique_ptr<ComboBoxAttachment> dynModeAttachment;
    std::unique_ptr<SliderAttachment> dynThresholdAttachment;
    std::unique_ptr<SliderAttachment> dynAttackAttachment;
    std::unique_ptr<SliderAttachment> dynReleaseAttachment;
    std::unique_ptr<ButtonAttachment> dynAutoAttachment;
    std::unique_ptr<ButtonAttachment> dynExternalAttachment;

    juce::RangedAudioParameter* freqParam = nullptr;
    juce::RangedAudioParameter* gainParam = nullptr;
    juce::RangedAudioParameter* qParam = nullptr;
    juce::RangedAudioParameter* mixParam = nullptr;
    juce::RangedAudioParameter* dynThreshParam = nullptr;
    juce::RangedAudioParameter* dynAttackParam = nullptr;
    juce::RangedAudioParameter* dynReleaseParam = nullptr;

    ThemeColors theme = makeDarkTheme();
    CompactComboLookAndFeel compactComboLookAndFeel;
    bool msEnabled = true;
    bool suppressParamCallbacks = false;
    std::optional<BandState> clipboard;
    float detectorDb = -60.0f;
    std::vector<juce::String> channelNames;
    std::vector<int> msChoiceMap;

    void resetSelectedBand();
    void updateComboBoxWidths();
    int comboWidthType = 0;
    int comboWidthMs = 0;
    int comboWidthSlope = 0;
};
