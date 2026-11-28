// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SettingsWidgets.h"

namespace sms::settingsui
{

using namespace theme;

//==============================================================================
void ToggleSwitch::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const bool on = getToggleState();
    auto r = getLocalBounds().toFloat().withSizeKeepingCentre (36.0f, 18.0f);

    g.setColour (! isEnabled() ? colours::panelLight
                               : on ? colours::accent : colours::panelDark);
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (on ? colours::accent : (highlighted ? colours::textDim : colours::separator));
    g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.0f);

    const auto knob = r.reduced (3.0f).withWidth (12.0f).withX (on ? r.getRight() - 15.0f : r.getX() + 3.0f);
    g.setColour (on ? colours::textDark : (isEnabled() ? colours::text : colours::textDim));
    g.fillEllipse (knob);
}

//==============================================================================
class FormPage::Content final : public juce::Component
{
public:
    explicit Content (FormPage& o) : owner (o) {}
    void paint (juce::Graphics& g) override
    {
        for (auto& it : owner.items)
        {
            if (it.type == Item::section)
            {
                g.setColour (colours::panelLight);
                g.fillRect (it.area);
                g.setColour (colours::text);
                g.setFont (font (14.0f, true));
                g.drawText (it.text, it.area.reduced (14, 0), juce::Justification::centredLeft, true);
            }
            else if (it.type == Item::row)
            {
                g.setColour (colours::text);
                g.setFont (font (13.5f));
                g.drawText (it.text, it.area.withTrimmedLeft (14).withTrimmedRight (it.width + 24),
                            juce::Justification::centredLeft, true);
            }
            else if (it.type == Item::note)
            {
                g.setColour (colours::textDim);
                g.setFont (font (12.5f));
                g.drawFittedText (it.text, it.area.reduced (14, 2), juce::Justification::topLeft, 8, 1.0f);
            }
        }
    }

private:
    FormPage& owner;
};

FormPage::FormPage() : content (std::make_unique<Content> (*this))
{
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (10);
    addAndMakeVisible (viewport);
}

FormPage::~FormPage()
{
    viewport.setViewedComponent (nullptr, false);
    items.clear(); // controls live inside the content component
}

void FormPage::addSection (const juce::String& title, juce::Component* rightSide, int rightWidth)
{
    if (! items.empty())
        addSpace (14);
    Item it { Item::section, title, nullptr, rightSide, rightWidth, 36, {} };
    if (rightSide != nullptr)
        content->addAndMakeVisible (rightSide);
    items.push_back (std::move (it));
    addSpace (6);
}

void FormPage::addRowInternal (const juce::String& label, std::unique_ptr<juce::Component> c, int width, const juce::String& tooltip)
{
    if (auto* tc = dynamic_cast<juce::SettableTooltipClient*> (c.get()); tc != nullptr && tooltip.isNotEmpty())
        tc->setTooltip (tooltip);
    content->addAndMakeVisible (*c);
    items.push_back ({ Item::row, label, std::move (c), nullptr, width, 32, {} });
}

void FormPage::addNote (const juce::String& text)
{
    items.push_back ({ Item::note, text, nullptr, nullptr, 0, 0, {} });
}

void FormPage::addFullInternal (std::unique_ptr<juce::Component> c, int height)
{
    content->addAndMakeVisible (*c);
    items.push_back ({ Item::full, {}, std::move (c), nullptr, 0, height, {} });
}

void FormPage::addSpace (int height)
{
    items.push_back ({ Item::space, {}, nullptr, nullptr, 0, height, {} });
}

void FormPage::layout()
{
    const auto w = juce::jmax (200, viewport.getMaximumVisibleWidth());
    int y = 0;
    const auto noteFont = juce::Font (font (12.5f));

    for (auto& it : items)
    {
        int h = it.height;
        if (it.type == Item::note)
        {
            // rough line count for wrapping
            const auto textW = juce::GlyphArrangement::getStringWidth (noteFont, it.text);
            const auto lines = juce::jmax (1, (int) std::ceil (textW / (float) juce::jmax (50, w - 40)) + it.text.containsChar ('\n'));
            h = lines * 17 + 8;
        }
        it.area = { 0, y, w, h };

        if (it.type == Item::section && it.right != nullptr)
            it.right->setBounds (it.area.withTrimmedRight (10).removeFromRight (it.width).withSizeKeepingCentre (it.width, 26));
        else if (it.type == Item::row && it.comp != nullptr)
            it.comp->setBounds (it.area.withTrimmedRight (14).removeFromRight (it.width).withSizeKeepingCentre (it.width, 26));
        else if (it.type == Item::full && it.comp != nullptr)
            it.comp->setBounds (it.area.reduced (14, 0));

        y += h;
    }

    content->setSize (w, y + 16);
}

