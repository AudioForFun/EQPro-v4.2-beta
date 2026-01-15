#pragma once

#include <JuceHeader.h>

namespace Version
{
inline juce::String versionString()
{
#if defined(EQPRO_VERSION)
    return EQPRO_VERSION;
#else
    return "0.0.0";
#endif
}

inline juce::String iterationString()
{
#if defined(EQPRO_ITERATION)
    return "Iter " + juce::String(EQPRO_ITERATION);
#else
    return "Iter 0";
#endif
}

inline juce::String displayString()
{
    return "v" + versionString() + " - " + iterationString();
}
} // namespace Version
