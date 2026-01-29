#include "BandControlsPanel.h"
#include "../PluginProcessor.h"
#include "../util/ColorUtils.h"

// Per-band control panel implementation and UI state caching.

namespace
{
// v4.5 beta: All labels in uppercase for graphical consistency
const juce::StringArray kFilterTypeChoices {
    "BELL",
    "LOW SHELF",
    "HIGH SHELF",
    "LOW PASS",
    "HIGH PASS",
    "NOTCH",
    "BAND PASS",
    "ALL PASS",
    "TILT",
    "FLAT TILT"
};

// v4.5 beta: All labels in uppercase for graphical consistency
const juce::StringArray kMsChoices {
    "ALL",
    "STEREO FRONT",
    "L",
    "R",
    "MID FRONT",
    "SIDE FRONT",
    "C",
    "LFE",
    "STEREO REAR",
    "LS",
    "RS",
    "MID REAR",
    "SIDE REAR",
    "STEREO LATERAL",
    "LRS",
    "RRS",
    "MID LATERAL",
    "SIDE LATERAL",
    "CS",
    "STEREO FRONT WIDE",
    "LW",
    "RW",
    "MID FRONT WIDE",
    "SIDE FRONT WIDE",
    "STEREO TOP FRONT",
    "TFL",
    "TFR",
    "MID TOP FRONT",
    "SIDE TOP FRONT",
    "STEREO TOP REAR",
    "TRL",
    "TRR",
    "MID TOP REAR",
    "SIDE TOP REAR",
    "STEREO TOP MIDDLE",
    "TML",
    "TMR",
    "MID TOP MIDDLE",
    "SIDE TOP MIDDLE"
};

enum MsChoiceIndex
{
    kMsAll = 0,
    kMsStereoFront,
    kMsLeft,
    kMsRight,
    kMsMidFront,
    kMsSideFront,
    kMsCentre,
    kMsLfe,
    kMsStereoRear,
    kMsLs,
    kMsRs,
    kMsMidRear,
    kMsSideRear,
    kMsStereoLateral,
    kMsLrs,
    kMsRrs,
    kMsMidLateral,
    kMsSideLateral,
    kMsCs,
    kMsStereoFrontWide,
    kMsLw,
    kMsRw,
    kMsMidFrontWide,
    kMsSideFrontWide,
    kMsStereoTopFront,
    kMsTfl,
    kMsTfr,
    kMsMidTopFront,
    kMsSideTopFront,
    kMsStereoTopRear,
    kMsTrl,
    kMsTrr,
    kMsMidTopRear,
    kMsSideTopRear,
    kMsStereoTopMiddle,
    kMsTml,
    kMsTmr,
    kMsMidTopMiddle,
    kMsSideTopMiddle
};

bool containsName(const std::vector<juce::String>& names, const juce::String& target)
{
    return std::find(names.begin(), names.end(), target) != names.end();
}

constexpr int kPanelPadding = 10;
constexpr int kRowHeight = 22;
constexpr int kLabelHeight = 14;
constexpr int kComboHeight = 20;
constexpr int kGap = 8;
constexpr int kKnobRowHeight = 124;

juce::String formatFrequency(float value)
{
    if (value >= 10000.0f)
        return juce::String(value / 1000.0f, 1) + "kHz";
    if (value >= 1000.0f)
        return juce::String(value / 1000.0f, 2) + "kHz";
    return juce::String(value, 0) + "Hz";
}

} // namespace