void FormPage::resized()
{
    viewport.setBounds (getLocalBounds());
    layout();
    layout(); // again: the scroll bar may have appeared
}

void FormPage::paint (juce::Graphics& g) { g.fillAll (colours::panel); }

//==============================================================================
CategoryPage::CategoryPage (const juce::String& t) : title (t)
{
    tabs.setTabBackgroundColour (0, colours::panel);
    tabs.onChange = [this] (int i) { showTab (i); };
    addAndMakeVisible (tabs);
    headerCaption.setFont (font (12.5f));
    headerCaption.setColour (juce::Label::textColourId, colours::textDim);
    headerCaption.setJustificationType (juce::Justification::centredRight);
    addChildComponent (headerCaption);
}

void CategoryPage::addTab (const juce::String& name, std::unique_ptr<juce::Component> content)
{
    tabs.addTab (name, colours::panel, -1);
    addChildComponent (*content);
    pages.push_back (std::move (content));
    if (current < 0)
        showTab (0);
    tabs.setVisible (pages.size() > 1);
    resized();
}

void CategoryPage::replaceTab (int index, std::unique_ptr<juce::Component> content)
{
    if (index < 0 || index >= (int) pages.size())
        return;
    addChildComponent (*content);
    content->setVisible (index == current);
    pages[(size_t) index] = std::move (content);
    resized();
}

void CategoryPage::setHeaderComponent (juce::Component* c, int width, const juce::String& caption)
{
    header = c;
    headerWidth = width;
    addAndMakeVisible (c);
    headerCaption.setText (caption, juce::dontSendNotification);
    headerCaption.setVisible (caption.isNotEmpty());
    resized();
}

void CategoryPage::showTab (int index)
{
    if (index < 0 || index >= (int) pages.size())
        return;
    current = index;
    for (size_t i = 0; i < pages.size(); ++i)
        pages[i]->setVisible ((int) i == index);
    if (tabs.getCurrentTabIndex() != index)
        tabs.setCurrentTabIndex (index, false);
}

void CategoryPage::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    g.setColour (colours::text);
    g.setFont (font (22.0f, true));
    g.drawText (title, 18, 10, getWidth() - 36 - headerWidth, 36, juce::Justification::centredLeft, true);
}

void CategoryPage::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (54).reduced (14, 12);
    if (header != nullptr)
    {
        header->setBounds (top.removeFromRight (headerWidth));
        headerCaption.setBounds (top.removeFromRight (110));
    }

    if (pages.size() > 1)
        tabs.setBounds (r.removeFromTop (32).reduced (10, 0));
    r.removeFromTop (6);

    for (auto& p : pages)
        p->setBounds (r.reduced (10, 0).withTrimmedBottom (10));
}

//==============================================================================
void Sidebar::setEntries (std::vector<Entry> e)
{
    entries = std::move (e);
    repaint();
}

void Sidebar::setSelected (int index)
{
    selected = juce::jlimit (0, juce::jmax (0, (int) entries.size() - 1), index);
    repaint();
}

juce::Rectangle<int> Sidebar::rowBounds (int index) const
{
    int y = 10;
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        if (entries[(size_t) i].gapBefore)
            y += 14;
        if (i == index)
            return { 6, y, getWidth() - 12, 32 };
        y += 34;
    }
    return {};
}

int Sidebar::indexAt (int y) const
{
    for (int i = 0; i < (int) entries.size(); ++i)
        if (rowBounds (i).contains (rowBounds (i).getX() + 1, y))
            return i;
    return -1;
}

void Sidebar::paint (juce::Graphics& g)
{
    g.fillAll (colours::panelDark);
    g.setColour (colours::border);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());

    for (int i = 0; i < (int) entries.size(); ++i)
    {
        auto r = rowBounds (i);
        if (i == selected || i == hover)
        {
            g.setColour (i == selected ? colours::header : colours::panelLight);
            g.fillRoundedRectangle (r.toFloat(), 4.0f);
        }
        if (i == selected)
        {
            g.setColour (colours::accent);
            g.fillRect (r.removeFromLeft (3).reduced (0, 6));
        }

        auto rr = rowBounds (i).reduced (12, 0);
        auto iconArea = rr.removeFromLeft (18).toFloat().withSizeKeepingCentre (16.0f, 16.0f);
        const auto& e = entries[(size_t) i];
        g.setColour (i == selected ? colours::text : colours::textDim);
        if (! e.icon.isEmpty())
            g.fillPath (e.icon, e.icon.getTransformToScaleToFit (iconArea, true));
        rr.removeFromLeft (10);
        g.setFont (font (14.0f, i == selected));
        g.drawText (e.name, rr, juce::Justification::centredLeft, true);
    }
}

