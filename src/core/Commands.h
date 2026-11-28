// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Command.h"

namespace sms::commands
{

//==============================================================================
class AddTrack final : public Command
{
public:
    /** The track must already have a valid id (Song::newId()). index < 0 appends. */
    AddTrack (Track t, int index = -1) : track (std::move (t)), requestedIndex (index) {}

    juce::String getName() const override { return u8"Spur hinzufügen"; }
    bool perform (Song&) override;
    void undo (Song&) override;

    ObjectId getTrackId() const noexcept { return track.id; }

private:
    Track track;
    int requestedIndex;
};

class RemoveTrack final : public Command
{
public:
    explicit RemoveTrack (ObjectId id) : trackId (id) {}

    juce::String getName() const override { return u8"Spur löschen"; }
    bool perform (Song&) override;
    void undo (Song&) override;

private:
    ObjectId trackId;
    std::optional<Track> removed;
    int index = -1;
};

class MoveTrack final : public Command
{
public:
    MoveTrack (ObjectId id, int newIndex) : trackId (id), toIndex (newIndex) {}

    juce::String getName() const override { return "Spur verschieben"; }
    bool perform (Song&) override;
    void undo (Song&) override;

private:
    ObjectId trackId;
    int toIndex, fromIndex = -1;
};

class RenameTrack final : public Command
{
public:
    RenameTrack (ObjectId id, juce::String newName) : trackId (id), name (std::move (newName)) {}

    juce::String getName() const override { return "Spur umbenennen"; }
    bool perform (Song&) override;
    void undo (Song&) override;

private:
    ObjectId trackId;
    juce::String name, oldName;
};

class SetTrackColour final : public Command
{
public:
    SetTrackColour (ObjectId id, juce::Colour c) : trackId (id), colour (c) {}

    juce::String getName() const override { return u8"Spurfarbe ändern"; }
    bool perform (Song&) override;
    void undo (Song&) override;

private:
    ObjectId trackId;
    juce::Colour colour, oldColour;
};

/** Mixer-type values of a track. Mergeable for fader/knob gestures. */
class SetTrackParam final : public Command
{
public:
    enum class Param { volumeDb, pan, mute, solo, armed, transpose, velocityOffset };

    SetTrackParam (ObjectId id, Param p, float v) : trackId (id), param (p), value (v) {}

    juce::String getName() const override;
    bool perform (Song&) override;
    void undo (Song&) override;
    bool mergeWith (const Command&) override;

    static float get (const Track&, Param);
    static void set (Track&, Param, float);

private:
    ObjectId trackId;
    Param param;
    float value, oldValue = 0.0f;
};

//==============================================================================
class SetTempo final : public Command
{
public:
    explicit SetTempo (double bpm) : tempo (bpm) {}

    juce::String getName() const override { return u8"Tempo ändern"; }
    bool perform (Song&) override;
    void undo (Song&) override;
    bool mergeWith (const Command&) override;

private:
    double tempo, oldTempo = 120.0;
};

class SetTimeSignature final : public Command
{
public:
    explicit SetTimeSignature (TimeSignature s) : sig (s) {}

    juce::String getName() const override { return u8"Taktart ändern"; }
    bool perform (Song&) override;
    void undo (Song&) override;

private:
    TimeSignature sig, oldSig;
};

//==============================================================================
class AddAudioSource final : public Command
{
public:
    explicit AddAudioSource (AudioSource s) : source (std::move (s)) {}

    juce::String getName() const override { return u8"Audiodatei hinzufügen"; }
    bool perform (Song&) override;
    void undo (Song&) override;

private:
    AudioSource source;
};

class AddRegion final : public Command
{
public:
    AddRegion (ObjectId track, Region r) : trackId (track), region (std::move (r)) {}

    juce::String getName() const override { return u8"Event hinzufügen"; }
    bool perform (Song&) override;
    void undo (Song&) override;

private:
    ObjectId trackId;
    Region region;
};

class RemoveRegion final : public Command
{
public:
    RemoveRegion (ObjectId track, ObjectId region) : trackId (track), regionId (region) {}

    juce::String getName() const override { return u8"Event löschen"; }
    bool perform (Song&) override;
    void undo (Song&) override;

private:
    ObjectId trackId, regionId;
    std::optional<Region> removed;
    size_t index = 0;
};

/** Replaces a region with a new state – optionally moving it to another
    track (same id). Used for move, resize, mute, and split (left part). */
class UpdateRegion final : public Command
{
public:
    UpdateRegion (ObjectId fromTrack, ObjectId toTrack, Region newState, juce::String name)
        : from (fromTrack), to (toTrack), region (std::move (newState)), commandName (std::move (name)) {}

    juce::String getName() const override { return commandName; }
    bool perform (Song&) override;
    void undo (Song&) override;

private:
    ObjectId from, to;
    Region region;
    std::optional<Region> old;
    size_t oldIndex = 0;
    juce::String commandName;
};

//==============================================================================
/** Sets (or replaces) the instrument of an instrument track. */
class SetInstrument final : public Command
{
public:
    SetInstrument (ObjectId track, juce::String pluginId, juce::String name)
        : trackId (track), newId (std::move (pluginId)), newName (std::move (name)) {}

    juce::String getName() const override { return u8"Instrument ändern"; }
    bool perform (Song&) override;
    void undo (Song&) override;

private:
    ObjectId trackId;
    juce::String newId, newName, oldId, oldName;
};

class AddInsert final : public Command
{
public:
    /** index < 0 appends to the end of the insert chain. */
    AddInsert (ObjectId track, InsertSlot s, int index = -1) : trackId (track), slot (std::move (s)), requestedIndex (index) {}

    juce::String getName() const override { return u8"Effekt einfügen"; }
    bool perform (Song&) override;
    void undo (Song&) override;

private:
    ObjectId trackId;
    InsertSlot slot;
    int requestedIndex;
};

/** Replaces an insert with another plug-in at the same position (new slot id). */
class ReplaceInsert final : public Command
{
public:
    ReplaceInsert (ObjectId track, ObjectId oldSlot, InsertSlot newSlot)
        : trackId (track), oldSlotId (oldSlot), slot (std::move (newSlot)) {}

    juce::String getName() const override { return "Effekt ersetzen"; }
    bool perform (Song&) override;
    void undo (Song&) override;

private:
    ObjectId trackId, oldSlotId;
    InsertSlot slot;
    std::optional<InsertSlot> old;
};

class SetInsertBypass final : public Command
{
public:
    SetInsertBypass (ObjectId track, ObjectId slot, bool b) : trackId (track), slotId (slot), bypass (b) {}

    juce::String getName() const override { return "Effekt-Bypass"; }
    bool perform (Song&) override;
    void undo (Song&) override;

private:
    bool set (Song&, bool);
    ObjectId trackId, slotId;
    bool bypass;
};

class RemoveInsert final : public Command
{
public:
    RemoveInsert (ObjectId track, ObjectId slot) : trackId (track), slotId (slot) {}

    juce::String getName() const override { return "Effekt entfernen"; }
    bool perform (Song&) override;
    void undo (Song&) override;

private:
    ObjectId trackId, slotId;
    std::optional<InsertSlot> removed;
    size_t index = 0;
};

} // namespace sms::commands
