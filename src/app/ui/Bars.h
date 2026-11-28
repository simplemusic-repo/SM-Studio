// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "extension/ExtensionApi.h"
#include "Theme.h"

namespace sms
{

/** Top bar: edit tools, snap, undo/redo. */
class Toolbar final : public juce::Component,
                      private CommandHistory::Listener,
                      private juce::Value::Listener
{
public:
    explicit Toolbar (AppContext&);
    ~Toolbar() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void historyChanged() override;
    void valueChanged (juce::Value&) override;

    AppContext& ctx;
    juce::OwnedArray<theme::IconButton> tools;
    theme::IconButton undoButton { u8"Rückgängig", theme::icons::undo() },
                      redoButton { "Wiederherstellen", theme::icons::redo() };
    juce::ComboBox snap;
    juce::Label snapLabel;
};

//==============================================================================
/** Bottom bar: performance info, time display, transport, tempo, page buttons. */
class TransportBar final : public juce::Component,
                           private Song::Listener,
                           private juce::Timer
{
public:
    /** isPanelVisible / togglePanel connect the Edit/Mix/Browse buttons to the main window. */
    TransportBar (AppContext&, std::function<bool (const juce::String& actionId)> isTicked);
    ~TransportBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void refreshToggles();

private:
    void songChanged (const SongChange&) override;
    void timerCallback() override;
    void updateTempoLabels();

    AppContext& ctx;
    std::function<bool (const juce::String&)> isTicked;

    theme::IconButton toStart { "Zum Anfang", theme::icons::toStart() },
                      rewind { u8"Zurückspulen", theme::icons::rewind() },
                      forward { "Vorspulen", theme::icons::forward() },
                      stop { "Stopp (zweimal: zum Anfang)", theme::icons::stop() },
                      play { "Wiedergabe (Leertaste)", theme::icons::play(), theme::colours::play },
                      record { "Aufnahme (folgt)", theme::icons::record(), theme::colours::record },
                      loop { "Loop (L oder /)", theme::icons::loop() },
                      metronome { "Metronom (C)", theme::icons::metronome() };
    bool metronomeRightClick = false;

    juce::Label tempo, signature;
    juce::TextButton editButton { "Edit" }, mixButton { "Mix" }, browseButton { "Browse" }, inspectorButton { "Inspector" };

    juce::Rectangle<int> infoArea, timeArea, tempoArea;
    juce::String timeBars, timeSeconds, info;
};

} // namespace sms
