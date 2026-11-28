// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "extension/ExtensionApi.h"
#include "Theme.h"

#include <map>

namespace sms
{

class InsertsBox;
class ChannelStrip;

/** Left panel: the selected track. Instrument settings on top (instrument
    tracks only), below a complete channel strip like in the console. */
class InspectorPanel final : public juce::Component,
                             private Song::Listener,
                             private Selection::Listener
{
public:
    explicit InspectorPanel (AppContext&);
    ~InspectorPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void songChanged (const SongChange&) override;
    void selectionChanged() override;
    void refresh();
    std::vector<juce::Component*> instrumentComponents();

    AppContext& ctx;
    ObjectId shownTrack;
    bool isInstrument = false;

    juce::Label nameLabel, typeLabel, instrumentLabel, emptyLabel;
    juce::Label transposeCaption, velocityCaption, arpCaption, midiFxCaption;
    juce::TextButton openButton { u8"Öffnen" }, arpButton { "Aus" }, midiFxButton { "folgt" };
    juce::Slider transpose, velocity;
    juce::Rectangle<int> instrumentArea;
    std::unique_ptr<ChannelStrip> strip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InspectorPanel)
};

//==============================================================================
/** Right panel: tabs of draggable items (instruments, effects, files). */
class BrowserPanel final : public juce::Component,
                           private juce::ListBoxModel
{
public:
    explicit BrowserPanel (AppContext&);
    ~BrowserPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    juce::String getTooltipForRow (int row) override;

    void selectTab (int index);
    void reload();

    struct Row
    {
        bool isHeader = false;
        juce::String text;
        BrowserItem item;
    };

    AppContext& ctx;
    juce::OwnedArray<juce::TextButton> tabs;
    juce::TextEditor search;
    juce::ListBox list { "browser", this };
    juce::Label hint, status;
    juce::OwnedArray<juce::TextButton> sourceButtons;
    std::map<int, std::unique_ptr<juce::Component>> customViews; // per tab, created on first use
    juce::Component* activeView = nullptr;
    int currentTab = 0;
    std::vector<Row> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrowserPanel)
};

//==============================================================================
/** Bottom panel: one channel strip per track plus master. */
class ConsolePanel final : public juce::Component,
                           private Song::Listener,
                           private Selection::Listener,
                           private juce::Timer
{
public:
    explicit ConsolePanel (AppContext&);
    ~ConsolePanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void songChanged (const SongChange&) override;
    void selectionChanged() override;
    void timerCallback() override;
    void rebuild();

    class MasterStrip;

    AppContext& ctx;
    juce::Viewport viewport;
    juce::Component stripHolder;
    juce::OwnedArray<ChannelStrip> strips;
    std::unique_ptr<MasterStrip> master;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConsolePanel)
};

} // namespace sms
