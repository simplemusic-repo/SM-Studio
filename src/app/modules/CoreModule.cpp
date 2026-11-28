// SPDX-License-Identifier: AGPL-3.0-or-later
// Track types and the basic File / Edit / Track actions.

#include "Modules.h"

#include <limits>

#include "SongHelpers.h"
#include "core/Commands.h"
#include "core/RegionOps.h"
#include "extension/ExtensionRegistry.h"
#include "ui/Theme.h"

namespace sms
{

namespace
{
    juce::KeyPress cmd (int key, int extraMods = 0)
    {
        return juce::KeyPress (key, juce::ModifierKeys (juce::ModifierKeys::commandModifier | extraMods), 0);
    }

    class CoreModule final : public Module
    {
    public:
        juce::String getId() const override { return "sms.core"; }
        juce::String getName() const override { return "Grundfunktionen"; }

        void initialise (AppContext& ctx) override
        {
            auto& reg = ctx.registry;

            reg.addTrackType ({ "audio", "Audio", "Audio", theme::trackColour (0) });
            reg.addTrackType ({ "instrument", "Instrument", "Instrument", theme::trackColour (1) });

            // --- Datei ---------------------------------------------------------
            {
                ActionInfo a;
                a.id = "file.quit";
                a.name = "Beenden";
                a.menu = "Datei";
                a.menuOrder = 100;
                a.keys = { cmd ('q') };
                a.perform = [] { juce::JUCEApplicationBase::quit(); };
                reg.addAction (std::move (a));
            }

            // --- Bearbeiten ------------------------------------------------------
            {
                ActionInfo a;
                a.id = "edit.undo";
                a.name = u8"Rückgängig";
                a.menu = "Bearbeiten";
                a.menuOrder = 10;
                a.keys = { cmd ('z') };
                a.perform = [&ctx] { ctx.history.undo(); };
                a.isEnabled = [&ctx] { return ctx.history.canUndo(); };
                a.dynamicName = [&ctx] { return ctx.history.canUndo() ? juce::String (u8"Rückgängig: ") + ctx.history.getUndoName() : juce::String (u8"Rückgängig"); };
                reg.addAction (std::move (a));
            }
            {
                ActionInfo a;
                a.id = "edit.redo";
                a.name = "Wiederherstellen";
                a.menu = "Bearbeiten";
                a.menuOrder = 20;
                a.keys = { cmd ('z', juce::ModifierKeys::shiftModifier), cmd ('y') };
                a.perform = [&ctx] { ctx.history.redo(); };
                a.isEnabled = [&ctx] { return ctx.history.canRedo(); };
                a.dynamicName = [&ctx] { return ctx.history.canRedo() ? "Wiederherstellen: " + ctx.history.getRedoName() : juce::String ("Wiederherstellen"); };
                reg.addAction (std::move (a));
            }

            // --- events ----------------------------------------------------------
            auto selectedEvents = [&ctx]
            {
                std::vector<std::pair<ObjectId, Region>> out;
                for (auto id : ctx.selection.getSelectedRegions())
                    if (auto* t = ctx.song.findTrackOfRegion (id))
                        out.emplace_back (t->id, *t->findRegion (id));
                return out;
            };
            auto hasEvents = [&ctx] { return ! ctx.selection.getSelectedRegions().empty(); };

            {
                ActionInfo a;
                a.id = "edit.delete";
                a.name = u8"Löschen";
                a.menu = "Bearbeiten";
                a.menuOrder = 30;
                a.separatorBefore = true;
                a.keys = { juce::KeyPress (juce::KeyPress::deleteKey), juce::KeyPress (juce::KeyPress::backspaceKey) };
                a.isEnabled = [&ctx, hasEvents] { return hasEvents() || ctx.song.findTrack (ctx.selection.getSelectedTrack()) != nullptr; };
                a.dynamicName = [hasEvents] { return hasEvents() ? juce::String (u8"Events löschen") : juce::String (u8"Spur löschen"); };
                a.perform = [&ctx, selectedEvents]
                {
                    auto events = selectedEvents();
                    if (events.empty())
                    {
                        ctx.registry.invoke ("track.delete"); // nothing selected on the timeline: delete the track
                        return;
                    }
                    ctx.history.beginTransaction (u8"Events löschen");
                    for (auto& [trackId, r] : events)
                        ctx.perform (std::make_unique<commands::RemoveRegion> (trackId, r.id));
                    ctx.history.endTransaction();
                    ctx.selection.clearRegions();
                };
                reg.addAction (std::move (a));
            }
            {
                ActionInfo a;
                a.id = "edit.duplicate";
                a.name = "Duplizieren";
                a.menu = "Bearbeiten";
                a.menuOrder = 40;
                a.keys = { juce::KeyPress ('d'), cmd ('d') };
                a.isEnabled = hasEvents;
                a.perform = [&ctx, selectedEvents]
                {
                    auto events = selectedEvents();
                    if (events.empty())
                        return;
                    Tick first = std::numeric_limits<Tick>::max(), last = 0;
                    for (auto& [t, r] : events)
                    {
                        first = std::min (first, r.position);
                        last = std::max (last, r.getEnd());
                    }
                    const auto offset = last - first; // place the copy right after the selection
                    std::vector<ObjectId> copies;
                    ctx.history.beginTransaction ("Duplizieren");
                    for (auto& [trackId, r] : events)
                    {
                        auto c = regionops::duplicate (r, ctx.song.getIds());
                        c.position += offset;
                        copies.push_back (c.id);
                        ctx.perform (std::make_unique<commands::AddRegion> (trackId, std::move (c)));
                    }
                    ctx.history.endTransaction();
                    ctx.selection.setSelectedRegions (copies);
                };
                reg.addAction (std::move (a));
            }
            {
                ActionInfo a;
                a.id = "edit.splitAtCursor";
                a.name = "Am Abspielpunkt teilen";
                a.menu = "Bearbeiten";
                a.menuOrder = 50;
                a.keys = { juce::KeyPress ('x', juce::ModifierKeys::altModifier, 0) };
                a.perform = [&ctx, selectedEvents]
                {
                    const auto& map = ctx.song.getTempoMap();
                    const auto at = map.secondsToTicks (ctx.engine.getPositionSeconds());
                    auto events = selectedEvents();

                    // nothing selected: split the events of the selected track
                    if (events.empty())
                        if (auto* t = ctx.song.findTrack (ctx.selection.getSelectedTrack()))
                            for (auto& r : t->regions)
                                events.emplace_back (t->id, r);

                    std::vector<ObjectId> right;
                    ctx.history.beginTransaction ("Event teilen");
                    for (auto& [trackId, r] : events)
                    {
                        if (auto parts = regionops::split (r, at, ctx.song.findAudioSource (r.sourceId), map, ctx.song.getIds()))
                        {
                            right.push_back (parts->second.id);
                            ctx.perform (std::make_unique<commands::UpdateRegion> (trackId, trackId, parts->first, "Event teilen"));
                            ctx.perform (std::make_unique<commands::AddRegion> (trackId, parts->second));
                        }
                    }
                    ctx.history.endTransaction();
                    if (! right.empty())
                        ctx.selection.setSelectedRegions (right);
                };
                reg.addAction (std::move (a));
            }
            {
                ActionInfo a;
                a.id = "edit.toggleMuteEvents";
                a.name = "Events stumm ein/aus";
                a.menu = "Bearbeiten";
                a.menuOrder = 60;
                a.keys = { juce::KeyPress ('m', juce::ModifierKeys::shiftModifier, 0) };
                a.isEnabled = hasEvents;
                a.perform = [&ctx, selectedEvents]
                {
                    auto events = selectedEvents();
                    if (events.empty())
                        return;
                    const bool mute = ! events.front().second.muted;
                    ctx.history.beginTransaction ("Events stummschalten");
                    for (auto& [trackId, r] : events)
                    {
                        auto m = r;
                        m.muted = mute;
                        ctx.perform (std::make_unique<commands::UpdateRegion> (trackId, trackId, m, "Events stummschalten"));
                    }
                    ctx.history.endTransaction();
                };
                reg.addAction (std::move (a));
            }
            {
                ActionInfo a;
                a.id = "edit.selectAll";
                a.name = u8"Alle Events auswählen";
                a.menu = "Bearbeiten";
                a.menuOrder = 70;
                a.separatorBefore = true;
                a.keys = { cmd ('a') };
                a.perform = [&ctx]
                {
                    std::vector<ObjectId> ids;
                    for (auto& t : ctx.song.getTracks())
                        for (auto& r : t.regions)
                            ids.push_back (r.id);
                    ctx.selection.setSelectedRegions (ids);
                };
                reg.addAction (std::move (a));
            }
            {
                ActionInfo a;
                a.id = "edit.deselect";
                a.name = u8"Auswahl aufheben";
                a.menu = "Bearbeiten";
                a.menuOrder = 80;
                a.keys = { juce::KeyPress (juce::KeyPress::escapeKey) };
                a.perform = [&ctx] { ctx.selection.clearRegions(); };
                reg.addAction (std::move (a));
            }

            // tools 1..6
            const char* toolNames[] = { "Pfeil", "Bereich", "Teilen", "Radiergummi", "Stift", "Stummschalten" };
            for (int i = 0; i < 6; ++i)
            {
                ActionInfo a;
                a.id = "edit.tool." + juce::String (i);
                a.name = juce::String ("Werkzeug: ") + toolNames[i];
                a.menu = "Bearbeiten";
                a.menuOrder = 100 + i;
                a.separatorBefore = i == 0;
                a.keys = { juce::KeyPress ('1' + i) };
                a.perform = [&ctx, i] { ctx.edit.tool = i; };
                a.isTicked = [&ctx, i] { return (int) ctx.edit.tool.getValue() == i; };
                reg.addAction (std::move (a));
            }

            // --- Spur ------------------------------------------------------------
            auto insertIndex = [&ctx]
            {
                const auto i = ctx.song.indexOfTrack (ctx.selection.getSelectedTrack());
                return i >= 0 ? i + 1 : -1;
            };

            {
                ActionInfo a;
                a.id = "track.addAudio";
                a.name = u8"Audiospur hinzufügen";
                a.menu = "Spur";
                a.menuOrder = 10;
                a.keys = { juce::KeyPress ('t') };
                a.perform = [&ctx, insertIndex] { addTrack (ctx, "audio", insertIndex()); };
                reg.addAction (std::move (a));
            }
            {
                ActionInfo a;
                a.id = "track.addInstrument";
                a.name = u8"Instrumentenspur hinzufügen";
                a.menu = "Spur";
                a.menuOrder = 20;
                a.keys = { juce::KeyPress ('T', juce::ModifierKeys::shiftModifier, 0) };
                a.perform = [&ctx, insertIndex] { addTrack (ctx, "instrument", insertIndex()); };
                reg.addAction (std::move (a));
            }
            {
                ActionInfo a;
                a.id = "track.delete";
                a.name = u8"Ausgewählte Spur löschen";
                a.menu = "Spur";
                a.menuOrder = 30;
                a.separatorBefore = true;
                a.isEnabled = [&ctx] { return ctx.song.findTrack (ctx.selection.getSelectedTrack()) != nullptr; };
                a.perform = [&ctx]
                {
                    const auto id = ctx.selection.getSelectedTrack();
                    const auto index = ctx.song.indexOfTrack (id);
                    if (ctx.perform (std::make_unique<commands::RemoveTrack> (id)))
                    {
                        // select the neighbour so repeated deletes work
                        const auto& tracks = ctx.song.getTracks();
                        if (! tracks.empty())
                            ctx.selection.selectTrack (tracks[(size_t) juce::jlimit (0, (int) tracks.size() - 1, index)].id);
                        else
                            ctx.selection.selectTrack ({});
                    }
                };
                reg.addAction (std::move (a));
            }

            auto selectRelative = [&ctx] (int delta)
            {
                const auto& tracks = ctx.song.getTracks();
                if (tracks.empty())
                    return;
                auto i = ctx.song.indexOfTrack (ctx.selection.getSelectedTrack());
                i = i < 0 ? 0 : juce::jlimit (0, (int) tracks.size() - 1, i + delta);
                ctx.selection.selectTrack (tracks[(size_t) i].id);
            };

            {
                ActionInfo a;
                a.id = "track.selectPrevious";
                a.name = u8"Vorherige Spur auswählen";
                a.menu = "Spur";
                a.menuOrder = 40;
                a.separatorBefore = true;
                a.keys = { juce::KeyPress (juce::KeyPress::upKey) };
                a.perform = [selectRelative] { selectRelative (-1); };
                reg.addAction (std::move (a));
            }
            {
                ActionInfo a;
                a.id = "track.selectNext";
                a.name = u8"Nächste Spur auswählen";
                a.menu = "Spur";
                a.menuOrder = 50;
                a.keys = { juce::KeyPress (juce::KeyPress::downKey) };
                a.perform = [selectRelative] { selectRelative (1); };
                reg.addAction (std::move (a));
            }
            {
                ActionInfo a;
                a.id = "track.toggleMute";
                a.name = "Spur stummschalten";
                a.menu = "Spur";
                a.menuOrder = 60;
                a.separatorBefore = true;
                a.keys = { juce::KeyPress ('m') };
                a.isEnabled = [&ctx] { return ctx.song.findTrack (ctx.selection.getSelectedTrack()) != nullptr; };
                a.perform = [&ctx]
                {
                    if (auto* t = ctx.song.findTrack (ctx.selection.getSelectedTrack()))
                        ctx.perform (std::make_unique<commands::SetTrackParam> (t->id, commands::SetTrackParam::Param::mute, t->mute ? 0.0f : 1.0f));
                };
                reg.addAction (std::move (a));
            }
            {
                ActionInfo a;
                a.id = "track.toggleSolo";
                a.name = "Spur solo";
                a.menu = "Spur";
                a.menuOrder = 70;
                a.keys = { juce::KeyPress ('s') };
                a.isEnabled = [&ctx] { return ctx.song.findTrack (ctx.selection.getSelectedTrack()) != nullptr; };
                a.perform = [&ctx]
                {
                    if (auto* t = ctx.song.findTrack (ctx.selection.getSelectedTrack()))
                        ctx.perform (std::make_unique<commands::SetTrackParam> (t->id, commands::SetTrackParam::Param::solo, t->solo ? 0.0f : 1.0f));
                };
                reg.addAction (std::move (a));
            }
        }
    };
}

std::unique_ptr<Module> createCoreModule() { return std::make_unique<CoreModule>(); }

} // namespace sms
