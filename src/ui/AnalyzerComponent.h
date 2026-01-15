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
    void setShowPhase(bool shouldShow);
    void setTheme(const ThemeColors& newTheme);

    std::function<void(int)> onBandSelected;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    void updateFft();
    void updateCurves();
    juce::Rectangle<int> getPlotArea() const;
    juce::Rectangle<int> getMagnitudeArea() const;
    juce::Rectangle<int> getPhaseArea() const;
    void drawLabels(juce::Graphics& g, const juce::Rectangle<int>& area);

    float xToFrequency(float x) const;
    float yToGain(float y) const;
    float frequencyToX(float freq) const;
    float gainToY(float gainDb) const;
    float phaseToY(float phase) const;
    float snapFrequencyToPeak(float x) const;

    void setBandParameter(int bandIndex, const juce::String& suffix, float value);
    float getBandParameter(int bandIndex, const juce::String& suffix) const;
    bool getBandBypassed(int bandIndex) const;
    int getBandType(int bandIndex) const;

    std::complex<double> computeBandResponse(int bandIndex, float frequency) const;

    EQProAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& parameters;
    AudioFifo& externalFifo;

    int selectedBand = 0;
    int selectedChannel = 0;
    int draggingBand = -1;
    juce::Point<float> dragStartPos;
    struct DragBandState
    {
        int band = 0;
        float freq = 0.0f;
        float gain = 0.0f;
    };
    std::vector<int> selectedBands;
    std::vector<DragBandState> dragBands;

    static constexpr int fftOrder = 11;
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
    std::vector<float> phaseCurve;
    std::vector<float> selectedBandCurveDb;
    std::vector<juce::Point<float>> bandPoints;

    float lastSampleRate = 48000.0f;
    int frameCounter = 0;
    bool showPhase = true;
    float minDb = -24.0f;
    float maxDb = 24.0f;
    int analyzerSpeedIndex = -1;
    uint64_t lastCurveHash = 0;
    int lastCurveBand = -1;
    int lastCurveChannel = -1;
    ThemeColors theme = makeDarkTheme();
};
