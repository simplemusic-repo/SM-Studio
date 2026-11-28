// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PianoRoll.h"

#include "Theme.h"
#include "core/Commands.h"
#include "core/NoteOps.h"
#include "extension/ExtensionRegistry.h"

#include <cmath>

namespace sms
{

using namespace theme;

namespace
{
    struct GridChoice { const char* name; Tick ticks; };
    const GridChoice gridChoices[] = {
        { "Raster: automatisch", 0 },
        { "Raster: 1/4", ticksPerQuarter },
        { "Raster: 1/8", ticksPerQuarter / 2 },
        { "Raster: 1/16", ticksPerQuarter / 4 },
        { "Raster: 1/32", ticksPerQuarter / 8 },
        { "Raster: 1/8 Triole", ticksPerQuarter / 3 },
        { "Raster: 1/16 Triole", ticksPerQuarter / 6 },
    };

    bool isBlackKey (int pitch)
    {
        const auto k = pitch % 12;
        return k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
    }

    juce::String pitchName (int pitch)
    {
        return juce::MidiMessage::getMidiNoteName (pitch, true, true, 3);
    }

    constexpr float edgeZone = 6.0f;
}

//==============================================================================
PianoRollPanel::PianoRollPanel (AppContext& c) : ctx (c)
{
    setWantsKeyboardFocus (true);
    setOpaque (true);

    title.setFont (font (14.0f, true));
    title.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (title);

    for (int i = 0; i < (int) std::size (gridChoices); ++i)
        gridBox.addItem (gridChoices[i].name, i + 1);
    gridBox.setSelectedItemIndex (juce::jlimit (0, (int) std::size (gridChoices) - 1, ctx.settings.getIntValue ("editor.grid", 3)),
                                  juce::dontSendNotification);
    gridBox.setTooltip (u8"Raster für Noten: Verschieben, Zeichnen und Quantisieren rasten hier ein (Alt gedrückt: frei)");
    gridBox.onChange = [this] { ctx.settings.setValue ("editor.grid", gridBox.getSelectedItemIndex()); repaint(); };
    addAndMakeVisible (gridBox);

    quantizeButton.setTooltip (u8"Ausgewählte Noten (oder alle) auf das Raster setzen (Q)");
    quantizeButton.setWantsKeyboardFocus (false);
    quantizeButton.onClick = [this] { quantizeSelection(); };
    addAndMakeVisible (quantizeButton);

    hint.setFont (font (12.0f));
    hint.setColour (juce::Label::textColourId, colours::textDim);
    hint.setJustificationType (juce::Justification::centredRight);
    hint.setText (u8"Doppelklick: Note setzen/löschen · Stift: zeichnen · Pfeiltasten: verschieben · Entf: löschen",
                  juce::dontSendNotification);
    hint.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (hint);

    for (auto* sb : { &hScroll, &vScroll })
    {
        sb->addListener (this);
        sb->setAutoHide (false);
        addAndMakeVisible (*sb);
    }

    ctx.song.addListener (this);
    ctx.selection.addListener (this);
    followSelection();
    startTimerHz (30);
}

PianoRollPanel::~PianoRollPanel()
{
    ctx.song.removeListener (this);
    ctx.selection.removeListener (this);
    if (playingPreview >= 0)
        ctx.engine.addLiveMidi (juce::MidiMessage::noteOff (1, playingPreview));
}

//==============================================================================
const Track* PianoRollPanel::getTrack() const
{
    return ctx.song.findTrack (trackId);
}

const Region* PianoRollPanel::getRegion() const
{
    if (auto* t = getTrack())
        if (auto* r = t->findRegion (regionId); r != nullptr && r->kind == Region::Kind::midi)
            return r;
    return nullptr;
}

void PianoRollPanel::songChanged (const SongChange&)
{
    // the region may have moved to another track (dragged in the arrangement)
    if (getRegion() == nullptr && regionId.isValid())
        if (auto* t = ctx.song.findTrackOfRegion (regionId))
            trackId = t->id;

    // (the selection is kept: a region update removes and re-inserts the region,
    //  so it is briefly missing in between)
    updateScrollBars();
    repaint();
}

void PianoRollPanel::selectionChanged()
{
    followSelection();
}

void PianoRollPanel::followSelection()
{
    for (auto id : ctx.selection.getSelectedRegions())
    {
        if (auto* t = ctx.song.findTrackOfRegion (id))
            if (auto* r = t->findRegion (id); r != nullptr && r->kind == Region::Kind::midi)
            {
                if (id != regionId)
                {
                    regionId = id;
                    trackId = t->id;
                    selected.clear();
                    showRegionInView();
                }
                break;
            }
    }

    if (auto* r = getRegion())
        title.setText (r->name.isNotEmpty() ? r->name + juce::String (u8"  –  ") + getTrack()->name : getTrack()->name,
                       juce::dontSendNotification);
    else
        title.setText ("Editor", juce::dontSendNotification);

    updateScrollBars();
    repaint();
}

void PianoRollPanel::commit (std::vector<Note> notes, const juce::String& name, bool merge)
{
    auto* r = getRegion();
    if (r == nullptr)
        return;

    auto edited = *r;
    noteops::sort (notes);
    edited.notes = std::move (notes);

    // notes beyond the end: make the part longer (whole bars)
    Tick maxEnd = 0;
    for (auto& n : edited.notes)
        maxEnd = std::max (maxEnd, n.start + n.length);
    if (maxEnd > edited.length)
    {
        const auto bar = ctx.song.getTempoMap().getTicksPerBar();
        edited.length = ((maxEnd + bar - 1) / bar) * bar;
    }

    ctx.perform (std::make_unique<commands::UpdateRegion> (trackId, trackId, std::move (edited), name), merge);
}

//==============================================================================
juce::Rectangle<int> PianoRollPanel::keyboardArea() const
{
    return { 0, headerHeight + rulerHeight, keyboardWidth, getHeight() - headerHeight - rulerHeight - velocityHeight - scrollSize };
}

juce::Rectangle<int> PianoRollPanel::rulerArea() const
{
    return { keyboardWidth, headerHeight, getWidth() - keyboardWidth - scrollSize, rulerHeight };
}

juce::Rectangle<int> PianoRollPanel::gridArea() const
{
    auto k = keyboardArea();
    return { keyboardWidth, k.getY(), getWidth() - keyboardWidth - scrollSize, k.getHeight() };
}

juce::Rectangle<int> PianoRollPanel::velocityArea() const
{
    auto g = gridArea();
    return { g.getX(), g.getBottom(), g.getWidth(), velocityHeight };
}

float PianoRollPanel::tickToX (Tick t) const
{
    return (float) gridArea().getX() + (float) (((double) t - scrollTicks) * pixelsPerTick);
}

Tick PianoRollPanel::xToTick (float x) const
{
    return (Tick) std::llround (scrollTicks + (double) (x - (float) gridArea().getX()) / pixelsPerTick);
}

float PianoRollPanel::pitchToY (int pitch) const
{
    return (float) gridArea().getY() + (float) (127 - pitch) * noteHeight - scrollY;
}

int PianoRollPanel::yToPitch (float y) const
{
    return juce::jlimit (0, 127, 127 - (int) std::floor ((y - (float) gridArea().getY() + scrollY) / noteHeight));
}

juce::Rectangle<float> PianoRollPanel::noteBounds (const Note& n, Tick regionPos) const
{
    const auto x1 = tickToX (regionPos + n.start);
    const auto x2 = tickToX (regionPos + n.start + n.length);
    return { x1, pitchToY (n.pitch) + 1.0f, std::max (3.0f, x2 - x1), noteHeight - 1.0f };
}

Tick PianoRollPanel::gridTicks() const
{
    const auto choice = gridChoices[(size_t) juce::jlimit (0, (int) std::size (gridChoices) - 1, gridBox.getSelectedItemIndex())].ticks;
    if (choice > 0)
        return choice;

    // automatic: the finest grid whose lines are at least ~12 px apart
    const auto& map = ctx.song.getTempoMap();
    for (Tick c : { ticksPerQuarter / 8, ticksPerQuarter / 4, ticksPerQuarter / 2, map.getTicksPerBeat(), map.getTicksPerBar() })
        if ((double) c * pixelsPerTick >= 12.0)
            return c;
    return map.getTicksPerBar();
}

void PianoRollPanel::updateScrollBars()
{
    const auto g = gridArea();
    if (g.isEmpty())
        return;

    // horizontal: the region plus some room, at least the visible width
    Tick start = 0, end = ctx.song.getTempoMap().getTicksPerBar() * 8;
    if (auto* r = getRegion())
    {
        start = std::max<Tick> (0, r->position - ctx.song.getTempoMap().getTicksPerBar());
        end = r->getEnd() + ctx.song.getTempoMap().getTicksPerBar() * 2;
    }
    const auto visible = (double) g.getWidth() / pixelsPerTick;
    const auto rangeStart = std::min ((double) start, scrollTicks);
    const auto rangeEnd = std::max ((double) end, scrollTicks + visible);
    hScroll.setRangeLimits (rangeStart, rangeEnd, juce::dontSendNotification);
    hScroll.setCurrentRange (scrollTicks, visible, juce::dontSendNotification);

    const auto total = 128.0 * noteHeight;
    scrollY = juce::jlimit (0.0f, (float) std::max (0.0, total - g.getHeight()), scrollY);
    vScroll.setRangeLimits (0.0, total, juce::dontSendNotification);
    vScroll.setCurrentRange (scrollY, g.getHeight(), juce::dontSendNotification);
}

void PianoRollPanel::scrollBarMoved (juce::ScrollBar* sb, double start)
{
    if (sb == &hScroll)
        scrollTicks = start;
    else
        scrollY = (float) start;
    repaint();
}

void PianoRollPanel::showRegionInView()
{
    auto* r = getRegion();
    const auto g = gridArea();
    if (r == nullptr || g.getWidth() <= 0)
        return;

    // whole part visible (zoom), starting a little before it
    const auto length = std::max<Tick> (r->length, ctx.song.getTempoMap().getTicksPerBar());
    pixelsPerTick = juce::jlimit (0.01, 2.0, (double) (g.getWidth() - 40) / (double) length);
    scrollTicks = std::max (0.0, (double) r->position - 20.0 / pixelsPerTick);

    // centre the notes vertically (or middle C)
    int lo = 60, hi = 72;
    if (! r->notes.empty())
    {
        lo = 127; hi = 0;
        for (auto& n : r->notes) { lo = std::min (lo, n.pitch); hi = std::max (hi, n.pitch); }
    }
    const auto centre = (lo + hi) / 2;
    scrollY = (float) (127 - centre) * noteHeight - (float) g.getHeight() * 0.5f;
    updateScrollBars();
}

//==============================================================================
void PianoRollPanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    // header
    g.setColour (colours::panelLight);
    g.fillRect (0, 0, getWidth(), headerHeight);
    g.setColour (colours::border);
    g.drawHorizontalLine (headerHeight - 1, 0.0f, (float) getWidth());

