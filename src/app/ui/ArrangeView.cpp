// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ArrangeView.h"

#include "SongHelpers.h"
#include "core/Commands.h"
#include "core/RegionOps.h"
#include "extension/ExtensionRegistry.h"

#include <map>

namespace sms
{

using namespace theme;
using Param = commands::SetTrackParam::Param;

namespace
{
    constexpr int edgeZone = 6;          // px at event edges for resizing
    constexpr int regionTitleHeight = 16;

    bool regionFitsTrack (const Region& r, const Track& t)
    {
        return r.kind == Region::Kind::audio ? t.typeId == "audio" : t.typeId == "instrument";
    }
}

//==============================================================================
class TrackHeader final : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    TrackHeader (AppContext& c, ObjectId id) : ctx (c), trackId (id)
    {
        name.setFont (font (13.5f, true));
        name.setEditable (false, true, false);
        name.setInterceptsMouseClicks (false, false);
        name.onTextChange = [this]
        {
            ctx.perform (std::make_unique<commands::RenameTrack> (trackId, name.getText().trim()));
        };
        addAndMakeVisible (name);

        for (auto* b : { &muteButton, &soloButton, &recButton })
        {
            b->setClickingTogglesState (false);
            addAndMakeVisible (*b);
        }

        muteButton.onClick = [this] { toggle (Param::mute); };
        soloButton.onClick = [this] { toggle (Param::solo); };
        recButton.onClick  = [this] { toggle (Param::armed); };

        setTooltip (u8"Ziehen zum Umsortieren · Doppelklick auf den Namen zum Umbenennen");
        update();
    }

    void update()
    {
        if (auto* t = ctx.song.findTrack (trackId))
        {
            name.setText (t->name, juce::dontSendNotification);
            muteButton.setToggleState (t->mute, juce::dontSendNotification);
            soloButton.setToggleState (t->solo, juce::dontSendNotification);
            recButton.setToggleState (t->armed, juce::dontSendNotification);
            colour = t->colour;
            isInstrument = t->typeId == "instrument";
            subtitle = isInstrument ? (t->instrumentName.isNotEmpty() ? t->instrumentName : juce::String ("Kein Instrument"))
                                    : juce::String ("Audio");
        }
        repaint();
    }

    void setIndex (int i) { index = i; repaint(); }
    ObjectId getTrackId() const noexcept { return trackId; }

    void paint (juce::Graphics& g) override
    {
        const bool selected = ctx.selection.getSelectedTrack() == trackId;
        auto r = getLocalBounds();

        g.setColour (selected ? colours::laneSelected.brighter (0.08f) : (index % 2 == 0 ? colours::panelLight : colours::panel));
        g.fillRect (r);

        auto strip = r.removeFromLeft (22);
        g.setColour (colour.withMultipliedSaturation (selected ? 1.0f : 0.8f));
        g.fillRect (strip);
        g.setColour (colours::textDark);
        g.setFont (font (11.5f, true));
        g.drawText (juce::String (index + 1), strip.removeFromTop (22), juce::Justification::centred);

        auto iconArea = juce::Rectangle<float> (30.0f, 34.0f, 16.0f, 16.0f);
        auto icon = isInstrument ? icons::instrument() : icons::waveform();
        g.setColour (colours::textDim);
        g.fillPath (icon, icon.getTransformToScaleToFit (iconArea, true));

        g.setFont (font (12.0f));
        g.drawText (subtitle, 52, 34, getWidth() - 140, 16, juce::Justification::centredLeft, true);

        if (selected)
        {
            g.setColour (colours::accent);
            g.fillRect (getWidth() - 3, 0, 3, getHeight());
        }

        g.setColour (colours::border);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
        g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
    }

    void resized() override
    {
        name.setBounds (26, 6, getWidth() - 110, 22);
        auto buttons = juce::Rectangle<int> (getWidth() - 82, 8, 72, 18);
        muteButton.setBounds (buttons.removeFromLeft (22));
        buttons.removeFromLeft (3);
        soloButton.setBounds (buttons.removeFromLeft (22));
        buttons.removeFromLeft (3);
        recButton.setBounds (buttons.removeFromLeft (22));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        draggingTrack = false;
        ctx.selection.selectTrack (trackId);
        if (e.mods.isPopupMenu())
            if (auto* arrange = findParentComponentOfClass<ArrangeView>())
                arrange->mouseDown (e.getEventRelativeTo (arrange));
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            return;

        if (! draggingTrack && std::abs (e.getDistanceFromDragStartY()) > 6)
            draggingTrack = true;

        if (draggingTrack)
            if (auto* arrange = findParentComponentOfClass<ArrangeView>())
                arrange->trackDragged (trackId, e.getEventRelativeTo (arrange).y);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const bool wasDragging = std::exchange (draggingTrack, false);

        // must be the last thing we do: moving the track rebuilds (deletes) the headers
        if (wasDragging)
            if (auto* arrange = findParentComponentOfClass<ArrangeView>())
                arrange->trackDragEnded (trackId, e.getEventRelativeTo (arrange).y);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (name.getBounds().contains (e.getPosition()))
            name.showEditor();
    }

private:
    void toggle (Param p)
    {
        if (auto* t = ctx.song.findTrack (trackId))
            ctx.perform (std::make_unique<commands::SetTrackParam> (trackId, p, commands::SetTrackParam::get (*t, p) > 0.5f ? 0.0f : 1.0f));
    }

    AppContext& ctx;
    ObjectId trackId;
    int index = 0;
    juce::Colour colour;
    bool isInstrument = false;
    bool draggingTrack = false;
    juce::String subtitle;
    juce::Label name;
    StateButton muteButton { "M", colours::mute, "Stumm" },
                soloButton { "S", colours::solo, "Solo" },
                recButton  { "R", colours::record, "Aufnahme bereit" };
};

//==============================================================================
namespace
{
    struct ClipComponent final : public juce::Component
    {
        ClipComponent() { setInterceptsMouseClicks (true, true); }
        void paint (juce::Graphics& g) override { g.fillAll (colours::laneB); }
    };
}

