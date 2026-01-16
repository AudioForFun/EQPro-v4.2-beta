#include "PluginEditor.h"
#include "util/ParamIDs.h"

namespace
{
constexpr int kEditorWidth = 1200;
constexpr int kEditorHeight = 800;
constexpr int kOuterMargin = 16;
constexpr int kLeftPanelWidth = 0;
constexpr int kRightPanelWidth = 180;
constexpr float kLabelFontSize = 12.0f;
constexpr float kHeaderFontSize = 20.0f;
} // namespace

EQProAudioProcessorEditor::EQProAudioProcessorEditor(EQProAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      meters(p),
      analyzer(p),
      bandControls(p),
      spectralPanel(p.getParameters()),
      correlation(p)
{
    setLookAndFeel(&lookAndFeel);
    openGLContext.setContinuousRepainting(false);
    openGLContext.attachTo(*this);
    analyzer.setInteractive(false);

    headerLabel.setText("EQ Pro", juce::dontSendNotification);
    headerLabel.setJustificationType(juce::Justification::centredLeft);
    headerLabel.setFont(juce::Font(kHeaderFontSize, juce::Font::bold));
    headerLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe5e7eb));
    addAndMakeVisible(headerLabel);

    versionLabel.setText(Version::displayString(), juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::centredRight);
    versionLabel.setFont(juce::Font(12.0f, juce::Font::plain));
    versionLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(versionLabel);

    globalBypassButton.setButtonText("Global Bypass");
    globalBypassButton.setColour(juce::ToggleButton::textColourId,
                                 juce::Colour(0xffcbd5e1));
    addAndMakeVisible(globalBypassButton);
    globalBypassAttachment =
        std::make_unique<ButtonAttachment>(processorRef.getParameters(), ParamIDs::globalBypass,
                                           globalBypassButton);

    globalMixLabel.setText("Para General Mix", juce::dontSendNotification);
    globalMixLabel.setJustificationType(juce::Justification::centredLeft);
    globalMixLabel.setFont(kLabelFontSize);
    globalMixLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(globalMixLabel);

    globalMixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    globalMixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 18);
    globalMixSlider.setTextBoxIsEditable(true);
    globalMixSlider.setTextValueSuffix(" %");
    globalMixSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff38bdf8));
    globalMixSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe2e8f0));
    globalMixSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(globalMixSlider);
    globalMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getParameters(), ParamIDs::globalMix, globalMixSlider);

    phaseLabel.setText("Processing Mode", juce::dontSendNotification);
    phaseLabel.setJustificationType(juce::Justification::centredLeft);
    phaseLabel.setFont(kLabelFontSize);
    phaseLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(phaseLabel);

    phaseModeBox.addItemList(juce::StringArray("Real-time", "Natural", "Linear"), 1);
    phaseModeBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0f141a));
    phaseModeBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    phaseModeBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(phaseModeBox);
    phaseModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::phaseMode, phaseModeBox);

    qualityLabel.setText("Quality", juce::dontSendNotification);
    qualityLabel.setJustificationType(juce::Justification::centredLeft);
    qualityLabel.setFont(kLabelFontSize);
    qualityLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(qualityLabel);

    linearQualityBox.addItemList(
        juce::StringArray("Low", "Medium", "High", "Very High", "Intensive"), 1);
    linearQualityBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0f141a));
    linearQualityBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    linearQualityBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(linearQualityBox);
    linearQualityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::linearQuality, linearQualityBox);

    windowLabel.setText("Window", juce::dontSendNotification);
    windowLabel.setJustificationType(juce::Justification::centredLeft);
    windowLabel.setFont(kLabelFontSize);
    windowLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(windowLabel);

    linearWindowBox.addItemList(juce::StringArray("Hann", "Blackman", "Kaiser"), 1);
    linearWindowBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0f141a));
    linearWindowBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    linearWindowBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(linearWindowBox);
    linearWindowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::linearWindow, linearWindowBox);
    windowLabel.setVisible(false);
    linearWindowBox.setVisible(false);

    oversamplingLabel.setText("OS", juce::dontSendNotification);
    oversamplingLabel.setJustificationType(juce::Justification::centredLeft);
    oversamplingLabel.setFont(kLabelFontSize);
    oversamplingLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(oversamplingLabel);

    oversamplingBox.addItemList(juce::StringArray("Off", "2x", "4x"), 1);
    oversamplingBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0f141a));
    oversamplingBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    oversamplingBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(oversamplingBox);
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::oversampling, oversamplingBox);
    oversamplingLabel.setVisible(false);
    oversamplingBox.setVisible(false);

    characterLabel.setText("Character", juce::dontSendNotification);
    characterLabel.setJustificationType(juce::Justification::centredLeft);
    characterLabel.setFont(kLabelFontSize);
    characterLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(characterLabel);

    characterBox.addItemList(juce::StringArray("Off", "Gentle", "Warm"), 1);
    characterBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0f141a));
    characterBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    characterBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(characterBox);
    characterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::characterMode, characterBox);

    qModeLabel.setText("Q", juce::dontSendNotification);
    qModeLabel.setJustificationType(juce::Justification::centredLeft);
    qModeLabel.setFont(kLabelFontSize);
    qModeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(qModeLabel);

    qModeBox.addItemList(juce::StringArray("Constant", "Proportional"), 1);
    qModeBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0f141a));
    qModeBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    qModeBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(qModeBox);
    qModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::qMode, qModeBox);

    qAmountLabel.setText("Q Amt", juce::dontSendNotification);
    qAmountLabel.setJustificationType(juce::Justification::centredLeft);
    qAmountLabel.setFont(kLabelFontSize);
    qAmountLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(qAmountLabel);

    qAmountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    qAmountSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 18);
    qAmountSlider.setTextBoxIsEditable(true);
    qAmountSlider.setTextValueSuffix(" %");
    addAndMakeVisible(qAmountSlider);
    qAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getParameters(), ParamIDs::qModeAmount, qAmountSlider);

    autoGainLabel.setText("Auto Gain", juce::dontSendNotification);
    autoGainLabel.setJustificationType(juce::Justification::centredLeft);
    autoGainLabel.setFont(kLabelFontSize);
    autoGainLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(autoGainLabel);

    autoGainToggle.setButtonText("Enable");
    autoGainToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(autoGainToggle);
    autoGainAttachment = std::make_unique<ButtonAttachment>(
        processorRef.getParameters(), ParamIDs::autoGainEnable, autoGainToggle);

    gainScaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    gainScaleSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    gainScaleSlider.setTextBoxIsEditable(true);
    gainScaleSlider.setTextValueSuffix(" %");
    addAndMakeVisible(gainScaleSlider);
    gainScaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getParameters(), ParamIDs::gainScale, gainScaleSlider);

    phaseInvertToggle.setButtonText("Phase Invert");
    phaseInvertToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(phaseInvertToggle);
    phaseInvertAttachment = std::make_unique<ButtonAttachment>(
        processorRef.getParameters(), ParamIDs::phaseInvert, phaseInvertToggle);

    analyzerRangeLabel.setText("Range", juce::dontSendNotification);
    analyzerRangeLabel.setJustificationType(juce::Justification::centredLeft);
    analyzerRangeLabel.setFont(kLabelFontSize);
    analyzerRangeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(analyzerRangeLabel);

    analyzerRangeBox.addItemList(juce::StringArray("3 dB", "6 dB", "12 dB", "30 dB"), 1);
    analyzerRangeBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0f141a));
    analyzerRangeBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    analyzerRangeBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(analyzerRangeBox);
    analyzerRangeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::analyzerRange, analyzerRangeBox);

    analyzerSpeedLabel.setText("Speed", juce::dontSendNotification);
    analyzerSpeedLabel.setJustificationType(juce::Justification::centredLeft);
    analyzerSpeedLabel.setFont(kLabelFontSize);
    analyzerSpeedLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(analyzerSpeedLabel);

    analyzerSpeedBox.addItemList(juce::StringArray("Slow", "Normal", "Fast"), 1);
    analyzerSpeedBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0f141a));
    analyzerSpeedBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    analyzerSpeedBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(analyzerSpeedBox);
    analyzerSpeedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::analyzerSpeed, analyzerSpeedBox);

    analyzerViewLabel.setText("View", juce::dontSendNotification);
    analyzerViewLabel.setJustificationType(juce::Justification::centredLeft);
    analyzerViewLabel.setFont(kLabelFontSize);
    analyzerViewLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(analyzerViewLabel);

    analyzerViewBox.addItemList(juce::StringArray("Both", "Pre", "Post"), 1);
    analyzerViewBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0f141a));
    analyzerViewBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    analyzerViewBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(analyzerViewBox);
    analyzerViewAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::analyzerView, analyzerViewBox);

    analyzerFreezeToggle.setButtonText("Freeze");
    analyzerFreezeToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(analyzerFreezeToggle);
    analyzerFreezeAttachment = std::make_unique<ButtonAttachment>(
        processorRef.getParameters(), ParamIDs::analyzerFreeze, analyzerFreezeToggle);

    analyzerExternalToggle.setButtonText("External");
    analyzerExternalToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(analyzerExternalToggle);
    analyzerExternalAttachment = std::make_unique<ButtonAttachment>(
        processorRef.getParameters(), ParamIDs::analyzerExternal, analyzerExternalToggle);

    smartSoloToggle.setButtonText("Smart Solo");
    smartSoloToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(smartSoloToggle);
    smartSoloAttachment = std::make_unique<ButtonAttachment>(
        processorRef.getParameters(), ParamIDs::smartSolo, smartSoloToggle);

    showSpectralToggle.setButtonText("Spectral");
    showSpectralToggle.setToggleState(true, juce::dontSendNotification);
    showSpectralToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(showSpectralToggle);
    showSpectralToggle.onClick = [this]()
    {
        spectralPanel.setVisible(showSpectralToggle.getToggleState());
        resized();
    };


    correlation.setVisible(true);

    midiLearnToggle.setButtonText("Learn");
    midiLearnToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(midiLearnToggle);
    midiLearnAttachment = std::make_unique<ButtonAttachment>(
        processorRef.getParameters(), ParamIDs::midiLearn, midiLearnToggle);

    midiTargetBox.addItemList(juce::StringArray("Gain", "Freq", "Q"), 1);
    midiTargetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0f141a));
    midiTargetBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    midiTargetBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(midiTargetBox);
    midiTargetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::midiTarget, midiTargetBox);

    outputTrimLabel.setText("Output", juce::dontSendNotification);
    outputTrimLabel.setJustificationType(juce::Justification::centredLeft);
    outputTrimLabel.setFont(kLabelFontSize);
    outputTrimLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(outputTrimLabel);

    outputTrimSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    outputTrimSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    outputTrimSlider.setTextBoxIsEditable(true);
    outputTrimSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(outputTrimSlider);
    outputTrimAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getParameters(), ParamIDs::outputTrim, outputTrimSlider);

    auto updateQualityEnabled = [this]
    {
        const int mode = phaseModeBox.getSelectedItemIndex();
        linearQualityBox.setEnabled(mode == 2);
        linearWindowBox.setEnabled(mode != 0);
    };
    phaseModeBox.onChange = [updateQualityEnabled]
    {
        updateQualityEnabled();
    };
    updateQualityEnabled();


    auto initSectionLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(juce::Font(11.0f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, theme.accent);
        addAndMakeVisible(label);
    };

    initSectionLabel(processingSectionLabel, "PROCESSING");
    initSectionLabel(analyzerSectionLabel, "ANALYZER");
    initSectionLabel(midiSectionLabel, "MIDI");
    initSectionLabel(presetSectionLabel, "PRESETS");
    initSectionLabel(snapshotSectionLabel, "SNAPSHOTS");
    initSectionLabel(channelSectionLabel, "CHANNEL");

    themeLabel.setText("Theme", juce::dontSendNotification);
    themeLabel.setJustificationType(juce::Justification::centredLeft);
    themeLabel.setFont(kLabelFontSize);
    addAndMakeVisible(themeLabel);

    themeBox.addItemList(juce::StringArray("Dark", "Light"), 1);
    themeBox.setSelectedItemIndex(processorRef.getThemeMode(), juce::dontSendNotification);
    addAndMakeVisible(themeBox);
    themeBox.onChange = [this]
    {
        const int mode = themeBox.getSelectedItemIndex();
        processorRef.setThemeMode(mode);
        const ThemeColors newTheme = (mode == 0) ? makeDarkTheme() : makeLightTheme();

        theme = newTheme;
        lookAndFeel.setTheme(newTheme);
        analyzer.setTheme(newTheme);
        bandControls.setTheme(newTheme);
        spectralPanel.setTheme(newTheme);
        meters.setTheme(newTheme);
        correlation.setTheme(newTheme);

        headerLabel.setColour(juce::Label::textColourId, newTheme.text);
        versionLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        globalBypassButton.setColour(juce::ToggleButton::textColourId, newTheme.textMuted);
        globalMixLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        themeLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        phaseLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        qualityLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        windowLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        oversamplingLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        outputTrimLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        characterLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        autoGainLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        analyzerRangeLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        analyzerSpeedLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        applyLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        presetLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        presetBrowserLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        channelLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        correlationLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        layoutLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        layoutValueLabel.setColour(juce::Label::textColourId, newTheme.text);
        msViewToggle.setColour(juce::ToggleButton::textColourId, newTheme.textMuted);
        savePresetButton.setColour(juce::TextButton::textColourOffId, newTheme.textMuted);
        loadPresetButton.setColour(juce::TextButton::textColourOffId, newTheme.textMuted);
        copyInstanceButton.setColour(juce::TextButton::textColourOffId, newTheme.textMuted);
        pasteInstanceButton.setColour(juce::TextButton::textColourOffId, newTheme.textMuted);
        favoriteToggle.setColour(juce::ToggleButton::textColourId, newTheme.textMuted);
        refreshPresetsButton.setColour(juce::TextButton::textColourOffId, newTheme.textMuted);
        snapshotRecallButton.setColour(juce::TextButton::textColourOffId, newTheme.textMuted);
        snapshotStoreButton.setColour(juce::TextButton::textColourOffId, newTheme.textMuted);
        analyzerFreezeToggle.setColour(juce::ToggleButton::textColourId, newTheme.textMuted);
        analyzerExternalToggle.setColour(juce::ToggleButton::textColourId, newTheme.textMuted);
        smartSoloToggle.setColour(juce::ToggleButton::textColourId, newTheme.textMuted);
        autoGainToggle.setColour(juce::ToggleButton::textColourId, newTheme.textMuted);
        midiLearnToggle.setColour(juce::ToggleButton::textColourId, newTheme.textMuted);
        phaseInvertToggle.setColour(juce::ToggleButton::textColourId, newTheme.textMuted);
        gainScaleSlider.setColour(juce::Slider::trackColourId, newTheme.accent);
        gainScaleSlider.setColour(juce::Slider::textBoxTextColourId, newTheme.text);
        gainScaleSlider.setColour(juce::Slider::textBoxOutlineColourId, newTheme.panelOutline);
        globalMixSlider.setColour(juce::Slider::trackColourId, newTheme.accent);
        globalMixSlider.setColour(juce::Slider::textBoxTextColourId, newTheme.text);
        globalMixSlider.setColour(juce::Slider::textBoxOutlineColourId, newTheme.panelOutline);
        qModeLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        qAmountLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        qAmountSlider.setColour(juce::Slider::trackColourId, newTheme.accent);
        qAmountSlider.setColour(juce::Slider::textBoxTextColourId, newTheme.text);
        qAmountSlider.setColour(juce::Slider::textBoxOutlineColourId, newTheme.panelOutline);
        processingSectionLabel.setColour(juce::Label::textColourId, newTheme.accent);
        analyzerSectionLabel.setColour(juce::Label::textColourId, newTheme.accent);
        midiSectionLabel.setColour(juce::Label::textColourId, newTheme.accent);
        presetSectionLabel.setColour(juce::Label::textColourId, newTheme.accent);
        snapshotSectionLabel.setColour(juce::Label::textColourId, newTheme.accent);
        channelSectionLabel.setColour(juce::Label::textColourId, newTheme.accent);

        auto setComboTheme = [&newTheme](juce::ComboBox& box)
        {
            box.setColour(juce::ComboBox::backgroundColourId, newTheme.panel);
            box.setColour(juce::ComboBox::textColourId, newTheme.text);
            box.setColour(juce::ComboBox::outlineColourId, newTheme.panelOutline);
        };
        setComboTheme(phaseModeBox);
        setComboTheme(linearQualityBox);
        setComboTheme(linearWindowBox);
        setComboTheme(oversamplingBox);
        setComboTheme(qModeBox);
        setComboTheme(characterBox);
        setComboTheme(analyzerRangeBox);
        setComboTheme(analyzerSpeedBox);
        setComboTheme(midiTargetBox);
        setComboTheme(applyTargetBox);
        setComboTheme(presetBox);
        setComboTheme(presetBrowserBox);
        setComboTheme(channelSelector);
        setComboTheme(correlationBox);
        setComboTheme(themeBox);
        setComboTheme(snapshotMenu);

        repaint();
    };

    applyLabel.setText("Apply", juce::dontSendNotification);
    applyLabel.setJustificationType(juce::Justification::centredLeft);
    applyLabel.setFont(kLabelFontSize);
    applyLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(applyLabel);

    applyTargetBox.addItemList(juce::StringArray("Selected", "All"), 1);
    applyTargetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0f141a));
    applyTargetBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    applyTargetBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    applyTargetBox.setSelectedItemIndex(processorRef.getPresetApplyTarget(),
                                        juce::dontSendNotification);
    addAndMakeVisible(applyTargetBox);
    applyTargetBox.onChange = [this]
    {
        processorRef.setPresetApplyTarget(applyTargetBox.getSelectedItemIndex());
    };

    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setJustificationType(juce::Justification::centredLeft);
    presetLabel.setFont(kLabelFontSize);
    presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(presetLabel);

    presetBox.addItemList(juce::StringArray(
        "Flat",
        "Bass Boost",
        "Vocal",
        "Air",
        "Warm",
        "Bright",
        "Cut Low",
        "Cut High"), 1);
    presetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0f141a));
    presetBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    presetBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    presetBox.setSelectedItemIndex(processorRef.getPresetSelection(),
                                   juce::dontSendNotification);
    addAndMakeVisible(presetBox);
    presetBox.onChange = [this]
    {
        const int preset = presetBox.getSelectedItemIndex();
        if (preset < 0)
            return;

        processorRef.setPresetSelection(preset);
        auto& params = processorRef.getParameters();
        auto setParam = [&params](const juce::String& id, float value)
        {
            if (auto* param = params.getParameter(id))
                param->setValueNotifyingHost(param->convertTo0to1(value));
        };

        if (auto* undo = processorRef.getUndoManager())
            undo->beginNewTransaction("Apply Preset");

        auto applyPresetToChannel = [&](int ch)
        {
            for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
            {
                setParam(ParamIDs::bandParamId(ch, band, "bypass"), 1.0f);
                setParam(ParamIDs::bandParamId(ch, band, "gain"), 0.0f);
                setParam(ParamIDs::bandParamId(ch, band, "q"), 0.707f);
                setParam(ParamIDs::bandParamId(ch, band, "type"), 0.0f);
                setParam(ParamIDs::bandParamId(ch, band, "ms"), 0.0f);
            }

            auto enableBand = [&](int band, float freq, float gain, float q, int type)
            {
                setParam(ParamIDs::bandParamId(ch, band, "bypass"), 0.0f);
                setParam(ParamIDs::bandParamId(ch, band, "freq"), freq);
                setParam(ParamIDs::bandParamId(ch, band, "gain"), gain);
                setParam(ParamIDs::bandParamId(ch, band, "q"), q);
                setParam(ParamIDs::bandParamId(ch, band, "type"), static_cast<float>(type));
            };

            switch (preset)
            {
                case 0: // Flat
                    break;
                case 1: // Bass Boost
                    enableBand(0, 80.0f, 6.0f, 0.8f, 1);
                    enableBand(1, 250.0f, 2.0f, 1.0f, 0);
                    break;
                case 2: // Vocal
                    enableBand(0, 80.0f, 0.0f, 0.7f, 4);
                    enableBand(1, 1000.0f, 3.0f, 1.2f, 0);
                    enableBand(2, 3000.0f, 2.0f, 1.2f, 0);
                    break;
                case 3: // Air
                    enableBand(0, 12000.0f, 6.0f, 0.7f, 2);
                    break;
                case 4: // Warm
                    enableBand(0, 120.0f, 3.0f, 0.8f, 1);
                    enableBand(1, 400.0f, 1.5f, 1.0f, 0);
                    break;
                case 5: // Bright
                    enableBand(0, 6000.0f, 2.5f, 1.0f, 0);
                    enableBand(1, 12000.0f, 4.0f, 0.7f, 2);
                    break;
                case 6: // Cut Low
                    enableBand(0, 80.0f, 0.0f, 0.7f, 4);
                    break;
                case 7: // Cut High
                    enableBand(0, 12000.0f, 0.0f, 0.7f, 3);
                    break;
                default:
                    break;
            }
        };

        const int applyTarget = applyTargetBox.getSelectedItemIndex();
        const int channelCount = juce::jlimit(
            1, ParamIDs::kMaxChannels, processorRef.getTotalNumInputChannels());

        if (applyTarget == 1)
        {
            for (int ch = 0; ch < channelCount; ++ch)
                applyPresetToChannel(ch);
        }
        else
        {
            applyPresetToChannel(selectedChannel);
        }
    };

    savePresetButton.setButtonText("Save");
    savePresetButton.onClick = [this]
    {
        saveChooser = std::make_unique<juce::FileChooser>(
            "Save Preset",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.xml");
        saveChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                     | juce::FileBrowserComponent::canSelectFiles,
                                 [this](const juce::FileChooser& chooser)
                                 {
                                     const auto file = chooser.getResult();
                                     if (file != juce::File() && file.hasFileExtension("xml"))
                                     {
                                         if (auto xml = processorRef.getParameters().copyState().createXml())
                                             xml->writeTo(file, {});
                                     }
                                     saveChooser.reset();
                                 });
    };
    addAndMakeVisible(savePresetButton);

    loadPresetButton.setButtonText("Load");
    loadPresetButton.onClick = [this]
    {
        loadChooser = std::make_unique<juce::FileChooser>(
            "Load Preset",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.xml");
        loadChooser->launchAsync(juce::FileBrowserComponent::openMode
                                     | juce::FileBrowserComponent::canSelectFiles,
                                 [this](const juce::FileChooser& chooser)
                                 {
                                     const auto file = chooser.getResult();
                                     if (file != juce::File())
                                     {
                                         if (auto xml = juce::XmlDocument::parse(file))
                                             processorRef.getParameters().replaceState(juce::ValueTree::fromXml(*xml));
                                     }
                                     loadChooser.reset();
                                 });
    };
    addAndMakeVisible(loadPresetButton);

    copyInstanceButton.setButtonText("Copy");
    copyInstanceButton.onClick = [this]
    {
        processorRef.copyStateToClipboard();
    };
    addAndMakeVisible(copyInstanceButton);

    pasteInstanceButton.setButtonText("Paste");
    pasteInstanceButton.onClick = [this]
    {
        processorRef.pasteStateFromClipboard();
    };
    addAndMakeVisible(pasteInstanceButton);

    presetBrowserLabel.setText("Presets", juce::dontSendNotification);
    presetBrowserLabel.setJustificationType(juce::Justification::centredLeft);
    presetBrowserLabel.setFont(kLabelFontSize);
    addAndMakeVisible(presetBrowserLabel);

    addAndMakeVisible(presetBrowserBox);
    favoriteToggle.setButtonText("Fav");
    addAndMakeVisible(favoriteToggle);
    refreshPresetsButton.setButtonText("Refresh");
    addAndMakeVisible(refreshPresetsButton);

    auto refreshPresetBrowser = [this](bool keepSelection)
    {
        const auto presetDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                   .getChildFile("EQPro")
                                   .getChildFile("Presets");
        presetDir.createDirectory();

        const auto files = presetDir.findChildFiles(juce::File::findFiles, false, "*.xml");
        juce::StringArray favorites;
        favorites.addTokens(processorRef.getFavoritePresets(), ";", "");
        favorites.removeEmptyStrings();

        const auto previous = presetBrowserBox.getText();
        presetBrowserBox.clear();
        for (const auto& file : files)
        {
            const auto name = file.getFileNameWithoutExtension();
            const bool isFav = favorites.contains(name);
            presetBrowserBox.addItem((isFav ? "★ " : "") + name, presetBrowserBox.getNumItems() + 1);
        }

        if (keepSelection && previous.isNotEmpty())
            presetBrowserBox.setText(previous, juce::dontSendNotification);
        else if (presetBrowserBox.getNumItems() > 0)
            presetBrowserBox.setSelectedItemIndex(0, juce::dontSendNotification);

        const auto currentName = presetBrowserBox.getText().trimStart();
        favoriteToggle.setToggleState(favorites.contains(currentName), juce::dontSendNotification);
    };

    refreshPresetsButton.onClick = [refreshPresetBrowser]
    {
        refreshPresetBrowser(true);
    };

    presetBrowserBox.onChange = [this, refreshPresetBrowser]
    {
        const auto presetDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                   .getChildFile("EQPro")
                                   .getChildFile("Presets");
        const auto name = presetBrowserBox.getText().trimStart();
        const auto file = presetDir.getChildFile(name + ".xml");
        if (file.existsAsFile())
        {
            if (auto xml = juce::XmlDocument::parse(file))
                processorRef.getParameters().replaceState(juce::ValueTree::fromXml(*xml));
        }
        refreshPresetBrowser(true);
    };

    favoriteToggle.onClick = [this, refreshPresetBrowser]
    {
        const auto name = presetBrowserBox.getText().trimStart();
        if (name.isEmpty())
            return;

        juce::StringArray favorites;
        favorites.addTokens(processorRef.getFavoritePresets(), ";", "");
        favorites.removeEmptyStrings();
        if (favoriteToggle.getToggleState())
        {
            if (! favorites.contains(name))
                favorites.add(name);
        }
        else
        {
            favorites.removeString(name);
        }

        processorRef.setFavoritePresets(favorites.joinIntoString(";"));
        refreshPresetBrowser(true);
    };

    refreshPresetBrowser(false);

    undoButton.setButtonText("Undo");
    undoButton.onClick = [this]
    {
        if (auto* undo = processorRef.getUndoManager())
            undo->undo();
    };
    addAndMakeVisible(undoButton);

    redoButton.setButtonText("Redo");
    redoButton.onClick = [this]
    {
        if (auto* undo = processorRef.getUndoManager())
            undo->redo();
    };
    addAndMakeVisible(redoButton);

    snapshotAButton.setButtonText("A");
    snapshotAButton.onClick = [this]
    {
        processorRef.recallSnapshotA();
    };
    addAndMakeVisible(snapshotAButton);

    snapshotBButton.setButtonText("B");
    snapshotBButton.onClick = [this]
    {
        processorRef.recallSnapshotB();
    };
    addAndMakeVisible(snapshotBButton);

    snapshotCButton.setButtonText("C");
    snapshotCButton.onClick = [this]
    {
        processorRef.recallSnapshotC();
    };
    addAndMakeVisible(snapshotCButton);

    snapshotDButton.setButtonText("D");
    snapshotDButton.onClick = [this]
    {
        processorRef.recallSnapshotD();
    };
    addAndMakeVisible(snapshotDButton);

    storeAButton.setButtonText("Store A");
    storeAButton.onClick = [this]
    {
        processorRef.storeSnapshotA();
    };
    addAndMakeVisible(storeAButton);

    storeBButton.setButtonText("Store B");
    storeBButton.onClick = [this]
    {
        processorRef.storeSnapshotB();
    };
    addAndMakeVisible(storeBButton);

    storeCButton.setButtonText("Store C");
    storeCButton.onClick = [this]
    {
        processorRef.storeSnapshotC();
    };
    addAndMakeVisible(storeCButton);

    storeDButton.setButtonText("Store D");
    storeDButton.onClick = [this]
    {
        processorRef.storeSnapshotD();
    };
    addAndMakeVisible(storeDButton);

    snapshotMenu.addItemList(juce::StringArray("Snapshot A", "Snapshot B", "Snapshot C", "Snapshot D"), 1);
    snapshotMenu.setSelectedItemIndex(0, juce::dontSendNotification);
    addAndMakeVisible(snapshotMenu);

    snapshotRecallButton.setButtonText("Recall");
    snapshotRecallButton.onClick = [this]
    {
        switch (snapshotMenu.getSelectedItemIndex())
        {
            case 0: processorRef.recallSnapshotA(); break;
            case 1: processorRef.recallSnapshotB(); break;
            case 2: processorRef.recallSnapshotC(); break;
            case 3: processorRef.recallSnapshotD(); break;
            default: break;
        }
    };
    addAndMakeVisible(snapshotRecallButton);

    snapshotStoreButton.setButtonText("Store");
    snapshotStoreButton.onClick = [this]
    {
        switch (snapshotMenu.getSelectedItemIndex())
        {
            case 0: processorRef.storeSnapshotA(); break;
            case 1: processorRef.storeSnapshotB(); break;
            case 2: processorRef.storeSnapshotC(); break;
            case 3: processorRef.storeSnapshotD(); break;
            default: break;
        }
    };
    addAndMakeVisible(snapshotStoreButton);

    correlationLabel.setText("Corr", juce::dontSendNotification);
    correlationLabel.setJustificationType(juce::Justification::centredLeft);
    correlationLabel.setFont(kLabelFontSize);
    addAndMakeVisible(correlationLabel);

    correlationBox.addItemList(processorRef.getCorrelationPairNames(), 1);
    correlationBox.setSelectedItemIndex(processorRef.getCorrelationPairIndex(),
                                        juce::dontSendNotification);
    correlationBox.onChange = [this]
    {
        processorRef.setCorrelationPairIndex(correlationBox.getSelectedItemIndex());
    };
    addAndMakeVisible(correlationBox);

    layoutLabel.setText("Layout", juce::dontSendNotification);
    layoutLabel.setJustificationType(juce::Justification::centredLeft);
    layoutLabel.setFont(kLabelFontSize);
    addAndMakeVisible(layoutLabel);

    layoutValueLabel.setText(processorRef.getCurrentLayoutDescription(),
                             juce::dontSendNotification);
    layoutValueLabel.setJustificationType(juce::Justification::centredLeft);
    layoutValueLabel.setFont(kLabelFontSize);
    addAndMakeVisible(layoutValueLabel);

    msViewToggle.setButtonText("M/S View");
    msViewToggle.setVisible(false);
    msViewToggle.onClick = [this] {};
    addAndMakeVisible(msViewToggle);

    channelLabel.setText("Channel", juce::dontSendNotification);
    channelLabel.setJustificationType(juce::Justification::centredLeft);
    channelLabel.setFont(kLabelFontSize);
    channelLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(channelLabel);

    addAndMakeVisible(channelSelector);
    channelSelector.onChange = [this]
    {
        const int index = channelSelector.getSelectedItemIndex();
        if (index >= 0)
        {
            selectedChannel = index;
            processorRef.setSelectedChannelIndex(selectedChannel);
            analyzer.setSelectedChannel(selectedChannel);
            bandControls.setSelectedBand(selectedChannel, selectedBand);
            meters.setSelectedChannel(selectedChannel);
        }
    };

    const auto channelNames = processorRef.getCurrentChannelNames();
    auto buildPairLabels = [&channelNames]()
    {
        std::vector<juce::String> labels(channelNames.size());
        auto findIndex = [&channelNames](const juce::String& name)
        {
            for (int i = 0; i < static_cast<int>(channelNames.size()); ++i)
                if (channelNames[static_cast<size_t>(i)] == name)
                    return i;
            return -1;
        };
        auto addPair = [&](const juce::String& left, const juce::String& right, const juce::String& label)
        {
            const int li = findIndex(left);
            const int ri = findIndex(right);
            if (li >= 0 && ri >= 0)
            {
                labels[static_cast<size_t>(li)] = label;
                labels[static_cast<size_t>(ri)] = label;
            }
        };

        addPair("L", "R", "L/R");
        addPair("Ls", "Rs", "Ls/Rs");
        addPair("Lrs", "Rrs", "Lrs/Rrs");
        addPair("Lc", "Rc", "Lc/Rc");
        addPair("Ltf", "Rtf", "Ltf/Rtf");
        addPair("Ltr", "Rtr", "Ltr/Rtr");
        addPair("Lts", "Rts", "Lts/Rts");
        addPair("Lw", "Rw", "Lw/Rw");
        addPair("Bfl", "Bfr", "Bfl/Bfr");
        return labels;
    };
    const auto pairLabels = buildPairLabels();
    channelSelector.clear();
    for (int i = 0; i < static_cast<int>(channelNames.size()); ++i)
    {
        const auto& name = channelNames[static_cast<size_t>(i)];
        const auto& pair = pairLabels[static_cast<size_t>(i)];
        const juce::String label = pair.isNotEmpty() ? (name + " (" + pair + ")") : name;
        channelSelector.addItem(label, i + 1);
    }
    channelSelector.setSelectedItemIndex(0, juce::dontSendNotification);
    juce::StringArray labels;
    for (const auto& name : channelNames)
        labels.add(name);
    meters.setChannelLabels(labels);
    bandControls.setMsEnabled(true);

    analyzer.onBandSelected = [this](int band)
    {
        selectedBand = band;
        processorRef.setSelectedBandIndex(selectedBand);
        bandControls.setSelectedBand(selectedChannel, selectedBand);
    };

    bandControls.onBandNavigate = [this](int band)
    {
        selectedBand = band;
        processorRef.setSelectedBandIndex(selectedBand);
        analyzer.setSelectedBand(selectedBand);
        bandControls.setSelectedBand(selectedChannel, selectedBand);
    };

    analyzer.setSelectedChannel(selectedChannel);
    analyzer.setSelectedBand(selectedBand);
    bandControls.setSelectedBand(selectedChannel, selectedBand);
    meters.setSelectedChannel(selectedChannel);
    processorRef.setSelectedBandIndex(selectedBand);
    processorRef.setSelectedChannelIndex(selectedChannel);

    themeBox.setSelectedItemIndex(processorRef.getThemeMode(), juce::dontSendNotification);
    themeBox.onChange();

    addAndMakeVisible(meters);
    addAndMakeVisible(analyzer);
    addAndMakeVisible(bandControls);
    addAndMakeVisible(spectralPanel);
    addAndMakeVisible(correlation);

    // Hide advanced panels until collapsible UI is added.
    presetSectionLabel.setVisible(false);
    presetLabel.setVisible(false);
    presetBox.setVisible(false);
    savePresetButton.setVisible(false);
    loadPresetButton.setVisible(false);
    copyInstanceButton.setVisible(false);
    pasteInstanceButton.setVisible(false);
    presetBrowserLabel.setVisible(false);
    presetBrowserBox.setVisible(false);
    favoriteToggle.setVisible(false);
    refreshPresetsButton.setVisible(false);
    applyLabel.setVisible(false);
    applyTargetBox.setVisible(false);
    snapshotSectionLabel.setVisible(true);
    undoButton.setVisible(true);
    redoButton.setVisible(true);
    snapshotAButton.setVisible(false);
    snapshotBButton.setVisible(false);
    snapshotCButton.setVisible(false);
    snapshotDButton.setVisible(false);
    storeAButton.setVisible(false);
    storeBButton.setVisible(false);
    storeCButton.setVisible(false);
    storeDButton.setVisible(false);
    snapshotMenu.setVisible(true);
    snapshotRecallButton.setVisible(true);
    snapshotStoreButton.setVisible(true);
    midiSectionLabel.setVisible(false);
    midiLearnToggle.setVisible(false);
    midiTargetBox.setVisible(false);
    themeLabel.setVisible(false);
    themeBox.setVisible(false);
    layoutLabel.setVisible(false);
    layoutValueLabel.setVisible(false);
    correlationLabel.setVisible(false);
    correlationBox.setVisible(false);

    setResizable(false, false);
    setResizeLimits(kEditorWidth, kEditorHeight, kEditorWidth, kEditorHeight);

    setSize(kEditorWidth, kEditorHeight);
}

