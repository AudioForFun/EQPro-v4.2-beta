#include "PluginProcessor.h"
#include "util/ParamIDs.h"
#include "util/ChannelLayoutUtils.h"
#include "util/Version.h"

// Audio processor implementation: parameters, DSP orchestration, and state I/O.
#include <cmath>
#include <complex>
#include <cstring>

namespace
{
constexpr const char* kParamFreqSuffix = "freq";
constexpr const char* kParamGainSuffix = "gain";
constexpr const char* kParamQSuffix = "q";
constexpr const char* kParamTypeSuffix = "type";
constexpr const char* kParamBypassSuffix = "bypass";
constexpr const char* kParamMsSuffix = "ms";
constexpr const char* kParamSlopeSuffix = "slope";
constexpr const char* kParamSoloSuffix = "solo";
constexpr const char* kParamMixSuffix = "mix";
// v4.4 beta: Harmonic layer parameters
constexpr const char* kParamOddSuffix = "odd";
constexpr const char* kParamMixOddSuffix = "mixOdd";
constexpr const char* kParamEvenSuffix = "even";
constexpr const char* kParamMixEvenSuffix = "mixEven";
constexpr const char* kParamHarmonicBypassSuffix = "harmonicBypass";  // v4.4 beta: Harmonic bypass per band (independent for each of 12 bands)
constexpr const char* kParamDynEnableSuffix = "dynEnable";
constexpr const char* kParamDynModeSuffix = "dynMode";
constexpr const char* kParamDynThreshSuffix = "dynThresh";
constexpr const char* kParamDynAttackSuffix = "dynAttack";
constexpr const char* kParamDynReleaseSuffix = "dynRelease";
constexpr const char* kParamDynAutoSuffix = "dynAuto";
constexpr const char* kParamDynExternalSuffix = "dynExternal";

const juce::StringArray kFilterTypeChoices {
    "Bell",
    "Low Shelf",
    "High Shelf",
    "Low Pass",
    "High Pass",
    "Notch",
    "Band Pass",
    "All Pass",
    "Tilt",
    "Flat Tilt"
};

const juce::StringArray kMsChoices {
    "All",
    "Stereo Front",
    "L",
    "R",
    "Mid Front",
    "Side Front",
    "C",
    "LFE",
    "Stereo Rear",
    "Ls",
    "Rs",
    "Mid Rear",
    "Side Rear",
    "Stereo Lateral",
    "Lrs",
    "Rrs",
    "Mid Lateral",
    "Side Lateral",
    "Cs",
    "Stereo Front Wide",
    "Lw",
    "Rw",
    "Mid Front Wide",
    "Side Front Wide",
    "Stereo Top Front",
    "TFL",
    "TFR",
    "Mid Top Front",
    "Side Top Front",
    "Stereo Top Rear",
    "TRL",
    "TRR",
    "Mid Top Rear",
    "Side Top Rear",
    "Stereo Top Middle",
    "TML",
    "TMR",
    "Mid Top Middle",
    "Side Top Middle"
};

constexpr std::array<float, ParamIDs::kBandsPerChannel> kDefaultBandFreqs {
    20.0f, 50.0f, 100.0f, 200.0f, 400.0f, 800.0f,
    1600.0f, 3200.0f, 6400.0f, 10000.0f, 14000.0f, 18000.0f
};

enum MsChoiceIndex
{
    kMsAll = 0,
    kMsStereoFront,
    kMsLeft,
    kMsRight,
    kMsMidFront,
    kMsSideFront,
    kMsCentre,
    kMsLfe,
    kMsStereoRear,
    kMsLs,
    kMsRs,
    kMsMidRear,
    kMsSideRear,
    kMsStereoLateral,
    kMsLrs,
    kMsRrs,
    kMsMidLateral,
    kMsSideLateral,
    kMsCs,
    kMsStereoFrontWide,
    kMsLw,
    kMsRw,
    kMsMidFrontWide,
    kMsSideFrontWide,
    kMsStereoTopFront,
    kMsTfl,
    kMsTfr,
    kMsMidTopFront,
    kMsSideTopFront,
    kMsStereoTopRear,
    kMsTrl,
    kMsTrr,
    kMsMidTopRear,
    kMsSideTopRear,
    kMsStereoTopMiddle,
    kMsTml,
    kMsTmr,
    kMsMidTopMiddle,
    kMsSideTopMiddle
};


const juce::StringArray kPhaseModeChoices {
    "Real-time",
    "Natural",
    "Linear"
};

const juce::StringArray kLinearQualityChoices {
    "Low",
    "Medium",
    "High",
    "Very High",
    "Intensive"
};

const juce::StringArray kLinearWindowChoices {
    "Hann",
    "Blackman",
    "Kaiser"
};

const juce::StringArray kOversamplingChoices {
    "Off",
    "2x",
    "4x"
};

juce::File getLogDirectory()
{
    auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    if (! documentsDir.exists())
        documentsDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);

    auto dir = documentsDir.getChildFile("EQPro").getChildFile("Logs");
    dir.createDirectory();
    return dir;
}

juce::File makeLogFile()
{
    const auto now = juce::Time::getCurrentTime();
    const juce::String name = "EQPro_" + now.formatted("%Y-%m-%d_%H-%M-%S") + ".log";
    return getLogDirectory().getChildFile(name);
}

std::atomic<int> gLoggerUsers { 0 };
std::unique_ptr<juce::FileLogger> gSharedLogger;
juce::File gSharedLogFile;
juce::CriticalSection gLoggerLock;
std::atomic<bool> gCrashHandlerInstalled { false };

void crashHandler(void*)
{
    if (auto* logger = juce::Logger::getCurrentLogger())
    {
        logger->writeToLog("CRASH: " + juce::SystemStats::getStackBacktrace());
    }
}

void startSharedLogger()
{
    const juce::ScopedLock lock(gLoggerLock);
    if (gLoggerUsers.fetch_add(1) == 0)
    {
        gSharedLogFile = makeLogFile();
        gSharedLogger = std::make_unique<juce::FileLogger>(gSharedLogFile, "EQ Pro log", 0);
        juce::Logger::setCurrentLogger(gSharedLogger.get());
        juce::Logger::writeToLog("Log file: " + gSharedLogFile.getFullPathName());
        juce::Logger::writeToLog("Version: " + Version::displayString());
        juce::Logger::writeToLog("Logger bootstrap: module load.");
        if (! gCrashHandlerInstalled.exchange(true))
            juce::SystemStats::setApplicationCrashHandler(crashHandler);
    }
}

void stopSharedLogger()
{
    const juce::ScopedLock lock(gLoggerLock);
    if (gLoggerUsers.fetch_sub(1) == 1)
    {
        juce::Logger::writeToLog("Log closed.");
        juce::Logger::setCurrentLogger(nullptr);
        gSharedLogger.reset();
        gSharedLogFile = juce::File();
    }
}

struct LoggerBootstrap
{
    LoggerBootstrap() { startSharedLogger(); }
    ~LoggerBootstrap() { stopSharedLogger(); }
};

LoggerBootstrap gLoggerBootstrap;

struct LinearPhaseJob final : public juce::ThreadPoolJob
{
    LinearPhaseJob(eqdsp::EqEngine& engineIn,
                   eqdsp::ParamSnapshot snapshotIn,
                   double sampleRateIn,
                   std::atomic<int>& pendingLatencyIn,
                   std::atomic<bool>& runningIn)
        : juce::ThreadPoolJob("LinearPhaseJob"),
          engine(engineIn),
          snapshot(std::move(snapshotIn)),
          sampleRate(sampleRateIn),
          pendingLatency(pendingLatencyIn),
          running(runningIn)
    {
    }

    JobStatus runJob() override
    {
        engine.updateLinearPhase(snapshot, sampleRate);
        pendingLatency.store(engine.getLatencySamples());
        running.store(false);
        return jobHasFinished;
    }

    eqdsp::EqEngine& engine;
    eqdsp::ParamSnapshot snapshot;
    double sampleRate = 48000.0;
    std::atomic<int>& pendingLatency;
    std::atomic<bool>& running;
};
} // namespace

juce::String EQProAudioProcessor::sharedStateClipboard;

EQProAudioProcessor::EQProAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, &undoManager, "PARAMETERS", createParameterLayout())
{
    initLogging();
    logStartup("EQProAudioProcessor ctor");
    logStartup("Standalone: " + juce::String(juce::JUCEApplicationBase::isStandaloneApp() ? "true" : "false"));
    logStartup("Executable: " + juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName());

    verifyBands =
        juce::SystemStats::getEnvironmentVariable("EQPRO_VERIFY_BANDS", "0").getIntValue() != 0;
    if (juce::JUCEApplicationBase::isStandaloneApp())
        verifyBands = false;
    bandVerifyLogFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("EQPro_band_verify.log");
    if (verifyBands)
        bandVerifyLogFile.deleteFile();

    initializeParamPointers();
    logStartup("Processor init done");
    startTimerHz(10);
}

EQProAudioProcessor::~EQProAudioProcessor()
{
    stopTimer();
    linearPhasePool.removeAllJobs(true, 2000);
    logStartup("Processor dtor");
    shutdownLogging();
}

void EQProAudioProcessor::initLogging()
{
    startSharedLogger();
}

void EQProAudioProcessor::shutdownLogging()
{
    stopSharedLogger();
}

void EQProAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const int channelCount = juce::jmax(getTotalNumInputChannels(), getTotalNumOutputChannels());
    eqEngine.prepare(sampleRate, samplesPerBlock, channelCount);
    meterTap.prepare(sampleRate);
    lastSampleRate = sampleRate;
    lastMaxBlockSize = samplesPerBlock;
    constexpr int analyzerBufferSize = 16384;
    analyzerPreTap.prepare(analyzerBufferSize);
    analyzerPostTap.prepare(analyzerBufferSize);
    analyzerHarmonicTap.prepare(analyzerBufferSize);  // v4.5 beta: Tap for program + harmonics (red curve)
    analyzerExternalTap.prepare(analyzerBufferSize);

    cachedChannelNames = getCurrentChannelNames();
    lastSnapshotHash = buildSnapshot(snapshots[0]);
    activeSnapshot.store(0);
}

void EQProAudioProcessor::releaseResources()
{
}

bool EQProAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainInput = layouts.getChannelSet(true, 0);
    const auto mainOutput = layouts.getChannelSet(false, 0);

    if (mainInput.isDisabled() || mainOutput.isDisabled())
        return false;

    if (mainInput != mainOutput)
        return false;

    const int channels = mainInput.size();
    if (channels < 1 || channels > ParamIDs::kMaxChannels)
        return false;

    const auto sidechain = layouts.getChannelSet(true, 1);
    if (! sidechain.isDisabled())
    {
        const int scChannels = sidechain.size();
        if (scChannels < 1 || scChannels > 2)
            return false;
    }

    return true;
}

void EQProAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    // Critical path: process audio on the realtime thread.
    juce::ScopedNoDenormals noDenormals;
    const auto startTicks = juce::Time::getHighResolutionTicks();

    const auto numChannels = juce::jmin(buffer.getNumChannels(), ParamIDs::kMaxChannels);

    // Optional MIDI learn / mapping.
    const bool midiLearnEnabled = midiLearnParam != nullptr && midiLearnParam->load() > 0.5f;
    if (midiLearnEnabled || learnedMidiCC.load() >= 0)
    {
        for (const auto metadata : midiMessages)
        {
            const auto msg = metadata.getMessage();
            if (! msg.isController())
                continue;

            const int cc = msg.getControllerNumber();
            if (midiLearnEnabled && learnedMidiCC.load() < 0)
                learnedMidiCC.store(cc);

            if (cc != learnedMidiCC.load())
                continue;

            const float value = msg.getControllerValue() / 127.0f;
            const int channel = juce::jlimit(0, ParamIDs::kMaxChannels - 1, selectedChannelIndex.load());
            const int band = juce::jlimit(0, ParamIDs::kBandsPerChannel - 1, selectedBandIndex.load());
            const int target = midiTargetParam != nullptr ? static_cast<int>(midiTargetParam->load()) : 0;
            const char* suffix = (target == 1 ? kParamFreqSuffix : (target == 2 ? kParamQSuffix : kParamGainSuffix));
            if (auto* param = parameters.getParameter(ParamIDs::bandParamId(channel, band, suffix)))
                param->setValueNotifyingHost(value);
        }
    }

    // Optional detector sidechain for dynamics.
    const bool sidechainEnabled = getBusCount(true) > 1 && getBus(true, 1) != nullptr
        && getBus(true, 1)->isEnabled();
    const juce::AudioBuffer<float>* detectorBuffer = nullptr;
    if (sidechainEnabled)
        detectorBuffer = &getBusBuffer(buffer, true, 1);

    if (correlationPairs.empty())
    {
        if (numChannels >= 2)
            meterTap.setCorrelationPair(0, 1);
    }
    else
    {
        const int safeIndex = juce::jlimit(0, static_cast<int>(correlationPairs.size() - 1),
                                           correlationPairIndex);
        const auto pair = correlationPairs[static_cast<size_t>(safeIndex)];
        meterTap.setCorrelationPair(pair.first, pair.second);
    }

    // Pull the active snapshot and run DSP.
    const int snapshotIndex = activeSnapshot.load();
    const auto& snapshot = snapshots[snapshotIndex];
    eqEngine.process(buffer, snapshot, detectorBuffer, analyzerPreTap, analyzerPostTap, analyzerHarmonicTap, meterTap);

    if (snapshot.phaseMode != 0 && lastSampleRate > 0.0)
    {
        // Adaptive quality: reduce linear FIR load during CPU pressure, recover when stable.
        const auto endTicks = juce::Time::getHighResolutionTicks();
        const double elapsedSeconds = juce::Time::highResolutionTicksToSeconds(endTicks - startTicks);
        const double blockSeconds = static_cast<double>(buffer.getNumSamples()) / lastSampleRate;
        if (blockSeconds > 0.0)
        {
            const double ratio = elapsedSeconds / blockSeconds;
            if (ratio > 0.90)
            {
                ++cpuOverloadCounter;
                cpuRecoverCounter = 0;
            }
            else if (ratio < 0.60)
            {
                ++cpuRecoverCounter;
                cpuOverloadCounter = 0;
            }

            int currentOffset = adaptiveQualityOffset.load();
            if (cpuOverloadCounter >= 3 && currentOffset > -2)
            {
                currentOffset -= 1;
                adaptiveQualityOffset.store(currentOffset);
                pendingAdaptiveQualityLog.store(currentOffset);
                cpuOverloadCounter = 0;
            }
            else if (cpuRecoverCounter >= 8 && currentOffset < 0)
            {
                currentOffset += 1;
                adaptiveQualityOffset.store(currentOffset);
                pendingAdaptiveQualityLog.store(currentOffset);
                cpuRecoverCounter = 0;
            }
            eqEngine.setAdaptiveQualityOffset(currentOffset);
        }
    }
    else
    {
        if (adaptiveQualityOffset.load() != 0)
        {
            adaptiveQualityOffset.store(0);
            pendingAdaptiveQualityLog.store(0);
            eqEngine.setAdaptiveQualityOffset(0);
        }
        cpuOverloadCounter = 0;
        cpuRecoverCounter = 0;
    }

    if (analyzerExternalParam != nullptr && analyzerExternalParam->load() > 0.5f && detectorBuffer != nullptr
        && detectorBuffer->getNumChannels() > 0)
        analyzerExternalTap.push(detectorBuffer->getReadPointer(0), detectorBuffer->getNumSamples());
}

bool EQProAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String EQProAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EQProAudioProcessor::acceptsMidi() const
{
    return true;
}

bool EQProAudioProcessor::producesMidi() const
{
    return false;
}

bool EQProAudioProcessor::isMidiEffect() const
{
    return false;
}

double EQProAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EQProAudioProcessor::getNumPrograms()
{
    return 1;
}

int EQProAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EQProAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String EQProAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void EQProAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void EQProAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    parameters.state.setProperty("showPhase", showPhasePreference, nullptr);
    parameters.state.setProperty("presetSelection", presetSelection, nullptr);
    parameters.state.setProperty("presetApplyTarget", presetApplyTarget, nullptr);
    parameters.state.setProperty("snapshotA", snapshotA, nullptr);
    parameters.state.setProperty("snapshotB", snapshotB, nullptr);
    parameters.state.setProperty("snapshotC", snapshotC, nullptr);
    parameters.state.setProperty("snapshotD", snapshotD, nullptr);
    parameters.state.setProperty("darkTheme", darkTheme, nullptr);
    parameters.state.setProperty("themeMode", themeMode, nullptr);
    parameters.state.setProperty("correlationPairIndex", correlationPairIndex, nullptr);
    parameters.state.setProperty("favoritePresets", favoritePresets, nullptr);
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void EQProAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const bool loadStateInStandalone =
        juce::SystemStats::getEnvironmentVariable("EQPRO_LOAD_STATE", "0").getIntValue() != 0;
    if (juce::JUCEApplicationBase::isStandaloneApp() && ! loadStateInStandalone)
        return;

    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
        replaceStateSafely(juce::ValueTree::fromXml(*xml));

    showPhasePreference = parameters.state.getProperty("showPhase", true);
    presetSelection = static_cast<int>(parameters.state.getProperty("presetSelection", 0));
    presetApplyTarget = static_cast<int>(parameters.state.getProperty("presetApplyTarget", 0));
    snapshotA = parameters.state.getProperty("snapshotA", "");
    snapshotB = parameters.state.getProperty("snapshotB", "");
    snapshotC = parameters.state.getProperty("snapshotC", "");
    snapshotD = parameters.state.getProperty("snapshotD", "");
    darkTheme = parameters.state.getProperty("darkTheme", true);
    themeMode = static_cast<int>(parameters.state.getProperty("themeMode", darkTheme ? 0 : 1));
    darkTheme = (themeMode == 0);
    correlationPairIndex = static_cast<int>(parameters.state.getProperty("correlationPairIndex", 0));
    favoritePresets = parameters.state.getProperty("favoritePresets", "");
}

juce::AudioProcessorValueTreeState& EQProAudioProcessor::getParameters()
{
    return parameters;
}

AudioFifo& EQProAudioProcessor::getAnalyzerPreFifo()
{
    return analyzerPreTap.getFifo();
}

AudioFifo& EQProAudioProcessor::getAnalyzerPostFifo()
{
    return analyzerPostTap.getFifo();
}

AudioFifo& EQProAudioProcessor::getAnalyzerHarmonicFifo()  // v4.5 beta: FIFO for program + harmonics (red curve)
{
    return analyzerHarmonicTap.getFifo();
}

AudioFifo& EQProAudioProcessor::getAnalyzerExternalFifo()
{
    return analyzerExternalTap.getFifo();
}

