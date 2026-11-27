// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MusicalTime.h"

#include <cmath>

namespace sms
{

juce::String BarsBeats::toString() const
{
    return juce::String (bar) + "." + juce::String (beat) + "." + juce::String (sixteenth) + "."
         + juce::String (ticks).paddedLeft ('0', 3);
}

void TempoMap::setTempo (double newBpm) noexcept
{
    bpm = juce::jlimit (minTempo, maxTempo, newBpm);
}

void TempoMap::setTimeSignature (TimeSignature s) noexcept
{
    s.numerator = juce::jlimit (1, 32, s.numerator);

    // denominator must be a power of two between 1 and 32
    int d = 1;
    while (d < s.denominator && d < 32)
        d *= 2;
    s.denominator = d;

    signature = s;
}

double TempoMap::ticksToSeconds (Tick t) const noexcept
{
    return (double) t / (double) ticksPerQuarter * 60.0 / bpm;
}

Tick TempoMap::secondsToTicks (double seconds) const noexcept
{
    return (Tick) std::llround (seconds * bpm / 60.0 * (double) ticksPerQuarter);
}

Tick TempoMap::getTicksPerBeat() const noexcept
{
    return ticksPerQuarter * 4 / signature.denominator;
}

Tick TempoMap::getTicksPerBar() const noexcept
{
    return getTicksPerBeat() * signature.numerator;
}

BarsBeats TempoMap::ticksToBarsBeats (Tick t) const noexcept
{
    if (t < 0)
        t = 0;

    const auto perBar = getTicksPerBar();
    const auto perBeat = getTicksPerBeat();
    const Tick perSixteenth = ticksPerQuarter / 4;

    BarsBeats bb;
    bb.bar = (int) (t / perBar) + 1;
    t %= perBar;
    bb.beat = (int) (t / perBeat) + 1;
    t %= perBeat;
    bb.sixteenth = (int) (t / perSixteenth) + 1;
    bb.ticks = (int) (t % perSixteenth);
    return bb;
}

Tick TempoMap::barsBeatsToTicks (const BarsBeats& bb) const noexcept
{
    const Tick perSixteenth = ticksPerQuarter / 4;
    return (Tick) (bb.bar - 1) * getTicksPerBar()
         + (Tick) (bb.beat - 1) * getTicksPerBeat()
         + (Tick) (bb.sixteenth - 1) * perSixteenth
         + bb.ticks;
}

juce::String formatSeconds (double seconds)
{
    if (seconds < 0)
        seconds = 0;

    const auto totalMs = (std::int64_t) std::llround (seconds * 1000.0);
    const auto ms = (int) (totalMs % 1000);
    const auto s = (int) ((totalMs / 1000) % 60);
    const auto m = (int) ((totalMs / 60000) % 60);
    const auto h = (int) (totalMs / 3600000);

    return juce::String (h) + ":" + juce::String (m).paddedLeft ('0', 2) + ":"
         + juce::String (s).paddedLeft ('0', 2) + "." + juce::String (ms).paddedLeft ('0', 3);
}

} // namespace sms
