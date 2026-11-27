// SPDX-License-Identifier: AGPL-3.0-or-later
// Transport actions (play, stop, locate, metronome).

#include "Modules.h"

#include "extension/ExtensionRegistry.h"

namespace sms
{

namespace
{
    class TransportModule final : public Module
    {
    public:
        juce::String getId() const override { return "sms.transport"; }
        juce::String getName() const override { return "Transport"; }

        void initialise (AppContext& ctx) override
        {
            auto& reg = ctx.registry;
            auto& engine = ctx.engine;

            auto add = [&reg] (const char* id, const juce::String& name, int order, std::vector<juce::KeyPress> keys, std::function<void()> fn)
            {
                ActionInfo a;
                a.id = id;
                a.name = name;
                a.menu = "Transport";
                a.menuOrder = order;
                a.keys = std::move (keys);
                a.perform = std::move (fn);
                reg.addAction (std::move (a));
            };

            add ("transport.play", "Wiedergabe / Stopp", 10, { juce::KeyPress (juce::KeyPress::spaceKey) },
                 [&engine] { engine.togglePlay(); });

            // stop; when already stopped, return to the start (like most DAWs)
            add ("transport.stop", "Stopp", 20, { juce::KeyPress (juce::KeyPress::numberPad0) },
                 [&engine]
                 {
                     if (engine.isPlaying())
                         engine.stop();
                     else
                         engine.setPositionSeconds (0.0);
                 });

            add ("transport.toStart", "Zum Anfang", 30, { juce::KeyPress (juce::KeyPress::homeKey), juce::KeyPress (',') },
                 [&engine] { engine.setPositionSeconds (0.0); });

            auto jumpBars = [&ctx] (int bars)
            {
                const auto& map = ctx.song.getTempoMap();
                const auto barSeconds = map.ticksToSeconds (map.getTicksPerBar());
                const auto now = ctx.engine.getPositionSeconds();
                const auto bar = std::floor (now / barSeconds + 1e-6);
                const auto target = bars < 0 && now - bar * barSeconds > 0.05 ? bar : bar + bars;
                ctx.engine.setPositionSeconds (juce::jmax (0.0, target * barSeconds));
            };

            add ("transport.rewind", u8"Einen Takt zurück", 40, { juce::KeyPress (juce::KeyPress::leftKey), juce::KeyPress (juce::KeyPress::numberPadSubtract) },
                 [jumpBars] { jumpBars (-1); });
            add ("transport.forward", "Einen Takt vor", 50, { juce::KeyPress (juce::KeyPress::rightKey), juce::KeyPress (juce::KeyPress::numberPadAdd) },
                 [jumpBars] { jumpBars (1); });

            ActionInfo metro;
            metro.id = "transport.metronome";
            metro.name = "Metronom";
            metro.menu = "Transport";
            metro.menuOrder = 60;
            metro.separatorBefore = true;
            metro.keys = { juce::KeyPress ('c') };
            metro.perform = [&engine] { engine.setMetronomeEnabled (! engine.isMetronomeEnabled()); };
            metro.isTicked = [&engine] { return engine.isMetronomeEnabled(); };
            reg.addAction (std::move (metro));
        }
    };
}

std::unique_ptr<Module> createTransportModule() { return std::make_unique<TransportModule>(); }

} // namespace sms
