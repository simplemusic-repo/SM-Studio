// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "core/Model.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>

#include <map>
#include <memory>

namespace sms
{

/** Min/max overview of an audio file for drawing waveforms. */
struct PeakData
{
    static constexpr int samplesPerPeak = 256;

    int numChannels = 0;
    std::vector<std::vector<std::pair<float, float>>> peaks; // [channel][block] = {min, max}

    /** Min/max of a channel over a range of source samples. */
    std::pair<float, float> getRange (int channel, std::int64_t startSample, std::int64_t endSample) const;
};

/** Decoded audio of one source plus its peaks. Immutable once loaded, so the
    audio thread can read it without locks. */
struct LoadedAudio
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 0.0;
    PeakData peaks;
};

/** Loads the song's audio sources into memory on a background thread.

    For now every file is decoded completely into RAM (simple and safe for the
    real-time thread). Disk streaming for very long recordings can replace this
    later without changing users of the pool.

    Listeners are notified on the message thread when a file finished loading. */
class AudioPool final : public juce::ChangeBroadcaster,
                        private Song::Listener,
                        private juce::AsyncUpdater
{
public:
    explicit AudioPool (Song&);
    ~AudioPool() override;

    /** nullptr while the file is loading (or if it failed). */
    std::shared_ptr<const LoadedAudio> get (ObjectId sourceId) const;

    bool isLoading() const;
    juce::String getError (ObjectId sourceId) const;

    /** Blocks until all pending files are loaded (used by tests). */
    void waitUntilLoaded (int timeoutMs = 10000);

    /** Decodes a file synchronously (used by the loader and by tests). */
    static std::shared_ptr<LoadedAudio> loadFile (const juce::File&, juce::String& error);

private:
    void songChanged (const SongChange&) override;
    void handleAsyncUpdate() override;
    void syncWithSong();

    class LoadJob;

    Song& song;
    juce::ThreadPool threads { 2 };
    mutable juce::CriticalSection lock;
    std::map<ObjectId, std::shared_ptr<const LoadedAudio>> loaded;
    std::map<ObjectId, juce::String> errors;
    std::map<ObjectId, bool> pending;
};

} // namespace sms
