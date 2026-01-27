#include "PluginEditor.h"
#include "util/ParamIDs.h"

// Editor implementation: creates and lays out all UI components.

juce::AudioProcessorEditor* EQProAudioProcessor::createEditor()
{
    logStartup("createEditor");
    if (juce::JUCEApplicationBase::isStandaloneApp())
    {
        const bool useGenericUi =
            juce::SystemStats::getEnvironmentVariable("EQPRO_STANDALONE_GENERIC_UI", "0").getIntValue() != 0;
        logStartup("Standalone generic UI: " + juce::String(useGenericUi ? "true" : "false"));
        if (useGenericUi)
            return new juce::GenericAudioProcessorEditor(*this);
    }
    return new EQProAudioProcessorEditor(*this);
}

namespace
{
constexpr int kEditorWidth = 1078;
constexpr int kEditorHeight = 726;
constexpr int kOuterMargin = 16;
constexpr int kLeftPanelWidth = 0;
constexpr int kRightPanelWidth = 180;
constexpr float kLabelFontSize = 12.0f;
constexpr float kHeaderFontSize = 20.0f;

juce::Image makeNoiseImage(int size)
{
    juce::Image noise(juce::Image::ARGB, size, size, true);
    juce::Random rng(0x5a17);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float shade = rng.nextFloat();
            const uint8 alpha = static_cast<uint8>(8 + shade * 18.0f);
            noise.setPixelAt(x, y, juce::Colour::fromRGBA(255, 255, 255, alpha));
        }
    }
    return noise;
}
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
    processorRef.logStartup("Editor ctor begin");
    setLookAndFeel(&lookAndFeel);
    setWantsKeyboardFocus(true);
    bool enableOpenGL = juce::SystemStats::getEnvironmentVariable("EQPRO_OPENGL", "0")
                            .getIntValue() != 0;
    if (juce::JUCEApplicationBase::isStandaloneApp())
        enableOpenGL = false;
    processorRef.logStartup("OpenGL enabled: " + juce::String(enableOpenGL ? "true" : "false"));
    if (enableOpenGL)
    {
        openGLContext.setContinuousRepainting(false);
        openGLContext.setComponentPaintingEnabled(true);
        openGLContext.setMultisamplingEnabled(true);
        openGLContext.setSwapInterval(1);
        openGLContext.attachTo(*this);
    }
    analyzer.setInteractive(true);
    backgroundNoise = makeNoiseImage(128);
    startTimerHz(2);

    // v4.4 beta: Uppercase for consistency
    headerLabel.setText("EQ PRO", juce::dontSendNotification);
    headerLabel.setJustificationType(juce::Justification::centredLeft);
    headerLabel.setFont(juce::Font(kHeaderFontSize, juce::Font::bold));
    headerLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe5e7eb));
    addAndMakeVisible(headerLabel);

    // v4.4 beta: Uppercase for consistency
    versionLabel.setText(Version::displayString().toUpperCase(), juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::centredRight);
    versionLabel.setFont(juce::Font(12.0f, juce::Font::plain));
    versionLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(versionLabel);

    globalBypassButton.setButtonText("GLOBAL BYPASS");
    globalBypassButton.setColour(juce::ToggleButton::textColourId,
                                 juce::Colour(0xffcbd5e1));
    globalBypassButton.setTooltip("Toggle global bypass");
    addAndMakeVisible(globalBypassButton);
    globalBypassAttachment =
        std::make_unique<ButtonAttachment>(processorRef.getParameters(), ParamIDs::globalBypass,
                                           globalBypassButton);

    globalMixLabel.setText("GLOBAL MIX", juce::dontSendNotification);
    globalMixLabel.setJustificationType(juce::Justification::centredLeft);
    globalMixLabel.setFont(kLabelFontSize);
    globalMixLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(globalMixLabel);

    globalMixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    globalMixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 18);
    globalMixSlider.setTextBoxIsEditable(true);
    globalMixSlider.setTextValueSuffix(" %");
    globalMixSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff38bdf8));
    globalMixSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe2e8f0));
    globalMixSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff1f2937));
    globalMixSlider.setTooltip("Global dry/wet mix");
    addAndMakeVisible(globalMixSlider);
    // Meter mode toggles (RMS vs Peak) in the top bar - styled like copy/paste buttons.
    rmsToggle.setLookAndFeel(&lookAndFeel);
    rmsToggle.setButtonText("RMS");
    rmsToggle.setClickingTogglesState(true);
    rmsToggle.setToggleState(true, juce::dontSendNotification);
    rmsToggle.setTooltip("Meter fill follows RMS");
    addAndMakeVisible(rmsToggle);

    peakToggle.setLookAndFeel(&lookAndFeel);
    peakToggle.setButtonText("PEAK");
    peakToggle.setClickingTogglesState(true);
    peakToggle.setToggleState(false, juce::dontSendNotification);
    peakToggle.setTooltip("Meter fill follows Peak");
    addAndMakeVisible(peakToggle);

    rmsToggle.onClick = [this]
    {
        if (! rmsToggle.getToggleState())
            rmsToggle.setToggleState(true, juce::dontSendNotification);
        peakToggle.setToggleState(false, juce::dontSendNotification);
        meters.setMeterMode(false);
    };
    peakToggle.onClick = [this]
    {
        if (! peakToggle.getToggleState())
            peakToggle.setToggleState(true, juce::dontSendNotification);
        rmsToggle.setToggleState(false, juce::dontSendNotification);
        meters.setMeterMode(true);
    };
    globalMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getParameters(), ParamIDs::globalMix, globalMixSlider);

    phaseLabel.setText("PROCESSING MODE", juce::dontSendNotification);
    phaseLabel.setJustificationType(juce::Justification::centredLeft);
    phaseLabel.setFont(kLabelFontSize);
    phaseLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(phaseLabel);

    phaseModeBox.addItemList(juce::StringArray("REAL-TIME", "NATURAL", "LINEAR"), 1);
    phaseModeBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    phaseModeBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    phaseModeBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(phaseModeBox);
    phaseModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::phaseMode, phaseModeBox);

    qualityLabel.setText("QUALITY", juce::dontSendNotification);
    qualityLabel.setJustificationType(juce::Justification::centredLeft);
    qualityLabel.setFont(kLabelFontSize);
    qualityLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(qualityLabel);

    linearQualityBox.addItemList(
        juce::StringArray("LOW", "MEDIUM", "HIGH", "VERY HIGH", "INTENSIVE"), 1);
    linearQualityBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    linearQualityBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    linearQualityBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(linearQualityBox);
    linearQualityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::linearQuality, linearQualityBox);
    
    // v4.5 beta: Global Harmonic layer oversampling toggles (applies to all bands uniformly)
    // Positioned next to "QUALITY" dropdown at the bottom to clearly indicate it's a global parameter
    // Only available in Natural Phase and Linear Phase modes (disabled/greyed out in Real-time)
    harmonicLayerOversamplingLabel.setText("HARMONIC LAYER OVERSAMPLING", juce::dontSendNotification);
    harmonicLayerOversamplingLabel.setJustificationType(juce::Justification::centredLeft);
    harmonicLayerOversamplingLabel.setFont(kLabelFontSize);
    harmonicLayerOversamplingLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(harmonicLayerOversamplingLabel);
    
    auto initHarmonicOversamplingToggle = [this](juce::ToggleButton& toggle, const juce::String& text, int value)
    {
        toggle.setButtonText(text);
        toggle.setClickingTogglesState(true);
        toggle.setToggleState(false, juce::dontSendNotification);
        toggle.setTooltip("Harmonic layer oversampling: " + text);
        toggle.onClick = [this, value]
        {
            // Set all other toggles to false (exclusive group)
            harmonicLayerOversamplingNoneToggle.setToggleState(value == 0, juce::dontSendNotification);
            harmonicLayerOversampling2xToggle.setToggleState(value == 1, juce::dontSendNotification);
            harmonicLayerOversampling4xToggle.setToggleState(value == 2, juce::dontSendNotification);
            harmonicLayerOversampling8xToggle.setToggleState(value == 3, juce::dontSendNotification);
            harmonicLayerOversampling16xToggle.setToggleState(value == 4, juce::dontSendNotification);
            
            // Set parameter value
            if (auto* param = processorRef.getParameters().getParameter(ParamIDs::harmonicLayerOversampling))
            {
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(value)));
            }
        };
        addAndMakeVisible(toggle);
    };
    
    initHarmonicOversamplingToggle(harmonicLayerOversamplingNoneToggle, "NONE", 0);
    initHarmonicOversamplingToggle(harmonicLayerOversampling2xToggle, "2X", 1);
    initHarmonicOversamplingToggle(harmonicLayerOversampling4xToggle, "4X", 2);
    initHarmonicOversamplingToggle(harmonicLayerOversampling8xToggle, "8X", 3);
    initHarmonicOversamplingToggle(harmonicLayerOversampling16xToggle, "16X", 4);

    windowLabel.setText("WINDOW", juce::dontSendNotification);
    windowLabel.setJustificationType(juce::Justification::centredLeft);
    windowLabel.setFont(kLabelFontSize);
    windowLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(windowLabel);

    linearWindowBox.addItemList(juce::StringArray("HANN", "BLACKMAN", "KAISER"), 1);
    linearWindowBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
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

    oversamplingBox.addItemList(juce::StringArray("OFF", "2X", "4X"), 1);
    oversamplingBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    oversamplingBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    oversamplingBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(oversamplingBox);
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::oversampling, oversamplingBox);
    oversamplingLabel.setVisible(false);
    oversamplingBox.setVisible(false);

    characterLabel.setText("CHARACTER", juce::dontSendNotification);
    characterLabel.setJustificationType(juce::Justification::centredLeft);
    characterLabel.setFont(kLabelFontSize);
    characterLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(characterLabel);

    characterBox.addItemList(juce::StringArray("OFF", "GENTLE", "WARM"), 1);
    characterBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
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

    qModeBox.addItemList(juce::StringArray("CONSTANT", "PROPORTIONAL"), 1);
    qModeBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    qModeBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    qModeBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(qModeBox);
    qModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::qMode, qModeBox);

    qAmountLabel.setText("Q AMT", juce::dontSendNotification);
    qAmountLabel.setJustificationType(juce::Justification::centredLeft);
    qAmountLabel.setFont(kLabelFontSize);
    qAmountLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(qAmountLabel);

    qAmountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    qAmountSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 18);
    qAmountSlider.setTextBoxIsEditable(true);
    qAmountSlider.setTextValueSuffix(" %");
    addAndMakeVisible(qAmountSlider);
    qAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getParameters(), ParamIDs::qModeAmount, qAmountSlider);

    autoGainLabel.setText("AUTO GAIN", juce::dontSendNotification);
    autoGainLabel.setJustificationType(juce::Justification::centred);
    autoGainLabel.setFont(kLabelFontSize);
    autoGainLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(autoGainLabel);

    autoGainToggle.setButtonText("ENABLE");
    autoGainToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(autoGainToggle);
    autoGainAttachment = std::make_unique<ButtonAttachment>(
        processorRef.getParameters(), ParamIDs::autoGainEnable, autoGainToggle);

    gainScaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    gainScaleSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 18);
    gainScaleSlider.setTextBoxIsEditable(true);
    gainScaleSlider.setTextValueSuffix(" %");
    addAndMakeVisible(gainScaleSlider);
    gainScaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getParameters(), ParamIDs::gainScale, gainScaleSlider);

    phaseInvertToggle.setButtonText("PHASE INVERT");
    phaseInvertToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(phaseInvertToggle);
    phaseInvertAttachment = std::make_unique<ButtonAttachment>(
        processorRef.getParameters(), ParamIDs::phaseInvert, phaseInvertToggle);

    analyzerRangeLabel.setText("RANGE", juce::dontSendNotification);
    analyzerRangeLabel.setJustificationType(juce::Justification::centredLeft);
    analyzerRangeLabel.setFont(kLabelFontSize);
    analyzerRangeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(analyzerRangeLabel);

    analyzerRangeBox.addItemList(juce::StringArray("3 DB", "6 DB", "12 DB", "30 DB"), 1);
    analyzerRangeBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    analyzerRangeBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    analyzerRangeBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(analyzerRangeBox);
    analyzerRangeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::analyzerRange, analyzerRangeBox);

    analyzerSpeedLabel.setText("SPEED", juce::dontSendNotification);
    analyzerSpeedLabel.setJustificationType(juce::Justification::centredLeft);
    analyzerSpeedLabel.setFont(kLabelFontSize);
    analyzerSpeedLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(analyzerSpeedLabel);

    analyzerSpeedBox.addItemList(juce::StringArray("SLOW", "NORMAL", "FAST"), 1);
    analyzerSpeedBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    analyzerSpeedBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    analyzerSpeedBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(analyzerSpeedBox);
    analyzerSpeedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::analyzerSpeed, analyzerSpeedBox);

    analyzerViewLabel.setText("VIEW", juce::dontSendNotification);
    analyzerViewLabel.setJustificationType(juce::Justification::centredLeft);
    analyzerViewLabel.setFont(kLabelFontSize);
    analyzerViewLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(analyzerViewLabel);

    analyzerViewBox.addItemList(juce::StringArray("BOTH", "PRE", "POST"), 1);
    analyzerViewBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    analyzerViewBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    analyzerViewBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(analyzerViewBox);
    analyzerViewAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::analyzerView, analyzerViewBox);

    analyzerFreezeToggle.setButtonText("FREEZE");
    analyzerFreezeToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(analyzerFreezeToggle);
    analyzerFreezeAttachment = std::make_unique<ButtonAttachment>(
        processorRef.getParameters(), ParamIDs::analyzerFreeze, analyzerFreezeToggle);

    analyzerExternalToggle.setButtonText("EXTERNAL");
    analyzerExternalToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(analyzerExternalToggle);
    analyzerExternalAttachment = std::make_unique<ButtonAttachment>(
        processorRef.getParameters(), ParamIDs::analyzerExternal, analyzerExternalToggle);

    smartSoloToggle.setButtonText("SMART SOLO");
    smartSoloToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(smartSoloToggle);
    smartSoloAttachment = std::make_unique<ButtonAttachment>(
        processorRef.getParameters(), ParamIDs::smartSolo, smartSoloToggle);

    showSpectralToggle.setButtonText("SPECTRAL");
    showSpectralToggle.setToggleState(true, juce::dontSendNotification);
    showSpectralToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(showSpectralToggle);
    showSpectralToggle.onClick = [this]()
    {
        spectralPanel.setVisible(showSpectralToggle.getToggleState());
        resized();
    };


    correlation.setVisible(true);

    midiLearnToggle.setButtonText("LEARN");
    midiLearnToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(midiLearnToggle);
    midiLearnAttachment = std::make_unique<ButtonAttachment>(
        processorRef.getParameters(), ParamIDs::midiLearn, midiLearnToggle);

    midiTargetBox.addItemList(juce::StringArray("GAIN", "FREQ", "Q"), 1);
    midiTargetBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    midiTargetBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    midiTargetBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    addAndMakeVisible(midiTargetBox);
    midiTargetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getParameters(), ParamIDs::midiTarget, midiTargetBox);

    outputTrimLabel.setText("OUTPUT", juce::dontSendNotification);
    outputTrimLabel.setJustificationType(juce::Justification::centred);
    outputTrimLabel.setFont(kLabelFontSize);
    outputTrimLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(outputTrimLabel);

    outputTrimSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    outputTrimSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 18);
    outputTrimSlider.setTextBoxIsEditable(true);
    outputTrimSlider.setTextValueSuffix(" dB");
    outputTrimSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff38bdf8));
    addAndMakeVisible(outputTrimSlider);
    outputTrimAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getParameters(), ParamIDs::outputTrim, outputTrimSlider);

    auto updateQualityEnabled = [this]
    {
        const auto* modeParam = processorRef.getParameters().getRawParameterValue(ParamIDs::phaseMode);
        const int mode = modeParam != nullptr ? static_cast<int>(modeParam->load())
                                              : phaseModeBox.getSelectedItemIndex();
        linearQualityBox.setEnabled(mode == 2);
        linearWindowBox.setEnabled(mode != 0);
        if (mode != 2)
            linearQualityBox.setSelectedItemIndex(4, juce::sendNotification);
        
        // v4.5 beta: Enable/disable harmonic layer oversampling toggles based on processing mode
        // Enabled for Natural Phase (mode == 1) and Linear Phase (mode == 2), disabled in Real-time (mode == 0)
        // Works with all quality settings in Natural and Linear modes
        const bool oversamplingEnabled = (mode == 1 || mode == 2);  // Natural (1) or Linear (2)
        harmonicLayerOversamplingNoneToggle.setEnabled(oversamplingEnabled);
        harmonicLayerOversampling2xToggle.setEnabled(oversamplingEnabled);
        harmonicLayerOversampling4xToggle.setEnabled(oversamplingEnabled);
        harmonicLayerOversampling8xToggle.setEnabled(oversamplingEnabled);
        harmonicLayerOversampling16xToggle.setEnabled(oversamplingEnabled);
        
        // Grey out when disabled (Real-time mode)
        const float oversamplingAlpha = oversamplingEnabled ? 1.0f : 0.35f;
        harmonicLayerOversamplingNoneToggle.setAlpha(oversamplingAlpha);
        harmonicLayerOversampling2xToggle.setAlpha(oversamplingAlpha);
        harmonicLayerOversampling4xToggle.setAlpha(oversamplingAlpha);
        harmonicLayerOversampling8xToggle.setAlpha(oversamplingAlpha);
        harmonicLayerOversampling16xToggle.setAlpha(oversamplingAlpha);
        
        // Also update label visibility/alpha
        harmonicLayerOversamplingLabel.setAlpha(oversamplingAlpha);
    };
    phaseModeBox.onChange = [updateQualityEnabled]
    {
        updateQualityEnabled();
    };
    updateQualityEnabled();
    
    // v4.5 beta: Sync harmonic layer oversampling toggle states from parameter
    if (auto* param = processorRef.getParameters().getParameter(ParamIDs::harmonicLayerOversampling))
    {
        const int osValue = static_cast<int>(param->convertFrom0to1(param->getValue()));
        harmonicLayerOversamplingNoneToggle.setToggleState(osValue == 0, juce::dontSendNotification);
        harmonicLayerOversampling2xToggle.setToggleState(osValue == 1, juce::dontSendNotification);
        harmonicLayerOversampling4xToggle.setToggleState(osValue == 2, juce::dontSendNotification);
        harmonicLayerOversampling8xToggle.setToggleState(osValue == 3, juce::dontSendNotification);
        harmonicLayerOversampling16xToggle.setToggleState(osValue == 4, juce::dontSendNotification);
    }


    auto initSectionLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(juce::Font(12.0f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, theme.accent);
        addAndMakeVisible(label);
    };

    initSectionLabel(processingSectionLabel, "PROCESSING");
    initSectionLabel(analyzerSectionLabel, "ANALYZER");
    initSectionLabel(midiSectionLabel, "MIDI");
    initSectionLabel(presetSectionLabel, "PRESETS");
    initSectionLabel(snapshotSectionLabel, "SNAPSHOTS");
    initSectionLabel(channelSectionLabel, "CHANNEL");

    themeLabel.setText("THEME", juce::dontSendNotification);
    themeLabel.setJustificationType(juce::Justification::centredLeft);
    themeLabel.setFont(kLabelFontSize);
    addAndMakeVisible(themeLabel);

    themeBox.addItemList(juce::StringArray("DARK", "LIGHT"), 1);
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
        presetPrevButton.setColour(juce::TextButton::textColourOffId, newTheme.textMuted);
        presetNextButton.setColour(juce::TextButton::textColourOffId, newTheme.textMuted);
        presetPrevButton.setColour(juce::TextButton::buttonColourId, newTheme.panel);
        presetPrevButton.setColour(juce::TextButton::buttonOnColourId, newTheme.panel.brighter(0.2f));
        presetNextButton.setColour(juce::TextButton::buttonColourId, newTheme.panel);
        presetNextButton.setColour(juce::TextButton::buttonOnColourId, newTheme.panel.brighter(0.2f));
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
        // Style toggles to match copy/paste buttons with text inside.
        // Colors are handled by custom LookAndFeel::drawToggleButton method.
        rmsToggle.setColour(juce::ToggleButton::textColourId, newTheme.textMuted);
        rmsToggle.setColour(juce::ToggleButton::tickColourId, juce::Colours::transparentBlack);
        rmsToggle.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::transparentBlack);
        peakToggle.setColour(juce::ToggleButton::textColourId, newTheme.textMuted);
        peakToggle.setColour(juce::ToggleButton::tickColourId, juce::Colours::transparentBlack);
        peakToggle.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::transparentBlack);
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
    applyTargetBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    applyTargetBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    applyTargetBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff1f2937));
    applyTargetBox.setSelectedItemIndex(processorRef.getPresetApplyTarget(),
                                        juce::dontSendNotification);
    addAndMakeVisible(applyTargetBox);
    applyTargetBox.onChange = [this]
    {
        processorRef.setPresetApplyTarget(applyTargetBox.getSelectedItemIndex());
    };

    presetDeltaToggle.setButtonText("DELTA");
    presetDeltaToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcbd5e1));
    presetDeltaToggle.setTooltip("Apply presets as delta (non-destructive)");
    addAndMakeVisible(presetDeltaToggle);
    presetDeltaToggle.setVisible(false);

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
    presetBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
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
            if (! presetDeltaToggle.getToggleState())
            {
                for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
                {
                    setParam(ParamIDs::bandParamId(ch, band, "bypass"), 1.0f);
                    setParam(ParamIDs::bandParamId(ch, band, "gain"), 0.0f);
                    setParam(ParamIDs::bandParamId(ch, band, "q"), 0.707f);
                    setParam(ParamIDs::bandParamId(ch, band, "type"), 0.0f);
                    setParam(ParamIDs::bandParamId(ch, band, "ms"), 0.0f);
                }
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

    savePresetButton.setButtonText("SAVE");
    savePresetButton.setTooltip("Save preset to file");
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

    loadPresetButton.setButtonText("LOAD");
    loadPresetButton.setTooltip("Load preset from file");
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
                                            processorRef.replaceStateSafely(juce::ValueTree::fromXml(*xml));
                                    }
                                     loadChooser.reset();
                                 });
    };
    addAndMakeVisible(loadPresetButton);

    presetPrevButton.setButtonText("PREV");
    presetPrevButton.setTooltip("Previous preset");
    presetPrevButton.onClick = [this]
    {
        const int total = presetBrowserBox.getNumItems();
        if (total <= 0)
            return;
        int index = presetBrowserBox.getSelectedItemIndex();
        if (index < 0)
            index = 0;
        index = (index - 1 + total) % total;
        presetBrowserBox.setSelectedItemIndex(index, juce::sendNotification);
    };
    addAndMakeVisible(presetPrevButton);

    presetNextButton.setButtonText("NEXT");
    presetNextButton.setTooltip("Next preset");
    presetNextButton.onClick = [this]
    {
        const int total = presetBrowserBox.getNumItems();
        if (total <= 0)
            return;
        int index = presetBrowserBox.getSelectedItemIndex();
        if (index < 0)
            index = 0;
        index = (index + 1) % total;
        presetBrowserBox.setSelectedItemIndex(index, juce::sendNotification);
    };
    addAndMakeVisible(presetNextButton);

    copyInstanceButton.setButtonText("COPY");
    copyInstanceButton.onClick = [this]
    {
        processorRef.copyStateToClipboard();
    };
    addAndMakeVisible(copyInstanceButton);

    pasteInstanceButton.setButtonText("PASTE");
    pasteInstanceButton.onClick = [this]
    {
        processorRef.pasteStateFromClipboard();
    };
    addAndMakeVisible(pasteInstanceButton);

    // v4.4 beta: Uppercase for consistency
    presetBrowserLabel.setText("PRESET", juce::dontSendNotification);
    presetBrowserLabel.setJustificationType(juce::Justification::centredLeft);
    presetBrowserLabel.setFont(kLabelFontSize);
    addAndMakeVisible(presetBrowserLabel);

    presetBrowserBox.setTooltip("Preset list");
    addAndMakeVisible(presetBrowserBox);
    favoriteToggle.setButtonText("FAV");
    addAndMakeVisible(favoriteToggle);
    refreshPresetsButton.setButtonText("REFRESH");
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

    undoButton.setButtonText("UNDO");
    undoButton.setTooltip("Undo last change");
    undoButton.onClick = [this]
    {
        if (auto* undo = processorRef.getUndoManager())
            undo->undo();
    };
    addAndMakeVisible(undoButton);

    redoButton.setButtonText("REDO");
    redoButton.setTooltip("Redo last change");
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

    snapshotRecallButton.setButtonText("RECALL");
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

    correlationLabel.setText("GONIO", juce::dontSendNotification);
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

    layoutLabel.setText("LAYOUT", juce::dontSendNotification);
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

    channelLabel.setText("CHANNEL", juce::dontSendNotification);
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

    refreshChannelLayout();

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
    savePresetButton.setVisible(true);
    loadPresetButton.setVisible(true);
    copyInstanceButton.setVisible(false);
    pasteInstanceButton.setVisible(false);
    presetBrowserLabel.setVisible(true);
    presetBrowserBox.setVisible(true);
    presetPrevButton.setVisible(true);
    presetNextButton.setVisible(true);
    favoriteToggle.setVisible(false);
    refreshPresetsButton.setVisible(false);
    applyLabel.setVisible(false);
    applyTargetBox.setVisible(false);
    presetDeltaToggle.setVisible(false);
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

    pendingWindowRescue = false;
    setResizable(false, false);
    setResizeLimits(kEditorWidth, kEditorHeight, kEditorWidth, kEditorHeight);
    
    // v4.4 beta: Use buffered rendering for better performance on initial load
    // Reduces repaint overhead and ensures all child components appear immediately
    setBufferedToImage(true);

    setSize(kEditorWidth, kEditorHeight);

    if (juce::JUCEApplicationBase::isStandaloneApp())
    {
        pendingWindowRescue = false;
        if (auto* peer = getPeer())
        {
            auto& desktop = juce::Desktop::getInstance();
            const auto& display = desktop.getDisplays().getMainDisplay();
            const auto area = display.userArea;
            if (! area.isEmpty())
            {
                const int targetW = juce::jmin(kEditorWidth, area.getWidth());
                const int targetH = juce::jmin(kEditorHeight, area.getHeight());
                setBounds(area.withSizeKeepingCentre(targetW, targetH));
                peer->setMinimised(false);
                toFront(true);
            }
        }
        else
        {
            processorRef.logStartup("Standalone editor: no peer yet, skip bounds");
        }
    }
    processorRef.logStartup("Editor ctor end");
}


