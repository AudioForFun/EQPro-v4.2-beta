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
        case juce::AudioChannelSet::leftSurroundSide: return "Ls";
        case juce::AudioChannelSet::rightSurroundSide: return "Rs";
        case juce::AudioChannelSet::leftCentre: return "Lc";
        case juce::AudioChannelSet::rightCentre: return "Rc";
        case juce::AudioChannelSet::leftSurroundRear: return "Lrs";
        case juce::AudioChannelSet::rightSurroundRear: return "Rrs";
        case juce::AudioChannelSet::centreSurround: return "Cs";
        case juce::AudioChannelSet::topFrontLeft: return "TFL";
        case juce::AudioChannelSet::topFrontRight: return "TFR";
        case juce::AudioChannelSet::topFrontCentre: return "TFC";
        case juce::AudioChannelSet::topMiddle: return "TM";
        case juce::AudioChannelSet::topRearLeft: return "TRL";
        case juce::AudioChannelSet::topRearRight: return "TRR";
        case juce::AudioChannelSet::topRearCentre: return "TRC";
        case juce::AudioChannelSet::topSideLeft: return "TML";
        case juce::AudioChannelSet::topSideRight: return "TMR";
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

using ChannelType = juce::AudioChannelSet::ChannelType;

bool matchesOrderedTypes(const juce::AudioChannelSet& layout, std::initializer_list<ChannelType> order)
{
    auto types = layout.getChannelTypes();
    if (types.isEmpty())
        types = juce::AudioChannelSet::canonicalChannelSet(layout.size()).getChannelTypes();

    if (types.size() != static_cast<int>(order.size()))
        return false;

    int index = 0;
    for (auto expected : order)
    {
        if (types[index++] != expected)
            return false;
    }

    return true;
}

std::vector<juce::String> buildNames(std::initializer_list<ChannelType> order,
                                     std::initializer_list<juce::String> labels = {})
{
    std::vector<juce::String> names;
    names.reserve(order.size());

    if (labels.size() == order.size())
    {
        for (const auto& label : labels)
            names.push_back(label);
        return names;
    }

    int index = 0;
    for (auto type : order)
    {
        auto label = labelForChannelType(type);
        if (label.isEmpty())
            label = "Ch " + juce::String(++index);
        else
            ++index;
        names.push_back(label);
    }

    return names;
}
} // namespace