//==============================================================================
/** Row of tool buttons shown at the top of the right-click menu (like the toolbar). */
class ToolMenuRow final : public juce::PopupMenu::CustomComponent
{
public:
    explicit ToolMenuRow (AppContext& c) : juce::PopupMenu::CustomComponent (false), ctx (c)
    {
        const std::pair<juce::String, juce::Path> defs[] = {
            { "Pfeil (1)", icons::arrowTool() }, { "Bereich (2)", icons::rangeTool() },
            { "Teilen (3)", icons::splitTool() }, { "Radiergummi (4)", icons::eraserTool() },
            { "Stift (5)", icons::pencilTool() }, { "Stummschalten (6)", icons::muteTool() },
        };

        const int current = ctx.edit.tool.getValue();
        for (int i = 0; i < (int) std::size (defs); ++i)
        {
            auto* b = buttons.add (new IconButton (defs[i].first, defs[i].second));
            b->setToggleState (i == current, juce::dontSendNotification);
            b->onClick = [this, i] { ctx.edit.tool = i; triggerMenuItem(); };
            addAndMakeVisible (b);
        }

        auto* undo = buttons.add (new IconButton (u8"Rückgängig", icons::undo()));
        undo->setEnabled (ctx.history.canUndo());
        undo->onClick = [this] { triggerMenuItem(); ctx.registry.invoke ("edit.undo"); };
        addAndMakeVisible (undo);

        auto* redo = buttons.add (new IconButton ("Wiederherstellen", icons::redo()));
        redo->setEnabled (ctx.history.canRedo());
        redo->onClick = [this] { triggerMenuItem(); ctx.registry.invoke ("edit.redo"); };
        addAndMakeVisible (redo);
    }

    void getIdealSize (int& w, int& h) override
    {
        w = buttons.size() * 30 + 12 + 14;
        h = 34;
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 3);
        for (int i = 0; i < buttons.size(); ++i)
        {
            if (i == 6)
                r.removeFromLeft (12); // gap before undo / redo
            buttons[i]->setBounds (r.removeFromLeft (30).reduced (1, 0));
        }
    }

private:
    AppContext& ctx;
    juce::OwnedArray<IconButton> buttons;
};

//==============================================================================
ArrangeView::ArrangeView (AppContext& c) : ctx (c)
{
    headerColumn = std::make_unique<ClipComponent>();
    addAndMakeVisible (*headerColumn);

    addTrackButton.setDrawBackground (false);
    addTrackButton.onClick = [this] { showAddTrackMenu (&addTrackButton); };
    addAndMakeVisible (addTrackButton);

    for (auto* s : { &hScroll, &vScroll })
    {
        s->addListener (this);
        s->setAutoHide (false);
        addAndMakeVisible (*s);
    }

    ctx.song.addListener (this);
    ctx.selection.addListener (this);
    ctx.pool.addChangeListener (this);
    ctx.edit.pixelsPerTick = pixelsPerTick;
    rebuildHeaders();
    startTimerHz (30);
}

ArrangeView::~ArrangeView()
{
    ctx.song.removeListener (this);
    ctx.selection.removeListener (this);
    ctx.pool.removeChangeListener (this);
}

//==============================================================================
juce::Rectangle<int> ArrangeView::getLaneArea() const
{
    return { headerWidth, rulerHeight, juce::jmax (0, getWidth() - headerWidth - vScroll.getWidth()),
             juce::jmax (0, getHeight() - rulerHeight - hScroll.getHeight()) };
}

juce::Rectangle<int> ArrangeView::getRulerArea() const
{
    return { headerWidth, 0, getLaneArea().getWidth(), rulerHeight };
}

double ArrangeView::tickToX (Tick t) const
{
    return (double) headerWidth + ((double) t - viewStartTick) * pixelsPerTick;
}

Tick ArrangeView::xToTick (double x) const
{
    return (Tick) std::llround (viewStartTick + (x - (double) headerWidth) / pixelsPerTick);
}

int ArrangeView::rowAtY (int y) const
{
    if (y < rulerHeight)
        return -1;
    return (y - rulerHeight + viewTop) / trackHeight;
}

int ArrangeView::rowTop (int row) const
{
    return rulerHeight + row * trackHeight - viewTop;
}

Tick ArrangeView::snap (Tick t) const
{
    return ctx.edit.snapTick (t, ctx.song.getTempoMap());
}

juce::Rectangle<float> ArrangeView::regionBounds (int row, const Region& r) const
{
    const auto x1 = tickToX (r.position), x2 = tickToX (r.getEnd());
    return { (float) x1, (float) rowTop (row) + 2.0f, (float) (x2 - x1), (float) trackHeight - 5.0f };
}

ArrangeView::Hit ArrangeView::hitTestEvents (juce::Point<int> p) const
{
    Hit h;
    if (! getLaneArea().contains (p))
        return h;

    h.row = rowAtY (p.y);
    const auto& tracks = ctx.song.getTracks();
    if (h.row < 0 || h.row >= (int) tracks.size())
        return h;

    const auto& track = tracks[(size_t) h.row];
    h.trackId = track.id;

    // topmost (last painted) event wins
    for (auto it = track.regions.rbegin(); it != track.regions.rend(); ++it)
    {
        const auto b = regionBounds (h.row, *it);
        if (! b.contains (p.toFloat()))
            continue;

        h.region = &*it;
        h.zone = Hit::body;

        if (b.getWidth() > edgeZone * 3)
        {
            if ((float) p.x < b.getX() + (float) edgeZone)       h.zone = Hit::leftEdge;
            else if ((float) p.x > b.getRight() - (float) edgeZone) h.zone = Hit::rightEdge;
        }
        break;
    }
    return h;
}

//==============================================================================
void ArrangeView::resized()
{
    const int sb = getLookAndFeel().getDefaultScrollbarWidth();
    vScroll.setBounds (getWidth() - sb, rulerHeight, sb, getHeight() - rulerHeight - sb);
    hScroll.setBounds (headerWidth, getHeight() - sb, getWidth() - headerWidth - sb, sb);
    headerColumn->setBounds (0, rulerHeight, headerWidth, getHeight() - rulerHeight - sb);
    addTrackButton.setBounds (6, 7, 20, 20);
    layoutHeaders();
    updateScrollBars();
}

void ArrangeView::rebuildHeaders()
{
    headers.clear();
    for (auto& t : ctx.song.getTracks())
    {
        auto h = std::make_unique<TrackHeader> (ctx, t.id);
        headerColumn->addAndMakeVisible (*h);
        headers.push_back (std::move (h));
    }
    layoutHeaders();
    updateScrollBars();
    repaint();
}

void ArrangeView::layoutHeaders()
{
    for (size_t i = 0; i < headers.size(); ++i)
    {
        headers[i]->setIndex ((int) i);
        headers[i]->setBounds (0, (int) i * trackHeight - viewTop, headerWidth, trackHeight);
    }
}

void ArrangeView::updateScrollBars()
{
    const auto lanes = getLaneArea();
    const auto contentHeight = (int) headers.size() * trackHeight + trackHeight * 2;
    vScroll.setRangeLimits (0, juce::jmax (contentHeight, lanes.getHeight()), juce::dontSendNotification);
    vScroll.setCurrentRange (viewTop, lanes.getHeight(), juce::dontSendNotification);

    const auto visibleTicks = (double) lanes.getWidth() / pixelsPerTick;
    const auto total = juce::jmax ((double) ctx.song.getSongEnd() + (double) ctx.song.getTempoMap().getTicksPerBar() * 16,
                                   viewStartTick + visibleTicks);
    hScroll.setRangeLimits (0, total, juce::dontSendNotification);
    hScroll.setCurrentRange (viewStartTick, visibleTicks, juce::dontSendNotification);
}