EQProAudioProcessorEditor::~EQProAudioProcessorEditor()
{
    processorRef.logStartup("Editor dtor begin");
    openGLContext.detach();
    setLookAndFeel(nullptr);
    processorRef.logStartup("Editor dtor end");
}

bool EQProAudioProcessorEditor::syncToHostBounds()
{
    return false;
}

void EQProAudioProcessorEditor::timerCallback()
{
    if (pendingWindowRescue)
    {
        ++windowRescueTicks;
        if (windowRescueTicks > 1)
        {
            if (auto* top = getTopLevelComponent())
            {
                auto& desktop = juce::Desktop::getInstance();
                const auto& display = desktop.getDisplays().getMainDisplay();
                const auto area = display.userArea;
                if (! area.isEmpty())
                {
                    const int targetW = juce::jmin(kEditorWidth, area.getWidth());
                    const int targetH = juce::jmin(kEditorHeight, area.getHeight());
                    top->setBounds(area.withSizeKeepingCentre(targetW, targetH));
                    top->setAlwaysOnTop(true);
                    top->setVisible(true);
                    top->toFront(true);
                    if (auto* peer = top->getPeer())
                        peer->setMinimised(false);
                }
            }
            if (windowRescueTicks > 10)
            {
                if (auto* top = getTopLevelComponent())
                    top->setAlwaysOnTop(false);
                pendingWindowRescue = false;
            }
        }
    }
    
    // v4.5 beta: Keep harmonic oversampling toggles in sync with phase mode
    // Read directly from the parameter to avoid stale UI state.
    const auto* modeParam = processorRef.getParameters().getRawParameterValue(ParamIDs::phaseMode);
    const int mode = modeParam != nullptr ? static_cast<int>(modeParam->load())
                                          : phaseModeBox.getSelectedItemIndex();
    const bool oversamplingEnabled = (mode == 1 || mode == 2);  // Natural (1) or Linear (2)
    harmonicLayerOversamplingNoneToggle.setEnabled(oversamplingEnabled);
    harmonicLayerOversampling2xToggle.setEnabled(oversamplingEnabled);
    harmonicLayerOversampling4xToggle.setEnabled(oversamplingEnabled);
    harmonicLayerOversampling8xToggle.setEnabled(oversamplingEnabled);
    harmonicLayerOversampling16xToggle.setEnabled(oversamplingEnabled);
    const float oversamplingAlpha = oversamplingEnabled ? 1.0f : 0.35f;
    harmonicLayerOversamplingNoneToggle.setAlpha(oversamplingAlpha);
    harmonicLayerOversampling2xToggle.setAlpha(oversamplingAlpha);
    harmonicLayerOversampling4xToggle.setAlpha(oversamplingAlpha);
    harmonicLayerOversampling8xToggle.setAlpha(oversamplingAlpha);
    harmonicLayerOversampling16xToggle.setAlpha(oversamplingAlpha);
    harmonicLayerOversamplingLabel.setAlpha(oversamplingAlpha);
    
    refreshChannelLayout();
}

