#pragma once

#include <JuceHeader.h>
#include "ParamIDs.h"

namespace ChannelLayoutUtils
{
// Returns a list of channel labels for a given JUCE layout.
std::vector<juce::String> getChannelNames(const juce::AudioChannelSet& layout);
} // namespace ChannelLayoutUtils
