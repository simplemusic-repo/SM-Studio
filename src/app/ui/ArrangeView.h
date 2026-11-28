// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "extension/ExtensionApi.h"
#include "Theme.h"

namespace sms
{

class TrackHeader;

/** The main arrangement: track headers on the left, timeline with events on
    the right, ruler (with loop range) on top.

    Mouse handling depends on the current tool (EditState::tool):
      arrow   select, move (Alt: copy), resize at the edges, rubber band
      split   split events at the click position
      eraser  delete events
      pencil  create an empty MIDI event on instrument tracks
      mute    mute / unmute events

    Accepts drops from the browser and from the OS. */
class ArrangeView final : public juce::Component,
                          public juce::DragAndDropTarget,
                          public juce::FileDragAndDropTarget,
                          private Song::Listener,
                          private Selection::Listener,
                          private juce::ChangeListener,
                          private juce::ScrollBar::Listener,
                          private juce::Timer
{
public:
    explicit ArrangeView (AppContext&);
    ~ArrangeView() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;

    // browser drag & drop
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    // files from Explorer / Finder
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragMove (const juce::StringArray&, int x, int y) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int x, int y) override;

    static constexpr int headerWidth = 236;
    static constexpr int rulerHeight = 34;
    static constexpr int trackHeight = 66;

    void showAddTrackMenu (juce::Component* target);

    // track reordering (called by the track headers)
    void trackDragged (ObjectId, int yInThis);
    void trackDragEnded (ObjectId, int yInThis);

private:
    void songChanged (const SongChange&) override;
    void selectionChanged() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void timerCallback() override;

    void rebuildHeaders();
    void layoutHeaders();
    void updateScrollBars();
    void showContextMenu (Tick mouseTick);

    juce::Rectangle<int> getLaneArea() const;
    juce::Rectangle<int> getRulerArea() const;
    double tickToX (Tick) const;
    Tick xToTick (double x) const;
    int rowAtY (int y) const;
    int rowTop (int row) const;
    DropTarget dropTargetAt (juce::Point<int>) const;
    void setDropIndicator (std::optional<juce::Point<int>>, bool newTrackOnly = false);
    void zoomAround (double factor, int x);

    // events
    struct Hit
    {
        ObjectId trackId;
        int row = -1;
        const Region* region = nullptr;
        enum Zone { none, body, leftEdge, rightEdge } zone = none;
    };
    Hit hitTestEvents (juce::Point<int>) const;
    juce::Rectangle<float> regionBounds (int row, const Region&) const;
    Tick snap (Tick) const;

    void beginEventDrag (const Hit&, const juce::MouseEvent&);
    void updateEventDrag (const juce::MouseEvent&);
    void commitEventDrag (bool copy);
    void applyTool (const Hit&, const juce::MouseEvent&);
    void updateRubberBand();

    void paintRuler (juce::Graphics&, juce::Rectangle<int>);
    void paintLanes (juce::Graphics&, juce::Rectangle<int>);
    void paintRegion (juce::Graphics&, const Track&, const Region&, juce::Rectangle<float>, bool selected, bool ghost);
    void paintWaveform (juce::Graphics&, const Region&, juce::Rectangle<float> body, juce::Colour);

    AppContext& ctx;

    std::unique_ptr<juce::Component> headerColumn;
    std::vector<std::unique_ptr<TrackHeader>> headers;
    theme::IconButton addTrackButton { u8"Spur hinzufügen", theme::icons::plus() };
    juce::ScrollBar hScroll { false }, vScroll { true };

    double pixelsPerTick = 16.0 / (double) ticksPerQuarter; // 16 px per quarter note
    double viewStartTick = 0.0;
    int viewTop = 0;
    double lastPlayheadX = -1.0;

    std::optional<juce::Point<int>> dropPoint;
    bool dropNewTrackOnly = false;

    // current mouse gesture
    enum class Gesture { none, setPlayhead, setLoop, moveEvents, resizeStart, resizeEnd, rubberBand };
    Gesture gesture = Gesture::none;
    struct DraggedRegion { ObjectId trackId; Region original; };
    std::vector<DraggedRegion> dragged;
    std::vector<std::pair<ObjectId, Region>> preview; // target track + new state while dragging
    Region anchor;                                    // the event under the mouse at mouse down
    int anchorRow = 0;
    bool dragMoved = false;
    Tick loopAnchor = 0;
    juce::Rectangle<int> rubberBand;
    std::vector<ObjectId> selectionBeforeRubberBand;

    // track reordering
    std::optional<int> trackDropIndex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangeView)
};

} // namespace sms
