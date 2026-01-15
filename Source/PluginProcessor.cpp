#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr const char* kParamGainId = "gain";
constexpr const char* kParamGainName = "Gain";
} // namespace

EQPluginAudioProcessor::EQPluginAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

EQPluginAudioProcessor::~EQPluginAudioProcessor() = default;

void EQPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void EQPluginAudioProcessor::releaseResources()
{
}

bool EQPluginAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainInput = layouts.getChannelSet(true, 0);
    const auto mainOutput = layouts.getChannelSet(false, 0);

    if (mainInput.isDisabled() || mainOutput.isDisabled())
        return false;

    if (mainInput != mainOutput)
        return false;

    return mainInput == juce::AudioChannelSet::mono()
        || mainInput == juce::AudioChannelSet::stereo();
}

void EQPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const auto gain = parameters.getRawParameterValue(kParamGainId)->load();
    const auto gainLinear = juce::Decibels::decibelsToGain(gain);

    buffer.applyGain(gainLinear);
}

juce::AudioProcessorEditor* EQPluginAudioProcessor::createEditor()
{
    return new EQPluginAudioProcessorEditor(*this);
}

bool EQPluginAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String EQPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EQPluginAudioProcessor::acceptsMidi() const
{
    return false;
}

bool EQPluginAudioProcessor::producesMidi() const
{
    return false;
}

bool EQPluginAudioProcessor::isMidiEffect() const
{
    return false;
}

double EQPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EQPluginAudioProcessor::getNumPrograms()
{
    return 1;
}

int EQPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EQPluginAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String EQPluginAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void EQPluginAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void EQPluginAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void EQPluginAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorValueTreeState& EQPluginAudioProcessor::getParameters()
{
    return parameters;
}

juce::AudioProcessorValueTreeState::ParameterLayout EQPluginAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        kParamGainId,
        kParamGainName,
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQPluginAudioProcessor();
}
