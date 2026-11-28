// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Ids.h"
#include "MusicalTime.h"

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

#include <optional>
#include <vector>

namespace sms
{

//==============================================================================
/** An audio file used by the song. Regions reference it by ID.
    Corresponds to an ARA "audio source". */
struct AudioSource
{
    ObjectId id;
    juce::File file;
    double sampleRate = 0.0;
    std::int64_t lengthInSamples = 0;
    int numChannels = 0;
};

/** A MIDI note inside a MIDI region. Position is relative to the region start. */
struct Note
{
    ObjectId id;
    Tick start = 0;
    Tick length = ticksPerQuarter;
    int pitch = 60;       // MIDI note number
    float velocity = 0.8f; // 0..1
};

/** An event on a track's timeline (Studio One calls these "events").
    Audio regions play a window of an AudioSource (non-destructive), MIDI
    regions contain notes. Corresponds to an ARA "playback region". */
struct Region
{
    enum class Kind { audio, midi };

    ObjectId id;
    Kind kind = Kind::midi;
    juce::String name;
    Tick position = 0; // song position of the region start
    Tick length = 0;
    bool muted = false;

    // audio
    ObjectId sourceId;
    std::int64_t sourceOffsetSamples = 0;
    float gainDb = 0.0f;

    // midi
    std::vector<Note> notes;

    Tick getEnd() const noexcept { return position + length; }
};

/** A plug-in instance in a track's insert chain. pluginId is the identifier
    used by the plug-in host module (empty until plug-in hosting exists). */
struct InsertSlot
{
    ObjectId id;
    juce::String pluginId;
    juce::String name;
    bool bypassed = false;
};

//==============================================================================
struct Track
{
    ObjectId id;
    juce::String typeId; // registered track type, e.g. "audio", "instrument"
    juce::String name;
    juce::Colour colour;

    float volumeDb = 0.0f; // -inf .. +10
    float pan = 0.0f;      // -1 .. +1
    bool mute = false;
    bool solo = false;
    bool armed = false;

    juce::String instrumentPluginId; // instrument tracks only
    juce::String instrumentName;

    std::vector<InsertSlot> inserts;
    std::vector<Region> regions;

    const Region* findRegion (ObjectId) const;
    Region* findRegion (ObjectId);

    static constexpr float minVolumeDb = -96.0f;
    static constexpr float maxVolumeDb = 10.0f;
};

//==============================================================================
/** Loop / cycle range of the transport. */
struct LoopRange
{
    Tick start = 0;
    Tick end = 0;
    bool enabled = false;

    bool isValid() const noexcept { return end > start; }
    bool operator== (const LoopRange&) const = default;
};

//==============================================================================
/** What changed in a song – passed to listeners. */
struct SongChange
{
    enum class What
    {
        trackAdded,
        trackRemoved,
        trackMoved,
        trackChanged,   // name, colour, mixer values, inserts ...
        regionsChanged, // regions of one track
        tempoChanged,
        sourcesChanged,
        loopChanged,    // loop range or loop on/off (not undoable)
        reset           // everything (e.g. project loaded)
    };

    What what;
    ObjectId id; // track id where applicable
};

//==============================================================================
/** The document. Owns tracks, audio sources and the tempo map.

    Threading: only the message thread may read or modify a Song. The audio
    engine never touches it directly; it receives immutable render snapshots.

    Undo: UI code must not call the mutating functions directly. Changes are
    made through Commands (see Command.h) so they can be undone. The mutators
    below are the low-level primitives those commands use.
*/
class Song
{
public:
    Song();

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void songChanged (const SongChange&) = 0;
    };

    void addListener (Listener* l) { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    //==========================================================================
    IdGenerator& getIds() noexcept { return ids; }
    ObjectId newId() noexcept { return ids.next(); }

    juce::String getTitle() const { return title; }
    void setTitle (const juce::String& t) { title = t; }

    //==========================================================================
    const std::vector<Track>& getTracks() const noexcept { return tracks; }
    int getNumTracks() const noexcept { return (int) tracks.size(); }

    const Track* findTrack (ObjectId) const;
    int indexOfTrack (ObjectId) const;

    /** Inserts a track (index < 0 or past the end appends). Returns the index used. */
    int insertTrack (Track, int index);

    /** Removes and returns the track. */
    std::optional<Track> removeTrack (ObjectId);

    bool moveTrack (ObjectId, int newIndex);

    /** Applies a modification to a track and notifies listeners. */
    template <typename Fn>
    bool modifyTrack (ObjectId id, Fn&& fn, SongChange::What what = SongChange::What::trackChanged)
    {
        if (auto* t = findTrackMutable (id))
        {
            fn (*t);
            notify ({ what, id });
            return true;
        }
        return false;
    }

    //==========================================================================
    const std::vector<AudioSource>& getAudioSources() const noexcept { return sources; }
    const AudioSource* findAudioSource (ObjectId) const;
    void addAudioSource (AudioSource);
    bool removeAudioSource (ObjectId);

    //==========================================================================
    const TempoMap& getTempoMap() const noexcept { return tempoMap; }
    void setTempo (double bpm);
    void setTimeSignature (TimeSignature);

    /** Loop range. Changed directly (not undoable, like in most DAWs). */
    const LoopRange& getLoop() const noexcept { return loop; }
    void setLoop (LoopRange);

    /** Finds the track containing a region. */
    const Track* findTrackOfRegion (ObjectId regionId) const;

    /** End of the last region (at least 8 bars) – used for scroll range and export. */
    Tick getSongEnd() const;

    /** Notifies listeners that everything changed. */
    void notifyReset() { notify ({ SongChange::What::reset, {} }); }

private:
    Track* findTrackMutable (ObjectId);
    void notify (const SongChange&);

    juce::String title { "Unbenannt" };
    IdGenerator ids;
    std::vector<Track> tracks;
    std::vector<AudioSource> sources;
    TempoMap tempoMap;
    LoopRange loop;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE (Song)
};

} // namespace sms
