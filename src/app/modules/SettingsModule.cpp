// SPDX-License-Identifier: AGPL-3.0-or-later
// The "Einstellungen" window (Cubase-like): categories on the left
// (Audio, MIDI, Metronom, Plug-ins, Darstellung), each with tabs.
//
//   Audio:       driver/device (top right), sample rate, buffer, latency,
//                driver panel, test tone; output and input channels
//   MIDI:        ports (enable, display name, status), filter (record / thru),
//                options (auto-enable new devices, activity)
//   Metronom:    sound, volume, accent, when to click, count-in
//                (also as a small window: right-click on the metronome button)
//   Plug-ins:    full scan / update, skipped plug-ins
//   Darstellung: UI size, colour scheme tiles

#include "Modules.h"

#include "engine/Metronome.h"
#include "extension/ExtensionRegistry.h"
#include "ui/PluginScanDialog.h"
#include "ui/SettingsWidgets.h"
#include "ui/Theme.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <iterator>
#include <set>

namespace sms
{

using namespace settingsui;

namespace
{
    void saveDeviceState (AppContext& ctx)
    {
        if (auto xml = ctx.engine.createDeviceStateXml())
            ctx.settings.setValue ("audio.deviceState", xml.get());
    }

    std::unique_ptr<juce::TextButton> makeButton (const juce::String& text, std::function<void()> fn, const juce::String& tooltip = {})
    {
        auto b = std::make_unique<juce::TextButton> (text);
        b->onClick = std::move (fn);
        b->setTooltip (tooltip);
        b->setWantsKeyboardFocus (false);
        return b;
    }

    std::unique_ptr<ToggleSwitch> makeSwitch (bool on, std::function<void (bool)> fn)
    {
        auto t = std::make_unique<ToggleSwitch>();
        t->setToggleState (on, juce::dontSendNotification);
        t->onClick = [b = t.get(), fn = std::move (fn)] { fn (b->getToggleState()); };
        return t;
    }

    std::unique_ptr<juce::Label> makeInfo (const juce::String& text)
    {
        auto l = std::make_unique<juce::Label> (juce::String(), text);
        l->setFont (theme::font (13.0f));
        l->setColour (juce::Label::textColourId, theme::colours::textDim);
        l->setJustificationType (juce::Justification::centredRight);
        l->setMinimumHorizontalScale (0.8f);
        return l;
    }

    juce::String typeDisplayName (const juce::String& type)
    {
        if (type == "Windows Audio") return "Windows Audio (WASAPI)";
        return type;
    }

