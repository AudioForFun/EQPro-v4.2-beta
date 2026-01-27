#pragma once

#include <JuceHeader.h>
#include <optional>
#include <atomic>
#include "../util/ParamIDs.h"
#include "Theme.h"

class EQProAudioProcessor;

// Main per-band control panel (knobs, type, slope, channel, reset/copy).
class BandControlsPanel final : public juce::Component,
                                private juce::Timer
{
public:
    explicit BandControlsPanel(EQProAudioProcessor& processor);
    ~BandControlsPanel() override;

    // Selects a band/channel and refreshes UI.
    void setSelectedBand(int channelIndex, int bandIndex);
    // Provide channel labels from processor layout.
    void setChannelNames(const std::vector<juce::String>& names);
    // Apply theme palette.
    void setTheme(const ThemeColors& newTheme);
    // Enables/disables MS/channel target controls.
    void setMsEnabled(bool enabled);
    std::function<void(int)> onBandNavigate;

    void paint(juce::Graphics&) override;
    void resized() override;
    // Periodic UI sync + solo validation.
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

        bool hitTest(int x, int y) override
        {
            const auto expanded = getLocalBounds().toFloat().expanded(4.0f);
            return expanded.contains(static_cast<float>(x), static_cast<float>(y));
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

        bool hitTest(int x, int y) override
        {
            const auto expanded = getLocalBounds().toFloat().expanded(6.0f);
            return expanded.contains(static_cast<float>(x), static_cast<float>(y));
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

        bool hitTest(int x, int y) override
        {
            const auto expanded = getLocalBounds().toFloat().expanded(4.0f);
            return expanded.contains(static_cast<float>(x), static_cast<float>(y));
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
    
    // v4.4 beta: Separate LookAndFeel for slope dropdown with larger font
    struct SlopeComboLookAndFeel final : public juce::LookAndFeel_V4
    {
        juce::Font getComboBoxFont(juce::ComboBox&) override
        {
            return juce::Font(12.5f);  // Larger font for slope dropdown
        }

        juce::Font getPopupMenuFont() override
        {
            return juce::Font(12.5f);  // Larger font for slope dropdown popup
        }
    };

    // v4.4 beta: Harmonic layer system
    enum class LayerType { EQ, Harmonic };
    
    void updateAttachments();
    void syncUiFromParams();
    void updateTypeUi();
    void setLayer(LayerType layer);  // v4.4 beta: Switch between EQ and Harmonic layers
    void updateLayerVisibility();  // v4.4 beta: Update visibility based on current layer
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
    void cacheBandFromUi(int channelIndex, int bandIndex);
    void cacheBandFromParams(int channelIndex, int bandIndex);
    void refreshCacheFromParams(int channelIndex);
    void applyCachedBandToParams(int channelIndex);
    void restoreBandFromCache();

    struct BandState
    {
        // Cached per-band parameters for UI persistence.
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
        // v4.4 beta: Harmonic layer parameters
        float odd = 0.0f;
        float mixOdd = 100.0f;
        float even = 0.0f;
        float mixEven = 100.0f;
        float harmonicBypass = 0.0f;  // v4.4 beta: Bypass for harmonic layer (per band)
    };

    EQProAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& parameters;
    int selectedChannel = 0;
    int selectedBand = 0;

    juce::Label titleLabel;
    juce::Label eqSectionLabel;
    juce::TextButton defaultButton;
    juce::TextButton resetAllButton;
    juce::TextButton prevBandButton;
    juce::TextButton nextBandButton;
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
    
    // v4.4 beta: Harmonic layer controls
    LayerType currentLayer { LayerType::EQ };
    juce::ToggleButton eqLayerToggle;
    juce::ToggleButton harmonicLayerToggle;
    
    juce::Label oddLabel;
    BandKnob oddHarmonicSlider;
    juce::Label mixOddLabel;
    BandKnob mixOddSlider;
    juce::Label evenLabel;
    BandKnob evenHarmonicSlider;
    juce::Label mixEvenLabel;
    BandKnob mixEvenSlider;
    juce::ToggleButton harmonicBypassToggle;  // v4.4 beta: Bypass for harmonic layer
    
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
    std::unique_ptr<SliderAttachment> oddHarmonicAttachment;
    std::unique_ptr<SliderAttachment> mixOddAttachment;
    std::unique_ptr<SliderAttachment> evenHarmonicAttachment;
    std::unique_ptr<SliderAttachment> mixEvenAttachment;
    std::unique_ptr<ButtonAttachment> harmonicBypassAttachment;  // v4.4 beta: Harmonic bypass attachment
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
    SlopeComboLookAndFeel slopeComboLookAndFeel;  // v4.4 beta: Larger font for slope dropdown
    bool msEnabled = true;
    bool suppressParamCallbacks = false;
    bool hasBeenResized = false;  // v4.4 beta: Defer timer start until after first resize
    std::optional<BandState> clipboard;
    float detectorDb = -60.0f;
    // Lightweight hover/selection fades for the band strip.
    std::array<juce::SmoothedValue<float>, ParamIDs::kBandsPerChannel> bandHoverFade {};
    std::array<juce::SmoothedValue<float>, ParamIDs::kBandsPerChannel> bandSelectFade {};
    std::array<juce::SmoothedValue<float>, ParamIDs::kBandsPerChannel> bandActiveFade {};
    float selectedBandGlow = 0.0f;
    std::vector<juce::String> channelNames;
    std::vector<int> msChoiceMap;
    std::array<std::array<BandState, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels> bandStateCache {};
    std::array<std::array<bool, ParamIDs::kBandsPerChannel>, ParamIDs::kMaxChannels> bandStateValid {};

    void resetSelectedBand();
    void resetAllBands();
    void updateComboBoxWidths();
    int comboWidthType = 0;
    int comboWidthMs = 0;
    int comboWidthSlope = 0;
};
