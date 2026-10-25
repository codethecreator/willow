cmake_minimum_required(VERSION 3.22)
project(LunarField VERSION 0.5.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.16-security GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
juce_add_plugin(LunarField
 COMPANY_NAME "Independent Audio"
 BUNDLE_ID "audio.independent.lunarfield"
 PLUGIN_MANUFACTURER_CODE Inau
 PLUGIN_CODE Lufd
 PRODUCT_NAME "LunarField"
 FORMATS VST3 Standalone
 IS_SYNTH FALSE
 NEEDS_MIDI_INPUT FALSE
 NEEDS_MIDI_OUTPUT FALSE
 IS_MIDI_EFFECT FALSE
 VST3_CATEGORIES Fx
 COPY_PLUGIN_AFTER_BUILD FALSE
)
juce_generate_juce_header(LunarField)
target_sources(LunarField PRIVATE Source/Plugin.cpp)
target_compile_definitions(LunarField PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(LunarField PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

add_executable(LunarField_EngineTests Tests/EngineTests.cpp)
target_compile_features(LunarField_EngineTests PRIVATE cxx_std_17)
enable_testing()
add_test(NAME LunarFieldDspTest COMMAND LunarField_EngineTests)

# Exercise the real JUCE processor API during aggressive preset changes.
juce_add_console_app(LunarField_HostSmoke PRODUCT_NAME "LunarField HostSmoke")
target_sources(LunarField_HostSmoke PRIVATE Tests/HostSmoke.cpp Source/Plugin.cpp)
juce_generate_juce_header(LunarField_HostSmoke)
target_compile_definitions(LunarField_HostSmoke PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(LunarField_HostSmoke PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
add_test(NAME LunarFieldHostSmoke COMMAND LunarField_HostSmoke)