BandControlsPanel::BandControlsPanel(EQProAudioProcessor& processorIn)
    : processor(processorIn),
      parameters(processorIn.getParameters())
{
    // v4.5 beta: Defer timer start to avoid expensive repaints before components are laid out.
    // Timer will start after first resize to ensure proper initialization.
    // This prevents UI blocking operations during component construction
    hasBeenResized = false;
    // v4.5 beta: Use buffered rendering for better performance on initial load
    // Reduces repaint overhead and ensures controls appear immediately
    setBufferedToImage(true);
    channelNames = processor.getCurrentChannelNames();
    for (auto& fade : bandHoverFade)
    {
        fade.reset(30.0, 0.18);
        fade.setCurrentAndTargetValue(0.0f);
    }
    for (auto& fade : bandSelectFade)
    {
        fade.reset(30.0, 0.18);
        fade.setCurrentAndTargetValue(0.0f);
    }
    for (auto& fade : bandActiveFade)
    {
        fade.reset(30.0, 0.18);
        fade.setCurrentAndTargetValue(1.0f);
    }

    // v4.5 beta: Remove "BAND" label, keep only number to save space
    // v4.5 beta: Display only the current band number (e.g., "1" instead of "1 / 12")
    titleLabel.setText("1", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, theme.text);
    addAndMakeVisible(titleLabel);

    eqSectionLabel.setText("EQ PARAMETERS", juce::dontSendNotification);
    eqSectionLabel.setJustificationType(juce::Justification::centredLeft);
    eqSectionLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    eqSectionLabel.setColour(juce::Label::textColourId, theme.accent);
    addAndMakeVisible(eqSectionLabel);
    eqSectionLabel.setVisible(false);

    // v4.5 beta: All button text in uppercase for graphical consistency
    copyButton.setButtonText("COPY");
    copyButton.setTooltip("Copy this band's settings");
    copyButton.onClick = [this] { copyBandState(); };
    addAndMakeVisible(copyButton);
    copyButton.setVisible(true);

    pasteButton.setButtonText("PASTE");
    pasteButton.setTooltip("Paste copied band settings");
    pasteButton.onClick = [this] { pasteBandState(); };
    addAndMakeVisible(pasteButton);
    pasteButton.setVisible(true);

    defaultButton.setButtonText("RESET BAND");
    defaultButton.setTooltip("Reset current band");
    defaultButton.onClick = [this] { resetSelectedBand(); };
    addAndMakeVisible(defaultButton);
    defaultButton.setVisible(true);

    resetAllButton.setButtonText("RESET ALL");
    resetAllButton.setTooltip("Reset all bands");
    resetAllButton.onClick = [this] { resetAllBands(); };
    addAndMakeVisible(resetAllButton);

    // Band navigation arrows (previous/next).
    prevBandButton.setButtonText("<");
    prevBandButton.setTooltip("Previous band");
    prevBandButton.onClick = [this]
    {
        const int target = findNextExisting(selectedBand, -1);
        if (onBandNavigate)
            onBandNavigate(target);
    };
    addAndMakeVisible(prevBandButton);

    nextBandButton.setButtonText(">");
    nextBandButton.setTooltip("Next band");
    nextBandButton.onClick = [this]
    {
        const int target = findNextExisting(selectedBand, +1);
        if (onBandNavigate)
            onBandNavigate(target);
    };
    addAndMakeVisible(nextBandButton);
    
    // v4.5 beta: Layer toggle buttons (EQ/Harmonic) next to band navigator
    eqLayerToggle.setButtonText("EQ");
    eqLayerToggle.setClickingTogglesState(true);
    eqLayerToggle.setToggleState(true, juce::dontSendNotification);  // EQ is default
    eqLayerToggle.setTooltip("EQ Layer");
    eqLayerToggle.onClick = [this]
    {
        if (eqLayerToggle.getToggleState())
        {
            harmonicLayerToggle.setToggleState(false, juce::dontSendNotification);
            setLayer(BandControlsPanel::LayerType::EQ);
        }
        else
        {
            eqLayerToggle.setToggleState(true, juce::dontSendNotification);  // Keep one active
        }
    };
    addAndMakeVisible(eqLayerToggle);
    
    harmonicLayerToggle.setButtonText("HARMONIC");
    harmonicLayerToggle.setClickingTogglesState(true);
    harmonicLayerToggle.setToggleState(false, juce::dontSendNotification);
    harmonicLayerToggle.setTooltip("Harmonic Layer");
    harmonicLayerToggle.onClick = [this]
    {
        if (harmonicLayerToggle.getToggleState())
        {
            eqLayerToggle.setToggleState(false, juce::dontSendNotification);
            setLayer(BandControlsPanel::LayerType::Harmonic);
        }
        else
        {
            harmonicLayerToggle.setToggleState(true, juce::dontSendNotification);  // Keep one active
        }
    };
    addAndMakeVisible(harmonicLayerToggle);

    for (int i = 0; i < static_cast<int>(bandSelectButtons.size()); ++i)
    {
        auto& button = bandSelectButtons[static_cast<size_t>(i)];
        button.setButtonText(juce::String(i + 1));
        button.setTooltip("Select band " + juce::String(i + 1));
        button.setClickingTogglesState(true);
        button.onClick = [this, index = i]()
        {
            if (onBandNavigate)
                onBandNavigate(index);
        };
        button.onDoubleClick = [this, index = i]()
        {
            const auto bypassId = ParamIDs::bandParamId(selectedChannel, index, "bypass");
            if (auto* param = parameters.getParameter(bypassId))
            {
                const float current = param->getValue();
                const float target = current < 0.5f ? 1.0f : 0.0f;
                param->setValueNotifyingHost(target);
            }
        };
        addAndMakeVisible(button);
    }

    for (int i = 0; i < static_cast<int>(bandSoloButtons.size()); ++i)
    {
        auto& button = bandSoloButtons[static_cast<size_t>(i)];
        button.setButtonText("S");
        button.setTooltip("Solo band " + juce::String(i + 1));
        button.setClickingTogglesState(true);
        button.setColour(juce::ToggleButton::textColourId, theme.textMuted);
        button.onClick = [this, index = i]()
        {
            ensureBandActiveFromEdit();
            const bool enabled = bandSoloButtons[static_cast<size_t>(index)].getToggleState();
            if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, index, "solo")))
                param->setValueNotifyingHost(param->convertTo0to1(enabled ? 1.0f : 0.0f));

            if (enabled)
            {
                for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
                {
                    if (band == index)
                        continue;
                    if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, band, "solo")))
                        param->setValueNotifyingHost(param->convertTo0to1(0.0f));
                    bandSoloButtons[static_cast<size_t>(band)].setToggleState(false, juce::dontSendNotification);
                }
            }
        };
        button.onDoubleClick = [this, index = i]()
        {
            if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, index, "solo")))
                param->setValueNotifyingHost(param->convertTo0to1(0.0f));
            bandSoloButtons[static_cast<size_t>(index)].setToggleState(false, juce::dontSendNotification);
        };
        addAndMakeVisible(button);
    }

    auto initLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, theme.textMuted);
        label.setFont(12.0f);
        addAndMakeVisible(label);
    };

    // v4.5 beta: All labels in uppercase for graphical consistency
    initLabel(freqLabel, "FREQ");
    initLabel(gainLabel, "GAIN");
    initLabel(qLabel, "Q");
    initLabel(typeLabel, "TYPE");
    initLabel(msLabel, "CHANNEL");
    initLabel(slopeLabel, "SLOPE");
    initLabel(mixLabel, "BAND MIX");
    // v4.5 beta: Harmonic layer labels
    initLabel(oddLabel, "ODD");
    initLabel(mixOddLabel, "MIX ODD");
    initLabel(evenLabel, "EVEN");
    initLabel(mixEvenLabel, "MIX EVEN");
    initLabel(thresholdLabel, "THRESH");
    initLabel(attackLabel, "ATTACK");
    initLabel(releaseLabel, "RELEASE");

    freqSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    const int knobTextW = 68;
    const int knobTextH = 18;
    freqSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    freqSlider.setTextBoxIsEditable(true);
    freqSlider.setSkewFactorFromMidPoint(1000.0);
    freqSlider.setTextValueSuffix(" Hz");
    freqSlider.setRange(10.0, 30000.0, 0.01);
    freqSlider.setTooltip("Band frequency");
    freqSlider.onDoubleClick = [this]
    {
        if (freqParam != nullptr)
            freqParam->setValueNotifyingHost(freqParam->convertTo0to1(1000.0f));
    };
    freqSlider.onValueChange = [this]
    {
        if (suppressParamCallbacks)
            return;
        ensureBandActiveFromEdit();
        mirrorToLinkedChannel("freq", static_cast<float>(freqSlider.getValue()));
        cacheBandFromUi(selectedChannel, selectedBand);
    };
    addAndMakeVisible(freqSlider);

    gainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    gainSlider.setTextBoxIsEditable(true);
    gainSlider.setTextValueSuffix(" dB");
    gainSlider.setRange(-30.0, 30.0, 0.01);
    gainSlider.setTooltip("Band gain");
    gainSlider.onDoubleClick = [this]
    {
        if (gainParam != nullptr)
            gainParam->setValueNotifyingHost(gainParam->convertTo0to1(0.0f));
    };
    gainSlider.onValueChange = [this]
    {
        if (suppressParamCallbacks)
            return;
        ensureBandActiveFromEdit();
        mirrorToLinkedChannel("gain", static_cast<float>(gainSlider.getValue()));
        cacheBandFromUi(selectedChannel, selectedBand);
    };
    addAndMakeVisible(gainSlider);

    qSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    qSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    qSlider.setTextBoxIsEditable(true);
    qSlider.setRange(0.025, 40.0, 0.001);
    qSlider.setTooltip("Band Q");
    qSlider.onDoubleClick = [this]
    {
        if (qParam != nullptr)
            qParam->setValueNotifyingHost(qParam->convertTo0to1(0.707f));
    };
    qSlider.onValueChange = [this]
    {
        if (suppressParamCallbacks)
            return;
        ensureBandActiveFromEdit();
        mirrorToLinkedChannel("q", static_cast<float>(qSlider.getValue()));
        cacheBandFromUi(selectedChannel, selectedBand);
    };
    addAndMakeVisible(qSlider);

    typeBox.addItemList(kFilterTypeChoices, 1);
    typeBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    typeBox.setColour(juce::ComboBox::textColourId, theme.text);
    typeBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    typeBox.setLookAndFeel(&compactComboLookAndFeel);
    typeBox.setTooltip("Filter type");
    typeBox.onChange = [this]
    {
        ensureBandActiveFromEdit();
        const int index = typeBox.getSelectedItemIndex();
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "type")))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(index)));
        updateTypeUi();
        mirrorToLinkedChannel("type", static_cast<float>(index));
        cacheBandFromUi(selectedChannel, selectedBand);
    };
    addAndMakeVisible(typeBox);

    msBox.addItemList(kMsChoices, 1);
    msBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    msBox.setColour(juce::ComboBox::textColourId, theme.text);
    msBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    msBox.setLookAndFeel(&compactComboLookAndFeel);
    msBox.setTooltip("Channel target");
    addAndMakeVisible(msBox);
    msBox.onChange = [this]
    {
        if (msChoiceMap.empty())
            return;
        ensureBandActiveFromEdit();
        const int uiIndex = msBox.getSelectedItemIndex();
        if (uiIndex < 0 || uiIndex >= static_cast<int>(msChoiceMap.size()))
            return;
        const int paramIndex = msChoiceMap[static_cast<size_t>(uiIndex)];
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "ms")))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(paramIndex)));
        mirrorToLinkedChannel("ms", static_cast<float>(paramIndex));
        cacheBandFromUi(selectedChannel, selectedBand);
    };

    for (int i = 0; i < 16; ++i)
    {
        const int slopeValue = 6 * (i + 1);
        // v4.5 beta: Uppercase for consistency
        slopeBox.addItem(juce::String(slopeValue) + " DB", i + 1);
    }
    slopeBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    slopeBox.setColour(juce::ComboBox::textColourId, theme.text);
    slopeBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    // v4.5 beta: Use separate LookAndFeel with larger font for slope dropdown
    slopeBox.setLookAndFeel(&slopeComboLookAndFeel);
    slopeBox.setTooltip("Slope");
    slopeBox.onChange = [this]
    {
        const int index = slopeBox.getSelectedItemIndex();
        if (index < 0)
            return;
        ensureBandActiveFromEdit();
        const float slopeValue = static_cast<float>((index + 1) * 6);
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "slope")))
            param->setValueNotifyingHost(param->convertTo0to1(slopeValue));
        mirrorToLinkedChannel("slope", slopeValue);
        cacheBandFromUi(selectedChannel, selectedBand);
    };
    addAndMakeVisible(slopeBox);

    mixSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    mixSlider.setTextBoxIsEditable(true);
    mixSlider.setTextValueSuffix(" %");
    mixSlider.setTooltip("Band mix");
    mixSlider.onDoubleClick = [this]
    {
        if (mixParam != nullptr)
            mixParam->setValueNotifyingHost(mixParam->convertTo0to1(100.0f));
    };
    addAndMakeVisible(mixSlider);
    mixSlider.onValueChange = [this]
    {
        if (suppressParamCallbacks)
            return;
        ensureBandActiveFromEdit();
        mirrorToLinkedChannel("mix", static_cast<float>(mixSlider.getValue()));
        cacheBandFromUi(selectedChannel, selectedBand);
    };
    
    // v4.5 beta: Initialize harmonic controls (same style as EQ controls)
    oddHarmonicSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    oddHarmonicSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    oddHarmonicSlider.setTextBoxIsEditable(true);
    oddHarmonicSlider.setTextValueSuffix(" dB");
    oddHarmonicSlider.setRange(-24.0, 24.0, 0.1);
    oddHarmonicSlider.setValue(0.0);
    oddHarmonicSlider.setTooltip("Odd harmonic amount");
    oddHarmonicSlider.onDoubleClick = [this]
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "odd")))
            param->setValueNotifyingHost(param->convertTo0to1(0.0f));
    };
    addAndMakeVisible(oddHarmonicSlider);
    oddHarmonicSlider.onValueChange = [this]
    {
        if (suppressParamCallbacks)
            return;
        ensureBandActiveFromEdit();
        mirrorToLinkedChannel("odd", static_cast<float>(oddHarmonicSlider.getValue()));
        cacheBandFromUi(selectedChannel, selectedBand);
    };
    
    mixOddSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    mixOddSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    mixOddSlider.setTextBoxIsEditable(true);
    mixOddSlider.setTextValueSuffix(" %");
    mixOddSlider.setRange(0.0, 100.0, 0.1);
    mixOddSlider.setValue(100.0);
    mixOddSlider.setTooltip("Mix for odd harmonics");
    mixOddSlider.onDoubleClick = [this]
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "mixOdd")))
            param->setValueNotifyingHost(param->convertTo0to1(100.0f));
    };
    addAndMakeVisible(mixOddSlider);
    mixOddSlider.onValueChange = [this]
    {
        if (suppressParamCallbacks)
            return;
        ensureBandActiveFromEdit();
        mirrorToLinkedChannel("mixOdd", static_cast<float>(mixOddSlider.getValue()));
        cacheBandFromUi(selectedChannel, selectedBand);
    };
    
    evenHarmonicSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    evenHarmonicSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    evenHarmonicSlider.setTextBoxIsEditable(true);
    evenHarmonicSlider.setTextValueSuffix(" dB");
    evenHarmonicSlider.setRange(-24.0, 24.0, 0.1);
    evenHarmonicSlider.setValue(0.0);
    evenHarmonicSlider.setTooltip("Even harmonic amount");
    evenHarmonicSlider.onDoubleClick = [this]
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "even")))
            param->setValueNotifyingHost(param->convertTo0to1(0.0f));
    };
    addAndMakeVisible(evenHarmonicSlider);
    evenHarmonicSlider.onValueChange = [this]
    {
        if (suppressParamCallbacks)
            return;
        ensureBandActiveFromEdit();
        mirrorToLinkedChannel("even", static_cast<float>(evenHarmonicSlider.getValue()));
        cacheBandFromUi(selectedChannel, selectedBand);
    };
    
    mixEvenSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    mixEvenSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    mixEvenSlider.setTextBoxIsEditable(true);
    mixEvenSlider.setTextValueSuffix(" %");
    mixEvenSlider.setRange(0.0, 100.0, 0.1);
    mixEvenSlider.setValue(100.0);
    mixEvenSlider.setTooltip("Mix for even harmonics");
    mixEvenSlider.onDoubleClick = [this]
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "mixEven")))
            param->setValueNotifyingHost(param->convertTo0to1(100.0f));
    };
    addAndMakeVisible(mixEvenSlider);
    mixEvenSlider.onValueChange = [this]
    {
        if (suppressParamCallbacks)
            return;
        ensureBandActiveFromEdit();
        mirrorToLinkedChannel("mixEven", static_cast<float>(mixEvenSlider.getValue()));
        cacheBandFromUi(selectedChannel, selectedBand);
    };
    
    // v4.4 beta: Harmonic bypass toggle (per-band, independent for each of 12 bands)
    harmonicBypassToggle.setButtonText("BYPASS");
    harmonicBypassToggle.setClickingTogglesState(true);
    harmonicBypassToggle.setToggleState(false, juce::dontSendNotification);
    harmonicBypassToggle.setTooltip("Bypass harmonic processing for this band");
    harmonicBypassToggle.onClick = [this]
    {
        if (suppressParamCallbacks)
            return;
        ensureBandActiveFromEdit();
        cacheBandFromUi(selectedChannel, selectedBand);
    };
    addAndMakeVisible(harmonicBypassToggle);
    
    // Initially hide harmonic controls (EQ layer is default)
    updateLayerVisibility();

    dynEnableToggle.setButtonText("DYNAMIC");
    dynEnableToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    addAndMakeVisible(dynEnableToggle);
    dynEnableToggle.onClick = [this]
    {
        ensureBandActiveFromEdit();
        cacheBandFromUi(selectedChannel, selectedBand);
    };

    dynUpButton.setButtonText("UP");
    dynDownButton.setButtonText("DOWN");
    dynUpButton.setClickingTogglesState(true);
    dynDownButton.setClickingTogglesState(true);
    dynUpButton.onClick = [this]()
    {
        ensureBandActiveFromEdit();
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "dynMode")))
            param->setValueNotifyingHost(param->convertTo0to1(0.0f));
        dynUpButton.setToggleState(true, juce::dontSendNotification);
        dynDownButton.setToggleState(false, juce::dontSendNotification);
        cacheBandFromUi(selectedChannel, selectedBand);
    };
    dynDownButton.onClick = [this]()
    {
        ensureBandActiveFromEdit();
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "dynMode")))
            param->setValueNotifyingHost(param->convertTo0to1(1.0f));
        dynUpButton.setToggleState(false, juce::dontSendNotification);
        dynDownButton.setToggleState(true, juce::dontSendNotification);
        cacheBandFromUi(selectedChannel, selectedBand);
    };
    addAndMakeVisible(dynUpButton);
    addAndMakeVisible(dynDownButton);

    dynExternalToggle.setButtonText("EXT SC");
    dynExternalToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    addAndMakeVisible(dynExternalToggle);
    dynExternalToggle.onClick = [this]
    {
        ensureBandActiveFromEdit();
        cacheBandFromUi(selectedChannel, selectedBand);
    };

    thresholdSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    thresholdSlider.setTextBoxIsEditable(true);
    thresholdSlider.setTextValueSuffix(" dB");
    thresholdSlider.onDoubleClick = [this]
    {
        if (dynThreshParam != nullptr)
            dynThreshParam->setValueNotifyingHost(dynThreshParam->convertTo0to1(-24.0f));
    };
    addAndMakeVisible(thresholdSlider);
    thresholdSlider.onValueChange = [this]
    {
        if (suppressParamCallbacks)
            return;
        ensureBandActiveFromEdit();
        cacheBandFromUi(selectedChannel, selectedBand);
    };

    attackSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    attackSlider.setTextBoxIsEditable(true);
    attackSlider.setTextValueSuffix(" ms");
    attackSlider.onDoubleClick = [this]
    {
        if (dynAttackParam != nullptr)
            dynAttackParam->setValueNotifyingHost(dynAttackParam->convertTo0to1(20.0f));
    };
    addAndMakeVisible(attackSlider);
    attackSlider.onValueChange = [this]
    {
        if (suppressParamCallbacks)
            return;
        ensureBandActiveFromEdit();
        cacheBandFromUi(selectedChannel, selectedBand);
    };

    releaseSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    releaseSlider.setTextBoxIsEditable(true);
    releaseSlider.setTextValueSuffix(" ms");
    releaseSlider.onDoubleClick = [this]
    {
        if (dynReleaseParam != nullptr)
            dynReleaseParam->setValueNotifyingHost(dynReleaseParam->convertTo0to1(200.0f));
    };
    addAndMakeVisible(releaseSlider);
    releaseSlider.onValueChange = [this]
    {
        if (suppressParamCallbacks)
            return;
        ensureBandActiveFromEdit();
        cacheBandFromUi(selectedChannel, selectedBand);
    };

    autoScaleToggle.setButtonText("AUTO SCALE");
    autoScaleToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    addAndMakeVisible(autoScaleToggle);
    autoScaleToggle.onClick = [this]
    {
        ensureBandActiveFromEdit();
        cacheBandFromUi(selectedChannel, selectedBand);
    };

    suppressParamCallbacks = true;
    updateAttachments();
    updateBandKnobColours();
    updateMsChoices();
    updateComboBoxWidths();
    syncUiFromParams();
    updateTypeUi();
    suppressParamCallbacks = false;
    refreshCacheFromParams(selectedChannel);
}

void BandControlsPanel::ensureBandActiveFromEdit()
{
    if (suppressParamCallbacks || resetInProgress)
        return;
    if (auto* bypassParam = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "bypass")))
    {
        if (bypassParam->getValue() > 0.5f)
            bypassParam->setValueNotifyingHost(0.0f);
    }
}

BandControlsPanel::~BandControlsPanel()
{
    typeBox.setLookAndFeel(nullptr);
    msBox.setLookAndFeel(nullptr);
    slopeBox.setLookAndFeel(nullptr);
}

void BandControlsPanel::setSelectedBand(int channelIndex, int bandIndex)
{
    cacheBandFromUi(selectedChannel, selectedBand);
    applyCachedBandToParams(selectedChannel);
    if (selectedChannel != channelIndex || selectedBand != bandIndex)
        pushUiStateToParams();
    selectedChannel = juce::jlimit(0, ParamIDs::kMaxChannels - 1, channelIndex);
    selectedBand = juce::jlimit(0, ParamIDs::kBandsPerChannel - 1, bandIndex);

        // v4.5 beta: Display only the current band number (e.g., "1" instead of "1 / 12")
    titleLabel.setText(juce::String(selectedBand + 1),
                       juce::dontSendNotification);
    for (int i = 0; i < static_cast<int>(bandSelectButtons.size()); ++i)
    {
        auto& button = bandSelectButtons[static_cast<size_t>(i)];
        button.setToggleState(i == selectedBand, juce::dontSendNotification);
        const auto colour = ColorUtils::bandColour(i);
        button.setColour(juce::TextButton::buttonColourId, colour.withAlpha(0.2f));
        button.setColour(juce::TextButton::buttonOnColourId, colour.withAlpha(0.55f));
        button.setColour(juce::TextButton::textColourOffId, colour.withAlpha(0.9f));
        button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }
    const auto bandColour = ColorUtils::bandColour(selectedBand);
    titleLabel.setColour(juce::Label::textColourId, bandColour);
    updateBandKnobColours();
    suppressParamCallbacks = true;
    updateAttachments();
    updateMsChoices();
    updateComboBoxWidths();
    restoreBandFromCache();
    syncUiFromParams();
    updateTypeUi();
    suppressParamCallbacks = false;
    applyCachedBandToParams(selectedChannel);
    
    // Force repaint to ensure frame color updates for all 12 bands.
    repaint();
}

