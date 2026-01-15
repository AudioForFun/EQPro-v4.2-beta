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
    "Stereo",
    "Mid",
    "Side",
    "Left",
    "Right"
};

const juce::StringArray kDynModeChoices {
    "Down",
    "Up"
};

const juce::StringArray kDynSourceChoices {
    "Internal",
    "External"
};
} // namespace

BandControlsPanel::BandControlsPanel(EQProAudioProcessor& processorIn)
    : processor(processorIn),
      parameters(processorIn.getParameters())
{
    titleLabel.setText("Band 1", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, theme.text);
    addAndMakeVisible(titleLabel);

    copyButton.setButtonText("Copy");
    copyButton.onClick = [this] { copyBandState(); };
    addAndMakeVisible(copyButton);

    pasteButton.setButtonText("Paste");
    pasteButton.onClick = [this] { pasteBandState(); };
    addAndMakeVisible(pasteButton);

    linkButton.setButtonText("Link");
    linkButton.onClick = [this]
    {
        linkEnabled = linkButton.getToggleState();
        if (linkEnabled && linkPairBox.getSelectedItemIndex() == 0 && linkPairBox.getNumItems() > 1)
            linkPairBox.setSelectedItemIndex(1, juce::dontSendNotification);
    };
    addAndMakeVisible(linkButton);

    linkPairLabel.setText("Link Pair", juce::dontSendNotification);
    linkPairLabel.setJustificationType(juce::Justification::centredLeft);
    linkPairLabel.setColour(juce::Label::textColourId, theme.textMuted);
    addAndMakeVisible(linkPairLabel);

    linkPairBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    linkPairBox.setColour(juce::ComboBox::textColourId, theme.text);
    linkPairBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    addAndMakeVisible(linkPairBox);
    linkPairBox.onChange = [this]
    {
        const bool enabled = linkPairBox.getSelectedItemIndex() > 0;
        linkEnabled = enabled;
        linkButton.setToggleState(enabled, juce::dontSendNotification);
    };

    freqSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    freqSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    freqSlider.setTextBoxIsEditable(true);
    freqSlider.setSkewFactorFromMidPoint(1000.0);
    freqSlider.setTextValueSuffix(" Hz");
    freqSlider.onValueChange = [this]
    {
        mirrorToLinkedChannel("freq", static_cast<float>(freqSlider.getValue()));
    };
    addAndMakeVisible(freqSlider);

    gainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    gainSlider.setTextBoxIsEditable(true);
    gainSlider.setTextValueSuffix(" dB");
    gainSlider.onValueChange = [this]
    {
        mirrorToLinkedChannel("gain", static_cast<float>(gainSlider.getValue()));
    };
    addAndMakeVisible(gainSlider);

    qSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    qSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    qSlider.setTextBoxIsEditable(true);
    qSlider.onValueChange = [this]
    {
        mirrorToLinkedChannel("q", static_cast<float>(qSlider.getValue()));
    };
    addAndMakeVisible(qSlider);

    typeBox.addItemList(kFilterTypeChoices, 1);
    typeBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    typeBox.setColour(juce::ComboBox::textColourId, theme.text);
    typeBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    addAndMakeVisible(typeBox);
    typeBox.onChange = [this]
    {
        updateTypeUi();
        mirrorToLinkedChannel("type", static_cast<float>(typeBox.getSelectedItemIndex()));
    };

    msBox.addItemList(kMsChoices, 1);
    msBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    msBox.setColour(juce::ComboBox::textColourId, theme.text);
    msBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    addAndMakeVisible(msBox);
    msBox.onChange = [this]
    {
        mirrorToLinkedChannel("ms", static_cast<float>(msBox.getSelectedItemIndex()));
    };

    slopeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    slopeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    slopeSlider.setTextBoxIsEditable(true);
    slopeSlider.setTextValueSuffix(" dB/oct");
    addAndMakeVisible(slopeSlider);
    slopeSlider.onValueChange = [this]
    {
        mirrorToLinkedChannel("slope", static_cast<float>(slopeSlider.getValue()));
    };

    bypassButton.setButtonText("Bypass");
    bypassButton.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    addAndMakeVisible(bypassButton);
    bypassButton.onClick = [this]
    {
        mirrorToLinkedChannel("bypass", bypassButton.getToggleState() ? 1.0f : 0.0f);
    };

    soloButton.setButtonText("Solo");
    soloButton.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    addAndMakeVisible(soloButton);
    soloButton.onClick = [this]
    {
        const bool enabled = soloButton.getToggleState();
        if (enabled)
        {
            for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
            {
                if (band == selectedBand)
                    continue;
                if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, band, "solo")))
                    param->setValueNotifyingHost(param->convertTo0to1(0.0f));
            }
        }
        mirrorToLinkedChannel("solo", enabled ? 1.0f : 0.0f);
    };

    dynEnableButton.setButtonText("Dyn");
    dynEnableButton.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    addAndMakeVisible(dynEnableButton);
    dynEnableButton.onClick = [this]
    {
        updateTypeUi();
        mirrorToLinkedChannel("dynEnable", dynEnableButton.getToggleState() ? 1.0f : 0.0f);
    };

    dynModeBox.addItemList(kDynModeChoices, 1);
    dynModeBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    dynModeBox.setColour(juce::ComboBox::textColourId, theme.text);
    dynModeBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    addAndMakeVisible(dynModeBox);
    dynModeBox.onChange = [this]
    {
        mirrorToLinkedChannel("dynMode", static_cast<float>(dynModeBox.getSelectedItemIndex()));
    };

    dynSourceBox.addItemList(kDynSourceChoices, 1);
    dynSourceBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    dynSourceBox.setColour(juce::ComboBox::textColourId, theme.text);
    dynSourceBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    addAndMakeVisible(dynSourceBox);
    dynSourceBox.onChange = [this]
    {
        mirrorToLinkedChannel("dynSource", static_cast<float>(dynSourceBox.getSelectedItemIndex()));
    };

    dynFilterButton.setButtonText("Filter");
    dynFilterButton.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    addAndMakeVisible(dynFilterButton);
    dynFilterButton.onClick = [this]
    {
        mirrorToLinkedChannel("dynFilter", dynFilterButton.getToggleState() ? 1.0f : 0.0f);
    };

    thresholdSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    thresholdSlider.setTextBoxIsEditable(true);
    thresholdSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(thresholdSlider);
    thresholdSlider.onValueChange = [this]
    {
        mirrorToLinkedChannel("dynThresh", static_cast<float>(thresholdSlider.getValue()));
    };

    attackSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    attackSlider.setTextBoxIsEditable(true);
    attackSlider.setTextValueSuffix(" ms");
    addAndMakeVisible(attackSlider);
    attackSlider.onValueChange = [this]
    {
        mirrorToLinkedChannel("dynAttack", static_cast<float>(attackSlider.getValue()));
    };

    releaseSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    releaseSlider.setTextBoxIsEditable(true);
    releaseSlider.setTextValueSuffix(" ms");
    addAndMakeVisible(releaseSlider);
    releaseSlider.onValueChange = [this]
    {
        mirrorToLinkedChannel("dynRelease", static_cast<float>(releaseSlider.getValue()));
    };

    dynMixSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    dynMixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    dynMixSlider.setTextBoxIsEditable(true);
    dynMixSlider.setTextValueSuffix(" %");
    addAndMakeVisible(dynMixSlider);
    dynMixSlider.onValueChange = [this]
    {
        mirrorToLinkedChannel("dynMix", static_cast<float>(dynMixSlider.getValue()));
    };

    updateAttachments();
    updateTypeUi();
    startTimerHz(30);
}

