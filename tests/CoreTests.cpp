// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core/Commands.h"
#include "core/NoteOps.h"

namespace sms
{

using namespace commands;
using Param = SetTrackParam::Param;

static Track makeTrack (Song& s, const juce::String& name)
{
    Track t;
    t.id = s.newId();
    t.typeId = "audio";
    t.name = name;
    return t;
}

//==============================================================================
class MusicalTimeTests final : public juce::UnitTest
{
public:
    MusicalTimeTests() : juce::UnitTest ("Musical time", "SMStudio") {}

    void runTest() override
    {
        TempoMap map;

        beginTest ("ticks <-> seconds at 120 bpm");
        expectWithinAbsoluteError (map.ticksToSeconds (ticksPerQuarter), 0.5, 1e-9);
        expectEquals ((long long) map.secondsToTicks (2.0), (long long) (ticksPerQuarter * 4));

        beginTest ("bars/beats in 4/4");
        auto bb = map.ticksToBarsBeats (map.getTicksPerBar() * 2 + ticksPerQuarter + 240 + 10);
        expectEquals (bb.bar, 3);
        expectEquals (bb.beat, 2);
        expectEquals (bb.sixteenth, 2);
        expectEquals (bb.ticks, 10);
        expectEquals (bb.toString(), juce::String ("3.2.2.010"));
        expectEquals ((long long) map.barsBeatsToTicks (bb), (long long) (map.getTicksPerBar() * 2 + ticksPerQuarter + 250));

        beginTest ("6/8 and tempo limits");
        map.setTimeSignature ({ 6, 8 });
        expectEquals ((long long) map.getTicksPerBeat(), (long long) (ticksPerQuarter / 2));
        expectEquals ((long long) map.getTicksPerBar(), (long long) (ticksPerQuarter * 3));
        map.setTimeSignature ({ 3, 5 }); // denominator rounded to a power of two
        expectEquals (map.getTimeSignature().denominator, 8);
        map.setTempo (5000.0);
        expectEquals (map.getTempo(), TempoMap::maxTempo);

        beginTest ("seconds formatting");
        expectEquals (formatSeconds (3725.5), juce::String ("1:02:05.500"));
    }
};

//==============================================================================
class ModelAndUndoTests final : public juce::UnitTest
{
public:
    ModelAndUndoTests() : juce::UnitTest ("Model and undo", "SMStudio") {}

