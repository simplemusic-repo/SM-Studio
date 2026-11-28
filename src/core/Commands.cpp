// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Commands.h"

#include <algorithm>

namespace sms::commands
{

//==============================================================================
bool AddTrack::perform (Song& s)
{
    if (s.findTrack (track.id) != nullptr)
        return false;

    s.insertTrack (track, requestedIndex);
    return true;
}

void AddTrack::undo (Song& s)
{
    if (auto t = s.removeTrack (track.id))
        track = std::move (*t); // keep any state that changed while it existed
}

//==============================================================================
bool RemoveTrack::perform (Song& s)
{
    index = s.indexOfTrack (trackId);
    removed = s.removeTrack (trackId);
    return removed.has_value();
}

void RemoveTrack::undo (Song& s)
{
    if (removed)
    {
        s.insertTrack (std::move (*removed), index);
        removed.reset();
    }
}

//==============================================================================
bool MoveTrack::perform (Song& s)
{
    fromIndex = s.indexOfTrack (trackId);
    return fromIndex >= 0 && s.moveTrack (trackId, toIndex);
}

void MoveTrack::undo (Song& s)
{
    s.moveTrack (trackId, fromIndex);
}

//==============================================================================
bool RenameTrack::perform (Song& s)
{
    auto* t = s.findTrack (trackId);
    if (t == nullptr || t->name == name)
        return false;

    oldName = t->name;
    return s.modifyTrack (trackId, [this] (Track& tr) { tr.name = name; });
}

void RenameTrack::undo (Song& s)
{
    s.modifyTrack (trackId, [this] (Track& tr) { tr.name = oldName; });
}

//==============================================================================
bool SetTrackColour::perform (Song& s)
{
    auto* t = s.findTrack (trackId);
    if (t == nullptr || t->colour == colour)
        return false;

    oldColour = t->colour;
    return s.modifyTrack (trackId, [this] (Track& tr) { tr.colour = colour; });
}

void SetTrackColour::undo (Song& s)
{
    s.modifyTrack (trackId, [this] (Track& tr) { tr.colour = oldColour; });
}

//==============================================================================
float SetTrackParam::get (const Track& t, Param p)
{
    switch (p)
    {
        case Param::volumeDb: return t.volumeDb;
        case Param::pan:      return t.pan;
        case Param::mute:     return t.mute ? 1.0f : 0.0f;
        case Param::solo:     return t.solo ? 1.0f : 0.0f;
        case Param::armed:    return t.armed ? 1.0f : 0.0f;
        case Param::transpose:      return (float) t.transpose;
        case Param::velocityOffset: return (float) t.velocityOffset;
    }
    return 0.0f;
}

void SetTrackParam::set (Track& t, Param p, float v)
{
    switch (p)
    {
        case Param::volumeDb: t.volumeDb = juce::jlimit (Track::minVolumeDb, Track::maxVolumeDb, v); break;
        case Param::pan:      t.pan = juce::jlimit (-1.0f, 1.0f, v); break;
        case Param::mute:     t.mute = v >= 0.5f; break;
        case Param::solo:     t.solo = v >= 0.5f; break;
        case Param::armed:    t.armed = v >= 0.5f; break;
        case Param::transpose:      t.transpose = juce::jlimit (-48, 48, juce::roundToInt (v)); break;
        case Param::velocityOffset: t.velocityOffset = juce::jlimit (-127, 127, juce::roundToInt (v)); break;
    }
}

juce::String SetTrackParam::getName() const
{
    switch (param)
    {
        case Param::volumeDb: return u8"Lautstärke ändern";
        case Param::pan:      return u8"Panorama ändern";
        case Param::mute:     return "Stummschaltung";
        case Param::solo:     return "Solo";
        case Param::armed:    return "Aufnahmebereitschaft";
        case Param::transpose:      return "Transponieren";
        case Param::velocityOffset: return u8"Anschlagstärke";
    }
    return {};
}

bool SetTrackParam::perform (Song& s)
{
    auto* t = s.findTrack (trackId);
    if (t == nullptr)
        return false;

    Track probe;
    set (probe, param, value);
    if (juce::exactlyEqual (get (*t, param), get (probe, param)))
        return false;

    oldValue = get (*t, param);
    return s.modifyTrack (trackId, [this] (Track& tr) { set (tr, param, value); });
}

void SetTrackParam::undo (Song& s)
{
    s.modifyTrack (trackId, [this] (Track& tr) { set (tr, param, oldValue); });
}

bool SetTrackParam::mergeWith (const Command& next)
{
    if (auto* n = dynamic_cast<const SetTrackParam*> (&next))
    {
        if (n->trackId == trackId && n->param == param)
        {
            value = n->value; // keep our oldValue, adopt the newest value
            return true;
        }
    }
    return false;
}

//==============================================================================
bool SetTempo::perform (Song& s)
{
    oldTempo = s.getTempoMap().getTempo();
    const auto clamped = juce::jlimit (TempoMap::minTempo, TempoMap::maxTempo, tempo);
    if (juce::approximatelyEqual (clamped, oldTempo))
        return false;

    s.setTempo (clamped);
    return true;
}

void SetTempo::undo (Song& s)
{
    s.setTempo (oldTempo);
}

bool SetTempo::mergeWith (const Command& next)
{
    if (auto* n = dynamic_cast<const SetTempo*> (&next))
    {
        tempo = n->tempo;
        return true;
    }
    return false;
}

//==============================================================================
bool SetTimeSignature::perform (Song& s)
{
    oldSig = s.getTempoMap().getTimeSignature();
    if (oldSig == sig)
        return false;

    s.setTimeSignature (sig);
    return true;
}

void SetTimeSignature::undo (Song& s)
{
    s.setTimeSignature (oldSig);
}

//==============================================================================
bool AddAudioSource::perform (Song& s)
{
    if (s.findAudioSource (source.id) != nullptr)
        return false;

    s.addAudioSource (source);
    return true;
}

void AddAudioSource::undo (Song& s)
{
    s.removeAudioSource (source.id);
}

//==============================================================================
bool AddRegion::perform (Song& s)
{
    return s.modifyTrack (trackId, [this] (Track& t) { t.regions.push_back (region); },
                          SongChange::What::regionsChanged);
}

void AddRegion::undo (Song& s)
{
    s.modifyTrack (trackId, [this] (Track& t)
    {
        auto& rs = t.regions;
        rs.erase (std::remove_if (rs.begin(), rs.end(), [this] (auto& r) { return r.id == region.id; }), rs.end());
    }, SongChange::What::regionsChanged);
}

//==============================================================================
bool RemoveRegion::perform (Song& s)
{
    auto* t = s.findTrack (trackId);
    if (t == nullptr || t->findRegion (regionId) == nullptr)
        return false;

    return s.modifyTrack (trackId, [this] (Track& tr)
    {
        for (size_t i = 0; i < tr.regions.size(); ++i)
        {
            if (tr.regions[i].id == regionId)
            {
                index = i;
                removed = std::move (tr.regions[i]);
                tr.regions.erase (tr.regions.begin() + (std::ptrdiff_t) i);
                break;
            }
        }
    }, SongChange::What::regionsChanged);
}

void RemoveRegion::undo (Song& s)
{
    if (! removed)
        return;

    s.modifyTrack (trackId, [this] (Track& tr)
    {
        const auto i = std::min (index, tr.regions.size());
        tr.regions.insert (tr.regions.begin() + (std::ptrdiff_t) i, std::move (*removed));
    }, SongChange::What::regionsChanged);
    removed.reset();
}

//==============================================================================
bool UpdateRegion::perform (Song& s)
{
    auto* src = s.findTrack (from);
    auto* dst = s.findTrack (to);
    if (src == nullptr || dst == nullptr)
        return false;

    auto* current = src->findRegion (region.id);
    if (current == nullptr)
        return false;

    if (from == to && current->position == region.position && current->length == region.length
        && current->muted == region.muted && current->sourceOffsetSamples == region.sourceOffsetSamples
        && current->notes.size() == region.notes.size() && current->name == region.name)
    {
        bool sameNotes = true;
        for (size_t i = 0; i < region.notes.size(); ++i)
            sameNotes = sameNotes && current->notes[i].start == region.notes[i].start;
        if (sameNotes)
            return false;
    }

    s.modifyTrack (from, [this] (Track& t)
    {
        for (size_t i = 0; i < t.regions.size(); ++i)
        {
            if (t.regions[i].id == region.id)
            {
                oldIndex = i;
                old = std::move (t.regions[i]);
                t.regions.erase (t.regions.begin() + (std::ptrdiff_t) i);
                break;
            }
        }
    }, SongChange::What::regionsChanged);

    s.modifyTrack (to, [this] (Track& t)
    {
        const auto i = std::min (oldIndex, t.regions.size());
        t.regions.insert (t.regions.begin() + (std::ptrdiff_t) i, region);
    }, SongChange::What::regionsChanged);

    return true;
}

void UpdateRegion::undo (Song& s)
{
    if (! old)
        return;

    s.modifyTrack (to, [this] (Track& t)
    {
        auto& rs = t.regions;
        rs.erase (std::remove_if (rs.begin(), rs.end(), [this] (auto& r) { return r.id == region.id; }), rs.end());
    }, SongChange::What::regionsChanged);

    s.modifyTrack (from, [this] (Track& t)
    {
        const auto i = std::min (oldIndex, t.regions.size());
        t.regions.insert (t.regions.begin() + (std::ptrdiff_t) i, std::move (*old));
    }, SongChange::What::regionsChanged);

    old.reset();
}

//==============================================================================
bool SetInstrument::perform (Song& s)
{
    auto* t = s.findTrack (trackId);
    if (t == nullptr || (t->instrumentPluginId == newId && t->instrumentName == newName))
        return false;

    oldId = t->instrumentPluginId;
    oldName = t->instrumentName;
    return s.modifyTrack (trackId, [this] (Track& tr) { tr.instrumentPluginId = newId; tr.instrumentName = newName; });
}

void SetInstrument::undo (Song& s)
{
    s.modifyTrack (trackId, [this] (Track& tr) { tr.instrumentPluginId = oldId; tr.instrumentName = oldName; });
}

//==============================================================================
bool AddInsert::perform (Song& s)
{
    return s.modifyTrack (trackId, [this] (Track& t)
    {
        auto i = requestedIndex < 0 ? t.inserts.size() : std::min ((size_t) requestedIndex, t.inserts.size());
        t.inserts.insert (t.inserts.begin() + (std::ptrdiff_t) i, slot);
    });
}

void AddInsert::undo (Song& s)
{
    s.modifyTrack (trackId, [this] (Track& t)
    {
        auto& v = t.inserts;
        v.erase (std::remove_if (v.begin(), v.end(), [this] (auto& x) { return x.id == slot.id; }), v.end());
    });
}

//==============================================================================
bool SetInsertBypass::set (Song& s, bool value)
{
    auto* t = s.findTrack (trackId);
    if (t == nullptr)
        return false;
    auto it = std::find_if (t->inserts.begin(), t->inserts.end(), [this] (auto& x) { return x.id == slotId; });
    if (it == t->inserts.end() || it->bypassed == value)
        return false;

    return s.modifyTrack (trackId, [this, value] (Track& tr)
    {
        for (auto& x : tr.inserts)
            if (x.id == slotId)
                x.bypassed = value;
    });
}

bool SetInsertBypass::perform (Song& s) { return set (s, bypass); }
void SetInsertBypass::undo (Song& s)    { set (s, ! bypass); }

//==============================================================================
bool RemoveInsert::perform (Song& s)
{
    auto* t = s.findTrack (trackId);
    if (t == nullptr)
        return false;

    auto it = std::find_if (t->inserts.begin(), t->inserts.end(), [this] (auto& x) { return x.id == slotId; });
    if (it == t->inserts.end())
        return false;

    return s.modifyTrack (trackId, [this] (Track& tr)
    {
        for (size_t i = 0; i < tr.inserts.size(); ++i)
        {
            if (tr.inserts[i].id == slotId)
            {
                index = i;
                removed = std::move (tr.inserts[i]);
                tr.inserts.erase (tr.inserts.begin() + (std::ptrdiff_t) i);
                break;
            }
        }
    });
}

void RemoveInsert::undo (Song& s)
{
    if (! removed)
        return;

    s.modifyTrack (trackId, [this] (Track& tr)
    {
        const auto i = std::min (index, tr.inserts.size());
        tr.inserts.insert (tr.inserts.begin() + (std::ptrdiff_t) i, std::move (*removed));
    });
    removed.reset();
}

} // namespace sms::commands
