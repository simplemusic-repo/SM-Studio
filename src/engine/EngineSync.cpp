// SPDX-License-Identifier: AGPL-3.0-or-later
#include "EngineSync.h"

namespace sms
{

EngineSync::EngineSync (Song& s, AudioPool& p, AudioEngine& e) : song (s), pool (p), engine (e)
{
    song.addListener (this);
    pool.addChangeListener (this);
    update();
    startTimerHz (30);
}

EngineSync::~EngineSync()
{
    song.removeListener (this);
    pool.removeChangeListener (this);
}

void EngineSync::update()
{
    syncTransport();
    builtForRate = engine.getSampleRate();
    engine.publish (buildRenderSnapshot (song, pool, builtForRate));
    dirty = false;
}

void EngineSync::syncTransport()
{
    const auto& map = song.getTempoMap();
    engine.setTempo (map.getTempo(), map.getTimeSignature());

    const auto& loop = song.getLoop();
    engine.setLoop (map.ticksToSeconds (loop.start), map.ticksToSeconds (loop.end), loop.enabled && loop.isValid());
}

void EngineSync::songChanged (const SongChange& c)
{
    if (c.what == SongChange::What::loopChanged)
        syncTransport();       // immediate, no snapshot needed
    else
        dirty = true;          // coalesce many changes into one rebuild
}

void EngineSync::changeListenerCallback (juce::ChangeBroadcaster*)
{
    dirty = true; // an audio file finished loading
}

void EngineSync::timerCallback()
{
    if (dirty || ! juce::approximatelyEqual (builtForRate, engine.getSampleRate()))
        update();
    else
        engine.collectGarbage();
}

} // namespace sms