void ArrangeView::scrollBarMoved (juce::ScrollBar* bar, double start)
{
    if (bar == &vScroll)
    {
        viewTop = (int) start;
        layoutHeaders();
    }
    else
    {
        viewStartTick = start;
    }
    repaint();
}

void ArrangeView::zoomAround (double factor, int x)
{
    const auto anchorTick = (double) xToTick ((double) x);
    pixelsPerTick = juce::jlimit (2.0 / (double) ticksPerQuarter, 600.0 / (double) ticksPerQuarter, pixelsPerTick * factor);
    ctx.edit.pixelsPerTick = pixelsPerTick;
    viewStartTick = juce::jmax (0.0, anchorTick - ((double) x - headerWidth) / pixelsPerTick);
    updateScrollBars();
    repaint();
}

//==============================================================================
void ArrangeView::songChanged (const SongChange& c)
{
    using W = SongChange::What;

    switch (c.what)
    {
        case W::trackAdded:
        case W::trackRemoved:
        case W::trackMoved:
        case W::reset:
            rebuildHeaders();
            break;

        case W::trackChanged:
            for (auto& h : headers)
                if (h->getTrackId() == c.id)
                    h->update();
            repaint();
            break;

        case W::regionsChanged:
        case W::tempoChanged:
            updateScrollBars();
            repaint();
            break;

        case W::loopChanged:
            repaint();
            break;

        case W::sourcesChanged:
            break;
    }
}

void ArrangeView::selectionChanged()
{
    for (auto& h : headers)
        h->repaint();
    repaint();
}

void ArrangeView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint(); // waveforms finished loading
}

void ArrangeView::timerCallback()
{
    const auto seconds = ctx.engine.getPositionSeconds();
    const auto x = tickToX (ctx.song.getTempoMap().secondsToTicks (seconds));

    if (! juce::approximatelyEqual (x, lastPlayheadX))
    {
        const auto lanes = getLaneArea();
        const auto& loop = ctx.song.getLoop();
        const bool loopVisible = loop.enabled && loop.isValid() && tickToX (loop.end) <= lanes.getRight();

        // follow the playhead page by page while playing (not while looping inside the view)
        if (ctx.engine.isPlaying() && x > lanes.getRight() - 20 && ! loopVisible && gesture == Gesture::none)
        {
            viewStartTick += (double) lanes.getWidth() * 0.8 / pixelsPerTick;
            updateScrollBars();
            repaint();
        }
        else
        {
            const auto h = getHeight() - hScroll.getHeight();
            repaint ((int) lastPlayheadX - 7, 0, 14, h);
            repaint ((int) x - 7, 0, 14, h);
        }
        lastPlayheadX = x;
    }
}

//==============================================================================
void ArrangeView::paint (juce::Graphics& g)
{
    g.fillAll (colours::laneB);

    auto corner = juce::Rectangle<int> (0, 0, headerWidth, rulerHeight);
    g.setColour (colours::header);
    g.fillRect (corner);
    g.setColour (colours::textDim);
    g.setFont (font (12.0f));
    g.drawText (juce::String (ctx.song.getNumTracks()) + (ctx.song.getNumTracks() == 1 ? " Spur" : " Spuren"),
                corner.withTrimmedLeft (32), juce::Justification::centredLeft);
    g.setColour (colours::border);
    g.drawHorizontalLine (rulerHeight - 1, 0.0f, (float) headerWidth);

    paintRuler (g, getRulerArea());
    paintLanes (g, getLaneArea());

    g.setColour (colours::panelDark);
    g.fillRect (getWidth() - vScroll.getWidth(), getHeight() - hScroll.getHeight(), vScroll.getWidth(), hScroll.getHeight());
    g.fillRect (0, getHeight() - hScroll.getHeight(), headerWidth, hScroll.getHeight());
}

void ArrangeView::paintRuler (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.saveState();
    g.reduceClipRegion (area);

    g.setGradientFill (juce::ColourGradient (colours::header, 0, 0, colours::panelLight, 0, (float) area.getBottom(), false));
    g.fillRect (area);

    // loop range in the upper half
    const auto loopBand = area.withHeight (area.getHeight() / 2 - 2).withTrimmedTop (2);
    g.setColour (colours::panelDark.withAlpha (0.5f));
    g.fillRect (loopBand);

    const auto& loop = ctx.song.getLoop();
    if (loop.isValid())
    {
        const auto x1 = (float) tickToX (loop.start), x2 = (float) tickToX (loop.end);
        auto band = juce::Rectangle<float> (x1, (float) loopBand.getY(), x2 - x1, (float) loopBand.getHeight());
        g.setColour (loop.enabled ? colours::accent : colours::textDim.withAlpha (0.5f));
        g.fillRoundedRectangle (band, 2.0f);
    }

    const auto& map = ctx.song.getTempoMap();
    const auto perBar = map.getTicksPerBar();
    const auto perBeat = map.getTicksPerBeat();
    const auto barPx = (double) perBar * pixelsPerTick;
    const int labelEvery = barPx >= 60 ? 1 : barPx >= 30 ? 2 : barPx >= 12 ? 4 : 8;
    const auto numbersTop = area.getY() + area.getHeight() / 2;

    const auto firstBar = (Tick) std::floor (viewStartTick / (double) perBar);
    g.setFont (font (11.5f));

    for (Tick bar = firstBar;; ++bar)
    {
        const auto x = (float) tickToX (bar * perBar);
        if (x > (float) area.getRight())
            break;

        if (bar % labelEvery == 0)
        {
            g.setColour (colours::textDim);
            g.drawVerticalLine ((int) x, (float) numbersTop, (float) area.getBottom());
            g.setColour (colours::text);
            g.drawText (juce::String (bar + 1), (int) x + 3, numbersTop, 40, 14, juce::Justification::centredLeft);
        }

        if (barPx >= 80)
        {
            g.setColour (colours::textDim.withAlpha (0.5f));
            for (int b = 1; b < map.getTimeSignature().numerator; ++b)
            {
                const auto bx = tickToX (bar * perBar + b * perBeat);
                g.drawVerticalLine ((int) bx, (float) area.getBottom() - 6.0f, (float) area.getBottom());
            }
        }
    }

    g.setColour (colours::border);
    g.drawHorizontalLine (area.getBottom() - 1, (float) area.getX(), (float) area.getRight());
    g.restoreState();
}

