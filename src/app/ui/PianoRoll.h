// SPDX-License-Identifier: AGPL-3.0-or-later
// The note editor ("Editor", F2): piano roll for the selected MIDI part.
#pragma once

#include "extension/ExtensionApi.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <set>

namespace sms
{

class PianoRollPanel final : public juce::Component,
                             private Song::Listener,
                             private Selection::Listener,
                             private juce::ScrollBar::Listener,
                             private juce::Timer
{
public:
    explicit PianoRollPanel (AppContext&);
    ~PianoRollPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    // --- model access -------------------------------------------------------
    const Track* getTrack() const;
    const Region* getRegion() const;
    void songChanged (const SongChange&) override;
    void selectionChanged() override;
    void followSelection();
    void commit (std::vector<Note> notes, const juce::String& name, bool merge);

    // --- geometry -----------------------------------------------------------
    juce::Rectangle<int> gridArea() const;
    juce::Rectangle<int> keyboardArea() const;
    juce::Rectangle<int> rulerArea() const;
    juce::Rectangle<int> velocityArea() const;
    float tickToX (Tick songTick) const;
    Tick xToTick (float x) const;
    float pitchToY (int pitch) const;
    int yToPitch (float y) const;
    juce::Rectangle<float> noteBounds (const Note&, Tick regionPos) const;
    Tick gridTicks() const;
    void updateScrollBars();
    void scrollBarMoved (juce::ScrollBar*, double) override;
    void showRegionInView();

    // --- painting -----------------------------------------------------------
    void paintKeyboard (juce::Graphics&);
    void paintRuler (juce::Graphics&);
    void paintGrid (juce::Graphics&);
    void paintNotes (juce::Graphics&);
    void paintVelocity (juce::Graphics&);

    // --- editing ------------------------------------------------------------
    enum class DragMode { none, move, resize, select, create, erase, velocity, keyboard };
    const Note* noteAt (juce::Point<float>) const;
    void preview (int pitch, float velocity = 0.8f);
    void quantizeSelection();
    void deleteSelection();
    void selectAll();
    void nudge (Tick dTicks, int dPitch);
    void setVelocityAt (juce::Point<float>, bool onlySelected);
    void timerCallback() override;

    AppContext& ctx;
    ObjectId regionId, trackId;
    std::set<ObjectId> selected;

    // view
    double pixelsPerTick = 0.12;
    double scrollTicks = 0.0;   // song tick at the left edge of the grid
    float noteHeight = 14.0f;
    float scrollY = 0.0f;       // pixels from pitch 127 down
    int velocityHeight = 90;

    // drag state
    DragMode dragMode = DragMode::none;
    std::vector<Note> dragStartNotes;
    juce::Point<float> dragStart;
    ObjectId dragNote;
    Tick dragNoteStartTick = 0;
    int lastPreviewPitch = -1;
    juce::Rectangle<float> rubberBand;
    std::set<ObjectId> selectionAtDragStart;
    float newNoteVelocity = 0.8f;
    int playingPreview = -1;
    double lastPlayhead = -1.0;
    bool initialised = false;

    // header
    juce::Label title;
    juce::ComboBox gridBox;
    juce::TextButton quantizeButton { "Quantisieren" };
    juce::Label hint;
    juce::ScrollBar hScroll { false }, vScroll { true };

    static constexpr int headerHeight = 34, keyboardWidth = 64, rulerHeight = 22, scrollSize = 10;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRollPanel)
};

} // namespace sms
