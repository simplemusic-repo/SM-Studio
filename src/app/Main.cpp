// SPDX-License-Identifier: AGPL-3.0-or-later
//
// SM Studio – application entry point.
//
// Command line options (useful for testing and CI):
//   --demo                 open with example tracks
//   --screenshot <file>    render the main window to a PNG and quit (no audio)
//   --size <W>x<H>         window size for --screenshot (default 1600x950)
//   --show <panels>        comma separated: console,inspector,browser (for --screenshot)

#include "SongHelpers.h"

#include <chrono>
#include <thread>
#include "engine/EngineSync.h"
#include "extension/ExtensionRegistry.h"
#include "modules/Modules.h"
#include "ui/MainComponent.h"
#include "ui/PluginScanDialog.h"
#include "ui/PluginWindows.h"
#include "ui/Theme.h"

namespace sms
{

/** Owns the document and all services; builds the AppContext. */
struct AppState final
{
    /** @param persistentSettings  false for screenshots/tests: settings stay in memory */
    explicit AppState (bool persistentSettings)
        : settings (chooseSettings (persistentSettings)),
          plugins (persistentSettings ? dataFolder() : juce::File())
    {
    }

    static juce::File dataFolder()
    {
       #if JUCE_LINUX || JUCE_BSD
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".config/SM Studio");
       #else
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("SM Studio");
       #endif
    }

    ~AppState()
    {
        appProperties.saveIfNeeded();
    }

    juce::PropertySet& chooseSettings (bool persistent)
    {
        if (! persistent)
            return memorySettings;

        juce::PropertiesFile::Options o;
        o.applicationName = "SM Studio";
       #if JUCE_LINUX || JUCE_BSD
        o.folderName = ".config/SM Studio";
       #else
        o.folderName = "SM Studio";
       #endif
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        o.millisecondsBeforeSaving = 1000;
        appProperties.setStorageParameters (o);
        return *appProperties.getUserSettings();
    }

    juce::ApplicationProperties appProperties;
    juce::PropertySet memorySettings;
    juce::PropertySet& settings;

    Song song;
    CommandHistory history { song };
    Selection selection;
    AudioEngine engine;
    AudioPool pool { song };
    PluginHost plugins;
    EngineSync engineSync { song, pool, engine, &plugins };
    PluginWindows pluginWindows { plugins }; // destroyed before the plug-ins
    ExtensionRegistry registry;
    EditState edit;
    AppContext ctx { song, history, selection, engine, pool, plugins, pluginWindows, registry, edit, settings, {} };
};