void ArrangeView::paintLanes (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.saveState();
    g.reduceClipRegion (area);

    const auto& tracks = ctx.song.getTracks();
    const auto selectedTrack = ctx.selection.getSelectedTrack();

    for (size_t i = 0; i < tracks.size(); ++i)
    {
        auto lane = juce::Rectangle<int> (area.getX(), rowTop ((int) i), area.getWidth(), trackHeight);
        if (! lane.intersects (area))
            continue;

        g.setColour (tracks[i].id == selectedTrack ? colours::laneSelected : (i % 2 == 0 ? colours::laneA : colours::laneB));
        g.fillRect (lane);
    }

    // loop range tint
    const auto& loop = ctx.song.getLoop();
    if (loop.enabled && loop.isValid())
    {
        const auto x1 = (float) tickToX (loop.start), x2 = (float) tickToX (loop.end);
        g.setColour (colours::accent.withAlpha (0.06f));
        g.fillRect (juce::Rectangle<float> (x1, (float) area.getY(), x2 - x1, (float) area.getHeight()));
        g.setColour (colours::accent.withAlpha (0.35f));
        g.drawVerticalLine ((int) x1, (float) area.getY(), (float) area.getBottom());
        g.drawVerticalLine ((int) x2, (float) area.getY(), (float) area.getBottom());
    }

    // grid
    const auto& map = ctx.song.getTempoMap();
    const auto perBar = map.getTicksPerBar();
    const auto perBeat = map.getTicksPerBeat();
    const auto barPx = (double) perBar * pixelsPerTick;
    const auto firstBar = (Tick) std::floor (viewStartTick / (double) perBar);

    for (Tick bar = firstBar;; ++bar)
    {
        const auto x = tickToX (bar * perBar);
        if (x > area.getRight())
            break;

        g.setColour (colours::gridBar);
        g.drawVerticalLine ((int) x, (float) area.getY(), (float) area.getBottom());

        if (barPx >= 60)
        {
            g.setColour (colours::gridBeat);
            for (int b = 1; b < map.getTimeSignature().numerator; ++b)
                g.drawVerticalLine ((int) tickToX (bar * perBar + b * perBeat), (float) area.getY(), (float) area.getBottom());
        }
    }

    // events (dragged ones are drawn as preview instead)
    auto isBeingDragged = [this] (ObjectId id)
    {
        if (gesture != Gesture::moveEvents && gesture != Gesture::resizeStart && gesture != Gesture::resizeEnd)
            return false;
        return std::any_of (dragged.begin(), dragged.end(), [id] (auto& d) { return d.original.id == id; });
    };
    const bool copying = gesture == Gesture::moveEvents && juce::ModifierKeys::currentModifiers.isAltDown();

    for (size_t i = 0; i < tracks.size(); ++i)
    {
        const auto y = rowTop ((int) i);
        g.setColour (colours::border.withAlpha (0.7f));
        g.drawHorizontalLine (y + trackHeight - 1, (float) area.getX(), (float) area.getRight());

        for (auto& r : tracks[i].regions)
        {
            const auto b = regionBounds ((int) i, r);
            if (b.getRight() < (float) area.getX() || b.getX() > (float) area.getRight())
                continue;

            const bool hiddenByDrag = isBeingDragged (r.id) && dragMoved && ! copying;
            paintRegion (g, tracks[i], r, b, ctx.selection.isRegionSelected (r.id), hiddenByDrag);
        }
    }

    // drag preview
    if (dragMoved)
    {
        for (auto& [trackId, r] : preview)
        {
            const auto row = ctx.song.indexOfTrack (trackId);
            if (auto* t = ctx.song.findTrack (trackId); t != nullptr && row >= 0)
                paintRegion (g, *t, r, regionBounds (row, r), true, false);
        }
    }

    if (tracks.empty())
    {
        g.setColour (colours::textDim);
        g.setFont (font (15.0f));
        g.drawFittedText (u8"Instrumente, Effekte oder Audiodateien aus dem Browser hierher ziehen\n"
                          u8"oder mit + bzw. Rechtsklick eine Spur hinzufügen",
                          area.reduced (40), juce::Justification::centred, 3);
    }

    g.restoreState();
}

void ArrangeView::paintRegion (juce::Graphics& g, const Track& track, const Region& r, juce::Rectangle<float> box,
                               bool selected, bool ghost)
{
    if (ghost)
    {
        g.setColour (track.colour.withAlpha (0.15f));
        g.drawRoundedRectangle (box, 3.0f, 1.0f);
        return;
    }

    auto base = track.colour.withMultipliedSaturation (r.muted ? 0.0f : 0.85f);
    if (r.muted)
        base = base.withMultipliedBrightness (0.6f);

    g.setColour (base.withAlpha (selected ? 0.7f : 0.45f));
    g.fillRoundedRectangle (box, 3.0f);

    auto title = box.withHeight ((float) regionTitleHeight);
    g.setColour (selected ? base.brighter (0.35f) : base.withAlpha (0.95f));
    g.fillRoundedRectangle (title, 3.0f);
    g.fillRect (title.withTrimmedTop (8.0f));

    if (box.getWidth() > 12.0f)
    {
        g.setColour (colours::textDark);
        g.setFont (font (11.5f, true));
        auto text = r.name;
        if (r.muted)
            text = "[stumm] " + text;
        g.drawText (text, title.reduced (5.0f, 0.0f), juce::Justification::centredLeft, true);
    }

    auto body = box.withTrimmedTop ((float) regionTitleHeight + 1.0f).reduced (1.0f, 2.0f);

    g.saveState();
    g.reduceClipRegion (body.toNearestInt());

    if (r.kind == Region::Kind::midi && ! r.notes.empty() && r.length > 0)
    {
        int lo = 127, hi = 0;
        for (auto& n : r.notes)
        {
            lo = std::min (lo, n.pitch);
            hi = std::max (hi, n.pitch);
        }
        const auto range = (float) juce::jmax (12, hi - lo + 1);
        const auto noteH = juce::jlimit (1.5f, 5.0f, body.getHeight() / range);

        g.setColour (base.brighter (0.6f));
        for (auto& n : r.notes)
        {
            const auto nx = box.getX() + (float) ((double) n.start * pixelsPerTick);
            const auto nw = juce::jmax (1.5f, (float) ((double) n.length * pixelsPerTick));
            const auto ny = body.getBottom() - ((float) (n.pitch - lo) + 1.0f) / range * body.getHeight();
            g.fillRect (juce::Rectangle<float> (nx, ny, nw, noteH));
        }
    }
    else if (r.kind == Region::Kind::audio)
    {
        paintWaveform (g, r, body, base.brighter (0.7f));
    }

    g.restoreState();

    g.setColour (selected ? juce::Colours::white.withAlpha (0.9f) : base.darker (0.6f));
    g.drawRoundedRectangle (box, 3.0f, selected ? 1.5f : 1.0f);
}