    if (getRegion() == nullptr)
    {
        g.setColour (colours::textDim);
        g.setFont (font (15.0f));
        g.drawText (u8"Wähle einen MIDI-Part im Arrangement (Doppelklick öffnet ihn hier).",
                    getLocalBounds().withTrimmedTop (headerHeight), juce::Justification::centred);
        return;
    }

    paintRuler (g);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (gridArea());
        paintGrid (g);
        paintNotes (g);
    }
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (keyboardArea());
        paintKeyboard (g);
    }
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (velocityArea().withLeft (0));
        paintVelocity (g);
    }

    // playhead
    const auto& map = ctx.song.getTempoMap();
    const auto x = tickToX (map.secondsToTicks (ctx.engine.getPositionSeconds()));
    if (x >= (float) gridArea().getX() && x <= (float) gridArea().getRight())
    {
        g.setColour (colours::playhead);
        g.drawVerticalLine ((int) x, (float) rulerArea().getY(), (float) velocityArea().getBottom());
    }

    // rubber band
    if (dragMode == DragMode::select && ! rubberBand.isEmpty())
    {
        g.setColour (colours::accent.withAlpha (0.15f));
        g.fillRect (rubberBand);
        g.setColour (colours::accent);
        g.drawRect (rubberBand, 1.0f);
    }
}

