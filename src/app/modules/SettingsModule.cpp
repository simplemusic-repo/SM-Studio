// SPDX-License-Identifier: AGPL-3.0-or-later
// The "Einstellungen" menu and window: audio & MIDI devices (sound card,
// driver incl. ASIO on Windows, sample rate, buffer size, MIDI inputs),
// plug-ins (scan, skipped plug-ins) and appearance (colour schemes).

#include "Modules.h"

#include "extension/ExtensionRegistry.h"
#include "ui/PluginScanDialog.h"
#include "ui/Theme.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace sms
{

namespace
{
    juce::Label& styleCaption (juce::Label& l, bool bold = false)
    {
        l.setFont (theme::font (bold ? 14.0f : 12.5f, bold));
        l.setColour (juce::Label::textColourId, bold ? theme::colours::text : theme::colours::textDim);
        l.setJustificationType (juce::Justification::topLeft);
        return l;
    }

    //==========================================================================
    class AudioPage final : public juce::Component
    {
    public:
        explicit AudioPage (AppContext& c)
            : ctx (c), selector (c.engine.getDeviceManager(), 0, 2, 2, 2, true, false, true, false)
        {
            styleCaption (hint).setText (u8"Treiber: unter Windows möglichst ASIO wählen (niedrigste Latenz). "
                                         u8"Kleinere Puffergröße = weniger Verzögerung, aber mehr CPU-Last. "
                                         u8"MIDI-Eingänge: aktivierte Keyboards spielen die ausgewählte Instrumentenspur.",
                                         juce::dontSendNotification);
            addAndMakeVisible (hint);
            addAndMakeVisible (selector);
        }

        ~AudioPage() override
        {
            if (auto xml = ctx.engine.createDeviceStateXml())
                ctx.settings.setValue ("audio.deviceState", xml.get());
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (10);
            hint.setBounds (r.removeFromBottom (48));
            selector.setBounds (r);
        }

    private:
        AppContext& ctx;
        juce::AudioDeviceSelectorComponent selector;
        juce::Label hint;
    };

    //==========================================================================
    class PluginPage final : public juce::Component,
                             private juce::ChangeListener
    {
    public:
        explicit PluginPage (AppContext& c) : ctx (c)
        {
            styleCaption (title, true).setText ("Plug-ins", juce::dontSendNotification);
            styleCaption (info).setText (u8"SM Studio sucht VST3-Plug-ins (am Mac auch Audio Units) in den Standardordnern. "
                                         u8"Plug-ins, die bei der Suche abstürzen, werden übersprungen und beim nächsten Mal "
                                         u8"nicht erneut geprüft.", juce::dontSendNotification);
            styleCaption (status);
            styleCaption (skippedTitle, true).setText (u8"Übersprungene Plug-ins", juce::dontSendNotification);
            skipped.setMultiLine (true);
            skipped.setReadOnly (true);
            skipped.setFont (theme::font (12.0f));

            scan.onClick = [this] { startPluginScanWithProgress (ctx); };
            rescan.onClick = [this] { ctx.plugins.clearSkippedPlugins(); startPluginScanWithProgress (ctx); };
            scan.setTooltip (u8"Nach neuen oder geänderten Plug-ins suchen");
            rescan.setTooltip (u8"Übersprungene Plug-ins noch einmal prüfen");

            for (auto* comp : std::initializer_list<juce::Component*> { &title, &info, &status, &scan, &rescan, &skippedTitle, &skipped })
                addAndMakeVisible (comp);

            ctx.plugins.addChangeListener (this);
            update();
        }

        ~PluginPage() override { ctx.plugins.removeChangeListener (this); }

        void resized() override
        {
            auto r = getLocalBounds().reduced (14);
            title.setBounds (r.removeFromTop (24));
            info.setBounds (r.removeFromTop (48));
            status.setBounds (r.removeFromTop (24));
            r.removeFromTop (6);
            auto buttons = r.removeFromTop (28);
            scan.setBounds (buttons.removeFromLeft (180));
            buttons.removeFromLeft (8);
            rescan.setBounds (buttons.removeFromLeft (280));
            r.removeFromTop (16);
            skippedTitle.setBounds (r.removeFromTop (22));
            skipped.setBounds (r);
        }

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override { update(); }

        void update()
        {
            status.setText (ctx.plugins.getStatusText(), juce::dontSendNotification);
            rescan.setEnabled (ctx.plugins.getNumSkippedPlugins() > 0);
            skipped.setText (ctx.plugins.getNumSkippedPlugins() > 0 ? ctx.plugins.getSkippedPlugins().joinIntoString ("\n")
                                                                     : juce::String ("(keine)"));
        }

        AppContext& ctx;
        juce::Label title, info, status, skippedTitle;
        juce::TextButton scan { u8"Plug-ins suchen …" }, rescan { u8"Übersprungene erneut prüfen …" };
        juce::TextEditor skipped;
    };

    //==========================================================================
    class AppearancePage final : public juce::Component
    {
    public:
        AppearancePage (AppContext& c, std::function<void()> afterChange) : ctx (c), onChanged (std::move (afterChange))
        {
            styleCaption (title, true).setText ("Farbschema", juce::dontSendNotification);
            styleCaption (info).setText (u8"Das Farbschema wird sofort übernommen und gespeichert.", juce::dontSendNotification);

            schemes.addItemList (theme::schemeNames(), 1);
            schemes.setSelectedItemIndex (ctx.settings.getIntValue ("ui.colourScheme", 0), juce::dontSendNotification);
            schemes.onChange = [this]
            {
                const auto index = schemes.getSelectedItemIndex();
                ctx.settings.setValue ("ui.colourScheme", index);
                // rebuild after this callback has returned (this page is rebuilt too)
                juce::MessageManager::callAsync ([&c = ctx, index, cb = onChanged]
                {
                    theme::applyScheme (index);
                    if (c.rebuildUi)
                        c.rebuildUi();
                    if (cb)
                        cb(); // reopen the settings window in the new colours
                });
            };

            for (auto* comp : std::initializer_list<juce::Component*> { &title, &info, &schemes })
                addAndMakeVisible (comp);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (14);
            title.setBounds (r.removeFromTop (24));
            info.setBounds (r.removeFromTop (24));
            r.removeFromTop (6);
            schemes.setBounds (r.removeFromTop (28).removeFromLeft (260));
        }

    private:
        AppContext& ctx;
        std::function<void()> onChanged;
        juce::Label title, info;
        juce::ComboBox schemes;
    };

    //==========================================================================
    class SettingsComponent final : public juce::Component
    {
    public:
        SettingsComponent (AppContext& ctx, int initialTab, std::function<void()> onSchemeChanged)
        {
            const auto bg = theme::colours::panel;
            tabs.addTab ("Audio & MIDI", bg, new AudioPage (ctx), true);
            tabs.addTab ("Plug-ins", bg, new PluginPage (ctx), true);
            tabs.addTab ("Darstellung", bg, new AppearancePage (ctx, std::move (onSchemeChanged)), true);
            tabs.setCurrentTabIndex (initialTab);
            tabs.setTabBarDepth (32);
            addAndMakeVisible (tabs);
            setSize (680, 600);
        }

        void paint (juce::Graphics& g) override { g.fillAll (theme::colours::panel); }
        void resized() override { tabs.setBounds (getLocalBounds()); }

    private:
        juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    };

    //==========================================================================
    class SettingsModule final : public Module
    {
    public:
        juce::String getId() const override { return "sms.settings"; }
        juce::String getName() const override { return "Einstellungen"; }

        void initialise (AppContext& c) override
        {
            ctx = &c;
            theme::applyScheme (c.settings.getIntValue ("ui.colourScheme", 0));

            auto add = [&c, this] (const char* id, const juce::String& name, int order, int tab,
                                   std::vector<juce::KeyPress> keys, bool separator = false)
            {
                ActionInfo a;
                a.id = id;
                a.name = name;
                a.menu = "Einstellungen";
                a.menuOrder = order;
                a.separatorBefore = separator;
                a.keys = std::move (keys);
                a.perform = [this, tab] { show (tab); };
                c.registry.addAction (std::move (a));
            };

            add ("settings.open", u8"Einstellungen …", 10, 0, { juce::KeyPress (',', juce::ModifierKeys::commandModifier, 0) });
            add ("settings.audioMidi", u8"Audio & MIDI (Soundkarte, Puffer, Keyboards) …", 20, 0, {}, true);
            add ("settings.plugins", u8"Plug-ins …", 30, 1, {});
            add ("settings.appearance", u8"Darstellung (Farbschema) …", 40, 2, {});

            ActionInfo scan;
            scan.id = "settings.scanPlugins";
            scan.name = u8"Plug-ins suchen …";
            scan.menu = "Einstellungen";
            scan.menuOrder = 50;
            scan.separatorBefore = true;
            scan.perform = [this] { startPluginScanWithProgress (*ctx); };
            c.registry.addAction (std::move (scan));
        }

    private:
        void show (int tab)
        {
            if (window != nullptr)
            {
                window->toFront (true);
                return;
            }

            juce::DialogWindow::LaunchOptions o;
            o.content.setOwned (new SettingsComponent (*ctx, tab, [this]
            {
                if (window != nullptr)
                    window->exitModalState (0); // deletes the dialog
                juce::MessageManager::callAsync ([this] { show (2); });
            }));
            o.dialogTitle = juce::String (u8"SM Studio – Einstellungen");
            o.dialogBackgroundColour = theme::colours::panel;
            o.useNativeTitleBar = true;
            o.resizable = true;
            o.componentToCentreAround = juce::TopLevelWindow::getActiveTopLevelWindow();
            window = o.launchAsync();
        }

        AppContext* ctx = nullptr;
        juce::Component::SafePointer<juce::DialogWindow> window;
    };
}

std::unique_ptr<Module> createSettingsModule() { return std::make_unique<SettingsModule>(); }

} // namespace sms
