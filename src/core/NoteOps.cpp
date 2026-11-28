// SPDX-License-Identifier: AGPL-3.0-or-later
#include "NoteOps.h"

#include <algorithm>
#include <cmath>

namespace sms::noteops
{

void sort (std::vector<Note>& notes)
{
    std::stable_sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b)
    {
        return a.start != b.start ? a.start < b.start : a.pitch < b.pitch;
    });
}

std::vector<Note> move (std::vector<Note> notes, const IdSet& ids, Tick dTicks, int dPitch)
{
    for (auto& n : notes)
    {
        if (ids.count (n.id) == 0)
            continue;
        n.start = std::max<Tick> (0, n.start + dTicks);
        n.pitch = std::clamp (n.pitch + dPitch, 0, 127);
    }
    sort (notes);
    return notes;
}

std::vector<Note> resize (std::vector<Note> notes, const IdSet& ids, Tick dTicks, Tick minLength)
{
    for (auto& n : notes)
        if (ids.count (n.id) > 0)
            n.length = std::max<Tick> (std::max<Tick> (1, minLength), n.length + dTicks);
    return notes;
}

std::vector<Note> setVelocity (std::vector<Note> notes, const IdSet& ids, float velocity)
{
    for (auto& n : notes)
        if (ids.count (n.id) > 0)
            n.velocity = std::clamp (velocity, 1.0f / 127.0f, 1.0f);
    return notes;
}

Tick snap (Tick pos, Tick grid)
{
    if (grid <= 0)
        return pos;
    return (Tick) std::llround ((double) pos / (double) grid) * grid;
}

Tick snapDown (Tick pos, Tick grid)
{
    if (grid <= 0)
        return pos;
    return (Tick) std::floor ((double) pos / (double) grid) * grid;
}

std::vector<Note> quantize (std::vector<Note> notes, const IdSet& ids, Tick regionPosition, Tick grid, float strength)
{
    if (grid <= 0)
        return notes;

    strength = std::clamp (strength, 0.0f, 1.0f);
    for (auto& n : notes)
    {
        if (! ids.empty() && ids.count (n.id) == 0)
            continue;
        const auto abs = regionPosition + n.start;
        const auto target = snap (abs, grid);
        const auto moved = abs + (Tick) std::llround ((double) (target - abs) * strength);
        n.start = std::max<Tick> (0, moved - regionPosition);
    }
    sort (notes);
    return notes;
}

std::vector<Note> remove (std::vector<Note> notes, const IdSet& ids)
{
    notes.erase (std::remove_if (notes.begin(), notes.end(), [&] (const Note& n) { return ids.count (n.id) > 0; }), notes.end());
    return notes;
}

} // namespace sms::noteops