void PianoRollPanel::paintRuler (juce::Graphics& g)
{
    const auto r = rulerArea();
    g.setColour (colours::panelDark);
    g.fillRect (r);
    g.fillRect (0, r.getY(), keyboardWidth, r.getHeight());

    const auto& map = ctx.song.getTempoMap();
    const auto bar = map.getTicksPerBar();
    const auto first = std::max<Tick> (0, (Tick) (scrollTicks / (double) bar) * bar);
    const auto last = xToTick ((float) r.getRight());
    const auto barPx = (double) bar * pixelsPerTick;
    const int step = barPx < 30 ? 4 : (barPx < 60 ? 2 : 1);

    g.setFont (font (11.5f));
    for (auto t = first; t <= last; t += bar)
    {
        const auto barIndex = (int) (t / bar);
        const auto x = tickToX (t);
        g.setColour (colours::gridBar);
        g.drawVerticalLine ((int) x, (float) r.getY() + (barIndex % step == 0 ? 4.0f : 12.0f), (float) r.getBottom());
        if (barIndex % step == 0)
        {
            g.setColour (colours::textDim);
            g.drawText (juce::String (barIndex + 1), (int) x + 3, r.getY(), 40, r.getHeight(), juce::Justification::centredLeft);
        }
    }

    // region range
    if (auto* reg = getRegion())
    {
        const auto x1 = tickToX (reg->position), x2 = tickToX (reg->getEnd());
        g.setColour ((getTrack() != nullptr ? getTrack()->colour : colours::accent).withAlpha (0.6f));
        g.fillRect (juce::Rectangle<float> (x1, (float) r.getBottom() - 4.0f, x2 - x1, 4.0f));
    }

    g.setColour (colours::border);
    g.drawHorizontalLine (r.getBottom() - 1, 0.0f, (float) r.getRight());
}

