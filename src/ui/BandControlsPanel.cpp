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

} // namespace

BandControlsPanel::BandControlsPanel(EQProAudioProcessor& processorIn)
    : processor(processorIn),
      parameters(processorIn.getParameters())
{
    startTimerHz(30);

    titleLabel.setText("Band 1 / " + juce::String(ParamIDs::kBandsPerChannel), juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, theme.text);
    dynamicLabel.setColour(juce::Label::textColourId, theme.accent);
    addAndMakeVisible(titleLabel);

    copyButton.setButtonText("Copy");
    copyButton.onClick = [this] { copyBandState(); };
    addAndMakeVisible(copyButton);

    pasteButton.setButtonText("Paste");
    pasteButton.onClick = [this] { pasteBandState(); };
    addAndMakeVisible(pasteButton);

    defaultButton.setButtonText("Reset");
    defaultButton.onClick = [this] { resetSelectedBand(false); };
    addAndMakeVisible(defaultButton);

    deleteButton.setButtonText("Delete");
    deleteButton.onClick = [this] { resetSelectedBand(true); };
    addAndMakeVisible(deleteButton);

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
    dynamicLabel.setText("DYNAMIC / CHANNEL", juce::dontSendNotification);
    dynamicLabel.setJustificationType(juce::Justification::centredLeft);
    dynamicLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    dynamicLabel.setColour(juce::Label::textColourId, theme.accent);
    addAndMakeVisible(dynamicLabel);
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
        mirrorToLinkedChannel("freq", static_cast<float>(freqSlider.getValue()));
    };
    addAndMakeVisible(freqSlider);

    gainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    gainSlider.setTextBoxIsEditable(true);
    gainSlider.setTextValueSuffix(" dB");
    gainSlider.onValueChange = [this]
    {
        mirrorToLinkedChannel("gain", static_cast<float>(gainSlider.getValue()));
    };
    addAndMakeVisible(gainSlider);

    qSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    qSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    qSlider.setTextBoxIsEditable(true);
    qSlider.onValueChange = [this]
    {
        mirrorToLinkedChannel("q", static_cast<float>(qSlider.getValue()));
    };
    addAndMakeVisible(qSlider);

    for (size_t i = 0; i < filterButtons.size(); ++i)
    {
        filterButtons[i].setButtonText(kFilterTypeChoices[static_cast<int>(i)]);
        filterButtons[i].setClickingTogglesState(true);
        filterButtons[i].onClick = [this, index = static_cast<int>(i)]()
        {
            if (! filterButtons[static_cast<size_t>(index)].getToggleState())
            {
                updateFilterButtonsFromType(getCurrentTypeIndex());
                return;
            }
            if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "type")))
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(index)));
            updateFilterButtonsFromType(index);
            updateTypeUi();
            mirrorToLinkedChannel("type", static_cast<float>(index));
        };
        addAndMakeVisible(filterButtons[i]);
    }

    msBox.addItemList(kMsChoices, 1);
    msBox.setColour(juce::ComboBox::backgroundColourId, theme.panel);
    msBox.setColour(juce::ComboBox::textColourId, theme.text);
    msBox.setColour(juce::ComboBox::outlineColourId, theme.panelOutline);
    addAndMakeVisible(msBox);
    msBox.onChange = [this]
    {
        mirrorToLinkedChannel("ms", static_cast<float>(msBox.getSelectedItemIndex()));
    };

    for (int i = 0; i < static_cast<int>(slopeButtons.size()); ++i)
    {
        const int slopeValue = 6 * (i + 1);
        auto& button = slopeButtons[static_cast<size_t>(i)];
        button.setButtonText(juce::String(slopeValue));
        button.setClickingTogglesState(true);
        button.onClick = [this, slopeValue, index = i]()
        {
            if (! slopeButtons[static_cast<size_t>(index)].getToggleState())
                return;
            if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "slope")))
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(slopeValue)));
            mirrorToLinkedChannel("slope", static_cast<float>(slopeValue));
            for (int j = 0; j < static_cast<int>(slopeButtons.size()); ++j)
                slopeButtons[static_cast<size_t>(j)].setToggleState(j == index, juce::dontSendNotification);
        };
        addAndMakeVisible(button);
    }

    mixSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    mixSlider.setTextBoxIsEditable(true);
    mixSlider.setTextValueSuffix(" %");
    addAndMakeVisible(mixSlider);
    mixSlider.onValueChange = [this]
    {
        mirrorToLinkedChannel("mix", static_cast<float>(mixSlider.getValue()));
    };

    dynEnableToggle.setButtonText("Dynamic");
    dynEnableToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    addAndMakeVisible(dynEnableToggle);

    dynUpButton.setButtonText("UP");
    dynDownButton.setButtonText("DOWN");
    dynUpButton.setClickingTogglesState(true);
    dynDownButton.setClickingTogglesState(true);
    dynUpButton.onClick = [this]()
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "dynMode")))
            param->setValueNotifyingHost(param->convertTo0to1(0.0f));
        dynUpButton.setToggleState(true, juce::dontSendNotification);
        dynDownButton.setToggleState(false, juce::dontSendNotification);
    };
    dynDownButton.onClick = [this]()
    {
        if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "dynMode")))
            param->setValueNotifyingHost(param->convertTo0to1(1.0f));
        dynUpButton.setToggleState(false, juce::dontSendNotification);
        dynDownButton.setToggleState(true, juce::dontSendNotification);
    };
    addAndMakeVisible(dynUpButton);
    addAndMakeVisible(dynDownButton);

    thresholdSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    thresholdSlider.setTextBoxIsEditable(true);
    thresholdSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(thresholdSlider);

    attackSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    attackSlider.setTextBoxIsEditable(true);
    attackSlider.setTextValueSuffix(" ms");
    addAndMakeVisible(attackSlider);

    releaseSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobTextW, knobTextH);
    releaseSlider.setTextBoxIsEditable(true);
    releaseSlider.setTextValueSuffix(" ms");
    addAndMakeVisible(releaseSlider);

    autoScaleToggle.setButtonText("Auto Scale");
    autoScaleToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    addAndMakeVisible(autoScaleToggle);

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

    tiltDirToggle.setButtonText("High Boost");
    tiltDirToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    tiltDirToggle.onClick = [this]
    {
        if (! tiltDirToggle.isVisible())
            return;
        auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "gain"));
        if (param == nullptr)
            return;
        const float current = param->getValue();
        const float gain = param->convertFrom0to1(current);
        const float targetGain = tiltDirToggle.getToggleState()
            ? (gain > 0.0f ? -gain : (gain == 0.0f ? -1.0f : gain))
            : (gain < 0.0f ? -gain : gain);
        param->setValueNotifyingHost(param->convertTo0to1(targetGain));
    };
    addAndMakeVisible(tiltDirToggle);

    updateAttachments();
    updateFilterButtonsFromType(getCurrentTypeIndex());
    updateTypeUi();
}

