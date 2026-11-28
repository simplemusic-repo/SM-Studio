// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core/Commands.h"
#include "core/RegionOps.h"
#include "engine/EngineSync.h"
#include "engine/PluginHost.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace sms
{

namespace
{
    /** Writes a mono or stereo WAV with a constant value per channel. */
    juce::File writeConstantWav (const juce::File& f, double sr, int length, std::vector<float> values)
    {
        f.deleteFile();
        juce::AudioBuffer<float> b ((int) values.size(), length);
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            juce::FloatVectorOperations::fill (b.getWritePointer (ch), values[(size_t) ch], length);

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> out = std::make_unique<juce::FileOutputStream> (f);
        const auto options = juce::AudioFormatWriterOptions{}.withSampleRate (sr)
                                                             .withNumChannels (b.getNumChannels())
                                                             .withBitsPerSample (24);
        if (auto writer = wav.createWriterFor (out, options))
            writer->writeFromAudioSampleBuffer (b, 0, length);
        return f;
    }

    struct Fixture
    {
        Song song;
        CommandHistory history { song };
        AudioEngine engine;
        std::unique_ptr<AudioPool> pool;
        std::unique_ptr<EngineSync> sync;
        juce::TemporaryFile tmpA { ".wav" }, tmpB { ".wav" };

        static constexpr double sr = 48000.0;

        Fixture()
        {
            engine.prepareToPlay (sr, 512);
            pool = std::make_unique<AudioPool> (song);
        }

        ObjectId addAudioTrack (const juce::File& file, Tick position, float volumeDb = 0.0f, float pan = 0.0f)
        {
            AudioSource src;
            src.id = song.newId();
            src.file = file;
            std::unique_ptr<juce::AudioFormatReader> r (juce::WavAudioFormat().createReaderFor (file.createInputStream().release(), true));
            src.sampleRate = r->sampleRate;
            src.lengthInSamples = r->lengthInSamples;
            src.numChannels = (int) r->numChannels;
            history.perform (std::make_unique<commands::AddAudioSource> (src));

            Track t;
            t.id = song.newId();
            t.typeId = "audio";
            t.volumeDb = volumeDb;
            t.pan = pan;
            const auto tid = t.id;
            history.perform (std::make_unique<commands::AddTrack> (std::move (t)));

            Region reg;
            reg.id = song.newId();
            reg.kind = Region::Kind::audio;
            reg.position = position;
            reg.sourceId = src.id;
            reg.length = song.getTempoMap().secondsToTicks ((double) src.lengthInSamples / src.sampleRate);
            history.perform (std::make_unique<commands::AddRegion> (tid, reg));
            return tid;
        }

        void publish()
        {
            pool->waitUntilLoaded();
            engine.publish (buildRenderSnapshot (song, *pool, sr));
            EngineSyncLoopHelper();
        }

        void EngineSyncLoopHelper()
        {
            const auto& map = song.getTempoMap();
            engine.setLoop (map.ticksToSeconds (song.getLoop().start), map.ticksToSeconds (song.getLoop().end), song.getLoop().enabled);
        }

        /** Renders `seconds` from `fromSeconds`, returns stereo output. */
        juce::AudioBuffer<float> render (double fromSeconds, double seconds, int blockSize = 512)
        {
            engine.setPositionSeconds (fromSeconds);
            engine.play();
            const auto total = (int) (seconds * sr);
            juce::AudioBuffer<float> out (2, total);
            for (int pos = 0; pos < total; pos += blockSize)
            {
                const auto n = std::min (blockSize, total - pos);
                float* chans[] = { out.getWritePointer (0, pos), out.getWritePointer (1, pos) };
                engine.renderBlock (chans, 2, n);
            }
            engine.stop();
            return out;
        }
    };
}

//==============================================================================
class PlaybackTests final : public juce::UnitTest
{
public:
    PlaybackTests() : juce::UnitTest ("Playback", "SMStudio") {}

    void runTest() override
    {
        beginTest ("pan law");
        {
            auto [l, r] = trackGains (0.0f, 0.0f);
            expectWithinAbsoluteError (l, 1.0f, 1e-6f);
            expectWithinAbsoluteError (r, 1.0f, 1e-6f);
            std::tie (l, r) = trackGains (0.0f, -1.0f);
            expectWithinAbsoluteError (r, 0.0f, 1e-6f);
            std::tie (l, r) = trackGains (-6.0f, 0.5f);
            expectWithinAbsoluteError (l, 0.5012f * 0.5f, 1e-3f);
            expectWithinAbsoluteError (r, 0.5012f, 1e-3f);
            std::tie (l, r) = trackGains (Track::minVolumeDb, 0.0f);
            expectEquals (l, 0.0f);
        }

        beginTest ("audio region plays at its position (mono source on both channels)");
        {
            Fixture f;
            const auto bar = f.song.getTempoMap().getTicksPerBar(); // 2 s at 120 bpm
            f.addAudioTrack (writeConstantWav (f.tmpA.getFile(), Fixture::sr, 48000, { 0.5f }), bar);
            f.publish();

            auto out = f.render (1.9, 0.3);               // 1.9 s .. 2.2 s
            const int onset = (int) (0.1 * Fixture::sr); // region starts at 2.0 s
            expect (out.getMagnitude (0, 0, onset - 1) < 1e-6f, "silence before the region");
            expectWithinAbsoluteError (out.getSample (0, onset + 1000), 0.5f, 1e-3f);
            expectWithinAbsoluteError (out.getSample (1, onset + 1000), 0.5f, 1e-3f);
            expect (out.getSample (0, onset + 10) < 0.5f, "fade-in at the clip start");
            expectWithinAbsoluteError (f.engine.getPositionSeconds(), 2.2, 0.011);
        }

        beginTest ("volume, pan, mute, solo, muted region");
        {
            Fixture f;
            auto a = f.addAudioTrack (writeConstantWav (f.tmpA.getFile(), Fixture::sr, 48000, { 0.5f, 0.25f }), 0, -6.0f, -1.0f);
            f.publish();
            auto out = f.render (0.2, 0.05);
            expectWithinAbsoluteError (out.getSample (0, 500), 0.5f * 0.5012f, 1e-3f);
            expectWithinAbsoluteError (out.getSample (1, 500), 0.0f, 1e-6f);

            using P = commands::SetTrackParam::Param;
            f.history.perform (std::make_unique<commands::SetTrackParam> (a, P::pan, 0.0f));
            f.history.perform (std::make_unique<commands::SetTrackParam> (a, P::volumeDb, 0.0f));
            f.publish();
            out = f.render (0.2, 0.05);
            // gains ramp within the first block after a change, later blocks are exact
            expectWithinAbsoluteError (out.getSample (1, 1500), 0.25f, 1e-3f);

            f.history.perform (std::make_unique<commands::SetTrackParam> (a, P::mute, 1.0f));
            f.publish();
            f.render (0.2, 0.02); // ramp down
            out = f.render (0.2, 0.05);
            expect (out.getMagnitude (0, out.getNumSamples()) < 1e-6f, "muted track is silent");

            // solo on another track silences this one
            f.history.perform (std::make_unique<commands::SetTrackParam> (a, P::mute, 0.0f));
            auto b = f.addAudioTrack (writeConstantWav (f.tmpB.getFile(), Fixture::sr, 48000, { 0.1f }), 0);
            f.history.perform (std::make_unique<commands::SetTrackParam> (b, P::solo, 1.0f));
            f.publish();
            f.render (0.2, 0.02);
            out = f.render (0.2, 0.05);
            expectWithinAbsoluteError (out.getSample (0, 1000), 0.1f, 1e-3f);

            // muted event
            auto* tb = f.song.findTrack (b);
            auto r = tb->regions.front();
            r.muted = true;
            f.history.perform (std::make_unique<commands::UpdateRegion> (b, b, r, "mute"));
            f.publish();
            f.render (0.2, 0.02);
            out = f.render (0.2, 0.05);
            expect (out.getMagnitude (0, out.getNumSamples()) < 1e-6f, "muted event is silent");
        }

        beginTest ("loop wraps the playhead");
        {
            Fixture f;
            f.addAudioTrack (writeConstantWav (f.tmpA.getFile(), Fixture::sr, 48000 * 4, { 0.3f }), 0);
            const auto beat = f.song.getTempoMap().getTicksPerBeat(); // 0.5 s
            f.song.setLoop ({ beat * 2, beat * 3, true });             // 1.0 .. 1.5 s
            f.publish();
            f.render (1.4, 0.3, 441);
            expectWithinAbsoluteError (f.engine.getPositionSeconds(), 1.2, 0.011);
        }

        beginTest ("sample rate conversion keeps the duration");
        {
            Fixture f;
            f.addAudioTrack (writeConstantWav (f.tmpA.getFile(), 44100.0, 44100, { 0.4f }), 0);
            f.publish();
            auto out = f.render (0.0, 1.2);
            expectWithinAbsoluteError (out.getSample (0, 47000), 0.4f, 1e-3f);
            expect (out.getMagnitude (0, 48100, 1000) < 1e-6f, "ends after one second");
        }

        beginTest ("MIDI notes play through an instrument, transpose and note-off on stop");
        {
            Fixture f;
            PluginHost host ({});
            host.setPlaybackFormat (Fixture::sr, 512);

            Track t;
            t.id = f.song.newId();
            t.typeId = "instrument";
            t.instrumentPluginId = "builtin:testsynth";
            const auto tid = t.id;
            f.history.perform (std::make_unique<commands::AddTrack> (std::move (t)));

            Region r;
            r.id = f.song.newId();
            r.kind = Region::Kind::midi;
            r.position = 960;      // 0.5 s
            r.length = 960 * 4;    // 2 s
            r.notes.push_back ({ f.song.newId(), 0, 960, 69, 0.8f }); // 0.5 .. 1.0 s
            f.history.perform (std::make_unique<commands::AddRegion> (tid, r));

            f.engine.publish (buildRenderSnapshot (f.song, *f.pool, Fixture::sr, &host));
            auto out = f.render (0.0, 1.6);
            const auto at = [] (double sec) { return (int) (sec * Fixture::sr); };
            expect (out.getMagnitude (0, 0, at (0.49)) < 1e-6f, "silence before the note");
            expect (out.getMagnitude (0, at (0.6), at (0.3)) > 0.02f, "note is audible");
            expect (out.getMagnitude (0, at (1.45), at (0.1)) < 1e-4f, "released after the note end");

            // the note sounds an octave higher with transpose +12: count zero crossings
            auto crossings = [] (const juce::AudioBuffer<float>& b, int from, int len)
            {
                int c = 0;
                for (int i = from + 1; i < from + len; ++i)
                    if ((b.getSample (0, i - 1) < 0.0f) != (b.getSample (0, i) < 0.0f))
                        ++c;
                return c;
            };
            const auto base = crossings (out, at (0.6), at (0.2));
            f.history.perform (std::make_unique<commands::SetTrackParam> (tid, commands::SetTrackParam::Param::transpose, 12.0f));
            f.engine.publish (buildRenderSnapshot (f.song, *f.pool, Fixture::sr, &host));
            out = f.render (0.0, 1.0);
            const auto higher = crossings (out, at (0.6), at (0.2));
            expect (higher > base * 3 / 2, "transpose +12 raises the pitch (" + juce::String (base) + " -> " + juce::String (higher) + ")");

            // stop in the middle of a note: the instrument gets a note-off and goes quiet
            f.history.perform (std::make_unique<commands::SetTrackParam> (tid, commands::SetTrackParam::Param::transpose, 0.0f));
            f.engine.publish (buildRenderSnapshot (f.song, *f.pool, Fixture::sr, &host));
            f.render (0.4, 0.3);               // stops at 0.7 s while the note sounds
            juce::AudioBuffer<float> tail (2, at (0.6));
            float* chans[] = { tail.getWritePointer (0), tail.getWritePointer (1) };
            for (int pos = 0; pos < tail.getNumSamples(); pos += 512)
            {
                float* c[] = { chans[0] + pos, chans[1] + pos };
                f.engine.renderBlock (c, 2, std::min (512, tail.getNumSamples() - pos));
            }
            expect (tail.getMagnitude (0, at (0.45), at (0.1)) < 1e-4f, "note released after stop");
        }

        beginTest ("after a device change, snapshots for the old rate stay silent");
        {
            Fixture f;
            PluginHost host ({});
            host.setPlaybackFormat (Fixture::sr, 512);
            Track t;
            t.id = f.song.newId();
            t.typeId = "instrument";
            t.instrumentPluginId = "builtin:testsynth";
            const auto tid = t.id;
            f.history.perform (std::make_unique<commands::AddTrack> (std::move (t)));
            Region r;
            r.id = f.song.newId();
            r.kind = Region::Kind::midi;
            r.length = 960 * 8;
            r.notes.push_back ({ f.song.newId(), 0, 960 * 8, 60, 0.8f });
            f.history.perform (std::make_unique<commands::AddRegion> (tid, r));
            f.engine.publish (buildRenderSnapshot (f.song, *f.pool, Fixture::sr, &host));

            f.engine.prepareToPlay (44100.0, 1024); // sound card changed: other rate and buffer
            auto out = f.render (0.0, 0.2, 1024);
            expect (out.getMagnitude (0, out.getNumSamples()) < 1e-6f, "silent until rebuilt for the new device");

            // EngineSync prepares the plug-in (not in use any more) and rebuilds
            host.setPlaybackFormat (44100.0, 1024);
            auto snap = buildRenderSnapshot (f.song, *f.pool, 44100.0, &host);
            for (auto& tr : snap->tracks)
                if (tr.instrument != nullptr)
                    PluginHost::prepare (*tr.instrument, 44100.0, 1024);
            f.engine.publish (std::move (snap));
            out = f.render (0.0, 0.2, 1024);
            expect (out.getMagnitude (0, out.getNumSamples()) > 0.01f, "plays again after rebuilding");
            f.engine.prepareToPlay (Fixture::sr, 512);
        }

        beginTest ("snapshots are freed when no longer used");
        {
            Fixture f;
            for (int i = 0; i < 5; ++i)
                f.publish();
            f.render (0.0, 0.01);
            f.engine.collectGarbage();
            expect (true); // no crash / leak detector would complain in debug builds
        }
    }
};

//==============================================================================
class RegionOpsTests final : public juce::UnitTest
{
public:
    RegionOpsTests() : juce::UnitTest ("Region operations", "SMStudio") {}

    void runTest() override
    {
        TempoMap map; // 120 bpm: 960 ticks = 0.5 s
        IdGenerator ids;
        ids.reserveUpTo (100);

        AudioSource src;
        src.id = { 1 };
        src.sampleRate = 48000.0;
        src.lengthInSamples = 48000 * 4; // 4 s = 7680 ticks

        Region a;
        a.id = { 2 };
        a.kind = Region::Kind::audio;
        a.sourceId = src.id;
        a.position = 1920;
        a.length = 3840; // 2 s

        beginTest ("split audio");
        {
            auto parts = regionops::split (a, 2880, &src, map, ids);
            expect (parts.has_value());
            expectEquals ((long long) parts->first.length, 960LL);
            expectEquals ((long long) parts->second.position, 2880LL);
            expectEquals ((long long) parts->second.length, 2880LL);
            expectEquals ((long long) parts->second.sourceOffsetSamples, 24000LL);
            expect (parts->second.id != a.id);
            expect (! regionops::split (a, 1920, &src, map, ids).has_value());
            expect (! regionops::split (a, 9000, &src, map, ids).has_value());
        }

        beginTest ("trim start and end are clamped to the file");
        {
            auto r = regionops::withStart (a, 0, &src, map); // cannot go before the file start
            expectEquals ((long long) r.position, 1920LL);
            expectEquals ((long long) r.sourceOffsetSamples, 0LL);

            r = regionops::withStart (a, 2400, &src, map);
            expectEquals ((long long) r.sourceOffsetSamples, 12000LL);
            expectEquals ((long long) r.getEnd(), (long long) a.getEnd());

            r = regionops::withEnd (a, 100000, &src, map);
            expectEquals ((long long) r.length, 7680LL); // whole file
            r = regionops::withEnd (a, 1000, &src, map);
            expectEquals ((long long) r.length, (long long) regionops::minLength);
        }

        beginTest ("split MIDI distributes notes");
        {
            Region m;
            m.id = { 3 };
            m.kind = Region::Kind::midi;
            m.position = 0;
            m.length = 3840;
            for (int i = 0; i < 4; ++i)
                m.notes.push_back ({ ObjectId { (std::uint64_t) (10 + i) }, (Tick) i * 960, 480, 60 + i, 0.8f });

            auto parts = regionops::split (m, 1920, nullptr, map, ids);
            expect (parts.has_value());
            expectEquals ((int) parts->first.notes.size(), 2);
            expectEquals ((int) parts->second.notes.size(), 2);
            expectEquals ((long long) parts->second.notes[0].start, 0LL);
            expectEquals (parts->second.notes[0].pitch, 62);

            auto d = regionops::duplicate (m, ids);
            expect (d.id != m.id && d.notes[0].id != m.notes[0].id);

            auto trimmed = regionops::withStart (m, 960, nullptr, map);
            expectEquals ((long long) trimmed.notes[1].start, 0LL); // note at 960 stays in place
        }

        beginTest ("UpdateRegion moves between tracks with undo");
        {
            Song s;
            CommandHistory h (s);
            Track t1, t2;
            t1.id = s.newId();
            t2.id = s.newId();
            const auto id1 = t1.id, id2 = t2.id;
            h.perform (std::make_unique<commands::AddTrack> (std::move (t1)));
            h.perform (std::make_unique<commands::AddTrack> (std::move (t2)));
            h.perform (std::make_unique<commands::AddRegion> (id1, a));

            auto moved = a;
            moved.position = 0;
            expect (h.perform (std::make_unique<commands::UpdateRegion> (id1, id2, moved, "Verschieben")));
            expect (s.findTrack (id1)->regions.empty());
            expectEquals ((long long) s.findTrack (id2)->regions[0].position, 0LL);
            expect (s.findTrackOfRegion (a.id)->id == id2);

            h.undo();
            expect (s.findTrack (id2)->regions.empty());
            expectEquals ((long long) s.findTrack (id1)->regions[0].position, 1920LL);

            expect (! h.perform (std::make_unique<commands::UpdateRegion> (id1, id1, a, "nichts")));
        }

        beginTest ("merge (glue)");
        {
            Song s;
            CommandHistory h (s);
            auto srcCopy = src;
            srcCopy.id = s.newId();
            h.perform (std::make_unique<commands::AddAudioSource> (srcCopy));

            auto whole = a;
            whole.sourceId = srcCopy.id;
            IdGenerator localIds;
            localIds.reserveUpTo (1000);
            auto parts = regionops::split (whole, 2880, &srcCopy, s.getTempoMap(), localIds);
            juce::String why;
            auto merged = regionops::merge ({ parts->second, parts->first }, s, why);
            expect (merged.has_value(), why);
            expect (merged->id == whole.id);
            expectEquals ((long long) merged->position, (long long) whole.position);
            expectEquals ((long long) merged->length, (long long) whole.length);
            expectEquals ((long long) merged->sourceOffsetSamples, 0LL);

            auto moved = parts->second;
            moved.position += 960; // gap -> not seamless
            expect (! regionops::merge ({ parts->first, moved }, s, why).has_value());
            expect (why.isNotEmpty());

            Region m1, m2;
            m1.id = { 900 }; m1.kind = Region::Kind::midi; m1.position = 0;    m1.length = 960;
            m2.id = { 901 }; m2.kind = Region::Kind::midi; m2.position = 1920; m2.length = 960;
            m1.notes.push_back ({ ObjectId { 910 }, 0, 480, 60, 0.8f });
            m2.notes.push_back ({ ObjectId { 911 }, 240, 480, 64, 0.8f });
            auto mm = regionops::merge ({ m1, m2 }, s, why);
            expect (mm.has_value());
            expectEquals ((long long) mm->length, 2880LL);
            expectEquals ((int) mm->notes.size(), 2);
            expectEquals ((long long) mm->notes[1].start, 2160LL);

            expect (! regionops::merge ({ m1 }, s, why).has_value());
            expect (! regionops::merge ({ m1, whole }, s, why).has_value());
        }

        beginTest ("loop range is normalised");
        {
            Song s;
            s.setLoop ({ 2000, 1000, true });
            expectEquals ((long long) s.getLoop().start, 1000LL);
            expect (s.getLoop().isValid());
        }
    }
};

static PlaybackTests playbackTests;
static RegionOpsTests regionOpsTests;

} // namespace sms
