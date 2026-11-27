// SPDX-License-Identifier: AGPL-3.0-or-later
// Registers the standard docked panels.

#include "Modules.h"

#include "extension/ExtensionRegistry.h"
#include "ui/Panels.h"

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

            reg.addPanel ({ "console", "Konsole", PanelLocation::bottom, 290, false,
                            [] (AppContext& c) { return std::make_unique<ConsolePanel> (c); } });
        }
    };
}

std::unique_ptr<Module> createPanelsModule() { return std::make_unique<PanelsModule>(); }

} // namespace sms