void BandControlsPanel::setSelectedBand(int channelIndex, int bandIndex)
{
    selectedChannel = juce::jlimit(0, ParamIDs::kMaxChannels - 1, channelIndex);
    selectedBand = juce::jlimit(0, ParamIDs::kBandsPerChannel - 1, bandIndex);

    titleLabel.setText("Band " + juce::String(selectedBand + 1), juce::dontSendNotification);
    titleLabel.setColour(juce::Label::textColourId, ColorUtils::bandColour(selectedBand));
    updateAttachments();
    updateTypeUi();
}

void BandControlsPanel::setTheme(const ThemeColors& newTheme)
{
    theme = newTheme;
    titleLabel.setColour(juce::Label::textColourId, ColorUtils::bandColour(selectedBand));
    typeBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    typeBox.setColour(juce::ComboBox::textColourId, theme.text);
    typeBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    msBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    msBox.setColour(juce::ComboBox::textColourId, theme.text);
    msBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    slopeSlider.setColour(juce::Slider::trackColourId, theme.accent);
    slopeSlider.setColour(juce::Slider::textBoxTextColourId, theme.text);
    slopeSlider.setColour(juce::Slider::textBoxOutlineColourId, theme.panelOutline);
    bypassButton.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    copyButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    pasteButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    linkButton.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    soloButton.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    dynEnableButton.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    dynModeBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    dynModeBox.setColour(juce::ComboBox::textColourId, theme.text);
    dynModeBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    dynSourceBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    dynSourceBox.setColour(juce::ComboBox::textColourId, theme.text);
    dynSourceBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    dynFilterButton.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    linkPairLabel.setColour(juce::Label::textColourId, theme.textMuted);
    linkPairBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    linkPairBox.setColour(juce::ComboBox::textColourId, theme.text);
    linkPairBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    repaint();
}