std::vector<juce::String> EQProAudioProcessor::getCurrentChannelNames() const
{
    const auto* bus = getBus(true, 0);
    if (bus == nullptr)
        bus = getBus(false, 0);

    if (bus != nullptr)
        return ChannelLayoutUtils::getChannelNames(bus->getCurrentLayout());

    return { "Ch 1" };
}

juce::String EQProAudioProcessor::getCurrentLayoutDescription() const
{
    const auto* bus = getBus(true, 0);
    if (bus == nullptr)
        bus = getBus(false, 0);

    if (bus != nullptr)
        return bus->getCurrentLayout().getDescription();

    return "Unknown";
}

eqdsp::ChannelMeterState EQProAudioProcessor::getMeterState(int channelIndex) const
{
    return meterTap.getState(channelIndex);
}

float EQProAudioProcessor::getCorrelation() const
{
    return meterTap.getCorrelation();
}

int EQProAudioProcessor::getGoniometerPoints(juce::Point<float>* dest, int maxPoints, int& writePos) const
{
    return meterTap.copyScopePoints(dest, maxPoints, writePos);
}

juce::StringArray EQProAudioProcessor::getCorrelationPairNames()
{
    const int channelCount = juce::jlimit(0, ParamIDs::kMaxChannels, getTotalNumInputChannels());
    if (channelCount != correlationChannelCount || correlationPairs.empty())
    {
        correlationPairs.clear();
        correlationChannelCount = channelCount;
        const auto names = getCurrentChannelNames();
        for (int i = 0; i < channelCount; ++i)
        {
            for (int j = i + 1; j < channelCount; ++j)
                correlationPairs.emplace_back(i, j);
        }
    }

    juce::StringArray labels;
    const auto names = getCurrentChannelNames();
    for (const auto& pair : correlationPairs)
    {
        const auto left = pair.first < static_cast<int>(names.size())
            ? names[static_cast<size_t>(pair.first)]
            : ("Ch " + juce::String(pair.first + 1));
        const auto right = pair.second < static_cast<int>(names.size())
            ? names[static_cast<size_t>(pair.second)]
            : ("Ch " + juce::String(pair.second + 1));
        labels.add(left + "/" + right);
    }

    if (labels.isEmpty())
        labels.add("L/R");

    return labels;
}

void EQProAudioProcessor::setCorrelationPairIndex(int index)
{
    correlationPairIndex = index;
    parameters.state.setProperty("correlationPairIndex", correlationPairIndex, nullptr);
}

int EQProAudioProcessor::getCorrelationPairIndex() const
{
    return correlationPairIndex;
}

void EQProAudioProcessor::setShowPhasePreference(bool enabled)
{
    showPhasePreference = enabled;
    parameters.state.setProperty("showPhase", showPhasePreference, nullptr);
}

bool EQProAudioProcessor::getShowPhasePreference() const
{
    return showPhasePreference;
}

int EQProAudioProcessor::getLastRmsPhaseMode() const
{
    return eqEngine.getLastRmsPhaseMode();
}

juce::UndoManager* EQProAudioProcessor::getUndoManager()
{
    return &undoManager;
}

void EQProAudioProcessor::setPresetSelection(int index)
{
    presetSelection = index;
    parameters.state.setProperty("presetSelection", presetSelection, nullptr);
}

int EQProAudioProcessor::getPresetSelection() const
{
    return presetSelection;
}

void EQProAudioProcessor::setPresetApplyTarget(int index)
{
    presetApplyTarget = index;
    parameters.state.setProperty("presetApplyTarget", presetApplyTarget, nullptr);
}

int EQProAudioProcessor::getPresetApplyTarget() const
{
    return presetApplyTarget;
}

void EQProAudioProcessor::storeSnapshotA()
{
    if (auto xml = parameters.copyState().createXml())
        snapshotA = xml->toString();
    parameters.state.setProperty("snapshotA", snapshotA, nullptr);
}

void EQProAudioProcessor::storeSnapshotB()
{
    if (auto xml = parameters.copyState().createXml())
        snapshotB = xml->toString();
    parameters.state.setProperty("snapshotB", snapshotB, nullptr);
}

void EQProAudioProcessor::recallSnapshotA()
{
    if (snapshotA.isNotEmpty())
        parameters.replaceState(juce::ValueTree::fromXml(snapshotA));
}

void EQProAudioProcessor::recallSnapshotB()
{
    if (snapshotB.isNotEmpty())
        parameters.replaceState(juce::ValueTree::fromXml(snapshotB));
}

void EQProAudioProcessor::storeSnapshotC()
{
    if (auto xml = parameters.copyState().createXml())
        snapshotC = xml->toString();
    parameters.state.setProperty("snapshotC", snapshotC, nullptr);
}

void EQProAudioProcessor::storeSnapshotD()
{
    if (auto xml = parameters.copyState().createXml())
        snapshotD = xml->toString();
    parameters.state.setProperty("snapshotD", snapshotD, nullptr);
}

void EQProAudioProcessor::recallSnapshotC()
{
    if (snapshotC.isNotEmpty())
        parameters.replaceState(juce::ValueTree::fromXml(snapshotC));
}

void EQProAudioProcessor::recallSnapshotD()
{
    if (snapshotD.isNotEmpty())
        parameters.replaceState(juce::ValueTree::fromXml(snapshotD));
}

void EQProAudioProcessor::setDarkTheme(bool enabled)
{
    darkTheme = enabled;
    parameters.state.setProperty("darkTheme", darkTheme, nullptr);
    themeMode = darkTheme ? 0 : 1;
    parameters.state.setProperty("themeMode", themeMode, nullptr);
}

bool EQProAudioProcessor::getDarkTheme() const
{
    return darkTheme;
}

void EQProAudioProcessor::setThemeMode(int mode)
{
    themeMode = juce::jlimit(0, 1, mode);
    darkTheme = (themeMode == 0);
    parameters.state.setProperty("themeMode", themeMode, nullptr);
    parameters.state.setProperty("darkTheme", darkTheme, nullptr);
}

int EQProAudioProcessor::getThemeMode() const
{
    return themeMode;
}

void EQProAudioProcessor::setFavoritePresets(const juce::String& names)
{
    favoritePresets = names;
    parameters.state.setProperty("favoritePresets", favoritePresets, nullptr);
}

juce::String EQProAudioProcessor::getFavoritePresets() const
{
    return favoritePresets;
}

void EQProAudioProcessor::copyStateToClipboard()
{
    if (auto xml = parameters.copyState().createXml())
        sharedStateClipboard = xml->toString();
}

void EQProAudioProcessor::pasteStateFromClipboard()
{
    if (sharedStateClipboard.isNotEmpty())
        replaceStateSafely(juce::ValueTree::fromXml(sharedStateClipboard));
}

bool EQProAudioProcessor::replaceStateSafely(const juce::ValueTree& newState)
{
    if (! newState.isValid())
        return false;
    if (newState.getType() != parameters.state.getType())
        return false;
    if (newState.getNumChildren() == 0)
        return false;

    parameters.replaceState(newState);
    selectedBandIndex.store(juce::jlimit(0, ParamIDs::kBandsPerChannel - 1, selectedBandIndex.load()));
    selectedChannelIndex.store(juce::jlimit(0, ParamIDs::kMaxChannels - 1, selectedChannelIndex.load()));
    return true;
}

void EQProAudioProcessor::setDebugToneEnabled(bool enabled)
{
    eqEngine.setDebugToneEnabled(enabled);
}

void EQProAudioProcessor::setSelectedBandIndex(int index)
{
    selectedBandIndex.store(juce::jlimit(0, ParamIDs::kBandsPerChannel - 1, index));
}

void EQProAudioProcessor::setSelectedChannelIndex(int index)
{
    selectedChannelIndex.store(juce::jlimit(0, ParamIDs::kMaxChannels - 1, index));
}

int EQProAudioProcessor::getSelectedBandIndex() const
{
    return selectedBandIndex.load();
}

int EQProAudioProcessor::getSelectedChannelIndex() const
{
    return selectedChannelIndex.load();
}

float EQProAudioProcessor::getBandDetectorDb(int channelIndex, int bandIndex) const
{
    juce::ignoreUnused(channelIndex, bandIndex);
    return -60.0f;
}

float EQProAudioProcessor::getBandDynamicGainDb(int channelIndex, int bandIndex) const
{
    juce::ignoreUnused(channelIndex, bandIndex);
    return 0.0f;
}

void EQProAudioProcessor::logStartup(const juce::String& message)
{
    if (auto* logger = juce::Logger::getCurrentLogger())
        logger->writeToLog(message);
}

void EQProAudioProcessor::logBandVerify(const juce::String& message)
{
    if (! verifyBands)
        return;
    bandVerifyLogFile.appendText(message + "\n");
    if (auto* logger = juce::Logger::getCurrentLogger())
        logger->writeToLog(message);
}