void BandControlsPanel::setSelectedBand(int channelIndex, int bandIndex)
{
    selectedChannel = juce::jlimit(0, ParamIDs::kMaxChannels - 1, channelIndex);
    selectedBand = juce::jlimit(0, ParamIDs::kBandsPerChannel - 1, bandIndex);

    titleLabel.setText("Band " + juce::String(selectedBand + 1)
                           + " / " + juce::String(ParamIDs::kBandsPerChannel),
                       juce::dontSendNotification);
    for (int i = 0; i < static_cast<int>(bandSelectButtons.size()); ++i)
        bandSelectButtons[static_cast<size_t>(i)].setToggleState(i == selectedBand, juce::dontSendNotification);
    const auto bandColour = ColorUtils::bandColour(selectedBand);
    titleLabel.setColour(juce::Label::textColourId, bandColour);
    freqSlider.setColour(juce::Slider::trackColourId, bandColour);
    gainSlider.setColour(juce::Slider::trackColourId, bandColour);
    qSlider.setColour(juce::Slider::trackColourId, bandColour);
    mixSlider.setColour(juce::Slider::trackColourId, bandColour);
    thresholdSlider.setColour(juce::Slider::trackColourId, bandColour);
    attackSlider.setColour(juce::Slider::trackColourId, bandColour);
    releaseSlider.setColour(juce::Slider::trackColourId, bandColour);
    updateAttachments();
    updateTypeUi();
}

