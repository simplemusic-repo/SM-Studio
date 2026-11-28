// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Panels.h"

#include "PluginWindows.h"
#include "SongHelpers.h"
#include "core/Commands.h"

#include <map>
#include "core/ValueFormat.h"
#include "extension/ExtensionRegistry.h"

namespace sms
{

using namespace theme;
using Param = commands::SetTrackParam::Param;

namespace
{
    constexpr int panelHeaderHeight = 28;

    using valueformat::formatDb;
    using valueformat::formatPan;

    void setupVolumeSlider (juce::Slider& s)
    {
        s.setRange (-60.0, 10.0, 0.1);
        s.setSkewFactorFromMidPoint (-12.0);
        s.setDoubleClickReturnValue (true, 0.0);
        s.setWantsKeyboardFocus (false);
    }

    void setupPanSlider (juce::Slider& s)
    {
        s.setRange (-1.0, 1.0, 0.01);
        s.setDoubleClickReturnValue (true, 0.0);
        s.getProperties().set ("bipolar", true);
        s.setWantsKeyboardFocus (false);
    }

    /** Mixer faders: grab anywhere and drag (no jumping to the click
        position), fine adjustment with Shift, dB scale painted beside. */
    void setupFaderFeel (juce::Slider& s)
    {
        s.setSliderSnapsToMousePosition (false);
        s.setVelocityBasedMode (false);
        s.setMouseDragSensitivity (300);
        s.getProperties().set ("dbScale", true);
        s.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        s.setTooltip (u8"Ziehen: Lautstärke  ·  Doppelklick: 0 dB");
    }

    /** dB from slider value: the bottom end of the fader means silence. */
    float sliderToDb (double v) { return v <= -59.95 ? Track::minVolumeDb : (float) v; }

    /** Typing a value into a label sets the track parameter (undoable). */
    void bindValueLabel (juce::Label& l, AppContext& ctx, std::function<ObjectId()> track, Param p)
    {
        l.onTextChange = [&l, &ctx, track, p]
        {
            const auto parsed = p == Param::volumeDb ? valueformat::parseDb (l.getText())
                                                     : valueformat::parsePan (l.getText());
            const auto* t = ctx.song.findTrack (track());

            if (parsed && t != nullptr)
                ctx.perform (std::make_unique<commands::SetTrackParam> (t->id, p, *parsed));

            // show the (clamped / unchanged) value in its normal format again
            if ((t = ctx.song.findTrack (track())) != nullptr)
                l.setText (p == Param::volumeDb ? formatDb (t->volumeDb) : formatPan (t->pan), juce::dontSendNotification);
        };
    }

    void bindToggle (juce::Button& b, AppContext& ctx, std::function<ObjectId()> track, Param p)
    {
        b.onClick = [&b, &ctx, track, p]
        {
            ctx.perform (std::make_unique<commands::SetTrackParam> (track(), p, b.getToggleState() ? 0.0f : 1.0f));
        };
    }

