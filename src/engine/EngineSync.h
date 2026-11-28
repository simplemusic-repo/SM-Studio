// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "AudioEngine.h"

namespace sms
{

/** Keeps the audio engine in step with the song: rebuilds the render snapshot
    when tracks, regions or audio files change, and forwards tempo and loop. */
class EngineSync final : private Song::Listener,
                         private juce::ChangeListener,
                         private juce::Timer
{
public:
    EngineSync (Song&, AudioPool&, AudioEngine&);
    ~EngineSync() override;

    /** Rebuilds and publishes immediately (also used by tests). */
    void update();

private:
    void songChanged (const SongChange&) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void syncTransport();

    Song& song;
    AudioPool& pool;
    AudioEngine& engine;
    bool dirty = true;
    double builtForRate = 0.0;
};

} // namespace sms