void BandControlsPanel::setMsEnabled(bool enabled)
{
    msEnabled = enabled;
    msBox.setEnabled(msEnabled);
    msBox.setAlpha(msEnabled ? 1.0f : 0.5f);
}

void BandControlsPanel::setLinkPairs(const juce::StringArray& names,
                                     const std::vector<std::pair<int, int>>& pairs)
{
    linkPairNames = names;
    linkPairIndices = pairs;
    linkPairBox.clear();
    linkPairBox.addItem("Off", 1);
    linkPairBox.addItemList(linkPairNames, 2);
    linkPairBox.setSelectedItemIndex(0, juce::dontSendNotification);
    const bool hasPairs = ! linkPairIndices.empty();
    linkButton.setEnabled(hasPairs);
    linkPairBox.setEnabled(hasPairs);
}

void BandControlsPanel::paint(juce::Graphics& g)
{
    g.setColour(theme.panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    g.setColour(theme.panelOutline);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);

    auto meterArea = getLocalBounds().reduced(12).removeFromBottom(10);
    g.setColour(theme.panelOutline);
    g.drawRoundedRectangle(meterArea.toFloat(), 3.0f, 1.0f);
    const float norm = juce::jlimit(-1.0f, 1.0f, dynMeterDb / 12.0f);
    const auto fillWidth = static_cast<int>((norm * 0.5f + 0.5f) * meterArea.getWidth());
    g.setColour(theme.accent.withAlpha(0.7f));
    g.fillRoundedRectangle(meterArea.removeFromLeft(fillWidth).toFloat(), 3.0f);
}

void BandControlsPanel::resized()
{
    auto bounds = getLocalBounds().reduced(12);
    auto headerRow = bounds.removeFromTop(24);
    titleLabel.setBounds(headerRow.removeFromLeft(80));
    copyButton.setBounds(headerRow.removeFromLeft(50));
    pasteButton.setBounds(headerRow.removeFromLeft(50));
    linkButton.setBounds(headerRow);

    bounds.removeFromTop(4);
    auto linkRow = bounds.removeFromTop(22);
    linkPairLabel.setBounds(linkRow.removeFromLeft(70));
    linkPairBox.setBounds(linkRow);

    auto topRow = bounds.removeFromTop(100);
    const int knobWidth = (topRow.getWidth() - 16) / 3;
    freqSlider.setBounds(topRow.removeFromLeft(knobWidth));
    topRow.removeFromLeft(8);
    gainSlider.setBounds(topRow.removeFromLeft(knobWidth));
    topRow.removeFromLeft(8);
    qSlider.setBounds(topRow.removeFromLeft(knobWidth));

    bounds.removeFromTop(8);
    typeBox.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(6);
    msBox.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(6);
    slopeSlider.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(6);
    auto togglesRow = bounds.removeFromTop(22);
    bypassButton.setBounds(togglesRow.removeFromLeft(70));
    soloButton.setBounds(togglesRow.removeFromLeft(60));
    dynEnableButton.setBounds(togglesRow);

    bounds.removeFromTop(6);
    auto dynHeaderRow = bounds.removeFromTop(22);
    const int dynHeaderGap = 4;
    const int dynHeaderWidth = (dynHeaderRow.getWidth() - dynHeaderGap * 2) / 3;
    dynModeBox.setBounds(dynHeaderRow.removeFromLeft(dynHeaderWidth));
    dynHeaderRow.removeFromLeft(dynHeaderGap);
    dynSourceBox.setBounds(dynHeaderRow.removeFromLeft(dynHeaderWidth));
    dynHeaderRow.removeFromLeft(dynHeaderGap);
    dynFilterButton.setBounds(dynHeaderRow);
    bounds.removeFromTop(6);
    auto dynRow = bounds.removeFromTop(96);
    const int dynWidth = (dynRow.getWidth() - 12) / 4;
    thresholdSlider.setBounds(dynRow.removeFromLeft(dynWidth));
    dynRow.removeFromLeft(4);
    attackSlider.setBounds(dynRow.removeFromLeft(dynWidth));
    dynRow.removeFromLeft(4);
    releaseSlider.setBounds(dynRow.removeFromLeft(dynWidth));
    dynRow.removeFromLeft(4);
    dynMixSlider.setBounds(dynRow.removeFromLeft(dynWidth));
}