void EQProAudioProcessorEditor::refreshChannelLayout()
{
    const auto channelNames = processorRef.getCurrentChannelNames();
    const auto layoutDesc = processorRef.getCurrentLayoutDescription();
    if (channelNames == cachedChannelNames && layoutDesc == cachedLayoutDescription)
        return;

    cachedChannelNames = channelNames;
    cachedLayoutDescription = layoutDesc;
    layoutValueLabel.setText(layoutDesc, juce::dontSendNotification);

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
        addPair("TFL", "TFR", "TFL/TFR");
        addPair("TRL", "TRR", "TRL/TRR");
        addPair("TML", "TMR", "TML/TMR");
        addPair("Lw", "Rw", "Lw/Rw");
        addPair("Bfl", "Bfr", "Bfl/Bfr");
        return labels;
    };

    const int previousSelection = selectedChannel;
    const auto pairLabels = buildPairLabels();
    channelSelector.clear(juce::dontSendNotification);
    
    // v4.2: Adapt channel selector width for immersive formats.
    // Find the longest possible channel label to size the dropdown appropriately.
    // Check all possible immersive format channel names (longest would be like "TML (TML/TMR)").
    const float uiScale = 1.0f;
    int maxLabelWidth = 0;
    juce::Font labelFont = channelSelector.getLookAndFeel().getComboBoxFont(channelSelector);
    
    for (int i = 0; i < static_cast<int>(channelNames.size()); ++i)
    {
        const auto& name = channelNames[static_cast<size_t>(i)];
        const auto& pair = pairLabels[static_cast<size_t>(i)];
        const juce::String label = pair.isNotEmpty() ? (name + " (" + pair + ")") : name;
        channelSelector.addItem(label, i + 1);
        
        // Calculate width needed for this label.
        const int labelWidth = static_cast<int>(labelFont.getStringWidthFloat(label));
        maxLabelWidth = juce::jmax(maxLabelWidth, labelWidth);
    }
    
    // Also check potential longest names from immersive formats that might not be in current layout.
    // Longest possible: "TML (TML/TMR)" = 15 chars, "Bfl (Bfl/Bfr)" = 13 chars, "Lrs (Lrs/Rrs)" = 12 chars
    const juce::StringArray testLabels = {
        "TML (TML/TMR)",  // Longest from immersive formats
        "TMR (TML/TMR)",
        "Bfl (Bfl/Bfr)",
        "Bfr (Bfl/Bfr)",
        "Lrs (Lrs/Rrs)",
        "Rrs (Lrs/Rrs)",
        "TFL (TFL/TFR)",
        "TRL (TRL/TRR)",
        "Lw (Lw/Rw)",
        "LFE2"  // Longest single name
    };
    for (const auto& testLabel : testLabels)
    {
        const int testWidth = static_cast<int>(labelFont.getStringWidthFloat(testLabel));
        maxLabelWidth = juce::jmax(maxLabelWidth, testWidth);
    }
    
    // Store the maximum width for use in layout (add padding for dropdown arrow and margins).
    channelSelectorMaxWidth = maxLabelWidth + static_cast<int>(40 * uiScale);

    const int maxIndex = juce::jmax(0, static_cast<int>(channelNames.size()) - 1);
    selectedChannel = juce::jlimit(0, maxIndex, previousSelection);
    channelSelector.setSelectedItemIndex(selectedChannel, juce::dontSendNotification);

    juce::StringArray labels;
    for (const auto& name : channelNames)
        labels.add(name);
    meters.setChannelLabels(labels);
    bandControls.setMsEnabled(true);
    bandControls.setChannelNames(channelNames);
    analyzer.invalidateCaches();
    analyzer.setSelectedChannel(selectedChannel);
    bandControls.setSelectedBand(selectedChannel, selectedBand);
    meters.setSelectedChannel(selectedChannel);
    processorRef.setSelectedChannelIndex(selectedChannel);
}

void EQProAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(theme.background);
    auto frameBounds = getLocalBounds().toFloat().reduced(6.0f);
    juce::ColourGradient sheen(theme.panel.withAlpha(0.35f),
                               frameBounds.getTopLeft(),
                               juce::Colours::transparentBlack,
                               frameBounds.getBottomLeft(),
                               false);
    g.setGradientFill(sheen);
    g.fillRoundedRectangle(frameBounds, 10.0f);
    g.setColour(theme.panelOutline);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(6.0f), 10.0f, 1.0f);

    if (backgroundNoise.isValid())
    {
        g.setOpacity(0.04f);
        g.drawImageWithin(backgroundNoise, 0, 0, getWidth(), getHeight(),
                          juce::RectanglePlacement::fillDestination);
        g.setOpacity(1.0f);
    }

    g.setColour(theme.panelOutline.withAlpha(0.35f));
    if (topBarBounds.getHeight() > 0)
        g.drawLine(static_cast<float>(topBarBounds.getX()),
                   static_cast<float>(topBarBounds.getBottom()),
                   static_cast<float>(topBarBounds.getRight()),
                   static_cast<float>(topBarBounds.getBottom()), 1.0f);
    if (analyzerBounds.getHeight() > 0)
        g.drawLine(static_cast<float>(analyzerBounds.getX()),
                   static_cast<float>(analyzerBounds.getBottom()),
                   static_cast<float>(analyzerBounds.getRight()),
                   static_cast<float>(analyzerBounds.getBottom()), 1.0f);

    if (debugVisible)
    {
        auto area = getLocalBounds().removeFromBottom(90).removeFromLeft(280).reduced(12);
        g.setColour(theme.panel.withAlpha(0.9f));
        g.fillRoundedRectangle(area.toFloat(), 6.0f);
        g.setColour(theme.panelOutline);
        g.drawRoundedRectangle(area.toFloat(), 6.0f, 1.0f);

        const double sr = processorRef.getSampleRate();
        const int latency = processorRef.getLatencySamples();
        const auto phaseMode = processorRef.getParameters()
                                   .getRawParameterValue(ParamIDs::phaseMode)
                                   ->load();
        const juce::String text = "Debug Panel\n"
            "SR: " + juce::String(sr, 0) + " Hz\n"
            "Latency: " + juce::String(latency) + " samples\n"
            "Phase Mode: " + juce::String(static_cast<int>(phaseMode)) + "\n"
            "Analyzer: " + juce::String(analyzer.getTimerHz()) + " Hz\n"
            "OpenGL: " + juce::String(openGLContext.isAttached() ? "On" : "Off");
        g.setColour(theme.text);
        g.setFont(12.0f);
        g.drawFittedText(text, area, juce::Justification::topLeft, 4);
    }

    juce::ignoreUnused(kOuterMargin);
}

