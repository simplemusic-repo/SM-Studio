// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "extension/ExtensionApi.h"

namespace sms
{

/** Creates (but does not add) a new track of a registered type with a fresh id,
    a default name ("Audio 3") and the next colour from the palette. */
Track createTrack (AppContext&, const juce::String& typeId, const juce::String& name = {});

/** Adds a track of the given type through the undo system and selects it.
    Returns the new track id. */
ObjectId addTrack (AppContext&, const juce::String& typeId, int index = -1, const juce::String& name = {});

/** True for files the audio format manager can read. */
bool isSupportedAudioFile (const juce::File&);

/** Imports an audio file as a region (undoable, one step).
    Drops on an audio track add the region there; otherwise a new audio
    track is created at target.insertIndex. */
bool importAudioFile (AppContext&, const juce::File&, const DropTarget&);

/** Fills the song with example tracks (used for screenshots and first tests). */
void createDemoSong (AppContext&);

} // namespace sms
