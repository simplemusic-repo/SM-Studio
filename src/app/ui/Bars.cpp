// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Bars.h"

#include "core/Commands.h"
#include "extension/ExtensionRegistry.h"

namespace sms
{

using namespace theme;

//==============================================================================
Toolbar::Toolbar (AppContext& c) : ctx (c)
{
    const std::pair<juce::String, juce::Path> toolDefs[] = {
        { "Pfeil (1)", icons::arrowTool() },
        { "Bereich (2)", icons::rangeTool() },
        { "Teilen (3)", icons::splitTool() },
        { "Radiergummi (4)", icons::eraserTool() },
        { "Stift (5)", icons::pencilTool() },
        { "Stummschalten (6)", icons::muteTool() },
    };

    for (int i = 0; i < (int) std::size (toolDefs); ++i)
    {
        auto* b = tools.add (new IconButton (toolDefs[i].first, toolDefs[i].second));
        b->setClickingTogglesState (false);
        b->setWantsKeyboardFocus (false);
        b->onClick = [this, i] { ctx.edit.tool = i; };
        addAndMakeVisible (b);
    }

    for (auto* b : { &undoButton, &redoButton })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (*b);
    }
    undoButton.onClick = [this] { ctx.registry.invoke ("edit.undo"); };
    redoButton.onClick = [this] { ctx.registry.invoke ("edit.redo"); };

    snapLabel.setText ("Raster", juce::dontSendNotification);
    snapLabel.setFont (font (12.5f));
    snapLabel.setColour (juce::Label::textColourId, colours::textDim);
    snapLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (snapLabel);

    snap.addItem ("Aus", EditState::snapOff + 1);
    snap.addItem ("Takt", EditState::snapBar + 1);
    snap.addItem (u8"Zählzeit", EditState::snapBeat + 1);
    snap.addItem ("1/8", EditState::snap8th + 1);
    snap.addItem ("1/16", EditState::snap16th + 1);
    snap.setSelectedId ((int) ctx.edit.snap.getValue() + 1, juce::dontSendNotification);
    snap.onChange = [this] { ctx.edit.snap = snap.getSelectedId() - 1; };
    snap.setWantsKeyboardFocus (false);
    addAndMakeVisible (snap);

    ctx.history.addListener (this);
    ctx.edit.tool.addListener (this);
    ctx.edit.snap.addListener (this);
    historyChanged();
    valueChanged (ctx.edit.tool);
}

Toolbar::~Toolbar()
{
    ctx.history.removeListener (this);
    ctx.edit.tool.removeListener (this);
    ctx.edit.snap.removeListener (this);
}

void Toolbar::historyChanged()
{
    undoButton.setEnabled (ctx.history.canUndo());
    redoButton.setEnabled (ctx.history.canRedo());
    undoButton.setTooltip (ctx.history.canUndo() ? juce::String (u8"Rückgängig: ") + ctx.history.getUndoName() : u8"Rückgängig");
    redoButton.setTooltip (ctx.history.canRedo() ? "Wiederherstellen: " + ctx.history.getRedoName() : "Wiederherstellen");
}

void Toolbar::valueChanged (juce::Value&)
{
    const int tool = ctx.edit.tool.getValue();
    for (int i = 0; i < tools.size(); ++i)
        tools[i]->setToggleState (i == tool, juce::dontSendNotification);

    snap.setSelectedId ((int) ctx.edit.snap.getValue() + 1, juce::dontSendNotification);
}

void Toolbar::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (colours::panelLight, 0, 0, colours::panel, 0, (float) getHeight(), false));
    g.fillAll();

    // wordmark
    g.setColour (colours::accent);
    g.fillRoundedRectangle (12.0f, 10.0f, 6.0f, (float) getHeight() - 20.0f, 2.0f);
    g.setColour (colours::text);
    g.setFont (font (17.0f, true));
    g.drawText ("SM Studio", 24, 0, 110, getHeight(), juce::Justification::centredLeft);

    g.setColour (colours::border);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void Toolbar::resized()
{
    auto r = getLocalBounds().reduced (8, 7).withTrimmedLeft (130);

    auto toolArea = r.removeFromLeft (tools.size() * 32);
    for (auto* t : tools)
        t->setBounds (toolArea.removeFromLeft (32).reduced (1, 0));

    r.removeFromLeft (16);
    undoButton.setBounds (r.removeFromLeft (30));
    r.removeFromLeft (2);
    redoButton.setBounds (r.removeFromLeft (30));

    r.removeFromLeft (16);
    snapLabel.setBounds (r.removeFromLeft (48));
    r.removeFromLeft (4);
    snap.setBounds (r.removeFromLeft (100));
}