void BandControlsPanel::setTheme(const ThemeColors& newTheme)
{
    theme = newTheme;
    titleLabel.setColour(juce::Label::textColourId, ColorUtils::bandColour(selectedBand));
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
    for (auto& button : slopeButtons)
        button.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    mixSlider.setColour(juce::Slider::trackColourId, ColorUtils::bandColour(selectedBand));
    mixSlider.setColour(juce::Slider::textBoxTextColourId, theme.text);
    mixSlider.setColour(juce::Slider::textBoxOutlineColourId, theme.panelOutline);
    thresholdSlider.setColour(juce::Slider::trackColourId, ColorUtils::bandColour(selectedBand));
    thresholdSlider.setColour(juce::Slider::textBoxTextColourId, theme.text);
    thresholdSlider.setColour(juce::Slider::textBoxOutlineColourId, theme.panelOutline);
    attackSlider.setColour(juce::Slider::trackColourId, ColorUtils::bandColour(selectedBand));
    attackSlider.setColour(juce::Slider::textBoxTextColourId, theme.text);
    attackSlider.setColour(juce::Slider::textBoxOutlineColourId, theme.panelOutline);
    releaseSlider.setColour(juce::Slider::trackColourId, ColorUtils::bandColour(selectedBand));
    releaseSlider.setColour(juce::Slider::textBoxTextColourId, theme.text);
    releaseSlider.setColour(juce::Slider::textBoxOutlineColourId, theme.panelOutline);
    bypassButton.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    copyButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    pasteButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    deleteButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    defaultButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    for (auto& button : bandSelectButtons)
        button.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    soloButton.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    tiltDirToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    dynEnableToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
    dynUpButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    dynDownButton.setColour(juce::TextButton::textColourOffId, theme.textMuted);
    autoScaleToggle.setColour(juce::ToggleButton::textColourId, theme.textMuted);
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

    if (detectorMeterBounds.getWidth() > 1.0f && detectorMeterBounds.getHeight() > 1.0f)
    {
        const auto meter = detectorMeterBounds;
        g.setColour(theme.panelOutline.withAlpha(0.6f));
        g.fillRoundedRectangle(meter, 4.0f);

        const float clampedDb = juce::jlimit(-60.0f, 0.0f, detectorDb);
        const float norm = (clampedDb + 60.0f) / 60.0f;
        auto fill = meter;
        fill.setY(meter.getBottom() - meter.getHeight() * norm);
        fill.setHeight(meter.getBottom() - fill.getY());
        g.setColour(ColorUtils::bandColour(selectedBand));
        g.fillRoundedRectangle(fill.reduced(2.0f), 3.0f);

        const float thresh = static_cast<float>(thresholdSlider.getValue());
        const float threshNorm = (juce::jlimit(-60.0f, 0.0f, thresh) + 60.0f) / 60.0f;
        const float y = meter.getBottom() - meter.getHeight() * threshNorm;
        g.setColour(theme.textMuted);
        g.drawLine(meter.getX(), y, meter.getRight(), y, 1.2f);
    }

}

void BandControlsPanel::timerCallback()
{
    detectorDb = processor.getBandDetectorDb(selectedChannel, selectedBand);
    repaint(detectorMeterBounds.getSmallestIntegerContainer());
}

void BandControlsPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    const int gap = 6;
    const int labelHeight = 16;
    const int rowHeight = 22;
    const int knobRowHeight = 120;
    const int knobSize = 90;

    auto left = bounds.removeFromLeft(static_cast<int>(bounds.getWidth() * 0.70f));
    auto right = bounds;

    auto headerRow = left.removeFromTop(rowHeight);
    titleLabel.setBounds(headerRow.removeFromLeft(90));
    const int btnW = 58;
    copyButton.setBounds(headerRow.removeFromLeft(btnW));
    pasteButton.setBounds(headerRow.removeFromLeft(btnW));
    defaultButton.setBounds(headerRow.removeFromLeft(btnW));
    deleteButton.setBounds(headerRow.removeFromLeft(btnW));

    left.removeFromTop(2);
    auto bandRow = left.removeFromTop(rowHeight);
    const int bandBtnWidth = std::max(18, (bandRow.getWidth() - gap * 11) / 12);
    for (int i = 0; i < static_cast<int>(bandSelectButtons.size()); ++i)
    {
        bandSelectButtons[static_cast<size_t>(i)].setBounds(
            bandRow.removeFromLeft(bandBtnWidth));
        bandRow.removeFromLeft(gap);
    }

    left.removeFromTop(gap);
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
    auto typeRow = left.removeFromTop(labelHeight + rowHeight * 2 + 4);
    typeLabel.setBounds(typeRow.removeFromTop(labelHeight));
    auto typeButtonsArea = typeRow;
    const int perRow = 5;
    const int buttonGap = 6;
    const int buttonWidth = (typeButtonsArea.getWidth() - buttonGap * (perRow - 1)) / perRow;
    auto row1 = typeButtonsArea.removeFromTop(rowHeight);
    auto row2 = typeButtonsArea.removeFromTop(rowHeight);
    for (int i = 0; i < static_cast<int>(filterButtons.size()); ++i)
    {
        auto& row = (i < perRow) ? row1 : row2;
        const int col = i % perRow;
        const int x = row.getX() + col * (buttonWidth + buttonGap);
        filterButtons[static_cast<size_t>(i)].setBounds(x, row.getY(), buttonWidth, rowHeight);
    }

    left.removeFromTop(2);
    auto togglesRow = left.removeFromTop(rowHeight);
    const int toggleW = 74;
    bypassButton.setBounds(togglesRow.removeFromLeft(toggleW));
    soloButton.setBounds(togglesRow.removeFromLeft(toggleW));
    tiltDirToggle.setBounds(togglesRow.removeFromLeft(120));

    // Dynamic panel (right column)
    auto dynHeader = right.removeFromTop(labelHeight);
    dynamicLabel.setBounds(dynHeader);
    auto dynRow = right.removeFromTop(rowHeight);
    dynEnableToggle.setBounds(dynRow.removeFromLeft(90));
    dynUpButton.setBounds(dynRow.removeFromLeft(50));
    dynDownButton.setBounds(dynRow.removeFromLeft(60));
    autoScaleToggle.setBounds(dynRow);

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

    right.removeFromTop(6);
    detectorMeterBounds = right.removeFromTop(64).toFloat();

    right.removeFromTop(6);
    auto msRow = right.removeFromTop(labelHeight + rowHeight);
    msLabel.setBounds(msRow.removeFromTop(labelHeight));
    msBox.setBounds(msRow.withHeight(rowHeight));

    left.removeFromTop(2);
    auto slopeRow = left.removeFromTop(labelHeight + rowHeight * 2 + 4);
    slopeLabel.setBounds(slopeRow.removeFromTop(labelHeight));
    const int perRowSlope = 8;
    const int slopeGap = 6;
    const int slopeButtonWidth = (slopeRow.getWidth() - slopeGap * (perRowSlope - 1)) / perRowSlope;
    auto slopeRow1 = slopeRow.removeFromTop(rowHeight);
    auto slopeRow2 = slopeRow.removeFromTop(rowHeight);
    for (int i = 0; i < static_cast<int>(slopeButtons.size()); ++i)
    {
        auto& row = (i < perRowSlope) ? slopeRow1 : slopeRow2;
        const int col = i % perRowSlope;
        const int x = row.getX() + col * (slopeButtonWidth + slopeGap);
        slopeButtons[static_cast<size_t>(i)].setBounds(x, row.getY(), slopeButtonWidth, rowHeight);
    }

}

