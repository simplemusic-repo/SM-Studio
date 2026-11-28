// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginWindows.h"

#include "Theme.h"

namespace sms
{

class PluginWindows::Window final : public juce::DocumentWindow
{
public:
    Window (PluginWindows& o, ObjectId id, std::shared_ptr<juce::AudioProcessor> p, const juce::String& title)
        : juce::DocumentWindow (title, theme::colours::window, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
          owner (o), ownerId (id), processor (std::move (p))
    {
        setUsingNativeTitleBar (true);

        juce::AudioProcessorEditor* editor = processor->hasEditor() ? processor->createEditorAndMakeActive() : nullptr;
        if (editor == nullptr)
            editor = new juce::GenericAudioProcessorEditor (*processor);

        // never full screen: the window takes the size the plug-in asks for. Editors that
        // are larger than the screen are shown in a scrollable area instead.
        const auto area = screenArea();
        if (editor->getWidth() > area.getWidth() * 9 / 10 || editor->getHeight() > area.getHeight() * 85 / 100)
        {
            auto* viewport = new juce::Viewport();
            viewport->setViewedComponent (editor, true); // deletes the editor with the viewport
            viewport->setSize (juce::jmin (editor->getWidth() + viewport->getScrollBarThickness(), area.getWidth() * 9 / 10),
                               juce::jmin (editor->getHeight() + viewport->getScrollBarThickness(), area.getHeight() * 85 / 100));
            setResizable (true, false);
            setContentOwned (viewport, true);
        }
        else
        {
            setResizable (editor->isResizable(), false);
            setContentOwned (editor, true);
        }

        fitToScreen();
    }

    static juce::Rectangle<int> screenArea()
    {
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            return d->userBounds.toNearestInt();
        return { 1280, 800 };
    }

    /** Keeps the window inside the screen and never maximised. */
    void fitToScreen()
    {
        if (isFullScreen())
            setFullScreen (false);

        auto area = screenArea();
        if (auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
            area = d->userBounds.toNearestInt();

        auto b = getBounds();
        if (b.getWidth() > area.getWidth() || b.getHeight() > area.getHeight())
            b = b.withSize (juce::jmin (b.getWidth(), area.getWidth()), juce::jmin (b.getHeight(), area.getHeight()));
        setBounds (b.constrainedWithin (area));
    }

    ~Window() override
    {
        // the editor must be deleted before the processor may go away
        clearContentComponent();
    }

    void closeButtonPressed() override
    {
        // deleting ourselves from inside our own callback is not allowed: do it asynchronously
        juce::MessageManager::callAsync ([&o = owner, id = ownerId] { o.close (id); });
    }

private:
    PluginWindows& owner;
    ObjectId ownerId;
    std::shared_ptr<juce::AudioProcessor> processor; // keeps the plug-in alive while its editor is open
};

PluginWindows::PluginWindows (PluginHost& h) : host (h) {}

PluginWindows::~PluginWindows()
{
    closeAll();
}

bool PluginWindows::open (ObjectId owner, const juce::String& pluginId, const juce::String& title)
{
    if (auto it = windows.find (owner); it != windows.end())
    {
        it->second->setMinimised (false);
        it->second->toFront (true);
        return true;
    }

    auto processor = host.getInstance (owner, pluginId);
    if (processor == nullptr)
        return false;

    auto w = std::make_unique<Window> (*this, owner, processor, title);

    if (auto pos = lastPositions.find (owner); pos != lastPositions.end())
        w->setTopLeftPosition (pos->second);
    else if (auto* main = juce::TopLevelWindow::getActiveTopLevelWindow())
        w->setCentrePosition (main->getScreenBounds().getCentre());
    else
        w->centreWithSize (w->getWidth(), w->getHeight());

    w->fitToScreen();
    w->setVisible (true);
    w->toFront (true);

    // some plug-ins change their size right after opening: check once more
    juce::Component::SafePointer<Window> safe (w.get());
    juce::Timer::callAfterDelay (400, [safe] { if (safe != nullptr) safe->fitToScreen(); });

    windows[owner] = std::move (w);
    return true;
}

void PluginWindows::close (ObjectId owner)
{
    if (auto it = windows.find (owner); it != windows.end())
    {
        lastPositions[owner] = it->second->getPosition();
        windows.erase (it);
    }
}

void PluginWindows::closeAll()
{
    for (auto& [id, w] : windows)
        lastPositions[id] = w->getPosition();
    windows.clear();
}

} // namespace sms