void PianoRollPanel::paintKeyboard (juce::Graphics& g)
{
    const auto k = keyboardArea();
    g.setColour (colours::panelDark);
    g.fillRect (k);

    const auto highlightPitch = lastPreviewPitch;
    for (int p = 0; p < 128; ++p)
    {
        const auto y = pitchToY (p);
        if (y > (float) k.getBottom() || y + noteHeight < (float) k.getY())
            continue;

        auto key = juce::Rectangle<float> ((float) k.getX(), y, (float) k.getWidth(), noteHeight);
        const bool black = isBlackKey (p);
        g.setColour (p == highlightPitch && dragMode != DragMode::none ? colours::accent
                     : black ? colours::window : colours::text.withAlpha (0.82f));
        g.fillRect (black ? key.withWidth ((float) k.getWidth() * 0.62f) : key);
        if (black)
        {
            g.setColour (colours::text.withAlpha (0.82f));
            g.fillRect (key.withTrimmedLeft ((float) k.getWidth() * 0.62f));
        }
        g.setColour (colours::panelDark);
        g.drawHorizontalLine ((int) (y + noteHeight), key.getX(), key.getRight());

        if (p % 12 == 0 && noteHeight >= 9.0f)
        {
            g.setColour (colours::textDark);
            g.setFont (font (std::min (11.0f, noteHeight - 1.0f)));
            g.drawText (pitchName (p), key.withTrimmedRight (3.0f), juce::Justification::centredRight);
        }
    }

    g.setColour (colours::border);
    g.drawVerticalLine (k.getRight() - 1, (float) k.getY(), (float) k.getBottom());
}

void PianoRollPanel::paintGrid (juce::Graphics& g)
{
    const auto area = gridArea();

    // rows: black keys darker
    for (int p = 0; p < 128; ++p)
    {
        const auto y = pitchToY (p);
        if (y > (float) area.getBottom() || y + noteHeight < (float) area.getY())
            continue;
        g.setColour (isBlackKey (p) ? colours::laneB : colours::laneA);
        g.fillRect ((float) area.getX(), y, (float) area.getWidth(), noteHeight);
        if (p % 12 == 0)
        {
            g.setColour (colours::gridBar);
            g.drawHorizontalLine ((int) (y + noteHeight), (float) area.getX(), (float) area.getRight());
        }
    }

    // outside the part: darker
    if (auto* r = getRegion())
    {
        const auto x1 = tickToX (r->position), x2 = tickToX (r->getEnd());
        g.setColour (colours::window.withAlpha (0.45f));
        if (x1 > (float) area.getX())
            g.fillRect ((float) area.getX(), (float) area.getY(), x1 - (float) area.getX(), (float) area.getHeight());
        if (x2 < (float) area.getRight())
            g.fillRect (x2, (float) area.getY(), (float) area.getRight() - x2, (float) area.getHeight());
    }

    // vertical grid lines
    const auto& map = ctx.song.getTempoMap();
    const auto grid = gridTicks();
    const auto bar = map.getTicksPerBar(), beat = map.getTicksPerBeat();
    const auto lineStep = (double) grid * pixelsPerTick >= 5.0 ? grid : beat;
    const auto first = std::max<Tick> (0, (Tick) (scrollTicks / (double) lineStep) * lineStep);
    const auto last = xToTick ((float) area.getRight());
    for (auto t = first; t <= last; t += lineStep)
    {
        g.setColour (t % bar == 0 ? colours::gridBar : (t % beat == 0 ? colours::gridBeat.brighter (0.15f) : colours::gridBeat));
        g.drawVerticalLine ((int) tickToX (t), (float) area.getY(), (float) area.getBottom());
    }
}

