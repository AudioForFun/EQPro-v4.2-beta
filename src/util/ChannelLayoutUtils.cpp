#include "ChannelLayoutUtils.h"

namespace
{
juce::String labelForChannelType(juce::AudioChannelSet::ChannelType type)
{
    switch (type)
    {
        case juce::AudioChannelSet::left: return "L";
        case juce::AudioChannelSet::right: return "R";
        case juce::AudioChannelSet::centre: return "C";
        case juce::AudioChannelSet::LFE: return "LFE";
        case juce::AudioChannelSet::leftSurround: return "Ls";
        case juce::AudioChannelSet::rightSurround: return "Rs";
        case juce::AudioChannelSet::leftCentre: return "Lc";
        case juce::AudioChannelSet::rightCentre: return "Rc";
        case juce::AudioChannelSet::leftSurroundRear: return "Lrs";
        case juce::AudioChannelSet::rightSurroundRear: return "Rrs";
        case juce::AudioChannelSet::centreSurround: return "Cs";
        case juce::AudioChannelSet::topFrontLeft: return "Ltf";
        case juce::AudioChannelSet::topFrontRight: return "Rtf";
        case juce::AudioChannelSet::topFrontCentre: return "Tfc";
        case juce::AudioChannelSet::topMiddle: return "Tm";
        case juce::AudioChannelSet::topRearLeft: return "Ltr";
        case juce::AudioChannelSet::topRearRight: return "Rtr";
        case juce::AudioChannelSet::topRearCentre: return "Trc";
        case juce::AudioChannelSet::topSideLeft: return "Lts";
        case juce::AudioChannelSet::topSideRight: return "Rts";
        case juce::AudioChannelSet::wideLeft: return "Lw";
        case juce::AudioChannelSet::wideRight: return "Rw";
        case juce::AudioChannelSet::LFE2: return "LFE2";
        case juce::AudioChannelSet::bottomFrontLeft: return "Bfl";
        case juce::AudioChannelSet::bottomFrontRight: return "Bfr";
        case juce::AudioChannelSet::bottomFrontCentre: return "Bfc";
        case juce::AudioChannelSet::ambisonicW: return "W";
        case juce::AudioChannelSet::ambisonicX: return "X";
        case juce::AudioChannelSet::ambisonicY: return "Y";
        case juce::AudioChannelSet::ambisonicZ: return "Z";
        default: break;
    }

    return {};
}
} // namespace

namespace ChannelLayoutUtils
{
std::vector<juce::String> getChannelNames(const juce::AudioChannelSet& layout)
{
    std::vector<juce::String> names;
    const int total = layout.size();
    names.reserve(juce::jmin(total, ParamIDs::kMaxChannels));

    auto types = layout.getChannelTypes();
    if (types.isEmpty())
        types = juce::AudioChannelSet::canonicalChannelSet(total).getChannelTypes();

    for (int i = 0; i < total && i < ParamIDs::kMaxChannels; ++i)
    {
        const auto label = labelForChannelType(types[i]);
        if (label.isNotEmpty())
            names.push_back(label);
        else
            names.push_back("Ch " + juce::String(i + 1));
    }

    if (names.empty())
        names.push_back("Ch 1");

    return names;
}

std::vector<std::pair<int, int>> getLinkPairs(const juce::AudioChannelSet& layout)
{
    std::vector<std::pair<int, int>> pairs;
    auto types = layout.getChannelTypes();
    if (types.isEmpty())
        types = juce::AudioChannelSet::canonicalChannelSet(layout.size()).getChannelTypes();

    auto findIndex = [&types](juce::AudioChannelSet::ChannelType type) -> int
    {
        for (int i = 0; i < types.size(); ++i)
            if (types[i] == type)
                return i;
        return -1;
    };

    auto addPair = [&pairs, &findIndex](juce::AudioChannelSet::ChannelType left,
                                        juce::AudioChannelSet::ChannelType right)
    {
        const int l = findIndex(left);
        const int r = findIndex(right);
        if (l >= 0 && r >= 0)
            pairs.emplace_back(l, r);
    };

    addPair(juce::AudioChannelSet::left, juce::AudioChannelSet::right);
    addPair(juce::AudioChannelSet::leftCentre, juce::AudioChannelSet::rightCentre);
    addPair(juce::AudioChannelSet::leftSurround, juce::AudioChannelSet::rightSurround);
    addPair(juce::AudioChannelSet::leftSurroundRear, juce::AudioChannelSet::rightSurroundRear);
    addPair(juce::AudioChannelSet::wideLeft, juce::AudioChannelSet::wideRight);
    addPair(juce::AudioChannelSet::topFrontLeft, juce::AudioChannelSet::topFrontRight);
    addPair(juce::AudioChannelSet::topRearLeft, juce::AudioChannelSet::topRearRight);
    addPair(juce::AudioChannelSet::topSideLeft, juce::AudioChannelSet::topSideRight);
    addPair(juce::AudioChannelSet::bottomFrontLeft, juce::AudioChannelSet::bottomFrontRight);

    return pairs;
}
} // namespace ChannelLayoutUtils
