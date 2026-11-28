// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"

#include "ArrangeView.h"
#include "Bars.h"
#include "Theme.h"
#include "extension/ExtensionRegistry.h"

namespace sms
{

//==============================================================================
/** Wraps a panel and adds the detach / dock button in its header. */
class MainComponent::PanelHost final : public juce::Component
{
public:
    PanelHost (std::unique_ptr<juce::Component> c, const juce::String& panelTitle)
        : content (std::move (c)), title (panelTitle)
    {
        addAndMakeVisible (*content);
        dockButton.setDrawBackground (false);
        dockButton.setWantsKeyboardFocus (false);
        dockButton.onClick = [this] { if (onToggleDock) onToggleDock(); };
        addAndMakeVisible (dockButton);
        setDetached (false);
    }

    void setDetached (bool detached)
    {
        dockButton.setIcon (detached ? theme::icons::dock() : theme::icons::detach());
        dockButton.setTooltip (detached ? title + juce::String (u8" wieder andocken")
                                        : title + juce::String (u8" abkoppeln (eigenes Fenster, z. B. für einen zweiten Bildschirm)"));
    }

    void resized() override
    {
        content->setBounds (getLocalBounds());
        dockButton.setBounds (getWidth() - 28, 3, 22, 22);
        dockButton.toFront (false);
    }

    std::function<void()> onToggleDock;

private:
    std::unique_ptr<juce::Component> content;
    juce::String title;
    theme::IconButton dockButton { "Abkoppeln", theme::icons::detach() };
};

//==============================================================================
/** A separate top-level window showing a detached panel. */
class MainComponent::PanelWindow final : public juce::DocumentWindow
{
public:
    PanelWindow (const juce::String& title, PanelHost& host)
        : juce::DocumentWindow (juce::String (u8"SM Studio – ") + title, theme::colours::window, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentNonOwned (&host, false);
        setResizable (true, false);
        setResizeLimits (320, 160, 10000, 10000);
    }

    void closeButtonPressed() override
    {
        if (onClose)
            onClose();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        // shortcuts (space = play ...) also work while this window has focus
        return onKey != nullptr && onKey (key);
    }

    std::function<void()> onClose;
    std::function<bool (const juce::KeyPress&)> onKey;
};

//==============================================================================
/** Thin bar at the inner edge of a docked panel. Dragging it resizes the panel. */
class MainComponent::PanelResizer final : public juce::Component,
                                          public juce::SettableTooltipClient
{
public:
    explicit PanelResizer (PanelLocation l) : location (l)
    {
        setMouseCursor (l == PanelLocation::bottom ? juce::MouseCursor::UpDownResizeCursor
                                                   : juce::MouseCursor::LeftRightResizeCursor);
        setTooltip (u8"Ziehen: Größe ändern");
        setRepaintsOnMouseActivity (true);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (isMouseOverOrDragging() ? theme::colours::accent.withAlpha (0.8f) : theme::colours::border);
        if (location == PanelLocation::bottom)
            g.fillRect (getLocalBounds().withSizeKeepingCentre (getWidth(), 2));
        else
            g.fillRect (getLocalBounds().withSizeKeepingCentre (2, getHeight()));
    }

    void mouseDown (const juce::MouseEvent&) override { startSize = getSize ? getSize() : 0; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! setSize)
            return;
        const auto dx = e.getDistanceFromDragStartX(), dy = e.getDistanceFromDragStartY();
        switch (location)
        {
            case PanelLocation::bottom: setSize (startSize - dy); break;
            case PanelLocation::left:   setSize (startSize + dx); break;
            case PanelLocation::right:  setSize (startSize - dx); break;
        }
    }

    void mouseUp (const juce::MouseEvent&) override { if (onFinished) onFinished(); }

    std::function<int()> getSize;
    std::function<void (int)> setSize;
    std::function<void()> onFinished;

private:
    PanelLocation location;
    int startSize = 0;
};

//==============================================================================
MainComponent::MainComponent (AppContext& c, bool isHeadless) : ctx (c), headless (isHeadless)
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
    if (! headless)
        juce::MenuBarModel::setMacMainMenu (this);
    else
   #endif
    {
        menuBar = std::make_unique<juce::MenuBarComponent> (this);
        addAndMakeVisible (*menuBar);
    }

    if (! headless)
        tooltipWindow = std::make_unique<juce::TooltipWindow> (nullptr, 500);

    ctx.history.addListener (this);
    setWantsKeyboardFocus (true);
    setSize (1600, 950);
}

MainComponent::~MainComponent()
{
    saveLayout();
    ctx.history.removeListener (this);

    for (auto& p : panels)
        p.window.reset();

   #if JUCE_MAC
    if (! headless)
        juce::MenuBarModel::setMacMainMenu (nullptr);
   #endif
}

//==============================================================================
juce::String MainComponent::key (const DockedPanel& p, const char* what) const
{
    return "panel." + p.id + "." + what;
}

