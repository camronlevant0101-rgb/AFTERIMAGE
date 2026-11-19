cmake_minimum_required(VERSION 3.22)
project(AFTERIMAGE VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(AFTERIMAGE_BUILD_PLUGIN "Build the JUCE VST3 / Standalone plugin (downloads JUCE)" ON)
option(AFTERIMAGE_BUILD_TESTS  "Build the native DSP test-suite" ON)

# ---- 1. Reusable DSP core (no JUCE dependency) --------------------------------------
add_library(afterimage_dsp STATIC DSP/GranularEngine.cpp)
target_include_directories(afterimage_dsp PUBLIC DSP)
if(MSVC)
  target_compile_options(afterimage_dsp PRIVATE /W4)
endif()

# ---- 2. Native tests + preset exporter -------------------------------------------------
if(AFTERIMAGE_BUILD_TESTS)
  enable_testing()
  add_executable(afterimage_tests Tests/test_engine.cpp)
  target_link_libraries(afterimage_tests PRIVATE afterimage_dsp)
  find_package(Threads REQUIRED)
  target_link_libraries(afterimage_tests PRIVATE Threads::Threads)
  add_test(NAME afterimage_dsp COMMAND afterimage_tests)

  add_executable(export_presets Tools/export_presets.cpp)
  target_link_libraries(export_presets PRIVATE afterimage_dsp)
endif()

# ---- 3. JUCE plugin ------------------------------------------------------------------------
if(AFTERIMAGE_BUILD_PLUGIN)
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
  FetchContent_MakeAvailable(JUCE)   # JUCE bundles the VST3 SDK (GPLv3/MIT dual) - no separate download

  juce_add_plugin(AFTERIMAGE
    COMPANY_NAME            "AFTERIMAGE Audio"
    PLUGIN_MANUFACTURER_CODE Aftr
    PLUGIN_CODE             Aimg
    FORMATS                 VST3 Standalone
    PRODUCT_NAME            "AFTERIMAGE"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    VST3_CATEGORIES         Fx Modulation
    COPY_PLUGIN_AFTER_BUILD FALSE)

  target_sources(AFTERIMAGE PRIVATE
    Source/PluginProcessor.cpp
    GUI/PluginEditor.cpp)
  target_include_directories(AFTERIMAGE PRIVATE Source GUI DSP)
  target_compile_definitions(AFTERIMAGE PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_STRICT_REFCOUNTEDPOINTER=1)
  target_link_libraries(AFTERIMAGE
    PRIVATE afterimage_dsp juce::juce_audio_utils
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
endif()