void BandControlsPanel::pushUiStateToParams()
{
    const int channel = selectedChannel;
    const int band = selectedBand;
    auto setParamValue = [this, channel, band](const juce::String& suffix, float value)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*>(
                parameters.getParameter(ParamIDs::bandParamId(channel, band, suffix))))
        {
            param->setValueNotifyingHost(param->convertTo0to1(value));
        }
    };

    setParamValue("freq", static_cast<float>(freqSlider.getValue()));
    setParamValue("gain", static_cast<float>(gainSlider.getValue()));
    setParamValue("q", static_cast<float>(qSlider.getValue()));

    const int typeIndex = typeBox.getSelectedItemIndex();
    if (typeIndex >= 0)
        setParamValue("type", static_cast<float>(typeIndex));

    const int msIndex = msBox.getSelectedItemIndex();
    if (msIndex >= 0 && msIndex < static_cast<int>(msChoiceMap.size()))
        setParamValue("ms", static_cast<float>(msChoiceMap[static_cast<size_t>(msIndex)]));

    const int slopeIndex = slopeBox.getSelectedItemIndex();
    if (slopeIndex >= 0)
        setParamValue("slope", static_cast<float>((slopeIndex + 1) * 6));

    setParamValue("mix", static_cast<float>(mixSlider.getValue()));
    setParamValue("dynEnable", dynEnableToggle.getToggleState() ? 1.0f : 0.0f);
    setParamValue("dynMode", dynDownButton.getToggleState() ? 1.0f : 0.0f);
    setParamValue("dynThresh", static_cast<float>(thresholdSlider.getValue()));
    setParamValue("dynAttack", static_cast<float>(attackSlider.getValue()));
    setParamValue("dynRelease", static_cast<float>(releaseSlider.getValue()));
    setParamValue("dynAuto", autoScaleToggle.getToggleState() ? 1.0f : 0.0f);
    setParamValue("dynExternal", dynExternalToggle.getToggleState() ? 1.0f : 0.0f);
}

void BandControlsPanel::setChannelNames(const std::vector<juce::String>& names)
{
    if (channelNames == names)
        return;
    channelNames = names;
    updateMsChoices();
    updateComboBoxWidths();
    syncUiFromParams();
}

void BandControlsPanel::setTheme(const ThemeColors& newTheme)
{
    theme = newTheme;
    titleLabel.setColour(juce::Label::textColourId, ColorUtils::bandColour(selectedBand));
    eqSectionLabel.setColour(juce::Label::textColourId, theme.accent);
    freqLabel.setColour(juce::Label::textColourId, theme.textMuted);
    gainLabel.setColour(juce::Label::textColourId, theme.textMuted);
    qLabel.setColour(juce::Label::textColourId, theme.textMuted);
    typeLabel.setColour(juce::Label::textColourId, theme.textMuted);
    msLabel.setColour(juce::Label::textColourId, theme.textMuted);
    slopeLabel.setColour(juce::Label::textColourId, theme.textMuted);
    mixLabel.setColour(juce::Label::textColourId, theme.textMuted);
    // v4.5 beta: Harmonic layer labels
    oddLabel.setColour(juce::Label::textColourId, theme.textMuted);
    mixOddLabel.setColour(juce::Label::textColourId, theme.textMuted);
    evenLabel.setColour(juce::Label::textColourId, theme.textMuted);
    mixEvenLabel.setColour(juce::Label::textColourId, theme.textMuted);
    harmonicBypassToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    msBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    msBox.setColour(juce::ComboBox::textColourId, theme.text);
    msBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    slopeBox.setColour(juce::ComboBox::textColourId, theme.text);
    updateBandKnobColours();
    mixSlider.setColour(juce::Slider::textBoxTextColourId, theme.text);
    mixSlider.setColour(juce::Slider::textBoxOutlineColourId, theme.panelOutline);
    thresholdSlider.setColour(juce::Slider::textBoxTextColourId, theme.text);
    thresholdSlider.setColour(juce::Slider::textBoxOutlineColourId, theme.panelOutline);
    attackSlider.setColour(juce::Slider::textBoxTextColourId, theme.text);
    attackSlider.setColour(juce::Slider::textBoxOutlineColourId, theme.panelOutline);
    releaseSlider.setColour(juce::Slider::textBoxTextColourId, theme.text);
    releaseSlider.setColour(juce::Slider::textBoxOutlineColourId, theme.panelOutline);
    copyButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    pasteButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    defaultButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    resetAllButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    prevBandButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    nextBandButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    for (int i = 0; i < static_cast<int>(bandSelectButtons.size()); ++i)
    {
        auto& button = bandSelectButtons[static_cast<size_t>(i)];
        const auto colour = ColorUtils::bandColour(i);
        button.setColour(juce::TextButton::buttonColourId, colour.withAlpha(0.2f));
        button.setColour(juce::TextButton::buttonOnColourId, colour.withAlpha(0.55f));
        button.setColour(juce::TextButton::textColourOffId, colour.withAlpha(0.9f));
        button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }
    for (auto& button : bandSoloButtons)
        button.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    dynEnableToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    dynUpButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    dynDownButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    autoScaleToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    dynExternalToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    repaint();
}

void BandControlsPanel::updateBandKnobColours()
{
    const auto bandColour = ColorUtils::bandColour(selectedBand);
    auto applyColour = [bandColour](juce::Slider& slider)
    {
        slider.setColour(juce::Slider::trackColourId, bandColour);
        slider.setColour(juce::Slider::rotarySliderFillColourId, bandColour);
        slider.setColour(juce::Slider::rotarySliderOutlineColourId, bandColour.darker(0.6f));
        slider.setColour(juce::Slider::thumbColourId, bandColour);
    };
    applyColour(freqSlider);
    applyColour(gainSlider);
    applyColour(qSlider);
    applyColour(mixSlider);
    // v4.4 beta: Apply band colors to harmonic controls
    applyColour(oddHarmonicSlider);
    applyColour(mixOddSlider);
    applyColour(evenHarmonicSlider);
    applyColour(mixEvenSlider);
    applyColour(thresholdSlider);
    applyColour(attackSlider);
    applyColour(releaseSlider);
}

// v4.4 beta: Layer switching and visibility management
void BandControlsPanel::setLayer(BandControlsPanel::LayerType layer)
{
    if (currentLayer == layer)
        return;
    
    currentLayer = layer;
    updateLayerVisibility();
    updateAttachments();  // Update parameter attachments for current layer
    syncUiFromParams();   // Sync UI with parameters
    resized();  // v4.4 beta: Force layout update to reposition controls for new layer
    repaint();
}

void BandControlsPanel::updateLayerVisibility()
{
    const bool isEQLayer = (currentLayer == BandControlsPanel::LayerType::EQ);
    
    // EQ layer controls
    freqLabel.setVisible(isEQLayer);
    gainLabel.setVisible(isEQLayer);
    qLabel.setVisible(isEQLayer);
    mixLabel.setVisible(isEQLayer);
    freqSlider.setVisible(isEQLayer);
    gainSlider.setVisible(isEQLayer);
    qSlider.setVisible(isEQLayer);
    mixSlider.setVisible(isEQLayer);
    
    // Harmonic layer controls - v4.4 beta: Ensure they're visible and properly added
    oddLabel.setVisible(!isEQLayer);
    mixOddLabel.setVisible(!isEQLayer);
    evenLabel.setVisible(!isEQLayer);
    mixEvenLabel.setVisible(!isEQLayer);
    oddHarmonicSlider.setVisible(!isEQLayer);
    mixOddSlider.setVisible(!isEQLayer);
    evenHarmonicSlider.setVisible(!isEQLayer);
    mixEvenSlider.setVisible(!isEQLayer);
    harmonicBypassToggle.setVisible(!isEQLayer);  // v4.4 beta: Show bypass only on Harmonic layer
    // v4.4 beta: Ensure harmonic controls are added to component tree and enabled
    if (!isEQLayer)
    {
        addAndMakeVisible(oddLabel);
        addAndMakeVisible(mixOddLabel);
        addAndMakeVisible(evenLabel);
        addAndMakeVisible(mixEvenLabel);
        addAndMakeVisible(oddHarmonicSlider);
        addAndMakeVisible(mixOddSlider);
        addAndMakeVisible(evenHarmonicSlider);
        addAndMakeVisible(mixEvenSlider);
        addAndMakeVisible(harmonicBypassToggle);
        
        // Enable all harmonic controls
        oddHarmonicSlider.setEnabled(true);
        mixOddSlider.setEnabled(true);
        evenHarmonicSlider.setEnabled(true);
        mixEvenSlider.setEnabled(true);
        harmonicBypassToggle.setEnabled(true);
    }
    
    // Dropdowns: hide on Harmonic layer
    typeLabel.setVisible(isEQLayer);
    typeBox.setVisible(isEQLayer);
    slopeLabel.setVisible(isEQLayer);
    slopeBox.setVisible(isEQLayer);
    msLabel.setVisible(isEQLayer);
    msBox.setVisible(isEQLayer);
}

void BandControlsPanel::setMsEnabled(bool enabled)
{
    msEnabled = enabled;
    msBox.setEnabled(msEnabled);
    msBox.setAlpha(msEnabled ? 1.0f : 0.5f);
}


void BandControlsPanel::paint(juce::Graphics& g)
{
    // v4.4 beta: Skip expensive painting if component isn't properly initialized or visible
    // Prevents rendering operations on uninitialized components, ensuring instant GUI loading
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0 || !isVisible())
        return;
    // Main panel background (fixed color, not band-dependent).
    g.setColour(theme.panel);
    g.fillRoundedRectangle(bounds, 8.0f);
    // Main panel outline (fixed color).
    g.setColour(theme.panelOutline.withAlpha(0.75f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.2f);

    auto layout = getLocalBounds().reduced(kPanelPadding);
    auto left = layout.removeFromLeft(static_cast<int>(layout.getWidth() * 0.62f));
    auto headerArea = left.removeFromTop(kRowHeight);
    const auto bandRowArea = left.removeFromTop(kRowHeight);
    left.removeFromTop(2);
    const auto soloRowArea = left.removeFromTop(kRowHeight);
    left.removeFromTop(kGap);
    const auto knobsArea = left.removeFromTop(kKnobRowHeight);
    left.removeFromTop(kGap);
    const auto comboRowArea = left.removeFromTop(kLabelHeight + kRowHeight);

    // Get band color for rotary section frame (ensure selectedBand is valid for all 12 bands).
    // Frame color changes dynamically based on selected band - works for all 12 bands (0-11).
    // Clamp to ensure valid index range for all bands.
    const int safeBandIndex = juce::jlimit(0, ParamIDs::kBandsPerChannel - 1, selectedBand);
    const auto bandColour = ColorUtils::bandColour(safeBandIndex);
    
    // Frame around ONLY the 4 rotaries (not including dropdowns) - dynamically colored by selected band.
    // No individual frames around each knob - single unified frame that changes color.
    // This frame color updates automatically when selectedBand changes (0-11 for all 12 bands).
    const auto rotaryFrameArea = juce::Rectangle<float>(
        static_cast<float>(knobsArea.getX()),
        static_cast<float>(knobsArea.getY()),
        static_cast<float>(knobsArea.getWidth()),
        static_cast<float>(knobsArea.getHeight())
    ).reduced(4.0f);
    // Background fill for rotary frame - uses current selected band color.
    g.setColour(bandColour.darker(0.8f).withAlpha(0.4f));
    g.fillRoundedRectangle(rotaryFrameArea, 8.0f);
    // Outline for rotary frame - color changes dynamically with selected band (all 12 bands).
    g.setColour(bandColour.withAlpha(0.75f));
    g.drawRoundedRectangle(rotaryFrameArea, 8.0f, 1.5f);

    const float glowAlpha = juce::jlimit(0.0f, 1.0f, selectedBandGlow);
    if (glowAlpha > 0.01f)
    {
        g.setColour(bandColour.withAlpha(0.12f * glowAlpha));
        g.fillRoundedRectangle(headerArea.toFloat().expanded(2.0f, 1.0f), 6.0f);
    }

    g.setColour(theme.panel.darker(0.25f).withAlpha(0.8f));
    g.fillRoundedRectangle(headerArea.toFloat(), 6.0f);
    g.setColour(bandColour.withAlpha(0.6f));
    g.drawRoundedRectangle(headerArea.toFloat(), 6.0f, 1.0f);

    if (titleLabel.getBounds().getWidth() > 0)
    {
        auto chip = titleLabel.getBounds().toFloat().removeFromLeft(14.0f);
        chip = chip.withSizeKeepingCentre(10.0f, 10.0f);
        g.setColour(bandColour.withAlpha(0.95f));
        g.fillEllipse(chip);
        g.setColour(theme.panel.withAlpha(0.9f));
        g.drawEllipse(chip, 1.0f);
    }

    // v4.2: Removed residual lines under band toggles, solo toggles, and dropdown menus.
    // These lines were visual artifacts that cluttered the UI and were not part of the intended design.

    if (detectorMeterBounds.getWidth() > 1.0f && detectorMeterBounds.getHeight() > 1.0f)
    {
        const auto meter = detectorMeterBounds;
        g.setColour(theme.panelOutline.withAlpha(0.6f));
        g.fillRoundedRectangle(meter, 4.0f);

        const auto inner = meter.reduced(5.0f);
        const auto start = juce::Point<float>(inner.getX(), inner.getBottom());
        const auto end = juce::Point<float>(inner.getRight(), inner.getY());
        g.setColour(theme.textMuted.withAlpha(0.7f));
        g.drawLine({ start, end }, 1.2f);

        const float clampedDb = juce::jlimit(-60.0f, 0.0f, detectorDb);
        const float norm = (clampedDb + 60.0f) / 60.0f;
        const auto ball = juce::Point<float>(juce::jmap(norm, start.x, end.x),
                                             juce::jmap(norm, start.y, end.y));
        g.setColour(ColorUtils::bandColour(selectedBand).withAlpha(0.95f));
        g.fillEllipse(ball.x - 4.5f, ball.y - 4.5f, 9.0f, 9.0f);
        g.setColour(theme.panel.withAlpha(0.9f));
        g.drawEllipse(ball.x - 4.5f, ball.y - 4.5f, 9.0f, 9.0f, 1.2f);

        const float thresh = static_cast<float>(thresholdSlider.getValue());
        const float threshNorm = (juce::jlimit(-60.0f, 0.0f, thresh) + 60.0f) / 60.0f;
        const auto threshPoint = juce::Point<float>(juce::jmap(threshNorm, start.x, end.x),
                                                    juce::jmap(threshNorm, start.y, end.y));
        g.setColour(theme.textMuted);
        g.drawEllipse(threshPoint.x - 3.0f, threshPoint.y - 3.0f, 6.0f, 6.0f, 1.0f);
    }

    auto drawFocus = [&](const juce::Component& comp)
    {
        if (! comp.hasKeyboardFocus(true))
            return;
        auto rect = comp.getBounds().toFloat().expanded(2.0f);
        g.setColour(theme.accent.withAlpha(0.55f));
        g.drawRoundedRectangle(rect, 4.0f, 1.2f);
    };

    auto drawValuePill = [&](const juce::Component& comp, const juce::String& text)
    {
        if (text.isEmpty())
            return;
        g.setFont(11.0f);
        const float padX = 6.0f;
        const float padY = 3.0f;
        const float textW = g.getCurrentFont().getStringWidthFloat(text);
        const float pillW = textW + padX * 2.0f;
        const float pillH = 16.0f;
        auto rect = comp.getBounds().toFloat();
        auto pill = juce::Rectangle<float>(rect.getCentreX() - pillW * 0.5f,
                                           rect.getY() - pillH - 4.0f,
                                           pillW, pillH);
        pill = pill.getIntersection(getLocalBounds().toFloat().reduced(2.0f));
        g.setColour(theme.panel.darker(0.35f).withAlpha(0.92f));
        g.fillRoundedRectangle(pill, 4.0f);
        g.setColour(theme.panelOutline.withAlpha(0.8f));
        g.drawRoundedRectangle(pill, 4.0f, 1.0f);
        g.setColour(theme.text);
        g.drawFittedText(text, pill.toNearestInt(), juce::Justification::centred, 1);
    };

    drawFocus(freqSlider);
    drawFocus(gainSlider);
    drawFocus(qSlider);
    drawFocus(mixSlider);
    drawFocus(typeBox);
    drawFocus(msBox);
    // v4.4 beta: Only draw focus for visible controls
    if (currentLayer == BandControlsPanel::LayerType::EQ)
    {
        drawFocus(typeBox);
        drawFocus(msBox);
        drawFocus(slopeBox);
    }

    if (freqSlider.isMouseButtonDown() || freqSlider.hasKeyboardFocus(true))
        drawValuePill(freqSlider, formatFrequency(static_cast<float>(freqSlider.getValue())));
    if (gainSlider.isMouseButtonDown() || gainSlider.hasKeyboardFocus(true))
        drawValuePill(gainSlider, juce::String(gainSlider.getValue(), 1) + " dB");
    if (qSlider.isMouseButtonDown() || qSlider.hasKeyboardFocus(true))
        drawValuePill(qSlider, "Q " + juce::String(qSlider.getValue(), 2));
    if (mixSlider.isMouseButtonDown() || mixSlider.hasKeyboardFocus(true))
        drawValuePill(mixSlider, juce::String(mixSlider.getValue(), 0) + " %");
    
    // v4.4 beta: Draw value pills for harmonic controls
    if (currentLayer == BandControlsPanel::LayerType::Harmonic)
    {
        if (oddHarmonicSlider.isMouseButtonDown() || oddHarmonicSlider.hasKeyboardFocus(true))
            drawValuePill(oddHarmonicSlider, juce::String(oddHarmonicSlider.getValue(), 1) + " dB");
        if (mixOddSlider.isMouseButtonDown() || mixOddSlider.hasKeyboardFocus(true))
            drawValuePill(mixOddSlider, juce::String(mixOddSlider.getValue(), 0) + " %");
        if (evenHarmonicSlider.isMouseButtonDown() || evenHarmonicSlider.hasKeyboardFocus(true))
            drawValuePill(evenHarmonicSlider, juce::String(evenHarmonicSlider.getValue(), 1) + " dB");
        if (mixEvenSlider.isMouseButtonDown() || mixEvenSlider.hasKeyboardFocus(true))
            drawValuePill(mixEvenSlider, juce::String(mixEvenSlider.getValue(), 0) + " %");
    }
}

