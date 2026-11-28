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

        setContentOwned (editor, true);
        setResizable (editor->isResizable(), false);
        setVisible (true);
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
    else
        w->centreWithSize (w->getWidth(), w->getHeight());

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