    void runTest() override
    {
        beginTest ("ids are unique and never reused");
        {
            Song s;
            auto a = s.newId(), b = s.newId();
            expect (a.isValid() && b.isValid() && a != b);
            s.getIds().reserveUpTo (100);
            expect (s.newId().value == 101);
        }

        beginTest ("add / remove track with undo and redo");
        {
            Song s;
            CommandHistory h (s);
            auto t1 = makeTrack (s, "A");
            auto t2 = makeTrack (s, "B");
            const auto id1 = t1.id, id2 = t2.id;

            expect (h.perform (std::make_unique<AddTrack> (std::move (t1))));
            expect (h.perform (std::make_unique<AddTrack> (std::move (t2), 0)));
            expectEquals (s.getNumTracks(), 2);
            expect (s.getTracks()[0].id == id2);

            expect (h.perform (std::make_unique<RemoveTrack> (id2)));
            expectEquals (s.getNumTracks(), 1);

            expect (h.undo());
            expectEquals (s.getNumTracks(), 2);
            expect (s.getTracks()[0].id == id2); // restored at its old index

            expect (h.undo());
            expect (h.undo());
            expectEquals (s.getNumTracks(), 0);
            expect (! h.canUndo());

            expect (h.redo());
            expect (h.redo());
            expect (h.redo());
            expectEquals (s.getNumTracks(), 1);
            expect (s.getTracks()[0].id == id1);
        }

        beginTest ("commands that change nothing are not recorded");
        {
            Song s;
            CommandHistory h (s);
            auto t = makeTrack (s, "A");
            const auto id = t.id;
            h.perform (std::make_unique<AddTrack> (std::move (t)));
            expect (! h.perform (std::make_unique<RenameTrack> (id, "A")));
            expect (! h.perform (std::make_unique<RemoveTrack> (ObjectId { 999 })));
            expectEquals (h.getUndoName(), juce::String (u8"Spur hinzufügen"));
        }

        beginTest ("fader gesture merges into one undo step");
        {
            Song s;
            CommandHistory h (s);
            auto t = makeTrack (s, "A");
            const auto id = t.id;
            h.perform (std::make_unique<AddTrack> (std::move (t)));

            for (float v : { -1.0f, -2.0f, -3.0f, -4.5f })
                h.perform (std::make_unique<SetTrackParam> (id, Param::volumeDb, v), true);
            h.endGesture();

            expectEquals (s.findTrack (id)->volumeDb, -4.5f);
            h.undo();
            expectEquals (s.findTrack (id)->volumeDb, 0.0f);
            expectEquals (s.getNumTracks(), 1); // only the gesture was undone

            h.redo();
            expectEquals (s.findTrack (id)->volumeDb, -4.5f);

            // a new gesture after endGesture() is a separate step
            h.perform (std::make_unique<SetTrackParam> (id, Param::volumeDb, -6.0f), true);
            h.endGesture();
            h.undo();
            expectEquals (s.findTrack (id)->volumeDb, -4.5f);
        }

        beginTest ("parameter limits");
        {
            Song s;
            CommandHistory h (s);
            auto t = makeTrack (s, "A");
            const auto id = t.id;
            h.perform (std::make_unique<AddTrack> (std::move (t)));
            h.perform (std::make_unique<SetTrackParam> (id, Param::volumeDb, 50.0f));
            h.perform (std::make_unique<SetTrackParam> (id, Param::pan, -7.0f));
            expectEquals (s.findTrack (id)->volumeDb, Track::maxVolumeDb);
            expectEquals (s.findTrack (id)->pan, -1.0f);
            h.perform (std::make_unique<SetTrackParam> (id, Param::transpose, 60.0f));
            h.perform (std::make_unique<SetTrackParam> (id, Param::velocityOffset, -12.4f));
            expectEquals (s.findTrack (id)->transpose, 48);
            expectEquals (s.findTrack (id)->velocityOffset, -12);
        }

        beginTest ("transactions form one undo step");
        {
            Song s;
            CommandHistory h (s);
            h.beginTransaction ("Mehrere");
            for (int i = 0; i < 3; ++i)
                h.perform (std::make_unique<AddTrack> (makeTrack (s, "T" + juce::String (i))));
            h.endTransaction();

            expectEquals (s.getNumTracks(), 3);
            expectEquals (h.getUndoName(), juce::String ("Mehrere"));
            h.undo();
            expectEquals (s.getNumTracks(), 0);
            h.redo();
            expectEquals (s.getNumTracks(), 3);
            expectEquals (s.getTracks()[2].name, juce::String ("T2"));
        }

        beginTest ("regions, inserts, instruments");
        {
            Song s;
            CommandHistory h (s);
            auto t = makeTrack (s, "A");
            const auto id = t.id;
            h.perform (std::make_unique<AddTrack> (std::move (t)));

            Region r;
            r.id = s.newId();
            r.position = 960;
            r.length = 3840;
            const auto rid = r.id;
            h.perform (std::make_unique<AddRegion> (id, r));
            expect (s.findTrack (id)->findRegion (rid) != nullptr);
            expectEquals ((long long) s.getSongEnd(), (long long) (s.getTempoMap().getTicksPerBar() * 8));

            h.perform (std::make_unique<RemoveRegion> (id, rid));
            expect (s.findTrack (id)->findRegion (rid) == nullptr);
            h.undo();
            expect (s.findTrack (id)->findRegion (rid) != nullptr);

            InsertSlot a { s.newId(), "x:eq", "EQ", false }, b { s.newId(), "x:comp", "Comp", false };
            h.perform (std::make_unique<AddInsert> (id, a));
            h.perform (std::make_unique<AddInsert> (id, b, 0));
            expectEquals (s.findTrack (id)->inserts[0].name, juce::String ("Comp"));
            h.perform (std::make_unique<RemoveInsert> (id, b.id));
            expectEquals ((int) s.findTrack (id)->inserts.size(), 1);
            h.undo();
            expectEquals (s.findTrack (id)->inserts[0].name, juce::String ("Comp"));

            h.perform (std::make_unique<SetInsertBypass> (id, b.id, true));
            expect (s.findTrack (id)->inserts[0].bypassed);
            expect (! h.perform (std::make_unique<SetInsertBypass> (id, b.id, true)));
            h.undo();
            expect (! s.findTrack (id)->inserts[0].bypassed);

            InsertSlot c { s.newId(), "x:reverb", "Reverb", false };
            h.perform (std::make_unique<ReplaceInsert> (id, b.id, c));
            expectEquals (s.findTrack (id)->inserts[0].name, juce::String ("Reverb"));
            expectEquals ((int) s.findTrack (id)->inserts.size(), 2);
            h.undo();
            expectEquals (s.findTrack (id)->inserts[0].name, juce::String ("Comp"));

            h.perform (std::make_unique<SetInstrument> (id, "x:synth", "Synth"));
            expectEquals (s.findTrack (id)->instrumentName, juce::String ("Synth"));
            h.undo();
            expect (s.findTrack (id)->instrumentName.isEmpty());
        }

        beginTest ("listeners are notified");
        {
            struct Counter : Song::Listener
            {
                void songChanged (const SongChange& c) override { ++count[(int) c.what]; }
                int count[16] {};
            } counter;

            Song s;
            s.addListener (&counter);
            CommandHistory h (s);
            auto t = makeTrack (s, "A");
            const auto id = t.id;
            h.perform (std::make_unique<AddTrack> (std::move (t)));
            h.perform (std::make_unique<RenameTrack> (id, "B"));
            h.perform (std::make_unique<SetTempo> (100.0));
            s.removeListener (&counter);

            expectEquals (counter.count[(int) SongChange::What::trackAdded], 1);
            expectEquals (counter.count[(int) SongChange::What::trackChanged], 1);
            expectEquals (counter.count[(int) SongChange::What::tempoChanged], 1);
        }
    }
};

static MusicalTimeTests musicalTimeTests;
static ModelAndUndoTests modelAndUndoTests;

} // namespace sms