void Sidebar::mouseMove (const juce::MouseEvent& e)
{
    const auto h = indexAt (e.y);
    if (h != hover)
    {
        hover = h;
        repaint();
    }
}

void Sidebar::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

void Sidebar::mouseDown (const juce::MouseEvent& e)
{
    const auto i = indexAt (e.y);
    if (i >= 0)
    {
        setSelected (i);
        if (onSelect)
            onSelect (i);
    }
}

//==============================================================================
Table::Table (std::vector<Column> c) : columns (std::move (c)) {}

void Table::addRow (std::vector<std::unique_ptr<juce::Component>> cells)
{
    for (auto& c : cells)
        if (c != nullptr)
            addAndMakeVisible (*c);
    rows.push_back (std::move (cells));
    resized();
    repaint();
}

void Table::clearRows()
{
    rows.clear();
    repaint();
}

std::unique_ptr<juce::Label> Table::text (const juce::String& t, bool dim)
{
    auto l = std::make_unique<juce::Label> (juce::String(), t);
    l->setFont (font (13.0f));
    l->setColour (juce::Label::textColourId, dim ? colours::textDim : colours::text);
    l->setMinimumHorizontalScale (0.8f);
    return l;
}

void Table::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (colours::panelDark);
    g.fillRect (r);

    auto head = r.removeFromTop (headerHeight);
    g.setColour (colours::panelLight);
    g.fillRect (head);

    g.setFont (font (12.5f, true));
    int x = 0;
    const auto fixed = [this] { int s = 0; for (auto& c : columns) s += juce::jmax (0, c.width); return s; }();
    for (auto& c : columns)
    {
        const auto w = c.width > 0 ? c.width : juce::jmax (40, getWidth() - fixed);
        g.setColour (colours::textDim);
        g.drawText (c.name, x + 8, head.getY(), w - 12, head.getHeight(), juce::Justification::centredLeft, true);
        g.setColour (colours::separator);
        g.drawVerticalLine (x + w - 1, (float) head.getY() + 5.0f, (float) head.getBottom() - 5.0f);
        x += w;
    }

    for (int i = 0; i < (int) rows.size(); ++i)
    {
        g.setColour (colours::separator.withAlpha (0.5f));
        g.drawHorizontalLine (headerHeight + (i + 1) * rowHeight, 0.0f, (float) getWidth());
    }

    if (rows.empty() && emptyText.isNotEmpty())
    {
        g.setColour (colours::textDim);
        g.setFont (font (13.0f));
        g.drawText (emptyText, r.reduced (8, 0).removeFromTop (rowHeight), juce::Justification::centredLeft, true);
    }
}

void Table::resized()
{
    const auto fixed = [this] { int s = 0; for (auto& c : columns) s += juce::jmax (0, c.width); return s; }();
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        int x = 0;
        const auto y = headerHeight + i * rowHeight;
        for (size_t c = 0; c < columns.size(); ++c)
        {
            const auto w = columns[c].width > 0 ? columns[c].width : juce::jmax (40, getWidth() - fixed);
            if (c < rows[(size_t) i].size() && rows[(size_t) i][c] != nullptr)
            {
                auto* comp = rows[(size_t) i][c].get();
                auto cell = juce::Rectangle<int> (x + 4, y + 2, w - 8, rowHeight - 4);
                if (dynamic_cast<juce::ToggleButton*> (comp) != nullptr)
                    cell = cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 40), cell.getHeight());
                comp->setBounds (cell);
            }
            x += w;
        }
    }
}

//==============================================================================
std::unique_ptr<juce::ComboBox> makeCombo (const juce::StringArray& items, int selectedIndex)
{
    auto c = std::make_unique<juce::ComboBox>();
    c->addItemList (items, 1);
    c->setSelectedItemIndex (selectedIndex, juce::dontSendNotification);
    return c;
}

} // namespace sms::settingsui
