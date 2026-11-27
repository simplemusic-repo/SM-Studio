// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Ids.h"

#include <juce_core/juce_core.h>

namespace sms
{

/** What the user has selected. Not part of the document and not undoable. */
class Selection
{
public:
    ObjectId getSelectedTrack() const noexcept { return track; }

    void selectTrack (ObjectId id)
    {
        if (id == track)
            return;
        track = id;
        listeners.call ([] (Listener& l) { l.selectionChanged(); });
    }

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void selectionChanged() = 0;
    };

    void addListener (Listener* l) { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

private:
    ObjectId track;
    juce::ListenerList<Listener> listeners;
};

} // namespace sms
