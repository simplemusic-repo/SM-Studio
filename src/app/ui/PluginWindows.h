// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "engine/PluginHost.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>

namespace sms
{

/** Opens and tracks the editor windows of plug-ins (one per owner: track
    instrument or insert slot). Window positions are kept while the app runs. */
class PluginWindows : private juce::Timer
{
public:
    explicit PluginWindows (PluginHost&);
    ~PluginWindows();

    /** Opens (or brings to front) the editor of the instance owned by `owner`.
        Returns false if the plug-in is not loaded (yet). */
    bool open (ObjectId owner, const juce::String& pluginId, const juce::String& title);

    void close (ObjectId owner);
    void closeAll();
    bool isOpen (ObjectId owner) const { return windows.count (owner) > 0; }

private:
    void timerCallback() override;
    class Window;

    PluginHost& host;
    std::map<ObjectId, std::unique_ptr<Window>> windows;
    std::map<ObjectId, juce::Point<int>> lastPositions;
};

} // namespace sms