void BandControlsPanel::updateAttachments()
{
    const auto freqId = ParamIDs::bandParamId(selectedChannel, selectedBand, "freq");
    const auto gainId = ParamIDs::bandParamId(selectedChannel, selectedBand, "gain");
    const auto qId = ParamIDs::bandParamId(selectedChannel, selectedBand, "q");
    const auto typeId = ParamIDs::bandParamId(selectedChannel, selectedBand, "type");
    const auto bypassId = ParamIDs::bandParamId(selectedChannel, selectedBand, "bypass");
    const auto msId = ParamIDs::bandParamId(selectedChannel, selectedBand, "ms");
    const auto slopeId = ParamIDs::bandParamId(selectedChannel, selectedBand, "slope");
    const auto soloId = ParamIDs::bandParamId(selectedChannel, selectedBand, "solo");
    const auto dynEnableId = ParamIDs::bandParamId(selectedChannel, selectedBand, "dynEnable");
    const auto dynModeId = ParamIDs::bandParamId(selectedChannel, selectedBand, "dynMode");
    const auto dynThresholdId = ParamIDs::bandParamId(selectedChannel, selectedBand, "dynThresh");
    const auto dynAttackId = ParamIDs::bandParamId(selectedChannel, selectedBand, "dynAttack");
    const auto dynReleaseId = ParamIDs::bandParamId(selectedChannel, selectedBand, "dynRelease");
    const auto dynMixId = ParamIDs::bandParamId(selectedChannel, selectedBand, "dynMix");
    const auto dynSourceId = ParamIDs::bandParamId(selectedChannel, selectedBand, "dynSource");
    const auto dynFilterId = ParamIDs::bandParamId(selectedChannel, selectedBand, "dynFilter");

    freqAttachment = std::make_unique<SliderAttachment>(parameters, freqId, freqSlider);
    gainAttachment = std::make_unique<SliderAttachment>(parameters, gainId, gainSlider);
    qAttachment = std::make_unique<SliderAttachment>(parameters, qId, qSlider);
    typeAttachment = std::make_unique<ComboBoxAttachment>(parameters, typeId, typeBox);
    msAttachment = std::make_unique<ComboBoxAttachment>(parameters, msId, msBox);
    slopeAttachment = std::make_unique<SliderAttachment>(parameters, slopeId, slopeSlider);
    bypassAttachment = std::make_unique<ButtonAttachment>(parameters, bypassId, bypassButton);
    soloAttachment = std::make_unique<ButtonAttachment>(parameters, soloId, soloButton);
    dynEnableAttachment = std::make_unique<ButtonAttachment>(parameters, dynEnableId, dynEnableButton);
    dynModeAttachment = std::make_unique<ComboBoxAttachment>(parameters, dynModeId, dynModeBox);
    dynSourceAttachment = std::make_unique<ComboBoxAttachment>(parameters, dynSourceId, dynSourceBox);
    dynFilterAttachment = std::make_unique<ButtonAttachment>(parameters, dynFilterId, dynFilterButton);
    thresholdAttachment = std::make_unique<SliderAttachment>(parameters, dynThresholdId, thresholdSlider);
    attackAttachment = std::make_unique<SliderAttachment>(parameters, dynAttackId, attackSlider);
    releaseAttachment = std::make_unique<SliderAttachment>(parameters, dynReleaseId, releaseSlider);
    dynMixAttachment = std::make_unique<SliderAttachment>(parameters, dynMixId, dynMixSlider);
}

