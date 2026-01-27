#pragma once

#include <JuceHeader.h>

namespace ParamIDs
{
// Maximum addressable channels and bands per channel.
constexpr int kMaxChannels = 16;
constexpr int kBandsPerChannel = 12;

// Global parameter IDs (APVTS).
extern const juce::String globalBypass;
extern const juce::String globalMix;
extern const juce::String phaseMode;
extern const juce::String linearQuality;
extern const juce::String linearWindow;
extern const juce::String oversampling;
extern const juce::String outputTrim;
extern const juce::String spectralEnable;
extern const juce::String spectralThreshold;
extern const juce::String spectralRatio;
extern const juce::String spectralAttack;
extern const juce::String spectralRelease;
extern const juce::String spectralMix;
extern const juce::String characterMode;
extern const juce::String qMode;
extern const juce::String qModeAmount;
extern const juce::String analyzerRange;
extern const juce::String analyzerSpeed;
extern const juce::String analyzerView;
extern const juce::String analyzerFreeze;
extern const juce::String analyzerExternal;
extern const juce::String autoGainEnable;
extern const juce::String gainScale;
extern const juce::String phaseInvert;
extern const juce::String midiLearn;
extern const juce::String midiTarget;
extern const juce::String smartSolo;

// Helper to create a full band parameter ID.
juce::String bandParamId(int channelIndex, int bandIndex, juce::StringRef suffix);
// Helper to create a readable band parameter name.
juce::String bandParamName(int channelIndex, int bandIndex, juce::StringRef name);
} // namespace ParamIDs
