# SM Studio – linear digital audio workstation
# Copyright (C) 2026 SimpleMusic contributors
# SPDX-License-Identifier: AGPL-3.0-or-later

cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(SMStudio VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows: link the C++ runtime statically so the .exe runs without installing
# the Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(SMS_BUILD_TESTS "Build the unit tests" ON)

# ---------------------------------------------------------------------------
# JUCE (AGPLv3). For offline/local builds pass
#   -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.6
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# Shared compile settings for all SM Studio targets
add_library(sms_options INTERFACE)
target_compile_definitions(sms_options INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0)
if(MSVC)
    target_compile_options(sms_options INTERFACE /W4 /permissive- /utf-8)
else()
    target_compile_options(sms_options INTERFACE -Wall -Wextra -Wno-unused-parameter)
endif()

# All JUCE modules are compiled exactly once into this static library.
# Every SM Studio target links against it instead of the JUCE modules directly
# (avoids duplicate symbols when several targets use JUCE).
add_library(sms_juce STATIC)
target_link_libraries(sms_juce
    PRIVATE
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        sms_options
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
target_compile_definitions(sms_juce PUBLIC
    JUCE_STANDALONE_APPLICATION=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=$<BOOL:${APPLE}>
    JUCE_PLUGINHOST_LV2=0
    JUCE_PLUGINHOST_LADSPA=0)
target_include_directories(sms_juce INTERFACE
    $<TARGET_PROPERTY:sms_juce,INCLUDE_DIRECTORIES>)
target_compile_definitions(sms_juce INTERFACE
    $<TARGET_PROPERTY:sms_juce,COMPILE_DEFINITIONS>)
set_target_properties(sms_juce PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_subdirectory(src/core)
add_subdirectory(src/engine)
add_subdirectory(src/app)

if(SMS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