//==============================================================================
TransportBar::TransportBar (AppContext& c, std::function<bool (const juce::String&)> ticked)
    : ctx (c), isTicked (std::move (ticked))
{
    for (auto* b : { &toStart, &rewind, &forward, &stop, &play, &record, &loop, &metronome })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (*b);
    }

    toStart.onClick   = [this] { ctx.registry.invoke ("transport.toStart"); };
    rewind.onClick    = [this] { ctx.registry.invoke ("transport.rewind"); };
    forward.onClick   = [this] { ctx.registry.invoke ("transport.forward"); };
    stop.onClick      = [this] { ctx.registry.invoke ("transport.stop"); };
    play.onClick      = [this] { ctx.registry.invoke ("transport.play"); };
    metronome.onClick = [this] { ctx.registry.invoke ("transport.metronome"); };
    loop.onClick      = [this] { ctx.registry.invoke ("transport.loop"); };
    record.setEnabled (false);

    for (auto* l : { &tempo, &signature })
    {
        l->setFont (font (17.0f, true));
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, colours::lcdText);
        l->setEditable (false, true, false);
        addAndMakeVisible (*l);
    }
    tempo.setTooltip (u8"Tempo – Doppelklick zum Bearbeiten");
    signature.setTooltip (u8"Taktart – Doppelklick zum Bearbeiten");

    tempo.onTextChange = [this]
    {
        const auto bpm = tempo.getText().retainCharacters ("0123456789.,").replaceCharacter (',', '.').getDoubleValue();
        if (bpm > 0)
            ctx.perform (std::make_unique<commands::SetTempo> (bpm));
        updateTempoLabels();
    };

    signature.onTextChange = [this]
    {
        auto parts = juce::StringArray::fromTokens (signature.getText(), "/ ", "");
        parts.removeEmptyStrings();
        if (parts.size() == 2)
            ctx.perform (std::make_unique<commands::SetTimeSignature> (TimeSignature { parts[0].getIntValue(), parts[1].getIntValue() }));
        updateTempoLabels();
    };

    const std::pair<juce::TextButton*, const char*> pages[] = {
        { &inspectorButton, "view.inspector" }, { &editButton, "view.editor" },
        { &mixButton, "view.console" }, { &browseButton, "view.browser" },
    };

    for (auto& [button, action] : pages)
    {
        button->setWantsKeyboardFocus (false);
        juce::String id (action);
        button->onClick = [this, id] { ctx.registry.invoke (id); refreshToggles(); };
        addAndMakeVisible (*button);
    }
    editButton.setTooltip ("Editor (folgt mit der Piano Roll)");

    ctx.song.addListener (this);
    updateTempoLabels();
    refreshToggles();
    timerCallback();
    startTimerHz (25);
}

TransportBar::~TransportBar()
{
    ctx.song.removeListener (this);
}

void TransportBar::refreshToggles()
{
    inspectorButton.setToggleState (isTicked ("view.inspector"), juce::dontSendNotification);
    editButton.setToggleState (isTicked ("view.editor"), juce::dontSendNotification);
    mixButton.setToggleState (isTicked ("view.console"), juce::dontSendNotification);
    browseButton.setToggleState (isTicked ("view.browser"), juce::dontSendNotification);
    metronome.setToggleState (ctx.engine.isMetronomeEnabled(), juce::dontSendNotification);
    loop.setToggleState (ctx.song.getLoop().enabled, juce::dontSendNotification);
}

void TransportBar::songChanged (const SongChange& c)
{
    if (c.what == SongChange::What::tempoChanged || c.what == SongChange::What::reset)
        updateTempoLabels();
    if (c.what == SongChange::What::loopChanged)
        loop.setToggleState (ctx.song.getLoop().enabled, juce::dontSendNotification);
}

