#pragma once

#include <JuceHeader.h>
#include "dsp/EqEngine.h"
#include "dsp/ParamSnapshot.h"
#include "dsp/AnalyzerTap.h"
#include "dsp/MeterTap.h"

// Core audio processor: owns DSP engine, parameters, meters, and analyzers.
class EQProAudioProcessor final : public juce::AudioProcessor,
                                  private juce::Timer
{
public:
    EQProAudioProcessor();
    ~EQProAudioProcessor() override;

    // Allocate DSP buffers and prepare processing pipeline.
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    // Release audio resources when playback stops.
    void releaseResources() override;

    // Validate host bus layout support.
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // Main audio processing callback.
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Create the editor instance.
    juce::AudioProcessorEditor* createEditor() override;
    // Whether this processor provides an editor.
    bool hasEditor() const override;

    // Display name for the plugin.
    const juce::String getName() const override;

    // MIDI capabilities.
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    // Report tail time (seconds).
    double getTailLengthSeconds() const override;

    // Program/preset bank support.
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    // State persistence for DAW/session.
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Expose parameter tree to editor components.
    juce::AudioProcessorValueTreeState& getParameters();
    // Analyzer tap accessors (UI thread).
    AudioFifo& getAnalyzerPreFifo();
    AudioFifo& getAnalyzerPostFifo();
    AudioFifo& getAnalyzerExternalFifo();
    // Current channel layout label helpers.
    std::vector<juce::String> getCurrentChannelNames() const;
    juce::String getCurrentLayoutDescription() const;
    // Meter state access.
    eqdsp::ChannelMeterState getMeterState(int channelIndex) const;
    // Correlation/goniometer helpers.
    float getCorrelation() const;
    int getGoniometerPoints(juce::Point<float>* dest, int maxPoints, int& writePos) const;
    juce::StringArray getCorrelationPairNames();
    void setCorrelationPairIndex(int index);
    int getCorrelationPairIndex() const;
    void setShowPhasePreference(bool enabled);
    bool getShowPhasePreference() const;
    // Undo/redo manager for UI actions.
    juce::UndoManager* getUndoManager();
    // Preset browser helpers.
    void setPresetSelection(int index);
    int getPresetSelection() const;
    void setPresetApplyTarget(int index);
    int getPresetApplyTarget() const;
    // Snapshot A/B/C/D helpers.
    void storeSnapshotA();
    void storeSnapshotB();
    void recallSnapshotA();
    void recallSnapshotB();
    void storeSnapshotC();
    void storeSnapshotD();
    void recallSnapshotC();
    void recallSnapshotD();
    // Theme management.
    void setDarkTheme(bool enabled);
    bool getDarkTheme() const;
    void setThemeMode(int mode);
    int getThemeMode() const;
    // Favorite presets list.
    void setFavoritePresets(const juce::String& names);
    juce::String getFavoritePresets() const;
    // Instance clipboard helpers.
    void copyStateToClipboard();
    void pasteStateFromClipboard();
    // Safe state replacement for preset loading.
    bool replaceStateSafely(const juce::ValueTree& newState);
    // Debug tone generator for calibration.
    void setDebugToneEnabled(bool enabled);
    // Selected band/channel (for UI).
    void setSelectedBandIndex(int index);
    void setSelectedChannelIndex(int index);
    int getSelectedBandIndex() const;
    int getSelectedChannelIndex() const;
    // Metering hooks per band.
    float getBandDetectorDb(int channelIndex, int bandIndex) const;
    float getBandDynamicGainDb(int channelIndex, int bandIndex) const;
    // Helper for startup/diagnostic logging.
    void logStartup(const juce::String& message);

    // Build full parameter layout for APVTS.
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct BandParamPointers
    {
        std::atomic<float>* frequency = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* q = nullptr;
        std::atomic<float>* type = nullptr;
        // v4.4 beta: Harmonic layer parameters
        std::atomic<float>* odd = nullptr;
        std::atomic<float>* mixOdd = nullptr;
        std::atomic<float>* even = nullptr;
        std::atomic<float>* mixEven = nullptr;
        std::atomic<float>* bypass = nullptr;
        std::atomic<float>* msTarget = nullptr;
        std::atomic<float>* slope = nullptr;
        std::atomic<float>* solo = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* dynEnable = nullptr;
        std::atomic<float>* dynMode = nullptr;
        std::atomic<float>* dynThreshold = nullptr;
        std::atomic<float>* dynAttack = nullptr;
        std::atomic<float>* dynRelease = nullptr;
        std::atomic<float>* dynAuto = nullptr;
        std::atomic<float>* dynExternal = nullptr;
    };

    // Cache parameter pointers for low-overhead access.
    void initializeParamPointers();
    void timerCallback() override;
    uint64_t buildSnapshot(eqdsp::ParamSnapshot& snapshot);
    void verifyBandIndependence();
    void logBandVerify(const juce::String& message);
    void initLogging();
    void shutdownLogging();

    juce::AudioProcessorValueTreeState parameters;
    juce::UndoManager undoManager;

    std::array<std::array<BandParamPointers, ParamIDs::kBandsPerChannel>,
               ParamIDs::kMaxChannels>
        bandParamPointers {};

    std::atomic<float>* globalBypassParam = nullptr;
    std::atomic<float>* globalMixParam = nullptr;
    std::atomic<float>* phaseModeParam = nullptr;
    std::atomic<float>* linearQualityParam = nullptr;
    std::atomic<float>* linearWindowParam = nullptr;
    std::atomic<float>* oversamplingParam = nullptr;
    std::atomic<float>* outputTrimParam = nullptr;
    std::atomic<float>* spectralEnableParam = nullptr;
    std::atomic<float>* spectralThresholdParam = nullptr;
    std::atomic<float>* spectralRatioParam = nullptr;
    std::atomic<float>* spectralAttackParam = nullptr;
    std::atomic<float>* spectralReleaseParam = nullptr;
    std::atomic<float>* spectralMixParam = nullptr;
    std::atomic<float>* characterModeParam = nullptr;
    std::atomic<float>* qModeParam = nullptr;
    std::atomic<float>* qModeAmountParam = nullptr;
    std::atomic<float>* analyzerExternalParam = nullptr;
    std::atomic<float>* autoGainEnableParam = nullptr;
    std::atomic<float>* gainScaleParam = nullptr;
    std::atomic<float>* phaseInvertParam = nullptr;
    std::atomic<float>* midiLearnParam = nullptr;
    std::atomic<float>* midiTargetParam = nullptr;
    std::atomic<float>* smartSoloParam = nullptr;

    bool verifyBands = false;
    bool verifyBandsDone = false;
    juce::File bandVerifyLogFile;

    // DSP engine + taps for analyzer/metering.
    eqdsp::EqEngine eqEngine;
    eqdsp::AnalyzerTap analyzerPreTap;
    eqdsp::AnalyzerTap analyzerPostTap;
    eqdsp::AnalyzerTap analyzerExternalTap;
    eqdsp::MeterTap meterTap;
    eqdsp::ParamSnapshot snapshots[2];
    std::atomic<int> activeSnapshot { 0 };
    std::atomic<int> selectedBandIndex { 0 };
    std::atomic<int> selectedChannelIndex { 0 };
    std::vector<juce::String> cachedChannelNames;
    bool showPhasePreference = true;
    int presetSelection = 0;
    int presetApplyTarget = 0;
    juce::String snapshotA;
    juce::String snapshotB;
    juce::String snapshotC;
    juce::String snapshotD;
    bool darkTheme = true;
    int themeMode = 0;
    int correlationPairIndex = 0;
    int correlationChannelCount = 0;
    std::vector<std::pair<int, int>> correlationPairs;
    std::atomic<int> learnedMidiCC { -1 };

    static juce::String sharedStateClipboard;
    juce::String favoritePresets;

    double lastSampleRate = 0.0;
    int lastMaxBlockSize = 0;
    uint64_t lastSnapshotHash = 0;
    int snapshotTick = 0;
    int lastLinearRebuildTick = -100;
    int lastLinearPhaseMode = 0;
    int lastLinearQuality = 0;
    int lastLinearWindow = 0;
    juce::ThreadPool linearPhasePool { 1 };
    std::atomic<bool> linearJobRunning { false };
    std::atomic<int> pendingLatencySamples { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQProAudioProcessor)
};
