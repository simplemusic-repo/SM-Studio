// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Ids.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <vector>

namespace sms
{

/** What the user has selected. Not part of the document and not undoable. */
class Selection
{
public:
    ObjectId getSelectedTrack() const noexcept { return track; }

    // --- events (regions) ---------------------------------------------------
    const std::vector<ObjectId>& getSelectedRegions() const noexcept { return regions; }
    bool isRegionSelected (ObjectId id) const { return std::find (regions.begin(), regions.end(), id) != regions.end(); }

    void setSelectedRegions (std::vector<ObjectId> ids)
    {
        if (ids == regions)
            return;
        regions = std::move (ids);
        notify();
    }

    void addRegion (ObjectId id)
    {
        if (! isRegionSelected (id))
        {
            regions.push_back (id);
            notify();
        }
    }

    void toggleRegion (ObjectId id)
    {
        auto it = std::find (regions.begin(), regions.end(), id);
        if (it != regions.end())
            regions.erase (it);
        else
            regions.push_back (id);
        notify();
    }

    void clearRegions() { setSelectedRegions ({}); }

    void selectTrack (ObjectId id)
    {
        if (id == track)
            return;
        track = id;
        notify();
    }

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void selectionChanged() = 0;
    };

    void addListener (Listener* l) { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

private:
    void notify() { listeners.call ([] (Listener& l) { l.selectionChanged(); }); }

    ObjectId track;
    std::vector<ObjectId> regions;
    juce::ListenerList<Listener> listeners;
};

} // namespace sms
