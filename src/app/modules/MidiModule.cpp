// SPDX-License-Identifier: AGPL-3.0-or-later
// Audio/MIDI device settings, live MIDI routing to the selected instrument
// track, and the on-screen keyboard.

#include "Modules.h"

#include "extension/ExtensionRegistry.h"
#include "ui/Theme.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace sms
{

namespace
{
    //==========================================================================
    /** On-screen keyboard: mouse or computer keys (A W S E D F T G Y H U J K)
        play the selected instrument track. */
    class KeyboardWindow final : public juce::DocumentWindow,
                                 private juce::MidiKeyboardState::Listener
    {
    public:
        explicit KeyboardWindow (AppContext& c)
            : juce::DocumentWindow (u8"SM Studio – Bildschirm-Klaviatur", theme::colours::window,
                                    juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
              ctx (c)
        {
            setUsingNativeTitleBar (true);
            state.addListener (this);

            content.keyboard.setKeyPressBaseOctave (octave);
            content.keyboard.setAvailableRange (21, 108);
            content.keyboard.setLowestVisibleKey (36);
            content.keyboard.setWantsKeyboardFocus (true);
            content.down.onClick = [this] { setOctave (octave - 1); };
            content.up.onClick = [this] { setOctave (octave + 1); };

            setContentNonOwned (&content, true);
            setResizable (true, false);
            setResizeLimits (420, 120, 3000, 300);
            centreWithSize (980, 150);
            updateHint();
        }

        ~KeyboardWindow() override
        {
            state.removeListener (this);
            clearContentComponent();
        }

        void show()
        {
            setVisible (true);
            toFront (true);
            content.keyboard.grabKeyboardFocus();
        }

        void closeButtonPressed() override { setVisible (false); }

        bool keyPressed (const juce::KeyPress& key) override
        {
            // other shortcuts (space = play ...) keep working here
            if (auto* a = ctx.registry.findActionForKey (key))
            {
                if (a->perform && (! a->isEnabled || a->isEnabled()))
                    a->perform();
                return true;
            }
            return false;
        }

    private:
        void setOctave (int o)
        {
            octave = juce::jlimit (1, 8, o);
            content.keyboard.setKeyPressBaseOctave (octave);
            content.keyboard.setLowestVisibleKey (juce::jmax (21, (octave - 1) * 12));
            updateHint();
            content.keyboard.grabKeyboardFocus();
        }

        void updateHint()
        {
            const auto* t = ctx.song.findTrack (ctx.engine.getLiveMidiTrack());
            content.hint.setText (juce::String (u8"Spielt: ") + (t != nullptr ? t->name : juce::String (u8"– (Instrumentenspur auswählen)"))
                                  + juce::String (u8"   ·   Maus oder Tasten A W S E D F T G Z H U J K   ·   Oktave ") + juce::String (octave),
                                  juce::dontSendNotification);
        }

        void handleNoteOn (juce::MidiKeyboardState*, int channel, int note, float velocity) override
        {
            ctx.engine.addLiveMidi (juce::MidiMessage::noteOn (channel, note, velocity));
        }

        void handleNoteOff (juce::MidiKeyboardState*, int channel, int note, float velocity) override
        {
            ctx.engine.addLiveMidi (juce::MidiMessage::noteOff (channel, note, velocity));
        }

        struct Content final : public juce::Component
        {
            explicit Content (juce::MidiKeyboardState& s) : keyboard (s, juce::KeyboardComponentBase::horizontalKeyboard)
            {
                hint.setFont (theme::font (12.0f));
                hint.setColour (juce::Label::textColourId, theme::colours::textDim);
                down.setTooltip ("Oktave tiefer");
                up.setTooltip (u8"Oktave höher");
                for (auto* b : { &down, &up })
                    b->setWantsKeyboardFocus (false);
                addAndMakeVisible (hint);
                addAndMakeVisible (down);
                addAndMakeVisible (up);
                addAndMakeVisible (keyboard);
                setSize (980, 150);
            }

            void paint (juce::Graphics& g) override { g.fillAll (theme::colours::panel); }

            void resized() override
            {
                auto r = getLocalBounds().reduced (6);
                auto top = r.removeFromTop (24);
                up.setBounds (top.removeFromRight (30));
                down.setBounds (top.removeFromRight (30));
                hint.setBounds (top);
                r.removeFromTop (4);
                keyboard.setBounds (r);
            }

            juce::MidiKeyboardComponent keyboard;
            juce::Label hint;
            juce::TextButton down { "-" }, up { "+" };
        };

        AppContext& ctx;
        juce::MidiKeyboardState state;
        Content content { state };
        int octave = 5;

        friend class MidiModule;
    };

    //==========================================================================
    class MidiModule final : public Module,
                             private Selection::Listener,
                             private Song::Listener
    {
    public:
        juce::String getId() const override { return "sms.midi"; }
        juce::String getName() const override { return "Audio/MIDI"; }

        void initialise (AppContext& c) override
        {
            ctx = &c;
            auto& reg = c.registry;

            ActionInfo keyboard;
            keyboard.id = "view.keyboard";
            keyboard.name = "Bildschirm-Klaviatur";
            keyboard.menu = "Ansicht";
            keyboard.menuOrder = 40;
            keyboard.keys = { juce::KeyPress (juce::KeyPress::F7Key) };
            keyboard.perform = [this] { toggleKeyboard(); };
            keyboard.isTicked = [this] { return keyboardWindow != nullptr && keyboardWindow->isVisible(); };
            reg.addAction (std::move (keyboard));

            c.selection.addListener (this);
            c.song.addListener (this);
            updateLiveTrack();
        }

        void shutdown() override
        {
            if (ctx != nullptr)
            {
                ctx->selection.removeListener (this);
                ctx->song.removeListener (this);
            }
            keyboardWindow.reset();
        }

    private:
        void selectionChanged() override { updateLiveTrack(); }

        void songChanged (const SongChange& c) override
        {
            if (c.what != SongChange::What::regionsChanged && c.what != SongChange::What::loopChanged)
                updateLiveTrack();
        }

        /** Live MIDI plays the selected track if it is an instrument track. */
        void updateLiveTrack()
        {
            auto* t = ctx->song.findTrack (ctx->selection.getSelectedTrack());
            const auto id = t != nullptr && t->typeId == "instrument" ? t->id : ObjectId {};
            ctx->engine.setLiveMidiTrack (id);
            if (keyboardWindow != nullptr)
                keyboardWindow->updateHint();
        }

        void toggleKeyboard()
        {
            if (keyboardWindow == nullptr)
                keyboardWindow = std::make_unique<KeyboardWindow> (*ctx);

            if (keyboardWindow->isVisible())
                keyboardWindow->setVisible (false);
            else
                keyboardWindow->show();
        }

        AppContext* ctx = nullptr;
        std::unique_ptr<KeyboardWindow> keyboardWindow;
    };
}

std::unique_ptr<Module> createMidiModule() { return std::make_unique<MidiModule>(); }

} // namespace sms
