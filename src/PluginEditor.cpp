#include "PluginEditor.h"
#include "util/ParamIDs.h"

namespace
{
constexpr int kEditorWidth = 720;
constexpr int kEditorHeight = 420;
constexpr int kOuterMargin = 16;
constexpr int kLeftPanelWidth = 80;
constexpr int kRightPanelWidth = 260;
constexpr float kLabelFontSize = 12.0f;
constexpr float kHeaderFontSize = 20.0f;
} // namespace

EQProAudioProcessorEditor::EQProAudioProcessorEditor(EQProAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      meters(p),
      analyzer(p),
      bandControls(p),
      ellipticPanel(p.getParameters()),
      spectralPanel(p.getParameters()),
      correlation(p)
{
    setLookAndFeel(&lookAndFeel);
    openGLContext.setContinuousRepainting(false);
    openGLContext.attachTo(*this);

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

    phaseLabel.setText("Phase", juce::dontSendNotification);
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

    phaseViewToggle.setButtonText("Phase View");
    phaseViewToggle.setToggleState(processorRef.getShowPhasePreference(),
                                   juce::dontSendNotification);
    phaseViewToggle.onClick = [this]
    {
        const bool enabled = phaseViewToggle.getToggleState();
        analyzer.setShowPhase(enabled);
        processorRef.setShowPhasePreference(enabled);
    };
    addAndMakeVisible(phaseViewToggle);

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
        ellipticPanel.setTheme(newTheme);
        spectralPanel.setTheme(newTheme);
        meters.setTheme(newTheme);
        correlation.setTheme(newTheme);

        headerLabel.setColour(juce::Label::textColourId, newTheme.text);
        versionLabel.setColour(juce::Label::textColourId, newTheme.textMuted);
        globalBypassButton.setColour(juce::ToggleButton::textColourId, newTheme.textMuted);
        phaseViewToggle.setColour(juce::ToggleButton::textColourId, newTheme.textMuted);
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
    msViewToggle.onClick = [this]
    {
        const auto channelNames = processorRef.getCurrentChannelNames();
        const bool msView = msViewToggle.getToggleState() && channelNames.size() == 2;
        channelSelector.clear();
        if (msView)
        {
            channelSelector.addItem("Mid", 1);
            channelSelector.addItem("Side", 2);
        }
        else
        {
            for (int i = 0; i < static_cast<int>(channelNames.size()); ++i)
                channelSelector.addItem(channelNames[static_cast<size_t>(i)], i + 1);
        }
        channelSelector.setSelectedItemIndex(juce::jlimit(0, channelSelector.getNumItems() - 1,
                                                          selectedChannel),
                                             juce::dontSendNotification);
        analyzer.setSelectedChannel(selectedChannel);
        bandControls.setSelectedBand(selectedChannel, selectedBand);
        bandControls.setMsEnabled(channelNames.size() == 2);
        bandControls.setLinkPairs(processorRef.getLinkPairNames(), processorRef.getLinkPairs());
        meters.setSelectedChannel(selectedChannel);
    };
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
    channelSelector.clear();
    for (int i = 0; i < static_cast<int>(channelNames.size()); ++i)
        channelSelector.addItem(channelNames[static_cast<size_t>(i)], i + 1);
    channelSelector.setSelectedItemIndex(0, juce::dontSendNotification);
    juce::StringArray labels;
    for (const auto& name : channelNames)
        labels.add(name);
    meters.setChannelLabels(labels);
    bandControls.setMsEnabled(channelNames.size() == 2);
    bandControls.setLinkPairs(processorRef.getLinkPairNames(), processorRef.getLinkPairs());

    analyzer.onBandSelected = [this](int band)
    {
        selectedBand = band;
        processorRef.setSelectedBandIndex(selectedBand);
        bandControls.setSelectedBand(selectedChannel, selectedBand);
    };

    analyzer.setSelectedChannel(selectedChannel);
    analyzer.setSelectedBand(selectedBand);
    analyzer.setShowPhase(phaseViewToggle.getToggleState());
    bandControls.setSelectedBand(selectedChannel, selectedBand);
    meters.setSelectedChannel(selectedChannel);
    processorRef.setSelectedBandIndex(selectedBand);
    processorRef.setSelectedChannelIndex(selectedChannel);

    themeBox.setSelectedItemIndex(processorRef.getThemeMode(), juce::dontSendNotification);
    themeBox.onChange();

    addAndMakeVisible(meters);
    addAndMakeVisible(analyzer);
    addAndMakeVisible(bandControls);
    addAndMakeVisible(ellipticPanel);
    addAndMakeVisible(spectralPanel);
    addAndMakeVisible(correlation);

    resizeConstrainer.setSizeLimits(900, 600, 1800, 1200);
    setResizable(true, true);
    setResizeLimits(900, 600, 1800, 1200);
    addAndMakeVisible(resizer);

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
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(8.0f), 8.0f, 1.0f);

    const auto bounds = getLocalBounds().reduced(kOuterMargin);
    const int leftX = bounds.getX() + kLeftPanelWidth;
    const int rightX = bounds.getRight() - kRightPanelWidth;
    g.setColour(theme.grid);
    g.drawLine(static_cast<float>(leftX), static_cast<float>(bounds.getY()),
               static_cast<float>(leftX), static_cast<float>(bounds.getBottom()), 1.0f);
    g.drawLine(static_cast<float>(rightX), static_cast<float>(bounds.getY()),
               static_cast<float>(rightX), static_cast<float>(bounds.getBottom()), 1.0f);
}

void EQProAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(kOuterMargin);
    auto headerRow = bounds.removeFromTop(32);
    headerLabel.setBounds(headerRow.removeFromLeft(220));
    versionLabel.setBounds(headerRow);
    globalBypassButton.setBounds(bounds.removeFromTop(24).removeFromLeft(160));

    bounds.removeFromTop(8);
    auto leftPanel = bounds.removeFromLeft(kLeftPanelWidth);
    meters.setBounds(leftPanel);
    auto rightPanel = bounds.removeFromRight(kRightPanelWidth);
    const int sectionHeight = 18;
    const int rowHeight = 22;
    const int smallGap = 4;
    const int groupGap = 8;

    processingSectionLabel.setBounds(rightPanel.removeFromTop(sectionHeight));
    rightPanel.removeFromTop(smallGap);
    auto phaseRow = rightPanel.removeFromTop(22);
    phaseLabel.setBounds(phaseRow.removeFromLeft(70));
    phaseModeBox.setBounds(phaseRow);
    rightPanel.removeFromTop(smallGap);
    auto qualityRow = rightPanel.removeFromTop(rowHeight);
    qualityLabel.setBounds(qualityRow.removeFromLeft(70));
    linearQualityBox.setBounds(qualityRow);
    rightPanel.removeFromTop(smallGap);
    auto windowRow = rightPanel.removeFromTop(rowHeight);
    windowLabel.setBounds(windowRow.removeFromLeft(70));
    linearWindowBox.setBounds(windowRow);
    rightPanel.removeFromTop(smallGap);
    auto oversampleRow = rightPanel.removeFromTop(rowHeight);
    oversamplingLabel.setBounds(oversampleRow.removeFromLeft(70));
    oversamplingBox.setBounds(oversampleRow);
    rightPanel.removeFromTop(smallGap);
    auto characterRow = rightPanel.removeFromTop(rowHeight);
    characterLabel.setBounds(characterRow.removeFromLeft(70));
    characterBox.setBounds(characterRow);
    rightPanel.removeFromTop(smallGap);
    auto autoGainRow = rightPanel.removeFromTop(rowHeight);
    autoGainLabel.setBounds(autoGainRow.removeFromLeft(70));
    autoGainToggle.setBounds(autoGainRow);
    rightPanel.removeFromTop(smallGap);
    gainScaleSlider.setBounds(rightPanel.removeFromTop(rowHeight));
    rightPanel.removeFromTop(smallGap);
    phaseInvertToggle.setBounds(rightPanel.removeFromTop(rowHeight));
    rightPanel.removeFromTop(smallGap);
    auto outputRow = rightPanel.removeFromTop(90);
    outputTrimLabel.setBounds(outputRow.removeFromTop(18));
    outputTrimSlider.setBounds(outputRow);
    rightPanel.removeFromTop(smallGap);
    phaseViewToggle.setBounds(rightPanel.removeFromTop(rowHeight));
    rightPanel.removeFromTop(smallGap);
    auto themeRow = rightPanel.removeFromTop(rowHeight);
    themeLabel.setBounds(themeRow.removeFromLeft(70));
    themeBox.setBounds(themeRow);
    rightPanel.removeFromTop(groupGap);

    analyzerSectionLabel.setBounds(rightPanel.removeFromTop(sectionHeight));
    rightPanel.removeFromTop(smallGap);
    auto analyzerRangeRow = rightPanel.removeFromTop(rowHeight);
    analyzerRangeLabel.setBounds(analyzerRangeRow.removeFromLeft(70));
    analyzerRangeBox.setBounds(analyzerRangeRow);
    rightPanel.removeFromTop(smallGap);
    auto analyzerSpeedRow = rightPanel.removeFromTop(rowHeight);
    analyzerSpeedLabel.setBounds(analyzerSpeedRow.removeFromLeft(70));
    analyzerSpeedBox.setBounds(analyzerSpeedRow);
    rightPanel.removeFromTop(smallGap);
    auto analyzerToggleRow = rightPanel.removeFromTop(rowHeight);
    analyzerFreezeToggle.setBounds(analyzerToggleRow.removeFromLeft(70));
    analyzerExternalToggle.setBounds(analyzerToggleRow);
    rightPanel.removeFromTop(smallGap);
    auto smartSoloRow = rightPanel.removeFromTop(rowHeight);
    smartSoloToggle.setBounds(smartSoloRow.removeFromLeft(100));
    rightPanel.removeFromTop(groupGap);

    midiSectionLabel.setBounds(rightPanel.removeFromTop(sectionHeight));
    rightPanel.removeFromTop(smallGap);
    auto midiRow = rightPanel.removeFromTop(rowHeight);
    midiLearnToggle.setBounds(midiRow.removeFromLeft(70));
    midiTargetBox.setBounds(midiRow);
    rightPanel.removeFromTop(groupGap);

    presetSectionLabel.setBounds(rightPanel.removeFromTop(sectionHeight));
    rightPanel.removeFromTop(smallGap);
    auto applyRow = rightPanel.removeFromTop(rowHeight);
    applyLabel.setBounds(applyRow.removeFromLeft(70));
    applyTargetBox.setBounds(applyRow);
    rightPanel.removeFromTop(smallGap);
    auto presetRow = rightPanel.removeFromTop(rowHeight);
    presetLabel.setBounds(presetRow.removeFromLeft(70));
    presetBox.setBounds(presetRow);
    rightPanel.removeFromTop(smallGap);
    auto presetButtons = rightPanel.removeFromTop(rowHeight);
    savePresetButton.setBounds(presetButtons.removeFromLeft(60));
    presetButtons.removeFromLeft(8);
    loadPresetButton.setBounds(presetButtons.removeFromLeft(60));
    rightPanel.removeFromTop(smallGap);
    auto instanceRow = rightPanel.removeFromTop(rowHeight);
    copyInstanceButton.setBounds(instanceRow.removeFromLeft(60));
    instanceRow.removeFromLeft(8);
    pasteInstanceButton.setBounds(instanceRow.removeFromLeft(60));
    rightPanel.removeFromTop(smallGap);
    auto presetBrowserRow = rightPanel.removeFromTop(rowHeight);
    presetBrowserLabel.setBounds(presetBrowserRow.removeFromLeft(70));
    presetBrowserBox.setBounds(presetBrowserRow);
    rightPanel.removeFromTop(smallGap);
    auto presetFavRow = rightPanel.removeFromTop(rowHeight);
    favoriteToggle.setBounds(presetFavRow.removeFromLeft(50));
    presetFavRow.removeFromLeft(6);
    refreshPresetsButton.setBounds(presetFavRow.removeFromLeft(70));
    rightPanel.removeFromTop(groupGap);

    snapshotSectionLabel.setBounds(rightPanel.removeFromTop(sectionHeight));
    rightPanel.removeFromTop(smallGap);
    auto undoRow = rightPanel.removeFromTop(rowHeight);
    undoButton.setBounds(undoRow.removeFromLeft(78));
    redoButton.setBounds(undoRow.removeFromLeft(78));
    rightPanel.removeFromTop(smallGap);
    auto snapshotMenuRow = rightPanel.removeFromTop(rowHeight);
    snapshotMenu.setBounds(snapshotMenuRow.removeFromLeft(120));
    snapshotMenuRow.removeFromLeft(6);
    snapshotRecallButton.setBounds(snapshotMenuRow.removeFromLeft(60));
    snapshotMenuRow.removeFromLeft(6);
    snapshotStoreButton.setBounds(snapshotMenuRow.removeFromLeft(60));
    rightPanel.removeFromTop(smallGap);
    auto abRow = rightPanel.removeFromTop(rowHeight);
    snapshotAButton.setBounds(abRow.removeFromLeft(30));
    snapshotBButton.setBounds(abRow.removeFromLeft(30));
    snapshotCButton.setBounds(abRow.removeFromLeft(30));
    snapshotDButton.setBounds(abRow.removeFromLeft(30));
    rightPanel.removeFromTop(smallGap);
    storeAButton.setBounds(rightPanel.removeFromTop(20));
    rightPanel.removeFromTop(smallGap);
    storeBButton.setBounds(rightPanel.removeFromTop(20));
    rightPanel.removeFromTop(smallGap);
    storeCButton.setBounds(rightPanel.removeFromTop(20));
    rightPanel.removeFromTop(smallGap);
    storeDButton.setBounds(rightPanel.removeFromTop(20));
    rightPanel.removeFromTop(groupGap);

    channelSectionLabel.setBounds(rightPanel.removeFromTop(sectionHeight));
    rightPanel.removeFromTop(smallGap);
    auto selectorRow = rightPanel.removeFromTop(24);
    channelLabel.setBounds(selectorRow.removeFromLeft(70));
    channelSelector.setBounds(selectorRow);
    rightPanel.removeFromTop(smallGap);
    auto layoutRow = rightPanel.removeFromTop(rowHeight);
    layoutLabel.setBounds(layoutRow.removeFromLeft(70));
    layoutValueLabel.setBounds(layoutRow);
    rightPanel.removeFromTop(smallGap);
    msViewToggle.setBounds(rightPanel.removeFromTop(rowHeight));
    rightPanel.removeFromTop(smallGap);
    auto corrRow = rightPanel.removeFromTop(rowHeight);
    correlationLabel.setBounds(corrRow.removeFromLeft(70));
    correlationBox.setBounds(corrRow);
    rightPanel.removeFromTop(8);
    analyzer.setBounds(bounds);
    auto correlationArea = rightPanel.removeFromBottom(90);
    auto ellipticArea = rightPanel.removeFromBottom(160);
    auto spectralArea = rightPanel.removeFromBottom(130);
    bandControls.setBounds(rightPanel);
    ellipticPanel.setBounds(ellipticArea);
    spectralPanel.setBounds(spectralArea);
    correlation.setBounds(correlationArea);
    resizer.setBounds(getLocalBounds().removeFromBottom(16).removeFromRight(16));
}