    void bindTrackSlider (juce::Slider& s, AppContext& ctx, std::function<ObjectId()> track, Param p, bool isVolume)
    {
        s.onValueChange = [&ctx, &s, track, p, isVolume]
        {
            const auto value = isVolume ? sliderToDb (s.getValue()) : (float) s.getValue();
            ctx.perform (std::make_unique<commands::SetTrackParam> (track(), p, value), true);
        };
        s.onDragEnd = [&ctx] { ctx.history.endGesture(); };
    }
} // namespace (shared helpers)

//==============================================================================
/** Insert slots of one channel. Each slot: power button (bypass), name
    (click: menu, double click: open), open button. "+" adds an effect;
    effects dropped from the browser on a slot replace it, below the last
    slot they are appended. */
class InsertsBox final : public juce::Component,
                         public juce::DragAndDropTarget,
                         public juce::TooltipClient
{
public:
    InsertsBox (AppContext& c, ObjectId track, int slots = 3, int rowHeight = 18)
        : ctx (c), trackId (track), numSlots (slots), slotHeight (rowHeight) {}

    static constexpr int titleHeight = 14;
    int preferredHeight() const { return titleHeight + numSlots * slotHeight + 2; }

    void setTrack (ObjectId id) { trackId = id; repaint(); }

    juce::String getTooltip() override
    {
        const auto pos = getMouseXYRelative();
        const auto index = slotAt (pos.y);
        const auto* t = ctx.song.findTrack (trackId);
        if (t == nullptr || index < 0)
            return {};
        if (index >= (int) t->inserts.size())
            return u8"Effekt hinzufügen (oder aus dem Browser hierher ziehen)";
        if (powerArea (index).contains (pos))
            return u8"Ein/Aus (Bypass)";
        if (openArea (index).contains (pos))
            return u8"Effekt öffnen";
        const auto err = ctx.plugins.getInstanceError (t->inserts[(size_t) index].id);
        return err.isNotEmpty() ? err : juce::String (u8"Klick: Menü · Doppelklick: öffnen · Effekt darauf ziehen: ersetzen");
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (colours::textDim);
        g.setFont (font (10.5f, true));
        g.drawText ("INSERTS", r.removeFromTop (titleHeight), juce::Justification::centredLeft);

        g.setColour (colours::panelDark);
        g.fillRoundedRectangle (r.toFloat(), 3.0f);

        const auto* t = ctx.song.findTrack (trackId);
        const auto count = t != nullptr ? (int) t->inserts.size() : 0;
        const auto visible = std::min (count, numSlots);

        for (int i = 0; i < numSlots; ++i)
        {
            auto slot = slotArea (i);
            const bool hover = dropSlot == i;

            if (i < visible)
            {
                const auto& ins = t->inserts[(size_t) i];
                const bool failed = ctx.plugins.getInstanceError (ins.id).isNotEmpty();

                g.setColour (hover ? colours::accentDim : (ins.bypassed ? colours::panelLight : colours::header));
                g.fillRoundedRectangle (slot.toFloat(), 2.5f);

                // power button
                auto power = powerArea (i).toFloat().reduced (2.0f);
                const auto on = ! ins.bypassed;
                juce::Path p;
                const auto c = power.getCentre();
                const auto rad = std::min (power.getWidth(), power.getHeight()) * 0.38f;
                p.addCentredArc (c.x, c.y, rad, rad, 0.0f, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
                g.setColour (on ? colours::accent : colours::textDim);
                g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.drawLine (c.x, c.y - rad - 1.0f, c.x, c.y - 1.0f, 1.6f);

                // open button
                auto open = openArea (i).toFloat().reduced (3.0f);
                auto icon = icons::detach();
                g.setColour (colours::textDim);
                g.fillPath (icon, icon.getTransformToScaleToFit (open, true));

                // name
                auto nameArea = slot.withTrimmedLeft (powerArea (i).getWidth() + 2).withTrimmedRight (openArea (i).getWidth() + 2);
                g.setColour (failed ? colours::record : (on ? colours::text : colours::textDim));
                g.setFont (font (std::min (12.5f, (float) slotHeight * 0.62f)));
                g.drawText (ins.name, nameArea, juce::Justification::centredLeft, true);
            }
            else if (i == visible && count <= numSlots)
            {
                g.setColour (hover ? colours::accentDim : colours::textDim.withAlpha (0.8f));
                if (hover)
                    g.fillRoundedRectangle (slot.toFloat(), 2.5f);
                g.setColour (colours::textDim);
                g.drawText ("+", slot, juce::Justification::centred);
            }
        }

        if (count > numSlots)
        {
            g.setColour (colours::textDim);
            g.setFont (font (10.5f));
            g.drawText ("+" + juce::String (count - numSlots) + " weitere", getLocalBounds().removeFromTop (titleHeight),
                        juce::Justification::centredRight);
        }

        if (dropSlot >= 0)
        {
            g.setColour (colours::accent);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 3.0f, 1.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        ctx.selection.selectTrack (trackId);
        const auto index = slotAt (e.y);
        const auto* t = ctx.song.findTrack (trackId);
        if (t == nullptr || index < 0)
            return;

        if (index >= (int) t->inserts.size())
        {
            showAddMenu (-1);
            return;
        }

        const auto slot = t->inserts[(size_t) index];

        if (powerArea (index).contains (e.getPosition()))
        {
            ctx.perform (std::make_unique<commands::SetInsertBypass> (trackId, slot.id, ! slot.bypassed));
            return;
        }

        if (openArea (index).contains (e.getPosition()) || e.getNumberOfClicks() > 1)
        {
            openEditor (slot);
            return;
        }

        juce::PopupMenu m;
        m.addSectionHeader (slot.name);
        const auto error = ctx.plugins.getInstanceError (slot.id);
        if (error.isNotEmpty())
            m.addItem (-1, error, false);
        m.addItem (3, juce::String (u8"Öffnen …"), error.isEmpty());
        m.addItem (1, "Ein (nicht umgangen)", true, ! slot.bypassed);
        m.addItem (4, juce::String (u8"Ersetzen durch …"));
        m.addSeparator();
        m.addItem (2, "Entfernen");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                         [this, safe = juce::Component::SafePointer<InsertsBox> (this), track = trackId, slot] (int result)
                         {
                             if (safe == nullptr)
                                 return;
                             auto& c = ctx;
                             if (result == 1) c.perform (std::make_unique<commands::SetInsertBypass> (track, slot.id, ! slot.bypassed));
                             if (result == 2) { c.pluginWindows.close (slot.id); c.perform (std::make_unique<commands::RemoveInsert> (track, slot.id)); }
                             if (result == 3) openEditor (slot);
                             if (result == 4) showAddMenu (slotIndexOf (slot.id));
                         });
    }

    void openEditor (const InsertSlot& slot)
    {
        auto* t = ctx.song.findTrack (trackId);
        ctx.pluginWindows.open (slot.id, slot.pluginId, slot.name + (t != nullptr ? juce::String (u8" – ") + t->name : juce::String()));
    }

    /** Menu of all effects; replaces slot `replaceIndex`, or appends if -1. */
    void showAddMenu (int replaceIndex)
    {
        std::vector<BrowserItem> effects;
        for (auto& src : ctx.registry.getBrowserSources())
            for (auto& item : src->getItems())
                if (item.kind == "effect")
                    effects.push_back (item);

        juce::PopupMenu m;
        m.addSectionHeader (replaceIndex >= 0 ? juce::String ("Effekt ersetzen") : juce::String (u8"Effekt hinzufügen"));
        if (effects.empty())
            m.addItem (-1, juce::String (u8"Keine Effekte gefunden – im Browser „Plug-ins suchen“"), false);

        // grouped by manufacturer
        std::map<juce::String, juce::PopupMenu> byMaker;
        for (size_t i = 0; i < effects.size(); ++i)
            byMaker[effects[i].category].addItem ((int) i + 1, effects[i].name);
        for (auto& [maker, sub] : byMaker)
            m.addSubMenu (maker, sub);

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                         [safe = juce::Component::SafePointer<InsertsBox> (this), effects, replaceIndex] (int result)
                         {
                             if (safe != nullptr && result > 0 && result <= (int) effects.size())
                                 safe->insertEffect (effects[(size_t) result - 1], replaceIndex);
                         });
    }

    void insertEffect (const BrowserItem& item, int replaceIndex)
    {
        InsertSlot slot;
        slot.id = ctx.song.newId();
        slot.pluginId = item.id;
        slot.name = item.name;
        ctx.plugins.noteUsed (item.id);

        const auto* t = ctx.song.findTrack (trackId);
        if (t != nullptr && replaceIndex >= 0 && replaceIndex < (int) t->inserts.size())
        {
            const auto oldId = t->inserts[(size_t) replaceIndex].id;
            ctx.pluginWindows.close (oldId);
            ctx.perform (std::make_unique<commands::ReplaceInsert> (trackId, oldId, slot));
        }
        else
        {
            ctx.perform (std::make_unique<commands::AddInsert> (trackId, slot));
        }
    }

    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        auto item = BrowserItem::fromDragDescription (d.description);
        return item && item->kind == "effect";
    }

