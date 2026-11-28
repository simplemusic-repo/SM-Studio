// SPDX-License-Identifier: AGPL-3.0-or-later
#include "RegionOps.h"

#include <cmath>
#include <limits>

namespace sms::regionops
{

std::int64_t ticksToSourceSamples (Tick t, const AudioSource& src, const TempoMap& map)
{
    return (std::int64_t) std::llround (map.ticksToSeconds (t) * src.sampleRate);
}

Tick maxLengthFromOffset (const Region& r, const AudioSource* src, const TempoMap& map)
{
    if (r.kind != Region::Kind::audio || src == nullptr || src->sampleRate <= 0)
        return std::numeric_limits<Tick>::max() / 4;

    const auto remaining = (double) (src->lengthInSamples - r.sourceOffsetSamples) / src->sampleRate;
    return std::max<Tick> (minLength, map.secondsToTicks (std::max (0.0, remaining)));
}

Region withStart (const Region& r, Tick newStart, const AudioSource* src, const TempoMap& map)
{
    auto out = r;
    const auto end = r.getEnd();
    newStart = std::clamp<Tick> (newStart, 0, end - minLength);

    if (r.kind == Region::Kind::audio && src != nullptr)
    {
        // cannot start before the beginning of the file
        const auto earliest = r.position - map.secondsToTicks ((double) r.sourceOffsetSamples / src->sampleRate);
        newStart = std::max (newStart, std::max<Tick> (0, earliest));

        const auto delta = newStart - r.position;
        out.sourceOffsetSamples = std::clamp<std::int64_t> (r.sourceOffsetSamples + ticksToSourceSamples (delta, *src, map),
                                                            0, src->lengthInSamples);
    }
    else if (r.kind == Region::Kind::midi)
    {
        const auto delta = newStart - r.position;
        for (auto& n : out.notes)
            n.start -= delta; // notes stay at the same song position
    }

    out.position = newStart;
    out.length = end - newStart;
    return out;
}

Region withEnd (const Region& r, Tick newEnd, const AudioSource* src, const TempoMap& map)
{
    auto out = r;
    const auto maxLen = maxLengthFromOffset (r, src, map);
    out.length = std::clamp<Tick> (newEnd - r.position, minLength, maxLen);
    return out;
}

std::optional<std::pair<Region, Region>> split (const Region& r, Tick at, const AudioSource* src,
                                                const TempoMap& map, IdGenerator& ids)
{
    if (at <= r.position + minLength / 2 || at >= r.getEnd() - minLength / 2)
        return std::nullopt;

    auto left = r;
    left.length = at - r.position;

    auto right = withStart (r, at, src, map);
    right.id = ids.next();

    if (r.kind == Region::Kind::midi)
    {
        // each note goes to the part it starts in
        left.notes.clear();
        right.notes.clear();
        for (auto n : r.notes)
        {
            if (r.position + n.start < at)
            {
                left.notes.push_back (n);
            }
            else
            {
                n.start -= (at - r.position);
                n.id = ids.next();
                right.notes.push_back (n);
            }
        }
    }

    return std::make_pair (std::move (left), std::move (right));
}

Region duplicate (const Region& r, IdGenerator& ids)
{
    auto out = r;
    out.id = ids.next();
    for (auto& n : out.notes)
        n.id = ids.next();
    return out;
}

} // namespace sms::regionops
