// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ExtensionRegistry.h"

namespace sms
{

juce::var BrowserItem::toDragDescription() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("smsBrowserItem", true);
    o->setProperty ("kind", kind);
    o->setProperty ("id", id);
    o->setProperty ("name", name);
    o->setProperty ("category", category);
    return juce::var (o);
}

std::optional<BrowserItem> BrowserItem::fromDragDescription (const juce::var& v)
{
    auto* o = v.getDynamicObject();
    if (o == nullptr || ! o->hasProperty ("smsBrowserItem"))
        return std::nullopt;

    BrowserItem i;
    i.kind = o->getProperty ("kind").toString();
    i.id = o->getProperty ("id").toString();
    i.name = o->getProperty ("name").toString();
    i.category = o->getProperty ("category").toString();
    return i;
}

//==============================================================================
void ExtensionRegistry::addAction (ActionInfo a)
{
    // registering an id again replaces the action (e.g. when the main window is rebuilt)
    for (auto& existing : actions)
    {
        if (existing.id == a.id)
        {
            existing = std::move (a);
            return;
        }
    }
    actions.push_back (std::move (a));
}

const ActionInfo* ExtensionRegistry::findAction (const juce::String& id) const
{
    for (auto& a : actions)
        if (a.id == id)
            return &a;
    return nullptr;
}

const ActionInfo* ExtensionRegistry::findActionForKey (const juce::KeyPress& key) const
{
    for (auto& a : actions)
        for (auto& k : a.keys)
            if (k == key)
                return &a;
    return nullptr;
}

bool ExtensionRegistry::invoke (const juce::String& id) const
{
    if (auto* a = findAction (id))
    {
        if (a->perform && (! a->isEnabled || a->isEnabled()))
        {
            a->perform();
            return true;
        }
    }
    return false;
}

juce::StringArray ExtensionRegistry::getMenuNames() const
{
    juce::StringArray names;
    for (auto& a : actions)
        if (a.menu.isNotEmpty())
            names.addIfNotAlreadyThere (a.menu);
    return names;
}

const TrackTypeInfo* ExtensionRegistry::findTrackType (const juce::String& id) const
{
    for (auto& t : trackTypes)
        if (t.id == id)
            return &t;
    return nullptr;
}

const DropHandlerInfo* ExtensionRegistry::findDropHandler (const juce::String& kind) const
{
    for (auto& d : dropHandlers)
        if (d.itemKind == kind)
            return &d;
    return nullptr;
}

void ExtensionRegistry::initialiseModules (AppContext& ctx)
{
    for (auto& m : modules)
        m->initialise (ctx);
}

void ExtensionRegistry::shutdownModules()
{
    for (auto it = modules.rbegin(); it != modules.rend(); ++it)
        (*it)->shutdown();
}

} // namespace sms
