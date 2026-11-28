// SPDX-License-Identifier: AGPL-3.0-or-later
// Registers the standard docked panels.

#include "Modules.h"

#include "extension/ExtensionRegistry.h"
#include "ui/Panels.h"
#include "ui/PianoRoll.h"

namespace sms
{

namespace
{
    class PanelsModule final : public Module
    {
    public:
        juce::String getId() const override { return "sms.panels"; }
        juce::String getName() const override { return "Standard-Panels"; }

        void initialise (AppContext& ctx) override
        {
            auto& reg = ctx.registry;

            reg.addPanel ({ "inspector", "Inspector", PanelLocation::left, 230, true,
                            [] (AppContext& c) { return std::make_unique<InspectorPanel> (c); } });

            reg.addPanel ({ "browser", "Browser", PanelLocation::right, 300, true,
                            [] (AppContext& c) { return std::make_unique<BrowserPanel> (c); } });

            // Editor (F2) and Konsole (F3) share the bottom: showing one hides the other
            reg.addPanel ({ "editor", "Editor", PanelLocation::bottom, 380, false,
                            [] (AppContext& c) { return std::make_unique<PianoRollPanel> (c); } });

            reg.addPanel ({ "console", "Konsole", PanelLocation::bottom, 460, false,
                            [] (AppContext& c) { return std::make_unique<ConsolePanel> (c); } });
        }
    };
}

std::unique_ptr<Module> createPanelsModule() { return std::make_unique<PanelsModule>(); }

} // namespace sms