void MainComponent::createPanels()
{
    // several panels may share a location; only one docked panel per location is visible
    for (auto& info : ctx.registry.getPanels())
    {
        if (! info.create)
            continue;

        DockedPanel p;
        p.id = info.id;
        p.title = info.title;
        p.location = info.location;
        p.size = ctx.settings.getIntValue ("panel." + info.id + ".size", info.defaultSize);
        p.visible = ctx.settings.getBoolValue ("panel." + info.id + ".visible", info.visibleByDefault);
        p.detached = ! headless && ctx.settings.getBoolValue ("panel." + info.id + ".detached", false);
        p.host = std::make_unique<PanelHost> (info.create (ctx), info.title);
        panels.push_back (std::move (p));
    }

    // at most one visible docked panel per location
    for (auto& p : panels)
        if (p.visible && ! p.detached)
            for (auto& other : panels)
                if (&other != &p && other.location == p.location && other.visible && ! other.detached)
                    other.visible = false;

    for (auto& p : panels)
    {
        const auto loc = p.location;
        const auto id = p.id;
        p.host->onToggleDock = [this, id] { setPanelDetached (id, ! isPanelDetached (id)); };

        p.resizer = std::make_unique<PanelResizer> (loc);
        p.resizer->getSize = [this, id] { auto* dp = findPanel (id); return dp != nullptr ? dp->size : 0; };
        p.resizer->setSize = [this, id, loc] (int newSize)
        {
            if (auto* dp = findPanel (id))
            {
                const auto maxSize = loc == PanelLocation::bottom ? getHeight() - 220 : getWidth() / 2 - 100;
                dp->size = juce::jlimit (loc == PanelLocation::bottom ? 140 : 180, juce::jmax (200, maxSize), newSize);
                resized();
            }
        };
        p.resizer->onFinished = [this] { saveLayout(); };
        addChildComponent (*p.resizer);

        if (p.detached)
        {
            openWindow (p, true);
        }
        else
        {
            addChildComponent (*p.host);
            p.host->setVisible (p.visible);
        }

        // menu entry "<Panel> abkoppeln"
        ActionInfo a;
        a.id = "view.detach." + p.id;
        a.name = p.title + juce::String (" in eigenem Fenster");
        a.menu = "Ansicht";
        a.menuOrder = 100 + (int) (&p - panels.data());
        a.separatorBefore = &p == &panels.front();
        a.perform = [this, id] { setPanelDetached (id, ! isPanelDetached (id)); };
        a.isTicked = [this, id] { return isPanelDetached (id); };
        a.isEnabled = [this] { return ! headless; };
        ctx.registry.addAction (std::move (a));
    }
}

void MainComponent::openWindow (DockedPanel& p, bool restorePosition)
{
    const auto dockedBounds = p.host->getScreenBounds();

    removeChildComponent (p.host.get());
    p.host->setDetached (true);

    p.window = std::make_unique<PanelWindow> (p.title, *p.host);
    const auto id = p.id;
    p.window->onClose = [this, id] { setPanelVisible (id, false); };
    p.window->onKey = [this] (const juce::KeyPress& k) { return keyPressed (k); };

    const auto saved = ctx.settings.getValue (key (p, "window"));
    bool placed = false;

    if (restorePosition && saved.isNotEmpty() && p.window->restoreWindowStateFromString (saved))
    {
        // only keep the saved position if it is still on a connected screen
        const auto screens = juce::Desktop::getInstance().getDisplays().getRectangleList (true);
        placed = screens.intersectsRectangle (p.window->getBounds().reduced (40));
    }

    if (! placed)
    {
        if (! dockedBounds.isEmpty() && dockedBounds.getWidth() > 200 && dockedBounds.getHeight() > 100)
            p.window->setBounds (dockedBounds.translated (30, -30));
        else if (p.location == PanelLocation::bottom)
            p.window->centreWithSize (1100, p.size + 40);
        else
            p.window->centreWithSize (p.size, 700);
    }

    p.window->setVisible (p.visible);
}

MainComponent::DockedPanel* MainComponent::findPanel (const juce::String& id)
{
    for (auto& p : panels)
        if (p.id == id)
            return &p;
    return nullptr;
}

const MainComponent::DockedPanel* MainComponent::findPanel (const juce::String& id) const
{
    for (auto& p : panels)
        if (p.id == id)
            return &p;
    return nullptr;
}

bool MainComponent::isPanelVisible (const juce::String& id) const
{
    auto* p = findPanel (id);
    return p != nullptr && p->visible;
}

bool MainComponent::isPanelDetached (const juce::String& id) const
{
    auto* p = findPanel (id);
    return p != nullptr && p->detached;
}

