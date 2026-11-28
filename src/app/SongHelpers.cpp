// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SongHelpers.h"

#include "core/Commands.h"
#include "extension/ExtensionRegistry.h"
#include "ui/Theme.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace sms
{

static juce::AudioFormatManager& formatManager()
{
    static juce::AudioFormatManager fm;
    static const bool registered = [] { fm.registerBasicFormats(); return true; }();
    juce::ignoreUnused (registered);
    return fm;
}

Track createTrack (AppContext& ctx, const juce::String& typeId, const juce::String& name)
{
    auto* type = ctx.registry.findTrackType (typeId);
    jassert (type != nullptr); // track type not registered

    Track t;
    t.id = ctx.song.newId();
    t.typeId = typeId;
    t.colour = theme::trackColour (ctx.song.getNumTracks());

    if (name.isNotEmpty())
    {
        t.name = name;
    }
    else
    {
        int sameType = 0;
        for (auto& other : ctx.song.getTracks())
            if (other.typeId == typeId)
                ++sameType;

        t.name = (type != nullptr ? type->defaultName : typeId) + " " + juce::String (sameType + 1);
    }

    return t;
}

ObjectId addTrack (AppContext& ctx, const juce::String& typeId, int index, const juce::String& name)
{
    auto t = createTrack (ctx, typeId, name);
    const auto id = t.id;

    if (ctx.perform (std::make_unique<commands::AddTrack> (std::move (t), index)))
    {
        ctx.selection.selectTrack (id);
        return id;
    }
    return {};
}

bool isSupportedAudioFile (const juce::File& f)
{
    return f.existsAsFile() && formatManager().findFormatForFileExtension (f.getFileExtension()) != nullptr;
}

bool importAudioFile (AppContext& ctx, const juce::File& file, const DropTarget& target)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager().createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0 || reader->lengthInSamples <= 0)
        return false;

    AudioSource src;
    src.id = ctx.song.newId();
    src.file = file;
    src.sampleRate = reader->sampleRate;
    src.lengthInSamples = reader->lengthInSamples;
    src.numChannels = (int) reader->numChannels;

    const auto& map = ctx.song.getTempoMap();
    const auto startSeconds = map.ticksToSeconds (target.position);
    const auto lengthSeconds = (double) src.lengthInSamples / src.sampleRate;

    Region r;
    r.id = ctx.song.newId();
    r.kind = Region::Kind::audio;
    r.name = file.getFileNameWithoutExtension();
    r.position = target.position;
    r.length = std::max<Tick> (1, map.secondsToTicks (startSeconds + lengthSeconds) - target.position);
    r.sourceId = src.id;

    ctx.history.beginTransaction ("Audiodatei importieren");
    ctx.perform (std::make_unique<commands::AddAudioSource> (src));

    auto trackId = target.trackId;
    auto* existing = ctx.song.findTrack (trackId);

    if (existing == nullptr || existing->typeId != "audio")
    {
        auto t = createTrack (ctx, "audio", file.getFileNameWithoutExtension());
        trackId = t.id;
        const auto index = existing != nullptr ? ctx.song.indexOfTrack (existing->id) + 1 : target.insertIndex;
        ctx.perform (std::make_unique<commands::AddTrack> (std::move (t), index));
    }

    ctx.perform (std::make_unique<commands::AddRegion> (trackId, std::move (r)));
    ctx.history.endTransaction();
    ctx.selection.selectTrack (trackId);
    return true;
}

bool activateBrowserItem (AppContext& ctx, const BrowserItem& item)
{
    auto* handler = ctx.registry.findDropHandler (item.kind);
    if (handler == nullptr)
        return false;

    DropTarget target;
    const auto selected = ctx.selection.getSelectedTrack();
    if (item.kind != "audiofile")
        target.trackId = selected;
    const auto idx = ctx.song.indexOfTrack (selected);
    target.insertIndex = idx >= 0 ? idx + 1 : ctx.song.getNumTracks();
    target.position = ctx.edit.snapTick (ctx.song.getTempoMap().secondsToTicks (ctx.engine.getPositionSeconds()),
                                         ctx.song.getTempoMap());
    return handler->drop (ctx, item, target);
}

