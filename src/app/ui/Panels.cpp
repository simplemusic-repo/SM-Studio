// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Panels.h"

#include "core/Commands.h"
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
}

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
    makeValueLabel (volumeValue);
    makeValueLabel (panValue);
    volumeValue.setJustificationType (juce::Justification::centred);
    panValue.setJustificationType (juce::Justification::centred);
    bindValueLabel (volumeValue, ctx, [this] { return shownTrack; }, Param::volumeDb);
    bindValueLabel (panValue, ctx, [this] { return shownTrack; }, Param::pan);

    volumeCaption.setText (u8"Lautstärke", juce::dontSendNotification);
    panCaption.setText ("Panorama", juce::dontSendNotification);
    for (auto* l : { &volumeCaption, &panCaption })
        l->setFont (font (12.5f));

    insertsTitle.setText ("Inserts", juce::dontSendNotification);
    insertsTitle.setFont (font (13.0f, true));

    emptyLabel.setText (u8"Keine Spur ausgewählt", juce::dontSendNotification);
    emptyLabel.setJustificationType (juce::Justification::centred);
    emptyLabel.setColour (juce::Label::textColourId, colours::textDim);

    setupVolumeSlider (volume);
    setupPanSlider (pan);
    bindTrackSlider (volume, ctx, [this] { return shownTrack; }, Param::volumeDb, true);
    bindTrackSlider (pan, ctx, [this] { return shownTrack; }, Param::pan, false);

    bindToggle (muteButton, ctx, [this] { return shownTrack; }, Param::mute);
    bindToggle (soloButton, ctx, [this] { return shownTrack; }, Param::solo);

    for (auto* comp : std::initializer_list<juce::Component*> { &nameLabel, &typeLabel, &instrumentLabel, &volume, &volumeValue, &volumeCaption, &panCaption,
                                                                &pan, &panValue, &muteButton, &soloButton, &insertsTitle })
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

void InspectorPanel::songChanged (const SongChange& c)
{
    if (c.what == SongChange::What::reset || c.id == shownTrack || c.what == SongChange::What::trackRemoved)
        refresh();
}

void InspectorPanel::selectionChanged() { refresh(); }

void InspectorPanel::refresh()
{
    auto* t = ctx.song.findTrack (ctx.selection.getSelectedTrack());
    shownTrack = t != nullptr ? t->id : ObjectId {};

    const bool has = t != nullptr;
    for (auto* comp : std::initializer_list<juce::Component*> { &nameLabel, &typeLabel, &volume, &volumeValue, &volumeCaption, &panCaption,
                                                                &pan, &panValue, &muteButton, &soloButton, &insertsTitle })
        comp->setVisible (has);
    emptyLabel.setVisible (! has);

    if (has)
    {
        nameLabel.setText (t->name, juce::dontSendNotification);
        const auto* type = ctx.registry.findTrackType (t->typeId);
        typeLabel.setText ((type != nullptr ? type->displayName : t->typeId) + "spur", juce::dontSendNotification);

        const bool isInstrument = t->typeId == "instrument";
        instrumentLabel.setVisible (isInstrument);
        instrumentLabel.setText ("Instrument: " + (t->instrumentName.isNotEmpty() ? t->instrumentName : juce::String (u8"–")),
                                 juce::dontSendNotification);

        if (! volume.isMouseButtonDown())
            volume.setValue (juce::jmax (-60.0, (double) t->volumeDb), juce::dontSendNotification);
        if (! pan.isMouseButtonDown())
            pan.setValue (t->pan, juce::dontSendNotification);

        if (! volumeValue.isBeingEdited())
            volumeValue.setText (formatDb (t->volumeDb), juce::dontSendNotification);
        if (! panValue.isBeingEdited())
            panValue.setText (formatPan (t->pan), juce::dontSendNotification);
        muteButton.setToggleState (t->mute, juce::dontSendNotification);
        soloButton.setToggleState (t->solo, juce::dontSendNotification);
    }
    else
    {
        instrumentLabel.setVisible (false);
    }

    rebuildInserts();
    resized();
    repaint();
}

