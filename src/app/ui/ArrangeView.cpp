// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ArrangeView.h"

#include "SongHelpers.h"
#include "core/Commands.h"
#include "extension/ExtensionRegistry.h"

namespace sms
{

using namespace theme;
using Param = commands::SetTrackParam::Param;

//==============================================================================
class TrackHeader final : public juce::Component
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

        // colour strip with track number
        auto strip = r.removeFromLeft (22);
        g.setColour (colour.withMultipliedSaturation (selected ? 1.0f : 0.8f));
        g.fillRect (strip);
        g.setColour (colours::textDark);
        g.setFont (font (11.5f, true));
        g.drawText (juce::String (index + 1), strip.removeFromTop (22), juce::Justification::centred);

        // type icon
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
        ctx.selection.selectTrack (trackId);
        if (e.mods.isPopupMenu())
            if (auto* arrange = findParentComponentOfClass<ArrangeView>())
                arrange->mouseDown (e.getEventRelativeTo (arrange));
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
    rebuildHeaders();
    startTimerHz (30);
}

ArrangeView::~ArrangeView()
{
    ctx.song.removeListener (this);
    ctx.selection.removeListener (this);
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

//==============================================================================
void ArrangeView::resized()
{
    const int sb = getLookAndFeel().getDefaultScrollbarWidth();
    vScroll.setBounds (getWidth() - sb, rulerHeight, sb, getHeight() - rulerHeight - sb);
    hScroll.setBounds (headerWidth, getHeight() - sb, getWidth() - headerWidth - sb, sb);
    headerColumn->setBounds (0, rulerHeight, headerWidth, getHeight() - rulerHeight - sb);
    addTrackButton.setBounds (6, 5, 20, 20);
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
    const auto anchor = (double) xToTick ((double) x);
    pixelsPerTick = juce::jlimit (2.0 / (double) ticksPerQuarter, 600.0 / (double) ticksPerQuarter, pixelsPerTick * factor);
    viewStartTick = juce::jmax (0.0, anchor - ((double) x - headerWidth) / pixelsPerTick);
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

void ArrangeView::timerCallback()
{
    const auto seconds = ctx.engine.getPositionSeconds();
    const auto x = tickToX (ctx.song.getTempoMap().secondsToTicks (seconds));

    if (! juce::approximatelyEqual (x, lastPlayheadX))
    {
        const auto lanes = getLaneArea();

        // follow the playhead page by page while playing
        if (ctx.engine.isPlaying() && x > lanes.getRight() - 20)
        {
            viewStartTick += (double) lanes.getWidth() * 0.8 / pixelsPerTick;
            updateScrollBars();
            repaint();
        }
        else
        {
            const auto top = 0, h = getHeight() - hScroll.getHeight();
            repaint ((int) lastPlayheadX - 6, top, 12, h);
            repaint ((int) x - 6, top, 12, h);
        }
        lastPlayheadX = x;
    }
}

//==============================================================================
void ArrangeView::paint (juce::Graphics& g)
{
    g.fillAll (colours::laneB);

    // top-left corner above the track headers
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

    // corner between scrollbars
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

    const auto& map = ctx.song.getTempoMap();
    const auto perBar = map.getTicksPerBar();
    const auto perBeat = map.getTicksPerBeat();
    const auto barPx = (double) perBar * pixelsPerTick;
    const int labelEvery = barPx >= 60 ? 1 : barPx >= 30 ? 2 : barPx >= 12 ? 4 : 8;

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
            g.drawVerticalLine ((int) x, (float) area.getY() + 12.0f, (float) area.getBottom());
            g.setColour (colours::text);
            g.drawText (juce::String (bar + 1), (int) x + 3, area.getY() + 2, 40, 14, juce::Justification::centredLeft);
        }

        if (barPx >= 80)
        {
            g.setColour (colours::textDim.withAlpha (0.5f));
            for (int b = 1; b < map.getTimeSignature().numerator; ++b)
            {
                const auto bx = tickToX (bar * perBar + b * perBeat);
                g.drawVerticalLine ((int) bx, (float) area.getBottom() - 7.0f, (float) area.getBottom());
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
    const auto selected = ctx.selection.getSelectedTrack();

    for (size_t i = 0; i < tracks.size(); ++i)
    {
        const auto y = area.getY() + (int) i * trackHeight - viewTop;
        auto lane = juce::Rectangle<int> (area.getX(), y, area.getWidth(), trackHeight);
        if (! lane.intersects (area))
            continue;

        g.setColour (tracks[i].id == selected ? colours::laneSelected : (i % 2 == 0 ? colours::laneA : colours::laneB));
        g.fillRect (lane);
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

    // lane separators and regions
    for (size_t i = 0; i < tracks.size(); ++i)
    {
        const auto y = area.getY() + (int) i * trackHeight - viewTop;
        g.setColour (colours::border.withAlpha (0.7f));
        g.drawHorizontalLine (y + trackHeight - 1, (float) area.getX(), (float) area.getRight());

        for (auto& r : tracks[i].regions)
        {
            const auto x1 = tickToX (r.position), x2 = tickToX (r.getEnd());
            if (x2 < area.getX() || x1 > area.getRight())
                continue;

            paintRegion (g, tracks[i], r, juce::Rectangle<float> ((float) x1, (float) y + 2.0f, (float) (x2 - x1), (float) trackHeight - 5.0f));
        }
    }

    if (tracks.empty())
    {
        g.setColour (colours::textDim);
        g.setFont (font (15.0f));
        g.drawFittedText ("Instrumente, Effekte oder Audiodateien aus dem Browser hierher ziehen\n"
                          u8"oder mit + bzw. Rechtsklick eine Spur hinzufügen",
                          area.reduced (40), juce::Justification::centred, 3);
    }

    g.restoreState();
}

void ArrangeView::paintRegion (juce::Graphics& g, const Track& track, const Region& r, juce::Rectangle<float> box)
{
    const auto base = track.colour.withMultipliedSaturation (r.muted ? 0.0f : 0.85f);
    const bool clipOnly = box.getWidth() < 6.0f;

    g.setColour (base.withAlpha (0.45f));
    g.fillRoundedRectangle (box, 3.0f);

    auto title = box.withHeight (16.0f);
    g.setColour (base.withAlpha (0.95f));
    g.fillRoundedRectangle (title, 3.0f);
    g.fillRect (title.withTrimmedTop (8.0f));

    if (! clipOnly)
    {
        g.setColour (colours::textDark);
        g.setFont (font (11.5f, true));
        g.drawText (r.name, title.reduced (5.0f, 0.0f), juce::Justification::centredLeft, true);
    }

    auto body = box.withTrimmedTop (17.0f).reduced (2.0f, 2.0f);

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
            g.fillRect (juce::Rectangle<float> (nx, ny, nw, noteH).getIntersection (body));
        }
    }
    else if (r.kind == Region::Kind::audio)
    {
        // waveform drawing arrives with audio playback (stage 2): show a centre line for now
        g.setColour (base.brighter (0.5f).withAlpha (0.6f));
        g.drawHorizontalLine ((int) body.getCentreY(), body.getX(), body.getRight());
    }

    g.setColour (base.darker (0.6f));
    g.drawRoundedRectangle (box, 3.0f, 1.0f);
}

void ArrangeView::paintOverChildren (juce::Graphics& g)
{
    // playhead
    const auto x = (float) tickToX (ctx.song.getTempoMap().secondsToTicks (ctx.engine.getPositionSeconds()));
    const auto lanes = getLaneArea();

    if (x >= (float) lanes.getX() && x <= (float) lanes.getRight())
    {
        g.setColour (colours::playhead);
        g.drawLine (x, 0.0f, x, (float) lanes.getBottom(), 1.0f);

        juce::Path marker;
        marker.addTriangle (x - 6.0f, (float) rulerHeight - 10.0f, x + 6.0f, (float) rulerHeight - 10.0f, x, (float) rulerHeight);
        g.fillPath (marker);
    }

    // drop indicator
    if (dropPoint)
    {
        const auto target = dropTargetAt (*dropPoint);
        g.setColour (colours::accent);

        if (target.trackId.isValid() && ! dropNewTrackOnly)
        {
            const auto row = ctx.song.indexOfTrack (target.trackId);
            const auto y = rulerHeight + row * trackHeight - viewTop;
            g.drawRect (juce::Rectangle<int> (0, y, getWidth() - vScroll.getWidth(), trackHeight), 2);
        }
        else
        {
            const auto y = rulerHeight + target.insertIndex * trackHeight - viewTop;
            g.fillRect (0, y - 1, getWidth() - vScroll.getWidth(), 3);
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
void ArrangeView::mouseDown (const juce::MouseEvent& e)
{
    if (getRulerArea().contains (e.getPosition()))
    {
        const auto t = ctx.edit.snapTick (xToTick (e.position.x), ctx.song.getTempoMap());
        ctx.engine.setPositionSeconds (ctx.song.getTempoMap().ticksToSeconds (t));
        repaint();
        return;
    }

    const auto row = rowAtY (e.y);
    const auto& tracks = ctx.song.getTracks();

    if (row >= 0 && row < (int) tracks.size())
        ctx.selection.selectTrack (tracks[(size_t) row].id);
    else if (! e.mods.isPopupMenu())
        ctx.selection.selectTrack ({});

    if (e.mods.isPopupMenu())
        showContextMenu();
}

void ArrangeView::mouseDrag (const juce::MouseEvent& e)
{
    if (getRulerArea().contains (e.getMouseDownPosition()))
    {
        const auto t = ctx.edit.snapTick (xToTick (e.position.x), ctx.song.getTempoMap());
        ctx.engine.setPositionSeconds (ctx.song.getTempoMap().ticksToSeconds (t));
        repaint();
    }
}

void ArrangeView::mouseDoubleClick (const juce::MouseEvent& e)
{
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

void ArrangeView::showContextMenu()
{
    juce::PopupMenu m;
    int i = 1;
    for (auto& type : ctx.registry.getTrackTypes())
        m.addItem (i++, type.displayName + juce::String (u8"spur hinzufügen"));

    const auto sel = ctx.selection.getSelectedTrack();
    m.addSeparator();
    m.addItem (1000, u8"Spur löschen", sel.isValid());

    m.showMenuAsync ({}, [this, sel] (int result)
    {
        if (result == 1000)
        {
            ctx.registry.invoke ("track.delete");
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
    t.position = ctx.edit.snapTick (xToTick (x), ctx.song.getTempoMap());
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
        // instruments only replace on instrument tracks, otherwise they create a new track
        newTrackOnly = (item->kind == "instrument" && track.typeId != "instrument");
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