    void itemDragEnter (const SourceDetails& d) override { itemDragMove (d); }
    void itemDragMove (const SourceDetails& d) override
    {
        const auto i = std::max (0, slotAt (d.localPosition.y));
        if (i != dropSlot)
        {
            dropSlot = i;
            repaint();
        }
    }
    void itemDragExit (const SourceDetails&) override { dropSlot = -1; repaint(); }

    void itemDropped (const SourceDetails& d) override
    {
        const auto index = slotAt (d.localPosition.y);
        dropSlot = -1;
        repaint();
        if (auto item = BrowserItem::fromDragDescription (d.description))
            insertEffect (*item, index); // on an existing slot: replace it
    }

private:
    juce::Rectangle<int> slotArea (int i) const
    {
        return getLocalBounds().withTrimmedTop (titleHeight).removeFromTop (slotHeight * (i + 1)).removeFromBottom (slotHeight).reduced (2, 1);
    }
    juce::Rectangle<int> powerArea (int i) const { auto s = slotArea (i); return s.removeFromLeft (s.getHeight() + 2); }
    juce::Rectangle<int> openArea (int i) const  { auto s = slotArea (i); return s.removeFromRight (s.getHeight() + 2); }

    int slotAt (int y) const
    {
        const auto i = (y - titleHeight) / slotHeight;
        return y < titleHeight || i >= numSlots ? -1 : i;
    }

    int slotIndexOf (ObjectId id) const
    {
        if (auto* t = ctx.song.findTrack (trackId))
            for (size_t i = 0; i < t->inserts.size(); ++i)
                if (t->inserts[i].id == id)
                    return (int) i;
        return -1;
    }

    AppContext& ctx;
    ObjectId trackId;
    int numSlots, slotHeight;
    int dropSlot = -1;
};

/** Sends area: arrives together with buses (stage 5). */
static void drawSendsPlaceholder (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (colours::textDim);
    g.setFont (font (10.5f, true));
    g.drawText ("SENDS", r.removeFromTop (14), juce::Justification::centredLeft);
    g.setColour (colours::panelDark);
    g.fillRoundedRectangle (r.toFloat(), 3.0f);
    g.setColour (colours::textDim.withAlpha (0.6f));
    g.setFont (font (10.5f));
    g.drawFittedText ("folgt mit Bussen", r.reduced (2), juce::Justification::centred, 2);
}



//==============================================================================
/** One mixer channel: inserts, sends, pan, mute/solo, fader. Used by the
    console (one per track) and by the inspector (for the selected track). */
class ChannelStrip final : public juce::Component
{
public:
    ChannelStrip (AppContext& c, ObjectId id, int insertSlots = 3, bool showNameBar = true, int insertRowHeight = 18)
        : ctx (c), trackId (id), inserts (c, id, insertSlots, insertRowHeight), nameBar (showNameBar)
    {
        setupVolumeSlider (fader);
        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setupFaderFeel (fader);
        bindTrackSlider (fader, ctx, [this] { return trackId; }, Param::volumeDb, true);
        addAndMakeVisible (fader);

        setupPanSlider (panKnob);
        panKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        panKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        panKnob.setTooltip ("Panorama (Doppelklick: Mitte)");
        bindTrackSlider (panKnob, ctx, [this] { return trackId; }, Param::pan, false);
        addAndMakeVisible (panKnob);

        for (auto* b : { &mute, &solo })
            addAndMakeVisible (*b);
        bindToggle (mute, ctx, [this] { return trackId; }, Param::mute);
        bindToggle (solo, ctx, [this] { return trackId; }, Param::solo);

        for (auto* l : { &value, &panValue })
        {
            makeValueLabel (*l, 11.5f);
            l->setJustificationType (juce::Justification::centred);
            addAndMakeVisible (*l);
        }
        bindValueLabel (value, ctx, [this] { return trackId; }, Param::volumeDb);
        bindValueLabel (panValue, ctx, [this] { return trackId; }, Param::pan);

        addAndMakeVisible (inserts);

        update();
    }

    ObjectId getTrackId() const noexcept { return trackId; }

    void setTrack (ObjectId id)
    {
        trackId = id;
        inserts.setTrack (id);
        update();
    }

