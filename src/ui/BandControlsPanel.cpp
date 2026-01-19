#include "BandControlsPanel.h"
#include "../PluginProcessor.h"
#include "../util/ColorUtils.h"

namespace
{
const juce::StringArray kFilterTypeChoices {
    "Bell",
    "Low Shelf",
    "High Shelf",
    "Low Pass",
    "High Pass",
    "Notch",
    "Band Pass",
    "All Pass",
    "Tilt",
    "Flat Tilt"
};

const juce::StringArray kMsChoices {
    "All",
    "Mid",
    "Side",
    "L/R",
    "Left",
    "Right",
    "Mono",
    "L",
    "R",
    "C",
    "LFE",
    "Ls",
    "Rs",
    "Lrs",
    "Rrs",
    "Lc",
    "Rc",
    "Ltf",
    "Rtf",
    "Tfc",
    "Tm",
    "Ltr",
    "Rtr",
    "Trc",
    "Lts",
    "Rts",
    "Lw",
    "Rw",
    "LFE2",
    "Bfl",
    "Bfr",
    "Bfc",
    "Ls/Rs",
    "Lrs/Rrs",
    "Lc/Rc",
    "Ltf/Rtf",
    "Ltr/Rtr",
    "Lts/Rts",
    "Lw/Rw",
    "Bfl/Bfr"
};

bool containsName(const std::vector<juce::String>& names, const juce::String& target)
{
    return std::find(names.begin(), names.end(), target) != names.end();
}

} // namespace

