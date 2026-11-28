# sms_core: the document model (song, tracks, regions), musical time and the
# undoable command system. Depends only on juce_core / juce_data_structures /
# juce_graphics (for juce::Colour) – never on GUI or audio device code.

add_library(sms_core STATIC
    Ids.h
    MusicalTime.h
    MusicalTime.cpp
    Model.h
    Model.cpp
    Command.h
    Command.cpp
    Commands.h
    Commands.cpp
    Selection.h
    ValueFormat.h
    ValueFormat.cpp)

target_include_directories(sms_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(sms_core PUBLIC sms_juce)