void BandControlsPanel::timerCallback()
{
    detectorDb = processor.getBandDetectorDb(selectedChannel, selectedBand);
    cacheBandFromParams(selectedChannel, selectedBand);
    int soloCount = 0;
    int soloIndex = -1;
    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, band, "solo")))
        {
            if (param->getValue() > 0.5f)
            {
                ++soloCount;
                if (soloIndex < 0)
                    soloIndex = band;
            }
        }
    }
    if (soloCount > 1 && soloIndex >= 0)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            if (band == soloIndex)
                continue;
            if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, band, "solo")))
                param->setValueNotifyingHost(param->convertTo0to1(0.0f));
        }
    }

    const bool dynEnabled = dynEnableToggle.getToggleState();
    const float dynAlpha = dynEnabled ? 1.0f : 0.35f;
    dynUpButton.setEnabled(dynEnabled);
    dynDownButton.setEnabled(dynEnabled);
    thresholdSlider.setEnabled(dynEnabled);
    attackSlider.setEnabled(dynEnabled);
    releaseSlider.setEnabled(dynEnabled);
    autoScaleToggle.setEnabled(dynEnabled);
    dynExternalToggle.setEnabled(dynEnabled);
    dynUpButton.setAlpha(dynAlpha);
    dynDownButton.setAlpha(dynAlpha);
    thresholdSlider.setAlpha(dynAlpha);
    attackSlider.setAlpha(dynAlpha);
    releaseSlider.setAlpha(dynAlpha);
    autoScaleToggle.setAlpha(dynAlpha);
    dynExternalToggle.setAlpha(dynAlpha);
    syncMsSelectionFromParam();
    for (int i = 0; i < static_cast<int>(bandSelectButtons.size()); ++i)
    {
        bool bypassed = false;
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, i, "bypass")))
            bypassed = param->getValue() > 0.5f;
        bool soloed = false;
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, i, "solo")))
            soloed = param->getValue() > 0.5f;
        const bool hovered = bandSelectButtons[static_cast<size_t>(i)].isMouseOver();
        const bool isSelected = (i == selectedBand);
        auto& hoverFade = bandHoverFade[static_cast<size_t>(i)];
        auto& selectFade = bandSelectFade[static_cast<size_t>(i)];
        auto& activeFade = bandActiveFade[static_cast<size_t>(i)];
        hoverFade.setTargetValue(hovered ? 1.0f : 0.0f);
        selectFade.setTargetValue(isSelected ? 1.0f : 0.0f);
        activeFade.setTargetValue(bypassed ? 0.0f : 1.0f);
        hoverFade.skip(1);
        selectFade.skip(1);
        activeFade.skip(1);
        const float hover = hoverFade.getCurrentValue();
        const float selected = selectFade.getCurrentValue();
        const float active = activeFade.getCurrentValue();
        auto baseColour = ColorUtils::bandColour(i);
        if (bypassed)
            baseColour = baseColour.withSaturation(0.05f).withBrightness(0.35f);
        baseColour = baseColour.interpolatedWith(baseColour.darker(0.7f), 1.0f - active);
        auto& button = bandSelectButtons[static_cast<size_t>(i)];
        const float baseAlpha = (bypassed ? 0.14f : 0.2f) + hover * 0.08f;
        const float onAlpha = (bypassed ? 0.32f : 0.55f) + selected * 0.2f;
        button.setColour(juce::TextButton::buttonColourId, baseColour.withAlpha(baseAlpha));
        button.setColour(juce::TextButton::buttonOnColourId, baseColour.withAlpha(onAlpha));
        
        // v4.2: Improved band number visibility for better contrast.
        // Make band numbers more visible: use brighter/lighter text color for better contrast.
        // - Selected bands: white text for maximum visibility against colored background.
        // - Non-selected bands: brighter band color (brighter 0.4) with high alpha (0.95) for clear visibility.
        // - Bypassed bands: slightly dimmed but still visible (0.6 alpha) to indicate inactive state.
        if (bypassed)
        {
            button.setColour(juce::TextButton::textColourOffId, baseColour.withAlpha(0.6f));
        }
        else if (isSelected)
        {
            // Selected band: white text for maximum visibility.
            button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        }
        else
        {
            // Non-selected bands: use brighter version of band color for better visibility.
            button.setColour(juce::TextButton::textColourOffId, baseColour.brighter(0.4f).withAlpha(0.95f));
        }
        button.setColour(juce::TextButton::textColourOnId, bypassed ? theme.textMuted : juce::Colours::white);

        auto& soloButton = bandSoloButtons[static_cast<size_t>(i)];
        soloButton.setToggleState(soloed, juce::dontSendNotification);
        soloButton.setColour(juce::ToggleButton::textColourId, soloed ? juce::Colours::white : theme.textMuted);
        soloButton.setColour(juce::ToggleButton::tickColourId, baseColour);
        soloButton.setColour(juce::ToggleButton::tickDisabledColourId, baseColour.withAlpha(0.4f));
    }
    if (selectedBand >= 0 && selectedBand < ParamIDs::kBandsPerChannel)
        selectedBandGlow = bandSelectFade[static_cast<size_t>(selectedBand)].getCurrentValue();
    repaint(detectorMeterBounds.getSmallestIntegerContainer());
}

void BandControlsPanel::cacheBandFromUi(int channelIndex, int bandIndex)
{
    if (channelIndex < 0 || channelIndex >= ParamIDs::kMaxChannels
        || bandIndex < 0 || bandIndex >= ParamIDs::kBandsPerChannel)
        return;

    auto& state = bandStateCache[static_cast<size_t>(channelIndex)][static_cast<size_t>(bandIndex)];
    
    // v4.4 beta: Cache parameters based on current layer
    if (currentLayer == BandControlsPanel::LayerType::EQ)
    {
        state.freq = static_cast<float>(freqSlider.getValue());
        state.gain = static_cast<float>(gainSlider.getValue());
        state.q = static_cast<float>(qSlider.getValue());
        state.type = static_cast<float>(typeBox.getSelectedItemIndex());
        if (! msChoiceMap.empty())
        {
            const int msIndex = msBox.getSelectedItemIndex();
            if (msIndex >= 0 && msIndex < static_cast<int>(msChoiceMap.size()))
                state.ms = static_cast<float>(msChoiceMap[static_cast<size_t>(msIndex)]);
        }
        const int slopeIndex = slopeBox.getSelectedItemIndex();
        if (slopeIndex >= 0)
            state.slope = static_cast<float>((slopeIndex + 1) * 6);
        state.mix = static_cast<float>(mixSlider.getValue());
    }
    else  // Harmonic layer
    {
        state.odd = static_cast<float>(oddHarmonicSlider.getValue());
        state.mixOdd = static_cast<float>(mixOddSlider.getValue());
        state.even = static_cast<float>(evenHarmonicSlider.getValue());
        state.mixEven = static_cast<float>(mixEvenSlider.getValue());
        // v4.4 beta: Harmonic bypass (per-band, independent for each of 12 bands)
        state.harmonicBypass = harmonicBypassToggle.getToggleState() ? 1.0f : 0.0f;
    }
    
    if (auto* param = parameters.getRawParameterValue(ParamIDs::bandParamId(channelIndex, bandIndex, "bypass")))
        state.bypass = param->load();
    if (auto* param = parameters.getRawParameterValue(ParamIDs::bandParamId(channelIndex, bandIndex, "solo")))
        state.solo = param->load();
    state.dynEnable = dynEnableToggle.getToggleState() ? 1.0f : 0.0f;
    state.dynMode = dynDownButton.getToggleState() ? 1.0f : 0.0f;
    state.dynThresh = static_cast<float>(thresholdSlider.getValue());
    state.dynAttack = static_cast<float>(attackSlider.getValue());
    state.dynRelease = static_cast<float>(releaseSlider.getValue());
    state.dynAuto = autoScaleToggle.getToggleState() ? 1.0f : 0.0f;
    state.dynExternal = dynExternalToggle.getToggleState() ? 1.0f : 0.0f;
    bandStateValid[static_cast<size_t>(channelIndex)][static_cast<size_t>(bandIndex)] = true;
    bandStateDirty[static_cast<size_t>(channelIndex)] = true;
}

void BandControlsPanel::cacheBandFromParams(int channelIndex, int bandIndex)
{
    if (channelIndex < 0 || channelIndex >= ParamIDs::kMaxChannels
        || bandIndex < 0 || bandIndex >= ParamIDs::kBandsPerChannel)
        return;

    auto& state = bandStateCache[static_cast<size_t>(channelIndex)][static_cast<size_t>(bandIndex)];
    auto readValue = [this, channelIndex](int band, const juce::String& suffix, float fallback)
    {
        if (auto* param = parameters.getRawParameterValue(ParamIDs::bandParamId(channelIndex, band, suffix)))
            return param->load();
        return fallback;
    };

    state.freq = readValue(bandIndex, "freq", state.freq);
    state.gain = readValue(bandIndex, "gain", state.gain);
    state.q = readValue(bandIndex, "q", state.q);
    state.type = readValue(bandIndex, "type", state.type);
    state.bypass = readValue(bandIndex, "bypass", state.bypass);
    state.ms = readValue(bandIndex, "ms", state.ms);
    state.slope = readValue(bandIndex, "slope", state.slope);
    state.solo = readValue(bandIndex, "solo", state.solo);
    state.mix = readValue(bandIndex, "mix", state.mix);
    // v4.4 beta: Harmonic parameters (per-band, independent for each of 12 bands)
    state.odd = readValue(bandIndex, "odd", state.odd);
    state.mixOdd = readValue(bandIndex, "mixOdd", state.mixOdd);
    state.even = readValue(bandIndex, "even", state.even);
    state.mixEven = readValue(bandIndex, "mixEven", state.mixEven);
    state.harmonicBypass = readValue(bandIndex, "harmonicBypass", state.harmonicBypass);
    state.dynEnable = readValue(bandIndex, "dynEnable", state.dynEnable);
    state.dynMode = readValue(bandIndex, "dynMode", state.dynMode);
    state.dynThresh = readValue(bandIndex, "dynThresh", state.dynThresh);
    state.dynAttack = readValue(bandIndex, "dynAttack", state.dynAttack);
    state.dynRelease = readValue(bandIndex, "dynRelease", state.dynRelease);
    state.dynAuto = readValue(bandIndex, "dynAuto", state.dynAuto);
    state.dynExternal = readValue(bandIndex, "dynExternal", state.dynExternal);
    bandStateValid[static_cast<size_t>(channelIndex)][static_cast<size_t>(bandIndex)] = true;
}