void InspectorPanel::rebuildInserts()
{
    insertRows.clear();

    auto* t = ctx.song.findTrack (shownTrack);
    if (t == nullptr)
        return;

    struct InsertRow final : public juce::Component
    {
        InsertRow (AppContext& c, ObjectId track, const InsertSlot& slot) : ctx (c), trackId (track), slotId (slot.id)
        {
            label.setText (slot.name, juce::dontSendNotification);
            label.setFont (font (12.5f));
            addAndMakeVisible (label);

            remove.setTooltip ("Effekt entfernen");
            remove.onClick = [this] { ctx.perform (std::make_unique<commands::RemoveInsert> (trackId, slotId)); };
            addAndMakeVisible (remove);
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (colours::panelDark);
            g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.0f);
            g.setColour (colours::accent);
            g.fillRect (4, 6, 3, getHeight() - 12);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (2);
            remove.setBounds (r.removeFromRight (r.getHeight()));
            label.setBounds (r.withTrimmedLeft (8));
        }

        AppContext& ctx;
        ObjectId trackId, slotId;
        juce::Label label;
        juce::TextButton remove { juce::String::fromUTF8 ("\xc3\x97") }; // ×
    };

    for (auto& slot : t->inserts)
        addAndMakeVisible (insertRows.add (new InsertRow (ctx, t->id, slot)));
}

void InspectorPanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    drawPanelHeader (g, getLocalBounds().removeFromTop (panelHeaderHeight), "Inspector");

    if (auto* t = ctx.song.findTrack (shownTrack))
    {
        g.setColour (t->colour);
        g.fillRect (8, panelHeaderHeight + 10, 4, 40);

        if (t->inserts.empty())
        {
            g.setColour (colours::textDim);
            g.setFont (font (12.0f));
            g.drawFittedText ("Effekte aus dem Browser auf die Spur ziehen",
                              juce::Rectangle<int> (12, insertsTitle.getBottom() + 2, getWidth() - 24, 34),
                              juce::Justification::topLeft, 2);
        }
    }

    g.setColour (colours::border);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
}

void InspectorPanel::resized()
{
    auto r = getLocalBounds().withTrimmedTop (panelHeaderHeight).reduced (10, 8);
    emptyLabel.setBounds (r.withHeight (60));

    auto top = r.removeFromTop (44).withTrimmedLeft (8);
    nameLabel.setBounds (top.removeFromTop (24));
    typeLabel.setBounds (top);

    if (instrumentLabel.isVisible())
        instrumentLabel.setBounds (r.removeFromTop (22));

    r.removeFromTop (10);
    auto volRow = r.removeFromTop (18);
    volumeValue.setBounds (volRow.removeFromRight (72).reduced (0, 0));
    volumeCaption.setBounds (volRow);
    volume.setBounds (r.removeFromTop (20));

    r.removeFromTop (8);
    auto panRow = r.removeFromTop (18);
    panValue.setBounds (panRow.removeFromRight (72));
    panCaption.setBounds (panRow);
    pan.setBounds (r.removeFromTop (20));

    r.removeFromTop (10);
    auto buttons = r.removeFromTop (26);
    muteButton.setBounds (buttons.removeFromLeft (34));
    buttons.removeFromLeft (6);
    soloButton.setBounds (buttons.removeFromLeft (34));

    r.removeFromTop (16);
    insertsTitle.setBounds (r.removeFromTop (20));
    r.removeFromTop (4);

    for (auto* row : insertRows)
    {
        row->setBounds (r.removeFromTop (24));
        r.removeFromTop (3);
    }

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
        b->setClickingTogglesState (false);
        b->setWantsKeyboardFocus (false);
        b->onClick = [this, i] { selectTab ((int) i); };
        addAndMakeVisible (b);

        sources[i]->onItemsChanged = [safe = juce::Component::SafePointer<BrowserPanel> (this)]
        {
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->reload(); });
        };
    }

    search.setTextToShowWhenEmpty ("Suchen", colours::textDim);
    search.setFont (font (13.0f));
    search.onTextChange = [this] { reload(); };
    addAndMakeVisible (search);

    list.setRowHeight (24);
    list.setColour (juce::ListBox::backgroundColourId, colours::panelDark);
    list.setMultipleSelectionEnabled (false);
    addAndMakeVisible (list);

    hint.setText (u8"Einträge auf das Arrangement ziehen", juce::dontSendNotification);
    hint.setFont (font (12.0f));
    hint.setColour (juce::Label::textColourId, colours::textDim);
    hint.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (hint);

    selectTab (0);
}

