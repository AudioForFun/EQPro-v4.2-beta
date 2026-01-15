#include "ColorUtils.h"

namespace ColorUtils
{
juce::Colour bandColour(int index)
{
    static const juce::Colour palette[] {
        juce::Colour(0xff22d3ee),
        juce::Colour(0xffa78bfa),
        juce::Colour(0xff34d399),
        juce::Colour(0xfffb7185),
        juce::Colour(0xfffbbf24),
        juce::Colour(0xff60a5fa),
        juce::Colour(0xfff472b6),
        juce::Colour(0xff2dd4bf),
        juce::Colour(0xffc084fc),
        juce::Colour(0xfff59e0b),
        juce::Colour(0xff38bdf8),
        juce::Colour(0xfffb923c),
        juce::Colour(0xff4ade80),
        juce::Colour(0xff818cf8),
        juce::Colour(0xffe879f9),
        juce::Colour(0xfff87171),
        juce::Colour(0xffa3e635),
        juce::Colour(0xff22c55e),
        juce::Colour(0xff0ea5e9),
        juce::Colour(0xffd946ef),
        juce::Colour(0xfff97316),
        juce::Colour(0xff84cc16),
        juce::Colour(0xff14b8a6),
        juce::Colour(0xff60a5fa)
    };

    const int count = static_cast<int>(sizeof(palette) / sizeof(palette[0]));
    const int safeIndex = count > 0 ? (index % count + count) % count : 0;
    return palette[safeIndex];
}
} // namespace ColorUtils