    void update()
    {
        if (auto* t = ctx.song.findTrack (trackId))
        {
            if (! fader.isMouseButtonDown())
                fader.setValue (juce::jmax (-60.0, (double) t->volumeDb), juce::dontSendNotification);
            if (! panKnob.isMouseButtonDown())
                panKnob.setValue (t->pan, juce::dontSendNotification);
            mute.setToggleState (t->mute, juce::dontSendNotification);
            solo.setToggleState (t->solo, juce::dontSendNotification);
            if (! value.isBeingEdited())
                value.setText (formatDb (t->volumeDb), juce::dontSendNotification);
            if (! panValue.isBeingEdited())
                panValue.setText (formatPan (t->pan), juce::dontSendNotification);
            name = t->name;
            colour = t->colour;
        }
        inserts.repaint();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const bool selected = nameBar && ctx.selection.getSelectedTrack() == trackId;
        g.setColour (selected ? colours::laneSelected.brighter (0.05f) : colours::panelLight);
        g.fillRect (getLocalBounds().reduced (1, 0));

        if (! sendsArea.isEmpty())
            drawSendsPlaceholder (g, sendsArea);

        if (nameBar)
        {
            auto nameArea = getLocalBounds().removeFromBottom (24).reduced (1, 0);
            g.setColour (colour);
            g.fillRect (nameArea);
            g.setColour (colours::textDark);
            g.setFont (font (12.0f, true));
            g.drawFittedText (name, nameArea.reduced (3, 0), juce::Justification::centred, 1, 0.8f);

            g.setColour (colours::border);
            g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 6);
        if (nameBar)
            r.removeFromBottom (24);

        // inserts and sends only when there is enough height for a usable fader
        const bool roomy = r.getHeight() > 300;
        inserts.setVisible (roomy);
        sendsArea = {};
        if (roomy)
        {
            inserts.setBounds (r.removeFromTop (inserts.preferredHeight()));
            r.removeFromTop (4);
            sendsArea = r.removeFromTop (14 + 20);
            r.removeFromTop (6);
        }

        // pan knob with its value next to it
        auto panRow = r.removeFromTop (30);
        panKnob.setBounds (panRow.removeFromLeft (30));
        panValue.setBounds (panRow.withSizeKeepingCentre (juce::jmin (70, panRow.getWidth() - 4), 17));
        r.removeFromTop (4);

        auto buttons = r.removeFromTop (20);
        if (buttons.getWidth() > 100)
            buttons = buttons.withSizeKeepingCentre (100, buttons.getHeight());
        mute.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 1));
        buttons.removeFromLeft (2);
        solo.setBounds (buttons);
        value.setBounds (r.removeFromBottom (18).withSizeKeepingCentre (68, 17));
        r.removeFromBottom (3);
        fader.setBounds (r.reduced (0, 2).withSizeKeepingCentre (juce::jmin (r.getWidth(), 90), r.getHeight() - 4));
    }

    void mouseDown (const juce::MouseEvent&) override { ctx.selection.selectTrack (trackId); }

private:
    AppContext& ctx;
    ObjectId trackId;
    juce::Slider fader, panKnob;
    StateButton mute { "M", colours::mute, "Stumm" }, solo { "S", colours::solo, "Solo" };
    juce::Label value, panValue;
    InsertsBox inserts;
    bool nameBar;
    juce::Rectangle<int> sendsArea;
    juce::String name;
    juce::Colour colour;
};


//==============================================================================
// Inspector
//==============================================================================
InspectorPanel::InspectorPanel (AppContext& c) : ctx (c)
{
    nameLabel.setFont (font (15.0f, true));
    nameLabel.setEditable (false, true, false);
    nameLabel.setTooltip ("Doppelklick zum Umbenennen");
    nameLabel.onTextChange = [this]
    {
        ctx.perform (std::make_unique<commands::RenameTrack> (shownTrack, nameLabel.getText().trim()));
    };

    for (auto* l : { &typeLabel, &instrumentLabel })
    {
        l->setFont (font (12.5f));
        l->setColour (juce::Label::textColourId, colours::textDim);
    }
    instrumentLabel.setColour (juce::Label::textColourId, colours::text);

    openButton.setTooltip (u8"Instrument-Fenster öffnen");
    openButton.onClick = [this]
    {
        if (auto* t = ctx.song.findTrack (shownTrack))
            if (! ctx.pluginWindows.open (t->id, t->instrumentPluginId, t->instrumentName + juce::String (u8" – ") + t->name))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, t->instrumentName,
                    ctx.plugins.getInstanceError (t->id).isNotEmpty() ? ctx.plugins.getInstanceError (t->id)
                                                                     : juce::String (u8"Das Instrument wird noch geladen."));
    };

    auto setupStepper = [this] (juce::Slider& s, double lo, double hi, Param p, const juce::String& tip)
    {
        s.setSliderStyle (juce::Slider::IncDecButtons);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 20);
        s.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
        s.setRange (lo, hi, 1.0);
        s.setTooltip (tip);
        s.onValueChange = [this, &s, p]
        {
            ctx.perform (std::make_unique<commands::SetTrackParam> (shownTrack, p, (float) s.getValue()), true);
        };
        s.onDragEnd = [this] { ctx.history.endGesture(); };
    };
    setupStepper (transpose, -48, 48, Param::transpose, u8"Transponieren in Halbtönen");
    setupStepper (velocity, -127, 127, Param::velocityOffset, u8"Anschlagstärke anheben/absenken");

    arpButton.setTooltip (u8"Arpeggiator (folgt)");
    arpButton.setEnabled (false);
    midiFxButton.setTooltip ("MIDI-Effekte (folgt)");
    midiFxButton.setEnabled (false);

    for (auto* l : { &transposeCaption, &velocityCaption, &arpCaption, &midiFxCaption })
    {
        l->setFont (font (12.5f));
        l->setColour (juce::Label::textColourId, colours::textDim);
    }
    transposeCaption.setText ("Transponieren", juce::dontSendNotification);
    velocityCaption.setText ("Anschlag", juce::dontSendNotification);
    arpCaption.setText ("Arpeggiator", juce::dontSendNotification);
    midiFxCaption.setText ("MIDI-FX", juce::dontSendNotification);

    emptyLabel.setText (u8"Keine Spur ausgewählt", juce::dontSendNotification);
    emptyLabel.setJustificationType (juce::Justification::centred);
    emptyLabel.setColour (juce::Label::textColourId, colours::textDim);

    strip = std::make_unique<ChannelStrip> (ctx, ObjectId {}, 6, false, 24);

    for (auto* comp : std::initializer_list<juce::Component*> { &nameLabel, &typeLabel, strip.get() })
        addChildComponent (comp);
    for (auto* comp : instrumentComponents())
        addChildComponent (comp);
    addAndMakeVisible (emptyLabel);

    ctx.song.addListener (this);
    ctx.selection.addListener (this);
    refresh();
}