void EQProAudioProcessor::verifyBandIndependence()
{
    const int channel = 0;
    const char* suffixes[] { kParamFreqSuffix, kParamGainSuffix, kParamQSuffix, kParamMixSuffix };
    std::array<std::array<float, ParamIDs::kBandsPerChannel>, 4> baseline {};
    for (int s = 0; s < 4; ++s)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            if (auto* param = parameters.getParameter(ParamIDs::bandParamId(channel, band, suffixes[s])))
                baseline[static_cast<size_t>(s)][static_cast<size_t>(band)] = param->getValue();
        }
    }

    logBandVerify("Band verify start");
    for (int s = 0; s < 4; ++s)
    {
        const juce::String suffix(suffixes[s]);
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            auto* target = parameters.getParameter(ParamIDs::bandParamId(channel, band, suffix));
            if (target == nullptr)
                continue;
            const float defaultValue = target->getDefaultValue();
            target->setValueNotifyingHost(defaultValue);

            for (int other = 0; other < ParamIDs::kBandsPerChannel; ++other)
            {
                if (other == band)
                    continue;
                auto* param = parameters.getParameter(ParamIDs::bandParamId(channel, other, suffix));
                if (param == nullptr)
                    continue;
                const float current = param->getValue();
                const float before = baseline[static_cast<size_t>(s)][static_cast<size_t>(other)];
                if (std::abs(current - before) > 0.0005f)
                {
                    logBandVerify("Cross-band change: " + suffix + " band " + juce::String(band + 1)
                        + " -> band " + juce::String(other + 1));
                }
            }

            for (int restoreBand = 0; restoreBand < ParamIDs::kBandsPerChannel; ++restoreBand)
            {
                auto* param = parameters.getParameter(ParamIDs::bandParamId(channel, restoreBand, suffix));
                if (param == nullptr)
                    continue;
                param->setValueNotifyingHost(baseline[static_cast<size_t>(s)][static_cast<size_t>(restoreBand)]);
            }
        }
    }
    logBandVerify("Band verify end");
}

void EQProAudioProcessor::initializeParamPointers()
{
    globalBypassParam = parameters.getRawParameterValue(ParamIDs::globalBypass);
    globalMixParam = parameters.getRawParameterValue(ParamIDs::globalMix);
    phaseModeParam = parameters.getRawParameterValue(ParamIDs::phaseMode);
    linearQualityParam = parameters.getRawParameterValue(ParamIDs::linearQuality);
    linearWindowParam = parameters.getRawParameterValue(ParamIDs::linearWindow);
    oversamplingParam = parameters.getRawParameterValue(ParamIDs::oversampling);
    outputTrimParam = parameters.getRawParameterValue(ParamIDs::outputTrim);
    spectralEnableParam = parameters.getRawParameterValue(ParamIDs::spectralEnable);
    spectralThresholdParam = parameters.getRawParameterValue(ParamIDs::spectralThreshold);
    spectralRatioParam = parameters.getRawParameterValue(ParamIDs::spectralRatio);
    spectralAttackParam = parameters.getRawParameterValue(ParamIDs::spectralAttack);
    spectralReleaseParam = parameters.getRawParameterValue(ParamIDs::spectralRelease);
    spectralMixParam = parameters.getRawParameterValue(ParamIDs::spectralMix);
    characterModeParam = parameters.getRawParameterValue(ParamIDs::characterMode);
    qModeParam = parameters.getRawParameterValue(ParamIDs::qMode);
    qModeAmountParam = parameters.getRawParameterValue(ParamIDs::qModeAmount);
    analyzerExternalParam = parameters.getRawParameterValue(ParamIDs::analyzerExternal);
    autoGainEnableParam = parameters.getRawParameterValue(ParamIDs::autoGainEnable);
    gainScaleParam = parameters.getRawParameterValue(ParamIDs::gainScale);
    phaseInvertParam = parameters.getRawParameterValue(ParamIDs::phaseInvert);
    midiLearnParam = parameters.getRawParameterValue(ParamIDs::midiLearn);
    midiTargetParam = parameters.getRawParameterValue(ParamIDs::midiTarget);
    smartSoloParam = parameters.getRawParameterValue(ParamIDs::smartSolo);

    for (int ch = 0; ch < ParamIDs::kMaxChannels; ++ch)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            bandParamPointers[ch][band].frequency =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamFreqSuffix));
            bandParamPointers[ch][band].gain =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamGainSuffix));
            bandParamPointers[ch][band].q =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamQSuffix));
            bandParamPointers[ch][band].type =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamTypeSuffix));
            bandParamPointers[ch][band].bypass =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamBypassSuffix));
            bandParamPointers[ch][band].msTarget =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamMsSuffix));
            bandParamPointers[ch][band].slope =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamSlopeSuffix));
            bandParamPointers[ch][band].solo =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamSoloSuffix));
            bandParamPointers[ch][band].mix =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamMixSuffix));
            // v4.4 beta: Harmonic parameter pointers
            bandParamPointers[ch][band].odd =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamOddSuffix));
            bandParamPointers[ch][band].mixOdd =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamMixOddSuffix));
            bandParamPointers[ch][band].even =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamEvenSuffix));
            bandParamPointers[ch][band].mixEven =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamMixEvenSuffix));
            // v4.5 beta: Harmonic bypass pointer (per-band, independent for each of 12 bands)
            bandParamPointers[ch][band].harmonicBypass =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamHarmonicBypassSuffix));
            bandParamPointers[ch][band].dynEnable =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamDynEnableSuffix));
            bandParamPointers[ch][band].dynMode =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamDynModeSuffix));
            bandParamPointers[ch][band].dynThreshold =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamDynThreshSuffix));
            bandParamPointers[ch][band].dynAttack =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamDynAttackSuffix));
            bandParamPointers[ch][band].dynRelease =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamDynReleaseSuffix));
            bandParamPointers[ch][band].dynAuto =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamDynAutoSuffix));
            bandParamPointers[ch][band].dynExternal =
                parameters.getRawParameterValue(ParamIDs::bandParamId(ch, band, kParamDynExternalSuffix));
        }
    }
}

// Defines every APVTS parameter (global + per-band).
juce::AudioProcessorValueTreeState::ParameterLayout EQProAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.reserve(ParamIDs::kMaxChannels * ParamIDs::kBandsPerChannel * 22 + 8);

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::globalBypass, "Global Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::globalMix, "Global Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        100.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::phaseMode, "Phase Mode",
        kPhaseModeChoices, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::linearQuality, "Linear Quality",
        kLinearQualityChoices, 1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::linearWindow, "Linear Window",
        kLinearWindowChoices, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            ParamIDs::oversampling, "Oversampling",
            kOversamplingChoices, 0));
        
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::outputTrim, "Output Trim",
        juce::NormalisableRange<float>(-100.0f, 24.0f, 0.01f),
        0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::spectralEnable, "Spectral Enable",
        false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::spectralThreshold, "Spectral Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f),
        -24.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::spectralRatio, "Spectral Ratio",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.01f),
        2.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::spectralAttack, "Spectral Attack",
        juce::NormalisableRange<float>(1.0f, 200.0f, 0.1f),
        20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::spectralRelease, "Spectral Release",
        juce::NormalisableRange<float>(5.0f, 1000.0f, 0.1f),
        200.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::spectralMix, "Spectral Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        100.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::characterMode, "Character Mode",
        juce::StringArray("Off", "Gentle", "Warm"),
        0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::qMode, "Q Mode",
        juce::StringArray("Constant", "Proportional"),
        0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::qModeAmount, "Q Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::analyzerRange, "Analyzer Range",
        juce::StringArray("3 dB", "6 dB", "12 dB", "30 dB"),
        2));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::analyzerSpeed, "Analyzer Speed",
        juce::StringArray("Slow", "Normal", "Fast"),
        1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::analyzerView, "Analyzer View",
        juce::StringArray("Both", "Pre", "Post"),
        0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::analyzerFreeze, "Analyzer Freeze",
        false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::analyzerExternal, "Analyzer External",
        false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::autoGainEnable, "Auto Gain",
        false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::gainScale, "Gain Scale",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        100.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::phaseInvert, "Phase Invert",
        false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::midiLearn, "MIDI Learn",
        false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::midiTarget, "MIDI Target",
        juce::StringArray("Gain", "Freq", "Q"),
        0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::smartSolo, "Smart Solo",
        false));

    const juce::NormalisableRange<float> freqRange(20.0f, 20000.0f, 0.01f, 0.5f);
    const juce::NormalisableRange<float> gainRange(-48.0f, 48.0f, 0.01f);
    const juce::NormalisableRange<float> qRange(0.1f, 18.0f, 0.01f, 0.5f);

    for (int ch = 0; ch < ParamIDs::kMaxChannels; ++ch)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamFreqSuffix),
                ParamIDs::bandParamName(ch, band, "Freq"),
                freqRange,
                kDefaultBandFreqs[static_cast<size_t>(band)]));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamGainSuffix),
                ParamIDs::bandParamName(ch, band, "Gain"),
                gainRange,
                0.0f));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamQSuffix),
                ParamIDs::bandParamName(ch, band, "Q"),
                qRange,
                0.707f));

            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                ParamIDs::bandParamId(ch, band, kParamTypeSuffix),
                ParamIDs::bandParamName(ch, band, "Type"),
                kFilterTypeChoices,
                0));

            params.push_back(std::make_unique<juce::AudioParameterBool>(
                ParamIDs::bandParamId(ch, band, kParamBypassSuffix),
                ParamIDs::bandParamName(ch, band, "Bypass"),
                true));

            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                ParamIDs::bandParamId(ch, band, kParamMsSuffix),
                ParamIDs::bandParamName(ch, band, "M/S"),
                kMsChoices,
                0));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamSlopeSuffix),
                ParamIDs::bandParamName(ch, band, "Slope"),
                juce::NormalisableRange<float>(6.0f, 96.0f, 6.0f),
                12.0f));

            params.push_back(std::make_unique<juce::AudioParameterBool>(
                ParamIDs::bandParamId(ch, band, kParamSoloSuffix),
                ParamIDs::bandParamName(ch, band, "Solo"),
                false));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamMixSuffix),
                ParamIDs::bandParamName(ch, band, "Mix"),
                juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
                100.0f));
            
            // v4.4 beta: Harmonic layer parameters
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamOddSuffix),
                ParamIDs::bandParamName(ch, band, "Odd Harmonic"),
                juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f),
                0.0f));
            
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamMixOddSuffix),
                ParamIDs::bandParamName(ch, band, "Mix Odd"),
                juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
                100.0f));
            
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamEvenSuffix),
                ParamIDs::bandParamName(ch, band, "Even Harmonic"),
                juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f),
                0.0f));
            
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamMixEvenSuffix),
                ParamIDs::bandParamName(ch, band, "Mix Even"),
                juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
                100.0f));
            
            // v4.5 beta: Harmonic bypass parameter (per-band, independent for each of 12 bands)
            // Default to bypassed so harmonic layer is opt-in per band.
            params.push_back(std::make_unique<juce::AudioParameterBool>(
                ParamIDs::bandParamId(ch, band, kParamHarmonicBypassSuffix),
                ParamIDs::bandParamName(ch, band, "Harmonic Bypass"),
                true));

            params.push_back(std::make_unique<juce::AudioParameterBool>(
                ParamIDs::bandParamId(ch, band, kParamDynEnableSuffix),
                ParamIDs::bandParamName(ch, band, "Dyn Enable"),
                false));

            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                ParamIDs::bandParamId(ch, band, kParamDynModeSuffix),
                ParamIDs::bandParamName(ch, band, "Dyn Mode"),
                juce::StringArray("Up", "Down"),
                0));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamDynThreshSuffix),
                ParamIDs::bandParamName(ch, band, "Dyn Threshold"),
                juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f),
                -24.0f));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamDynAttackSuffix),
                ParamIDs::bandParamName(ch, band, "Dyn Attack"),
                juce::NormalisableRange<float>(1.0f, 200.0f, 0.1f),
                20.0f));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                ParamIDs::bandParamId(ch, band, kParamDynReleaseSuffix),
                ParamIDs::bandParamName(ch, band, "Dyn Release"),
                juce::NormalisableRange<float>(5.0f, 1000.0f, 0.1f),
                200.0f));

            params.push_back(std::make_unique<juce::AudioParameterBool>(
                ParamIDs::bandParamId(ch, band, kParamDynAutoSuffix),
                ParamIDs::bandParamName(ch, band, "Dyn Auto Scale"),
                true));

            params.push_back(std::make_unique<juce::AudioParameterBool>(
                ParamIDs::bandParamId(ch, band, kParamDynExternalSuffix),
                ParamIDs::bandParamName(ch, band, "Dyn External"),
                false));

        }
    }

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQProAudioProcessor();
}

