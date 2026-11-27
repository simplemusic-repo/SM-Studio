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
#include "extension/ExtensionRegistry.h"
#include "modules/Modules.h"
#include "ui/MainComponent.h"
#include "ui/Theme.h"

namespace sms
{

/** Owns the document and all services; builds the AppContext. */
struct AppState final : private Song::Listener
{
    AppState()
    {
        song.addListener (this);
        syncTempo();
    }

    ~AppState() override { song.removeListener (this); }

    void songChanged (const SongChange& c) override
    {
        if (c.what == SongChange::What::tempoChanged || c.what == SongChange::What::reset)
            syncTempo();
    }

    void syncTempo()
    {
        engine.setTempo (song.getTempoMap().getTempo(), song.getTempoMap().getTimeSignature());
    }

    Song song;
    CommandHistory history { song };
    Selection selection;
    AudioEngine engine;
    ExtensionRegistry registry;
    EditState edit;
    AppContext ctx { song, history, selection, engine, registry, edit };
};

//==============================================================================
class MainWindow final : public juce::DocumentWindow
{
public:
    explicit MainWindow (AppContext& ctx)
        : juce::DocumentWindow (juce::JUCEApplication::getInstance()->getApplicationName(),
                                theme::colours::window, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new MainComponent (ctx), true);
        setResizable (true, true);
        setResizeLimits (1000, 640, 10000, 10000);

        auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        const auto area = display != nullptr ? display->userBounds.toNearestInt() : juce::Rectangle<int> (1600, 950);
        centreWithSize (juce::jmin (1600, area.getWidth() - 40), juce::jmin (950, area.getHeight() - 40));
        setVisible (true);
        getContentComponent()->grabKeyboardFocus();
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

//==============================================================================
class Application final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "SM Studio"; }
    const juce::String getApplicationVersion() override { return SMS_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String& commandLine) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

        const auto args = juce::StringArray::fromTokens (commandLine, true);
        auto argValue = [&args] (const juce::String& name)
        {
            const auto i = args.indexOf (name);
            return i >= 0 && i + 1 < args.size() ? args[i + 1].unquoted() : juce::String();
        };

        state = std::make_unique<AppState>();

        for (auto& m : createBuiltInModules())
            state->registry.addModule (std::move (m));
        state->registry.initialiseModules (state->ctx);

        if (args.contains ("--demo") || args.contains ("--screenshot"))
            createDemoSong (state->ctx);

        if (const auto shot = argValue ("--screenshot"); shot.isNotEmpty())
        {
            renderScreenshot (juce::File::getCurrentWorkingDirectory().getChildFile (shot),
                              argValue ("--size"), argValue ("--show"));
            return;
        }

        if (auto error = state->engine.initialiseAudio(); error.isNotEmpty())
            juce::Logger::writeToLog ("Audio initialisation failed: " + error);

        mainWindow = std::make_unique<MainWindow> (state->ctx);
    }

    void shutdown() override
    {
        mainWindow.reset();

        if (state != nullptr)
        {
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
            MainComponent comp (state->ctx, false);
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