void PianoRollPanel::paintNotes (juce::Graphics& g)
{
    auto* r = getRegion();
    auto* t = getTrack();
    if (r == nullptr || t == nullptr)
        return;

    const auto base = t->colour;
    const auto area = gridArea().toFloat();

    for (auto& n : r->notes)
    {
        auto b = noteBounds (n, r->position);
        if (! b.intersects (area))
            continue;

        const bool sel = selected.count (n.id) > 0;
        const auto fill = sel ? colours::accent : base.withMultipliedBrightness (0.55f + 0.45f * n.velocity);
        g.setColour (fill);
        g.fillRoundedRectangle (b, 2.0f);
        g.setColour (sel ? colours::text : base.darker (0.6f));
        g.drawRoundedRectangle (b.reduced (0.5f), 2.0f, sel ? 1.5f : 1.0f);

        if (b.getWidth() > 34.0f && noteHeight >= 11.0f)
        {
            g.setColour (sel ? colours::textDark : colours::textDark.withAlpha (0.8f));
            g.setFont (font (std::min (11.0f, noteHeight - 2.0f)));
            g.drawText (pitchName (n.pitch), b.reduced (3.0f, 0.0f), juce::Justification::centredLeft, false);
        }
    }
}

void PianoRollPanel::paintVelocity (juce::Graphics& g)
{
    const auto v = velocityArea();
    g.setColour (colours::panelDark);
    g.fillRect (v.withLeft (0));
    g.setColour (colours::border);
    g.drawHorizontalLine (v.getY(), 0.0f, (float) getWidth());

    g.setColour (colours::textDim);
    g.setFont (font (11.5f));
    g.drawText ("Anschlag", 6, v.getY() + 4, keyboardWidth - 8, 16, juce::Justification::centredLeft);

    auto* r = getRegion();
    auto* t = getTrack();
    if (r == nullptr || t == nullptr)
        return;

    const auto lane = v.reduced (0, 8).toFloat();
    g.setColour (colours::gridBeat);
    g.drawHorizontalLine ((int) lane.getY(), lane.getX(), lane.getRight());
    g.drawHorizontalLine ((int) lane.getCentreY(), lane.getX(), lane.getRight());

    for (auto& n : r->notes)
    {
        const auto x = tickToX (r->position + n.start);
        if (x < lane.getX() - 4.0f || x > lane.getRight())
            continue;
        const bool sel = selected.count (n.id) > 0;
        const auto top = lane.getBottom() - lane.getHeight() * n.velocity;
        g.setColour (sel ? colours::accent : t->colour);
        g.fillRect (juce::Rectangle<float> (x, top, 3.0f, lane.getBottom() - top));
        g.fillEllipse (x - 2.0f, top - 3.0f, 7.0f, 7.0f);
    }
}

void PianoRollPanel::resized()
{
    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (8, 4);
    title.setBounds (header.removeFromLeft (260));
    gridBox.setBounds (header.removeFromLeft (180).reduced (0, 1));
    header.removeFromLeft (8);
    quantizeButton.setBounds (header.removeFromLeft (120).reduced (0, 1));
    header.removeFromLeft (8);
    hint.setBounds (header.withTrimmedRight (30)); // room for the panel's detach button

    velocityHeight = juce::jlimit (50, 140, getHeight() / 5);
    const auto g = gridArea();
    vScroll.setBounds (g.getRight(), g.getY(), scrollSize, g.getHeight());
    hScroll.setBounds (g.getX(), getHeight() - scrollSize, g.getWidth(), scrollSize);

    if (getRegion() != nullptr && ! std::exchange (initialised, true))
        showRegionInView();
    updateScrollBars();
}

//==============================================================================
const Note* PianoRollPanel::noteAt (juce::Point<float> p) const
{
    auto* r = getRegion();
    if (r == nullptr)
        return nullptr;

    // topmost (last drawn) first
    for (auto it = r->notes.rbegin(); it != r->notes.rend(); ++it)
        if (noteBounds (*it, r->position).expanded (0.0f, 0.5f).contains (p))
            return &*it;
    return nullptr;
}

void PianoRollPanel::preview (int pitch, float velocity)
{
    if (pitch == playingPreview)
        return;
    if (playingPreview >= 0)
        ctx.engine.addLiveMidi (juce::MidiMessage::noteOff (1, playingPreview));
    playingPreview = pitch;
    if (pitch >= 0)
    {
        ctx.engine.setLiveMidiTrack (trackId);
        ctx.engine.addLiveMidi (juce::MidiMessage::noteOn (1, pitch, juce::jlimit (0.05f, 1.0f, velocity)));
    }
}