void BandControlsPanel::refreshCacheFromParams(int channelIndex)
{
    if (channelIndex < 0 || channelIndex >= ParamIDs::kMaxChannels)
        return;

    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        cacheBandFromParams(channelIndex, band);
}

void BandControlsPanel::applyCachedBandToParams(int channelIndex)
{
    if (channelIndex < 0 || channelIndex >= ParamIDs::kMaxChannels)
        return;
    if (! bandStateDirty[static_cast<size_t>(channelIndex)])
        return;
    juce::Logger::writeToLog("Band cache: applying cached band state for channel "
                             + juce::String(channelIndex));

    auto setParamValue = [this, channelIndex](int band, const juce::String& suffix, float value)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*>(
                parameters.getParameter(ParamIDs::bandParamId(channelIndex, band, suffix))))
        {
            param->setValueNotifyingHost(param->convertTo0to1(value));
        }
    };

    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
    {
        if (! bandStateValid[static_cast<size_t>(channelIndex)][static_cast<size_t>(band)])
            continue;
        const auto& state = bandStateCache[static_cast<size_t>(channelIndex)][static_cast<size_t>(band)];
        setParamValue(band, "freq", state.freq);
        setParamValue(band, "gain", state.gain);
        setParamValue(band, "q", state.q);
        setParamValue(band, "type", state.type);
        setParamValue(band, "bypass", state.bypass);
        setParamValue(band, "ms", state.ms);
        setParamValue(band, "slope", state.slope);
        setParamValue(band, "solo", state.solo);
        setParamValue(band, "mix", state.mix);
        // v4.4 beta: Harmonic parameters (per-band, independent for each of 12 bands)
        setParamValue(band, "odd", state.odd);
        setParamValue(band, "mixOdd", state.mixOdd);
        setParamValue(band, "even", state.even);
        setParamValue(band, "mixEven", state.mixEven);
        setParamValue(band, "harmonicBypass", state.harmonicBypass);
        setParamValue(band, "dynEnable", state.dynEnable);
        setParamValue(band, "dynMode", state.dynMode);
        setParamValue(band, "dynThresh", state.dynThresh);
        setParamValue(band, "dynAttack", state.dynAttack);
        setParamValue(band, "dynRelease", state.dynRelease);
        setParamValue(band, "dynAuto", state.dynAuto);
        setParamValue(band, "dynExternal", state.dynExternal);
    }
    bandStateDirty[static_cast<size_t>(channelIndex)] = false;
}

void BandControlsPanel::restoreBandFromCache()
{
    if (selectedChannel < 0 || selectedChannel >= ParamIDs::kMaxChannels
        || selectedBand < 0 || selectedBand >= ParamIDs::kBandsPerChannel)
        return;

    if (! bandStateValid[static_cast<size_t>(selectedChannel)][static_cast<size_t>(selectedBand)])
        return;

    const auto& state = bandStateCache[static_cast<size_t>(selectedChannel)][static_cast<size_t>(selectedBand)];
    
    // v4.4 beta: Sync parameters based on current layer
    if (currentLayer == BandControlsPanel::LayerType::EQ)
    {
        freqSlider.setValue(state.freq, juce::dontSendNotification);
        gainSlider.setValue(state.gain, juce::dontSendNotification);
        qSlider.setValue(state.q, juce::dontSendNotification);
        if (static_cast<int>(state.type) >= 0)
            typeBox.setSelectedItemIndex(static_cast<int>(state.type), juce::dontSendNotification);
        const int slopeIndex = juce::jlimit(0, 15, static_cast<int>(std::round(state.slope / 6.0f)) - 1);
        slopeBox.setSelectedItemIndex(slopeIndex, juce::dontSendNotification);
        mixSlider.setValue(state.mix, juce::dontSendNotification);
    }
    else  // Harmonic layer
    {
        oddHarmonicSlider.setValue(state.odd, juce::dontSendNotification);
        mixOddSlider.setValue(state.mixOdd, juce::dontSendNotification);
        evenHarmonicSlider.setValue(state.even, juce::dontSendNotification);
        mixEvenSlider.setValue(state.mixEven, juce::dontSendNotification);
        // v4.4 beta: Harmonic bypass (per-band, independent for each of 12 bands)
        harmonicBypassToggle.setToggleState(state.harmonicBypass > 0.5f, juce::dontSendNotification);
    }
    dynEnableToggle.setToggleState(state.dynEnable > 0.5f, juce::dontSendNotification);
    dynUpButton.setToggleState(state.dynMode < 0.5f, juce::dontSendNotification);
    dynDownButton.setToggleState(state.dynMode > 0.5f, juce::dontSendNotification);
    thresholdSlider.setValue(state.dynThresh, juce::dontSendNotification);
    attackSlider.setValue(state.dynAttack, juce::dontSendNotification);
    releaseSlider.setValue(state.dynRelease, juce::dontSendNotification);
    autoScaleToggle.setToggleState(state.dynAuto > 0.5f, juce::dontSendNotification);
    dynExternalToggle.setToggleState(state.dynExternal > 0.5f, juce::dontSendNotification);
    if (! msChoiceMap.empty())
    {
        auto it = std::find(msChoiceMap.begin(), msChoiceMap.end(), static_cast<int>(state.ms));
        const int uiIndex = (it != msChoiceMap.end())
            ? static_cast<int>(std::distance(msChoiceMap.begin(), it))
            : 0;
        msBox.setSelectedItemIndex(uiIndex, juce::dontSendNotification);
    }
}


void BandControlsPanel::mouseDoubleClick(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
}

void BandControlsPanel::resized()
{
    // v4.4 beta: Start timer only after first resize to ensure components are properly laid out.
    // This prevents expensive repaints before initialization is complete.
    // Critical for ensuring all controls (buttons, knobs, dropdowns) appear immediately on plugin load
    if (!hasBeenResized)
    {
        hasBeenResized = true;
        // Force initial repaint to ensure all components render properly
        repaint();
        // Start timer at normal rate after components are laid out
        startTimerHz(30);
    }
    
    auto bounds = getLocalBounds().reduced(kPanelPadding);
    const int knobSize = juce::jmin(86, kKnobRowHeight - kLabelHeight - 6);

    auto left = bounds.removeFromLeft(static_cast<int>(bounds.getWidth() * 0.62f));
    auto right = bounds;

    auto headerRow = left.removeFromTop(kRowHeight);
    
    // v4.4 beta: Layer toggle buttons on the LEFT, optimized sizes based on text length
    // EQ toggle: smaller (short text "EQ")
    const int eqToggleW = 45;  // Optimized for "EQ" text
    eqLayerToggle.setBounds(headerRow.removeFromLeft(eqToggleW));
    headerRow.removeFromLeft(2);  // Small gap between toggles
    // Harmonic toggle: larger (longer text "HARMONIC")
    const int harmonicToggleW = 95;  // Optimized for "HARMONIC" text
    harmonicLayerToggle.setBounds(headerRow.removeFromLeft(harmonicToggleW));
    
    headerRow.removeFromLeft(4);  // Gap after toggles
    
    // Action buttons in the middle
    const int btnW = 58;
    const int resetW = 86;
    copyButton.setBounds(headerRow.removeFromLeft(btnW));
    pasteButton.setBounds(headerRow.removeFromLeft(btnW));
    defaultButton.setBounds(headerRow.removeFromLeft(resetW));
    resetAllButton.setBounds(headerRow.removeFromLeft(resetW));
    const int navW = 24;
    prevBandButton.setBounds(headerRow.removeFromLeft(navW));
    nextBandButton.setBounds(headerRow.removeFromLeft(navW));
    
    // v4.4 beta: Band number on the RIGHT - only showing current band number (e.g., "1")
    // Reduced width since we only show the number, not "1 / 12"
    titleLabel.setBounds(headerRow.removeFromRight(40));
    
    eqSectionLabel.setBounds({0, 0, 0, 0});

    left.removeFromTop(2);
    auto bandRow = left.removeFromTop(kRowHeight);
    const int bandBtnWidth = std::max(18, (bandRow.getWidth() - kGap * 11) / 12);
    for (int i = 0; i < static_cast<int>(bandSelectButtons.size()); ++i)
    {
        bandSelectButtons[static_cast<size_t>(i)].setBounds(
            bandRow.removeFromLeft(bandBtnWidth));
        bandRow.removeFromLeft(kGap);
    }

    left.removeFromTop(2);
    auto soloRow = left.removeFromTop(kRowHeight);
    const int soloBtnWidth = std::max(18, (soloRow.getWidth() - kGap * 11) / 12);
    for (int i = 0; i < static_cast<int>(bandSoloButtons.size()); ++i)
    {
        bandSoloButtons[static_cast<size_t>(i)].setBounds(
            soloRow.removeFromLeft(soloBtnWidth));
        soloRow.removeFromLeft(kGap);
    }
    left.removeFromTop(kGap);

    const int eqKnobTop = left.getY();
    auto knobsRow = left.removeFromTop(kKnobRowHeight);
    // Extra gap below rotary frame so Freq/Gain/Q/Band Mix labels sit a little lower and do not touch the frame.
    const int kLabelTopGap = 6;
    knobsRow.removeFromTop(kLabelTopGap);
    const int knobWidth = (knobsRow.getWidth() - kGap * 3) / 4;
    auto squareKnob = [](juce::Rectangle<int> area)
    {
        const int size = std::min(area.getWidth(), area.getHeight());
        return juce::Rectangle<int>(size, size).withCentre(area.getCentre());
    };
    // v4.4 beta: Position controls based on current layer (EQ or Harmonic)
    if (currentLayer == BandControlsPanel::LayerType::EQ)
    {
        auto freqArea = knobsRow.removeFromLeft(knobWidth);
        freqLabel.setBounds(freqArea.removeFromTop(kLabelHeight));
        freqSlider.setBounds(squareKnob(freqArea).withSizeKeepingCentre(knobSize, knobSize));
        knobsRow.removeFromLeft(kGap);
        auto gainArea = knobsRow.removeFromLeft(knobWidth);
        gainLabel.setBounds(gainArea.removeFromTop(kLabelHeight));
        gainSlider.setBounds(squareKnob(gainArea).withSizeKeepingCentre(knobSize, knobSize));
        knobsRow.removeFromLeft(kGap);
        auto qArea = knobsRow.removeFromLeft(knobWidth);
        qLabel.setBounds(qArea.removeFromTop(kLabelHeight));
        qSlider.setBounds(squareKnob(qArea).withSizeKeepingCentre(knobSize, knobSize));
        knobsRow.removeFromLeft(kGap);
        auto mixArea = knobsRow.removeFromLeft(knobWidth);
        mixLabel.setBounds(mixArea.removeFromTop(kLabelHeight));
        mixSlider.setBounds(squareKnob(mixArea).withSizeKeepingCentre(knobSize, knobSize));
    }
    else  // Harmonic layer
    {
        auto freqArea = knobsRow.removeFromLeft(knobWidth);
        oddLabel.setBounds(freqArea.removeFromTop(kLabelHeight));
        oddHarmonicSlider.setBounds(squareKnob(freqArea).withSizeKeepingCentre(knobSize, knobSize));
        knobsRow.removeFromLeft(kGap);
        auto mixOddArea = knobsRow.removeFromLeft(knobWidth);
        mixOddLabel.setBounds(mixOddArea.removeFromTop(kLabelHeight));
        mixOddSlider.setBounds(squareKnob(mixOddArea).withSizeKeepingCentre(knobSize, knobSize));
        knobsRow.removeFromLeft(kGap);
        auto evenArea = knobsRow.removeFromLeft(knobWidth);
        evenLabel.setBounds(evenArea.removeFromTop(kLabelHeight));
        evenHarmonicSlider.setBounds(squareKnob(evenArea).withSizeKeepingCentre(knobSize, knobSize));
        knobsRow.removeFromLeft(kGap);
        auto mixEvenArea = knobsRow.removeFromLeft(knobWidth);
        mixEvenLabel.setBounds(mixEvenArea.removeFromTop(kLabelHeight));
        mixEvenSlider.setBounds(squareKnob(mixEvenArea).withSizeKeepingCentre(knobSize, knobSize));
    }

    // v4.4 beta: Layout for dropdowns (EQ layer) or bypass toggle (Harmonic layer)
    left.removeFromTop(kGap + 4);  // Extra 4 pixels to prevent overlap with frame
    if (currentLayer == BandControlsPanel::LayerType::EQ)
    {
        auto comboRow = left.removeFromTop(kLabelHeight + kRowHeight);
        const int comboColWidth = (comboRow.getWidth() - kGap * 2) / 3;

        auto channelCol = comboRow.removeFromLeft(comboColWidth);
        msLabel.setBounds(channelCol.removeFromTop(kLabelHeight));
        msBox.setBounds(channelCol.withHeight(kComboHeight).withSizeKeepingCentre(comboWidthMs, kComboHeight));
        comboRow.removeFromLeft(kGap);
        auto typeCol = comboRow.removeFromLeft(comboColWidth);
        typeLabel.setBounds(typeCol.removeFromTop(kLabelHeight));
        const int typeWidth = juce::jmin(typeCol.getWidth(), comboWidthType);
        typeBox.setBounds(typeCol.withHeight(kComboHeight).withSizeKeepingCentre(typeWidth, kComboHeight));
        comboRow.removeFromLeft(kGap);
        auto slopeCol = comboRow.removeFromLeft(comboColWidth);
        slopeLabel.setBounds(slopeCol.removeFromTop(kLabelHeight));
        const int slopeWidth = juce::jmin(slopeCol.getWidth(), comboWidthSlope);
        slopeBox.setBounds(slopeCol.withHeight(kComboHeight).withSizeKeepingCentre(slopeWidth, kComboHeight));
    }
    else  // Harmonic layer: show bypass toggle only (oversampling is global, shown in PluginEditor)
    {
        auto controlsRow = left.removeFromTop(kRowHeight);
        const int bypassToggleW = 80;
        harmonicBypassToggle.setBounds(controlsRow.removeFromLeft(bypassToggleW)
                                          .withSizeKeepingCentre(bypassToggleW, kRowHeight));
    }

    left.removeFromTop(2);
    auto togglesRow = left.removeFromTop(kRowHeight);
    juce::ignoreUnused(togglesRow, eqKnobTop, right);

    dynEnableToggle.setBounds({0, 0, 0, 0});
    dynUpButton.setBounds({0, 0, 0, 0});
    dynDownButton.setBounds({0, 0, 0, 0});
    autoScaleToggle.setBounds({0, 0, 0, 0});
    dynExternalToggle.setBounds({0, 0, 0, 0});
    thresholdLabel.setBounds({0, 0, 0, 0});
    thresholdSlider.setBounds({0, 0, 0, 0});
    attackLabel.setBounds({0, 0, 0, 0});
    attackSlider.setBounds({0, 0, 0, 0});
    releaseLabel.setBounds({0, 0, 0, 0});
    releaseSlider.setBounds({0, 0, 0, 0});
    detectorMeterBounds = {};
}

