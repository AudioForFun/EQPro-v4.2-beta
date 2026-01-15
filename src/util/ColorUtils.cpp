#include "ColorUtils.h"

namespace ColorUtils
{
juce::Colour bandColour(int index)
{
    static const juce::Colour palette[] {
        juce::Colour(0xff4fc3f7),
        juce::Colour(0xffff8a65),
        juce::Colour(0xff81c784),
        juce::Colour(0xffba68c8),
        juce::Colour(0xffffd54f),
        juce::Colour(0xff64b5f6),
        juce::Colour(0xffe57373),
        juce::Colour(0xffaed581),
        juce::Colour(0xff9575cd),
        juce::Colour(0xffffb74d),
        juce::Colour(0xff4db6ac),
        juce::Colour(0xfff06292)
    };

    const int count = static_cast<int>(sizeof(palette) / sizeof(palette[0]));
    const int safeIndex = juce::jlimit(0, count - 1, index);
    return palette[safeIndex];
}
} // namespace ColorUtils
