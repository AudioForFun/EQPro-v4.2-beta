#pragma once

#include <JuceHeader.h>
#include "ParamIDs.h"

namespace ChannelLayoutUtils
{
std::vector<juce::String> getChannelNames(const juce::AudioChannelSet& layout);
std::vector<std::pair<int, int>> getLinkPairs(const juce::AudioChannelSet& layout);
} // namespace ChannelLayoutUtils
