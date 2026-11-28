// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure functions for editing the notes of a MIDI region (piano roll).
// Note times are relative to the region start; grids are in song time.
#pragma once

#include "Model.h"

#include <set>
#include <vector>

namespace sms::noteops
{

using IdSet = std::set<ObjectId>;

/** Sorted by start, then pitch (the order the engine and the editor expect). */
void sort (std::vector<Note>&);

/** Moves the given notes by dTicks / dPitch (pitch clamped to 0..127, start >= 0). */
std::vector<Note> move (std::vector<Note>, const IdSet&, Tick dTicks, int dPitch);

/** Changes the length of the given notes by dTicks (at least minLength). */
std::vector<Note> resize (std::vector<Note>, const IdSet&, Tick dTicks, Tick minLength);

/** Sets the velocity (0..1) of the given notes. */
std::vector<Note> setVelocity (std::vector<Note>, const IdSet&, float velocity);

/** Snaps the starts of the given notes (all if ids is empty) to the grid.
    regionPosition: song position of the region (the grid is in song time).
    strength 0..1: 1 = exactly on the grid. Lengths are kept. */
std::vector<Note> quantize (std::vector<Note>, const IdSet&, Tick regionPosition, Tick grid, float strength = 1.0f);

/** Removes the given notes. */
std::vector<Note> remove (std::vector<Note>, const IdSet&);

/** Rounds a song position to the grid (grid <= 0: unchanged). */
Tick snap (Tick songPosition, Tick grid);

/** Same, but always to the grid line at or before the position. */
Tick snapDown (Tick songPosition, Tick grid);

} // namespace sms::noteops