BandControlsPanel::BandControlsPanel(EQProAudioProcessor& processorIn)
    : processor(processorIn),
      parameters(processorIn.getParameters())
{
    startTimerHz(30);
    channelNames = processor.getCurrentChannelNames();

    titleLabel.setText("Band 1 / " + juce::String(ParamIDs::kBandsPerChannel), juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, theme.text);
    addAndMakeVisible(titleLabel);

    eqSectionLabel.setText("EQ Parameters", juce::dontSendNotification);
    eqSectionLabel.setJustificationType(juce::Justification::centredLeft);
    eqSectionLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    eqSectionLabel.setColour(juce::Label::textColourId, theme.accent);
    addAndMakeVisible(eqSectionLabel);

    copyButton.setButtonText("Copy");
    copyButton.onClick = [this] { copyBandState(); };
    addAndMakeVisible(copyButton);

    pasteButton.setButtonText("Paste");
    pasteButton.onClick = [this] { pasteBandState(); };
    addAndMakeVisible(pasteButton);

    defaultButton.setButtonText("Reset");
    defaultButton.onClick = [this] { resetSelectedBand(); };
    addAndMakeVisible(defaultButton);

    for (int i = 0; i < static_cast<int>(bandSelectButtons.size()); ++i)
    {
        auto& button = bandSelectButtons[static_cast<size_t>(i)];
        button.setButtonText(juce::String(i + 1));
        button.setClickingTogglesState(true);
        button.onClick = [this, index = i]()
        {
            if (onBandNavigate)
                onBandNavigate(index);
        };
        addAndMakeVisible(button);
    }

    for (int i = 0; i < static_cast<int>(bandSoloButtons.size()); ++i)
    {
        auto& button = bandSoloButtons[static_cast<size_t>(i)];
        button.setButtonText("S");
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
        addAndMakeVisible(button);
    }

    auto initLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, theme.textMuted);
        label.setFont(11.0f);
        addAndMakeVisible(label);
    };

    initLabel(freqLabel, "Freq");
    initLabel(gainLabel, "Gain");
    initLabel(qLabel, "Q");
    initLabel(typeLabel, "Type");
    initLabel(msLabel, "Channel");
    initLabel(slopeLabel, "Slope");
    initLabel(mixLabel, "Band Mix");
    initLabel(thresholdLabel, "Thresh");
    initLabel(attackLabel, "Attack");
    initLabel(releaseLabel, "Release");

    freqSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    const int knobTextW = 68;
    const int knobTextH = 18;
    freqSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    freqSlider.setTextBoxIsEditable(true);
    freqSlider.setSkewFactorFromMidPoint(1000.0);
    freqSlider.setTextValueSuffix(" Hz");
    freqSlider.onValueChange = [this]
    {
        ensureBandActiveFromEdit();
        mirrorToLinkedChannel("freq", static_cast<float>(freqSlider.getValue()));
    };
    addAndMakeVisible(freqSlider);

    gainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    gainSlider.setTextBoxIsEditable(true);
    gainSlider.setTextValueSuffix(" dB");
    gainSlider.setDoubleClickReturnValue(true, 0.0);
    gainSlider.onValueChange = [this]
    {
        ensureBandActiveFromEdit();
        mirrorToLinkedChannel("gain", static_cast<float>(gainSlider.getValue()));
    };
    addAndMakeVisible(gainSlider);

    qSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    qSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    qSlider.setTextBoxIsEditable(true);
    qSlider.setDoubleClickReturnValue(true, 0.707f);
    qSlider.onValueChange = [this]
    {
        ensureBandActiveFromEdit();
        mirrorToLinkedChannel("q", static_cast<float>(qSlider.getValue()));
    };
    addAndMakeVisible(qSlider);

    typeBox.addItemList(kFilterTypeChoices, 1);
    typeBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    typeBox.setColour(juce::ComboBox::textColourId, theme.text);
    typeBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    typeBox.setLookAndFeel(&compactComboLookAndFeel);
    typeBox.onChange = [this]
    {
        ensureBandActiveFromEdit();
        const int index = typeBox.getSelectedItemIndex();
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "type")))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(index)));
        updateTypeUi();
        mirrorToLinkedChannel("type", static_cast<float>(index));
    };
    addAndMakeVisible(typeBox);

    msBox.addItemList(kMsChoices, 1);
    msBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    msBox.setColour(juce::ComboBox::textColourId, theme.text);
    msBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    msBox.setLookAndFeel(&compactComboLookAndFeel);
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
    };

    for (int i = 0; i < 16; ++i)
    {
        const int slopeValue = 6 * (i + 1);
        slopeBox.addItem(juce::String(slopeValue) + " dB", i + 1);
    }
    slopeBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    slopeBox.setColour(juce::ComboBox::textColourId, theme.text);
    slopeBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    slopeBox.setLookAndFeel(&compactComboLookAndFeel);
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
    };
    addAndMakeVisible(slopeBox);

    mixSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    mixSlider.setTextBoxIsEditable(true);
    mixSlider.setTextValueSuffix(" %");
    mixSlider.setDoubleClickReturnValue(true, 100.0);
    addAndMakeVisible(mixSlider);
    mixSlider.onValueChange = [this]
    {
        ensureBandActiveFromEdit();
        mirrorToLinkedChannel("mix", static_cast<float>(mixSlider.getValue()));
    };

    dynEnableToggle.setButtonText("Dynamic");
    dynEnableToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    addAndMakeVisible(dynEnableToggle);
    dynEnableToggle.onClick = [this]
    {
        ensureBandActiveFromEdit();
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
    };
    dynDownButton.onClick = [this]()
    {
        ensureBandActiveFromEdit();
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "dynMode")))
            param->setValueNotifyingHost(param->convertTo0to1(1.0f));
        dynUpButton.setToggleState(false, juce::dontSendNotification);
        dynDownButton.setToggleState(true, juce::dontSendNotification);
    };
    addAndMakeVisible(dynUpButton);
    addAndMakeVisible(dynDownButton);

    dynExternalToggle.setButtonText("Ext SC");
    dynExternalToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    addAndMakeVisible(dynExternalToggle);
    dynExternalToggle.onClick = [this]
    {
        ensureBandActiveFromEdit();
    };

    thresholdSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    thresholdSlider.setTextBoxIsEditable(true);
    thresholdSlider.setTextValueSuffix(" dB");
    thresholdSlider.setDoubleClickReturnValue(true, -24.0);
    addAndMakeVisible(thresholdSlider);
    thresholdSlider.onValueChange = [this]
    {
        ensureBandActiveFromEdit();
    };

    attackSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    attackSlider.setTextBoxIsEditable(true);
    attackSlider.setTextValueSuffix(" ms");
    attackSlider.setDoubleClickReturnValue(true, 20.0);
    addAndMakeVisible(attackSlider);
    attackSlider.onValueChange = [this]
    {
        ensureBandActiveFromEdit();
    };

    releaseSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    releaseSlider.setTextBoxIsEditable(true);
    releaseSlider.setTextValueSuffix(" ms");
    releaseSlider.setDoubleClickReturnValue(true, 200.0);
    addAndMakeVisible(releaseSlider);
    releaseSlider.onValueChange = [this]
    {
        ensureBandActiveFromEdit();
    };

    autoScaleToggle.setButtonText("Auto Scale");
    autoScaleToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    addAndMakeVisible(autoScaleToggle);
    autoScaleToggle.onClick = [this]
    {
        ensureBandActiveFromEdit();
    };

    updateAttachments();
    updateBandKnobColours();
    if (auto* typeParam = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "type")))
    {
        const int typeIndex = static_cast<int>(typeParam->convertFrom0to1(typeParam->getValue()));
        typeBox.setSelectedItemIndex(typeIndex, juce::dontSendNotification);
    }
    updateTypeUi();
    updateMsChoices();
    updateComboBoxWidths();
    syncMsSelectionFromParam();
}