void ArrangeView::paintWaveform (juce::Graphics& g, const Region& r, juce::Rectangle<float> body, juce::Colour colour)
{
    auto audio = ctx.pool.get (r.sourceId);

    if (audio == nullptr)
    {
        const auto error = ctx.pool.getError (r.sourceId);
        g.setColour (colour.withAlpha (0.8f));
        g.setFont (font (11.0f));
        if (ctx.song.findAudioSource (r.sourceId) == nullptr)
            g.drawHorizontalLine ((int) body.getCentreY(), body.getX(), body.getRight()); // demo events without a file
        else
            g.drawText (error.isNotEmpty() ? error : juce::String (u8"Lädt …"), body.reduced (4.0f, 0.0f),
                        juce::Justification::centredLeft, true);
        return;
    }

    const auto& map = ctx.song.getTempoMap();
    const auto numCh = audio->peaks.numChannels;
    const auto laneH = body.getHeight() / (float) numCh;
    const auto clip = g.getClipBounds();
    const auto x0 = std::max ((int) body.getX(), clip.getX());
    const auto x1 = std::min ((int) body.getRight(), clip.getRight());
    const auto regionStartSec = map.ticksToSeconds (r.position);

    auto sampleAtX = [&] (int x)
    {
        const auto sec = map.ticksToSeconds (xToTick ((double) x)) - regionStartSec;
        return (std::int64_t) (sec * audio->sampleRate) + r.sourceOffsetSamples;
    };

    const auto gain = juce::Decibels::decibelsToGain (r.gainDb);
    g.setColour (colour.withAlpha (0.85f));

    for (int ch = 0; ch < numCh; ++ch)
    {
        const auto centre = body.getY() + laneH * ((float) ch + 0.5f);
        const auto halfH = laneH * 0.48f;

        auto s0 = sampleAtX (x0);
        for (int x = x0; x < x1; ++x)
        {
            const auto s1 = std::max (s0 + 1, sampleAtX (x + 1));
            auto [lo, hi] = audio->peaks.getRange (ch, s0, s1);
            lo = juce::jlimit (-1.0f, 1.0f, lo * gain);
            hi = juce::jlimit (-1.0f, 1.0f, hi * gain);
            g.drawVerticalLine (x, centre - hi * halfH, centre - lo * halfH + 1.0f);
            s0 = s1;
        }
    }
}

void ArrangeView::paintOverChildren (juce::Graphics& g)
{
    const auto lanes = getLaneArea();

    // playhead
    const auto x = (float) tickToX (ctx.song.getTempoMap().secondsToTicks (ctx.engine.getPositionSeconds()));
    if (x >= (float) lanes.getX() && x <= (float) lanes.getRight())
    {
        g.setColour (colours::playhead);
        g.drawLine (x, (float) rulerHeight / 2.0f, x, (float) lanes.getBottom(), 1.0f);

        juce::Path marker;
        marker.addTriangle (x - 6.0f, (float) rulerHeight - 10.0f, x + 6.0f, (float) rulerHeight - 10.0f, x, (float) rulerHeight);
        g.fillPath (marker);
    }

    // rubber band
    if (gesture == Gesture::rubberBand && ! rubberBand.isEmpty())
    {
        g.setColour (colours::accent.withAlpha (0.12f));
        g.fillRect (rubberBand);
        g.setColour (colours::accent.withAlpha (0.8f));
        g.drawRect (rubberBand, 1);
    }

    // track reorder indicator
    if (trackDropIndex)
    {
        g.setColour (colours::accent);
        g.fillRect (0, rowTop (*trackDropIndex) - 1, getWidth() - vScroll.getWidth(), 3);
    }

    // browser drop indicator
    if (dropPoint)
    {
        const auto target = dropTargetAt (*dropPoint);
        g.setColour (colours::accent);

        if (target.trackId.isValid() && ! dropNewTrackOnly)
        {
            const auto row = ctx.song.indexOfTrack (target.trackId);
            g.drawRect (juce::Rectangle<int> (0, rowTop (row), getWidth() - vScroll.getWidth(), trackHeight), 2);
        }
        else
        {
            g.fillRect (0, rowTop (target.insertIndex) - 1, getWidth() - vScroll.getWidth(), 3);
        }

        const auto px = (float) tickToX (target.position);
        if (px >= (float) lanes.getX())
        {
            g.setColour (colours::accent.withAlpha (0.6f));
            g.drawVerticalLine ((int) px, (float) lanes.getY(), (float) lanes.getBottom());
        }
    }
}

//==============================================================================
void ArrangeView::mouseMove (const juce::MouseEvent& e)
{
    const int tool = ctx.edit.tool.getValue();
    auto newCursor = juce::MouseCursor::NormalCursor;

    if (getRulerArea().contains (e.getPosition()))
    {
        newCursor = e.y < rulerHeight / 2 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::PointingHandCursor;
    }
    else if (auto hit = hitTestEvents (e.getPosition()); hit.region != nullptr)
    {
        if (tool == EditState::arrow || tool == EditState::range)
            newCursor = hit.zone == Hit::body ? juce::MouseCursor::NormalCursor : juce::MouseCursor::LeftRightResizeCursor;
        else if (tool == EditState::split)
            newCursor = juce::MouseCursor::IBeamCursor;
        else
            newCursor = juce::MouseCursor::PointingHandCursor;
    }
    else if (tool == EditState::pencil)
    {
        newCursor = juce::MouseCursor::CrosshairCursor;
    }

    setMouseCursor (newCursor);
}

