#include "PluginEditor.h"

namespace
{
constexpr int kEditorWidth = 360;
constexpr int kEditorHeight = 200;
} // namespace

EQPluginAudioProcessorEditor::EQPluginAudioProcessorEditor(EQPluginAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    gainLabel.setText("Gain (dB)", juce::dontSendNotification);
    gainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(gainLabel);

    gainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    addAndMakeVisible(gainSlider);

    gainAttachment = std::make_unique<Attachment>(processorRef.getParameters(), "gain", gainSlider);

    setSize(kEditorWidth, kEditorHeight);
}

EQPluginAudioProcessorEditor::~EQPluginAudioProcessorEditor() = default;

void EQPluginAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    g.drawFittedText("EQPlugin", getLocalBounds().removeFromTop(30),
                     juce::Justification::centred, 1);
}

void EQPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    gainLabel.setBounds(bounds.removeFromTop(30));
    gainSlider.setBounds(bounds.reduced(40, 10));
}
