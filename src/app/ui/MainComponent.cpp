// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"

#include "ArrangeView.h"
#include "Bars.h"
#include "Theme.h"
#include "extension/ExtensionRegistry.h"

namespace sms
{

MainComponent::MainComponent (AppContext& c, bool useNativeMacMenu) : ctx (c), nativeMacMenu (useNativeMacMenu)
{
    registerViewActions();

    toolbar = std::make_unique<Toolbar> (ctx);
    addAndMakeVisible (*toolbar);

    arrange = std::make_unique<ArrangeView> (ctx);
    addAndMakeVisible (*arrange);

    transport = std::make_unique<TransportBar> (ctx, [this] (const juce::String& id)
    {
        auto* a = ctx.registry.findAction (id);
        return a != nullptr && a->isTicked && a->isTicked();
    });
    addAndMakeVisible (*transport);

    createPanels();

   #if JUCE_MAC
    if (nativeMacMenu)
        juce::MenuBarModel::setMacMainMenu (this);
    else
   #endif
    {
        menuBar = std::make_unique<juce::MenuBarComponent> (this);
        addAndMakeVisible (*menuBar);
    }

    ctx.history.addListener (this);
    setWantsKeyboardFocus (true);
    setSize (1600, 950);
}

MainComponent::~MainComponent()
{
    ctx.history.removeListener (this);
   #if JUCE_MAC
    if (nativeMacMenu)
        juce::MenuBarModel::setMacMainMenu (nullptr);
   #endif
}

//==============================================================================
void MainComponent::createPanels()
{
    // the first registered panel per location is shown; more panels per
    // location (tabs) can be added later without changing modules
    for (auto& info : ctx.registry.getPanels())
    {
        const bool taken = std::any_of (panels.begin(), panels.end(), [&] (auto& p) { return p.location == info.location; });
        if (taken || ! info.create)
            continue;

        DockedPanel p { info.location, info.defaultSize, info.visibleByDefault, info.create (ctx) };
        addChildComponent (*p.component);
        p.component->setVisible (p.visible);
        panels.push_back (std::move (p));
    }
}

bool MainComponent::isPanelVisible (PanelLocation loc) const
{
    for (auto& p : panels)
        if (p.location == loc)
            return p.visible;
    return false;
}

void MainComponent::setPanelVisible (PanelLocation loc, bool shouldBeVisible)
{
    for (auto& p : panels)
    {
        if (p.location == loc)
        {
            p.visible = shouldBeVisible;
            p.component->setVisible (shouldBeVisible);
        }
    }
    resized();
    transport->refreshToggles();
    menuItemsChanged();
}

void MainComponent::registerViewActions()
{
    auto addToggle = [this] (const char* id, const char* name, int key, PanelLocation loc, int order)
    {
        ActionInfo a;
        a.id = id;
        a.name = name;
        a.menu = "Ansicht";
        a.menuOrder = order;
        a.keys = { juce::KeyPress (key) };
        a.perform = [this, loc] { setPanelVisible (loc, ! isPanelVisible (loc)); };
        a.isTicked = [this, loc] { return isPanelVisible (loc); };
        ctx.registry.addAction (std::move (a));
    };

    addToggle ("view.console", "Konsole", juce::KeyPress::F3Key, PanelLocation::bottom, 10);
    addToggle ("view.inspector", "Inspector", juce::KeyPress::F4Key, PanelLocation::left, 20);
    addToggle ("view.browser", "Browser", juce::KeyPress::F5Key, PanelLocation::right, 30);

    ActionInfo editor;
    editor.id = "view.editor";
    editor.name = "Editor (folgt)";
    editor.menu = "Ansicht";
    editor.menuOrder = 5;
    editor.keys = { juce::KeyPress (juce::KeyPress::F2Key) };
    editor.perform = [] {};
    editor.isEnabled = [] { return false; };
    editor.isTicked = [] { return false; };
    ctx.registry.addAction (std::move (editor));
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::colours::window);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();

    if (menuBar != nullptr)
        menuBar->setBounds (r.removeFromTop (24));

    toolbar->setBounds (r.removeFromTop (44));
    transport->setBounds (r.removeFromBottom (64));

    auto place = [&] (PanelLocation loc)
    {
        for (auto& p : panels)
        {
            if (p.location != loc || ! p.visible)
                continue;

            switch (loc)
            {
                case PanelLocation::bottom: p.component->setBounds (r.removeFromBottom (juce::jmin (p.size, r.getHeight() / 2))); break;
                case PanelLocation::left:   p.component->setBounds (r.removeFromLeft (p.size)); break;
                case PanelLocation::right:  p.component->setBounds (r.removeFromRight (p.size)); break;
            }
        }
    };

    // like Studio One: the console spans the full width below everything
    place (PanelLocation::bottom);
    place (PanelLocation::left);
    place (PanelLocation::right);
    arrange->setBounds (r);
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (auto* a = ctx.registry.findActionForKey (key))
    {
        if (a->perform && (! a->isEnabled || a->isEnabled()))
        {
            a->perform();
            transport->refreshToggles();
        }
        return true;
    }
    return false;
}

//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    return ctx.registry.getMenuNames();
}

juce::PopupMenu MainComponent::getMenuForIndex (int, const juce::String& menuName)
{
    const auto& actions = ctx.registry.getActions();

    std::vector<size_t> indices;
    for (size_t i = 0; i < actions.size(); ++i)
        if (actions[i].menu == menuName)
            indices.push_back (i);

    std::stable_sort (indices.begin(), indices.end(), [&] (size_t a, size_t b) { return actions[a].menuOrder < actions[b].menuOrder; });

    juce::PopupMenu m;
    for (auto i : indices)
    {
        auto& a = actions[i];
        if (a.separatorBefore)
            m.addSeparator();

        juce::PopupMenu::Item item (a.dynamicName ? a.dynamicName() : a.name);
        item.itemID = (int) i + 1;
        item.isEnabled = ! a.isEnabled || a.isEnabled();
        item.isTicked = a.isTicked && a.isTicked();
        if (! a.keys.empty())
            item.shortcutKeyDescription = a.keys.front().getTextDescriptionWithIcons();
        m.addItem (std::move (item));
    }
    return m;
}

void MainComponent::menuItemSelected (int itemId, int)
{
    const auto& actions = ctx.registry.getActions();
    const auto index = (size_t) (itemId - 1);

    if (itemId > 0 && index < actions.size())
    {
        ctx.registry.invoke (actions[index].id);
        transport->refreshToggles();
    }
}

} // namespace sms
