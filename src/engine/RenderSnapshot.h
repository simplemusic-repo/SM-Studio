// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "AudioPool.h"

#include <juce_audio_processors/juce_audio_processors.h>

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

    struct MidiEvent
    {
        std::int64_t time = 0; // timeline position in output samples
        juce::uint8 status = 0, data1 = 0, data2 = 0;
    };

    struct Track
    {
        ObjectId id;
        float gainL = 1.0f, gainR = 1.0f; // volume, pan, mute and solo resolved
        std::vector<Clip> clips;          // sorted by start
        std::vector<MidiEvent> midi;      // sorted by time (note-offs before note-ons)

        std::shared_ptr<juce::AudioProcessor> instrument;
        std::vector<std::shared_ptr<juce::AudioProcessor>> inserts; // active (not bypassed) in order

        bool hasProcessors() const noexcept { return instrument != nullptr || ! inserts.empty(); }
    };

    double sampleRate = 44100.0;
    int maxBlock = 512; // block size the plug-ins were prepared for
    std::vector<Track> tracks;
};

/** Left/right gains for a track (balance pan law, mute/solo applied by caller). */
std::pair<float, float> trackGains (float volumeDb, float pan);

class PluginHost;

/** Builds a snapshot. With a PluginHost, instrument and insert plug-ins are
    included (instances that are still loading are left out until they are ready). */
std::unique_ptr<RenderSnapshot> buildRenderSnapshot (const Song&, const AudioPool&, double sampleRate,
                                                     PluginHost* plugins = nullptr);

} // namespace sms