InspectorPanel::~InspectorPanel()
{
    ctx.song.removeListener (this);
    ctx.selection.removeListener (this);
}

std::vector<juce::Component*> InspectorPanel::instrumentComponents()
{
    return { &instrumentLabel, &openButton, &transpose, &velocity, &arpButton, &midiFxButton,
             &transposeCaption, &velocityCaption, &arpCaption, &midiFxCaption };
}

void InspectorPanel::songChanged (const SongChange& c)
{
    if (c.what == SongChange::What::reset || c.id == shownTrack || c.what == SongChange::What::trackRemoved)
        refresh();
}

void InspectorPanel::selectionChanged() { refresh(); }

void InspectorPanel::refresh()
{
    auto* t = ctx.song.findTrack (ctx.selection.getSelectedTrack());
    const auto newTrack = t != nullptr ? t->id : ObjectId {};

    if (newTrack != shownTrack)
    {
        shownTrack = newTrack;
        strip->setTrack (shownTrack);
    }
    else
    {
        strip->update();
    }

    const bool has = t != nullptr;
    isInstrument = has && t->typeId == "instrument";

    nameLabel.setVisible (has);
    typeLabel.setVisible (has);
    strip->setVisible (has);
    emptyLabel.setVisible (! has);
    for (auto* comp : instrumentComponents())
        comp->setVisible (isInstrument);

    if (has)
    {
        nameLabel.setText (t->name, juce::dontSendNotification);
        const auto* type = ctx.registry.findTrackType (t->typeId);
        typeLabel.setText ((type != nullptr ? type->displayName : t->typeId) + "spur", juce::dontSendNotification);
        instrumentLabel.setText (t->instrumentName.isNotEmpty() ? t->instrumentName : juce::String ("Kein Instrument"),
                                 juce::dontSendNotification);
        openButton.setEnabled (t->instrumentPluginId.isNotEmpty());

        if (! transpose.isMouseButtonDown())
            transpose.setValue (t->transpose, juce::dontSendNotification);
        if (! velocity.isMouseButtonDown())
            velocity.setValue (t->velocityOffset, juce::dontSendNotification);
    }

    resized();
    repaint();
}

void InspectorPanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    drawPanelHeader (g, getLocalBounds().removeFromTop (panelHeaderHeight), "Inspector");

    if (auto* t = ctx.song.findTrack (shownTrack))
    {
        g.setColour (t->colour);
        g.fillRect (8, panelHeaderHeight + 10, 4, 40);

        if (isInstrument && ! instrumentArea.isEmpty())
        {
            g.setColour (colours::panelDark);
            g.fillRoundedRectangle (instrumentArea.toFloat(), 4.0f);
            g.setColour (colours::textDim);
            g.setFont (font (10.5f, true));
            g.drawText ("INSTRUMENT", instrumentArea.reduced (8, 4).removeFromTop (14), juce::Justification::centredLeft);
        }

        // separator above the channel strip
        g.setColour (colours::separator);
        g.drawHorizontalLine (strip->getY() - 4, 10.0f, (float) getWidth() - 10.0f);
    }

    g.setColour (colours::border);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
}

void InspectorPanel::resized()
{
    auto r = getLocalBounds().withTrimmedTop (panelHeaderHeight).reduced (8, 8);
    emptyLabel.setBounds (r.withHeight (60));

    auto top = r.removeFromTop (44).withTrimmedLeft (10);
    nameLabel.setBounds (top.removeFromTop (24));
    typeLabel.setBounds (top);
    r.removeFromTop (8);

    instrumentArea = {};
    if (isInstrument)
    {
        constexpr int rowH = 24;
        instrumentArea = r.removeFromTop (14 + 6 + rowH * 5 + 10);
        auto a = instrumentArea.reduced (8, 4);
        a.removeFromTop (16);

        auto row = a.removeFromTop (rowH);
        openButton.setBounds (row.removeFromRight (70).reduced (0, 1));
        instrumentLabel.setBounds (row);

        auto addRow = [&a] (juce::Label& caption, juce::Component& c)
        {
            a.removeFromTop (2);
            auto rr = a.removeFromTop (rowH - 2);
            caption.setBounds (rr.removeFromLeft (96));
            c.setBounds (rr);
        };
        addRow (transposeCaption, transpose);
        addRow (velocityCaption, velocity);
        addRow (arpCaption, arpButton);
        addRow (midiFxCaption, midiFxButton);
        r.removeFromTop (10);
    }

    r.removeFromTop (6);
    strip->setBounds (r);
}