void BandControlsPanel::updateTypeUi()
{
    const int typeIndex = typeBox.getSelectedItemIndex();
    const bool isAllPass = typeIndex == 7;
    const bool isHpLp = (typeIndex == 3 || typeIndex == 4);
    gainSlider.setEnabled(! isAllPass);
    gainSlider.setAlpha(isAllPass ? 0.5f : 1.0f);
    msBox.setEnabled(msEnabled);
    msBox.setAlpha(msEnabled ? 1.0f : 0.5f);
    slopeSlider.setEnabled(isHpLp);
    slopeSlider.setAlpha(isHpLp ? 1.0f : 0.5f);

    const bool supportsDyn = (typeIndex != 3 && typeIndex != 4 && typeIndex != 7);
    dynEnableButton.setEnabled(supportsDyn);
    dynEnableButton.setAlpha(supportsDyn ? 1.0f : 0.5f);
    const bool dynEnabled = dynEnableButton.getToggleState() && supportsDyn;
    dynModeBox.setEnabled(dynEnabled);
    dynSourceBox.setEnabled(dynEnabled);
    dynFilterButton.setEnabled(dynEnabled);
    thresholdSlider.setEnabled(dynEnabled);
    attackSlider.setEnabled(dynEnabled);
    releaseSlider.setEnabled(dynEnabled);
    dynMixSlider.setEnabled(dynEnabled);
    const float dynAlpha = dynEnabled ? 1.0f : 0.5f;
    dynModeBox.setAlpha(dynAlpha);
    dynSourceBox.setAlpha(dynAlpha);
    dynFilterButton.setAlpha(dynAlpha);
    thresholdSlider.setAlpha(dynAlpha);
    attackSlider.setAlpha(dynAlpha);
    releaseSlider.setAlpha(dynAlpha);
    dynMixSlider.setAlpha(dynAlpha);
}

void BandControlsPanel::timerCallback()
{
    dynMeterDb = processor.getDynamicGainDb(selectedChannel, selectedBand);
    repaint();
}

void BandControlsPanel::copyBandState()
{
    BandState state;
    state.freq = static_cast<float>(freqSlider.getValue());
    state.gain = static_cast<float>(gainSlider.getValue());
    state.q = static_cast<float>(qSlider.getValue());
    state.type = static_cast<float>(typeBox.getSelectedItemIndex());
    state.bypass = bypassButton.getToggleState() ? 1.0f : 0.0f;
    state.ms = static_cast<float>(msBox.getSelectedItemIndex());
    state.slope = static_cast<float>(slopeSlider.getValue());
    state.solo = soloButton.getToggleState() ? 1.0f : 0.0f;
    state.dynEnable = dynEnableButton.getToggleState() ? 1.0f : 0.0f;
    state.dynMode = static_cast<float>(dynModeBox.getSelectedItemIndex());
    state.dynThresh = static_cast<float>(thresholdSlider.getValue());
    state.dynAttack = static_cast<float>(attackSlider.getValue());
    state.dynRelease = static_cast<float>(releaseSlider.getValue());
    state.dynMix = static_cast<float>(dynMixSlider.getValue());
    state.dynSource = static_cast<float>(dynSourceBox.getSelectedItemIndex());
    state.dynFilter = dynFilterButton.getToggleState() ? 1.0f : 0.0f;
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
    setParam("dynEnable", state.dynEnable);
    setParam("dynMode", state.dynMode);
    setParam("dynThresh", state.dynThresh);
    setParam("dynAttack", state.dynAttack);
    setParam("dynRelease", state.dynRelease);
    setParam("dynMix", state.dynMix);
    setParam("dynSource", state.dynSource);
    setParam("dynFilter", state.dynFilter);
}

void BandControlsPanel::mirrorToLinkedChannel(const juce::String& suffix, float value)
{
    if (! linkEnabled)
        return;
    const int pairIndex = linkPairBox.getSelectedItemIndex() - 1;
    if (pairIndex < 0 || pairIndex >= static_cast<int>(linkPairIndices.size()))
        return;

    const auto pair = linkPairIndices[static_cast<size_t>(pairIndex)];
    int other = -1;
    if (selectedChannel == pair.first)
        other = pair.second;
    else if (selectedChannel == pair.second)
        other = pair.first;
    else
        return;

    if (auto* param = parameters.getParameter(ParamIDs::bandParamId(other, selectedBand, suffix)))
        param->setValueNotifyingHost(param->convertTo0to1(value));
}
