// SPDX-License-Identifier: AGPL-3.0-or-later
#include "RenderSnapshot.h"

#include <cmath>

namespace sms
{

std::pair<float, float> trackGains (float volumeDb, float pan)
{
    const auto gain = volumeDb <= Track::minVolumeDb + 0.01f ? 0.0f : juce::Decibels::decibelsToGain (volumeDb);
    pan = juce::jlimit (-1.0f, 1.0f, pan);
    return { gain * std::min (1.0f, 1.0f - pan), gain * std::min (1.0f, 1.0f + pan) };
}

std::unique_ptr<RenderSnapshot> buildRenderSnapshot (const Song& song, const AudioPool& pool, double sampleRate)
{
    auto snap = std::make_unique<RenderSnapshot>();
    snap->sampleRate = sampleRate;

    const auto& map = song.getTempoMap();
    const bool anySolo = std::any_of (song.getTracks().begin(), song.getTracks().end(), [] (auto& t) { return t.solo; });
    auto toSamples = [&] (Tick t) { return (std::int64_t) std::llround (map.ticksToSeconds (t) * sampleRate); };

    for (auto& t : song.getTracks())
    {
        RenderSnapshot::Track rt;
        rt.id = t.id;

        const bool audible = ! t.mute && (! anySolo || t.solo);
        std::tie (rt.gainL, rt.gainR) = audible ? trackGains (t.volumeDb, t.pan) : std::pair<float, float> { 0.0f, 0.0f };

        for (auto& r : t.regions)
        {
            if (r.kind != Region::Kind::audio || r.muted)
                continue;

            auto audio = pool.get (r.sourceId);
            if (audio == nullptr || audio->sampleRate <= 0)
                continue;

            RenderSnapshot::Clip c;
            c.start = toSamples (r.position);
            c.length = toSamples (r.getEnd()) - c.start;
            c.sourceStart = (double) r.sourceOffsetSamples;
            c.ratio = audio->sampleRate / sampleRate;
            c.gain = juce::Decibels::decibelsToGain (r.gainDb);

            const auto available = (double) audio->buffer.getNumSamples() - c.sourceStart;
            c.length = std::min<std::int64_t> (c.length, (std::int64_t) std::floor (available / c.ratio));
            if (c.length <= 0)
                continue;

            c.audio = std::move (audio);
            rt.clips.push_back (std::move (c));
        }

        std::sort (rt.clips.begin(), rt.clips.end(), [] (auto& a, auto& b) { return a.start < b.start; });
        snap->tracks.push_back (std::move (rt));
    }

    return snap;
}

} // namespace sms