//==============================================================================
#include "core/ValueFormat.h"

namespace sms
{

class ValueFormatTests final : public juce::UnitTest
{
public:
    ValueFormatTests() : juce::UnitTest ("Value format", "SMStudio") {}

    void runTest() override
    {
        using namespace valueformat;

        beginTest ("volume");
        expectEquals (formatDb (0.0f), juce::String ("0.0 dB"));
        expectEquals (formatDb (-3.46f), juce::String ("-3.5 dB"));
        expectEquals (formatDb (2.0f), juce::String ("+2.0 dB"));
        expectEquals (formatDb (-0.01f), juce::String ("0.0 dB"));
        expectEquals (formatDb (Track::minVolumeDb), juce::String ("-inf dB"));
        expectEquals (*parseDb ("-3,5 dB"), -3.5f);
        expectEquals (*parseDb (" +4 "), 4.0f);
        expectEquals (*parseDb ("99"), Track::maxVolumeDb);
        expectEquals (*parseDb ("-inf"), Track::minVolumeDb);
        expect (! parseDb ("abc").has_value());
        expect (! parseDb ("").has_value());

        beginTest ("pan");
        expectEquals (formatPan (0.0f), juce::String ("C"));
        expectEquals (formatPan (-0.3f), juce::String ("L30"));
        expectEquals (formatPan (1.0f), juce::String ("R100"));
        expectEquals (*parsePan ("c"), 0.0f);
        expectEquals (*parsePan ("L30"), -0.3f);
        expectEquals (*parsePan ("r 20"), 0.2f);
        expectEquals (*parsePan ("-45"), -0.45f);
        expectEquals (*parsePan ("45"), 0.45f);
        expectEquals (*parsePan ("L250"), -1.0f);
        expect (! parsePan ("links").has_value());
    }
};

static ValueFormatTests valueFormatTests;

//==============================================================================
class NoteOpsTests final : public juce::UnitTest
{
public:
    NoteOpsTests() : juce::UnitTest ("Note editing", "SMStudio") {}

