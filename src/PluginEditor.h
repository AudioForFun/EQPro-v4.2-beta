#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/AnalyzerComponent.h"
#include "ui/BandControlsPanel.h"
#include "ui/MetersComponent.h"
#include "ui/CorrelationComponent.h"
#include "ui/Theme.h"
#include "ui/LookAndFeel.h"
#include "ui/SpectralDynamicsPanel.h"
#include "util/Version.h"

class EQProAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit EQProAudioProcessorEditor(EQProAudioProcessor&);
    ~EQProAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    void timerCallback() override;
    void refreshChannelLayout();

    EQProAudioProcessor& processorRef;

    juce::ToggleButton globalBypassButton;
    juce::Label globalMixLabel;
    juce::Slider globalMixSlider;
    juce::Label headerLabel;
    juce::Label versionLabel;
    juce::ComboBox channelSelector;
    juce::Label channelLabel;
    juce::Label phaseLabel;
    juce::ComboBox phaseModeBox;
    juce::Label qualityLabel;
    juce::ComboBox linearQualityBox;
    juce::Label windowLabel;
    juce::ComboBox linearWindowBox;
    juce::Label oversamplingLabel;
    juce::ComboBox oversamplingBox;
    juce::Label outputTrimLabel;
    juce::Slider outputTrimSlider;
    juce::Label characterLabel;
    juce::ComboBox characterBox;
    juce::Label qModeLabel;
    juce::ComboBox qModeBox;
    juce::Label qAmountLabel;
    juce::Slider qAmountSlider;
    juce::Label autoGainLabel;
    juce::ToggleButton autoGainToggle;
    juce::Slider gainScaleSlider;
    juce::ToggleButton phaseInvertToggle;
    juce::Label themeLabel;
    juce::ComboBox themeBox;
    ThemeColors theme = makeDarkTheme();
    juce::Label processingSectionLabel;
    juce::Label presetSectionLabel;
    juce::Label snapshotSectionLabel;
    juce::Label channelSectionLabel;
    juce::Label analyzerSectionLabel;
    juce::Label analyzerRangeLabel;
    juce::Label analyzerSpeedLabel;
    juce::Label analyzerViewLabel;
    juce::ComboBox analyzerRangeBox;
    juce::ComboBox analyzerSpeedBox;
    juce::ComboBox analyzerViewBox;
    juce::ToggleButton analyzerFreezeToggle;
    juce::ToggleButton analyzerExternalToggle;
    juce::ToggleButton smartSoloToggle;
    juce::ToggleButton showSpectralToggle;
    juce::Label midiSectionLabel;
    juce::ToggleButton midiLearnToggle;
    juce::ComboBox midiTargetBox;
    EQProLookAndFeel lookAndFeel;
    juce::Label applyLabel;
    juce::ComboBox applyTargetBox;
    juce::ToggleButton presetDeltaToggle;
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TextButton savePresetButton;
    juce::TextButton loadPresetButton;
    juce::TextButton copyInstanceButton;
    juce::TextButton pasteInstanceButton;
    juce::Label presetBrowserLabel;
    juce::ComboBox presetBrowserBox;
    juce::ToggleButton favoriteToggle;
    juce::TextButton refreshPresetsButton;
    std::unique_ptr<juce::FileChooser> saveChooser;
    std::unique_ptr<juce::FileChooser> loadChooser;
    juce::TextButton undoButton;
    juce::TextButton redoButton;
    juce::TextButton snapshotAButton;
    juce::TextButton snapshotBButton;
    juce::TextButton snapshotCButton;
    juce::TextButton snapshotDButton;
    juce::TextButton storeAButton;
    juce::TextButton storeBButton;
    juce::TextButton storeCButton;
    juce::TextButton storeDButton;
    juce::ComboBox snapshotMenu;
    juce::TextButton snapshotRecallButton;
    juce::TextButton snapshotStoreButton;
    juce::Label correlationLabel;
    juce::ComboBox correlationBox;
    juce::Label layoutLabel;
    juce::Label layoutValueLabel;
    juce::ToggleButton msViewToggle;
    MetersComponent meters;
    AnalyzerComponent analyzer;
    BandControlsPanel bandControls;
    SpectralDynamicsPanel spectralPanel;
    CorrelationComponent correlation;
    juce::ResizableCornerComponent resizer { this, &resizeConstrainer };
    juce::ComponentBoundsConstrainer resizeConstrainer;
    juce::OpenGLContext openGLContext;
    juce::Image backgroundNoise;

    bool debugVisible = false;
    bool pendingWindowRescue = true;
    int windowRescueTicks = 0;

    int selectedBand = 0;
    int selectedChannel = 0;
    std::vector<juce::String> cachedChannelNames;
    juce::String cachedLayoutDescription;

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ButtonAttachment> globalBypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> globalMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> phaseModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> linearQualityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> linearWindowAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputTrimAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> characterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainScaleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> phaseInvertAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> analyzerRangeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> analyzerSpeedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> analyzerViewAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> analyzerFreezeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> analyzerExternalAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiLearnAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> smartSoloAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> midiTargetAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQProAudioProcessorEditor)
};