void BandControlsPanel::ensureBandActiveFromEdit()
{
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
    selectedChannel = juce::jlimit(0, ParamIDs::kMaxChannels - 1, channelIndex);
    selectedBand = juce::jlimit(0, ParamIDs::kBandsPerChannel - 1, bandIndex);

    titleLabel.setText("Band " + juce::String(selectedBand + 1)
                           + " / " + juce::String(ParamIDs::kBandsPerChannel),
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
    updateAttachments();
    updateTypeUi();
    updateMsChoices();
    updateComboBoxWidths();
    syncMsSelectionFromParam();
}

void BandControlsPanel::setChannelNames(const std::vector<juce::String>& names)
{
    if (channelNames == names)
        return;
    channelNames = names;
    updateMsChoices();
    updateComboBoxWidths();
    syncMsSelectionFromParam();
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
    applyColour(thresholdSlider);
    applyColour(attackSlider);
    applyColour(releaseSlider);
}

void BandControlsPanel::setMsEnabled(bool enabled)
{
    msEnabled = enabled;
    msBox.setEnabled(true);
    msBox.setAlpha(1.0f);
}


void BandControlsPanel::paint(juce::Graphics& g)
{
    g.setColour(theme.panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    g.setColour(theme.panelOutline);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);

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

}

void BandControlsPanel::timerCallback()
{
    detectorDb = processor.getBandDetectorDb(selectedChannel, selectedBand);
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
        auto baseColour = ColorUtils::bandColour(i);
        if (bypassed)
            baseColour = baseColour.withSaturation(0.05f).withBrightness(0.35f);
        auto& button = bandSelectButtons[static_cast<size_t>(i)];
        button.setColour(juce::TextButton::buttonColourId, baseColour.withAlpha(0.2f));
        button.setColour(juce::TextButton::buttonOnColourId, baseColour.withAlpha(0.55f));
        button.setColour(juce::TextButton::textColourOffId, baseColour.withAlpha(bypassed ? 0.45f : 0.9f));
        button.setColour(juce::TextButton::textColourOnId, bypassed ? theme.textMuted : juce::Colours::white);

        auto& soloButton = bandSoloButtons[static_cast<size_t>(i)];
        soloButton.setToggleState(soloed, juce::dontSendNotification);
        soloButton.setColour(juce::ToggleButton::textColourId, soloed ? juce::Colours::white : theme.textMuted);
        soloButton.setColour(juce::ToggleButton::tickColourId, baseColour);
        soloButton.setColour(juce::ToggleButton::tickDisabledColourId, baseColour.withAlpha(0.4f));
    }
    repaint(detectorMeterBounds.getSmallestIntegerContainer());
}