void BandControlsPanel::updateAttachments()
{
    const auto freqId = ParamIDs::bandParamId(selectedChannel, selectedBand, "freq");
    const auto gainId = ParamIDs::bandParamId(selectedChannel, selectedBand, "gain");
    const auto qId = ParamIDs::bandParamId(selectedChannel, selectedBand, "q");
    const auto msId = ParamIDs::bandParamId(selectedChannel, selectedBand, "ms");
    const auto slopeId = ParamIDs::bandParamId(selectedChannel, selectedBand, "slope");
    const auto mixId = ParamIDs::bandParamId(selectedChannel, selectedBand, "mix");

    freqParam = parameters.getParameter(freqId);
    gainParam = parameters.getParameter(gainId);
    qParam = parameters.getParameter(qId);
    mixParam = parameters.getParameter(mixId);
    dynThreshParam = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "dynThresh"));
    dynAttackParam = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "dynAttack"));
    dynReleaseParam = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "dynRelease"));

    // v4.4 beta: Create attachments based on current layer
    if (currentLayer == BandControlsPanel::LayerType::EQ)
    {
        freqAttachment = std::make_unique<SliderAttachment>(parameters, freqId, freqSlider);
        gainAttachment = std::make_unique<SliderAttachment>(parameters, gainId, gainSlider);
        qAttachment = std::make_unique<SliderAttachment>(parameters, qId, qSlider);
        mixAttachment = std::make_unique<SliderAttachment>(parameters, mixId, mixSlider);
        // Clear harmonic attachments when on EQ layer
        oddHarmonicAttachment.reset();
        mixOddAttachment.reset();
        evenHarmonicAttachment.reset();
        mixEvenAttachment.reset();
        harmonicBypassAttachment.reset();
    }
    else  // Harmonic layer
    {
        const auto oddId = ParamIDs::bandParamId(selectedChannel, selectedBand, "odd");
        const auto mixOddId = ParamIDs::bandParamId(selectedChannel, selectedBand, "mixOdd");
        const auto evenId = ParamIDs::bandParamId(selectedChannel, selectedBand, "even");
        const auto mixEvenId = ParamIDs::bandParamId(selectedChannel, selectedBand, "mixEven");
        
        oddHarmonicAttachment = std::make_unique<SliderAttachment>(parameters, oddId, oddHarmonicSlider);
        mixOddAttachment = std::make_unique<SliderAttachment>(parameters, mixOddId, mixOddSlider);
        evenHarmonicAttachment = std::make_unique<SliderAttachment>(parameters, evenId, evenHarmonicSlider);
        mixEvenAttachment = std::make_unique<SliderAttachment>(parameters, mixEvenId, mixEvenSlider);
        
        // v4.4 beta: Harmonic bypass attachment (per-band, independent for each of 12 bands)
        const auto harmonicBypassId = ParamIDs::bandParamId(selectedChannel, selectedBand, "harmonicBypass");
        harmonicBypassAttachment = std::make_unique<ButtonAttachment>(parameters, harmonicBypassId, harmonicBypassToggle);
        
        // Clear EQ attachments when on Harmonic layer
        freqAttachment.reset();
        gainAttachment.reset();
        qAttachment.reset();
        mixAttachment.reset();
    }
    dynEnableAttachment = std::make_unique<ButtonAttachment>(
        parameters, ParamIDs::bandParamId(selectedChannel, selectedBand, "dynEnable"), dynEnableToggle);
    dynThresholdAttachment = std::make_unique<SliderAttachment>(
        parameters, ParamIDs::bandParamId(selectedChannel, selectedBand, "dynThresh"), thresholdSlider);
    dynAttackAttachment = std::make_unique<SliderAttachment>(
        parameters, ParamIDs::bandParamId(selectedChannel, selectedBand, "dynAttack"), attackSlider);
    dynReleaseAttachment = std::make_unique<SliderAttachment>(
        parameters, ParamIDs::bandParamId(selectedChannel, selectedBand, "dynRelease"), releaseSlider);
    dynAutoAttachment = std::make_unique<ButtonAttachment>(
        parameters, ParamIDs::bandParamId(selectedChannel, selectedBand, "dynAuto"), autoScaleToggle);
    dynExternalAttachment = std::make_unique<ButtonAttachment>(
        parameters, ParamIDs::bandParamId(selectedChannel, selectedBand, "dynExternal"), dynExternalToggle);

    if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "dynMode")))
    {
        const int mode = static_cast<int>(param->convertFrom0to1(param->getValue()));
        dynUpButton.setToggleState(mode == 0, juce::dontSendNotification);
        dynDownButton.setToggleState(mode == 1, juce::dontSendNotification);
    }

    if (auto* slopeParam = parameters.getParameter(slopeId))
    {
        const float slopeValue = slopeParam->convertFrom0to1(slopeParam->getValue());
        const int slopeIndex = juce::jlimit(0, 15,
                                            static_cast<int>(std::round(slopeValue / 6.0f)) - 1);
        slopeBox.setSelectedItemIndex(slopeIndex, juce::dontSendNotification);
    }

    if (auto* typeParam = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "type")))
    {
        const int typeIndex = static_cast<int>(typeParam->convertFrom0to1(typeParam->getValue()));
        typeBox.setSelectedItemIndex(typeIndex, juce::dontSendNotification);
    }
    updateComboBoxWidths();

}

void BandControlsPanel::syncUiFromParams()
{
    auto setSliderFromParam = [this](juce::Slider& slider, const juce::String& suffix)
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, suffix)))
            slider.setValue(param->convertFrom0to1(param->getValue()), juce::dontSendNotification);
    };
    auto setToggleFromParam = [this](juce::ToggleButton& button, const juce::String& suffix)
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, suffix)))
            button.setToggleState(param->getValue() > 0.5f, juce::dontSendNotification);
    };

    // v4.4 beta: Sync parameters based on current layer
    if (currentLayer == BandControlsPanel::LayerType::EQ)
    {
        setSliderFromParam(freqSlider, "freq");
        setSliderFromParam(gainSlider, "gain");
        setSliderFromParam(qSlider, "q");
        setSliderFromParam(mixSlider, "mix");
        
        if (auto* typeParam = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "type")))
        {
            const int typeIndex = static_cast<int>(typeParam->convertFrom0to1(typeParam->getValue()));
            typeBox.setSelectedItemIndex(typeIndex, juce::dontSendNotification);
        }

        if (auto* slopeParam = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "slope")))
        {
            const float slopeValue = slopeParam->convertFrom0to1(slopeParam->getValue());
            const int slopeIndex = juce::jlimit(0, 15,
                                                static_cast<int>(std::round(slopeValue / 6.0f)) - 1);
            slopeBox.setSelectedItemIndex(slopeIndex, juce::dontSendNotification);
        }
    }
    else  // Harmonic layer
    {
        setSliderFromParam(oddHarmonicSlider, "odd");
        setSliderFromParam(mixOddSlider, "mixOdd");
        setSliderFromParam(evenHarmonicSlider, "even");
        setSliderFromParam(mixEvenSlider, "mixEven");
        
        // v4.5 beta: Harmonic bypass toggle (per-band, independent for each of 12 bands)
        setToggleFromParam(harmonicBypassToggle, "harmonicBypass");
        // Note: Oversampling is global and controlled in PluginEditor, not here
    }
    
    setSliderFromParam(thresholdSlider, "dynThresh");
    setSliderFromParam(attackSlider, "dynAttack");
    setSliderFromParam(releaseSlider, "dynRelease");

    setToggleFromParam(dynEnableToggle, "dynEnable");
    setToggleFromParam(autoScaleToggle, "dynAuto");
    setToggleFromParam(dynExternalToggle, "dynExternal");

    if (auto* dynModeParam = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "dynMode")))
    {
        const int mode = static_cast<int>(dynModeParam->convertFrom0to1(dynModeParam->getValue()));
        dynUpButton.setToggleState(mode == 0, juce::dontSendNotification);
        dynDownButton.setToggleState(mode == 1, juce::dontSendNotification);
    }

    syncMsSelectionFromParam();
}

void BandControlsPanel::resetSelectedBand()
{
    const int channel = selectedChannel;
    const int band = selectedBand;
    auto resetParam = [this, channel, band](const juce::String& suffix)
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(channel, band, suffix)))
            param->setValueNotifyingHost(param->getDefaultValue());
    };

    // Prevent UI edits from auto-unbypassing during reset.
    resetInProgress = true;
    suppressParamCallbacks = true;
    resetParam("freq");
    resetParam("gain");
    resetParam("q");
    resetParam("type");
    resetParam("ms");
    resetParam("slope");
    resetParam("solo");
    resetParam("mix");
    resetParam("dynEnable");
    resetParam("dynMode");
    resetParam("dynThresh");
    resetParam("dynAttack");
    resetParam("dynRelease");
    resetParam("dynAuto");
    resetParam("dynExternal");
    resetParam("odd");
    resetParam("mixOdd");
    resetParam("even");
    resetParam("mixEven");
    resetParam("harmonicBypass");
    resetParam("bypass");
    resetParam("solo");
    if (auto* bypassParam = parameters.getParameter(ParamIDs::bandParamId(channel, band, "bypass")))
        bypassParam->setValueNotifyingHost(1.0f);
    if (auto* soloParam = parameters.getParameter(ParamIDs::bandParamId(channel, band, "solo")))
        soloParam->setValueNotifyingHost(0.0f);

    suppressParamCallbacks = false;
    resetInProgress = false;
    cacheBandFromParams(channel, band);
    syncUiFromParams();
}

void BandControlsPanel::resetAllBands()
{
    const int channel = selectedChannel;
    auto resetParam = [this, channel](int band, const juce::String& suffix)
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(channel, band, suffix)))
            param->setValueNotifyingHost(param->getDefaultValue());
    };

    // Prevent UI edits from auto-unbypassing during reset-all.
    resetInProgress = true;
    suppressParamCallbacks = true;
    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
    {
        resetParam(band, "freq");
        resetParam(band, "gain");
        resetParam(band, "q");
        resetParam(band, "type");
        resetParam(band, "ms");
        resetParam(band, "slope");
        resetParam(band, "solo");
        resetParam(band, "mix");
        resetParam(band, "dynEnable");
        resetParam(band, "dynMode");
        resetParam(band, "dynThresh");
        resetParam(band, "dynAttack");
        resetParam(band, "dynRelease");
        resetParam(band, "dynAuto");
        resetParam(band, "dynExternal");
        resetParam(band, "odd");
        resetParam(band, "mixOdd");
        resetParam(band, "even");
        resetParam(band, "mixEven");
        resetParam(band, "harmonicBypass");
        resetParam(band, "bypass");
        resetParam(band, "solo");
        if (auto* bypassParam = parameters.getParameter(ParamIDs::bandParamId(channel, band, "bypass")))
            bypassParam->setValueNotifyingHost(1.0f);
        if (auto* soloParam = parameters.getParameter(ParamIDs::bandParamId(channel, band, "solo")))
            soloParam->setValueNotifyingHost(0.0f);

        cacheBandFromParams(channel, band);
    }
    suppressParamCallbacks = false;
    resetInProgress = false;
    syncUiFromParams();
}

void BandControlsPanel::updateComboBoxWidths()
{
    auto computeWidth = [](const juce::StringArray& labels, const juce::Font& font)
    {
        int maxWidth = 0;
        for (int i = 0; i < labels.size(); ++i)
            maxWidth = std::max(maxWidth, static_cast<int>(std::ceil(font.getStringWidthFloat(labels[i]))));
        return maxWidth;
    };

    const juce::Font font = compactComboLookAndFeel.getComboBoxFont(msBox);
    const int padding = 28; // arrow + margins
    comboWidthType = computeWidth(kFilterTypeChoices, font) + padding;

    juce::StringArray msLabels;
    for (int i = 0; i < msBox.getNumItems(); ++i)
        msLabels.add(msBox.getItemText(i + 1));
    int currentMsWidth = computeWidth(msLabels, font);
    
    // v4.2: Adapt channel selection dropdown width for immersive formats.
    // Also check against longest possible immersive format channel names to ensure dropdown is wide enough.
    // Longest possible channel names from immersive formats (9.1.6, 7.1.4, etc.):
    // Note: MS dropdown can have labels like "STEREO TOP FRONT", "STEREO TOP REAR", "STEREO TOP MIDDLE"
    const juce::StringArray immersiveTestNames = {
        "TML",                  // Top Middle Left (longest 3-char name)
        "TMR",                  // Top Middle Right
        "TFL",                  // Top Front Left
        "TFR",                  // Top Front Right
        "TRL",                  // Top Rear Left
        "TRR",                  // Top Rear Right
        "Bfl",                  // Bottom Front Left
        "Bfr",                  // Bottom Front Right
        "LFE2",                 // Longest single name (4 chars)
        "Lrs",                  // Left Rear Surround
        "Rrs",                  // Right Rear Surround
        "Lw",                   // Wide Left
        "Rw",                   // Wide Right
        "Stereo Front",         // From kMsChoices
        "STEREO TOP FRONT",     // Longest MS label from immersive formats
        "STEREO TOP REAR",      // Long MS label
        "STEREO TOP MIDDLE",    // Longest MS label (17 chars)
        "All"                   // From kMsChoices
    };
    int immersiveWidth = computeWidth(immersiveTestNames, font);
    
    // Use the maximum of current items and longest possible immersive names.
    comboWidthMs = juce::jmax(currentMsWidth, immersiveWidth) + padding;

    juce::StringArray slopeLabels;
    for (int i = 0; i < slopeBox.getNumItems(); ++i)
        slopeLabels.add(slopeBox.getItemText(i + 1));
    // v4.4 beta: Use slope dropdown's larger font for width calculation
    const juce::Font slopeFont = slopeComboLookAndFeel.getComboBoxFont(slopeBox);
    comboWidthSlope = computeWidth(slopeLabels, slopeFont) + padding;
}

