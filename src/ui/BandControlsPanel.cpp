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

    resetButton.setButtonText("Reset");
    resetButton.onClick = [this] { resetSelectedBand(false); };
    addAndMakeVisible(resetButton);

    defaultButton.setButtonText("Default");
    defaultButton.onClick = [this] { resetSelectedBand(false); };
    addAndMakeVisible(defaultButton);

    deleteButton.setButtonText("Delete");
    deleteButton.onClick = [this] { resetSelectedBand(true); };
    addAndMakeVisible(deleteButton);

    prevBandButton.setButtonText("<");
    prevBandButton.onClick = [this]
    {
        const int next = (selectedBand + ParamIDs::kBandsPerChannel - 1) % ParamIDs::kBandsPerChannel;
        if (onBandNavigate)
            onBandNavigate(next);
    };
    addAndMakeVisible(prevBandButton);

    nextBandButton.setButtonText(">");
    nextBandButton.onClick = [this]
    {
        const int next = (selectedBand + 1) % ParamIDs::kBandsPerChannel;
        if (onBandNavigate)
            onBandNavigate(next);
    };
    addAndMakeVisible(nextBandButton);


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
    initLabel(qModeLabel, "Variable Q");
    initLabel(qAmountLabel, "Var Q");
    initLabel(typeLabel, "Type");
    initLabel(msLabel, "Channel");
    initLabel(slopeLabel, "Slope");
    initLabel(mixLabel, "Mix");

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

    qModeToggle.setButtonText("On");
    qModeToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    qModeToggle.onClick = [this]
    {
        if (auto* param = parameters.getParameter(ParamIDs::qMode))
            param->setValueNotifyingHost(qModeToggle.getToggleState() ? 1.0f : 0.0f);
        updateTypeUi();
    };
    addAndMakeVisible(qModeToggle);

    qAmountSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    qAmountSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    qAmountSlider.setTextBoxIsEditable(true);
    qAmountSlider.setTextValueSuffix(" %");
    addAndMakeVisible(qAmountSlider);

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

    mixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    mixSlider.setTextBoxIsEditable(true);
    mixSlider.setTextValueSuffix(" %");
    addAndMakeVisible(mixSlider);
    mixSlider.onValueChange = [this]
    {
        mirrorToLinkedChannel("mix", static_cast<float>(mixSlider.getValue()));
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

    updateAttachments();
    updateTypeUi();
}

void BandControlsPanel::setSelectedBand(int channelIndex, int bandIndex)
{
    selectedChannel = juce::jlimit(0, ParamIDs::kMaxChannels - 1, channelIndex);
    selectedBand = juce::jlimit(0, ParamIDs::kBandsPerChannel - 1, bandIndex);

    titleLabel.setText("Band " + juce::String(selectedBand + 1), juce::dontSendNotification);
    const auto bandColour = ColorUtils::bandColour(selectedBand);
    titleLabel.setColour(juce::Label::textColourId, bandColour);
    freqSlider.setColour(juce::Slider::trackColourId, bandColour);
    gainSlider.setColour(juce::Slider::trackColourId, bandColour);
    qSlider.setColour(juce::Slider::trackColourId, bandColour);
    qAmountSlider.setColour(juce::Slider::trackColourId, bandColour);
    updateAttachments();
    syncQModeToggle();
    updateTypeUi();
}

void BandControlsPanel::setTheme(const ThemeColors& newTheme)
{
    theme = newTheme;
    titleLabel.setColour(juce::Label::textColourId, ColorUtils::bandColour(selectedBand));
    freqLabel.setColour(juce::Label::textColourId, theme.textMuted);
    gainLabel.setColour(juce::Label::textColourId, theme.textMuted);
    qLabel.setColour(juce::Label::textColourId, theme.textMuted);
    qModeLabel.setColour(juce::Label::textColourId, theme.textMuted);
    qAmountLabel.setColour(juce::Label::textColourId, theme.textMuted);
    typeLabel.setColour(juce::Label::textColourId, theme.textMuted);
    msLabel.setColour(juce::Label::textColourId, theme.textMuted);
    slopeLabel.setColour(juce::Label::textColourId, theme.textMuted);
    mixLabel.setColour(juce::Label::textColourId, theme.textMuted);
    typeBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    typeBox.setColour(juce::ComboBox::textColourId, theme.text);
    typeBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    msBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    msBox.setColour(juce::ComboBox::textColourId, theme.text);
    msBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    slopeSlider.setColour(juce::Slider::trackColourId, theme.accent);
    slopeSlider.setColour(juce::Slider::textBoxTextColourId, theme.text);
    slopeSlider.setColour(juce::Slider::textBoxOutlineColourId, theme.panelOutline);
    mixSlider.setColour(juce::Slider::trackColourId, theme.accent);
    mixSlider.setColour(juce::Slider::textBoxTextColourId, theme.text);
    mixSlider.setColour(juce::Slider::textBoxOutlineColourId, theme.panelOutline);
    bypassButton.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    copyButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    pasteButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    resetButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    deleteButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    defaultButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    prevBandButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    nextBandButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    soloButton.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    qModeToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    qAmountSlider.setColour(juce::Slider::trackColourId, theme.accent);
    qAmountSlider.setColour(juce::Slider::textBoxTextColourId, theme.text);
    qAmountSlider.setColour(juce::Slider::textBoxOutlineColourId, theme.panelOutline);
    syncQModeToggle();
    repaint();
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

}

void BandControlsPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    const int gap = 6;
    const int labelHeight = 16;
    const int rowHeight = 22;
    const int knobRowHeight = 120;

    auto left = bounds.removeFromLeft(static_cast<int>(bounds.getWidth() * 0.75f));
    auto right = bounds;

    auto headerRow = left.removeFromTop(rowHeight);
    titleLabel.setBounds(headerRow.removeFromLeft(70));
    prevBandButton.setBounds(headerRow.removeFromLeft(24));
    nextBandButton.setBounds(headerRow.removeFromLeft(24));
    const int btnW = 58;
    copyButton.setBounds(headerRow.removeFromLeft(btnW));
    pasteButton.setBounds(headerRow.removeFromLeft(btnW));
    resetButton.setBounds(headerRow.removeFromLeft(btnW));
    defaultButton.setBounds(headerRow.removeFromLeft(btnW));
    deleteButton.setBounds(headerRow);

    left.removeFromTop(gap);
    auto knobsRow = left.removeFromTop(knobRowHeight);
    const int knobWidth = (knobsRow.getWidth() - gap * 3) / 4;
    auto freqArea = knobsRow.removeFromLeft(knobWidth);
    freqLabel.setBounds(freqArea.removeFromTop(labelHeight));
    freqSlider.setBounds(freqArea);
    knobsRow.removeFromLeft(gap);
    auto gainArea = knobsRow.removeFromLeft(knobWidth);
    gainLabel.setBounds(gainArea.removeFromTop(labelHeight));
    gainSlider.setBounds(gainArea);
    knobsRow.removeFromLeft(gap);
    auto qArea = knobsRow.removeFromLeft(knobWidth);
    qLabel.setBounds(qArea.removeFromTop(labelHeight));
    qSlider.setBounds(qArea);
    knobsRow.removeFromLeft(gap);
    auto qAmountArea = knobsRow.removeFromLeft(knobWidth);
    qAmountLabel.setBounds(qAmountArea.removeFromTop(labelHeight));
    qAmountSlider.setBounds(qAmountArea);

    left.removeFromTop(gap);
    auto qModeRow = left.removeFromTop(labelHeight + rowHeight);
    qModeLabel.setBounds(qModeRow.removeFromLeft(90));
    qModeToggle.setBounds(qModeRow.removeFromLeft(60));
    typeLabel.setBounds(qModeRow.removeFromLeft(50));
    typeBox.setBounds(qModeRow);

    left.removeFromTop(2);
    auto togglesRow = left.removeFromTop(rowHeight);
    bypassButton.setBounds(togglesRow.removeFromLeft(70));
    soloButton.setBounds(togglesRow.removeFromLeft(60));

    left.removeFromTop(2);
    auto msRow = left.removeFromTop(labelHeight + rowHeight);
    msLabel.setBounds(msRow.removeFromTop(labelHeight));
    msBox.setBounds(msRow);

    left.removeFromTop(2);
    auto slopeRow = left.removeFromTop(labelHeight + rowHeight);
    slopeLabel.setBounds(slopeRow.removeFromTop(labelHeight));
    slopeSlider.setBounds(slopeRow);

    left.removeFromTop(2);
    auto mixRow = left.removeFromTop(labelHeight + rowHeight);
    mixLabel.setBounds(mixRow.removeFromTop(labelHeight));
    mixSlider.setBounds(mixRow);
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
    const auto mixId = ParamIDs::bandParamId(selectedChannel, selectedBand, "mix");

    freqAttachment = std::make_unique<SliderAttachment>(parameters, freqId, freqSlider);
    gainAttachment = std::make_unique<SliderAttachment>(parameters, gainId, gainSlider);
    qAttachment = std::make_unique<SliderAttachment>(parameters, qId, qSlider);
    typeAttachment = std::make_unique<ComboBoxAttachment>(parameters, typeId, typeBox);
    msAttachment = std::make_unique<ComboBoxAttachment>(parameters, msId, msBox);
    slopeAttachment = std::make_unique<SliderAttachment>(parameters, slopeId, slopeSlider);
    mixAttachment = std::make_unique<SliderAttachment>(parameters, mixId, mixSlider);
    bypassAttachment = std::make_unique<ButtonAttachment>(parameters, bypassId, bypassButton);
    soloAttachment = std::make_unique<ButtonAttachment>(parameters, soloId, soloButton);
    qAmountAttachment = std::make_unique<SliderAttachment>(parameters, ParamIDs::qModeAmount, qAmountSlider);
}

void BandControlsPanel::syncQModeToggle()
{
    if (auto* value = parameters.getRawParameterValue(ParamIDs::qMode))
        qModeToggle.setToggleState(value->load() > 0.5f, juce::dontSendNotification);
    qAmountSlider.setEnabled(qModeToggle.getToggleState());
    qAmountSlider.setAlpha(qModeToggle.getToggleState() ? 1.0f : 0.4f);
}

void BandControlsPanel::resetSelectedBand(bool shouldBypass)
{
    auto resetParam = [this](const juce::String& suffix)
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, suffix)))
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

    if (auto* bypassParam = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "bypass")))
        bypassParam->setValueNotifyingHost(shouldBypass ? 1.0f : 0.0f);
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

    syncQModeToggle();

    // No dynamic EQ controls in this revision.
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
    state.mix = static_cast<float>(mixSlider.getValue());
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
}

void BandControlsPanel::mirrorToLinkedChannel(const juce::String& suffix, float value)
{
    juce::ignoreUnused(suffix, value);
}
