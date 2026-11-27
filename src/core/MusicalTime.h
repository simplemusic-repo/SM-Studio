// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <juce_core/juce_core.h>

namespace sms
{

/** Musical positions are stored in ticks. 960 ticks per quarter note. */
using Tick = std::int64_t;
inline constexpr Tick ticksPerQuarter = 960;

struct TimeSignature
{
    int numerator = 4;
    int denominator = 4;

    bool operator== (const TimeSignature&) const = default;
};

/** Bars/beats display position (all 1-based except ticks). */
struct BarsBeats
{
    int bar = 1;
    int beat = 1;
    int sixteenth = 1;
    int ticks = 0;

    juce::String toString() const;
};

/** Converts between ticks, seconds and bars/beats.

    Currently a single tempo and time signature for the whole song. The
    interface is designed so a tempo track with changes can be added later
    without touching callers (they only ever ask the map to convert).
*/
class TempoMap
{
public:
    double getTempo() const noexcept { return bpm; }
    void setTempo (double newBpm) noexcept;

    TimeSignature getTimeSignature() const noexcept { return signature; }
    void setTimeSignature (TimeSignature) noexcept;

    double ticksToSeconds (Tick) const noexcept;
    Tick secondsToTicks (double seconds) const noexcept;

    Tick getTicksPerBeat() const noexcept;
    Tick getTicksPerBar() const noexcept;

    BarsBeats ticksToBarsBeats (Tick) const noexcept;
    Tick barsBeatsToTicks (const BarsBeats&) const noexcept;

    static constexpr double minTempo = 20.0;
    static constexpr double maxTempo = 999.0;

private:
    double bpm = 120.0;
    TimeSignature signature;
};

/** Formats seconds as h:mm:ss.mmm */
juce::String formatSeconds (double seconds);

} // namespace sms