void BandControlsPanel::mouseDoubleClick(const juce::MouseEvent& event)
{
    for (int i = 0; i < static_cast<int>(bandSelectButtons.size()); ++i)
    {
        if (event.eventComponent == &bandSelectButtons[static_cast<size_t>(i)])
        {
            const auto bypassId = ParamIDs::bandParamId(selectedChannel, i, "bypass");
            if (auto* param = parameters.getParameter(bypassId))
            {
                const float current = param->getValue();
                const float target = current < 0.5f ? 1.0f : 0.0f;
                param->setValueNotifyingHost(target);
            }
            break;
        }
    }

    for (int i = 0; i < static_cast<int>(bandSoloButtons.size()); ++i)
    {
        if (event.eventComponent == &bandSoloButtons[static_cast<size_t>(i)])
        {
            if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, i, "solo")))
                param->setValueNotifyingHost(param->convertTo0to1(0.0f));
            bandSoloButtons[static_cast<size_t>(i)].setToggleState(false, juce::dontSendNotification);
            break;
        }
    }
}

void BandControlsPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    const int gap = 6;
    const int labelHeight = 14;
    const int rowHeight = 20;
    const int comboHeight = 18;
    const int knobRowHeight = 120;
    const int knobSize = juce::jmin(86, knobRowHeight - labelHeight - 6);

    auto left = bounds.removeFromLeft(static_cast<int>(bounds.getWidth() * 0.62f));
    auto right = bounds;

    auto headerRow = left.removeFromTop(rowHeight);
    titleLabel.setBounds(headerRow.removeFromLeft(100));
    const int btnW = 52;
    copyButton.setBounds(headerRow.removeFromLeft(btnW));
    pasteButton.setBounds(headerRow.removeFromLeft(btnW));
    defaultButton.setBounds(headerRow.removeFromLeft(btnW));
    eqSectionLabel.setBounds(headerRow);

    left.removeFromTop(2);
    auto bandRow = left.removeFromTop(rowHeight);
    const int bandBtnWidth = std::max(18, (bandRow.getWidth() - gap * 11) / 12);
    for (int i = 0; i < static_cast<int>(bandSelectButtons.size()); ++i)
    {
        bandSelectButtons[static_cast<size_t>(i)].setBounds(
            bandRow.removeFromLeft(bandBtnWidth));
        bandRow.removeFromLeft(gap);
    }

    left.removeFromTop(2);
    auto soloRow = left.removeFromTop(rowHeight);
    const int soloBtnWidth = std::max(18, (soloRow.getWidth() - gap * 11) / 12);
    for (int i = 0; i < static_cast<int>(bandSoloButtons.size()); ++i)
    {
        bandSoloButtons[static_cast<size_t>(i)].setBounds(
            soloRow.removeFromLeft(soloBtnWidth));
        soloRow.removeFromLeft(gap);
    }
    left.removeFromTop(gap);

    const int eqKnobTop = left.getY();
    auto knobsRow = left.removeFromTop(knobRowHeight);
    const int knobWidth = (knobsRow.getWidth() - gap * 3) / 4;
    auto squareKnob = [](juce::Rectangle<int> area)
    {
        const int size = std::min(area.getWidth(), area.getHeight());
        return juce::Rectangle<int>(size, size).withCentre(area.getCentre());
    };
    auto freqArea = knobsRow.removeFromLeft(knobWidth);
    freqLabel.setBounds(freqArea.removeFromTop(labelHeight));
    freqSlider.setBounds(squareKnob(freqArea).withSizeKeepingCentre(knobSize, knobSize));
    knobsRow.removeFromLeft(gap);
    auto gainArea = knobsRow.removeFromLeft(knobWidth);
    gainLabel.setBounds(gainArea.removeFromTop(labelHeight));
    gainSlider.setBounds(squareKnob(gainArea).withSizeKeepingCentre(knobSize, knobSize));
    knobsRow.removeFromLeft(gap);
    auto qArea = knobsRow.removeFromLeft(knobWidth);
    qLabel.setBounds(qArea.removeFromTop(labelHeight));
    qSlider.setBounds(squareKnob(qArea).withSizeKeepingCentre(knobSize, knobSize));
    knobsRow.removeFromLeft(gap);
    auto mixArea = knobsRow.removeFromLeft(knobWidth);
    mixLabel.setBounds(mixArea.removeFromTop(labelHeight));
    mixSlider.setBounds(squareKnob(mixArea).withSizeKeepingCentre(knobSize, knobSize));

    left.removeFromTop(gap);
    auto channelRow = left.removeFromTop(labelHeight + rowHeight);
    msLabel.setBounds(channelRow.removeFromTop(labelHeight));
    const int msWidth = juce::jmin(channelRow.getWidth(), comboWidthMs);
    msBox.setBounds(channelRow.withHeight(comboHeight).withSizeKeepingCentre(msWidth, comboHeight));

    left.removeFromTop(gap);
    auto typeRow = left.removeFromTop(labelHeight + rowHeight);
    typeLabel.setBounds(typeRow.removeFromTop(labelHeight));
    const int typeWidth = juce::jmin(typeRow.getWidth(), comboWidthType);
    typeBox.setBounds(typeRow.withHeight(comboHeight).withSizeKeepingCentre(typeWidth, comboHeight));

    left.removeFromTop(2);
    auto togglesRow = left.removeFromTop(rowHeight);
    juce::ignoreUnused(togglesRow);

    // Dynamic panel (right column)
    const int padBeforeDynHeader = juce::jmax(0, eqKnobTop
                                                     - (right.getY() + rowHeight + 4));
    right.removeFromTop(padBeforeDynHeader);
    auto dynRow = right.removeFromTop(rowHeight);
    dynEnableToggle.setBounds(dynRow.removeFromLeft(88));
    dynUpButton.setBounds(dynRow.removeFromLeft(48));
    dynDownButton.setBounds(dynRow.removeFromLeft(56));
    autoScaleToggle.setBounds(dynRow.removeFromLeft(82));
    dynExternalToggle.setBounds(dynRow);

    right.removeFromTop(4);
    auto dynKnobs = right.removeFromTop(knobRowHeight);
    const int dynKnobWidth = (dynKnobs.getWidth() - gap * 2) / 3;
    auto threshArea = dynKnobs.removeFromLeft(dynKnobWidth);
    thresholdLabel.setBounds(threshArea.removeFromTop(labelHeight));
    thresholdSlider.setBounds(squareKnob(threshArea).withSizeKeepingCentre(knobSize, knobSize));
    dynKnobs.removeFromLeft(gap);
    auto attackArea = dynKnobs.removeFromLeft(dynKnobWidth);
    attackLabel.setBounds(attackArea.removeFromTop(labelHeight));
    attackSlider.setBounds(squareKnob(attackArea).withSizeKeepingCentre(knobSize, knobSize));
    dynKnobs.removeFromLeft(gap);
    auto releaseArea = dynKnobs.removeFromLeft(dynKnobWidth);
    releaseLabel.setBounds(releaseArea.removeFromTop(labelHeight));
    releaseSlider.setBounds(squareKnob(releaseArea).withSizeKeepingCentre(knobSize, knobSize));

    right.removeFromTop(4);
    auto detectorArea = right.removeFromTop(52);
    const int detectorSize = std::min(detectorArea.getWidth(), detectorArea.getHeight());
    detectorMeterBounds = juce::Rectangle<int>(detectorSize, detectorSize)
                              .withCentre(detectorArea.getCentre())
                              .toFloat();

    left.removeFromTop(2);
    auto slopeRow = left.removeFromTop(labelHeight + rowHeight);
    slopeLabel.setBounds(slopeRow.removeFromTop(labelHeight));
    const int slopeWidth = juce::jmin(slopeRow.getWidth(), comboWidthSlope);
    slopeBox.setBounds(slopeRow.withHeight(comboHeight).withSizeKeepingCentre(slopeWidth, comboHeight));

}