void ArrangeView::mouseDown (const juce::MouseEvent& e)
{
    gesture = Gesture::none;
    dragMoved = false;
    preview.clear();
    dragged.clear();

    // --- ruler: upper half = loop range, lower half = playhead ----------------
    if (getRulerArea().contains (e.getPosition()))
    {
        if (e.y < rulerHeight / 2)
        {
            gesture = Gesture::setLoop;
            loopAnchor = snap (xToTick (e.position.x));
        }
        else
        {
            gesture = Gesture::setPlayhead;
            ctx.engine.setPositionSeconds (ctx.song.getTempoMap().ticksToSeconds (snap (xToTick (e.position.x))));
            repaint();
        }
        return;
    }

    const auto hit = hitTestEvents (e.getPosition());

    if (hit.trackId.isValid())
        ctx.selection.selectTrack (hit.trackId);
    else if (! e.mods.isPopupMenu() && e.y > rulerHeight && rowAtY (e.y) >= ctx.song.getNumTracks())
        ctx.selection.selectTrack ({});

    if (e.mods.isPopupMenu())
    {
        if (hit.region != nullptr && ! ctx.selection.isRegionSelected (hit.region->id))
            ctx.selection.setSelectedRegions ({ hit.region->id });
        else if (hit.region == nullptr)
            ctx.selection.clearRegions();
        showContextMenu (snap (xToTick (e.position.x)));
        return;
    }

    const int tool = ctx.edit.tool.getValue();

    if (tool == EditState::arrow || tool == EditState::range)
    {
        if (hit.region != nullptr)
        {
            const auto id = hit.region->id;
            const bool additive = e.mods.isCommandDown() || e.mods.isShiftDown();

            if (additive && ctx.selection.isRegionSelected (id))
            {
                ctx.selection.toggleRegion (id); // deselect, no drag
                return;
            }

            if (additive)
                ctx.selection.addRegion (id);
            else if (! ctx.selection.isRegionSelected (id))
                ctx.selection.setSelectedRegions ({ id });

            beginEventDrag (hit, e);
        }
        else if (getLaneArea().contains (e.getPosition()))
        {
            gesture = Gesture::rubberBand;
            selectionBeforeRubberBand = (e.mods.isCommandDown() || e.mods.isShiftDown()) ? ctx.selection.getSelectedRegions()
                                                                                       : std::vector<ObjectId> {};
            rubberBand = {};
            if (selectionBeforeRubberBand.empty())
                ctx.selection.clearRegions();
        }
        return;
    }

    applyTool (hit, e);
}

void ArrangeView::applyTool (const Hit& hit, const juce::MouseEvent& e)
{
    const int tool = ctx.edit.tool.getValue();
    const auto& map = ctx.song.getTempoMap();

    // tools act on all selected events if the clicked one is selected, otherwise on the clicked one
    auto targets = [&]
    {
        std::vector<std::pair<ObjectId, Region>> out;
        if (hit.region == nullptr)
            return out;

        if (ctx.selection.isRegionSelected (hit.region->id))
        {
            for (auto id : ctx.selection.getSelectedRegions())
                if (auto* t = ctx.song.findTrackOfRegion (id))
                    out.emplace_back (t->id, *t->findRegion (id));
        }
        else
        {
            out.emplace_back (hit.trackId, *hit.region);
        }
        return out;
    };

    if (tool == EditState::split && hit.region != nullptr)
    {
        const auto at = snap (xToTick (e.position.x));
        ctx.history.beginTransaction ("Event teilen");
        std::vector<ObjectId> newSelection;

        for (auto& [trackId, r] : targets())
        {
            if (auto parts = regionops::split (r, at, ctx.song.findAudioSource (r.sourceId), map, ctx.song.getIds()))
            {
                newSelection.push_back (parts->second.id);
                ctx.perform (std::make_unique<commands::UpdateRegion> (trackId, trackId, parts->first, "Event teilen"));
                ctx.perform (std::make_unique<commands::AddRegion> (trackId, parts->second));
            }
        }
        ctx.history.endTransaction();
        if (! newSelection.empty())
            ctx.selection.setSelectedRegions (newSelection);
    }
    else if (tool == EditState::eraser && hit.region != nullptr)
    {
        ctx.history.beginTransaction (u8"Event löschen");
        for (auto& [trackId, r] : targets())
            ctx.perform (std::make_unique<commands::RemoveRegion> (trackId, r.id));
        ctx.history.endTransaction();
        ctx.selection.clearRegions();
    }
    else if (tool == EditState::mute && hit.region != nullptr)
    {
        const bool newState = ! hit.region->muted;
        ctx.history.beginTransaction ("Event stummschalten");
        for (auto& [trackId, r] : targets())
        {
            auto m = r;
            m.muted = newState;
            ctx.perform (std::make_unique<commands::UpdateRegion> (trackId, trackId, m, "Event stummschalten"));
        }
        ctx.history.endTransaction();
    }
    else if (tool == EditState::pencil && hit.region == nullptr && hit.trackId.isValid())
    {
        auto* track = ctx.song.findTrack (hit.trackId);
        if (track == nullptr || track->typeId != "instrument")
            return;

        Region r;
        r.id = ctx.song.newId();
        r.kind = Region::Kind::midi;
        r.name = track->name;
        const auto grid = std::max<Tick> (map.getTicksPerBar(), 1);
        r.position = (xToTick (e.position.x) / grid) * grid; // start of the bar
        r.length = map.getTicksPerBar();
        const auto id = r.id;
        ctx.perform (std::make_unique<commands::AddRegion> (track->id, std::move (r)));
        ctx.selection.setSelectedRegions ({ id });
    }
}

void ArrangeView::beginEventDrag (const Hit& hit, const juce::MouseEvent&)
{
    anchor = *hit.region;
    anchorRow = hit.row;
    gesture = hit.zone == Hit::leftEdge  ? Gesture::resizeStart
            : hit.zone == Hit::rightEdge ? Gesture::resizeEnd
                                         : Gesture::moveEvents;

    for (auto id : ctx.selection.getSelectedRegions())
        if (auto* t = ctx.song.findTrackOfRegion (id))
            dragged.push_back ({ t->id, *t->findRegion (id) });
}

void ArrangeView::updateEventDrag (const juce::MouseEvent& e)
{
    if (! dragMoved && e.getDistanceFromDragStart() < 4)
        return;

    dragMoved = true;
    preview.clear();

    const auto& map = ctx.song.getTempoMap();
    const auto& tracks = ctx.song.getTracks();
    const auto mouseTick = xToTick (e.position.x);
    const auto downTick = xToTick ((float) e.getMouseDownX());

    if (gesture == Gesture::moveEvents)
    {
        // snap the start of the grabbed event, move everything by the same amount
        auto newStart = snap (anchor.position + (mouseTick - downTick));
        if (e.mods.isShiftDown())
            newStart = std::max<Tick> (0, anchor.position + (mouseTick - downTick)); // Shift while dragging: no snap

        Tick delta = newStart - anchor.position;
        for (auto& d : dragged)
            delta = std::max (delta, -d.original.position); // nothing before 0

        const auto rowDelta = rowAtY (e.y) - anchorRow;

        for (auto& d : dragged)
        {
            auto r = d.original;
            r.position += delta;

            auto targetTrack = d.trackId;
            const auto fromRow = ctx.song.indexOfTrack (d.trackId);
            const auto toRow = juce::jlimit (0, (int) tracks.size() - 1, fromRow + rowDelta);
            if (rowDelta != 0 && regionFitsTrack (r, tracks[(size_t) toRow]))
                targetTrack = tracks[(size_t) toRow].id;

            preview.emplace_back (targetTrack, std::move (r));
        }
    }
    else
    {
        const auto edgeTick = e.mods.isShiftDown() ? mouseTick : snap (mouseTick);
        const auto delta = edgeTick - (gesture == Gesture::resizeStart ? anchor.position : anchor.getEnd());

        for (auto& d : dragged)
        {
            const auto* src = ctx.song.findAudioSource (d.original.sourceId);
            auto r = gesture == Gesture::resizeStart ? regionops::withStart (d.original, d.original.position + delta, src, map)
                                                     : regionops::withEnd (d.original, d.original.getEnd() + delta, src, map);
            preview.emplace_back (d.trackId, std::move (r));
        }
    }

    repaint();
}