void EQProAudioProcessor::timerCallback()
{
    // Critical path: build next snapshot and schedule FIR rebuilds.
    if (verifyBands && ! verifyBandsDone)
    {
        verifyBandsDone = true;
        verifyBandIndependence();
    }

    cachedChannelNames = getCurrentChannelNames();
    const int nextIndex = 1 - activeSnapshot.load();
    const uint64_t hash = buildSnapshot(snapshots[nextIndex]);
    ++snapshotTick;

    const auto sampleRate = getSampleRate();
    if (pendingLatencySamples.load() >= 0)
    {
        setLatencySamples(pendingLatencySamples.load());
        pendingLatencySamples.store(-1);
    }

    const bool paramChanged = hash != lastSnapshotHash;
    if (paramChanged)
    {
        lastParamChangeTick = snapshotTick;
        pendingLinearRebuild = true;
    }

    if (sampleRate > 0.0 && (paramChanged || pendingLinearRebuild))
    {
        const auto& snapshot = snapshots[nextIndex];
        const bool phaseConfigChanged = snapshot.phaseMode != lastLinearPhaseMode
            || snapshot.linearQuality != lastLinearQuality
            || snapshot.linearWindow != lastLinearWindow;
        // Debounce FIR rebuilds while dragging to reduce linear-phase artifacts.
        const bool allowRebuild = phaseConfigChanged
            || (pendingLinearRebuild && (snapshotTick - lastParamChangeTick) >= 6);

        if (allowRebuild && ! linearJobRunning.load())
        {
            linearJobRunning.store(true);
            juce::Logger::writeToLog("LinearPhase: scheduling FIR rebuild (mode="
                                     + juce::String(snapshot.phaseMode)
                                     + ", quality=" + juce::String(snapshot.linearQuality)
                                     + ", window=" + juce::String(snapshot.linearWindow) + ")");
            linearPhasePool.addJob(new LinearPhaseJob(eqEngine, snapshot, sampleRate,
                                                      pendingLatencySamples, linearJobRunning),
                                   true);
            lastLinearRebuildTick = snapshotTick;
            lastLinearPhaseMode = snapshot.phaseMode;
            lastLinearQuality = snapshot.linearQuality;
            lastLinearWindow = snapshot.linearWindow;
            pendingLinearRebuild = false;
        }
    }

    if (hash != lastSnapshotHash)
    {
        lastSnapshotHash = hash;
        activeSnapshot.store(nextIndex);
    }

    static int rmsLogTick = 0;
    static int lastLogMode = -1;
    static int lastLogQuality = -1;
    if (++rmsLogTick >= 30)
    {
        rmsLogTick = 0;
        const int mode = eqEngine.getLastRmsPhaseMode();
        const int quality = eqEngine.getLastRmsQuality();
        const float preDb = eqEngine.getLastPreRmsDb();
        const float postDb = eqEngine.getLastPostRmsDb();
        if (mode != lastLogMode || quality != lastLogQuality
            || std::abs(postDb - preDb) > 0.5f)
        {
            lastLogMode = mode;
            lastLogQuality = quality;
            logStartup("RMS delta: mode=" + juce::String(mode)
                       + " quality=" + juce::String(quality)
                       + " pre=" + juce::String(preDb, 2) + " dB"
                       + " post=" + juce::String(postDb, 2) + " dB"
                       + " delta=" + juce::String(postDb - preDb, 2) + " dB");
        }
    }

    const int pendingQualityLog = pendingAdaptiveQualityLog.exchange(999);
    if (pendingQualityLog != 999)
    {
        logStartup("Adaptive quality offset: " + juce::String(pendingQualityLog));
        pendingLinearRebuild = true;
        lastParamChangeTick = snapshotTick - 6;
    }
}