void BandControlsPanel::updateAttachments()
{
    const auto freqId = ParamIDs::bandParamId(selectedChannel, selectedBand, "freq");
    const auto gainId = ParamIDs::bandParamId(selectedChannel, selectedBand, "gain");
    const auto qId = ParamIDs::bandParamId(selectedChannel, selectedBand, "q");
    const auto bypassId = ParamIDs::bandParamId(selectedChannel, selectedBand, "bypass");
    const auto msId = ParamIDs::bandParamId(selectedChannel, selectedBand, "ms");
    const auto slopeId = ParamIDs::bandParamId(selectedChannel, selectedBand, "slope");
    const auto soloId = ParamIDs::bandParamId(selectedChannel, selectedBand, "solo");
    const auto mixId = ParamIDs::bandParamId(selectedChannel, selectedBand, "mix");

    freqAttachment = std::make_unique<SliderAttachment>(parameters, freqId, freqSlider);
    gainAttachment = std::make_unique<SliderAttachment>(parameters, gainId, gainSlider);
    qAttachment = std::make_unique<SliderAttachment>(parameters, qId, qSlider);
    msAttachment = std::make_unique<ComboBoxAttachment>(parameters, msId, msBox);
    mixAttachment = std::make_unique<SliderAttachment>(parameters, mixId, mixSlider);
    bypassAttachment = std::make_unique<ButtonAttachment>(parameters, bypassId, bypassButton);
    soloAttachment = std::make_unique<ButtonAttachment>(parameters, soloId, soloButton);
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

    if (auto* param = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "dynMode")))
    {
        const int mode = static_cast<int>(param->convertFrom0to1(param->getValue()));
        dynUpButton.setToggleState(mode == 0, juce::dontSendNotification);
        dynDownButton.setToggleState(mode == 1, juce::dontSendNotification);
    }

    if (auto* slopeParam = parameters.getParameter(slopeId))
    {
        const float slopeValue = slopeParam->convertFrom0to1(slopeParam->getValue());
        const int slopeIndex = juce::jlimit(0, static_cast<int>(slopeButtons.size()) - 1,
                                            static_cast<int>(std::round(slopeValue / 6.0f)) - 1);
        for (int i = 0; i < static_cast<int>(slopeButtons.size()); ++i)
            slopeButtons[static_cast<size_t>(i)].setToggleState(i == slopeIndex, juce::dontSendNotification);
    }
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
    resetParam("dynEnable");
    resetParam("dynMode");
    resetParam("dynThresh");
    resetParam("dynAttack");
    resetParam("dynRelease");
    resetParam("dynAuto");

    if (auto* bypassParam = parameters.getParameter(ParamIDs::bandParamId(selectedChannel, selectedBand, "bypass")))
        bypassParam->setValueNotifyingHost(shouldBypass ? 1.0f : 0.0f);
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
    for (auto& button : slopeButtons)
    {
        button.setEnabled(isHpLp);
        button.setAlpha(isHpLp ? 1.0f : 0.5f);
    }
    updateFilterButtonsFromType(typeIndex);
    const bool isTilt = (typeIndex == 8 || typeIndex == 9);
    tiltDirToggle.setVisible(isTilt);
    if (isTilt)
    {
        const auto gainId = ParamIDs::bandParamId(selectedChannel, selectedBand, "gain");
        if (auto* param = parameters.getParameter(gainId))
        {
            const float gain = param->convertFrom0to1(param->getValue());
            tiltDirToggle.setToggleState(gain < 0.0f, juce::dontSendNotification);
        }
    }
    dynEnableToggle.setVisible(true);
    dynUpButton.setVisible(true);
    dynDownButton.setVisible(true);
    thresholdSlider.setVisible(true);
    attackSlider.setVisible(true);
    releaseSlider.setVisible(true);
    autoScaleToggle.setVisible(true);
}

int BandControlsPanel::getCurrentTypeIndex() const
{
    const auto typeId = ParamIDs::bandParamId(selectedChannel, selectedBand, "type");
    if (auto* param = parameters.getParameter(typeId))
        return static_cast<int>(param->convertFrom0to1(param->getValue()));
    return 0;
}

void BandControlsPanel::updateFilterButtonsFromType(int typeIndex)
{
    for (size_t i = 0; i < filterButtons.size(); ++i)
        filterButtons[i].setToggleState(static_cast<int>(i) == typeIndex, juce::dontSendNotification);
}

void BandControlsPanel::copyBandState()
{
    BandState state;
    state.freq = static_cast<float>(freqSlider.getValue());
    state.gain = static_cast<float>(gainSlider.getValue());
    state.q = static_cast<float>(qSlider.getValue());
    state.type = static_cast<float>(getCurrentTypeIndex());
    state.bypass = bypassButton.getToggleState() ? 1.0f : 0.0f;
    state.ms = static_cast<float>(msBox.getSelectedItemIndex());
    const auto slopeId = ParamIDs::bandParamId(selectedChannel, selectedBand, "slope");
    if (auto* param = parameters.getParameter(slopeId))
        state.slope = param->convertFrom0to1(param->getValue());
    state.solo = soloButton.getToggleState() ? 1.0f : 0.0f;
    state.mix = static_cast<float>(mixSlider.getValue());
    state.dynEnable = dynEnableToggle.getToggleState() ? 1.0f : 0.0f;
    state.dynMode = dynUpButton.getToggleState() ? 0.0f : 1.0f;
    state.dynThresh = static_cast<float>(thresholdSlider.getValue());
    state.dynAttack = static_cast<float>(attackSlider.getValue());
    state.dynRelease = static_cast<float>(releaseSlider.getValue());
    state.dynAuto = autoScaleToggle.getToggleState() ? 1.0f : 0.0f;
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