//==============================================================================
class MainWindow final : public juce::DocumentWindow
{
public:
    explicit MainWindow (AppContext& c)
        : juce::DocumentWindow (juce::JUCEApplication::getInstance()->getApplicationName(),
                                theme::colours::window, juce::DocumentWindow::allButtons),
          ctx (c)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new MainComponent (ctx), true);
        setResizable (true, true);
        setResizeLimits (1000, 640, 10000, 10000);

        auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        const auto area = display != nullptr ? display->userBounds.toNearestInt() : juce::Rectangle<int> (1600, 950);
        centreWithSize (juce::jmin (1600, area.getWidth() - 40), juce::jmin (950, area.getHeight() - 40));

        // restore the last position if that screen is still connected
        const auto saved = ctx.settings.getValue ("mainWindow.state");
        const auto before = getBounds();
        if (saved.isNotEmpty() && restoreWindowStateFromString (saved))
        {
            const auto screens = juce::Desktop::getInstance().getDisplays().getRectangleList (true);
            if (! screens.intersectsRectangle (getBounds().reduced (60)))
                setBounds (before);
        }

        setVisible (true);
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<juce::Component> (getContentComponent())]
        {
            if (safe != nullptr)
                safe->grabKeyboardFocus();
        });
    }

    /** Keys that reach the window itself (no component focused) go to the main
        component so shortcuts always work. */
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (auto* main = getContentComponent())
            return main->keyPressed (key);
        return false;
    }

    ~MainWindow() override
    {
        ctx.settings.setValue ("mainWindow.state", getWindowStateAsString());
        if (auto* main = dynamic_cast<MainComponent*> (getContentComponent()))
            main->saveLayout();
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

    /** Recreates the UI (after a colour scheme change). Song, history and layout are kept. */
    void rebuildContent()
    {
        if (auto* main = dynamic_cast<MainComponent*> (getContentComponent()))
            main->saveLayout();
        setBackgroundColour (theme::colours::window);
        setContentOwned (new MainComponent (ctx), false);
        getContentComponent()->grabKeyboardFocus();
    }

private:
    AppContext& ctx;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

//==============================================================================
class Application final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "SM Studio"; }
    const juce::String getApplicationVersion() override { return SMS_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return true; }

    /** German texts for JUCE's built-in dialogs (audio device settings). */
    static void installTranslations()
    {
        auto* t = new juce::LocalisedStrings (juce::String::fromUTF8 (
            "language: Deutsch\n"
            "\"Audio device type:\" = \"Treiber:\"\n"
            "\"Device:\" = \"Gerät:\"\n"
            "\"Output:\" = \"Ausgang:\"\n"
            "\"Input:\" = \"Eingang:\"\n"
            "\"Sample rate:\" = \"Abtastrate:\"\n"
            "\"Audio buffer size:\" = \"Puffergröße:\"\n"
            "\"Active MIDI inputs:\" = \"MIDI-Eingänge:\"\n"
            "\"MIDI Output:\" = \"MIDI-Ausgang:\"\n"
            "\"Active input channels:\" = \"Aktive Eingänge:\"\n"
            "\"Active output channels:\" = \"Aktive Ausgänge:\"\n"
            "\"(no audio input channels found)\" = \"(keine Audio-Eingänge gefunden)\"\n"
            "\"(no audio output channels found)\" = \"(keine Audio-Ausgänge gefunden)\"\n"
            "\"No MIDI inputs available\" = \"Keine MIDI-Eingänge vorhanden\"\n"
            "\"Control Panel\" = \"Treiber-Einstellungen\"\n"
            "\"Opens the device's own control panel\" = \"Öffnet die Einstellungen des Soundkarten-Treibers (z. B. ASIO-Puffer)\"\n"
            "\"Plays a test tone\" = \"Spielt einen Testton\"\n"
            "\"Reset Device\" = \"Gerät zurücksetzen\"\n"
            "\"Show advanced settings...\" = \"Erweiterte Einstellungen …\"\n"
            "\"Error when trying to open audio device!\" = \"Das Audiogerät konnte nicht geöffnet werden!\"\n"
            "\"Scan for bluetooth MIDI devices\" = \"Bluetooth-MIDI-Geräte suchen\"\n"
            "\"none\" = \"keins\"\n"
            "\"samples\" = \"Samples\"\n"
            "\"Hide advanced settings\" = \"Erweiterte Einstellungen ausblenden\"\n"), false);
        juce::LocalisedStrings::setCurrentMappings (t);
    }

    void initialise (const juce::String& commandLine) override
    {
        // plug-in scanner child process: check one plug-in and quit (no window)
        {
            const auto args = juce::StringArray::fromTokens (commandLine, true);
            const auto i = args.indexOf ("--scan-plugin");
            if (i >= 0 && i + 3 < args.size())
            {
                // safety net: never outlive the scan (e.g. if SM Studio itself was closed meanwhile)
                std::thread ([] { std::this_thread::sleep_for (std::chrono::seconds (45)); std::_Exit (3); }).detach();

                const auto ok = PluginHost::scanFileAndWriteResult (args[i + 1].unquoted(), args[i + 2].unquoted(),
                                                                    juce::File (args[i + 3].unquoted()));
                setApplicationReturnValue (ok ? 0 : 1);
                quit();
                return;
            }
        }

        installTranslations();
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

        const auto args = juce::StringArray::fromTokens (commandLine, true);
        auto argValue = [&args] (const juce::String& name)
        {
            const auto i = args.indexOf (name);
            return i >= 0 && i + 1 < args.size() ? args[i + 1].unquoted() : juce::String();
        };

        const bool screenshotMode = argValue ("--screenshot").isNotEmpty();
        state = std::make_unique<AppState> (! screenshotMode);

        for (auto& m : createBuiltInModules())
            state->registry.addModule (std::move (m));
        state->registry.initialiseModules (state->ctx);
        lookAndFeel.refreshColours(); // colour scheme from the settings (applied by the settings module)

        if (args.contains ("--demo") || args.contains ("--screenshot"))
            createDemoSong (state->ctx);

        if (const auto shot = argValue ("--screenshot"); shot.isNotEmpty())
        {
            renderScreenshot (juce::File::getCurrentWorkingDirectory().getChildFile (shot),
                              argValue ("--size"), argValue ("--show"));
            return;
        }

        const auto savedDevice = state->settings.getXmlValue ("audio.deviceState");
        if (auto error = state->engine.initialiseAudio (savedDevice.get()); error.isNotEmpty())
            juce::Logger::writeToLog ("Audio initialisation failed: " + error);

        mainWindow = std::make_unique<MainWindow> (state->ctx);

        state->ctx.rebuildUi = [this]
        {
            lookAndFeel.refreshColours();
            if (mainWindow != nullptr)
                mainWindow->rebuildContent();
        };

        // first start: look for installed plug-ins in the background
        if (! AppState::dataFolder().getChildFile ("plugins.xml").existsAsFile())
            juce::MessageManager::callAsync ([this] { if (state != nullptr) startPluginScanWithProgress (state->ctx); });
    }

    void shutdown() override
    {
        mainWindow.reset();

        if (state != nullptr)
        {
            state->pluginWindows.closeAll();

            if (auto xml = state->engine.createDeviceStateXml())
                state->settings.setValue ("audio.deviceState", xml.get());
            state->engine.shutdownAudio();
            state->registry.shutdownModules();
        }

        state.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

private:
    void renderScreenshot (const juce::File& file, const juce::String& size, const juce::String& show)
    {
        int w = 1600, h = 950;
        if (size.containsChar ('x'))
        {
            w = size.upToFirstOccurrenceOf ("x", false, false).getIntValue();
            h = size.fromFirstOccurrenceOf ("x", false, false).getIntValue();
        }

        {
            MainComponent comp (state->ctx, true);
            comp.setSize (w, h);

            for (auto& p : juce::StringArray::fromTokens (show, ",", ""))
            {
                if (p == "console")   comp.setPanelVisible (PanelLocation::bottom, true);
                if (p == "inspector") comp.setPanelVisible (PanelLocation::left, true);
                if (p == "browser")   comp.setPanelVisible (PanelLocation::right, true);
            }

            state->engine.setPositionSeconds (state->song.getTempoMap().ticksToSeconds (state->song.getTempoMap().getTicksPerBar() * 5 + 960));

            auto image = comp.createComponentSnapshot (comp.getLocalBounds(), true, 1.0f);
            file.deleteFile();
            juce::FileOutputStream out (file);
            juce::PNGImageFormat png;

            if (out.openedOk() && png.writeImageToStream (image, out))
                std::printf ("Screenshot written: %s\n", file.getFullPathName().toRawUTF8());
            else
                setApplicationReturnValue (1);
        }

        quit();
    }

    theme::LookAndFeel lookAndFeel;
    std::unique_ptr<AppState> state;
    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace sms

START_JUCE_APPLICATION (sms::Application)