EQProAudioProcessorEditor::~EQProAudioProcessorEditor()
{
    openGLContext.detach();
    setLookAndFeel(nullptr);
}

void EQProAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(theme.background.brighter(0.02f),
                            0.0f, 0.0f,
                            theme.background.darker(0.04f),
                            0.0f, static_cast<float>(getHeight()),
                            false);
    g.setGradientFill(bg);
    g.fillAll();
    g.setColour(theme.panelOutline);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(6.0f), 6.0f, 1.0f);

    juce::ignoreUnused(kOuterMargin);
}

void EQProAudioProcessorEditor::resized()
{
    const float uiScale = juce::jlimit(0.8f, 2.5f, getWidth() / static_cast<float>(kEditorWidth));
    analyzer.setUiScale(uiScale);
    const int margin = static_cast<int>(kOuterMargin * uiScale);
    auto bounds = getLocalBounds().reduced(margin);

    headerLabel.setBounds({0, 0, 0, 0});
    versionLabel.setBounds({0, 0, 0, 0});

    const int topBarHeight = static_cast<int>(32 * uiScale);
    auto topBar = bounds.removeFromTop(topBarHeight);
    const int globalBypassWidth = static_cast<int>(120 * uiScale);
    const int globalBypassHeight = static_cast<int>(24 * uiScale);
    globalBypassButton.setBounds(topBar.removeFromLeft(globalBypassWidth)
                                     .withSizeKeepingCentre(globalBypassWidth, globalBypassHeight));
    topBar.removeFromLeft(static_cast<int>(12 * uiScale));
    const int mixLabelWidth = static_cast<int>(36 * uiScale);
    globalMixLabel.setBounds(topBar.removeFromLeft(mixLabelWidth)
                                 .withSizeKeepingCentre(mixLabelWidth, globalBypassHeight));
    const int mixSliderWidth = static_cast<int>(140 * uiScale);
    globalMixSlider.setBounds(topBar.removeFromLeft(mixSliderWidth)
                                  .withSizeKeepingCentre(mixSliderWidth, globalBypassHeight + 8));

    auto content = bounds;
    const int metersWidth = static_cast<int>(kRightPanelWidth * uiScale);
    auto rightPanel = content.removeFromRight(metersWidth);
    auto leftContent = content;
    const int analyzerHeight = static_cast<int>(leftContent.getHeight() * 0.70f);
    auto analyzerArea = leftContent.removeFromTop(analyzerHeight);
    analyzer.setBounds(analyzerArea);

    auto controlsArea = leftContent;
    auto metersArea = rightPanel;
    const int trimSize = static_cast<int>(86 * uiScale);
    const int trimLabelHeight = static_cast<int>(14 * uiScale);
    auto trimArea = metersArea.removeFromBottom(trimSize + trimLabelHeight + static_cast<int>(10 * uiScale));
    auto outputArea = trimArea.removeFromLeft(trimArea.getWidth() / 2);
    outputTrimLabel.setBounds(outputArea.removeFromTop(trimLabelHeight));
    outputTrimSlider.setBounds(outputArea.withSizeKeepingCentre(trimSize, trimSize));
    autoGainLabel.setBounds(trimArea.removeFromTop(trimLabelHeight));
    autoGainToggle.setBounds(trimArea.withSizeKeepingCentre(static_cast<int>(60 * uiScale),
                                                            static_cast<int>(22 * uiScale)));

    auto meterTop = metersArea;
    auto corrArea = meterTop.removeFromBottom(static_cast<int>(meterTop.getHeight() * 0.40f));
    correlation.setBounds(corrArea);
    meters.setBounds(meterTop);
    const int processingRowHeight = static_cast<int>(28 * uiScale);
    auto processingRow = controlsArea.removeFromBottom(processingRowHeight);
    phaseLabel.setBounds(processingRow.removeFromLeft(static_cast<int>(70 * uiScale)));
    phaseModeBox.setBounds(processingRow.removeFromLeft(static_cast<int>(140 * uiScale)));
    qualityLabel.setBounds(processingRow.removeFromLeft(static_cast<int>(70 * uiScale)));
    linearQualityBox.setBounds(processingRow.removeFromLeft(static_cast<int>(120 * uiScale)));
    bandControls.setBounds(controlsArea.reduced(static_cast<int>(6 * uiScale), 0));

    spectralPanel.setBounds({0, 0, 0, 0});

    characterLabel.setBounds({0, 0, 0, 0});
    characterBox.setBounds({0, 0, 0, 0});
    qModeLabel.setBounds({0, 0, 0, 0});
    qModeBox.setBounds({0, 0, 0, 0});
    qAmountLabel.setBounds({0, 0, 0, 0});
    qAmountSlider.setBounds({0, 0, 0, 0});
    channelLabel.setBounds({0, 0, 0, 0});
    channelSelector.setBounds({0, 0, 0, 0});
    msViewToggle.setBounds({0, 0, 0, 0});
    gainScaleSlider.setBounds({0, 0, 0, 0});
    phaseInvertToggle.setBounds({0, 0, 0, 0});
    analyzerRangeLabel.setBounds({0, 0, 0, 0});
    analyzerRangeBox.setBounds({0, 0, 0, 0});
    analyzerSpeedLabel.setBounds({0, 0, 0, 0});
    analyzerSpeedBox.setBounds({0, 0, 0, 0});
    analyzerViewLabel.setBounds({0, 0, 0, 0});
    analyzerViewBox.setBounds({0, 0, 0, 0});
    analyzerFreezeToggle.setBounds({0, 0, 0, 0});
    analyzerExternalToggle.setBounds({0, 0, 0, 0});
    smartSoloToggle.setBounds({0, 0, 0, 0});
    showSpectralToggle.setBounds({0, 0, 0, 0});

    layoutLabel.setBounds({0, 0, 0, 0});
    layoutValueLabel.setBounds({0, 0, 0, 0});
    correlationLabel.setBounds({0, 0, 0, 0});
    correlationBox.setBounds({0, 0, 0, 0});
    themeLabel.setBounds({0, 0, 0, 0});
    themeBox.setBounds({0, 0, 0, 0});
    resizer.setBounds({0, 0, 0, 0});
}
