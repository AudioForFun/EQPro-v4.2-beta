#pragma once

#include <JuceHeader.h>
#include "../util/RingBuffer.h"
#include "../util/ParamIDs.h"
#include "Theme.h"

class EQProAudioProcessor;

class AnalyzerComponent final : public juce::Component,
                                private juce::Timer
{
public:
    explicit AnalyzerComponent(EQProAudioProcessor& processor);

    void setSelectedBand(int bandIndex);
    void setSelectedChannel(int channelIndex);
    void setTheme(const ThemeColors& newTheme);
    void setUiScale(float scale);
    void setInteractive(bool shouldAllow);
    void invalidateCaches();
    int getTimerHz() const noexcept { return lastTimerHz; }

    std::function<void(int)> onBandSelected;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    void updateFft();
    void updateCurves();
    juce::Rectangle<int> getPlotArea() const;
    juce::Rectangle<int> getMagnitudeArea() const;
    void drawLabels(juce::Graphics& g, const juce::Rectangle<int>& area);
    float getMaxFreq() const;

    float xToFrequency(float x) const;
    float yToGain(float y) const;
    float frequencyToX(float freq) const;
    float gainToY(float gainDb) const;
    float snapFrequencyToPeak(float x) const;
    void createBandAtPosition(const juce::Point<float>& position);
    void resetBandToDefaults(int bandIndex, bool shouldBypass);
    void startAltSolo(const juce::Point<float>& position);
    void updateAltSolo(const juce::Point<float>& position);
    void stopAltSolo();

    void setBandParameter(int bandIndex, const juce::String& suffix, float value);
    float getBandParameter(int bandIndex, const juce::String& suffix) const;
    float getBandDynamicGainDb(int bandIndex) const;
    bool getBandBypassed(int bandIndex) const;
    int getBandType(int bandIndex) const;

    std::complex<double> computeBandResponse(int bandIndex, float frequency) const;

    EQProAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& parameters;
    AudioFifo& externalFifo;

    int selectedBand = 0;
    int selectedChannel = 0;
    int draggingBand = -1;
    int tempSoloBand = -1;
    bool tempSoloWasEnabled = false;
    int hoverBand = -1;
    juce::Point<float> hoverPos;
    bool isAltSoloing = false;
    int altSoloBand = -1;
    struct AltSoloState
    {
        float freqNorm = 0.0f;
        float gainNorm = 0.0f;
        float qNorm = 0.0f;
        float typeNorm = 0.0f;
        float bypassNorm = 0.0f;
        float soloNorm = 0.0f;
    };
    AltSoloState altSoloState {};
    bool draggingQ = false;
    int qDragSide = 0;
    float qDragStart = 1.0f;
    juce::Point<float> dragStartPos;
    struct DragBandState
    {
        int band = 0;
        float freq = 0.0f;
        float gain = 0.0f;
    };
    std::vector<int> selectedBands;
    std::vector<DragBandState> dragBands;
    bool allowInteraction = false;

    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int fftBins = fftSize / 2;

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, fftSize> timeBuffer {};
    std::array<float, fftSize * 2> fftDataPre {};
    std::array<float, fftSize * 2> fftDataPost {};

    std::array<float, fftBins> preMagnitudes {};
    std::array<float, fftBins> postMagnitudes {};
    std::array<float, fftBins> externalMagnitudes {};

    std::vector<float> eqCurveDb;
    std::vector<float> selectedBandCurveDb;
    std::vector<std::vector<float>> perBandCurveDb;
    std::vector<bool> perBandActive;
    std::vector<juce::Point<float>> bandPoints;
    std::vector<juce::Rectangle<float>> bypassIcons;
    std::array<juce::Rectangle<float>, 2> qHandleRects {};
    bool hasQHandles = false;

    float lastSampleRate = 48000.0f;
    int frameCounter = 0;
    float minDb = -60.0f;
    float maxDb = 60.0f;
    int analyzerSpeedIndex = -1;
    int lastTimerHz = 0;
    int lastCurveWidth = 0;
    std::vector<uint64_t> perBandCurveHash;
    uint64_t lastCurveHash = 0;
    int lastCurveBand = -1;
    int lastCurveChannel = -1;
    float uiScale = 1.0f;
    ThemeColors theme = makeDarkTheme();
};