void MainComponent::setPanelVisible (const juce::String& id, bool shouldBeVisible)
{
    auto* p = findPanel (id);
    if (p == nullptr)
        return;

    p->visible = shouldBeVisible;

    // a docked panel replaces the other docked panel at its place (Edit <-> Mix)
    if (shouldBeVisible && ! p->detached)
        for (auto& other : panels)
            if (&other != p && other.location == p->location && other.visible && ! other.detached)
            {
                other.visible = false;
                other.host->setVisible (false);
            }

    if (p->window != nullptr)
    {
        p->window->setVisible (shouldBeVisible);
        if (shouldBeVisible)
            p->window->toFront (true);
    }
    else
    {
        p->host->setVisible (shouldBeVisible);
    }

    layoutChanged();
}

void MainComponent::setPanelDetached (const juce::String& id, bool shouldDetach)
{
    auto* p = findPanel (id);
    if (p == nullptr || headless || p->detached == shouldDetach)
        return;

    p->detached = shouldDetach;
    p->visible = true;

    if (shouldDetach)
    {
        openWindow (*p, true);
        p->window->toFront (true);
    }
    else
    {
        ctx.settings.setValue (key (*p, "window"), p->window->getWindowStateAsString());
        p->window->clearContentComponent();
        p->window.reset();
        p->host->setDetached (false);
        addAndMakeVisible (*p->host);
        for (auto& other : panels)
            if (&other != p && other.location == p->location && other.visible && ! other.detached)
            {
                other.visible = false;
                other.host->setVisible (false);
            }
    }

    layoutChanged();
}

void MainComponent::layoutChanged()
{
    resized();
    transport->refreshToggles();
    menuItemsChanged();
    saveLayout();
}

void MainComponent::saveLayout()
{
    if (headless)
        return;

    for (auto& p : panels)
    {
        ctx.settings.setValue (key (p, "visible"), p.visible);
        ctx.settings.setValue (key (p, "detached"), p.detached);
        ctx.settings.setValue (key (p, "size"), p.size);

        if (p.window != nullptr)
            ctx.settings.setValue (key (p, "window"), p.window->getWindowStateAsString());
    }
}

void MainComponent::registerViewActions()
{
    auto addToggle = [this] (const char* id, const char* name, int keyCode, const juce::String& panelId, int order)
    {
        ActionInfo a;
        a.id = id;
        a.name = name;
        a.menu = "Ansicht";
        a.menuOrder = order;
        a.keys = { juce::KeyPress (keyCode) };
        a.perform = [this, panelId] { setPanelVisible (panelId, ! isPanelVisible (panelId)); };
        a.isTicked = [this, panelId] { return isPanelVisible (panelId); };
        a.isEnabled = [this, panelId] { return findPanel (panelId) != nullptr; };
        ctx.registry.addAction (std::move (a));
    };

    addToggle ("view.editor", "Editor", juce::KeyPress::F2Key, "editor", 5);
    addToggle ("view.console", "Konsole", juce::KeyPress::F3Key, "console", 10);
    addToggle ("view.inspector", "Inspector", juce::KeyPress::F4Key, "inspector", 20);
    addToggle ("view.browser", "Browser", juce::KeyPress::F5Key, "browser", 30);

    // opens the editor without toggling (double-click on a MIDI part)
    ActionInfo showEditor;
    showEditor.id = "view.editor.show";
    showEditor.name = "Editor zeigen";
    showEditor.perform = [this] { setPanelVisible ("editor", true); };
    ctx.registry.addAction (std::move (showEditor));
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

    for (auto& p : panels)
        p.resizer->setVisible (p.visible && ! p.detached);

    auto place = [&] (PanelLocation loc)
    {
        for (auto& p : panels)
        {
            if (p.location != loc || ! p.visible || p.detached)
                continue;
            if (! p.host->isVisible())
                p.host->setVisible (true);

            constexpr int grip = 5;
            const auto size = juce::jmin (p.size, (loc == PanelLocation::bottom ? r.getHeight() : r.getWidth()) - 150);

            switch (loc)
            {
                case PanelLocation::bottom:
                    p.host->setBounds (r.removeFromBottom (size));
                    p.resizer->setBounds (p.host->getX(), p.host->getY() - grip / 2 - 1, p.host->getWidth(), grip);
                    break;
                case PanelLocation::left:
                    p.host->setBounds (r.removeFromLeft (size));
                    p.resizer->setBounds (p.host->getRight() - grip / 2 - 1, p.host->getY(), grip, p.host->getHeight());
                    break;
                case PanelLocation::right:
                    p.host->setBounds (r.removeFromRight (size));
                    p.resizer->setBounds (p.host->getX() - grip / 2, p.host->getY(), grip, p.host->getHeight());
                    break;
            }
        }
    };

    // the console spans the full width below everything
    place (PanelLocation::bottom);
    place (PanelLocation::left);
    place (PanelLocation::right);
    arrange->setBounds (r);

    for (auto& p : panels)
        p.resizer->toFront (false);
}

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
    if (auto* a = ctx.registry.findActionForKey (k))
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