void ArrangeView::commitEventDrag (bool copy)
{
    if (! dragMoved || preview.size() != dragged.size())
        return;

    const auto name = gesture == Gesture::moveEvents ? (copy ? "Event kopieren" : "Event verschieben")
                                                     : juce::String (u8"Länge ändern");
    ctx.history.beginTransaction (name);
    std::vector<ObjectId> newSelection;

    for (size_t i = 0; i < dragged.size(); ++i)
    {
        auto& [toTrack, r] = preview[i];

        if (copy && gesture == Gesture::moveEvents)
        {
            auto c = regionops::duplicate (r, ctx.song.getIds());
            newSelection.push_back (c.id);
            ctx.perform (std::make_unique<commands::AddRegion> (toTrack, std::move (c)));
        }
        else
        {
            newSelection.push_back (r.id);
            ctx.perform (std::make_unique<commands::UpdateRegion> (dragged[i].trackId, toTrack, r, name));
        }
    }

    ctx.history.endTransaction();
    ctx.selection.setSelectedRegions (newSelection);
}

void ArrangeView::updateRubberBand()
{
    std::vector<ObjectId> ids = selectionBeforeRubberBand;
    const auto band = rubberBand.toFloat();
    const auto& tracks = ctx.song.getTracks();

    for (size_t i = 0; i < tracks.size(); ++i)
        for (auto& r : tracks[i].regions)
            if (regionBounds ((int) i, r).intersects (band) && std::find (ids.begin(), ids.end(), r.id) == ids.end())
                ids.push_back (r.id);

    ctx.selection.setSelectedRegions (ids);
}

void ArrangeView::mouseDrag (const juce::MouseEvent& e)
{
    switch (gesture)
    {
        case Gesture::setPlayhead:
            ctx.engine.setPositionSeconds (ctx.song.getTempoMap().ticksToSeconds (snap (xToTick (e.position.x))));
            repaint();
            break;

        case Gesture::setLoop:
        {
            const auto t = snap (xToTick (e.position.x));
            if (t != loopAnchor)
                ctx.song.setLoop ({ std::min (loopAnchor, t), std::max (loopAnchor, t), true });
            break;
        }

        case Gesture::moveEvents:
        case Gesture::resizeStart:
        case Gesture::resizeEnd:
            updateEventDrag (e);
            break;

        case Gesture::rubberBand:
            rubberBand = juce::Rectangle<int> (e.getMouseDownPosition(), e.getPosition()).getIntersection (getLaneArea());
            updateRubberBand();
            repaint();
            break;

        case Gesture::none:
            break;
    }
}

void ArrangeView::mouseUp (const juce::MouseEvent& e)
{
    if (gesture == Gesture::moveEvents || gesture == Gesture::resizeStart || gesture == Gesture::resizeEnd)
    {
        // a plain click on an already selected event (no drag, no modifier) selects only that one
        if (! dragMoved && ! e.mods.isCommandDown() && ! e.mods.isShiftDown())
            ctx.selection.setSelectedRegions ({ anchor.id });

        commitEventDrag (e.mods.isAltDown());
    }

    gesture = Gesture::none;
    dragMoved = false;
    preview.clear();
    dragged.clear();
    rubberBand = {};
    repaint();
}

void ArrangeView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (getRulerArea().contains (e.getPosition()) && e.y < rulerHeight / 2)
    {
        auto loop = ctx.song.getLoop();
        loop.enabled = ! loop.enabled;
        ctx.song.setLoop (loop);
        return;
    }

    // double click below the last track: add a track (like Studio One)
    if (e.y > rulerHeight && rowAtY (e.y) >= ctx.song.getNumTracks())
        showAddTrackMenu (nullptr);
}

void ArrangeView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        zoomAround (std::pow (1.25, (double) w.deltaY * 4.0), e.x);
        return;
    }

    const bool horizontal = e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY);

    if (horizontal)
    {
        const auto delta = (e.mods.isShiftDown() ? w.deltaY : w.deltaX) * 400.0f;
        hScroll.setCurrentRangeStart (hScroll.getCurrentRangeStart() - (double) delta / pixelsPerTick);
    }
    else
    {
        vScroll.setCurrentRangeStart (vScroll.getCurrentRangeStart() - (double) w.deltaY * 300.0);
    }
}

void ArrangeView::mouseMagnify (const juce::MouseEvent& e, float scale)
{
    zoomAround ((double) scale, e.x);
}

//==============================================================================
void ArrangeView::trackDragged (ObjectId, int y)
{
    const auto n = ctx.song.getNumTracks();
    const auto row = juce::jlimit (0, n, (y - rulerHeight + viewTop + trackHeight / 2) / trackHeight);
    trackDropIndex = row;
    repaint();
}

void ArrangeView::trackDragEnded (ObjectId id, int y)
{
    trackDragged (id, y);
    const auto from = ctx.song.indexOfTrack (id);
    auto to = *trackDropIndex;
    trackDropIndex.reset();

    if (from >= 0)
    {
        if (to > from)
            --to; // index after removal

        // perform asynchronously: the header that is still handling its mouse
        // event would otherwise be deleted underneath it by rebuildHeaders()
        if (to != from)
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ArrangeView> (this), id, to]
            {
                if (safe != nullptr)
                    safe->ctx.perform (std::make_unique<commands::MoveTrack> (id, to));
            });
    }
    repaint();
}

//==============================================================================
void ArrangeView::showAddTrackMenu (juce::Component* target)
{
    juce::PopupMenu m;
    m.addSectionHeader (u8"Spur hinzufügen");
    int i = 1;
    for (auto& type : ctx.registry.getTrackTypes())
        m.addItem (i++, type.displayName + "spur");

    auto options = juce::PopupMenu::Options();
    if (target != nullptr)
        options = options.withTargetComponent (target);

    m.showMenuAsync (options, [this] (int result)
    {
        if (result > 0 && result <= (int) ctx.registry.getTrackTypes().size())
        {
            const auto selected = ctx.song.indexOfTrack (ctx.selection.getSelectedTrack());
            addTrack (ctx, ctx.registry.getTrackTypes()[(size_t) result - 1].id, selected >= 0 ? selected + 1 : -1);
        }
    });
}

