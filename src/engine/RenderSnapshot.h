// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "AudioPool.h"

namespace sms
{

/** Everything the audio thread needs to play the song, in sample units.
    Built on the message thread, never modified after publishing. */
struct RenderSnapshot
{
    struct Clip
    {
        std::int64_t start = 0;  // timeline position in output samples
        std::int64_t length = 0; // in output samples
        double sourceStart = 0;  // source sample at clip start
        double ratio = 1.0;      // source samples per output sample (sample rate conversion)
        float gain = 1.0f;
        std::shared_ptr<const LoadedAudio> audio;
    };

    struct Track
    {
        ObjectId id;
        float gainL = 1.0f, gainR = 1.0f; // volume, pan, mute and solo resolved
        std::vector<Clip> clips;          // sorted by start
    };

    double sampleRate = 44100.0;
    std::vector<Track> tracks;
};

/** Left/right gains for a track (balance pan law, mute/solo applied by caller). */
std::pair<float, float> trackGains (float volumeDb, float pan);

std::unique_ptr<RenderSnapshot> buildRenderSnapshot (const Song&, const AudioPool&, double sampleRate);

} // namespace sms