    void runTest() override
    {
        using namespace noteops;
        std::vector<Note> notes {
            { ObjectId { 1 }, 0,   480, 60, 0.8f },
            { ObjectId { 2 }, 250, 240, 64, 0.5f },
            { ObjectId { 3 }, 970, 100, 67, 0.7f },
        };

        beginTest ("move: time and pitch, clamped, sorted");
        {
            auto r = move (notes, { ObjectId { 1 } }, 1000, 70);
            expectEquals ((int) r.back().id.value, 1);
            expectEquals ((int) r.back().start, 1000);
            expectEquals (r.back().pitch, 127);
            r = move (notes, { ObjectId { 2 } }, -1000, -100);
            expectEquals ((int) r.front().start, 0);
            expect (r.front().pitch == 0 || r[1].pitch == 0);
        }

        beginTest ("resize keeps a minimum length");
        {
            auto r = resize (notes, { ObjectId { 3 } }, -500, 30);
            expectEquals ((int) r[2].length, 30);
            r = resize (notes, { ObjectId { 3 } }, 140, 30);
            expectEquals ((int) r[2].length, 240);
        }

        beginTest ("quantize snaps starts in song time");
        {
            // region at 100 ticks, 1/16 grid (240): absolute 100, 350, 1070 -> 0, 240(=480? no: 350->240), 960
            auto r = quantize (notes, {}, 100, 240);
            expectEquals ((int) r[0].start, -100 + 100);   // 100 -> 0 abs -> clamped start 0
            expectEquals ((int) r[1].start, 240 - 100);    // 350 -> 240
            expectEquals ((int) r[2].start, 960 - 100);    // 1070 -> 960
            expectEquals ((int) r[2].length, 100, "length kept");

            auto half = quantize (notes, { ObjectId { 3 } }, 0, 240, 0.5f); // 970 -> 960: half way = 965
            expectEquals ((int) half[2].start, 965);
            expectEquals ((int) half[1].start, 250, "others untouched");
        }

        beginTest ("velocity and remove");
        {
            auto r = setVelocity (notes, { ObjectId { 1 }, ObjectId { 2 } }, 2.0f);
            expectEquals (r[0].velocity, 1.0f);
            expectEquals (r[2].velocity, 0.7f);
            r = remove (notes, { ObjectId { 2 } });
            expectEquals ((int) r.size(), 2);
        }

        beginTest ("snap helpers");
        expectEquals ((int) snap (350, 240), 480 - 240);
        expectEquals ((int) snap (370, 240), 480);
        expectEquals ((int) snapDown (479, 240), 240);
        expectEquals ((int) snap (123, 0), 123);

        beginTest ("note edits merge into one undo step and change pitch/velocity");
        {
            Song song;
            CommandHistory history { song };
            Track t;
            t.id = song.newId();
            t.typeId = "instrument";
            const auto tid = t.id;
            history.perform (std::make_unique<commands::AddTrack> (std::move (t)));
            Region r;
            r.id = song.newId();
            r.kind = Region::Kind::midi;
            r.length = 3840;
            r.notes = notes;
            history.perform (std::make_unique<commands::AddRegion> (tid, r));

            for (int step = 1; step <= 5; ++step)
            {
                auto edited = r;
                edited.notes = move (r.notes, { ObjectId { 1 } }, 0, step); // pitch only
                expect (history.perform (std::make_unique<commands::UpdateRegion> (tid, tid, edited, "Noten verschieben"), true),
                        "pitch-only change is applied");
            }
            history.endGesture();
            expectEquals (song.findTrack (tid)->findRegion (r.id)->notes[0].pitch, 65);

            history.undo();
            expectEquals (song.findTrack (tid)->findRegion (r.id)->notes[0].pitch, 60, "one undo restores the drag start");
        }
    }
};

static NoteOpsTests noteOpsTests;

} // namespace sms