//==============================================================================
// Browser
//==============================================================================
BrowserPanel::BrowserPanel (AppContext& c) : ctx (c)
{
    auto& sources = ctx.registry.getBrowserSources();

    for (size_t i = 0; i < sources.size(); ++i)
    {
        auto* b = tabs.add (new juce::TextButton (sources[i]->getTitle()));
        b->setTooltip (sources[i]->getTitle() + juce::String (u8" anzeigen"));
        b->setClickingTogglesState (false);
        b->setWantsKeyboardFocus (false);
        b->onClick = [this, i] { selectTab ((int) i); };
        addAndMakeVisible (b);

        if (sources[i]->getSortModes().size() > 0)
            sources[i]->setSortMode (ctx.settings.getIntValue ("browser." + sources[i]->getId() + ".sort", 0));

        sources[i]->onItemsChanged = [safe = juce::Component::SafePointer<BrowserPanel> (this)]
        {
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->reload(); });
        };
    }

    search.setTextToShowWhenEmpty ("Suchen", colours::textDim);
    search.setTooltip ("Liste filtern");
    search.setFont (font (13.0f));
    search.onTextChange = [this] { reload(); };
    addAndMakeVisible (search);

    list.setRowHeight (24);
    list.setColour (juce::ListBox::backgroundColourId, colours::panelDark);
    list.setMultipleSelectionEnabled (false);
    addAndMakeVisible (list);

    status.setFont (font (11.5f));
    status.setColour (juce::Label::textColourId, colours::textDim);
    status.setJustificationType (juce::Justification::centredLeft);
    status.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (status);

    hint.setText (u8"Ziehen oder Doppelklick: ins Arrangement", juce::dontSendNotification);
    hint.setFont (font (12.0f));
    hint.setColour (juce::Label::textColourId, colours::textDim);
    hint.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (hint);

    selectTab (juce::jlimit (0, juce::jmax (0, (int) sources.size() - 1), ctx.settings.getIntValue ("browser.tab", 0)));
}

BrowserPanel::~BrowserPanel()
{
    for (auto& s : ctx.registry.getBrowserSources())
        s->onItemsChanged = nullptr;
}

void BrowserPanel::selectTab (int index)
{
    currentTab = index;
    ctx.settings.setValue ("browser.tab", index);
    for (int i = 0; i < tabs.size(); ++i)
        tabs[i]->setToggleState (i == index, juce::dontSendNotification);

    // sources with their own view (e.g. the folder tree) replace the list
    if (activeView != nullptr)
        activeView->setVisible (false);
    activeView = nullptr;

    auto& sources = ctx.registry.getBrowserSources();
    if (index >= 0 && index < (int) sources.size())
    {
        auto it = customViews.find (index);
        if (it == customViews.end())
            it = customViews.emplace (index, sources[(size_t) index]->createView (ctx)).first;

        if (it->second != nullptr)
        {
            activeView = it->second.get();
            addAndMakeVisible (activeView);
        }
    }

    for (auto* c : std::initializer_list<juce::Component*> { &search, &list, &hint, &status })
        c->setVisible (activeView == nullptr);

    // sort / group selector ("Hersteller | Art | Favoriten")
    sortButtons.clear();
    if (activeView == nullptr && index >= 0 && index < (int) sources.size())
    {
        auto source = sources[(size_t) index];
        const auto modes = source->getSortModes();
        const auto current = ctx.settings.getIntValue ("browser." + source->getId() + ".sort", 0);
        for (int m = 0; m < modes.size(); ++m)
        {
            auto* b = sortButtons.add (new juce::TextButton (modes[m]));
            b->setTooltip (modes[m] == "Favoriten" ? juce::String (u8"Die am häufigsten benutzten Plug-ins zuerst")
                                                   : juce::String ("Nach ") + modes[m] + " gruppieren");
            b->setWantsKeyboardFocus (false);
            b->setToggleState (m == current, juce::dontSendNotification);
            b->setConnectedEdges ((m > 0 ? juce::Button::ConnectedOnLeft : 0) | (m < modes.size() - 1 ? juce::Button::ConnectedOnRight : 0));
            b->onClick = [this, m] { setSortMode (m); };
            addAndMakeVisible (b);
        }
    }

    reload();
}

void BrowserPanel::setSortMode (int mode)
{
    auto& sources = ctx.registry.getBrowserSources();
    if (currentTab < 0 || currentTab >= (int) sources.size())
        return;

    auto source = sources[(size_t) currentTab];
    source->setSortMode (mode);
    ctx.settings.setValue ("browser." + source->getId() + ".sort", mode);
    for (int i = 0; i < sortButtons.size(); ++i)
        sortButtons[i]->setToggleState (i == mode, juce::dontSendNotification);
    list.deselectAllRows();
    reload();
}

void BrowserPanel::reload()
{
    rows.clear();
    auto& sources = ctx.registry.getBrowserSources();

    // per-source buttons and status line
    sourceButtons.clear();
    status.setText ({}, juce::dontSendNotification);
    if (currentTab >= 0 && currentTab < (int) sources.size())
    {
        auto source = sources[(size_t) currentTab];
        status.setText (source->getStatusText(), juce::dontSendNotification);
        for (auto& def : source->getButtons())
        {
            auto* b = sourceButtons.add (new juce::TextButton (def.text));
            b->setTooltip (def.tooltip);
            b->setWantsKeyboardFocus (false);
            b->onClick = [fn = def.onClick] { if (fn) fn(); };
            addAndMakeVisible (b);
        }
    }
    resized();

    if (currentTab >= 0 && currentTab < (int) sources.size())
    {
        auto items = sources[(size_t) currentTab]->getItems();
        const auto filter = search.getText().trim();

        if (! sources[(size_t) currentTab]->keepsItemOrder())
            std::stable_sort (items.begin(), items.end(), [] (auto& a, auto& b) { return a.category.compareNatural (b.category) < 0; });

        juce::String lastCategory = "\x01";
        for (auto& item : items)
        {
            if (filter.isNotEmpty() && ! item.name.containsIgnoreCase (filter) && ! item.category.containsIgnoreCase (filter))
                continue;

            if (item.category != lastCategory)
            {
                rows.push_back ({ true, item.category, {} });
                lastCategory = item.category;
            }
            rows.push_back ({ false, item.name, item });
        }
    }

    list.updateContent();
    list.repaint();
}

int BrowserPanel::getNumRows() { return (int) rows.size(); }

