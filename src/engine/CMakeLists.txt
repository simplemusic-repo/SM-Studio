# sms_engine: audio device handling, transport, audio file pool and rendering.

add_library(sms_engine STATIC
    AudioEngine.h
    AudioEngine.cpp
    AudioPool.h
    AudioPool.cpp
    EngineSync.h
    EngineSync.cpp
    Metronome.h
    Metronome.cpp
    RenderSnapshot.h
    RenderSnapshot.cpp)

target_include_directories(sms_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(sms_engine PUBLIC sms_core sms_juce)