bool EQProAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress('d', juce::ModifierKeys::ctrlModifier, 0))
    {
        debugVisible = ! debugVisible;
        processorRef.setDebugToneEnabled(debugVisible);
        repaint();
        return true;
    }
    return false;
}

void EQProAudioProcessorEditor::resized()
{
    const float uiScale = 1.0f;
    analyzer.setUiScale(uiScale);
    const int margin = static_cast<int>(kOuterMargin * uiScale);
    auto bounds = getLocalBounds().reduced(margin);

    const int headerHeight = static_cast<int>(26 * uiScale);
    auto headerRow = bounds.removeFromTop(headerHeight);
    headerBounds = headerRow;
    const int headerWidth = static_cast<int>(220 * uiScale);
    headerLabel.setBounds(headerRow.removeFromLeft(headerWidth));
    versionLabel.setBounds(headerRow.removeFromRight(static_cast<int>(220 * uiScale)));

    const int topBarHeight = static_cast<int>(32 * uiScale);
    auto topBar = bounds.removeFromTop(topBarHeight);
    topBarBounds = topBar;
    const int globalBypassWidth = static_cast<int>(120 * uiScale);
    const int globalBypassHeight = static_cast<int>(24 * uiScale);
    globalBypassButton.setBounds(topBar.removeFromLeft(globalBypassWidth)
                                     .withSizeKeepingCentre(globalBypassWidth, globalBypassHeight));
    topBar.removeFromLeft(static_cast<int>(12 * uiScale));
    const int mixLabelWidth = static_cast<int>(
        globalMixLabel.getFont().getStringWidthFloat(globalMixLabel.getText()) + 10 * uiScale);
    globalMixLabel.setBounds(topBar.removeFromLeft(mixLabelWidth)
                                 .withSizeKeepingCentre(mixLabelWidth, globalBypassHeight));
    const int mixSliderWidth = static_cast<int>(140 * uiScale);
    globalMixSlider.setBounds(topBar.removeFromLeft(mixSliderWidth)
                                  .withSizeKeepingCentre(mixSliderWidth, globalBypassHeight + 8));
    topBar.removeFromLeft(static_cast<int>(10 * uiScale));
    // RMS/Peak toggles are positioned above the meters on the right panel.
    const int actionBtnW = static_cast<int>(60 * uiScale);
    undoButton.setBounds(topBar.removeFromLeft(actionBtnW)
                             .withSizeKeepingCentre(actionBtnW, globalBypassHeight));
    redoButton.setBounds(topBar.removeFromLeft(actionBtnW)
                             .withSizeKeepingCentre(actionBtnW, globalBypassHeight));
    savePresetButton.setBounds(topBar.removeFromLeft(actionBtnW)
                                   .withSizeKeepingCentre(actionBtnW, globalBypassHeight));
    loadPresetButton.setBounds(topBar.removeFromLeft(actionBtnW)
                                   .withSizeKeepingCentre(actionBtnW, globalBypassHeight));
    topBar.removeFromLeft(static_cast<int>(8 * uiScale));
    const int presetLabelWidth = static_cast<int>(
        presetBrowserLabel.getFont().getStringWidthFloat(presetBrowserLabel.getText()) + 8 * uiScale);
    presetBrowserLabel.setBounds(topBar.removeFromLeft(presetLabelWidth)
                                     .withSizeKeepingCentre(presetLabelWidth, globalBypassHeight));
    // Preset navigation buttons match copy/paste button size (58 width, 22 height).
    // Place Prev/Next side-by-side before the dropdown.
    const int navW = 58;  // Same as btnW in BandControlsPanel
    const int navH = 22;  // Same as kRowHeight in BandControlsPanel
    const int navGap = static_cast<int>(6 * uiScale);
    presetPrevButton.setBounds(topBar.removeFromLeft(navW)
                                   .withSizeKeepingCentre(navW, navH));
    topBar.removeFromLeft(navGap);
    presetNextButton.setBounds(topBar.removeFromLeft(navW)
                                   .withSizeKeepingCentre(navW, navH));
    topBar.removeFromLeft(static_cast<int>(8 * uiScale));
    const int presetBoxW = static_cast<int>(180 * uiScale);
    presetBrowserBox.setBounds(topBar.removeFromLeft(presetBoxW)
                                   .withSizeKeepingCentre(presetBoxW, globalBypassHeight + 6));
    presetDeltaToggle.setBounds({0, 0, 0, 0});

    auto content = bounds;
    const int metersWidth = static_cast<int>(kRightPanelWidth * uiScale);
    auto rightPanel = content.removeFromRight(metersWidth);
    auto leftContent = content;
    const int analyzerHeight = static_cast<int>(leftContent.getHeight() * 0.52f);
    auto analyzerArea = leftContent.removeFromTop(analyzerHeight);
    analyzerBounds = analyzerArea;
    analyzer.setBounds(analyzerArea);

    auto controlsArea = leftContent;
    auto metersArea = rightPanel;
    // RMS/Peak toggles match copy/paste button size (58 width, 22 height).
    // Center them above the meters.
    const int meterToggleH = 22;  // Same as kRowHeight in BandControlsPanel
    auto meterToggleArea = metersArea.removeFromTop(meterToggleH);
    const int meterToggleW = 58;  // Same as btnW in BandControlsPanel
    const int meterToggleGap = static_cast<int>(6 * uiScale);
    const int totalToggleWidth = meterToggleW * 2 + meterToggleGap;
    // Center the toggles in the available space.
    auto toggleRow = meterToggleArea.withSizeKeepingCentre(totalToggleWidth, meterToggleH);
    rmsToggle.setBounds(toggleRow.removeFromLeft(meterToggleW)
                            .withSizeKeepingCentre(meterToggleW, meterToggleH));
    toggleRow.removeFromLeft(meterToggleGap);
    peakToggle.setBounds(toggleRow.removeFromLeft(meterToggleW)
                             .withSizeKeepingCentre(meterToggleW, meterToggleH));
    // Output knob should be same size as other rotaries (86 pixels).
    const int outputKnobSize = 86;  // Same as knobSize in BandControlsPanel
    const int trimLabelHeight = static_cast<int>(14 * uiScale);
    auto trimArea = metersArea.removeFromBottom(outputKnobSize + trimLabelHeight + static_cast<int>(10 * uiScale));
    auto outputArea = trimArea.removeFromLeft(trimArea.getWidth() / 2);
    // Center the Output label above the slider (use knob width for centering).
    auto outputLabelArea = outputArea.removeFromTop(trimLabelHeight);
    outputTrimLabel.setBounds(outputLabelArea.withSizeKeepingCentre(outputKnobSize, trimLabelHeight));
    outputTrimSlider.setBounds(outputArea.withSizeKeepingCentre(outputKnobSize, outputKnobSize));
    // Center the Auto Gain label above the toggle button.
    const int autoGainToggleWidth = static_cast<int>(60 * uiScale);
    auto autoGainLabelArea = trimArea.removeFromTop(trimLabelHeight);
    autoGainLabel.setBounds(autoGainLabelArea.withSizeKeepingCentre(autoGainToggleWidth, trimLabelHeight));
    autoGainToggle.setBounds(trimArea.withSizeKeepingCentre(autoGainToggleWidth,
                                                            static_cast<int>(22 * uiScale)));

    meters.setBounds(metersArea);
    const int processingRowHeight = static_cast<int>(28 * uiScale);
    auto processingRow = controlsArea.removeFromBottom(processingRowHeight);
    const int phaseLabelWidth = static_cast<int>(
        phaseLabel.getFont().getStringWidthFloat(phaseLabel.getText()) + 10 * uiScale);
    phaseLabel.setBounds(processingRow.removeFromLeft(phaseLabelWidth));
    phaseModeBox.setBounds(processingRow.removeFromLeft(static_cast<int>(140 * uiScale)));
    const int qualityLabelWidth = static_cast<int>(
        qualityLabel.getFont().getStringWidthFloat(qualityLabel.getText()) + 10 * uiScale);
    qualityLabel.setBounds(processingRow.removeFromLeft(qualityLabelWidth));
    linearQualityBox.setBounds(processingRow.removeFromLeft(static_cast<int>(120 * uiScale)));
    
    // v4.5 beta: Global Harmonic layer oversampling toggles (next to quality at the bottom)
    // These are always visible and positioned next to the quality dropdown
    processingRow.removeFromLeft(static_cast<int>(8 * uiScale));  // Gap after quality
    const int harmonicOversamplingLabelWidth = static_cast<int>(
        harmonicLayerOversamplingLabel.getFont().getStringWidthFloat(harmonicLayerOversamplingLabel.getText()) + 10 * uiScale);
    harmonicLayerOversamplingLabel.setBounds(processingRow.removeFromLeft(harmonicOversamplingLabelWidth));
    const int toggleWidth = static_cast<int>(50 * uiScale);
    const int toggleGap = static_cast<int>(4 * uiScale);
    harmonicLayerOversamplingNoneToggle.setBounds(processingRow.removeFromLeft(toggleWidth));
    processingRow.removeFromLeft(toggleGap);
    harmonicLayerOversampling2xToggle.setBounds(processingRow.removeFromLeft(toggleWidth));
    processingRow.removeFromLeft(toggleGap);
    harmonicLayerOversampling4xToggle.setBounds(processingRow.removeFromLeft(toggleWidth));
    processingRow.removeFromLeft(toggleGap);
    harmonicLayerOversampling8xToggle.setBounds(processingRow.removeFromLeft(toggleWidth));
    processingRow.removeFromLeft(toggleGap);
    harmonicLayerOversampling16xToggle.setBounds(processingRow.removeFromLeft(toggleWidth));
    const auto bandArea = controlsArea.reduced(static_cast<int>(6 * uiScale), 0);
    bandBounds = bandArea;
    bandControls.setBounds(bandArea);
    auto gonioArea = bandArea;
    gonioArea = gonioArea.removeFromRight(static_cast<int>(bandArea.getWidth() * 0.38f));
    gonioArea = gonioArea.reduced(static_cast<int>(6 * uiScale));
    correlation.setBounds(gonioArea);

    spectralPanel.setBounds({0, 0, 0, 0});

    characterLabel.setBounds({0, 0, 0, 0});
    characterBox.setBounds({0, 0, 0, 0});
    qModeLabel.setBounds({0, 0, 0, 0});
    qModeBox.setBounds({0, 0, 0, 0});
    qAmountLabel.setBounds({0, 0, 0, 0});
    qAmountSlider.setBounds({0, 0, 0, 0});
    // Channel selector: use calculated max width to accommodate longest channel names (immersive formats).
    // If max width hasn't been calculated yet, use a reasonable default.
    const int channelSelectorWidth = channelSelectorMaxWidth > 0 
        ? channelSelectorMaxWidth 
        : static_cast<int>(180 * uiScale);  // Default fallback width
    const int channelLabelWidth = static_cast<int>(
        channelLabel.getFont().getStringWidthFloat(channelLabel.getText()) + 8 * uiScale);
    
    // Position channel selector in top bar if there's space, otherwise keep it hidden for now.
    // For now, keep it hidden but with proper width calculation for when it's displayed.
    channelLabel.setBounds({0, 0, 0, 0});
    channelSelector.setBounds({0, 0, channelSelectorWidth, static_cast<int>(24 * uiScale)});
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