void BrowserPanel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) rows.size())
        return;

    auto& r = rows[(size_t) row];

    if (r.isHeader)
    {
        g.setColour (colours::panelLight);
        g.fillRect (0, 0, w, h);
        g.setColour (colours::textDim);
        g.setFont (font (12.0f, true));
        g.drawText (r.text.isEmpty() ? juce::String ("Allgemein") : r.text, 8, 0, w - 16, h, juce::Justification::centredLeft, true);
        return;
    }

    if (selected)
    {
        g.setColour (colours::accentDim);
        g.fillRect (0, 0, w, h);
    }

    auto icon = r.item.kind == "instrument" ? icons::instrument()
              : r.item.kind == "audiofile"  ? icons::waveform()
                                            : icons::rangeTool();
    if (r.item.kind == "effect")
    {
        icon.clear();
        icon.addEllipse (2.0f, 2.0f, 6.0f, 6.0f);
    }

    g.setColour (colours::textDim);
    g.fillPath (icon, icon.getTransformToScaleToFit (juce::Rectangle<float> (14.0f, 5.0f, 14.0f, (float) h - 10.0f), true));
    g.setColour (colours::text);
    g.setFont (font (13.0f));
    g.drawText (r.text, 36, 0, w - 42, h, juce::Justification::centredLeft, true);
}

juce::var BrowserPanel::getDragSourceDescription (const juce::SparseSet<int>& selected)
{
    if (selected.size() == 1)
    {
        const auto row = selected[0];
        if (row >= 0 && row < (int) rows.size() && ! rows[(size_t) row].isHeader)
            return rows[(size_t) row].item.toDragDescription();
    }
    return {};
}

void BrowserPanel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    // double click: same as dropping onto the selected track (or a new track)
    if (row < 0 || row >= (int) rows.size() || rows[(size_t) row].isHeader)
        return;

    activateBrowserItem (ctx, rows[(size_t) row].item);
}

juce::String BrowserPanel::getTooltipForRow (int row)
{
    if (row < 0 || row >= (int) rows.size() || rows[(size_t) row].isHeader)
        return {};

    auto& item = rows[(size_t) row].item;
    if (item.kind == "audiofile")
        return item.id;

    auto& sources = ctx.registry.getBrowserSources();
    if (currentTab >= 0 && currentTab < (int) sources.size())
        return sources[(size_t) currentTab]->getItemTooltip (item);
    return {};
}

void BrowserPanel::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() || row < 0 || row >= (int) rows.size() || rows[(size_t) row].isHeader)
        return;

    auto& sources = ctx.registry.getBrowserSources();
    if (currentTab < 0 || currentTab >= (int) sources.size())
        return;

    const auto item = rows[(size_t) row].item;
    auto entries = sources[(size_t) currentTab]->getItemMenu (item);

    juce::PopupMenu m;
    m.addSectionHeader (item.name);
    m.addItem (1000, u8"Einfügen (wie Doppelklick)");
    if (! entries.empty())
        m.addSeparator();
    for (size_t i = 0; i < entries.size(); ++i)
        m.addItem ((int) i + 1, entries[i].text);

    // the actions may rebuild the list: run them after the menu has closed
    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                     [&c = ctx, item, entries] (int result)
                     {
                         if (result == 1000)
                             activateBrowserItem (c, item);
                         else if (result > 0 && result <= (int) entries.size() && entries[(size_t) result - 1].onClick)
                             entries[(size_t) result - 1].onClick();
                     });
}

void BrowserPanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    drawPanelHeader (g, getLocalBounds().removeFromTop (panelHeaderHeight), "Browser");
    g.setColour (colours::border);
    g.drawVerticalLine (0, 0.0f, (float) getHeight());
}

void BrowserPanel::resized()
{
    auto r = getLocalBounds().withTrimmedTop (panelHeaderHeight).reduced (8, 8);

    auto tabRow = r.removeFromTop (26);
    const auto tabW = tabs.isEmpty() ? 0 : tabRow.getWidth() / tabs.size();
    for (auto* t : tabs)
        t->setBounds (tabRow.removeFromLeft (tabW).reduced (1, 0));

    r.removeFromTop (8);

    if (activeView != nullptr)
    {
        activeView->setBounds (r);
        return;
    }

    search.setBounds (r.removeFromTop (26));
    r.removeFromTop (6);

    if (! sortButtons.isEmpty())
    {
        auto row = r.removeFromTop (24);
        const auto w = row.getWidth() / sortButtons.size();
        for (auto* b : sortButtons)
            b->setBounds (row.removeFromLeft (w));
        r.removeFromTop (6);
    }

    // buttons on the left, status text next to them
    if (! sourceButtons.isEmpty() || status.getText().isNotEmpty())
    {
        auto row = r.removeFromTop (30);
        for (auto* b : sourceButtons)
        {
            const auto w = b->getButtonText().length() <= 2 ? 28 : juce::jmin (110, row.getWidth());
            b->setBounds (row.removeFromLeft (w).withSizeKeepingCentre (w, 24));
            row.removeFromLeft (6);
        }
        status.setBounds (row);
        r.removeFromTop (4);
    }
    else
    {
        status.setBounds ({});
    }

    hint.setBounds (r.removeFromBottom (22));
    list.setBounds (r);
}