void PianoRollPanel::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    auto* r = getRegion();
    if (r == nullptr)
        return;

    const auto pos = e.position;
    dragStart = pos;
    dragStartNotes = r->notes;
    selectionAtDragStart = selected;
    dragMode = DragMode::none;
    rubberBand = {};

    // splitter between grid and velocity lane: not resizable for now
    if (keyboardArea().contains (pos.toInt()))
    {
        dragMode = DragMode::keyboard;
        lastPreviewPitch = yToPitch (pos.y);
        preview (lastPreviewPitch);
        repaint();
        return;
    }

    if (velocityArea().contains (pos.toInt()))
    {
        dragMode = DragMode::velocity;
        setVelocityAt (pos, ! selected.empty());
        return;
    }

    if (! gridArea().contains (pos.toInt()))
        return;

    const auto tool = (int) ctx.edit.tool.getValue();
    auto* hit = noteAt (pos);

    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem (1, "Quantisieren", true);
        m.addItem (2, u8"Löschen", ! selected.empty() || hit != nullptr);
        m.addItem (3, u8"Alles auswählen");
        if (hit != nullptr && selected.count (hit->id) == 0)
            selected = { hit->id };
        repaint();
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                         [safe = juce::Component::SafePointer<PianoRollPanel> (this)] (int res)
                         {
                             if (safe == nullptr) return;
                             if (res == 1) safe->quantizeSelection();
                             if (res == 2) safe->deleteSelection();
                             if (res == 3) safe->selectAll();
                         });
        return;
    }

    if (tool == EditState::eraser)
    {
        dragMode = DragMode::erase;
        if (hit != nullptr)
            commit (noteops::remove (r->notes, { hit->id }), u8"Noten löschen", true);
        return;
    }

    if (hit != nullptr)
    {
        if (e.mods.isShiftDown())
        {
            if (! selected.erase (hit->id))
                selected.insert (hit->id);
        }
        else if (selected.count (hit->id) == 0)
        {
            selected = { hit->id };
        }

        dragNote = hit->id;
        dragNoteStartTick = r->position + hit->start;
        const auto b = noteBounds (*hit, r->position);
        dragMode = pos.x > b.getRight() - edgeZone ? DragMode::resize : DragMode::move;
        newNoteVelocity = hit->velocity;
        lastPreviewPitch = hit->pitch;
        preview (hit->pitch, hit->velocity);
        repaint();
        return;
    }

    if (tool == EditState::pencil)
    {
        // draw a new note; dragging sets its length
        const auto grid = gridTicks();
        const auto start = e.mods.isAltDown() ? xToTick (pos.x) : noteops::snapDown (xToTick (pos.x), grid);
        Note n;
        n.id = ctx.song.newId();
        n.start = std::max<Tick> (0, start - r->position);
        n.length = std::max<Tick> (grid, 30);
        n.pitch = yToPitch (pos.y);
        n.velocity = newNoteVelocity;
        auto notes = r->notes;
        notes.push_back (n);
        selected = { n.id };
        dragNote = n.id;
        dragStartNotes = notes;
        commit (std::move (notes), u8"Note zeichnen", true);
        dragMode = DragMode::create;
        lastPreviewPitch = n.pitch;
        preview (n.pitch, n.velocity);
        return;
    }

    // empty space with the arrow: rubber-band selection
    if (! e.mods.isShiftDown())
        selected.clear();
    dragMode = DragMode::select;
    repaint();
}