#if 0
void EQProAudioProcessor::rebuildLinearPhase(int taps, double sampleRate, int channels)
{
    const int fftSize = juce::nextPowerOfTwo(taps * 2);
    const int fftOrder = static_cast<int>(std::log2(fftSize));
    if (fftSize != firFftSize || fftOrder != firFftOrder)
    {
        firFftSize = fftSize;
        firFftOrder = fftOrder;
        firFft = std::make_unique<juce::dsp::FFT>(firFftOrder);
        firData.assign(static_cast<size_t>(firFftSize) * 2, 0.0f);
    }

    if (static_cast<int>(firImpulse.size()) != taps)
        firImpulse.assign(static_cast<size_t>(taps), 0.0f);

    const int windowIndex = linearWindowParam != nullptr
        ? static_cast<int>(linearWindowParam->load())
        : 0;
    const auto method = windowIndex == 1
        ? juce::dsp::WindowingFunction<float>::blackman
        : (windowIndex == 2 ? juce::dsp::WindowingFunction<float>::kaiser : juce::dsp::WindowingFunction<float>::hann);

    if (firWindow == nullptr || static_cast<int>(firImpulse.size()) != taps
        || firWindowMethod != static_cast<int>(method))
    {
        firWindow = std::make_unique<juce::dsp::WindowingFunction<float>>(taps, method);
        firWindowMethod = static_cast<int>(method);
    }

    const auto& channelNames = cachedChannelNames.empty() ? getCurrentChannelNames() : cachedChannelNames;
    auto findIndex = [&channelNames](const juce::String& name) -> int
    {
        for (int i = 0; i < static_cast<int>(channelNames.size()); ++i)
            if (channelNames[static_cast<size_t>(i)] == name)
                return i;
        return -1;
    };
    auto maskFor = [&](const juce::String& name) -> uint32_t
    {
        const int idx = findIndex(name);
        return idx >= 0 ? (1u << static_cast<uint32_t>(idx)) : 0u;
    };
    auto maskForPair = [&](const juce::String& left, const juce::String& right) -> uint32_t
    {
        return maskFor(left) | maskFor(right);
    };
    std::array<uint32_t, ParamIDs::kBandsPerChannel> bandChannelMasks {};
    const uint32_t maskAll = (channels >= 32)
        ? 0xFFFFFFFFu
        : (channels > 0 ? ((1u << static_cast<uint32_t>(channels)) - 1u) : 0u);
    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
    {
        const int target = lastMsTargets[band];
        uint32_t mask = maskAll;
        switch (target)
        {
            case kMsAll: mask = maskAll; break;
            case kMsStereoFront: mask = maskForPair("L", "R"); break;
            case kMsLeft: mask = maskFor("L"); break;
            case kMsRight: mask = maskFor("R"); break;
            case kMsMidFront: mask = maskForPair("L", "R"); break;
            case kMsSideFront: mask = maskForPair("L", "R"); break;
            case kMsCentre: mask = maskFor("C"); break;
            case kMsLfe: mask = maskFor("LFE"); break;
            case kMsStereoRear: mask = maskForPair("Ls", "Rs"); break;
            case kMsLs: mask = maskFor("Ls"); break;
            case kMsRs: mask = maskFor("Rs"); break;
            case kMsMidRear: mask = maskForPair("Ls", "Rs"); break;
            case kMsSideRear: mask = maskForPair("Ls", "Rs"); break;
            case kMsStereoLateral: mask = maskForPair("Lrs", "Rrs"); break;
            case kMsLrs: mask = maskFor("Lrs"); break;
            case kMsRrs: mask = maskFor("Rrs"); break;
            case kMsMidLateral: mask = maskForPair("Lrs", "Rrs"); break;
            case kMsSideLateral: mask = maskForPair("Lrs", "Rrs"); break;
            case kMsCs: mask = maskFor("Cs"); break;
            case kMsStereoFrontWide: mask = maskForPair("Lw", "Rw"); break;
            case kMsLw: mask = maskFor("Lw"); break;
            case kMsRw: mask = maskFor("Rw"); break;
            case kMsMidFrontWide: mask = maskForPair("Lw", "Rw"); break;
            case kMsSideFrontWide: mask = maskForPair("Lw", "Rw"); break;
            case kMsStereoTopFront: mask = maskForPair("TFL", "TFR"); break;
            case kMsTfl: mask = maskFor("TFL"); break;
            case kMsTfr: mask = maskFor("TFR"); break;
            case kMsMidTopFront: mask = maskForPair("TFL", "TFR"); break;
            case kMsSideTopFront: mask = maskForPair("TFL", "TFR"); break;
            case kMsStereoTopRear: mask = maskForPair("TRL", "TRR"); break;
            case kMsTrl: mask = maskFor("TRL"); break;
            case kMsTrr: mask = maskFor("TRR"); break;
            case kMsMidTopRear: mask = maskForPair("TRL", "TRR"); break;
            case kMsSideTopRear: mask = maskForPair("TRL", "TRR"); break;
            case kMsStereoTopMiddle: mask = maskForPair("TML", "TMR"); break;
            case kMsTml: mask = maskFor("TML"); break;
            case kMsTmr: mask = maskFor("TMR"); break;
            case kMsMidTopMiddle: mask = maskForPair("TML", "TMR"); break;
            case kMsSideTopMiddle: mask = maskForPair("TML", "TMR"); break;
            default: mask = maskAll; break;
        }
        bandChannelMasks[band] = mask;
    }

    auto buildImpulse = [&](int channel, std::function<bool(int)> includeBand) -> juce::AudioBuffer<float>
    {
        std::fill(firData.begin(), firData.end(), 0.0f);
        const double nyquist = sampleRate * 0.5;

        for (int bin = 0; bin <= fftSize / 2; ++bin)
        {
            const double freq = (sampleRate * bin) / static_cast<double>(fftSize);
            if (freq > nyquist)
                continue;

            double totalMag = 1.0;
            for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
            {
                if (! includeBand(band))
                    continue;

                const auto& ptrs = bandParamPointers[channel][band];
                if (ptrs.frequency == nullptr)
                    continue;

                if (ptrs.bypass->load() > 0.5f)
                    continue;

                const double mix = ptrs.mix != nullptr ? juce::jlimit(0.0f, 100.0f, ptrs.mix->load()) / 100.0f : 1.0;
                if (mix <= 0.0001)
                    continue;
                const double gainDb = ptrs.gain->load() * mix;
                const double q = std::max(0.1f, ptrs.q->load());
                const double freqParam = ptrs.frequency->load();
                const int type = static_cast<int>(ptrs.type->load());
                const float slopeDb = ptrs.slope != nullptr ? ptrs.slope->load() : 12.0f;

                const double clampedFreq = juce::jlimit(10.0, nyquist * 0.99, freqParam);
                const double omega = 2.0 * juce::MathConstants<double>::pi
                    * clampedFreq / sampleRate;
                const double sinW = std::sin(omega);
                const double cosW = std::cos(omega);
                const double alpha = sinW / (2.0 * q);
                const double a = std::pow(10.0, gainDb / 40.0);

                auto computeMagForType = [&](eqdsp::FilterType filterType,
                                             double gainDbLocal,
                                             double qOverride)
                {
                    const double qLocal = (qOverride > 0.0) ? qOverride : q;
                    const double alphaLocal = sinW / (2.0 * qLocal);
                    const double aLocal = std::pow(10.0, gainDbLocal / 40.0);
                    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a0 = 1.0, a1 = 0.0, a2 = 0.0;

                    switch (filterType)
                    {
                        case eqdsp::FilterType::bell:
                            b0 = 1.0 + alphaLocal * aLocal;
                            b1 = -2.0 * cosW;
                            b2 = 1.0 - alphaLocal * aLocal;
                            a0 = 1.0 + alphaLocal / aLocal;
                            a1 = -2.0 * cosW;
                            a2 = 1.0 - alphaLocal / aLocal;
                            break;
                        case eqdsp::FilterType::lowShelf:
                        {
                            const double beta = std::sqrt(aLocal) / qLocal;
                            b0 = aLocal * ((aLocal + 1.0) - (aLocal - 1.0) * cosW + beta * sinW);
                            b1 = 2.0 * aLocal * ((aLocal - 1.0) - (aLocal + 1.0) * cosW);
                            b2 = aLocal * ((aLocal + 1.0) - (aLocal - 1.0) * cosW - beta * sinW);
                            a0 = (aLocal + 1.0) + (aLocal - 1.0) * cosW + beta * sinW;
                            a1 = -2.0 * ((aLocal - 1.0) + (aLocal + 1.0) * cosW);
                            a2 = (aLocal + 1.0) + (aLocal - 1.0) * cosW - beta * sinW;
                            break;
                        }
                        case eqdsp::FilterType::highShelf:
                        {
                            const double beta = std::sqrt(aLocal) / qLocal;
                            b0 = aLocal * ((aLocal + 1.0) + (aLocal - 1.0) * cosW + beta * sinW);
                            b1 = -2.0 * aLocal * ((aLocal - 1.0) + (aLocal + 1.0) * cosW);
                            b2 = aLocal * ((aLocal + 1.0) + (aLocal - 1.0) * cosW - beta * sinW);
                            a0 = (aLocal + 1.0) - (aLocal - 1.0) * cosW + beta * sinW;
                            a1 = 2.0 * ((aLocal - 1.0) - (aLocal + 1.0) * cosW);
                            a2 = (aLocal + 1.0) - (aLocal - 1.0) * cosW - beta * sinW;
                            break;
                        }
                        case eqdsp::FilterType::lowPass:
                            b0 = (1.0 - cosW) * 0.5;
                            b1 = 1.0 - cosW;
                            b2 = (1.0 - cosW) * 0.5;
                            a0 = 1.0 + alphaLocal;
                            a1 = -2.0 * cosW;
                            a2 = 1.0 - alphaLocal;
                            break;
                        case eqdsp::FilterType::highPass:
                            b0 = (1.0 + cosW) * 0.5;
                            b1 = -(1.0 + cosW);
                            b2 = (1.0 + cosW) * 0.5;
                            a0 = 1.0 + alphaLocal;
                            a1 = -2.0 * cosW;
                            a2 = 1.0 - alphaLocal;
                            break;
                        case eqdsp::FilterType::notch:
                            b0 = 1.0;
                            b1 = -2.0 * cosW;
                            b2 = 1.0;
                            a0 = 1.0 + alphaLocal;
                            a1 = -2.0 * cosW;
                            a2 = 1.0 - alphaLocal;
                            break;
                        case eqdsp::FilterType::bandPass:
                            b0 = alphaLocal;
                            b1 = 0.0;
                            b2 = -alphaLocal;
                            a0 = 1.0 + alphaLocal;
                            a1 = -2.0 * cosW;
                            a2 = 1.0 - alphaLocal;
                            break;
                        case eqdsp::FilterType::allPass:
                            b0 = 1.0 - alphaLocal;
                            b1 = -2.0 * cosW;
                            b2 = 1.0 + alphaLocal;
                            a0 = 1.0 + alphaLocal;
                            a1 = -2.0 * cosW;
                            a2 = 1.0 - alphaLocal;
                            break;
                        case eqdsp::FilterType::tilt:
                        case eqdsp::FilterType::flatTilt:
                            break;
                    }

                    const double invA0 = 1.0 / a0;
                    b0 *= invA0;
                    b1 *= invA0;
                    b2 *= invA0;
                    a1 *= invA0;
                    a2 *= invA0;

                    const double w = 2.0 * juce::MathConstants<double>::pi
                        * juce::jlimit(10.0, nyquist * 0.99, freq) / sampleRate;
                    const std::complex<double> z = std::exp(std::complex<double>(0.0, -w));
                    const std::complex<double> z2 = z * z;
                    const std::complex<double> numerator = b0 + b1 * z + b2 * z2;
                    const std::complex<double> denominator = 1.0 + a1 * z + a2 * z2;
                    return std::abs(numerator / denominator);
                };

                double mag = 1.0;
                const auto filterType = static_cast<eqdsp::FilterType>(type);
                if (filterType == eqdsp::FilterType::tilt || filterType == eqdsp::FilterType::flatTilt)
                {
                    const double qOverride = (filterType == eqdsp::FilterType::flatTilt) ? 0.5 : -1.0;
                    mag = computeMagForType(eqdsp::FilterType::lowShelf, gainDb * 0.5, qOverride)
                        * computeMagForType(eqdsp::FilterType::highShelf, -gainDb * 0.5, qOverride);
                }
                else
                {
                    mag = computeMagForType(filterType, gainDb, -1.0);
                }

                if (filterType == eqdsp::FilterType::allPass)
                    continue;
                if (filterType == eqdsp::FilterType::lowPass || filterType == eqdsp::FilterType::highPass)
                {
                    auto onePoleMag = [&](double cutoff, double freqHz)
                    {
                        const double clamped = juce::jlimit(10.0, nyquist * 0.99, cutoff);
                        const double a1p = std::exp(-2.0 * juce::MathConstants<double>::pi * clamped / sampleRate);
                        const std::complex<double> z1p = std::exp(std::complex<double>(0.0,
                                                                                      -2.0 * juce::MathConstants<double>::pi
                                                                                          * freqHz / sampleRate));
                        if (filterType == eqdsp::FilterType::lowPass)
                            return std::abs((1.0 - a1p) / (1.0 - a1p * z1p));

                        return std::abs(((1.0 + a1p) * 0.5) * (1.0 - z1p) / (1.0 - a1p * z1p));
                    };

                    const float clamped = juce::jlimit(6.0f, 96.0f, slopeDb);
                    const int stages = static_cast<int>(std::floor(clamped / 12.0f));
                    const float remainder = clamped - static_cast<float>(stages) * 12.0f;
                    const bool useOnePole = (remainder >= 6.0f) || stages == 0;
                    if (stages > 0)
                        mag = std::pow(mag, stages);
                    if (useOnePole)
                        mag *= onePoleMag(freqParam, freq);
                }

                totalMag *= mag;
            }

            const int index = bin * 2;
            firData[static_cast<size_t>(index)] = static_cast<float>(totalMag);
            firData[static_cast<size_t>(index + 1)] = 0.0f;

            if (bin > 0 && bin < fftSize / 2)
            {
                const int mirror = (fftSize - bin) * 2;
                firData[static_cast<size_t>(mirror)] = static_cast<float>(totalMag);
                firData[static_cast<size_t>(mirror + 1)] = 0.0f;
            }
        }

        firFft->performRealOnlyInverseTransform(firData.data());

        const int offset = (fftSize - taps) / 2;
        for (int i = 0; i < taps; ++i)
            firImpulse[static_cast<size_t>(i)] = firData[static_cast<size_t>(offset + i)] / fftSize;

        firWindow->multiplyWithWindowingTable(firImpulse.data(), taps);

        juce::AudioBuffer<float> buffer(1, taps);
        buffer.copyFrom(0, 0, firImpulse.data(), taps);
        return buffer;
    };

    auto includeForChannel = [this, &bandChannelMasks](int channel)
    {
        return [this, channel, &bandChannelMasks](int band)
        {
            const int target = lastMsTargets[band];
            if ((bandChannelMasks[band] & (1u << static_cast<uint32_t>(channel))) == 0)
                return false;
            if (channel < 2 && (target == 1 || target == 2 || target == 6))
                return false;
            return true;
        };
    };
    for (int ch = 0; ch < channels; ++ch)
    {
        auto impulse = buildImpulse(ch, includeForChannel(ch));
        linearPhaseEq.loadImpulse(ch, std::move(impulse), sampleRate);
    }

    const bool useMs = channels >= 2
        && std::any_of(lastMsTargets.begin(), lastMsTargets.end(), [](int v) { return v != 0; });

    if (useMs)
    {
        auto includeMid = [this](int band)
        {
            return lastMsTargets[band] == 0 || lastMsTargets[band] == 1 || lastMsTargets[band] == 6;
        };
        auto includeSide = [this](int band)
        {
            return lastMsTargets[band] == 0 || lastMsTargets[band] == 2;
        };

        auto midImpulse = buildImpulse(0, includeMid);
        auto sideImpulse = buildImpulse(0, includeSide);
        linearPhaseMsEq.loadImpulse(0, std::move(midImpulse), sampleRate);
        linearPhaseMsEq.loadImpulse(1, std::move(sideImpulse), sampleRate);
    }

    const int latency = (taps - 1) / 2;
    linearPhaseEq.setLatencySamples(latency);
    setLatencySamples(latency);
}