//==============================================================================
// Console
//==============================================================================
class ConsolePanel::MasterStrip final : public juce::Component
{
public:
    explicit MasterStrip (AppContext& c) : ctx (c)
    {
        setupVolumeSlider (fader);
        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setupFaderFeel (fader);
        fader.setValue (0.0, juce::dontSendNotification);
        fader.onValueChange = [this]
        {
            ctx.engine.setMasterGainDb (sliderToDb (fader.getValue()));
            value.setText (formatDb (sliderToDb (fader.getValue())), juce::dontSendNotification);
        };
        addAndMakeVisible (fader);

        makeValueLabel (value, 11.5f);
        value.setJustificationType (juce::Justification::centred);
        value.setText (formatDb (0.0f), juce::dontSendNotification);
        value.onTextChange = [this]
        {
            if (auto db = valueformat::parseDb (value.getText()))
                fader.setValue (juce::jmax (-60.0, (double) *db), juce::sendNotificationSync);
            value.setText (formatDb (sliderToDb (fader.getValue())), juce::dontSendNotification);
        };
        addAndMakeVisible (value);
    }

    void pushLevels (float left, float right)
    {
        auto decay = [] (float current, float peak) { return std::max (peak, current * 0.86f); };
        levels[0] = decay (levels[0], left);
        levels[1] = decay (levels[1], right);
        repaint (meterArea);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (colours::panelLight.darker (0.1f));
        g.fillRect (getLocalBounds());

        auto nameArea = getLocalBounds().removeFromBottom (24);
        g.setColour (colours::header);
        g.fillRect (nameArea);
        g.setColour (colours::text);
        g.setFont (font (12.0f, true));
        g.drawText ("Main", nameArea, juce::Justification::centred);

        // stereo meter
        g.setColour (colours::panelDark);
        g.fillRect (meterArea);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto bar = meterArea.toFloat().withWidth ((float) meterArea.getWidth() / 2.0f - 1.0f)
                                 .translated ((float) ch * ((float) meterArea.getWidth() / 2.0f + 1.0f), 0.0f);
            const auto db = juce::Decibels::gainToDecibels (levels[ch], -60.0f);
            const auto prop = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
            // flat segments: green up to -12 dB, yellow up to -3 dB, red above
            auto yFor = [&] (float dB) { return bar.getBottom() - bar.getHeight() * juce::jlimit (0.0f, 1.0f, (dB + 60.0f) / 66.0f); };
            const auto top = bar.getBottom() - bar.getHeight() * prop;
            const std::tuple<float, float, juce::Colour> zones[] = {
                { -60.0f, -12.0f, colours::play }, { -12.0f, -3.0f, colours::solo }, { -3.0f, 6.0f, colours::record }
            };
            for (auto& [from, to, colour] : zones)
            {
                const auto zoneTop = std::max (top, yFor (to)), zoneBottom = yFor (from);
                if (zoneTop < zoneBottom)
                {
                    g.setColour (colour);
                    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (bar.getX(), zoneTop, bar.getRight(), zoneBottom));
                }
            }
        }

        g.setColour (colours::border);
        g.drawVerticalLine (0, 0.0f, (float) getHeight());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 8);
        r.removeFromBottom (24);
        value.setBounds (r.removeFromBottom (18).withSizeKeepingCentre (68, 17));
        r.removeFromBottom (3);
        meterArea = r.removeFromRight (14).withTrimmedTop (40);
        r.removeFromRight (4);
        fader.setBounds (r.withTrimmedTop (36));
    }

private:
    AppContext& ctx;
    juce::Slider fader;
    juce::Label value;
    juce::Rectangle<int> meterArea;
    float levels[2] {};
};

ConsolePanel::ConsolePanel (AppContext& c) : ctx (c)
{
    viewport.setViewedComponent (&stripHolder, false);
    viewport.setScrollBarsShown (false, true);
    addAndMakeVisible (viewport);

    master = std::make_unique<MasterStrip> (ctx);
    addAndMakeVisible (*master);

    ctx.song.addListener (this);
    ctx.selection.addListener (this);
    rebuild();
    startTimerHz (30);
}

ConsolePanel::~ConsolePanel()
{
    ctx.song.removeListener (this);
    ctx.selection.removeListener (this);
}

void ConsolePanel::rebuild()
{
    strips.clear();
    for (auto& t : ctx.song.getTracks())
        stripHolder.addAndMakeVisible (strips.add (new ChannelStrip (ctx, t.id)));
    resized();
}

void ConsolePanel::songChanged (const SongChange& c)
{
    using W = SongChange::What;
    if (c.what == W::trackAdded || c.what == W::trackRemoved || c.what == W::trackMoved || c.what == W::reset)
        rebuild();
    else if (c.what == W::trackChanged)
        for (auto* s : strips)
            if (s->getTrackId() == c.id)
                s->update();
}

void ConsolePanel::selectionChanged()
{
    for (auto* s : strips)
        s->repaint();
}

void ConsolePanel::timerCallback()
{
    master->pushLevels (ctx.engine.getAndResetPeak (0), ctx.engine.getAndResetPeak (1));
}

void ConsolePanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    drawPanelHeader (g, getLocalBounds().removeFromTop (panelHeaderHeight), "Konsole");

    if (strips.isEmpty())
    {
        g.setColour (colours::textDim);
        g.setFont (font (13.0f));
        g.drawText ("Noch keine Spuren", viewport.getBounds(), juce::Justification::centred);
    }

    g.setColour (colours::border);
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
}

void ConsolePanel::resized()
{
    auto r = getLocalBounds().withTrimmedTop (panelHeaderHeight);
    master->setBounds (r.removeFromRight (104));
    viewport.setBounds (r);

    constexpr int stripWidth = 90;
    const auto h = r.getHeight() - (viewport.isHorizontalScrollBarShown() ? viewport.getScrollBarThickness() : 0);
    stripHolder.setSize (juce::jmax (r.getWidth(), strips.size() * stripWidth), juce::jmax (0, h));
    for (int i = 0; i < strips.size(); ++i)
        strips[i]->setBounds (i * stripWidth, 0, stripWidth, stripHolder.getHeight());
}

} // namespace sms