void BandControlsPanel::updateTypeUi()
{
    const int typeIndex = getCurrentTypeIndex();
    const bool isAllPass = typeIndex == 7;
    const bool isHpLp = (typeIndex == 3 || typeIndex == 4);
    gainSlider.setEnabled(! isAllPass);
    gainSlider.setAlpha(isAllPass ? 0.5f : 1.0f);
    msBox.setEnabled(msEnabled);
    msBox.setAlpha(msEnabled ? 1.0f : 0.5f);
    slopeBox.setEnabled(isHpLp);
    slopeBox.setAlpha(isHpLp ? 1.0f : 0.5f);
    const bool isTilt = (typeIndex == 8 || typeIndex == 9);
    juce::ignoreUnused(isTilt);
    dynEnableToggle.setVisible(false);
    dynUpButton.setVisible(false);
    dynDownButton.setVisible(false);
    thresholdLabel.setVisible(false);
    attackLabel.setVisible(false);
    releaseLabel.setVisible(false);
    thresholdSlider.setVisible(false);
    attackSlider.setVisible(false);
    releaseSlider.setVisible(false);
    autoScaleToggle.setVisible(false);
    dynExternalToggle.setVisible(false);
}

int BandControlsPanel::getMsParamValue() const
{
    const auto msId = ParamIDs::bandParamId(selectedChannel, selectedBand, "ms");
    if (auto* param = parameters.getParameter(msId))
        return static_cast<int>(param->convertFrom0to1(param->getValue()));
    return 0;
}

void BandControlsPanel::updateMsChoices()
{
    msChoiceMap.clear();
    juce::StringArray labels;

    auto addChoice = [&](int index, const juce::String& labelOverride = {})
    {
        msChoiceMap.push_back(index);
        labels.add(labelOverride.isNotEmpty() ? labelOverride : kMsChoices[index]);
    };

    // The available routing choices depend on the exact channel order of the current layout.
    auto matchesOrder = [&](std::initializer_list<juce::String> order)
    {
        if (channelNames.size() != order.size())
            return false;
        int index = 0;
        for (const auto& name : order)
        {
            if (channelNames[static_cast<size_t>(index++)] != name)
                return false;
        }
        return true;
    };

    enum class ChannelFormat
    {
        Mono,
        Stereo,
        TwoOne,
        ThreeZero,
        ThreeOne,
        FourZero,
        FourOne,
        FiveZeroFilm,
        FiveZeroMusic,
        FiveOneFilm,
        FiveOneMusic,
        SixZeroFilm,
        SixOneFilm,
        SevenZeroFilm,
        SevenOneFilm,
        SevenOneMusic,
        SevenOneTwo,
        SevenOneFour,
        NineOneSix,
        Unknown
    };

    ChannelFormat format = ChannelFormat::Unknown;
    if (matchesOrder({ "M" }) || matchesOrder({ "L" }) || matchesOrder({ "R" }))
        format = ChannelFormat::Mono;
    else if (matchesOrder({ "L", "R" }))
        format = ChannelFormat::Stereo;
    else if (matchesOrder({ "L", "R", "LFE" }))
        format = ChannelFormat::TwoOne;
    else if (matchesOrder({ "L", "R", "C" }))
        format = ChannelFormat::ThreeZero;
    else if (matchesOrder({ "L", "R", "C", "LFE" }))
        format = ChannelFormat::ThreeOne;
    else if (matchesOrder({ "L", "R", "Ls", "Rs" }))
        format = ChannelFormat::FourZero;
    else if (matchesOrder({ "L", "R", "LFE", "Ls", "Rs" }))
        format = ChannelFormat::FourOne;
    else if (matchesOrder({ "L", "R", "C", "Ls", "Rs" }))
        format = ChannelFormat::FiveZeroFilm;
    else if (matchesOrder({ "L", "R", "Ls", "Rs", "C" }))
        format = ChannelFormat::FiveZeroMusic;
    else if (matchesOrder({ "L", "R", "C", "LFE", "Ls", "Rs" }))
        format = ChannelFormat::FiveOneFilm;
    else if (matchesOrder({ "L", "R", "Ls", "Rs", "C", "LFE" }))
        format = ChannelFormat::FiveOneMusic;
    else if (matchesOrder({ "L", "R", "C", "Ls", "Rs", "Cs" }))
        format = ChannelFormat::SixZeroFilm;
    else if (matchesOrder({ "L", "R", "C", "LFE", "Ls", "Rs", "Cs" }))
        format = ChannelFormat::SixOneFilm;
    else if (matchesOrder({ "L", "R", "C", "Ls", "Rs", "Lrs", "Rrs" }))
        format = ChannelFormat::SevenZeroFilm;
    else if (matchesOrder({ "L", "R", "C", "LFE", "Ls", "Rs", "Lrs", "Rrs" }))
        format = ChannelFormat::SevenOneFilm;
    else if (matchesOrder({ "L", "R", "Ls", "Rs", "C", "LFE", "Lrs", "Rrs" }))
        format = ChannelFormat::SevenOneMusic;
    else if (matchesOrder({ "L", "R", "C", "LFE", "Ls", "Rs", "Lrs", "Rrs", "TFL", "TFR" }))
        format = ChannelFormat::SevenOneTwo;
    else if (matchesOrder({ "L", "R", "C", "LFE", "Ls", "Rs", "Lrs", "Rrs", "TFL", "TFR",
                            "TRL", "TRR" }))
        format = ChannelFormat::SevenOneFour;
    else if (matchesOrder({ "L", "R", "C", "LFE", "Ls", "Rs", "Lrs", "Rrs", "Lw", "Rw",
                            "TFL", "TFR", "TML", "TMR", "TRL", "TRR" }))
        format = ChannelFormat::NineOneSix;

    switch (format)
    {
        case ChannelFormat::Mono:
            // Mono exposes only a single "M" target.
            addChoice(kMsAll, "M");
            break;
        case ChannelFormat::Stereo:
            // Stereo exposes M/S pair and individual channels.
            addChoice(kMsAll, "ALL (STEREO)");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsMidFront, "MID");
            addChoice(kMsSideFront, "SIDE");
            break;
        case ChannelFormat::TwoOne:
            addChoice(kMsAll, "ALL (2.1)");
            addChoice(kMsStereoFront, "STEREO");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsLfe, "LFE");
            addChoice(kMsMidFront, "MID");
            addChoice(kMsSideFront, "SIDE");
            break;
        case ChannelFormat::ThreeZero:
            addChoice(kMsAll, "ALL (3.0)");
            addChoice(kMsStereoFront, "STEREO");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsCentre, "C");
            addChoice(kMsMidFront, "MID");
            addChoice(kMsSideFront, "SIDE");
            break;
        case ChannelFormat::ThreeOne:
            addChoice(kMsAll, "ALL (3.1)");
            addChoice(kMsStereoFront, "STEREO");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsCentre, "C");
            addChoice(kMsLfe, "LFE");
            addChoice(kMsMidFront, "MID");
            addChoice(kMsSideFront, "SIDE");
            break;
        case ChannelFormat::FourZero:
            addChoice(kMsAll, "ALL (4.0)");
            addChoice(kMsStereoFront, "STEREO FRONT");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsStereoRear, "STEREO REAR");
            addChoice(kMsLs, "LS");
            addChoice(kMsRs, "RS");
            addChoice(kMsMidFront, "MID FRONT");
            addChoice(kMsMidRear, "MID REAR");
            addChoice(kMsSideFront, "SIDE FRONT");
            addChoice(kMsSideRear, "SIDE REAR");
            break;
        case ChannelFormat::FourOne:
            addChoice(kMsAll, "ALL (4.1)");
            addChoice(kMsStereoFront, "STEREO FRONT");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsLfe, "LFE");
            addChoice(kMsStereoRear, "STEREO REAR");
            addChoice(kMsLs, "LS");
            addChoice(kMsRs, "RS");
            addChoice(kMsMidFront, "MID FRONT");
            addChoice(kMsMidRear, "MID REAR");
            addChoice(kMsSideFront, "SIDE FRONT");
            addChoice(kMsSideRear, "SIDE REAR");
            break;
        case ChannelFormat::FiveZeroFilm:
            addChoice(kMsAll, "ALL (5.0)");
            addChoice(kMsStereoFront, "STEREO FRONT");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsCentre, "C");
            addChoice(kMsStereoRear, "STEREO REAR");
            addChoice(kMsLs, "LS");
            addChoice(kMsRs, "RS");
            addChoice(kMsMidFront, "MID FRONT");
            addChoice(kMsMidRear, "MID REAR");
            break;
        case ChannelFormat::FiveZeroMusic:
            addChoice(kMsAll, "ALL (5.0)");
            addChoice(kMsStereoFront, "STEREO FRONT");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsStereoRear, "STEREO REAR");
            addChoice(kMsLs, "LS");
            addChoice(kMsRs, "RS");
            addChoice(kMsCentre, "C");
            addChoice(kMsMidFront, "MID FRONT");
            addChoice(kMsMidRear, "MID REAR");
            addChoice(kMsSideFront, "SIDE FRONT");
            addChoice(kMsSideRear, "SIDE REAR");
            break;
        case ChannelFormat::FiveOneFilm:
            addChoice(kMsAll, "ALL (5.1)");
            addChoice(kMsStereoFront, "STEREO FRONT");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsCentre, "C");
            addChoice(kMsLfe, "LFE");
            addChoice(kMsStereoRear, "STEREO REAR");
            addChoice(kMsLs, "LS");
            addChoice(kMsRs, "RS");
            addChoice(kMsMidFront, "MID FRONT");
            addChoice(kMsMidRear, "MID REAR");
            addChoice(kMsSideFront, "SIDE FRONT");
            addChoice(kMsSideRear, "SIDE REAR");
            break;
        case ChannelFormat::FiveOneMusic:
            addChoice(kMsAll, "ALL (5.1)");
            addChoice(kMsStereoFront, "STEREO FRONT");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsStereoRear, "STEREO REAR");
            addChoice(kMsLs, "LS");
            addChoice(kMsRs, "RS");
            addChoice(kMsCentre, "C");
            addChoice(kMsLfe, "LFE");
            addChoice(kMsMidFront, "MID FRONT");
            addChoice(kMsMidRear, "MID REAR");
            addChoice(kMsSideFront, "SIDE FRONT");
            addChoice(kMsSideRear, "SIDE REAR");
            break;
        case ChannelFormat::SixZeroFilm:
            addChoice(kMsAll, "ALL (6.0)");
            addChoice(kMsStereoFront, "STEREO FRONT");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsCentre, "C");
            addChoice(kMsStereoRear, "STEREO REAR");
            addChoice(kMsLs, "LS");
            addChoice(kMsRs, "RS");
            addChoice(kMsCs, "CS");
            addChoice(kMsMidFront, "MID FRONT");
            addChoice(kMsMidRear, "MID REAR");
            addChoice(kMsSideFront, "SIDE FRONT");
            addChoice(kMsSideRear, "SIDE REAR");
            break;
        case ChannelFormat::SixOneFilm:
            addChoice(kMsAll, "ALL (6.1)");
            addChoice(kMsStereoFront, "STEREO FRONT");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsCentre, "C");
            addChoice(kMsLfe, "LFE");
            addChoice(kMsStereoRear, "STEREO REAR");
            addChoice(kMsLs, "LS");
            addChoice(kMsRs, "RS");
            addChoice(kMsCs, "CS");
            addChoice(kMsMidFront, "MID FRONT");
            addChoice(kMsMidRear, "MID REAR");
            addChoice(kMsSideFront, "SIDE FRONT");
            addChoice(kMsSideRear, "SIDE REAR");
            break;
        case ChannelFormat::SevenZeroFilm:
            addChoice(kMsAll, "ALL (7.0)");
            addChoice(kMsStereoFront, "STEREO FRONT");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsCentre, "C");
            addChoice(kMsStereoRear, "STEREO REAR");
            addChoice(kMsLs, "LS");
            addChoice(kMsRs, "RS");
            addChoice(kMsStereoLateral, "STEREO LATERAL");
            addChoice(kMsLrs, "LRS");
            addChoice(kMsRrs, "RRS");
            addChoice(kMsMidFront, "MID FRONT");
            addChoice(kMsMidRear, "MID REAR");
            addChoice(kMsMidLateral, "MID LATERAL");
            addChoice(kMsSideFront, "SIDE FRONT");
            addChoice(kMsSideRear, "SIDE REAR");
            addChoice(kMsSideLateral, "SIDE LATERAL");
            break;
        case ChannelFormat::SevenOneFilm:
            addChoice(kMsAll, "ALL (7.1)");
            addChoice(kMsStereoFront, "STEREO FRONT");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsCentre, "C");
            addChoice(kMsLfe, "LFE");
            addChoice(kMsStereoRear, "STEREO REAR");
            addChoice(kMsLs, "LS");
            addChoice(kMsRs, "RS");
            addChoice(kMsStereoLateral, "STEREO LATERAL");
            addChoice(kMsLrs, "LRS");
            addChoice(kMsRrs, "RRS");
            addChoice(kMsMidFront, "MID FRONT");
            addChoice(kMsMidRear, "MID REAR");
            addChoice(kMsMidLateral, "MID LATERAL");
            break;
        case ChannelFormat::SevenOneMusic:
            addChoice(kMsAll, "ALL (7.1)");
            addChoice(kMsStereoFront, "STEREO FRONT");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsStereoRear, "STEREO REAR");
            addChoice(kMsLs, "LS");
            addChoice(kMsRs, "RS");
            addChoice(kMsCentre, "C");
            addChoice(kMsLfe, "LFE");
            addChoice(kMsStereoLateral, "STEREO LATERAL");
            addChoice(kMsLrs, "LRS");
            addChoice(kMsRrs, "RRS");
            addChoice(kMsMidFront, "MID FRONT");
            addChoice(kMsMidRear, "MID REAR");
            addChoice(kMsMidLateral, "MID LATERAL");
            addChoice(kMsSideFront, "SIDE FRONT");
            addChoice(kMsSideRear, "SIDE REAR");
            addChoice(kMsSideLateral, "SIDE LATERAL");
            break;
        case ChannelFormat::SevenOneTwo:
            addChoice(kMsAll, "ALL (7.1.2)");
            addChoice(kMsStereoFront, "STEREO FRONT");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsCentre, "C");
            addChoice(kMsLfe, "LFE");
            addChoice(kMsStereoRear, "STEREO REAR");
            addChoice(kMsLs, "LS");
            addChoice(kMsRs, "RS");
            addChoice(kMsStereoLateral, "STEREO LATERAL");
            addChoice(kMsLrs, "LRS");
            addChoice(kMsRrs, "RRS");
            addChoice(kMsStereoTopFront, "STEREO TOP FRONT");
            addChoice(kMsTfl, "TOP FRONT LEFT (TFL)");
            addChoice(kMsTfr, "TOP FRONT RIGHT (TFR)");
            addChoice(kMsMidFront, "MID FRONT");
            addChoice(kMsMidRear, "MID REAR");
            addChoice(kMsMidLateral, "MID LATERAL");
            addChoice(kMsMidTopFront, "MID TOP FRONT");
            addChoice(kMsSideFront, "SIDE FRONT");
            addChoice(kMsSideRear, "SIDE REAR");
            addChoice(kMsSideLateral, "SIDE LATERAL");
            addChoice(kMsSideTopFront, "SIDE TOP FRONT");
            break;
        case ChannelFormat::SevenOneFour:
            addChoice(kMsAll, "ALL (7.1.4)");
            addChoice(kMsStereoFront, "STEREO FRONT");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsCentre, "C");
            addChoice(kMsLfe, "LFE");
            addChoice(kMsStereoRear, "STEREO REAR");
            addChoice(kMsLs, "LS");
            addChoice(kMsRs, "RS");
            addChoice(kMsStereoLateral, "STEREO LATERAL");
            addChoice(kMsLrs, "LRS");
            addChoice(kMsRrs, "RRS");
            addChoice(kMsStereoTopFront, "STEREO TOP FRONT");
            addChoice(kMsTfl, "TOP FRONT LEFT (TFL)");
            addChoice(kMsTfr, "TOP FRONT RIGHT (TFR)");
            addChoice(kMsStereoTopRear, "STEREO TOP REAR");
            addChoice(kMsTrl, "TOP REAR LEFT (TRL)");
            addChoice(kMsTrr, "TOP REAR RIGHT (TRR)");
            addChoice(kMsMidFront, "MID FRONT");
            addChoice(kMsMidRear, "MID REAR");
            addChoice(kMsMidLateral, "MID LATERAL");
            addChoice(kMsMidTopFront, "MID TOP FRONT");
            addChoice(kMsSideFront, "SIDE FRONT");
            addChoice(kMsSideRear, "SIDE REAR");
            addChoice(kMsSideLateral, "SIDE LATERAL");
            addChoice(kMsSideTopFront, "SIDE TOP FRONT");
            break;
        case ChannelFormat::NineOneSix:
            // Atmos 9.1.6 exposes front/rear/lateral/wide and top pairs.
            addChoice(kMsAll, "ALL (9.1.6)");
            addChoice(kMsStereoFront, "STEREO FRONT");
            addChoice(kMsLeft, "L");
            addChoice(kMsRight, "R");
            addChoice(kMsCentre, "C");
            addChoice(kMsLfe, "LFE");
            addChoice(kMsStereoRear, "STEREO REAR");
            addChoice(kMsLs, "LS");
            addChoice(kMsRs, "RS");
            addChoice(kMsStereoLateral, "STEREO LATERAL");
            addChoice(kMsLrs, "LRS");
            addChoice(kMsRrs, "RRS");
            addChoice(kMsStereoFrontWide, "STEREO FRONT WIDE");
            addChoice(kMsLw, "FRONT WIDE LEFT (LW)");
            addChoice(kMsRw, "FRONT WIDE RIGHT (RW)");
            addChoice(kMsStereoTopFront, "STEREO TOP FRONT");
            addChoice(kMsTfl, "TOP FRONT LEFT");
            addChoice(kMsTfr, "TOP FRONT RIGHT");
            addChoice(kMsStereoTopMiddle, "STEREO TOP MIDDLE");
            addChoice(kMsTml, "TOP MIDDLE LEFT");
            addChoice(kMsTmr, "TOP MIDDLE RIGHT");
            addChoice(kMsStereoTopRear, "STEREO TOP REAR");
            addChoice(kMsTrl, "TOP REAR LEFT");
            addChoice(kMsTrr, "TOP REAR RIGHT");
            addChoice(kMsMidFront, "MID FRONT");
            addChoice(kMsMidRear, "MID REAR");
            addChoice(kMsMidLateral, "MID LATERAL");
            addChoice(kMsMidFrontWide, "MID FRONT WIDE");
            addChoice(kMsMidTopFront, "MID TOP FRONT");
            addChoice(kMsMidTopRear, "MID TOP REAR");
            addChoice(kMsMidTopMiddle, "MID TOP MIDDLE");
            addChoice(kMsSideFront, "SIDE FRONT");
            addChoice(kMsSideRear, "SIDE REAR");
            addChoice(kMsSideLateral, "SIDE LATERAL");
            addChoice(kMsSideFrontWide, "SIDE FRONT WIDE");
            addChoice(kMsSideTopFront, "SIDE TOP FRONT");
            addChoice(kMsSideTopRear, "SIDE TOP REAR");
            addChoice(kMsSideTopMiddle, "SIDE TOP MIDDLE");
            break;
        case ChannelFormat::Unknown:
        default:
        {
            addChoice(kMsAll);

            auto addIfPresent = [&](int index, const juce::String& name)
            {
                if (containsName(channelNames, name))
                    addChoice(index);
            };

            auto addPairIfPresent = [&](int index, const juce::String& left, const juce::String& right,
                                        const juce::String& label)
            {
                if (containsName(channelNames, left) && containsName(channelNames, right))
                    addChoice(index, label);
            };

            addPairIfPresent(kMsStereoFront, "L", "R", "STEREO");
            addIfPresent(kMsLeft, "L");
            addIfPresent(kMsRight, "R");
            addIfPresent(kMsCentre, "C");
            addIfPresent(kMsLfe, "LFE");
            addIfPresent(kMsLs, "LS");
            addIfPresent(kMsRs, "RS");
            addIfPresent(kMsLrs, "LRS");
            addIfPresent(kMsRrs, "RRS");
            addIfPresent(kMsCs, "CS");
            addIfPresent(kMsLw, "LW");
            addIfPresent(kMsRw, "RW");
            addIfPresent(kMsTfl, "TFL");
            addIfPresent(kMsTfr, "TFR");
            addIfPresent(kMsTrl, "TRL");
            addIfPresent(kMsTrr, "TRR");
            addIfPresent(kMsTml, "TML");
            addIfPresent(kMsTmr, "TMR");
            addPairIfPresent(kMsStereoRear, "Ls", "Rs", "STEREO REAR");
            addPairIfPresent(kMsStereoLateral, "Lrs", "Rrs", "STEREO LATERAL");
            addPairIfPresent(kMsStereoFrontWide, "Lw", "Rw", "STEREO FRONT WIDE");
            addPairIfPresent(kMsStereoTopFront, "TFL", "TFR", "STEREO TOP FRONT");
            addPairIfPresent(kMsStereoTopRear, "TRL", "TRR", "STEREO TOP REAR");
            addPairIfPresent(kMsStereoTopMiddle, "TML", "TMR", "STEREO TOP MIDDLE");
            break;
        }
    }

    msBox.clear(juce::dontSendNotification);
    msBox.addItemList(labels, 1);
}