namespace ChannelLayoutUtils
{
std::vector<juce::String> getChannelNames(const juce::AudioChannelSet& layout)
{
    std::vector<juce::String> names;
    const int total = layout.size();
    names.reserve(juce::jmin(total, ParamIDs::kMaxChannels));

    const auto mono = std::initializer_list<ChannelType> { juce::AudioChannelSet::centre };
    if (matchesOrderedTypes(layout, mono))
        return { "M" };
    if (matchesOrderedTypes(layout, { juce::AudioChannelSet::left })
        || matchesOrderedTypes(layout, { juce::AudioChannelSet::right }))
    {
        return { "M" };
    }

    const auto stereo = std::initializer_list<ChannelType>
        { juce::AudioChannelSet::left, juce::AudioChannelSet::right };
    if (matchesOrderedTypes(layout, stereo))
        return buildNames(stereo);

    const auto stereoLfe = std::initializer_list<ChannelType>
        { juce::AudioChannelSet::left, juce::AudioChannelSet::right, juce::AudioChannelSet::LFE };
    if (matchesOrderedTypes(layout, stereoLfe))
        return buildNames(stereoLfe);

    const auto threeZero = std::initializer_list<ChannelType>
        { juce::AudioChannelSet::left, juce::AudioChannelSet::right, juce::AudioChannelSet::centre };
    if (matchesOrderedTypes(layout, threeZero))
        return buildNames(threeZero);

    const auto threeOne = std::initializer_list<ChannelType>
        { juce::AudioChannelSet::left, juce::AudioChannelSet::right, juce::AudioChannelSet::centre,
          juce::AudioChannelSet::LFE };
    if (matchesOrderedTypes(layout, threeOne))
        return buildNames(threeOne);

    const auto quadMusic = std::initializer_list<ChannelType>
        { juce::AudioChannelSet::left, juce::AudioChannelSet::right,
          juce::AudioChannelSet::leftSurround, juce::AudioChannelSet::rightSurround };
    if (matchesOrderedTypes(layout, quadMusic))
        return buildNames(quadMusic);

    const auto quadMusicSide = std::initializer_list<ChannelType>
        { juce::AudioChannelSet::left, juce::AudioChannelSet::right,
          juce::AudioChannelSet::leftSurroundSide, juce::AudioChannelSet::rightSurroundSide };
    if (matchesOrderedTypes(layout, quadMusicSide))
        return buildNames(quadMusicSide);

    const auto quadLfe = std::initializer_list<ChannelType>
        { juce::AudioChannelSet::left, juce::AudioChannelSet::right, juce::AudioChannelSet::LFE,
          juce::AudioChannelSet::leftSurround, juce::AudioChannelSet::rightSurround };
    if (matchesOrderedTypes(layout, quadLfe))
        return buildNames(quadLfe);

    const auto quadLfeSide = std::initializer_list<ChannelType>
        { juce::AudioChannelSet::left, juce::AudioChannelSet::right, juce::AudioChannelSet::LFE,
          juce::AudioChannelSet::leftSurroundSide, juce::AudioChannelSet::rightSurroundSide };
    if (matchesOrderedTypes(layout, quadLfeSide))
        return buildNames(quadLfeSide);

    const auto fiveZeroFilm = std::initializer_list<ChannelType>
        { juce::AudioChannelSet::left, juce::AudioChannelSet::right, juce::AudioChannelSet::centre,
          juce::AudioChannelSet::leftSurround, juce::AudioChannelSet::rightSurround };
    if (matchesOrderedTypes(layout, fiveZeroFilm))
        return buildNames(fiveZeroFilm);

    const auto fiveZeroMusic = std::initializer_list<ChannelType>
        { juce::AudioChannelSet::left, juce::AudioChannelSet::right,
          juce::AudioChannelSet::leftSurround, juce::AudioChannelSet::rightSurround,
          juce::AudioChannelSet::centre };
    if (matchesOrderedTypes(layout, fiveZeroMusic))
        return buildNames(fiveZeroMusic);

    const auto fiveOneFilm = std::initializer_list<ChannelType>
        { juce::AudioChannelSet::left, juce::AudioChannelSet::right, juce::AudioChannelSet::centre,
          juce::AudioChannelSet::LFE, juce::AudioChannelSet::leftSurround,
          juce::AudioChannelSet::rightSurround };
    if (matchesOrderedTypes(layout, fiveOneFilm))
        return buildNames(fiveOneFilm);

    const auto fiveOneMusic = std::initializer_list<ChannelType>
        { juce::AudioChannelSet::left, juce::AudioChannelSet::right,
          juce::AudioChannelSet::leftSurround, juce::AudioChannelSet::rightSurround,
          juce::AudioChannelSet::centre, juce::AudioChannelSet::LFE };
    if (matchesOrderedTypes(layout, fiveOneMusic))
        return buildNames(fiveOneMusic);

    const auto sixZeroFilm = std::initializer_list<ChannelType>
        { juce::AudioChannelSet::left, juce::AudioChannelSet::right, juce::AudioChannelSet::centre,
          juce::AudioChannelSet::leftSurround, juce::AudioChannelSet::rightSurround,
          juce::AudioChannelSet::centreSurround };
    if (matchesOrderedTypes(layout, sixZeroFilm))
        return buildNames(sixZeroFilm);

    const auto sixOneFilm = std::initializer_list<ChannelType>
        { juce::AudioChannelSet::left, juce::AudioChannelSet::right, juce::AudioChannelSet::centre,
          juce::AudioChannelSet::LFE, juce::AudioChannelSet::leftSurround,
          juce::AudioChannelSet::rightSurround, juce::AudioChannelSet::centreSurround };
    if (matchesOrderedTypes(layout, sixOneFilm))
        return buildNames(sixOneFilm);

    const auto sevenZeroFilm = std::initializer_list<ChannelType>
        { juce::AudioChannelSet::left, juce::AudioChannelSet::right, juce::AudioChannelSet::centre,
          juce::AudioChannelSet::leftSurroundSide, juce::AudioChannelSet::rightSurroundSide,
          juce::AudioChannelSet::leftSurroundRear, juce::AudioChannelSet::rightSurroundRear };
    if (matchesOrderedTypes(layout, sevenZeroFilm))
        return buildNames(sevenZeroFilm);

    const auto sevenOneFilm = std::initializer_list<ChannelType>
        { juce::AudioChannelSet::left, juce::AudioChannelSet::right, juce::AudioChannelSet::centre,
          juce::AudioChannelSet::LFE, juce::AudioChannelSet::leftSurroundSide,
          juce::AudioChannelSet::rightSurroundSide, juce::AudioChannelSet::leftSurroundRear,
          juce::AudioChannelSet::rightSurroundRear };
    if (matchesOrderedTypes(layout, sevenOneFilm))
        return buildNames(sevenOneFilm);

    const auto sevenOneMusic = std::initializer_list<ChannelType>
        { juce::AudioChannelSet::left, juce::AudioChannelSet::right,
          juce::AudioChannelSet::leftSurroundSide, juce::AudioChannelSet::rightSurroundSide,
          juce::AudioChannelSet::centre, juce::AudioChannelSet::LFE,
          juce::AudioChannelSet::leftSurroundRear, juce::AudioChannelSet::rightSurroundRear };
    if (matchesOrderedTypes(layout, sevenOneMusic))
        return buildNames(sevenOneMusic);

    if (layout == juce::AudioChannelSet::create7point1point2())
    {
        return buildNames({ juce::AudioChannelSet::left, juce::AudioChannelSet::right,
                            juce::AudioChannelSet::centre, juce::AudioChannelSet::LFE,
                            juce::AudioChannelSet::leftSurroundSide,
                            juce::AudioChannelSet::rightSurroundSide,
                            juce::AudioChannelSet::leftSurroundRear,
                            juce::AudioChannelSet::rightSurroundRear,
                            juce::AudioChannelSet::topSideLeft, juce::AudioChannelSet::topSideRight },
                          { "L", "R", "C", "LFE", "Ls", "Rs", "Lrs", "Rrs", "TFL", "TFR" });
    }

    if (layout == juce::AudioChannelSet::create7point1point4())
    {
        return buildNames({ juce::AudioChannelSet::left, juce::AudioChannelSet::right,
                            juce::AudioChannelSet::centre, juce::AudioChannelSet::LFE,
                            juce::AudioChannelSet::leftSurroundSide,
                            juce::AudioChannelSet::rightSurroundSide,
                            juce::AudioChannelSet::leftSurroundRear,
                            juce::AudioChannelSet::rightSurroundRear,
                            juce::AudioChannelSet::topFrontLeft, juce::AudioChannelSet::topFrontRight,
                            juce::AudioChannelSet::topRearLeft, juce::AudioChannelSet::topRearRight });
    }

    if (layout == juce::AudioChannelSet::create9point1point6()
        || layout == juce::AudioChannelSet::create9point1point6ITU())
    {
        return buildNames({ juce::AudioChannelSet::left, juce::AudioChannelSet::right,
                            juce::AudioChannelSet::centre, juce::AudioChannelSet::LFE,
                            juce::AudioChannelSet::leftSurroundSide,
                            juce::AudioChannelSet::rightSurroundSide,
                            juce::AudioChannelSet::leftSurroundRear,
                            juce::AudioChannelSet::rightSurroundRear,
                            juce::AudioChannelSet::wideLeft, juce::AudioChannelSet::wideRight,
                            juce::AudioChannelSet::topFrontLeft, juce::AudioChannelSet::topFrontRight,
                            juce::AudioChannelSet::topSideLeft, juce::AudioChannelSet::topSideRight,
                            juce::AudioChannelSet::topRearLeft, juce::AudioChannelSet::topRearRight });
    }

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

} // namespace ChannelLayoutUtils
