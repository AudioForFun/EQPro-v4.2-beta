#pragma once

#include <JuceHeader.h>

namespace Version
{
// Version string baked into the build.
inline juce::String versionString()
{
#if defined(EQPRO_VERSION)
    return EQPRO_VERSION;
#else
    return "0.0.0";
#endif
}

// Human-readable iteration label.
inline juce::String iterationString()
{
#if defined(EQPRO_ITERATION)
    return "Iter " + juce::String(EQPRO_ITERATION);
#else
    return "Iter 0";
#endif
}

// Display string shown in the UI.
inline juce::String displayString()
{
    juce::String stamp;
#if defined(EQPRO_BUILD_STAMP)
    stamp = " (" + juce::String(EQPRO_BUILD_STAMP) + ")";
#endif
    return "v" + versionString() + " - " + iterationString() + stamp;
}
} // namespace Version
