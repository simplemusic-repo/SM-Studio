// SPDX-License-Identifier: AGPL-3.0-or-later
#include "EngineSync.h"

namespace sms
{

EngineSync::EngineSync (Song& s, AudioPool& p, AudioEngine& e, PluginHost* h) : song (s), pool (p), engine (e), plugins (h)
{
    song.addListener (this);
    pool.addChangeListener (this);
    if (plugins != nullptr)
        plugins->addChangeListener (this);
    update();
    startTimerHz (30);
}

EngineSync::~EngineSync()
{
    song.removeListener (this);
    pool.removeChangeListener (this);
    if (plugins != nullptr)
        plugins->removeChangeListener (this);
}

void EngineSync::update()
{
    syncTransport();
    builtForRate = engine.getSampleRate();
    builtForBlock = engine.getMaxBlockSize();

    if (plugins != nullptr)
        plugins->setPlaybackFormat (builtForRate, builtForBlock);

    auto snap = buildRenderSnapshot (song, pool, builtForRate, plugins);

    // plug-ins that were prepared for another rate/size and are not playing right now
    auto check = [this] (const std::shared_ptr<juce::AudioProcessor>& p)
    {
        if (p != nullptr && (! juce::approximatelyEqual (p->getSampleRate(), builtForRate) || p->getBlockSize() != builtForBlock)
            && ! engine.isProcessorInUse (p.get()))
            PluginHost::prepare (*p, builtForRate, builtForBlock);
    };
    for (auto& t : snap->tracks)
    {
        check (t.instrument);
        for (auto& i : t.inserts)
            check (i);
    }

    engine.publish (std::move (snap));
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
    dirty = true; // an audio file finished loading, or a plug-in instance is ready
}

void EngineSync::timerCallback()
{
    if (dirty || ! juce::approximatelyEqual (builtForRate, engine.getSampleRate()) || builtForBlock != engine.getMaxBlockSize())
        update();
    else
        engine.collectGarbage();
}

} // namespace sms
