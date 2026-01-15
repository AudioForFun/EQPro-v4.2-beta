#include "ParamIDs.h"

namespace ParamIDs
{
const juce::String globalBypass = "globalBypass";
const juce::String ellipticBypass = "ellipticBypass";
const juce::String ellipticFreq = "ellipticFreq";
const juce::String ellipticAmount = "ellipticAmount";
const juce::String phaseMode = "phaseMode";
const juce::String linearQuality = "linearQuality";
const juce::String linearWindow = "linearWindow";
const juce::String oversampling = "oversampling";
const juce::String outputTrim = "outputTrim";
const juce::String spectralEnable = "spectralEnable";
const juce::String spectralThreshold = "spectralThreshold";
const juce::String spectralRatio = "spectralRatio";
const juce::String spectralAttack = "spectralAttack";
const juce::String spectralRelease = "spectralRelease";
const juce::String spectralMix = "spectralMix";
const juce::String characterMode = "characterMode";
const juce::String qMode = "qMode";
const juce::String qModeAmount = "qModeAmount";
const juce::String analyzerRange = "analyzerRange";
const juce::String analyzerSpeed = "analyzerSpeed";
const juce::String analyzerView = "analyzerView";
const juce::String analyzerFreeze = "analyzerFreeze";
const juce::String analyzerExternal = "analyzerExternal";
const juce::String autoGainEnable = "autoGainEnable";
const juce::String gainScale = "gainScale";
const juce::String phaseInvert = "phaseInvert";
const juce::String midiLearn = "midiLearn";
const juce::String midiTarget = "midiTarget";
const juce::String smartSolo = "smartSolo";

juce::String bandParamId(int channelIndex, int bandIndex, juce::StringRef suffix)
{
    return "ch" + juce::String(channelIndex + 1)
        + "_b" + juce::String(bandIndex + 1)
        + "_" + suffix;
}

juce::String bandParamName(int channelIndex, int bandIndex, juce::StringRef name)
{
    return "Ch " + juce::String(channelIndex + 1)
        + " Band " + juce::String(bandIndex + 1)
        + " " + name;
}
} // namespace ParamIDs