void PianoRollPanel::mouseDrag (const juce::MouseEvent& e)
{
    auto* r = getRegion();
    if (r == nullptr)
        return;

    const auto pos = e.position;
    const auto grid = e.mods.isAltDown() ? (Tick) 1 : gridTicks();

    switch (dragMode)
    {
        case DragMode::keyboard:
        {
            const auto p = yToPitch (pos.y);
            if (p != lastPreviewPitch)
            {
                lastPreviewPitch = p;
                preview (p);
                repaint();
            }
            break;
        }

        case DragMode::velocity:
            setVelocityAt (pos, ! selected.empty());
            break;

        case DragMode::move:
        {
            const auto rawDelta = xToTick (pos.x) - xToTick (dragStart.x);
            // the grabbed note lands on the grid; the others keep their distance
            const auto target = noteops::snap (dragNoteStartTick + rawDelta, grid);
            const auto dTicks = target - dragNoteStartTick;
            const auto dPitch = yToPitch (pos.y) - yToPitch (dragStart.y);
            const bool copy = e.mods.isCommandDown() || e.mods.isCtrlDown();

            std::vector<Note> notes;
            if (copy)
            {
                // Ctrl-drag: copies (originals stay)
                notes = dragStartNotes;
                std::set<ObjectId> copies;
                for (auto& n : dragStartNotes)
                    if (selected.count (n.id) > 0 || selectionAtDragStart.count (n.id) > 0)
                    {
                        auto c = n;
                        c.id = ctx.song.newId();
                        c.start = std::max<Tick> (0, c.start + dTicks);
                        c.pitch = juce::jlimit (0, 127, c.pitch + dPitch);
                        notes.push_back (c);
                    }
            }
            else
            {
                notes = noteops::move (dragStartNotes, selected, dTicks, dPitch);
            }
            commit (std::move (notes), copy ? u8"Noten kopieren" : u8"Noten verschieben", true);

            if (auto* moved = getRegion()->findNote (dragNote); moved != nullptr && moved->pitch != lastPreviewPitch)
            {
                lastPreviewPitch = moved->pitch;
                preview (moved->pitch, moved->velocity);
            }
            break;
        }

        case DragMode::resize:
        case DragMode::create:
        {
            const Note* original = nullptr;
            for (auto& n : dragStartNotes)
                if (n.id == dragNote)
                    original = &n;
            if (original == nullptr)
                break;
            const auto endAbs = noteops::snap (xToTick (pos.x), grid);
            const auto newLen = std::max<Tick> (std::max<Tick> (grid, 15), endAbs - (r->position + original->start));
            commit (noteops::resize (dragStartNotes, dragMode == DragMode::create ? std::set<ObjectId> { dragNote } : selected,
                                     newLen - original->length, std::max<Tick> (grid, 15)),
                    dragMode == DragMode::create ? juce::String (u8"Note zeichnen") : juce::String (u8"Notenlänge ändern"), true);
            break;
        }

        case DragMode::erase:
            if (auto* hit = noteAt (pos))
                commit (noteops::remove (r->notes, { hit->id }), u8"Noten löschen", true);
            break;

        case DragMode::select:
        {
            rubberBand = juce::Rectangle<float> (dragStart, pos);
            selected = selectionAtDragStart;
            for (auto& n : r->notes)
                if (noteBounds (n, r->position).intersects (rubberBand))
                    selected.insert (n.id);
            repaint();
            break;
        }

        case DragMode::none:
            break;
    }
}

void PianoRollPanel::mouseUp (const juce::MouseEvent&)
{
    ctx.history.endGesture();
    preview (-1);
    if (dragMode == DragMode::keyboard)
        lastPreviewPitch = -1;
    dragMode = DragMode::none;
    rubberBand = {};
    repaint();
}

void PianoRollPanel::mouseMove (const juce::MouseEvent& e)
{
    auto newCursor = juce::MouseCursor::NormalCursor;
    if (gridArea().contains (e.getPosition()))
    {
        const auto tool = (int) ctx.edit.tool.getValue();
        if (auto* r = getRegion(); r != nullptr)
            if (auto* hit = noteAt (e.position))
                newCursor = e.position.x > noteBounds (*hit, r->position).getRight() - edgeZone ? juce::MouseCursor::LeftRightResizeCursor
                                                                                             : juce::MouseCursor::DraggingHandCursor;
        if (tool == EditState::pencil && newCursor == juce::MouseCursor::NormalCursor)
            newCursor = juce::MouseCursor::CrosshairCursor;
    }
    else if (velocityArea().contains (e.getPosition()))
    {
        newCursor = juce::MouseCursor::UpDownResizeCursor;
    }
    setMouseCursor (newCursor);
}

void PianoRollPanel::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto* r = getRegion();
    if (r == nullptr || ! gridArea().contains (e.getPosition()) || (int) ctx.edit.tool.getValue() == EditState::pencil)
        return;

    // double-click: on a note deletes it, on empty space adds one
    if (auto* hit = noteAt (e.position))
    {
        selected.erase (hit->id);
        commit (noteops::remove (r->notes, { hit->id }), u8"Note löschen", false);
        return;
    }

    const auto grid = gridTicks();
    Note n;
    n.id = ctx.song.newId();
    n.start = std::max<Tick> (0, noteops::snapDown (xToTick (e.position.x), grid) - r->position);
    n.length = std::max<Tick> (grid, 30);
    n.pitch = yToPitch (e.position.y);
    n.velocity = newNoteVelocity;
    auto notes = r->notes;
    notes.push_back (n);
    selected = { n.id };
    commit (std::move (notes), u8"Note setzen", false);
    preview (n.pitch, n.velocity);
    juce::Timer::callAfterDelay (200, [safe = juce::Component::SafePointer<PianoRollPanel> (this)] { if (safe != nullptr) safe->preview (-1); });
}