void BandControlsPanel::updateAttachments()
{
    const auto freqId = ParamIDs::bandParamId(selectedChannel, selectedBand, "freq");
    const auto gainId = ParamIDs::bandParamId(selectedChannel, selectedBand, "gain");
    const auto qId = ParamIDs::bandParamId(selectedChannel, selectedBand, "q");
    const auto msId = ParamIDs::bandParamId(selectedChannel, selectedBand, "ms");
    const auto slopeId = ParamIDs::bandParamId(selectedChannel, selectedBand, "slope");
    const auto mixId = ParamIDs::bandParamId(selectedChannel, selectedBand, "mix");

    freqAttachment = std::make_unique<SliderAttachment>(parameters, freqId, freqSlider);
    gainAttachment = std::make_unique<SliderAttachment>(parameters, gainId, gainSlider);
    qAttachment = std::make_unique<SliderAttachment>(parameters, qId, qSlider);
    mixAttachment = std::make_unique<SliderAttachment>(parameters, mixId, mixSlider);
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

void BandControlsPanel::resetSelectedBand()
{
    const int channel = selectedChannel;
    const int band = selectedBand;
    auto resetParam = [this, channel, band](const juce::String& suffix)
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(channel, band, suffix)))
            param->setValueNotifyingHost(param->getDefaultValue());
    };

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

    if (auto* bypassParam = parameters.getParameter(ParamIDs::bandParamId(channel, band, "bypass")))
        bypassParam->setValueNotifyingHost(0.0f);
    if (auto* soloParam = parameters.getParameter(ParamIDs::bandParamId(channel, band, "solo")))
        soloParam->setValueNotifyingHost(0.0f);
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
    comboWidthMs = computeWidth(msLabels, font) + padding;

    juce::StringArray slopeLabels;
    for (int i = 0; i < slopeBox.getNumItems(); ++i)
        slopeLabels.add(slopeBox.getItemText(i + 1));
    comboWidthSlope = computeWidth(slopeLabels, font) + padding;
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
    dynEnableToggle.setVisible(true);
    dynUpButton.setVisible(true);
    dynDownButton.setVisible(true);
    thresholdSlider.setVisible(true);
    attackSlider.setVisible(true);
    releaseSlider.setVisible(true);
    autoScaleToggle.setVisible(true);
    dynExternalToggle.setVisible(true);
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

    const bool hasL = containsName(channelNames, "L");
    const bool hasR = containsName(channelNames, "R");

    auto addChoice = [&](int index)
    {
        msChoiceMap.push_back(index);
        labels.add(kMsChoices[index]);
    };

    addChoice(0); // All
    if (hasL && hasR)
    {
        addChoice(1); // Mid
        addChoice(2); // Side
        addChoice(3); // L/R
        addChoice(4); // Left
        addChoice(5); // Right
        addChoice(6); // Mono
        addChoice(7); // L
        addChoice(8); // R
    }

    auto addIfPresent = [&](int index, const juce::String& name)
    {
        if (containsName(channelNames, name))
            addChoice(index);
    };

    addIfPresent(9, "C");
    addIfPresent(10, "LFE");
    addIfPresent(11, "Ls");
    addIfPresent(12, "Rs");
    addIfPresent(13, "Lrs");
    addIfPresent(14, "Rrs");
    addIfPresent(15, "Lc");
    addIfPresent(16, "Rc");
    addIfPresent(17, "Ltf");
    addIfPresent(18, "Rtf");
    addIfPresent(19, "Tfc");
    addIfPresent(20, "Tm");
    addIfPresent(21, "Ltr");
    addIfPresent(22, "Rtr");
    addIfPresent(23, "Trc");
    addIfPresent(24, "Lts");
    addIfPresent(25, "Rts");
    addIfPresent(26, "Lw");
    addIfPresent(27, "Rw");
    addIfPresent(28, "LFE2");
    addIfPresent(29, "Bfl");
    addIfPresent(30, "Bfr");
    addIfPresent(31, "Bfc");

    auto addPairIfPresent = [&](int index, const juce::String& left, const juce::String& right)
    {
        if (containsName(channelNames, left) && containsName(channelNames, right))
            addChoice(index);
    };

    addPairIfPresent(32, "Ls", "Rs");
    addPairIfPresent(33, "Lrs", "Rrs");
    addPairIfPresent(34, "Lc", "Rc");
    addPairIfPresent(35, "Ltf", "Rtf");
    addPairIfPresent(36, "Ltr", "Rtr");
    addPairIfPresent(37, "Lts", "Rts");
    addPairIfPresent(38, "Lw", "Rw");
    addPairIfPresent(39, "Bfl", "Bfr");

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
