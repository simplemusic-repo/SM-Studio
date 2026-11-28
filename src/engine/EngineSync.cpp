// SPDX-License-Identifier: AGPL-3.0-or-later
#include "EngineSync.h"

#include "CrashInfo.h"

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

    // plug-ins prepared for another rate / block size (e.g. after changing the sound card)
    std::vector<juce::AudioProcessor*> needPrepare;
    auto check = [&] (const std::shared_ptr<juce::AudioProcessor>& p)
    {
        if (p != nullptr && (! juce::approximatelyEqual (p->getSampleRate(), builtForRate) || p->getBlockSize() != builtForBlock))
            needPrepare.push_back (p.get());
    };
    for (auto& t : snap->tracks)
    {
        check (t.instrument);
        for (auto& i : t.inserts)
            check (i);
    }

    if (std::any_of (needPrepare.begin(), needPrepare.end(), [this] (auto* p) { return engine.isProcessorInUse (p); }))
    {
        // the audio thread may still hold them: first publish a snapshot without
        // plug-ins (audio clips keep playing), prepare on one of the next ticks
        engine.publish (buildRenderSnapshot (song, pool, builtForRate, nullptr));
        dirty = true;
        return;
    }

    for (auto* p : needPrepare)
    {
        crashinfo::note ("re-preparing " + p->getName() + " for " + juce::String (builtForRate) + " Hz / " + juce::String (builtForBlock));
        PluginHost::prepare (*p, builtForRate, builtForBlock);
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
