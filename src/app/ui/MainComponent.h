// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "extension/ExtensionApi.h"

namespace sms
{

class Toolbar;
class TransportBar;
class ArrangeView;

/** The main window content. Lays out the toolbar, the panels contributed by
    modules (left / right / bottom), the arrangement and the transport bar,
    and turns registered actions into menus and keyboard shortcuts.

    Every panel can be detached into its own window (e.g. the console on a
    second monitor). Visibility, docking state and window positions are
    stored in the settings and restored on the next start. */
class MainComponent final : public juce::Component,
                            public juce::DragAndDropContainer,
                            public juce::MenuBarModel,
                            private CommandHistory::Listener
{
public:
    /** @param headless  no native menu and no separate windows (screenshots, tests) */
    explicit MainComponent (AppContext&, bool headless = false);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int index, const juce::String& name) override;
    void menuItemSelected (int itemId, int topLevelIndex) override;

    bool isPanelVisible (PanelLocation) const;
    void setPanelVisible (PanelLocation, bool);

    bool isPanelDetached (PanelLocation) const;
    void setPanelDetached (PanelLocation, bool);

    /** Writes panel visibility, docking state and window positions to the settings. */
    void saveLayout();

private:
    class PanelHost;
    class PanelWindow;
    class PanelResizer;

    struct DockedPanel
    {
        juce::String id, title;
        PanelLocation location;
        int size = 0;
        bool visible = true;
        bool detached = false;
        std::unique_ptr<PanelHost> host;
        std::unique_ptr<PanelResizer> resizer; // drag bar at the inner edge while docked
        std::unique_ptr<PanelWindow> window; // only while detached (destroyed before host)
    };

    void historyChanged() override { menuItemsChanged(); }
    void registerViewActions();
    void createPanels();
    DockedPanel* findPanel (PanelLocation);
    const DockedPanel* findPanel (PanelLocation) const;
    void openWindow (DockedPanel&, bool restorePosition);
    void layoutChanged();
    juce::String key (const DockedPanel&, const char* what) const;

    AppContext& ctx;
    bool headless;
    std::unique_ptr<juce::MenuBarComponent> menuBar;
    std::unique_ptr<Toolbar> toolbar;
    std::unique_ptr<TransportBar> transport;
    std::unique_ptr<ArrangeView> arrange;
    std::vector<DockedPanel> panels;
    std::unique_ptr<juce::TooltipWindow> tooltipWindow; // shows the hover help of all components

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace sms
