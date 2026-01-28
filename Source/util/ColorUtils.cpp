#include "ColorUtils.h"

namespace ColorUtils
{
juce::Colour bandColour(int index)
{
    static const juce::Colour palette[] {
        juce::Colour(0xff00d1ff),
        juce::Colour(0xffb45bff),
        juce::Colour(0xff3ddc84),
        juce::Colour(0xffff6b6b),
        juce::Colour(0xffffd166),
        juce::Colour(0xff4d96ff),
        juce::Colour(0xffff8fab),
        juce::Colour(0xff2ec4b6),
        juce::Colour(0xffc77dff),
        juce::Colour(0xffff9f1c),
        juce::Colour(0xffa7f432),
        juce::Colour(0xfff9844a),
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
