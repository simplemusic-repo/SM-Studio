// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Command.h"

namespace sms
{

bool CompoundCommand::perform (Song& s)
{
    bool any = false;
    for (auto& c : commands)
        any = c->perform (s) || any;
    return any;
}

void CompoundCommand::undo (Song& s)
{
    for (auto it = commands.rbegin(); it != commands.rend(); ++it)
        (*it)->undo (s);
}

void CompoundCommand::redo (Song& s)
{
    for (auto& c : commands)
        c->redo (s);
}

//==============================================================================
CommandHistory::CommandHistory (Song& s) : song (s) {}

bool CommandHistory::perform (std::unique_ptr<Command> cmd, bool allowMerge)
{
    jassert (cmd != nullptr);

    if (! cmd->perform (song))
        return false;

    redoStack.clear();

    if (transaction != nullptr)
    {
        transaction->addPerformed (std::move (cmd));
    }
    else if (allowMerge && mergeAllowed && ! undoStack.empty() && undoStack.back()->mergeWith (*cmd))
    {
        // absorbed into the previous step
    }
    else
    {
        push (std::move (cmd));
    }

    mergeAllowed = allowMerge;
    changed();
    return true;
}

void CommandHistory::beginTransaction (const juce::String& name)
{
    if (transactionDepth++ == 0)
        transaction = std::make_unique<CompoundCommand> (name);

    mergeAllowed = false;
}

void CommandHistory::endTransaction()
{
    jassert (transactionDepth > 0);

    if (transactionDepth == 0 || --transactionDepth > 0)
        return;

    auto t = std::move (transaction);

    if (! t->isEmpty())
    {
        push (std::move (t));
        changed();
    }
}

juce::String CommandHistory::getUndoName() const
{
    return undoStack.empty() ? juce::String() : undoStack.back()->getName();
}

juce::String CommandHistory::getRedoName() const
{
    return redoStack.empty() ? juce::String() : redoStack.back()->getName();
}

bool CommandHistory::undo()
{
    if (undoStack.empty() || transaction != nullptr)
        return false;

    auto c = std::move (undoStack.back());
    undoStack.pop_back();
    c->undo (song);
    redoStack.push_back (std::move (c));
    mergeAllowed = false;
    changed();
    return true;
}

bool CommandHistory::redo()
{
    if (redoStack.empty() || transaction != nullptr)
        return false;

    auto c = std::move (redoStack.back());
    redoStack.pop_back();
    c->redo (song);
    undoStack.push_back (std::move (c));
    mergeAllowed = false;
    changed();
    return true;
}

void CommandHistory::clear()
{
    undoStack.clear();
    redoStack.clear();
    transaction.reset();
    transactionDepth = 0;
    mergeAllowed = false;
    listeners.call ([] (Listener& l) { l.historyChanged(); });
}

void CommandHistory::push (std::unique_ptr<Command> c)
{
    undoStack.push_back (std::move (c));

    if (undoStack.size() > maxUndoSteps)
        undoStack.erase (undoStack.begin());
}

void CommandHistory::changed()
{
    ++changeCounter;
    listeners.call ([] (Listener& l) { l.historyChanged(); });
}

} // namespace sms
