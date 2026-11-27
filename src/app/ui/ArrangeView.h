// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "extension/ExtensionApi.h"
#include "Theme.h"

namespace sms
{

class TrackHeader;

/** The main arrangement: track headers on the left, timeline with regions on
    the right, ruler on top. Accepts drops from the browser and from the OS. */
class ArrangeView final : public juce::Component,
                          public juce::DragAndDropTarget,
                          public juce::FileDragAndDropTarget,
                          private Song::Listener,
                          private Selection::Listener,
                          private juce::ScrollBar::Listener,
                          private juce::Timer
{
public:
    explicit ArrangeView (AppContext&);
    ~ArrangeView() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
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
    static constexpr int rulerHeight = 30;
    static constexpr int trackHeight = 66;

    void showAddTrackMenu (juce::Component* target);

private:
    void songChanged (const SongChange&) override;
    void selectionChanged() override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void timerCallback() override;

    void rebuildHeaders();
    void layoutHeaders();
    void updateScrollBars();
    void showContextMenu();

    juce::Rectangle<int> getLaneArea() const;
    juce::Rectangle<int> getRulerArea() const;
    double tickToX (Tick) const;
    Tick xToTick (double x) const;
    int rowAtY (int y) const;
    DropTarget dropTargetAt (juce::Point<int>) const;
    void setDropIndicator (std::optional<juce::Point<int>>, bool newTrackOnly = false);
    void zoomAround (double factor, int x);

    void paintRuler (juce::Graphics&, juce::Rectangle<int>);
    void paintLanes (juce::Graphics&, juce::Rectangle<int>);
    void paintRegion (juce::Graphics&, const Track&, const Region&, juce::Rectangle<float>);

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangeView)
};

} // namespace sms
