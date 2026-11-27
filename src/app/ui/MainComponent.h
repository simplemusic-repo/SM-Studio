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
    and turns registered actions into menus and keyboard shortcuts. */
class MainComponent final : public juce::Component,
                            public juce::DragAndDropContainer,
                            public juce::MenuBarModel,
                            private CommandHistory::Listener
{
public:
    explicit MainComponent (AppContext&, bool useNativeMacMenu = true);
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

private:
    void historyChanged() override { menuItemsChanged(); }
    void registerViewActions();
    void createPanels();

    struct DockedPanel
    {
        PanelLocation location;
        int size;
        bool visible;
        std::unique_ptr<juce::Component> component;
    };

    AppContext& ctx;
    bool nativeMacMenu;
    std::unique_ptr<juce::MenuBarComponent> menuBar;
    std::unique_ptr<Toolbar> toolbar;
    std::unique_ptr<TransportBar> transport;
    std::unique_ptr<ArrangeView> arrange;
    std::vector<DockedPanel> panels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace sms
