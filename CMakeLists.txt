cmake_minimum_required(VERSION 3.15)

project(EQPro VERSION 1.6.0)

# Set this to your local JUCE path or pass -DJUCE_DIR=... when configuring.
if (NOT DEFINED JUCE_DIR)
    set(JUCE_DIR "${CMAKE_CURRENT_LIST_DIR}/JUCE")
endif()

add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(EQPro
    COMPANY_NAME "DidAudio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE DidA
    PLUGIN_CODE EqpP
    FORMATS VST2 VST3 AU Standalone
    PRODUCT_NAME "EQ Pro"
)

juce_generate_juce_header(EQPro)

juce_add_binary_data(EQProAssets
    SOURCES
        assets/knob-86/knob-86-Filmstrip.png
)

target_sources(EQPro PRIVATE
    src/PluginProcessor.cpp
    src/PluginProcessor.h
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/dsp/Biquad.cpp
    src/dsp/Biquad.h
    src/dsp/EQBand.h
    src/dsp/EQDSP.cpp
    src/dsp/EQDSP.h
    src/dsp/OnePole.cpp
    src/dsp/OnePole.h
    src/dsp/LinearPhaseEQ.cpp
    src/dsp/LinearPhaseEQ.h
    src/dsp/MeteringDSP.cpp
    src/dsp/MeteringDSP.h
    src/dsp/SpectralDynamicsDSP.cpp
    src/dsp/SpectralDynamicsDSP.h
    src/ui/AnalyzerComponent.cpp
    src/ui/AnalyzerComponent.h
    src/ui/BandControlsPanel.cpp
    src/ui/BandControlsPanel.h
    src/ui/LookAndFeel.cpp
    src/ui/LookAndFeel.h
    src/ui/Theme.h
    src/ui/MetersComponent.cpp
    src/ui/MetersComponent.h
    src/ui/SpectralDynamicsPanel.cpp
    src/ui/SpectralDynamicsPanel.h
    src/ui/CorrelationComponent.cpp
    src/ui/CorrelationComponent.h
    src/util/ParamIDs.cpp
    src/util/ParamIDs.h
    src/util/ChannelLayoutUtils.cpp
    src/util/ChannelLayoutUtils.h
    src/util/RingBuffer.cpp
    src/util/RingBuffer.h
    src/util/FFTUtils.h
    src/util/Smoothing.h
    src/util/ColorUtils.cpp
    src/util/ColorUtils.h
    src/util/Version.h
)

target_compile_definitions(EQPro PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1
    EQPRO_VERSION="${PROJECT_VERSION}"
    EQPRO_ITERATION="v1.6 beta"
)

target_link_libraries(EQPro PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_opengl
    EQProAssets
)

target_include_directories(EQPro PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/EQPro_artefacts/JuceLibraryCode
)