BrowserPanel::~BrowserPanel()
{
    for (auto& s : ctx.registry.getBrowserSources())
        s->onItemsChanged = nullptr;
}

void BrowserPanel::selectTab (int index)
{
    currentTab = index;
    for (int i = 0; i < tabs.size(); ++i)
        tabs[i]->setToggleState (i == index, juce::dontSendNotification);
    reload();
}

void BrowserPanel::reload()
{
    rows.clear();
    auto& sources = ctx.registry.getBrowserSources();

    if (currentTab >= 0 && currentTab < (int) sources.size())
    {
        auto items = sources[(size_t) currentTab]->getItems();
        const auto filter = search.getText().trim();

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

    auto& item = rows[(size_t) row].item;
    if (auto* handler = ctx.registry.findDropHandler (item.kind))
    {
        DropTarget target;
        target.trackId = ctx.selection.getSelectedTrack();
        const auto idx = ctx.song.indexOfTrack (target.trackId);
        target.insertIndex = idx >= 0 ? idx + 1 : ctx.song.getNumTracks();
        target.position = ctx.edit.snapTick (ctx.song.getTempoMap().secondsToTicks (ctx.engine.getPositionSeconds()),
                                             ctx.song.getTempoMap());
        handler->drop (ctx, item, target);
    }
}

juce::String BrowserPanel::getTooltipForRow (int row)
{
    if (row >= 0 && row < (int) rows.size() && rows[(size_t) row].item.kind == "audiofile")
        return rows[(size_t) row].item.id;
    return {};
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
    search.setBounds (r.removeFromTop (26));
    r.removeFromTop (8);
    hint.setBounds (r.removeFromBottom (22));
    list.setBounds (r);
}

//==============================================================================
// Console
//==============================================================================
class ConsolePanel::Strip final : public juce::Component
{
public:
    Strip (AppContext& c, ObjectId id) : ctx (c), trackId (id)
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

        update();
    }

    ObjectId getTrackId() const noexcept { return trackId; }

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
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const bool selected = ctx.selection.getSelectedTrack() == trackId;
        g.setColour (selected ? colours::laneSelected.brighter (0.05f) : colours::panelLight);
        g.fillRect (getLocalBounds().reduced (1, 0));

        auto nameArea = getLocalBounds().removeFromBottom (24).reduced (1, 0);
        g.setColour (colour);
        g.fillRect (nameArea);
        g.setColour (colours::textDark);
        g.setFont (font (12.0f, true));
        g.drawFittedText (name, nameArea.reduced (3, 0), juce::Justification::centred, 1, 0.8f);

        g.setColour (colours::border);
        g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 6);
        r.removeFromBottom (24);
        panKnob.setBounds (r.removeFromTop (34).withSizeKeepingCentre (32, 32));
        panValue.setBounds (r.removeFromTop (18).withSizeKeepingCentre (52, 17));
        r.removeFromTop (5);
        auto buttons = r.removeFromTop (20);
        mute.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 1));
        buttons.removeFromLeft (2);
        solo.setBounds (buttons);
        value.setBounds (r.removeFromBottom (18).withSizeKeepingCentre (68, 17));
        r.removeFromBottom (3);
        fader.setBounds (r.reduced (0, 2));
    }

    void mouseDown (const juce::MouseEvent&) override { ctx.selection.selectTrack (trackId); }

private:
    AppContext& ctx;
    ObjectId trackId;
    juce::Slider fader, panKnob;
    StateButton mute { "M", colours::mute, "Stumm" }, solo { "S", colours::solo, "Solo" };
    juce::Label value, panValue;
    juce::String name;
    juce::Colour colour;
};

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
            auto fill = bar.withTop (bar.getBottom() - bar.getHeight() * prop);
            g.setGradientFill (juce::ColourGradient (colours::play, 0, bar.getBottom(), colours::record, 0, bar.getY(), false));
            g.fillRect (fill);
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
        stripHolder.addAndMakeVisible (strips.add (new Strip (ctx, t.id)));
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
