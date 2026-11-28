// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Model.h"

#include <algorithm>

namespace sms
{

const Region* Track::findRegion (ObjectId rid) const
{
    for (auto& r : regions)
        if (r.id == rid)
            return &r;
    return nullptr;
}

Region* Track::findRegion (ObjectId rid)
{
    for (auto& r : regions)
        if (r.id == rid)
            return &r;
    return nullptr;
}

//==============================================================================
Song::Song() = default;

const Track* Song::findTrack (ObjectId id) const
{
    for (auto& t : tracks)
        if (t.id == id)
            return &t;
    return nullptr;
}

Track* Song::findTrackMutable (ObjectId id)
{
    for (auto& t : tracks)
        if (t.id == id)
            return &t;
    return nullptr;
}

int Song::indexOfTrack (ObjectId id) const
{
    for (size_t i = 0; i < tracks.size(); ++i)
        if (tracks[i].id == id)
            return (int) i;
    return -1;
}

int Song::insertTrack (Track t, int index)
{
    jassert (t.id.isValid());
    jassert (findTrack (t.id) == nullptr);

    if (index < 0 || index > (int) tracks.size())
        index = (int) tracks.size();

    const auto id = t.id;
    tracks.insert (tracks.begin() + index, std::move (t));
    notify ({ SongChange::What::trackAdded, id });
    return index;
}

std::optional<Track> Song::removeTrack (ObjectId id)
{
    const auto index = indexOfTrack (id);
    if (index < 0)
        return std::nullopt;

    auto t = std::move (tracks[(size_t) index]);
    tracks.erase (tracks.begin() + index);
    notify ({ SongChange::What::trackRemoved, id });
    return t;
}

bool Song::moveTrack (ObjectId id, int newIndex)
{
    const auto index = indexOfTrack (id);
    if (index < 0)
        return false;

    newIndex = juce::jlimit (0, (int) tracks.size() - 1, newIndex);
    if (newIndex == index)
        return false;

    auto t = std::move (tracks[(size_t) index]);
    tracks.erase (tracks.begin() + index);
    tracks.insert (tracks.begin() + newIndex, std::move (t));
    notify ({ SongChange::What::trackMoved, id });
    return true;
}

const AudioSource* Song::findAudioSource (ObjectId id) const
{
    for (auto& s : sources)
        if (s.id == id)
            return &s;
    return nullptr;
}

void Song::addAudioSource (AudioSource s)
{
    jassert (s.id.isValid());
    sources.push_back (std::move (s));
    notify ({ SongChange::What::sourcesChanged, {} });
}

bool Song::removeAudioSource (ObjectId id)
{
    auto it = std::find_if (sources.begin(), sources.end(), [id] (auto& s) { return s.id == id; });
    if (it == sources.end())
        return false;

    sources.erase (it);
    notify ({ SongChange::What::sourcesChanged, {} });
    return true;
}

void Song::setTempo (double bpm)
{
    tempoMap.setTempo (bpm);
    notify ({ SongChange::What::tempoChanged, {} });
}

void Song::setTimeSignature (TimeSignature sig)
{
    tempoMap.setTimeSignature (sig);
    notify ({ SongChange::What::tempoChanged, {} });
}

void Song::setLoop (LoopRange l)
{
    if (l.end < l.start)
        std::swap (l.start, l.end);
    l.start = std::max<Tick> (0, l.start);

    if (l == loop)
        return;

    loop = l;
    notify ({ SongChange::What::loopChanged, {} });
}

const Track* Song::findTrackOfRegion (ObjectId regionId) const
{
    for (auto& t : tracks)
        if (t.findRegion (regionId) != nullptr)
            return &t;
    return nullptr;
}

Tick Song::getSongEnd() const
{
    Tick end = tempoMap.getTicksPerBar() * 8;

    for (auto& t : tracks)
        for (auto& r : t.regions)
            end = std::max (end, r.getEnd());

    return end;
}

void Song::notify (const SongChange& c)
{
    listeners.call ([&] (Listener& l) { l.songChanged (c); });
}

} // namespace sms