void PianoRollPanel::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        // zoom around the mouse
        const auto anchor = xToTick (e.position.x);
        pixelsPerTick = juce::jlimit (0.005, 3.0, pixelsPerTick * std::pow (1.25, (double) w.deltaY * 4.0));
        scrollTicks = std::max (0.0, (double) anchor - (double) (e.position.x - (float) gridArea().getX()) / pixelsPerTick);
    }
    else if (e.mods.isAltDown())
    {
        const auto pitch = yToPitch (e.position.y);
        noteHeight = juce::jlimit (6.0f, 30.0f, noteHeight * (w.deltaY > 0 ? 1.15f : 1.0f / 1.15f));
        scrollY = (float) (127 - pitch) * noteHeight - (e.position.y - (float) gridArea().getY());
    }
    else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        const auto d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
        scrollTicks = std::max (0.0, scrollTicks - (double) d * 400.0 / pixelsPerTick);
    }
    else
    {
        scrollY -= w.deltaY * noteHeight * 12.0f;
    }

    updateScrollBars();
    repaint();
}

bool PianoRollPanel::keyPressed (const juce::KeyPress& k)
{
    if (getRegion() == nullptr)
        return false;

    const auto code = k.getKeyCode();
    const auto shift = k.getModifiers().isShiftDown();

    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        deleteSelection();
        return true;
    }
    if (code == 'A' && k.getModifiers().isCommandDown())
    {
        selectAll();
        return true;
    }
    if (code == 'Q' && ! k.getModifiers().isCommandDown())
    {
        quantizeSelection();
        return true;
    }
    if (! selected.empty())
    {
        if (code == juce::KeyPress::upKey)    { nudge (0, shift ? 12 : 1);  return true; }
        if (code == juce::KeyPress::downKey)  { nudge (0, shift ? -12 : -1); return true; }
        if (code == juce::KeyPress::leftKey)  { nudge (-gridTicks(), 0);    return true; }
        if (code == juce::KeyPress::rightKey) { nudge (gridTicks(), 0);     return true; }
    }
    if (code == juce::KeyPress::escapeKey && ! selected.empty())
    {
        selected.clear();
        repaint();
        return true;
    }
    return false; // everything else (play, undo ...) goes to the main window
}

void PianoRollPanel::quantizeSelection()
{
    if (auto* r = getRegion())
        commit (noteops::quantize (r->notes, selected, r->position, gridTicks()), "Quantisieren", false);
}

void PianoRollPanel::deleteSelection()
{
    if (auto* r = getRegion(); r != nullptr && ! selected.empty())
    {
        commit (noteops::remove (r->notes, selected), u8"Noten löschen", false);
        selected.clear();
    }
}

void PianoRollPanel::selectAll()
{
    selected.clear();
    if (auto* r = getRegion())
        for (auto& n : r->notes)
            selected.insert (n.id);
    repaint();
}

void PianoRollPanel::nudge (Tick dTicks, int dPitch)
{
    if (auto* r = getRegion())
    {
        commit (noteops::move (r->notes, selected, dTicks, dPitch), u8"Noten verschieben", false);
        if (dPitch != 0)
            if (auto* n = getRegion()->findNote (*selected.begin()))
            {
                preview (n->pitch, n->velocity);
                juce::Timer::callAfterDelay (180, [safe = juce::Component::SafePointer<PianoRollPanel> (this)] { if (safe != nullptr) safe->preview (-1); });
            }
    }
}

void PianoRollPanel::setVelocityAt (juce::Point<float> p, bool onlySelected)
{
    auto* r = getRegion();
    if (r == nullptr)
        return;

    const auto lane = velocityArea().reduced (0, 8).toFloat();
    const auto vel = juce::jlimit (1.0f / 127.0f, 1.0f, (lane.getBottom() - p.y) / lane.getHeight());

    // all (selected) notes whose stem is near the mouse
    std::set<ObjectId> ids;
    for (auto& n : r->notes)
        if (std::abs (tickToX (r->position + n.start) - p.x) <= 5.0f && (! onlySelected || selected.count (n.id) > 0))
            ids.insert (n.id);

    // a selected note was hit: change all selected together
    if (onlySelected && ! ids.empty())
        ids = selected;

    if (! ids.empty())
    {
        newNoteVelocity = vel;
        commit (noteops::setVelocity (r->notes, ids, vel), "Anschlag", true);
    }
}

void PianoRollPanel::timerCallback()
{
    const auto now = ctx.engine.getPositionSeconds();
    if (! juce::approximatelyEqual (now, lastPlayhead))
    {
        lastPlayhead = now;
        repaint (gridArea().withTop (rulerArea().getY()).withBottom (velocityArea().getBottom()));
    }
}

} // namespace sms