void TransportBar::updateTempoLabels()
{
    const auto& map = ctx.song.getTempoMap();
    tempo.setText (juce::String (map.getTempo(), 2), juce::dontSendNotification);
    signature.setText (juce::String (map.getTimeSignature().numerator) + " / " + juce::String (map.getTimeSignature().denominator),
                       juce::dontSendNotification);
}

void TransportBar::timerCallback()
{
    const auto seconds = ctx.engine.getPositionSeconds();
    const auto& map = ctx.song.getTempoMap();
    auto bars = map.ticksToBarsBeats (map.secondsToTicks (seconds)).toString();
    auto secs = formatSeconds (seconds);

    juce::String newInfo;
    if (ctx.engine.hasDevice())
        newInfo = juce::String (ctx.engine.getSampleRate() / 1000.0, 1) + " kHz  |  "
                + juce::String ((double) ctx.engine.getBufferSize() / ctx.engine.getSampleRate() * 1000.0, 1) + " ms  |  CPU "
                + juce::String (juce::roundToInt (ctx.engine.getCpuUsage() * 100.0)) + " %";
    else
        newInfo = u8"Kein Audiogerät";

    if (bars != timeBars || secs != timeSeconds || newInfo != info)
    {
        timeBars = bars;
        timeSeconds = secs;
        info = newInfo;
        repaint (timeArea);
        repaint (infoArea);
    }

    play.setToggleState (ctx.engine.isPlaying(), juce::dontSendNotification);
    metronome.setToggleState (ctx.engine.isMetronomeEnabled(), juce::dontSendNotification);
}

void TransportBar::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (colours::panelLight, 0, 0, colours::panelDark, 0, (float) getHeight(), false));
    g.fillAll();
    g.setColour (colours::border);
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());

    // performance info
    g.setColour (colours::textDim);
    g.setFont (font (12.0f));
    g.drawFittedText (info, infoArea, juce::Justification::centredLeft, 2);

    // time display (LCD look)
    auto lcd = [&g] (juce::Rectangle<int> r)
    {
        g.setColour (colours::lcd);
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
        g.setColour (colours::border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 4.0f, 1.0f);
    };

    lcd (timeArea);
    auto t = timeArea.reduced (10, 4);
    g.setColour (colours::lcdText);
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 22.0f, juce::Font::bold));
    g.drawText (timeBars, t.removeFromTop (t.getHeight() * 3 / 5), juce::Justification::centred);
    g.setColour (colours::lcdText.withAlpha (0.65f));
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    g.drawText (timeSeconds, t, juce::Justification::centred);

    lcd (tempoArea);
    g.setColour (colours::textDim);
    g.setFont (font (10.5f));
    g.drawText ("TEMPO", tempo.getBounds().withY (tempoArea.getY() + 3).withHeight (12), juce::Justification::centred);
    g.drawText ("TAKTART", signature.getBounds().withY (tempoArea.getY() + 3).withHeight (12), juce::Justification::centred);
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced (10, 8);

    // right: page buttons
    auto pages = r.removeFromRight (4 * 78);
    for (auto* b : { &inspectorButton, &editButton, &mixButton, &browseButton })
        b->setBounds (pages.removeFromLeft (78).reduced (2, 6));

    infoArea = r.removeFromLeft (210);

    // centre block: time display, transport buttons, tempo
    const int blockWidth = 230 + 12 + 8 * 38 + 12 + 170;
    auto block = r.withSizeKeepingCentre (juce::jmin (blockWidth, r.getWidth()), r.getHeight());

    timeArea = block.removeFromLeft (230);
    block.removeFromLeft (12);

    auto buttons = block.removeFromLeft (8 * 38);
    for (auto* b : { &toStart, &rewind, &forward, &stop, &play, &record, &loop, &metronome })
        b->setBounds (buttons.removeFromLeft (38).reduced (2, 6));

    block.removeFromLeft (12);
    tempoArea = block.removeFromLeft (170);
    auto tempoInner = tempoArea.reduced (4, 2).withTrimmedTop (12);
    tempo.setBounds (tempoInner.removeFromLeft (tempoInner.getWidth() / 2));
    signature.setBounds (tempoInner);
}

} // namespace sms
