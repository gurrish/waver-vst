cmake_minimum_required(VERSION 3.22)
project(WaverVST VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

# JUCE added as a git submodule: git submodule add https://github.com/juce-framework/JUCE
add_subdirectory(JUCE)

juce_add_plugin(WaverVST
    COMPANY_NAME               "gurrish"
    PLUGIN_MANUFACTURER_CODE   Guri
    PLUGIN_CODE                Wvr1
    FORMATS                    VST3 Standalone
    PRODUCT_NAME               "Waver"
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES            "Fx"
    COPY_PLUGIN_AFTER_BUILD    FALSE
)

# Generates JuceHeader.h in the build tree so Source files can #include it
juce_generate_juce_header(WaverVST)

target_sources(WaverVST PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/PitchShifter.cpp
    Source/DSP/VariableDelay.cpp
)

target_include_directories(WaverVST PRIVATE Source)

target_compile_definitions(WaverVST PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(WaverVST
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
