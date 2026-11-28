// SPDX-License-Identifier: AGPL-3.0-or-later
// Building blocks for the settings window (flat, Cubase-like layout):
// a category sidebar, pages with a big title and tabs, form pages with section
// headers and rows, toggle switches and simple tables.
#pragma once

#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

namespace sms::settingsui
{

//==============================================================================
/** Flat on/off switch (pill with a knob). */
class ToggleSwitch final : public juce::ToggleButton
{
public:
    ToggleSwitch() { setWantsKeyboardFocus (false); setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

//==============================================================================
/** Vertical list of sections and rows ("label ......... control"), scrollable. */
class FormPage : public juce::Component
{
public:
    FormPage();
    ~FormPage() override;

    /** Dark header bar; an optional component is placed at its right end. */
    void addSection (const juce::String& title, juce::Component* rightSide = nullptr, int rightWidth = 0);

    /** Label on the left, control on the right (the page owns the control). */
    template <typename C>
    C& addRow (const juce::String& label, std::unique_ptr<C> control, int controlWidth = 200, const juce::String& tooltip = {})
    {
        auto& ref = *control;
        addRowInternal (label, std::move (control), controlWidth, tooltip);
        return ref;
    }

    /** Wrapped hint text. */
    void addNote (const juce::String& text);

    /** Any component over the full width with a fixed height (page owns it). */
    template <typename C>
    C& addFull (std::unique_ptr<C> c, int height)
    {
        auto& ref = *c;
        addFullInternal (std::move (c), height);
        return ref;
    }

    void addSpace (int height = 12);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    struct Item
    {
        enum Type { section, row, note, full, space } type;
        juce::String text;
        std::unique_ptr<juce::Component> comp;
        juce::Component* right = nullptr;
        int width = 0, height = 0;
        juce::Rectangle<int> area;
    };

    class Content;
    void addRowInternal (const juce::String&, std::unique_ptr<juce::Component>, int width, const juce::String& tooltip);
    void addFullInternal (std::unique_ptr<juce::Component>, int height);
    void layout();

    std::vector<Item> items;
    std::unique_ptr<Content> content;
    juce::Viewport viewport;
};

//==============================================================================
/** A page with a big title, an optional component at the top right (e.g. the
    audio driver) and tabs below; each tab shows one component. */
class CategoryPage : public juce::Component
{
public:
    explicit CategoryPage (const juce::String& title);

    void addTab (const juce::String& name, std::unique_ptr<juce::Component> content);
    /** Swaps the content of a tab (e.g. after the audio device changed). */
    void replaceTab (int index, std::unique_ptr<juce::Component> content);
    void setHeaderComponent (juce::Component* c, int width, const juce::String& caption = {});
    void showTab (int index);
    int getCurrentTab() const { return current; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Tabs final : public juce::TabbedButtonBar
    {
        Tabs() : juce::TabbedButtonBar (juce::TabbedButtonBar::TabsAtTop) {}
        std::function<void (int)> onChange;
        void currentTabChanged (int i, const juce::String&) override { if (onChange) onChange (i); }
    };

    juce::String title;
    Tabs tabs;
    std::vector<std::unique_ptr<juce::Component>> pages;
    juce::Component* header = nullptr;
    int headerWidth = 0;
    juce::Label headerCaption;
    int current = -1;
};

//==============================================================================
/** Left column with the categories (icon + name). */
class Sidebar final : public juce::Component
{
public:
    struct Entry
    {
        juce::String name;
        juce::Path icon;
        bool gapBefore = false;
    };

    std::function<void (int)> onSelect;

    void setEntries (std::vector<Entry>);
    void setSelected (int index);
    int getSelected() const { return selected; }

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    int indexAt (int y) const;
    juce::Rectangle<int> rowBounds (int index) const;

    std::vector<Entry> entries;
    int selected = 0, hover = -1;
};

//==============================================================================
/** Simple table: header row and rows of components (one per column). */
class Table final : public juce::Component
{
public:
    struct Column
    {
        juce::String name;
        int width; // <= 0: takes the remaining space
    };

    explicit Table (std::vector<Column>);

    /** Adds a row; the table owns the components (nullptr = empty cell). */
    void addRow (std::vector<std::unique_ptr<juce::Component>> cells);
    void clearRows();
    int getNumRows() const { return (int) rows.size(); }

    /** Text shown when there are no rows. */
    void setEmptyText (const juce::String& t) { emptyText = t; repaint(); }

    int getIdealHeight() const { return headerHeight + juce::jmax (1, (int) rows.size()) * rowHeight + 2; }

    void paint (juce::Graphics&) override;
    void resized() override;

    static std::unique_ptr<juce::Label> text (const juce::String&, bool dim = false);

private:
    std::vector<Column> columns;
    std::vector<std::vector<std::unique_ptr<juce::Component>>> rows;
    juce::String emptyText;
    static constexpr int headerHeight = 28, rowHeight = 30;
};

/** Combo box styled for the settings. */
std::unique_ptr<juce::ComboBox> makeCombo (const juce::StringArray& items, int selectedIndex);

} // namespace sms::settingsui