void createDemoSong (AppContext& ctx)
{
    using namespace commands;

    const auto bar = ctx.song.getTempoMap().getTicksPerBar();
    const auto beat = ctx.song.getTempoMap().getTicksPerBeat();

    auto makeMidi = [&] (const juce::String& name, Tick pos, int bars, std::initializer_list<int> pitches, Tick step)
    {
        Region r;
        r.id = ctx.song.newId();
        r.kind = Region::Kind::midi;
        r.name = name;
        r.position = pos;
        r.length = bar * bars;
        Tick t = 0;
        size_t i = 0;
        const std::vector<int> p (pitches);
        while (t < r.length)
        {
            Note n;
            n.id = ctx.song.newId();
            n.start = t;
            n.length = step * 3 / 4;
            n.pitch = p[i++ % p.size()];
            r.notes.push_back (n);
            t += step;
        }
        return r;
    };

    auto makeAudio = [&] (const juce::String& name, Tick pos, int bars)
    {
        Region r;
        r.id = ctx.song.newId();
        r.kind = Region::Kind::audio;
        r.name = name;
        r.position = pos;
        r.length = bar * bars;
        return r;
    };

    ctx.history.beginTransaction ("Demo-Song");

    auto drums = createTrack (ctx, "audio", "Drums");
    auto drumsId = drums.id;
    ctx.perform (std::make_unique<AddTrack> (std::move (drums)));
    ctx.perform (std::make_unique<AddRegion> (drumsId, makeAudio ("Drums Intro", 0, 4)));
    ctx.perform (std::make_unique<AddRegion> (drumsId, makeAudio ("Drums Strophe", bar * 4, 8)));

    auto bass = createTrack (ctx, "instrument", "Bass");
    bass.instrumentName = "SM Testsynth";
    bass.instrumentPluginId = "builtin:testsynth";
    auto bassId = bass.id;
    ctx.perform (std::make_unique<AddTrack> (std::move (bass)));
    ctx.perform (std::make_unique<AddRegion> (bassId, makeMidi ("Bass A", bar * 4, 4, { 36, 36, 43, 41, 36, 39, 41, 43 }, beat / 2)));
    ctx.perform (std::make_unique<AddRegion> (bassId, makeMidi ("Bass B", bar * 8, 4, { 41, 41, 48, 46, 43, 43, 50, 48 }, beat / 2)));

    auto keys = createTrack (ctx, "instrument", "Keys");
    keys.instrumentName = "SM Testsynth";
    keys.instrumentPluginId = "builtin:testsynth";
    auto keysId = keys.id;
    ctx.perform (std::make_unique<AddTrack> (std::move (keys)));
    ctx.perform (std::make_unique<AddRegion> (keysId, makeMidi ("Akkorde", 0, 12, { 60, 64, 67, 72, 62, 65, 69, 74 }, beat)));

    auto vox = createTrack (ctx, "audio", "Lead Vocal");
    auto voxId = vox.id;
    ctx.perform (std::make_unique<AddTrack> (std::move (vox)));
    ctx.perform (std::make_unique<AddRegion> (voxId, makeAudio ("Vocal Take 3", bar * 4, 8)));
    InsertSlot eq;
    eq.id = ctx.song.newId();
    eq.name = "SM EQ";
    eq.pluginId = "builtin:eq";
    ctx.perform (std::make_unique<AddInsert> (voxId, eq));

    ctx.perform (std::make_unique<SetTrackParam> (voxId, SetTrackParam::Param::volumeDb, -3.5f));
    ctx.perform (std::make_unique<SetTrackParam> (keysId, SetTrackParam::Param::pan, -0.3f));

    ctx.history.endTransaction();
    ctx.history.clear();
    ctx.selection.selectTrack (bassId);
}

} // namespace sms