    //==========================================================================
    // AUDIO
    //==========================================================================
    class AudioCategory final : public CategoryPage,
                                private juce::ChangeListener
    {
    public:
        explicit AudioCategory (AppContext& c) : CategoryPage ("Audio"), ctx (c), dm (c.engine.getDeviceManager())
        {
            driverBox.setTooltip (u8"Soundkarte / Treiber. Unter Windows möglichst ASIO wählen (niedrigste Latenz).");
            driverBox.onChange = [this] { driverChosen(); };
            setHeaderComponent (&driverBox, 340, "Audiotreiber");

            addTab (u8"Gerät", buildDevicePage());
            addTab (u8"Ausgänge", buildChannelPage (false));
            addTab (u8"Eingänge", buildChannelPage (true));
            fillDriverBox();
            dm.addChangeListener (this);
        }

        ~AudioCategory() override
        {
            dm.removeChangeListener (this);
            saveDeviceState (ctx);
        }

    private:
        struct Choice { juce::String type, device; };

        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            // rebuild after the current callback (a combo of ours may be calling)
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<AudioCategory> (this)]
            {
                if (safe != nullptr)
                    safe->rebuild();
            });
        }

        void rebuild()
        {
            fillDriverBox();
            replaceTab (0, buildDevicePage());
            replaceTab (1, buildChannelPage (false));
            replaceTab (2, buildChannelPage (true));
        }

        void fillDriverBox()
        {
            driverBox.clear (juce::dontSendNotification);
            choices.clear();

            auto* device = dm.getCurrentAudioDevice();
            const auto currentType = dm.getCurrentAudioDeviceType();
            const auto currentName = device != nullptr ? device->getName() : juce::String();

            for (auto* type : dm.getAvailableDeviceTypes())
            {
                type->scanForDevices();
                const auto names = type->getDeviceNames (false);
                if (names.isEmpty())
                    continue;

                driverBox.addSectionHeading (typeDisplayName (type->getTypeName()));
                for (auto& n : names)
                {
                    choices.push_back ({ type->getTypeName(), n });
                    driverBox.addItem (n, (int) choices.size());
                    if (type->getTypeName() == currentType && n == currentName)
                        driverBox.setSelectedId ((int) choices.size(), juce::dontSendNotification);
                }
            }

            if (driverBox.getSelectedId() == 0)
                driverBox.setText (juce::String (u8"– kein Gerät –"), juce::dontSendNotification);
        }

        void driverChosen()
        {
            const auto id = driverBox.getSelectedId();
            if (id <= 0 || id > (int) choices.size())
                return;
            const auto choice = choices[(size_t) id - 1];

            if (dm.getCurrentAudioDeviceType() != choice.type)
                dm.setCurrentAudioDeviceType (choice.type, true);

            auto setup = dm.getAudioDeviceSetup();
            setup.outputDeviceName = choice.device;
            // no input device unless asked for (separate in/out devices can crackle on Windows Audio)
            setup.inputDeviceName = isSeparateIo() ? juce::String() : choice.device;
            setup.useDefaultOutputChannels = true;
            setup.useDefaultInputChannels = false;
            setup.inputChannels.clear();

            const auto error = dm.setAudioDeviceSetup (setup, true);
            if (error.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Audiotreiber",
                                                        juce::String (u8"Das Gerät konnte nicht geöffnet werden:\n") + error);
            saveDeviceState (ctx);
        }

        bool isSeparateIo() const
        {
            for (auto* type : dm.getAvailableDeviceTypes())
                if (type->getTypeName() == dm.getCurrentAudioDeviceType())
                    return type->hasSeparateInputsAndOutputs();
            return false;
        }

        void applySetup (const juce::AudioDeviceManager::AudioDeviceSetup& setup)
        {
            const auto error = dm.setAudioDeviceSetup (setup, true);
            if (error.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Audio", error);
            saveDeviceState (ctx);
        }

        std::unique_ptr<juce::Component> buildDevicePage()
        {
            auto page = std::make_unique<FormPage>();
            auto* device = dm.getCurrentAudioDevice();

            page->addSection (device != nullptr ? device->getName() : juce::String (u8"Kein Gerät geöffnet"));
            if (device == nullptr)
            {
                page->addNote (u8"Wähle oben rechts unter „Audiotreiber“ eine Soundkarte aus.");
                return page;
            }

            const auto sr = device->getCurrentSampleRate();

            // sample rate
            juce::StringArray rates;
            int rateIndex = 0;
            const auto availableRates = device->getAvailableSampleRates();
            for (int i = 0; i < availableRates.size(); ++i)
            {
                rates.add (juce::String (availableRates[i], 0) + " Hz");
                if (juce::approximatelyEqual (availableRates[i], sr))
                    rateIndex = i;
            }
            auto& rateBox = page->addRow ("Abtastrate", makeCombo (rates, rateIndex), 220);
            rateBox.onChange = [this, &rateBox, availableRates]
            {
                auto setup = dm.getAudioDeviceSetup();
                setup.sampleRate = availableRates[rateBox.getSelectedItemIndex()];
                applySetup (setup);
            };

            // buffer size
            juce::StringArray sizes;
            int sizeIndex = 0;
            const auto availableSizes = device->getAvailableBufferSizes();
            for (int i = 0; i < availableSizes.size(); ++i)
            {
                sizes.add (juce::String (availableSizes[i]) + " Samples  (" + juce::String (availableSizes[i] * 1000.0 / sr, 1) + " ms)");
                if (availableSizes[i] == device->getCurrentBufferSizeSamples())
                    sizeIndex = i;
            }
            auto& sizeBox = page->addRow (u8"Puffergröße", makeCombo (sizes, sizeIndex), 220,
                                          u8"Kleiner = weniger Verzögerung beim Einspielen, aber mehr CPU-Last");
            sizeBox.onChange = [this, &sizeBox, availableSizes]
            {
                auto setup = dm.getAudioDeviceSetup();
                setup.bufferSize = availableSizes[sizeBox.getSelectedItemIndex()];
                applySetup (setup);
            };

            const auto inMs = device->getInputLatencyInSamples() * 1000.0 / sr;
            const auto outMs = device->getOutputLatencyInSamples() * 1000.0 / sr;
            page->addRow ("Latenz", makeInfo ("Eingang " + juce::String (inMs, 1) + " ms  /  Ausgang " + juce::String (outMs, 1) + " ms"), 320);

            if (device->hasControlPanel())
                page->addRow (u8"Treiber-Einstellungen", makeButton (u8"Bedienfeld öffnen …", [this]
                {
                    if (auto* d = dm.getCurrentAudioDevice(); d != nullptr && d->showControlPanel())
                    {
                        dm.closeAudioDevice(); // settings may have changed: reopen
                        dm.restartLastAudioDevice();
                    }
                }, u8"Das Bedienfeld des Soundkarten-Treibers (z. B. ASIO-Puffer)"), 220);

            page->addRow ("Testton", makeButton (u8"Abspielen", [this] { dm.playTestSound(); },
                                                 u8"Spielt einen kurzen Ton über die Ausgänge"), 220);

            page->addSection ("Hinweise");
            page->addNote (u8"Unter Windows liefert ASIO die niedrigste Latenz. Ohne ASIO-Treiber der Soundkarte "
                           u8"„Windows Audio (WASAPI)“ wählen. Kleinere Puffergröße = weniger Verzögerung beim "
                           u8"Spielen, aber mehr CPU-Last – knackt es, den Puffer vergrößern.");
            return page;
        }

        std::unique_ptr<juce::Component> buildChannelPage (bool inputs)
        {
            auto page = std::make_unique<FormPage>();
            auto* device = dm.getCurrentAudioDevice();

            if (inputs && isSeparateIo())
            {
                // choose an input device (or none)
                juce::StringArray names { u8"Keins" };
                int index = 0;
                for (auto* type : dm.getAvailableDeviceTypes())
                    if (type->getTypeName() == dm.getCurrentAudioDeviceType())
                        for (auto& n : type->getDeviceNames (true))
                        {
                            names.add (n);
                            if (n == dm.getAudioDeviceSetup().inputDeviceName)
                                index = names.size() - 1;
                        }

                page->addSection (u8"Eingabegerät");
                auto& box = page->addRow (u8"Eingabegerät", makeCombo (names, index), 320);
                box.onChange = [this, &box, names]
                {
                    auto setup = dm.getAudioDeviceSetup();
                    setup.inputDeviceName = box.getSelectedItemIndex() <= 0 ? juce::String() : names[box.getSelectedItemIndex()];
                    setup.useDefaultInputChannels = true;
                    applySetup (setup);
                };
                page->addNote (u8"Nur für Aufnahmen nötig (kommt mit Etappe 7). Unterschiedliche Ein- und "
                               u8"Ausgabegeräte können unter Windows Audio knacksen – dann „Keins“ wählen.");
            }

            page->addSection (inputs ? juce::String (u8"Eingänge") : juce::String (u8"Ausgänge"));

            if (device == nullptr)
            {
                page->addNote (u8"Kein Gerät geöffnet.");
                return page;
            }

            const auto names = inputs ? device->getInputChannelNames() : device->getOutputChannelNames();
            const auto active = inputs ? device->getActiveInputChannels() : device->getActiveOutputChannels();

            auto table = std::make_unique<Table> (std::vector<Table::Column> { { "Kanal", 70 }, { "Name", 0 }, { "Aktiv", 80 } });
            table->setEmptyText (inputs ? juce::String (u8"Keine Eingänge (kein Eingabegerät gewählt)")
                                        : juce::String (u8"Keine Ausgänge"));
            for (int i = 0; i < names.size(); ++i)
            {
                std::vector<std::unique_ptr<juce::Component>> cells;
                cells.push_back (Table::text (juce::String (i + 1), true));
                cells.push_back (Table::text (names[i]));
                cells.push_back (makeSwitch (active[i], [this, inputs, i] (bool on)
                {
                    auto setup = dm.getAudioDeviceSetup();
                    auto& bits = inputs ? setup.inputChannels : setup.outputChannels;
                    (inputs ? setup.useDefaultInputChannels : setup.useDefaultOutputChannels) = false;
                    bits.setBit (i, on);
                    applySetup (setup);
                }));
                table->addRow (std::move (cells));
            }
            const auto h = table->getIdealHeight();
            page->addFull (std::move (table), h);
            page->addNote (inputs ? juce::String (u8"Aktive Eingänge stehen später für Aufnahmen zur Verfügung.")
                                  : juce::String (u8"SM Studio spielt die Stereosumme über die ersten beiden aktiven Ausgänge."));
            return page;
        }

        AppContext& ctx;
        juce::AudioDeviceManager& dm;
        juce::ComboBox driverBox;
        std::vector<Choice> choices;
    };

    //==========================================================================
    // MIDI
    //==========================================================================
    struct MidiFilterRow { const char* name; int bit; };
    const MidiFilterRow midiFilterRows[] = {
        { "Note", AudioEngine::filterNotes },
        { "Poly Pressure", AudioEngine::filterPolyPressure },
        { "Controller", AudioEngine::filterControllers },
        { "Program Change", AudioEngine::filterProgramChange },
        { "Aftertouch", AudioEngine::filterAftertouch },
        { "Pitchbend", AudioEngine::filterPitchbend },
        { "SysEx", AudioEngine::filterSysEx },
    };
    constexpr int defaultMidiMask = 0x3f; // everything except SysEx

    void applyMidiThru (AppContext& ctx)
    {
        ctx.engine.setMidiThruMask (ctx.settings.getIntValue ("midi.filter.thru", defaultMidiMask));
    }

    /** Flat lamp that flashes while MIDI arrives. */
    class MidiActivity final : public juce::Component, private juce::Timer
    {
    public:
        explicit MidiActivity (AudioEngine& e) : engine (e), last (e.getLiveMidiCount()) { startTimerHz (20); }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            const auto lamp = r.removeFromLeft (16.0f).withSizeKeepingCentre (10.0f, 10.0f);
            g.setColour (blink > 0 ? theme::colours::accent : theme::colours::panelDark);
            g.fillEllipse (lamp);
            g.setColour (theme::colours::separator);
            g.drawEllipse (lamp, 1.0f);
            g.setColour (theme::colours::textDim);
            g.setFont (theme::font (12.5f));
            g.drawText (blink > 0 ? juce::String (u8"Daten kommen an") : juce::String (u8"keine Daten – Taste drücken zum Testen"),
                        r.withTrimmedLeft (6.0f), juce::Justification::centredLeft);
        }

    private:
        void timerCallback() override
        {
            const auto now = engine.getLiveMidiCount();
            if (now != last) { last = now; blink = 4; repaint(); }
            else if (blink > 0 && --blink == 0) repaint();
        }

        AudioEngine& engine;
        int last, blink = 0;
    };

    class MidiCategory final : public CategoryPage, private juce::Timer
    {
    public:
        explicit MidiCategory (AppContext& c) : CategoryPage ("MIDI"), ctx (c)
        {
            addTab (u8"Anschlüsse", buildPorts());
            addTab ("Filter", buildFilter());
            addTab ("Einstellungen", buildOptions());
            startTimer (1000);
        }

        ~MidiCategory() override { saveDeviceState (ctx); }

    private:
        void timerCallback() override
        {
            // hot-plug: rebuild the port list when devices come or go (or were auto-enabled)
            auto& dm = ctx.engine.getDeviceManager();
            int enabledNow = 0;
            for (auto& d : juce::MidiInput::getAvailableDevices())
                enabledNow += dm.isMidiInputDeviceEnabled (d.identifier) ? 1 : 0;

            if (juce::MidiInput::getAvailableDevices() != inputs || juce::MidiOutput::getAvailableDevices() != outputs
                || enabledNow != enabledCount)
                replaceTab (0, buildPorts());
        }

        std::unique_ptr<juce::Component> buildPorts()
        {
            inputs = juce::MidiInput::getAvailableDevices();
            outputs = juce::MidiOutput::getAvailableDevices();
            auto& dm = ctx.engine.getDeviceManager();
            enabledCount = 0;

            auto page = std::make_unique<FormPage>();
            page->addSection (u8"MIDI-Anschlüsse");

            auto table = std::make_unique<Table> (std::vector<Table::Column> {
                { "E/A", 50 }, { "Anschluss", 0 }, { "Anzeigen als", 190 }, { "Aktiv", 70 }, { "Status", 130 } });
            table->setEmptyText (u8"Keine MIDI-Geräte gefunden. Ist das Keyboard angeschlossen und eingeschaltet?");

            auto aliasEditor = [this] (const juce::MidiDeviceInfo& d)
            {
                auto e = std::make_unique<juce::TextEditor>();
                const auto key = "midi.alias." + d.identifier.toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789");
                e->setText (ctx.settings.getValue (key, d.name), false);
                e->setFont (theme::font (13.0f));
                e->setTooltip (u8"Eigener Name für dieses Gerät");
                e->onTextChange = [this, key, ed = e.get(), name = d.name]
                {
                    const auto t = ed->getText().trim();
                    ctx.settings.setValue (key, t.isEmpty() || t == name ? juce::var() : juce::var (t));
                };
                return e;
            };

            for (auto& d : inputs)
            {
                const bool on = dm.isMidiInputDeviceEnabled (d.identifier);
                enabledCount += on ? 1 : 0;
                std::vector<std::unique_ptr<juce::Component>> cells;
                cells.push_back (Table::text ("In", true));
                cells.push_back (Table::text (d.name));
                cells.push_back (aliasEditor (d));
                auto status = Table::text (on ? "Aktiv" : "Aus", ! on);
                auto* statusPtr = status.get();
                cells.push_back (makeSwitch (on, [this, id = d.identifier, statusPtr] (bool enabled)
                {
                    ctx.engine.getDeviceManager().setMidiInputDeviceEnabled (id, enabled);
                    statusPtr->setText (enabled ? "Aktiv" : "Aus", juce::dontSendNotification);
                    enabledCount += enabled ? 1 : -1;
                    saveDeviceState (ctx);
                }));
                cells.push_back (std::move (status));
                table->addRow (std::move (cells));
            }

            for (auto& d : outputs)
            {
                std::vector<std::unique_ptr<juce::Component>> cells;
                cells.push_back (Table::text ("Out", true));
                cells.push_back (Table::text (d.name));
                cells.push_back (aliasEditor (d));
                cells.push_back (nullptr);
                cells.push_back (Table::text (u8"noch nicht genutzt", true));
                table->addRow (std::move (cells));
            }

            const auto h = table->getIdealHeight();
            page->addFull (std::move (table), h);
            page->addNote (u8"Aktive Eingänge spielen die ausgewählte Instrumentenspur (im Arrangement auf eine Spur "
                           u8"mit Instrument klicken). Neu angeschlossene Geräte erscheinen automatisch.");
            return page;
        }

        std::unique_ptr<juce::Component> buildFilter()
        {
            auto page = std::make_unique<FormPage>();
            page->addSection ("Nachrichten durchlassen");

            auto table = std::make_unique<Table> (std::vector<Table::Column> { { "Nachricht", 0 }, { "Aufnahme", 110 }, { "Thru (live)", 110 } });
            for (auto& row : midiFilterRows)
            {
                std::vector<std::unique_ptr<juce::Component>> cells;
                cells.push_back (Table::text (row.name));
                for (const char* key : { "midi.filter.record", "midi.filter.thru" })
                {
                    const auto mask = ctx.settings.getIntValue (key, defaultMidiMask);
                    auto sw = makeSwitch ((mask & row.bit) != 0, [this, key, bit = row.bit] (bool on)
                    {
                        auto m = ctx.settings.getIntValue (key, defaultMidiMask);
                        m = on ? (m | bit) : (m & ~bit);
                        ctx.settings.setValue (key, m);
                        applyMidiThru (ctx);
                    });
                    if (row.bit == AudioEngine::filterSysEx && juce::String (key) == "midi.filter.thru")
                    {
                        sw->setEnabled (false);
                        sw->setToggleState (false, juce::dontSendNotification);
                        sw->setTooltip (u8"SysEx wird live nicht weitergeleitet");
                    }
                    cells.push_back (std::move (sw));
                }
                table->addRow (std::move (cells));
            }
            const auto h = table->getIdealHeight();
            page->addFull (std::move (table), h);
            page->addNote (u8"Eingeschaltet = die Nachricht wird durchgelassen. „Thru“ ist das, was du beim Spielen "
                           u8"sofort hörst; „Aufnahme“ gilt für MIDI-Aufnahmen (kommen mit Etappe 7).");
            return page;
        }

        std::unique_ptr<juce::Component> buildOptions()
        {
            auto page = std::make_unique<FormPage>();
            page->addSection ("Eingang");
            page->addRow (u8"Neue MIDI-Geräte automatisch aktivieren",
                          makeSwitch (ctx.settings.getBoolValue ("midi.autoEnable", true),
                                      [this] (bool on) { ctx.settings.setValue ("midi.autoEnable", on); }), 60);
            page->addRow (u8"MIDI-Aktivität", std::make_unique<MidiActivity> (ctx.engine), 300);
            page->addSection (u8"Bildschirm-Klaviatur");
            page->addNote (u8"Mit F7 öffnet sich eine Klaviatur am Bildschirm; sie spielt wie ein Keyboard die "
                           u8"ausgewählte Instrumentenspur.");
            return page;
        }

        AppContext& ctx;
        juce::Array<juce::MidiDeviceInfo> inputs, outputs;
        int enabledCount = 0;
    };

    //==========================================================================
    // METRONOM
    //==========================================================================
    AudioEngine::MetronomeOptions loadMetronome (juce::PropertySet& s)
    {
        AudioEngine::MetronomeOptions o;
        o.sound = s.getIntValue ("metronome.sound", 0);
        o.volumeDb = (float) s.getDoubleValue ("metronome.volumeDb", 0.0);
        o.accent = s.getBoolValue ("metronome.accent", true);
        o.clickOnPlay = s.getBoolValue ("metronome.onPlay", true);
        o.clickOnRecord = s.getBoolValue ("metronome.onRecord", true);
        o.countInBars = s.getIntValue ("metronome.countInBars", 1);
        o.countInOnPlay = s.getBoolValue ("metronome.countInPlay", false);
        o.countInOnRecord = s.getBoolValue ("metronome.countInRecord", true);
        return o;
    }

    void storeMetronome (AppContext& ctx, const AudioEngine::MetronomeOptions& o)
    {
        auto& s = ctx.settings;
        s.setValue ("metronome.sound", o.sound);
        s.setValue ("metronome.volumeDb", o.volumeDb);
        s.setValue ("metronome.accent", o.accent);
        s.setValue ("metronome.onPlay", o.clickOnPlay);
        s.setValue ("metronome.onRecord", o.clickOnRecord);
        s.setValue ("metronome.countInBars", o.countInBars);
        s.setValue ("metronome.countInPlay", o.countInOnPlay);
        s.setValue ("metronome.countInRecord", o.countInOnRecord);
        ctx.engine.setMetronomeOptions (o);
    }

    class MetronomePage final : public FormPage, private juce::Timer
    {
    public:
        explicit MetronomePage (AppContext& c) : ctx (c), opts (loadMetronome (c.settings))
        {
            addSection ("Klick");
            enabled = &addRow ("Metronom eingeschaltet", makeSwitch (ctx.engine.isMetronomeEnabled(), [this] (bool on)
            {
                ctx.engine.setMetronomeEnabled (on);
                ctx.settings.setValue ("metronome.enabled", on);
            }), 60, u8"Wie der Metronom-Knopf in der Transportleiste");

            juce::StringArray sounds;
            for (int i = 0; i < Metronome::numSounds; ++i)
                sounds.add (Metronome::getSoundName (i));
            auto& soundBox = addRow ("Klang", makeCombo (sounds, opts.sound), 200);
            soundBox.onChange = [this, &soundBox] { opts.sound = soundBox.getSelectedItemIndex(); apply(); ctx.engine.previewClick(); };
            addRow (u8"Vorhören", makeButton (u8"Klick abspielen", [this] { ctx.engine.previewClick(); }), 200);

            auto volume = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            volume->setRange (-40.0, 6.0, 0.5);
            volume->setTextValueSuffix (" dB");
            volume->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
            volume->setValue (opts.volumeDb, juce::dontSendNotification);
            volume->setDoubleClickReturnValue (true, 0.0);
            auto& vol = addRow (u8"Lautstärke", std::move (volume), 260, u8"Doppelklick: 0 dB");
            vol.onDragEnd = [this] { ctx.engine.previewClick(); };
            vol.onValueChange = [this, &vol] { opts.volumeDb = (float) vol.getValue(); apply(); };

            addRow (u8"Betonung auf Zählzeit 1", makeSwitch (opts.accent, [this] (bool on) { opts.accent = on; apply(); }), 60,
                    u8"Der erste Schlag jedes Takts klingt höher und lauter");

            addSection (u8"Klicken während");
            addRow ("Wiedergabe", makeSwitch (opts.clickOnPlay, [this] (bool on) { opts.clickOnPlay = on; apply(); }), 60);
            addRow ("Aufnahme", makeSwitch (opts.clickOnRecord, [this] (bool on) { opts.clickOnRecord = on; apply(); }), 60);

            addSection (u8"Vorzähler");
            addRow (u8"Vorzähler bei Wiedergabe", makeSwitch (opts.countInOnPlay, [this] (bool on) { opts.countInOnPlay = on; apply(); }), 60,
                    u8"Vor dem Start der Wiedergabe zählt das Metronom vor; der Song wartet so lange");
            addRow (u8"Vorzähler bei Aufnahme", makeSwitch (opts.countInOnRecord, [this] (bool on) { opts.countInOnRecord = on; apply(); }), 60);
            auto& bars = addRow (u8"Vorzähler-Takte", makeCombo ({ "1 Takt", "2 Takte", "3 Takte", "4 Takte" },
                                                                juce::jlimit (0, 3, opts.countInBars - 1)), 200);
            bars.onChange = [this, &bars] { opts.countInBars = bars.getSelectedItemIndex() + 1; apply(); };
            addNote (u8"Der Vorzähler klickt immer – auch wenn das Metronom aus ist. Aufnahme kommt mit Etappe 7; "
                     u8"die Aufnahme-Einstellungen werden schon gespeichert.");

            startTimerHz (4); // follow the metronome button
        }

    private:
        void apply() { storeMetronome (ctx, opts); }

        void timerCallback() override
        {
            if (enabled->getToggleState() != ctx.engine.isMetronomeEnabled())
                enabled->setToggleState (ctx.engine.isMetronomeEnabled(), juce::dontSendNotification);
        }

        AppContext& ctx;
        AudioEngine::MetronomeOptions opts;
        ToggleSwitch* enabled = nullptr;
    };

    class MetronomeCategory final : public CategoryPage
    {
    public:
        explicit MetronomeCategory (AppContext& c) : CategoryPage ("Metronom") { addTab ("Metronom", std::make_unique<MetronomePage> (c)); }
    };

    /** The small metronome window (right-click on the metronome button). */
    class MetronomeWindow final : public juce::DocumentWindow
    {
    public:
        explicit MetronomeWindow (AppContext& ctx)
            : juce::DocumentWindow ("Metronom", theme::colours::panel, juce::DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar (true);
            auto content = std::make_unique<MetronomePage> (ctx);
            content->setSize (460, 600);
            setContentOwned (content.release(), true);
            setResizable (true, false);
            setAlwaysOnTop (true);
        }

        void closeButtonPressed() override
        {
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<juce::Component> (this)] { delete safe.getComponent(); });
        }
    };

    //==========================================================================
    // PLUG-INS
    //==========================================================================
    class PluginCategory final : public CategoryPage, private juce::ChangeListener
    {
    public:
        explicit PluginCategory (AppContext& c) : CategoryPage ("Plug-ins"), ctx (c)
        {
            auto search = std::make_unique<FormPage>();
            search->addSection ("Plug-in-Suche");
            status = &search->addRow ("Status", makeInfo ({}), 420);
            search->addRow ("Alles neu suchen", makeButton (u8"Plug-ins suchen …", [this] { startPluginScanWithProgress (ctx, true); },
                            u8"Alle Plug-ins komplett neu prüfen, auch übersprungene – als wäre noch nichts bekannt"), 220);
            search->addRow ("Nur neue Plug-ins", makeButton (u8"Aktualisieren …", [this] { startPluginScanWithProgress (ctx, false); },
                            u8"Nur nach neu installierten oder geänderten Plug-ins schauen (schnell)"), 220);
            search->addNote (u8"SM Studio sucht VST3-Plug-ins (am Mac auch Audio Units) in den Standardordnern. Jedes "
                             u8"Plug-in wird in einem eigenen Prozess geprüft: stürzt es ab oder hängt es länger als "
                             u8"30 Sekunden, wird es übersprungen.");
            addTab ("Suche", std::move (search));

            auto skippedPage = std::make_unique<FormPage>();
            skippedPage->addSection (u8"Übersprungene Plug-ins");
            auto editor = std::make_unique<juce::TextEditor>();
            editor->setMultiLine (true);
            editor->setReadOnly (true);
            editor->setFont (theme::font (12.5f));
            skipped = &skippedPage->addFull (std::move (editor), 300);
            skippedPage->addNote (u8"„Plug-ins suchen“ prüft auch diese noch einmal.");
            addTab (u8"Übersprungen", std::move (skippedPage));

            ctx.plugins.addChangeListener (this);
            update();
        }

        ~PluginCategory() override { ctx.plugins.removeChangeListener (this); }

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override { update(); }

        void update()
        {
            status->setText (ctx.plugins.getStatusText(), juce::dontSendNotification);
            skipped->setText (ctx.plugins.getNumSkippedPlugins() > 0 ? ctx.plugins.getSkippedPlugins().joinIntoString ("\n")
                                                                       : juce::String ("(keine)"));
        }

        AppContext& ctx;
        juce::Label* status = nullptr;
        juce::TextEditor* skipped = nullptr;
    };

    //==========================================================================
    // DARSTELLUNG
    //==========================================================================
    /** Clickable preview tiles of the colour schemes. */
    class SchemeTiles final : public juce::Component
    {
    public:
        SchemeTiles (int sel, std::function<void (int)> cb) : selected (sel), onPick (std::move (cb))
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void paint (juce::Graphics& g) override
        {
            const auto names = theme::schemeNames();
            for (int i = 0; i < names.size(); ++i)
            {
                auto r = tile (i).toFloat();
                const auto p = theme::schemePreview (i);
                auto box = r.removeFromTop (r.getHeight() - 22.0f).reduced (2.0f);

                g.setColour (p.window);
                g.fillRoundedRectangle (box, 4.0f);
                auto inner = box.reduced (6.0f);
                g.setColour (p.panel);
                g.fillRect (inner.removeFromLeft (inner.getWidth() * 0.3f));
                inner.removeFromLeft (4.0f);
                g.setColour (p.header);
                g.fillRect (inner.removeFromTop (10.0f));
                inner.removeFromTop (6.0f);
                g.setColour (p.accent);
                g.fillRect (inner.removeFromTop (8.0f).withWidth (inner.getWidth() * 0.6f));
                inner.removeFromTop (4.0f);
                g.setColour (p.text.withAlpha (0.6f));
                g.fillRect (inner.removeFromTop (4.0f).withWidth (inner.getWidth() * 0.8f));

                g.setColour (i == selected ? theme::colours::accent : (i == hover ? theme::colours::textDim : theme::colours::separator));
                g.drawRoundedRectangle (box.expanded (1.0f), 5.0f, i == selected ? 2.5f : 1.0f);

                g.setColour (i == selected ? theme::colours::text : theme::colours::textDim);
                g.setFont (theme::font (12.5f, i == selected));
                g.drawText (names[i], r, juce::Justification::centred);
            }
        }

        void mouseMove (const juce::MouseEvent& e) override { setHover (indexAt (e.getPosition())); }
        void mouseExit (const juce::MouseEvent&) override { setHover (-1); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (const auto i = indexAt (e.getPosition()); i >= 0 && i != selected)
            {
                selected = i;
                repaint();
                if (onPick)
                    onPick (i);
            }
        }

    private:
        juce::Rectangle<int> tile (int i) const
        {
            const auto n = theme::schemeNames().size();
            const auto w = juce::jmin (150, (getWidth() - (n - 1) * 12) / n);
            return { i * (w + 12), 0, w, getHeight() };
        }
        int indexAt (juce::Point<int> p) const
        {
            for (int i = 0; i < theme::schemeNames().size(); ++i)
                if (tile (i).contains (p))
                    return i;
            return -1;
        }
        void setHover (int h) { if (h != hover) { hover = h; repaint(); } }

        int selected, hover = -1;
        std::function<void (int)> onPick;
    };

    const int uiScales[] = { 100, 125, 150, 175, 200 };

    class AppearanceCategory final : public CategoryPage
    {
    public:
        AppearanceCategory (AppContext& c, std::function<void()> afterSchemeChange)
            : CategoryPage ("Darstellung"), ctx (c), reopen (std::move (afterSchemeChange))
        {
            auto page = std::make_unique<FormPage>();
            page->addSection ("Allgemeines");
            auto& lang = page->addRow ("Sprache", makeCombo ({ "Deutsch" }, 0), 200);
            lang.setEnabled (false);
            lang.setTooltip (u8"Weitere Sprachen folgen später");

            juce::StringArray scales;
            int scaleIndex = 0;
            for (int i = 0; i < (int) std::size (uiScales); ++i)
            {
                scales.add (juce::String (uiScales[i]) + " %");
                if (uiScales[i] == ctx.settings.getIntValue ("ui.scale", 100))
                    scaleIndex = i;
            }
            auto& scale = page->addRow (u8"Größe der Oberfläche", makeCombo (scales, scaleIndex), 200,
                                        u8"Alles größer darstellen (z. B. für hochauflösende Bildschirme)");
            scale.onChange = [this, &scale]
            {
                const auto percent = uiScales[juce::jlimit (0, (int) std::size (uiScales) - 1, scale.getSelectedItemIndex())];
                ctx.settings.setValue ("ui.scale", percent);
                juce::MessageManager::callAsync ([percent] { juce::Desktop::getInstance().setGlobalScaleFactor ((float) percent / 100.0f); });
            };

            page->addSection ("Farbschema");
            page->addFull (std::make_unique<SchemeTiles> (ctx.settings.getIntValue ("ui.colourScheme", 0), [this] (int index)
            {
                ctx.settings.setValue ("ui.colourScheme", index);
                // rebuild after this callback has returned (this page is rebuilt too)
                juce::MessageManager::callAsync ([&c = ctx, index, cb = reopen]
                {
                    theme::applyScheme (index);
                    if (c.rebuildUi)
                        c.rebuildUi();
                    if (cb)
                        cb(); // reopen the settings window in the new colours
                });
            }), 110);
            page->addNote (u8"Ein Klick auf ein Farbschema übernimmt es sofort.");

            addTab (u8"Programmoberfläche", std::move (page));
        }

    private:
        AppContext& ctx;
        std::function<void()> reopen;
    };

    //==========================================================================
    enum Category { audio = 0, midi, metronome, plugins, appearance, numCategories };

    class SettingsComponent final : public juce::Component
    {
    public:
        SettingsComponent (AppContext& c, int initial, std::function<void()> onSchemeChanged)
            : ctx (c), reopenAfterScheme (std::move (onSchemeChanged))
        {
            juce::Path palette;
            palette.addEllipse (0.0f, 0.0f, 10.0f, 10.0f);
            palette.addEllipse (2.5f, 2.5f, 2.0f, 2.0f);
            palette.addEllipse (5.5f, 2.0f, 2.0f, 2.0f);
            palette.setUsingNonZeroWinding (false);

            juce::Path puzzle;
            puzzle.addRoundedRectangle (0.0f, 2.0f, 8.0f, 8.0f, 1.0f);
            puzzle.addEllipse (2.5f, 0.0f, 3.0f, 3.0f);
            puzzle.addEllipse (6.5f, 4.5f, 3.0f, 3.0f);

            sidebar.setEntries ({ { "Audio", theme::icons::waveform() },
                                  { "MIDI", theme::icons::instrument() },
                                  { "Metronom", theme::icons::metronome() },
                                  { "Plug-ins", puzzle, true },
                                  { "Darstellung", palette } });
            sidebar.onSelect = [this] (int i) { select (i); };
            addAndMakeVisible (sidebar);

            select (initial);
            setSize (980, 640);
        }

        void select (int index)
        {
            index = juce::jlimit (0, numCategories - 1, index);
            sidebar.setSelected (index);
            ctx.settings.setValue ("settings.lastCategory", index);

            page.reset();
            switch (index)
            {
                case audio:      page = std::make_unique<AudioCategory> (ctx); break;
                case midi:       page = std::make_unique<MidiCategory> (ctx); break;
                case metronome:  page = std::make_unique<MetronomeCategory> (ctx); break;
                case plugins:    page = std::make_unique<PluginCategory> (ctx); break;
                default:         page = std::make_unique<AppearanceCategory> (ctx, reopenAfterScheme); break;
            }
            addAndMakeVisible (*page);
            resized();
        }

        void paint (juce::Graphics& g) override { g.fillAll (theme::colours::panel); }
        void resized() override
        {
            auto r = getLocalBounds();
            sidebar.setBounds (r.removeFromLeft (190));
            if (page != nullptr)
                page->setBounds (r);
        }

    private:
        AppContext& ctx;
        std::function<void()> reopenAfterScheme;
        Sidebar sidebar;
        std::unique_ptr<juce::Component> page;
    };

    //==========================================================================
    class SettingsModule final : public Module, private juce::Timer
    {
    public:
        juce::String getId() const override { return "sms.settings"; }
        juce::String getName() const override { return "Einstellungen"; }

        void initialise (AppContext& c) override
        {
            ctx = &c;
            theme::applyScheme (c.settings.getIntValue ("ui.colourScheme", 0));
            if (const auto scale = c.settings.getIntValue ("ui.scale", 100); scale != 100)
                juce::Desktop::getInstance().setGlobalScaleFactor ((float) scale / 100.0f);

            c.engine.setMetronomeOptions (loadMetronome (c.settings));
            c.engine.setMetronomeEnabled (c.settings.getBoolValue ("metronome.enabled", false));
            applyMidiThru (c);

            auto add = [&c, this] (const char* id, const juce::String& name, int order, int category,
                                   std::vector<juce::KeyPress> keys, bool separator = false)
            {
                ActionInfo a;
                a.id = id;
                a.name = name;
                a.menu = "Einstellungen";
                a.menuOrder = order;
                a.separatorBefore = separator;
                a.keys = std::move (keys);
                a.perform = [this, category]
                {
                    // -1: the category that was open last time
                    show (category >= 0 ? category : ctx->settings.getIntValue ("settings.lastCategory", 0));
                };
                c.registry.addAction (std::move (a));
            };

            add ("settings.open", u8"Einstellungen …", 10, -1, { juce::KeyPress (',', juce::ModifierKeys::commandModifier, 0) });
            add ("settings.audioMidi", u8"Audio (Soundkarte, Puffer) …", 20, audio, {}, true);
            add ("settings.midi", u8"MIDI (Keyboards, Controller) …", 25, midi, {});
            add ("settings.metronomePage", u8"Metronom …", 27, metronome, {});
            add ("settings.plugins", u8"Plug-ins …", 30, plugins, {});
            add ("settings.appearance", u8"Darstellung (Farbschema, Größe) …", 40, appearance, {});

            ActionInfo scan;
            scan.id = "settings.scanPlugins";
            scan.name = u8"Plug-ins suchen (alles neu) …";
            scan.menu = "Einstellungen";
            scan.menuOrder = 50;
            scan.separatorBefore = true;
            scan.perform = [this] { startPluginScanWithProgress (*ctx, true); };
            c.registry.addAction (std::move (scan));

            // small metronome window (right-click on the metronome button)
            ActionInfo metro;
            metro.id = "transport.metronomeSettings";
            metro.name = u8"Metronom-Einstellungen …";
            metro.menu = "Transport";
            metro.menuOrder = 210;
            metro.perform = [this] { showMetronomeWindow(); };
            c.registry.addAction (std::move (metro));

            // watch for newly connected MIDI devices
            for (auto& d : juce::MidiInput::getAvailableDevices())
                knownMidi.insert (d.identifier);
            startTimer (2000);
        }

        void shutdown() override
        {
            stopTimer();
            delete metronomeWindow.getComponent();
            if (window != nullptr)
                delete window.getComponent();
        }

    private:
        void timerCallback() override
        {
            for (auto& d : juce::MidiInput::getAvailableDevices())
            {
                if (knownMidi.insert (d.identifier).second && ctx->settings.getBoolValue ("midi.autoEnable", true))
                {
                    ctx->engine.getDeviceManager().setMidiInputDeviceEnabled (d.identifier, true);
                    saveDeviceState (*ctx);
                }
            }
        }

        void show (int category)
        {
            if (window != nullptr)
            {
                if (auto* sc = dynamic_cast<SettingsComponent*> (window->getContentComponent()))
                    sc->select (category);
                window->toFront (true);
                return;
            }

            juce::DialogWindow::LaunchOptions o;
            o.content.setOwned (new SettingsComponent (*ctx, category, [this]
            {
                if (window != nullptr)
                    window->exitModalState (0); // deletes the dialog
                juce::MessageManager::callAsync ([this] { show (appearance); });
            }));
            o.dialogTitle = juce::String (u8"SM Studio – Einstellungen");
            o.dialogBackgroundColour = theme::colours::panel;
            o.useNativeTitleBar = true;
            o.resizable = true;
            o.componentToCentreAround = juce::TopLevelWindow::getActiveTopLevelWindow();
            window = o.launchAsync();
            if (window != nullptr)
                window->setResizeLimits (760, 480, 4000, 3000);
        }

        void showMetronomeWindow()
        {
            if (metronomeWindow != nullptr)
            {
                metronomeWindow->toFront (true);
                return;
            }

            auto* w = new MetronomeWindow (*ctx);
            const auto mouse = juce::Desktop::getMousePosition();
            const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForPoint (mouse.toFloat());
            const auto area = display != nullptr ? display->userBounds.toNearestInt() : juce::Rectangle<int> (0, 0, 1920, 1080);
            w->setTopLeftPosition (juce::jlimit (area.getX(), juce::jmax (area.getX(), area.getRight() - w->getWidth()), mouse.x - w->getWidth() / 2),
                                   juce::jlimit (area.getY(), juce::jmax (area.getY(), area.getBottom() - w->getHeight()), mouse.y - w->getHeight() - 20));
            w->setVisible (true);
            metronomeWindow = w;
        }

        AppContext* ctx = nullptr;
        juce::Component::SafePointer<juce::DialogWindow> window;
        juce::Component::SafePointer<juce::DocumentWindow> metronomeWindow;
        std::set<juce::String> knownMidi;
    };
}

std::unique_ptr<Module> createSettingsModule() { return std::make_unique<SettingsModule>(); }

} // namespace sms
