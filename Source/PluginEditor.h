#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class EQPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit EQPluginAudioProcessorEditor(EQPluginAudioProcessor&);
    ~EQPluginAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    EQPluginAudioProcessor& processorRef;

    juce::Slider gainSlider;
    juce::Label gainLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQPluginAudioProcessorEditor)
};
