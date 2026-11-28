// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Model.h"

#include <optional>
#include <utility>

namespace sms::regionops
{

/** Smallest allowed region length (a 64th note). */
inline constexpr Tick minLength = ticksPerQuarter / 16;

/** Length of the audio source in ticks, measured from the region's source offset.
    Returns a very large value for MIDI regions (no limit). */
Tick maxLengthFromOffset (const Region&, const AudioSource*, const TempoMap&);

/** Converts a tick distance into source samples for an audio region. */
std::int64_t ticksToSourceSamples (Tick, const AudioSource&, const TempoMap&);

/** Moves the start of a region (end stays). Audio: the source offset follows,
    MIDI: notes keep their song position. Clamped to valid ranges. */
Region withStart (const Region&, Tick newStart, const AudioSource*, const TempoMap&);

/** Moves the end of a region (start stays). Clamped to the audio file length. */
Region withEnd (const Region&, Tick newEnd, const AudioSource*, const TempoMap&);

/** Splits a region at a song position. Returns {left, right}; the right part
    gets a new id (and new note ids). nullopt if `at` is not inside. */
std::optional<std::pair<Region, Region>> split (const Region&, Tick at, const AudioSource*,
                                                const TempoMap&, IdGenerator&);

/** Merges events of one track into a single event ("glue").
    MIDI: always possible (notes keep their song positions).
    Audio: only if all parts come from the same file and follow each other
    seamlessly (e.g. after splitting) – otherwise nullopt with a reason.
    The result keeps the id of the earliest event. */
std::optional<Region> merge (std::vector<Region> parts, const Song&, juce::String& whyNot);

/** Copy with fresh ids (region and notes). */
Region duplicate (const Region&, IdGenerator&);

} // namespace sms::regionops