void EQProAudioProcessor::updateOversampling()
{
    oversamplingIndex = 0;
    oversampler.reset();
    oversampledBuffer.setSize(0, 0);
    return;

    const int factor = oversamplingIndex;
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        getTotalNumInputChannels(),
        factor,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true);
    oversampler->initProcessing(static_cast<size_t>(lastMaxBlockSize));

    const int channels = juce::jmin(getTotalNumInputChannels(), ParamIDs::kMaxChannels);
    const int upFactor = 1 << factor;
    oversampledBuffer.setSize(channels, lastMaxBlockSize * upFactor);
    oversampledBuffer.clear();

    eqDspOversampled.prepare(lastSampleRate * upFactor, lastMaxBlockSize * upFactor, channels);
    eqDspOversampled.reset();
}
#endif

uint64_t EQProAudioProcessor::buildSnapshot(eqdsp::ParamSnapshot& snapshot)
{
    // Critical path: copy current APVTS values into an atomic-safe snapshot.
    const int ioChannels = juce::jmax(getTotalNumInputChannels(), getTotalNumOutputChannels());
    const int numChannels = juce::jmin(ioChannels, ParamIDs::kMaxChannels);
    snapshot.numChannels = numChannels;
    snapshot.globalBypass = globalBypassParam != nullptr && globalBypassParam->load() > 0.5f;
    snapshot.globalMix = globalMixParam != nullptr ? (globalMixParam->load() / 100.0f) : 1.0f;
    snapshot.phaseMode = phaseModeParam != nullptr ? static_cast<int>(phaseModeParam->load()) : 0;
    const int rawQuality =
        linearQualityParam != nullptr ? static_cast<int>(linearQualityParam->load()) : 1;
    // v4.6 beta: Quality now applies across realtime/natural/linear modes.
    snapshot.linearQuality = rawQuality;
    snapshot.linearWindow = linearWindowParam != nullptr ? static_cast<int>(linearWindowParam->load()) : 0;
    // v4.6 beta: Oversampling is driven by the quality ladder (low->none ... intensive->16x).
    snapshot.oversampling = rawQuality;
    snapshot.outputTrimDb = outputTrimParam != nullptr ? outputTrimParam->load() : 0.0f;
    snapshot.characterMode = characterModeParam != nullptr ? static_cast<int>(characterModeParam->load()) : 0;
    snapshot.smartSolo = smartSoloParam != nullptr && smartSoloParam->load() > 0.5f;
    snapshot.qMode = qModeParam != nullptr ? static_cast<int>(qModeParam->load()) : 0;
    snapshot.qModeAmount = qModeAmountParam != nullptr ? qModeAmountParam->load() : 50.0f;
    snapshot.spectralEnabled = spectralEnableParam != nullptr && spectralEnableParam->load() > 0.5f;
    snapshot.spectralThresholdDb = spectralThresholdParam != nullptr ? spectralThresholdParam->load() : -24.0f;
    snapshot.spectralRatio = spectralRatioParam != nullptr ? spectralRatioParam->load() : 2.0f;
    snapshot.spectralAttackMs = spectralAttackParam != nullptr ? spectralAttackParam->load() : 20.0f;
    snapshot.spectralReleaseMs = spectralReleaseParam != nullptr ? spectralReleaseParam->load() : 200.0f;
    snapshot.spectralMix = spectralMixParam != nullptr ? (spectralMixParam->load() / 100.0f) : 1.0f;
    snapshot.autoGainEnabled = autoGainEnableParam != nullptr && autoGainEnableParam->load() > 0.5f;
    snapshot.gainScale = gainScaleParam != nullptr ? (gainScaleParam->load() / 100.0f) : 1.0f;
    snapshot.phaseInvert = phaseInvertParam != nullptr && phaseInvertParam->load() > 0.5f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            const auto& ptrs = bandParamPointers[ch][band];
            auto& dst = snapshot.bands[ch][band];
            if (ptrs.frequency != nullptr)
                dst.frequencyHz = ptrs.frequency->load();
            if (ptrs.gain != nullptr)
                dst.gainDb = ptrs.gain->load();
            if (ptrs.q != nullptr)
                dst.q = ptrs.q->load();
            if (ptrs.type != nullptr)
                dst.type = static_cast<int>(ptrs.type->load());
            dst.bypassed = ptrs.bypass != nullptr && ptrs.bypass->load() > 0.5f;
            dst.msTarget = ptrs.msTarget != nullptr ? static_cast<int>(ptrs.msTarget->load()) : 0;
            dst.slopeDb = ptrs.slope != nullptr ? ptrs.slope->load() : 12.0f;
            dst.solo = ptrs.solo != nullptr && ptrs.solo->load() > 0.5f;
            dst.mix = ptrs.mix != nullptr ? (ptrs.mix->load() / 100.0f) : 1.0f;
            dst.dynEnabled = ptrs.dynEnable != nullptr && ptrs.dynEnable->load() > 0.5f;
            dst.dynMode = ptrs.dynMode != nullptr ? static_cast<int>(ptrs.dynMode->load()) : 0;
            dst.dynThresholdDb = ptrs.dynThreshold != nullptr ? ptrs.dynThreshold->load() : -24.0f;
            dst.dynAttackMs = ptrs.dynAttack != nullptr ? ptrs.dynAttack->load() : 20.0f;
            dst.dynReleaseMs = ptrs.dynRelease != nullptr ? ptrs.dynRelease->load() : 200.0f;
            dst.dynAuto = ptrs.dynAuto != nullptr && ptrs.dynAuto->load() > 0.5f;
            dst.dynExternal = ptrs.dynExternal != nullptr && ptrs.dynExternal->load() > 0.5f;
            // v4.4 beta: Harmonic parameters (per-band, independent for each of 12 bands)
            dst.oddHarmonicDb = ptrs.odd != nullptr ? ptrs.odd->load() : 0.0f;
            dst.mixOdd = ptrs.mixOdd != nullptr ? (ptrs.mixOdd->load() / 100.0f) : 1.0f;
            dst.evenHarmonicDb = ptrs.even != nullptr ? ptrs.even->load() : 0.0f;
            dst.mixEven = ptrs.mixEven != nullptr ? (ptrs.mixEven->load() / 100.0f) : 1.0f;
            dst.harmonicBypassed = ptrs.harmonicBypass != nullptr && ptrs.harmonicBypass->load() > 0.5f;

            // Auto-activate a band if parameters deviate from defaults.
            if (dst.bypassed)
            {
                constexpr float kEps = 1.0e-3f;
                const float defaultFreq = kDefaultBandFreqs[static_cast<size_t>(band)];
                const bool isDefault =
                    std::abs(dst.frequencyHz - defaultFreq) < 0.01f
                    && std::abs(dst.gainDb) < kEps
                    && std::abs(dst.q - 0.707f) < kEps
                    && dst.type == 0
                    && std::abs(dst.slopeDb - 12.0f) < kEps
                    && std::abs(dst.mix - 1.0f) < kEps
                    && dst.msTarget == 0
                    && !dst.solo;
                if (! isDefault)
                    dst.bypassed = false;
            }
        }
    }

    snapshot.msTargets.fill(0);
    snapshot.bandChannelMasks.fill(0);
    const uint32_t maskAll = (numChannels >= 32)
        ? 0xFFFFFFFFu
        : (numChannels > 0 ? ((1u << static_cast<uint32_t>(numChannels)) - 1u) : 0u);

    const auto& channelNames = cachedChannelNames.empty() ? getCurrentChannelNames() : cachedChannelNames;
    auto findIndex = [&channelNames](const juce::String& name) -> int
    {
        for (int i = 0; i < static_cast<int>(channelNames.size()); ++i)
            if (channelNames[static_cast<size_t>(i)] == name)
                return i;
        return -1;
    };
    auto maskForIndex = [&](int index) -> uint32_t
    {
        return (index >= 0 && index < numChannels) ? (1u << static_cast<uint32_t>(index)) : 0u;
    };
    auto maskFor = [&](const juce::String& name) -> uint32_t
    {
        const int idx = findIndex(name);
        return maskForIndex(idx);
    };
    auto maskForPair = [&](const juce::String& left, const juce::String& right) -> uint32_t
    {
        return maskFor(left) | maskFor(right);
    };
    const int sourceChannel = juce::jlimit(0, numChannels - 1, selectedChannelIndex.load());
    const int lIndex = findIndex("L");
    const int rIndex = findIndex("R");
    const uint32_t maskL = maskForIndex(lIndex >= 0 ? lIndex : 0);
    const uint32_t maskR = maskForIndex(rIndex >= 0 ? rIndex : (numChannels > 1 ? 1 : 0));
    const uint32_t maskStereo = maskL | maskR;

    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
    {
        const int target = snapshot.bands[sourceChannel][band].msTarget;
        int msTarget = 0;
        uint32_t mask = maskAll;

        // Map UI selection to a channel mask and optional M/S target.
        switch (target)
        {
            case kMsAll: mask = maskAll; break;
            case kMsStereoFront: mask = maskStereo; break;
            case kMsLeft: mask = maskL; break;
            case kMsRight: mask = maskR; break;
            case kMsMidFront: msTarget = 1; mask = maskStereo; break;
            case kMsSideFront: msTarget = 2; mask = maskStereo; break;
            case kMsCentre: mask = maskFor("C"); break;
            case kMsLfe: mask = maskFor("LFE"); break;
            case kMsStereoRear: mask = maskForPair("Ls", "Rs"); break;
            case kMsLs: mask = maskFor("Ls"); break;
            case kMsRs: mask = maskFor("Rs"); break;
            case kMsMidRear: msTarget = 1; mask = maskForPair("Ls", "Rs"); break;
            case kMsSideRear: msTarget = 2; mask = maskForPair("Ls", "Rs"); break;
            case kMsStereoLateral: mask = maskForPair("Lrs", "Rrs"); break;
            case kMsLrs: mask = maskFor("Lrs"); break;
            case kMsRrs: mask = maskFor("Rrs"); break;
            case kMsMidLateral: msTarget = 1; mask = maskForPair("Lrs", "Rrs"); break;
            case kMsSideLateral: msTarget = 2; mask = maskForPair("Lrs", "Rrs"); break;
            case kMsCs: mask = maskFor("Cs"); break;
            case kMsStereoFrontWide: mask = maskForPair("Lw", "Rw"); break;
            case kMsLw: mask = maskFor("Lw"); break;
            case kMsRw: mask = maskFor("Rw"); break;
            case kMsMidFrontWide: msTarget = 1; mask = maskForPair("Lw", "Rw"); break;
            case kMsSideFrontWide: msTarget = 2; mask = maskForPair("Lw", "Rw"); break;
            case kMsStereoTopFront: mask = maskForPair("TFL", "TFR"); break;
            case kMsTfl: mask = maskFor("TFL"); break;
            case kMsTfr: mask = maskFor("TFR"); break;
            case kMsMidTopFront: msTarget = 1; mask = maskForPair("TFL", "TFR"); break;
            case kMsSideTopFront: msTarget = 2; mask = maskForPair("TFL", "TFR"); break;
            case kMsStereoTopRear: mask = maskForPair("TRL", "TRR"); break;
            case kMsTrl: mask = maskFor("TRL"); break;
            case kMsTrr: mask = maskFor("TRR"); break;
            case kMsMidTopRear: msTarget = 1; mask = maskForPair("TRL", "TRR"); break;
            case kMsSideTopRear: msTarget = 2; mask = maskForPair("TRL", "TRR"); break;
            case kMsStereoTopMiddle: mask = maskForPair("TML", "TMR"); break;
            case kMsTml: mask = maskFor("TML"); break;
            case kMsTmr: mask = maskFor("TMR"); break;
            case kMsMidTopMiddle: msTarget = 1; mask = maskForPair("TML", "TMR"); break;
            case kMsSideTopMiddle: msTarget = 2; mask = maskForPair("TML", "TMR"); break;
            default: mask = maskAll; break;
        }

        // Guard against missing channel labels: fall back to full mask.
        if (mask == 0u)
        {
            mask = maskAll;
            msTarget = 0;
        }
        const auto maskBitCount = [](uint32_t value)
        {
            int count = 0;
            while (value != 0)
            {
                value &= (value - 1u);
                ++count;
            }
            return count;
        };
        // Only allow MS targets when a stereo pair is present.
        if (msTarget != 0 && maskBitCount(mask) < 2)
            msTarget = 0;

        snapshot.msTargets[band] = msTarget;
        snapshot.bandChannelMasks[band] = mask;

        // For multi-channel selections, mirror the band parameters to the covered channels.
        if (maskBitCount(mask) > 1)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                if ((mask & (1u << static_cast<uint32_t>(ch))) != 0)
                    snapshot.bands[ch][band] = snapshot.bands[sourceChannel][band];
            }
        }
    }

    auto hash = uint64_t { 1469598103934665603ull };
    const auto hashFloat = [&hash](float value)
    {
        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        hash ^= bits;
        hash *= 1099511628211ull;
    };
    const auto hashBool = [&hashFloat](bool value)
    {
        hashFloat(value ? 1.0f : 0.0f);
    };

    hashFloat(static_cast<float>(snapshot.numChannels));
    hashBool(snapshot.globalBypass);
    hashFloat(snapshot.globalMix);
    hashFloat(static_cast<float>(snapshot.phaseMode));
    hashFloat(static_cast<float>(snapshot.linearQuality));
    hashFloat(static_cast<float>(snapshot.linearWindow));
    hashFloat(snapshot.outputTrimDb);
    hashFloat(static_cast<float>(snapshot.characterMode));
    hashBool(snapshot.smartSolo);
    hashFloat(static_cast<float>(snapshot.qMode));
    hashFloat(snapshot.qModeAmount);
    hashBool(snapshot.spectralEnabled);
    hashFloat(snapshot.spectralThresholdDb);
    hashFloat(snapshot.spectralRatio);
    hashFloat(snapshot.spectralAttackMs);
    hashFloat(snapshot.spectralReleaseMs);
    hashFloat(snapshot.spectralMix);
    hashBool(snapshot.autoGainEnabled);
    hashFloat(snapshot.gainScale);
    hashBool(snapshot.phaseInvert);

    for (int ch = 0; ch < snapshot.numChannels; ++ch)
    {
        for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
        {
            const auto& b = snapshot.bands[ch][band];
            hashFloat(b.frequencyHz);
            hashFloat(b.gainDb);
            hashFloat(b.q);
            hashFloat(static_cast<float>(b.type));
            hashBool(b.bypassed);
            hashFloat(static_cast<float>(b.msTarget));
            hashFloat(b.slopeDb);
            hashBool(b.solo);
            hashFloat(b.mix);
            hashBool(b.dynEnabled);
            hashFloat(static_cast<float>(b.dynMode));
            hashFloat(b.dynThresholdDb);
            hashFloat(b.dynAttackMs);
            hashFloat(b.dynReleaseMs);
            hashBool(b.dynAuto);
            hashBool(b.dynExternal);
        }
    }

    for (int band = 0; band < ParamIDs::kBandsPerChannel; ++band)
    {
        hashFloat(static_cast<float>(snapshot.msTargets[band]));
        hashFloat(static_cast<float>(snapshot.bandChannelMasks[band]));
    }

    return hash;
}
