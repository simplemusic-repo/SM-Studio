// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Model.h"

#include <memory>
#include <vector>

namespace sms
{

/** An undoable change to a Song.

    Every user-visible modification of the document goes through a Command.
    Extensions add features by writing new Command subclasses – they never
    need to touch the undo system itself.
*/
class Command
{
public:
    virtual ~Command() = default;

    /** Shown in the Edit menu, e.g. "Spur hinzufügen". */
    virtual juce::String getName() const = 0;

    /** Applies the change. Return false if nothing could be done – the
        command is then discarded and not put on the undo stack. */
    virtual bool perform (Song&) = 0;

    /** Reverts exactly what perform() did. */
    virtual void undo (Song&) = 0;

    /** Re-applies after undo. Default: perform() again. */
    virtual void redo (Song& s) { perform (s); }

    /** Lets continuous gestures (e.g. dragging a fader) collapse into a single
        undo step. Called on the previous command with the newer one, after the
        newer one has been performed. Return true if `next` was absorbed. */
    virtual bool mergeWith (const Command& next) { juce::ignoreUnused (next); return false; }
};

/** Groups several commands into one undo step. */
class CompoundCommand final : public Command
{
public:
    explicit CompoundCommand (juce::String nameToUse) : name (std::move (nameToUse)) {}

    void add (std::unique_ptr<Command> c) { commands.push_back (std::move (c)); }
    bool isEmpty() const noexcept { return commands.empty(); }

    juce::String getName() const override { return name; }
    bool perform (Song&) override;
    void undo (Song&) override;
    void redo (Song&) override;

    /** Adds an already-performed command (used by CommandHistory transactions). */
    void addPerformed (std::unique_ptr<Command> c) { commands.push_back (std::move (c)); }

private:
    juce::String name;
    std::vector<std::unique_ptr<Command>> commands;
};

//==============================================================================
/** Undo/redo stacks for one Song. */
class CommandHistory
{
public:
    explicit CommandHistory (Song&);

    /** Performs a command and records it.
        @param allowMerge  if true, the command may be merged into the previous
                           one (see Command::mergeWith) – use for continuous
                           gestures and call endGesture() when the gesture ends.
        @returns true if the command changed something. */
    bool perform (std::unique_ptr<Command>, bool allowMerge = false);

    /** Ends merging of continuous gestures (e.g. on mouse up). */
    void endGesture() noexcept { mergeAllowed = false; }

    /** Everything performed between begin/endTransaction becomes one undo step. */
    void beginTransaction (const juce::String& name);
    void endTransaction();

    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    juce::String getUndoName() const;
    juce::String getRedoName() const;

    bool undo();
    bool redo();
    void clear();

    /** Increments whenever the document changes through this history.
        Compare with a saved value to know if there are unsaved changes. */
    std::uint64_t getChangeCounter() const noexcept { return changeCounter; }

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void historyChanged() = 0;
    };

    void addListener (Listener* l) { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    static constexpr size_t maxUndoSteps = 500;

private:
    void push (std::unique_ptr<Command>);
    void changed();

    Song& song;
    std::vector<std::unique_ptr<Command>> undoStack, redoStack;
    std::unique_ptr<CompoundCommand> transaction;
    int transactionDepth = 0;
    bool mergeAllowed = false;
    std::uint64_t changeCounter = 0;
    juce::ListenerList<Listener> listeners;
};

} // namespace sms