void ArrangeView::showContextMenu (Tick mouseTick)
{
    juce::PopupMenu m;
    const bool hasEvents = ! ctx.selection.getSelectedRegions().empty();

    // menu entry for a registered action, with its shortcut and enabled state
    std::map<int, juce::String> actionIds;
    int nextId = 3000;
    auto addAction = [&] (const juce::String& actionId, const juce::String& label)
    {
        auto* a = ctx.registry.findAction (actionId);
        if (a == nullptr)
            return;
        juce::PopupMenu::Item item (label);
        item.itemID = nextId;
        item.isEnabled = ! a->isEnabled || a->isEnabled();
        if (! a->keys.empty())
            item.shortcutKeyDescription = a->keys.front().getTextDescriptionWithIcons();
        m.addItem (std::move (item));
        actionIds[nextId++] = actionId;
    };

    constexpr int splitHere = 2000;

    // tools at the top, like the toolbar
    m.addCustomItem (4000, std::make_unique<ToolMenuRow> (ctx), nullptr, "Werkzeuge");
    m.addSeparator();

    if (hasEvents)
    {
        m.addItem (splitHere, "Hier teilen");
        addAction ("edit.splitAtCursor", "Am Abspielpunkt teilen");
        addAction ("edit.merge", u8"Zusammenfügen");
        m.addSeparator();
        addAction ("edit.cut", "Ausschneiden");
        addAction ("edit.copy", "Kopieren");
        addAction ("edit.paste", u8"Einfügen");
        addAction ("edit.duplicate", "Duplizieren");
        m.addSeparator();
        addAction ("edit.toggleMuteEvents", "Stumm ein/aus");
        addAction ("transport.loopSelection", "Loop auf Auswahl");
        addAction ("edit.delete", u8"Löschen");
    }
    else
    {
        addAction ("edit.paste", u8"Einfügen (am Abspielpunkt)");
        m.addSeparator();

        int i = 1;
        for (auto& type : ctx.registry.getTrackTypes())
            m.addItem (i++, type.displayName + juce::String (u8"spur hinzufügen"));

        m.addSeparator();
        m.addItem (1000, u8"Spur löschen", ctx.selection.getSelectedTrack().isValid());
    }

    const auto sel = ctx.selection.getSelectedTrack();

    m.showMenuAsync ({}, [this, sel, mouseTick, actionIds] (int result)
    {
        if (result == 1000)
        {
            ctx.registry.invoke ("track.delete");
        }
        else if (result == splitHere)
        {
            const auto& map = ctx.song.getTempoMap();
            std::vector<ObjectId> right;
            ctx.history.beginTransaction ("Event teilen");
            for (auto id : ctx.selection.getSelectedRegions())
            {
                auto* t = ctx.song.findTrackOfRegion (id);
                if (t == nullptr)
                    continue;
                const auto trackId = t->id;
                const auto r = *t->findRegion (id);
                if (auto parts = regionops::split (r, mouseTick, ctx.song.findAudioSource (r.sourceId), map, ctx.song.getIds()))
                {
                    right.push_back (parts->second.id);
                    ctx.perform (std::make_unique<commands::UpdateRegion> (trackId, trackId, parts->first, "Event teilen"));
                    ctx.perform (std::make_unique<commands::AddRegion> (trackId, parts->second));
                }
            }
            ctx.history.endTransaction();
            if (! right.empty())
                ctx.selection.setSelectedRegions (right);
        }
        else if (auto it = actionIds.find (result); it != actionIds.end())
        {
            ctx.registry.invoke (it->second);
        }
        else if (result > 0 && result <= (int) ctx.registry.getTrackTypes().size())
        {
            const auto index = ctx.song.indexOfTrack (sel);
            addTrack (ctx, ctx.registry.getTrackTypes()[(size_t) result - 1].id, index >= 0 ? index + 1 : -1);
        }
    });
}

//==============================================================================
DropTarget ArrangeView::dropTargetAt (juce::Point<int> p) const
{
    DropTarget t;
    const auto& tracks = ctx.song.getTracks();
    const auto row = rowAtY (p.y);

    if (row >= 0 && row < (int) tracks.size())
    {
        t.trackId = tracks[(size_t) row].id;
        t.insertIndex = row + 1;
    }
    else
    {
        t.insertIndex = (int) tracks.size();
    }

    const auto x = juce::jmax ((double) headerWidth, (double) p.x);
    t.position = snap (xToTick (x));
    return t;
}

void ArrangeView::setDropIndicator (std::optional<juce::Point<int>> p, bool newTrackOnly)
{
    dropPoint = p;
    dropNewTrackOnly = newTrackOnly;
    repaint();
}

bool ArrangeView::isInterestedInDragSource (const SourceDetails& d)
{
    if (auto item = BrowserItem::fromDragDescription (d.description))
        return ctx.registry.findDropHandler (item->kind) != nullptr;
    return false;
}

void ArrangeView::itemDragEnter (const SourceDetails& d) { itemDragMove (d); }

void ArrangeView::itemDragMove (const SourceDetails& d)
{
    auto item = BrowserItem::fromDragDescription (d.description);
    const auto row = rowAtY (d.localPosition.y);
    bool newTrackOnly = false;

    if (item && row >= 0 && row < ctx.song.getNumTracks())
    {
        auto& track = ctx.song.getTracks()[(size_t) row];
        newTrackOnly = (item->kind == "instrument" && track.typeId != "instrument")
                    || (item->kind == "audiofile" && track.typeId != "audio");
    }

    setDropIndicator (d.localPosition, newTrackOnly);
}

void ArrangeView::itemDragExit (const SourceDetails&) { setDropIndicator (std::nullopt); }

void ArrangeView::itemDropped (const SourceDetails& d)
{
    setDropIndicator (std::nullopt);

    if (auto item = BrowserItem::fromDragDescription (d.description))
        if (auto* handler = ctx.registry.findDropHandler (item->kind))
            handler->drop (ctx, *item, dropTargetAt (d.localPosition));
}

bool ArrangeView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (isSupportedAudioFile (juce::File (f)))
            return true;
    return false;
}

void ArrangeView::fileDragMove (const juce::StringArray&, int x, int y)
{
    setDropIndicator (juce::Point<int> (x, y));
}

void ArrangeView::fileDragExit (const juce::StringArray&) { setDropIndicator (std::nullopt); }

void ArrangeView::filesDropped (const juce::StringArray& files, int x, int y)
{
    setDropIndicator (std::nullopt);
    auto target = dropTargetAt ({ x, y });

    for (auto& f : files)
    {
        BrowserItem item { "audiofile", f, juce::File (f).getFileNameWithoutExtension(), {} };
        if (auto* handler = ctx.registry.findDropHandler (item.kind))
            if (handler->drop (ctx, item, target))
                target.insertIndex++; // several files: one track per file, in order
    }
}

} // namespace sms
