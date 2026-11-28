// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

/*  The extension API.

    Every feature of SM Studio – including the built-in ones – is a Module that
    registers what it provides with the ExtensionRegistry:

      - actions      (menu entries + keyboard shortcuts)
      - panels       (Inspector, Browser, Console, ... docked left/right/bottom)
      - track types  (audio, instrument, ...)
      - browser sources (tabs in the browser: instruments, effects, files ...)
      - drop handlers (what happens when a browser item is dropped on the arrangement)

    The core never knows about concrete modules. To add a feature, write a new
    Module and register it in Modules.cpp. Later this same API will be exposed
    as a C interface so extensions can be loaded from DLL/dylib files.
*/

#include "core/Command.h"
#include "core/Model.h"
#include "core/Selection.h"
#include "engine/AudioEngine.h"
#include "engine/AudioPool.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

namespace sms
{

class ExtensionRegistry;

/** Editing state shared by toolbar, arrangement and editors. Not undoable. */
struct EditState
{
    enum Tool { arrow = 0, range, split, eraser, pencil, mute };
    enum Snap { snapOff = 0, snapBar, snapBeat, snap8th, snap16th };

    juce::Value tool { juce::var ((int) arrow) };
    juce::Value snap { juce::var ((int) snapBar) };

    /** Rounds a position to the current snap grid. */
    Tick snapTick (Tick t, const TempoMap& map) const
    {
        Tick grid = 0;
        switch ((int) snap.getValue())
        {
            case snapBar:  grid = map.getTicksPerBar(); break;
            case snapBeat: grid = map.getTicksPerBeat(); break;
            case snap8th:  grid = ticksPerQuarter / 2; break;
            case snap16th: grid = ticksPerQuarter / 4; break;
            default: break;
        }
        if (grid <= 0)
            return std::max<Tick> (0, t);
        return std::max<Tick> (0, ((t + grid / 2) / grid) * grid);
    }
};

/** Everything a module may use. Passed to modules and panel factories. */
struct AppContext
{
    Song& song;
    CommandHistory& history;
    Selection& selection;
    AudioEngine& engine;
    AudioPool& pool;
    ExtensionRegistry& registry;
    EditState& edit;

    /** Persistent user settings (window positions, preferences ...).
        Saved automatically; keys are namespaced, e.g. "panel.console.visible". */
    juce::PropertySet& settings;

    /** Shorthand: performs an undoable command. */
    bool perform (std::unique_ptr<Command> c, bool allowMerge = false) { return history.perform (std::move (c), allowMerge); }
};

//==============================================================================
struct ActionInfo
{
    juce::String id;        // "transport.play"
    juce::String name;      // shown in menus
    juce::String menu;      // top-level menu: "Datei", "Bearbeiten", "Spur", "Transport", "Ansicht"
    std::vector<juce::KeyPress> keys;
    std::function<void()> perform;
    std::function<bool()> isEnabled; // optional
    std::function<bool()> isTicked;  // optional (toggle actions)
    std::function<juce::String()> dynamicName; // optional, e.g. "Rückgängig: Spur löschen"
    int menuOrder = 0;
    bool separatorBefore = false;
};

//==============================================================================
enum class PanelLocation { left, right, bottom };

struct PanelInfo
{
    juce::String id;
    juce::String title;
    PanelLocation location = PanelLocation::right;
    int defaultSize = 280; // width for left/right, height for bottom
    bool visibleByDefault = true;
    std::function<std::unique_ptr<juce::Component> (AppContext&)> create;
};

//==============================================================================
struct TrackTypeInfo
{
    juce::String id;          // "audio"
    juce::String displayName; // "Audio"
    juce::String defaultName; // name for new tracks, a number is appended
    juce::Colour colour;      // default colour
};

//==============================================================================
/** Something that can be dragged from the browser. */
struct BrowserItem
{
    juce::String kind;     // "instrument", "effect", "audiofile" ... (matches a drop handler)
    juce::String id;       // plug-in id, file path, ...
    juce::String name;
    juce::String category; // grouping in the list ("Synth", "EQ", folder name ...)

    /** Encodes the item for juce::DragAndDropContainer. */
    juce::var toDragDescription() const;
    static std::optional<BrowserItem> fromDragDescription (const juce::var&);
};

/** One tab of the browser. */
class BrowserSource
{
public:
    virtual ~BrowserSource() = default;
    virtual juce::String getId() const = 0;
    virtual juce::String getTitle() const = 0;
    virtual std::vector<BrowserItem> getItems() = 0;

    /** Sources call this when their item list changed (e.g. after a plug-in scan). */
    std::function<void()> onItemsChanged;
};

//==============================================================================
/** Where something was dropped on the arrangement. */
struct DropTarget
{
    ObjectId trackId;     // track under the mouse, invalid = empty area
    int insertIndex = -1; // where a new track should go
    Tick position = 0;    // musical position under the mouse
};

struct DropHandlerInfo
{
    juce::String itemKind;
    /** Returns true if the drop was handled. Should perform undoable commands. */
    std::function<bool (AppContext&, const BrowserItem&, const DropTarget&)> drop;
};

//==============================================================================
/** A feature package. */
class Module
{
public:
    virtual ~Module() = default;
    virtual juce::String getId() const = 0;
    virtual juce::String getName() const = 0;

    /** Register actions, panels, track types ... */
    virtual void initialise (AppContext&) = 0;

    /** Called before the app shuts down (stop threads, save settings ...). */
    virtual void shutdown() {}
};

} // namespace sms