void BandControlsPanel::syncMsSelectionFromParam()
{
    if (msChoiceMap.empty())
        return;
    const int paramValue = getMsParamValue();
    auto it = std::find(msChoiceMap.begin(), msChoiceMap.end(), paramValue);
    if (it == msChoiceMap.end())
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "ms")))
            param->setValueNotifyingHost(param->convertTo0to1(0.0f));
        msBox.setSelectedItemIndex(0, juce::dontSendNotification);
        return;
    }
    const int uiIndex = static_cast<int>(std::distance(msChoiceMap.begin(), it));
    msBox.setSelectedItemIndex(uiIndex, juce::dontSendNotification);
}

int BandControlsPanel::getCurrentTypeIndex() const
{
    const auto typeId = ParamIDs::bandParamId(selectedChannel, selectedBand, "type");
    if (auto* param = parameters.getParameter(typeId))
        return static_cast<int>(param->convertFrom0to1(param->getValue()));
    return 0;
}

void BandControlsPanel::copyBandState()
{
    BandState state;
    state.freq = static_cast<float>(freqSlider.getValue());
    state.gain = static_cast<float>(gainSlider.getValue());
    state.q = static_cast<float>(qSlider.getValue());
    state.type = static_cast<float>(getCurrentTypeIndex());
    if (auto* bypassParam = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "bypass")))
        state.bypass = (bypassParam->getValue() > 0.5f) ? 1.0f : 0.0f;
    state.ms = static_cast<float>(getMsParamValue());
    const auto slopeId = ParamIDs::bandParamId(selectedChannel, selectedBand, "slope");
    if (auto* param = parameters.getParameter(slopeId))
        state.slope = param->convertFrom0to1(param->getValue());
    if (auto* soloParam = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "solo")))
        state.solo = (soloParam->getValue() > 0.5f) ? 1.0f : 0.0f;
    state.mix = static_cast<float>(mixSlider.getValue());
    state.dynEnable = dynEnableToggle.getToggleState() ? 1.0f : 0.0f;
    state.dynMode = dynUpButton.getToggleState() ? 0.0f : 1.0f;
    state.dynThresh = static_cast<float>(thresholdSlider.getValue());
    state.dynAttack = static_cast<float>(attackSlider.getValue());
    state.dynRelease = static_cast<float>(releaseSlider.getValue());
    state.dynAuto = autoScaleToggle.getToggleState() ? 1.0f : 0.0f;
    state.dynExternal = dynExternalToggle.getToggleState() ? 1.0f : 0.0f;
    clipboard = state;
}

void BandControlsPanel::pasteBandState()
{
    if (! clipboard.has_value())
        return;

    const auto& state = clipboard.value();
    auto setParam = [this](const juce::String& suffix, float value)
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, suffix)))
            param->setValueNotifyingHost(param->convertTo0to1(value));
    };

    setParam("freq", state.freq);
    setParam("gain", state.gain);
    setParam("q", state.q);
    setParam("type", state.type);
    setParam("bypass", state.bypass);
    setParam("ms", state.ms);
    setParam("slope", state.slope);
    setParam("solo", state.solo);
    setParam("mix", state.mix);
    setParam("dynEnable", state.dynEnable);
    setParam("dynMode", state.dynMode);
    setParam("dynThresh", state.dynThresh);
    setParam("dynAttack", state.dynAttack);
    setParam("dynRelease", state.dynRelease);
    setParam("dynAuto", state.dynAuto);
    setParam("dynExternal", state.dynExternal);
}

void BandControlsPanel::mirrorToLinkedChannel(const juce::String& suffix, float value)
{
    if (suffix != "ms")
    {
        juce::ignoreUnused(suffix, value);
        return;
    }

    for (int ch = 0; ch < ParamIDs::kMaxChannels; ++ch)
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(ch, selectedBand, suffix)))
            param->setValueNotifyingHost(param->convertTo0to1(value));
    }
}

bool BandControlsPanel::isBandExisting(int bandIndex) const
{
    const auto bypassId = ParamIDs::bandParamId(selectedChannel, bandIndex, "bypass");
    if (auto* bypassParam = parameters.getParameter(bypassId))
    {
        if (bypassParam->getValue() < 0.5f)
            return true;
    }

    const char* suffixes[] { "freq", "gain", "q", "type", "ms", "slope", "solo", "mix" };
    for (const auto* suffix : suffixes)
    {
        const auto paramId = ParamIDs::bandParamId(selectedChannel, bandIndex, suffix);
        if (auto* param = parameters.getParameter(paramId))
        {
            const float current = param->getValue();
            const float def = param->getDefaultValue();
            if (std::abs(current - def) > 0.0005f)
                return true;
        }
    }
    return false;
}

int BandControlsPanel::findNextExisting(int startIndex, int direction) const
{
    const int total = ParamIDs::kBandsPerChannel;
    if (total <= 1)
        return startIndex;
    const int step = (direction >= 0) ? 1 : -1;
    for (int i = 1; i < total; ++i)
    {
        const int idx = (startIndex + step * i + total) % total;
        if (isBandExisting(idx))
            return idx;
    }
    return startIndex;
}
