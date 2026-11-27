// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "ExtensionApi.h"

namespace sms
{

/** Central list of everything modules have registered. */
class ExtensionRegistry
{
public:
    // --- actions --------------------------------------------------------------
    void addAction (ActionInfo);
    const std::vector<ActionInfo>& getActions() const noexcept { return actions; }
    const ActionInfo* findAction (const juce::String& id) const;
    const ActionInfo* findActionForKey (const juce::KeyPress&) const;

    /** Runs an action if it exists and is enabled. */
    bool invoke (const juce::String& id) const;

    /** Top-level menu names in registration order. */
    juce::StringArray getMenuNames() const;

    // --- panels ---------------------------------------------------------------
    void addPanel (PanelInfo p) { panels.push_back (std::move (p)); }
    const std::vector<PanelInfo>& getPanels() const noexcept { return panels; }

    // --- track types ----------------------------------------------------------
    void addTrackType (TrackTypeInfo t) { trackTypes.push_back (std::move (t)); }
    const std::vector<TrackTypeInfo>& getTrackTypes() const noexcept { return trackTypes; }
    const TrackTypeInfo* findTrackType (const juce::String& id) const;

    // --- browser --------------------------------------------------------------
    void addBrowserSource (std::shared_ptr<BrowserSource> s) { browserSources.push_back (std::move (s)); }
    const std::vector<std::shared_ptr<BrowserSource>>& getBrowserSources() const noexcept { return browserSources; }

    // --- drag & drop ----------------------------------------------------------
    void addDropHandler (DropHandlerInfo d) { dropHandlers.push_back (std::move (d)); }
    const DropHandlerInfo* findDropHandler (const juce::String& itemKind) const;

    // --- modules --------------------------------------------------------------
    void addModule (std::unique_ptr<Module> m) { modules.push_back (std::move (m)); }
    void initialiseModules (AppContext&);
    void shutdownModules();
    const std::vector<std::unique_ptr<Module>>& getModules() const noexcept { return modules; }

private:
    std::vector<ActionInfo> actions;
    std::vector<PanelInfo> panels;
    std::vector<TrackTypeInfo> trackTypes;
    std::vector<std::shared_ptr<BrowserSource>> browserSources;
    std::vector<DropHandlerInfo> dropHandlers;
    std::vector<std::unique_ptr<Module>> modules;
};

} // namespace sms
