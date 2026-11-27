// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Theme.h"

namespace sms::theme
{

juce::Colour trackColour (int index)
{
    static const juce::uint32 palette[] = {
        0xff4f8fe6, 0xff59c27a, 0xffe2a23b, 0xffd9574f, 0xff9a6de0,
        0xff3fbac2, 0xffe0709f, 0xff9cc43d, 0xffd88a4a, 0xff6c7ee8,
    };
    constexpr int n = (int) (sizeof (palette) / sizeof (palette[0]));
    return juce::Colour (palette[((index % n) + n) % n]);
}

juce::FontOptions font (float height, bool bold)
{
    return juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain);
}

//==============================================================================
namespace icons
{
    // All icons are drawn into a 0..10 box and scaled when painted.

    juce::Path play()
    {
        juce::Path p;
        p.addTriangle (2.5f, 1.5f, 2.5f, 8.5f, 8.5f, 5.0f);
        return p;
    }

    juce::Path stop()
    {
        juce::Path p;
        p.addRectangle (2.0f, 2.0f, 6.0f, 6.0f);
        return p;
    }

    juce::Path record()
    {
        juce::Path p;
        p.addEllipse (1.8f, 1.8f, 6.4f, 6.4f);
        return p;
    }

    juce::Path rewind()
    {
        juce::Path p;
        p.addTriangle (5.0f, 2.0f, 5.0f, 8.0f, 1.0f, 5.0f);
        p.addTriangle (9.0f, 2.0f, 9.0f, 8.0f, 5.0f, 5.0f);
        return p;
    }

    juce::Path forward()
    {
        juce::Path p;
        p.addTriangle (1.0f, 2.0f, 1.0f, 8.0f, 5.0f, 5.0f);
        p.addTriangle (5.0f, 2.0f, 5.0f, 8.0f, 9.0f, 5.0f);
        return p;
    }

    juce::Path toStart()
    {
        juce::Path p;
        p.addRectangle (1.5f, 2.0f, 1.4f, 6.0f);
        p.addTriangle (8.5f, 2.0f, 8.5f, 8.0f, 3.2f, 5.0f);
        return p;
    }

    juce::Path loop()
    {
        juce::Path p;
        juce::Path stroke;
        stroke.addRoundedRectangle (1.5f, 3.0f, 7.0f, 4.5f, 2.0f);
        juce::PathStrokeType (1.1f).createStrokedPath (p, stroke);
        p.addTriangle (5.0f, 1.6f, 5.0f, 4.4f, 7.0f, 3.0f);
        return p;
    }

    juce::Path metronome()
    {
        juce::Path p;
        juce::Path body;
        body.startNewSubPath (3.8f, 1.2f);
        body.lineTo (6.2f, 1.2f);
        body.lineTo (8.2f, 8.8f);
        body.lineTo (1.8f, 8.8f);
        body.closeSubPath();
        juce::PathStrokeType (1.0f).createStrokedPath (p, body);
        juce::Path needle;
        needle.startNewSubPath (5.0f, 7.6f);
        needle.lineTo (7.4f, 2.6f);
        juce::PathStrokeType (1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (needle, needle);
        p.addPath (needle);
        p.addRectangle (2.4f, 6.6f, 5.2f, 0.9f);
        return p;
    }

    juce::Path arrowTool()
    {
        juce::Path p;
        p.startNewSubPath (2.5f, 1.0f);
        p.lineTo (2.5f, 8.6f);
        p.lineTo (4.4f, 6.8f);
        p.lineTo (5.8f, 9.4f);
        p.lineTo (7.0f, 8.8f);
        p.lineTo (5.7f, 6.3f);
        p.lineTo (8.2f, 6.3f);
        p.closeSubPath();
        return p;
    }

    juce::Path rangeTool()
    {
        juce::Path p;
        p.addRectangle (1.0f, 3.0f, 8.0f, 4.0f);
        juce::Path inner;
        inner.addRectangle (2.0f, 4.0f, 6.0f, 2.0f);
        p.addPath (inner);
        p.setUsingNonZeroWinding (false);
        p.addRectangle (1.0f, 1.0f, 1.0f, 8.0f);
        p.addRectangle (8.0f, 1.0f, 1.0f, 8.0f);
        return p;
    }

    juce::Path splitTool()
    {
        juce::Path p;
        p.addRectangle (4.5f, 0.8f, 1.0f, 8.4f);
        p.addTriangle (2.6f, 2.0f, 4.3f, 3.2f, 2.6f, 4.4f);
        p.addTriangle (7.4f, 5.6f, 5.7f, 6.8f, 7.4f, 8.0f);
        return p;
    }

    juce::Path eraserTool()
    {
        juce::Path p;
        p.startNewSubPath (1.0f, 6.5f);
        p.lineTo (5.5f, 2.0f);
        p.lineTo (9.0f, 5.5f);
        p.lineTo (5.5f, 9.0f);
        p.lineTo (3.5f, 9.0f);
        p.closeSubPath();
        return p;
    }

    juce::Path pencilTool()
    {
        juce::Path p;
        p.startNewSubPath (1.2f, 8.8f);
        p.lineTo (1.8f, 6.4f);
        p.lineTo (7.0f, 1.2f);
        p.lineTo (8.8f, 3.0f);
        p.lineTo (3.6f, 8.2f);
        p.closeSubPath();
        return p;
    }

    juce::Path muteTool()
    {
        juce::Path p;
        p.startNewSubPath (1.0f, 3.6f);
        p.lineTo (3.0f, 3.6f);
        p.lineTo (5.6f, 1.2f);
        p.lineTo (5.6f, 8.8f);
        p.lineTo (3.0f, 6.4f);
        p.lineTo (1.0f, 6.4f);
        p.closeSubPath();
        juce::Path x;
        x.startNewSubPath (6.8f, 3.6f);
        x.lineTo (9.2f, 6.4f);
        x.startNewSubPath (9.2f, 3.6f);
        x.lineTo (6.8f, 6.4f);
        juce::Path stroked;
        juce::PathStrokeType (1.0f).createStrokedPath (stroked, x);
        p.addPath (stroked);
        return p;
    }

    static juce::Path curvedArrow (bool left)
    {
        juce::Path arc;
        arc.addCentredArc (5.0f, 6.0f, 3.5f, 3.0f, 0.0f, -1.9f, 1.9f, true);
        juce::Path p;
        juce::PathStrokeType (1.2f).createStrokedPath (p, arc);
        const auto start = arc.getPointAlongPath (0.0f);
        p.addTriangle (start.x - 1.8f, start.y - 0.4f, start.x + 1.0f, start.y - 1.8f, start.x + 0.6f, start.y + 1.6f);
        if (! left)
            p.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f, 5.0f, 5.0f));
        return p;
    }

    juce::Path undo() { return curvedArrow (true); }
    juce::Path redo() { return curvedArrow (false); }

    juce::Path plus()
    {
        juce::Path p;
        p.addRectangle (4.3f, 1.5f, 1.4f, 7.0f);
        p.addRectangle (1.5f, 4.3f, 7.0f, 1.4f);
        return p;
    }

    juce::Path instrument()
    {
        juce::Path p;
        juce::Path outline;
        outline.addRoundedRectangle (0.8f, 2.0f, 8.4f, 6.0f, 0.8f);
        juce::PathStrokeType (0.9f).createStrokedPath (p, outline);
        for (int i = 0; i < 4; ++i)
            p.addRectangle (2.2f + (float) i * 1.8f, 2.4f, 0.9f, 3.3f);
        return p;
    }

    juce::Path waveform()
    {
        juce::Path p;
        const float h[] = { 1.5f, 3.5f, 2.0f, 4.5f, 3.0f, 1.0f, 3.8f, 2.4f };
        for (int i = 0; i < 8; ++i)
            p.addRectangle (1.0f + (float) i * 1.1f, 5.0f - h[i], 0.7f, h[i] * 2.0f);
        return p;
    }
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    using namespace colours;

    setColourScheme ({ panel, window, panelLight, separator, text, accent, juce::Colours::white, panelLight, text });

    setColour (juce::ResizableWindow::backgroundColourId, window);
    setColour (juce::DocumentWindow::textColourId, text);

    setColour (juce::TextButton::buttonColourId, header);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);

    setColour (juce::Label::textColourId, text);
    setColour (juce::Label::textWhenEditingColourId, text);
    setColour (juce::Label::backgroundWhenEditingColourId, panelDark);
    setColour (juce::Label::outlineWhenEditingColourId, accent);

    setColour (juce::TextEditor::backgroundColourId, panelDark);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, border);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::TextEditor::highlightColourId, accentDim);
    setColour (juce::CaretComponent::caretColourId, text);

    setColour (juce::ComboBox::backgroundColourId, panelDark);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, border);
    setColour (juce::ComboBox::arrowColourId, textDim);

    setColour (juce::PopupMenu::backgroundColourId, panelLight);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, textDim);

    setColour (juce::ListBox::backgroundColourId, panelDark);
    setColour (juce::ListBox::outlineColourId, border);
    setColour (juce::ScrollBar::thumbColourId, headerLight);

    setColour (juce::Slider::thumbColourId, text);
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::backgroundColourId, panelDark);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, panelDark);
    setColour (juce::Slider::textBoxOutlineColourId, border);

    setColour (juce::TooltipWindow::backgroundColourId, panelLight);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, border);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& background,
                                        bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto base = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : background;

    if (! b.isEnabled())
        base = base.withMultipliedAlpha (0.5f);
    else if (down)
        base = base.darker (0.25f);
    else if (highlighted)
        base = base.brighter (0.12f);

    g.setGradientFill (juce::ColourGradient (base.brighter (0.06f), 0, r.getY(), base.darker (0.08f), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                  .withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return juce::Font (font (juce::jmin (13.0f, (float) h * 0.6f)));
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                    juce::Slider::SliderStyle style, juce::Slider& s)
{
    const bool vertical = style == juce::Slider::LinearVertical;
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const auto fill = s.findColour (juce::Slider::trackColourId);

    if (vertical)
    {
        auto track = bounds.withSizeKeepingCentre (4.0f, bounds.getHeight());
        g.setColour (colours::panelDark);
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (fill.withAlpha (0.7f));
        g.fillRoundedRectangle (track.withTop (pos), 2.0f);

        auto thumb = juce::Rectangle<float> (juce::jmin (bounds.getWidth(), 26.0f), 12.0f).withCentre ({ bounds.getCentreX(), pos });
        g.setGradientFill (juce::ColourGradient (colours::headerLight.brighter (0.2f), 0, thumb.getY(), colours::header, 0, thumb.getBottom(), false));
        g.fillRoundedRectangle (thumb, 2.0f);
        g.setColour (colours::border);
        g.drawRoundedRectangle (thumb, 2.0f, 1.0f);
        g.setColour (colours::text);
        g.fillRect (thumb.withSizeKeepingCentre (thumb.getWidth() - 6.0f, 1.0f));
    }
    else
    {
        auto track = bounds.withSizeKeepingCentre (bounds.getWidth(), 4.0f);
        g.setColour (colours::panelDark);
        g.fillRoundedRectangle (track, 2.0f);

        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0 && s.getProperties().contains ("bipolar");
        g.setColour (fill.withAlpha (0.8f));
        if (bipolar)
        {
            const auto centre = (float) s.getPositionOfValue (0.0) + (float) x;
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (centre, pos), track.getY(), juce::jmax (centre, pos), track.getBottom()));
        }
        else
        {
            g.fillRoundedRectangle (track.withRight (pos), 2.0f);
        }

        auto thumb = juce::Rectangle<float> (8.0f, juce::jmin (bounds.getHeight(), 14.0f)).withCentre ({ pos, bounds.getCentreY() });
        g.setColour (colours::text);
        g.fillRoundedRectangle (thumb, 2.0f);
    }
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const auto angle = startAngle + pos * (endAngle - startAngle);

    g.setColour (colours::panelDark);
    g.fillEllipse (juce::Rectangle<float> (radius * 2, radius * 2).withCentre (centre));

    // bipolar knobs (pan) draw the arc from the centre position
    const auto from = s.getProperties().contains ("bipolar") ? (startAngle + endAngle) * 0.5f : startAngle;

    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (s.findColour (juce::Slider::trackColourId));
    g.strokePath (arc, juce::PathStrokeType (2.0f));

    g.setColour (colours::text);
    g.drawLine (juce::Line<float> (centre, centre.getPointOnCircumference (radius - 3, angle)), 1.5f);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (w, h).toFloat().reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);

    juce::Path arrow;
    const auto ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&) { return juce::Font (font (13.0f)); }
juce::Font LookAndFeel::getPopupMenuFont() { return juce::Font (font (14.0f)); }
juce::Font LookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }

void LookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                 int thumbStart, int thumbSize, bool over, bool down)
{
    g.setColour (colours::panelDark);
    g.fillRect (x, y, w, h);

    if (thumbSize <= 0)
        return;

    auto thumb = vertical ? juce::Rectangle<int> (x + 2, thumbStart, w - 4, thumbSize)
                          : juce::Rectangle<int> (thumbStart, y + 2, thumbSize, h - 4);
    g.setColour (colours::headerLight.brighter (over || down ? 0.2f : 0.0f));
    g.fillRoundedRectangle (thumb.toFloat(), 3.0f);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (colours::panelLight);
    g.setColour (colours::border);
    g.drawRect (0, 0, w, h);
}

void LookAndFeel::drawMenuBarBackground (juce::Graphics& g, int w, int h, bool, juce::MenuBarComponent&)
{
    g.fillAll (colours::panelDark);
    g.setColour (colours::border);
    g.drawHorizontalLine (h - 1, 0.0f, (float) w);
}

juce::Font LookAndFeel::getMenuBarFont (juce::MenuBarComponent&, int, const juce::String&)
{
    return juce::Font (font (13.5f));
}

void LookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (juce::Rectangle<int> (w, h).toFloat(), 3.0f);
}

void LookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? e.findColour (juce::TextEditor::focusedOutlineColourId)
                                           : e.findColour (juce::TextEditor::outlineColourId));
    g.drawRoundedRectangle (juce::Rectangle<int> (w, h).toFloat().reduced (0.5f), 3.0f, 1.0f);
}

//==============================================================================
IconButton::IconButton (const juce::String& name, juce::Path p, juce::Colour on)
    : juce::Button (name), icon (std::move (p)), onColour (on)
{
    setTooltip (name);
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();

    if (drawBackground)
    {
        auto base = colours::header;
        if (down)
            base = base.darker (0.3f);
        else if (highlighted)
            base = base.brighter (0.12f);

        g.setGradientFill (juce::ColourGradient (base.brighter (0.07f), 0, r.getY(), base.darker (0.1f), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (colours::border);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }
    else if (highlighted || down)
    {
        g.setColour (colours::headerLight.withAlpha (down ? 0.6f : 0.35f));
        g.fillRoundedRectangle (r, 3.0f);
    }

    auto iconArea = r.reduced (juce::jmax (4.0f, juce::jmin (r.getWidth(), r.getHeight()) * 0.24f));
    auto colour = on ? onColour : colours::text.withAlpha (0.85f);
    if (! isEnabled())
        colour = colour.withMultipliedAlpha (0.4f);

    g.setColour (colour);
    g.fillPath (icon, icon.getTransformToScaleToFit (iconArea, true));
}

//==============================================================================
void drawPanelHeader (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
{
    g.setGradientFill (juce::ColourGradient (colours::header, 0, (float) area.getY(), colours::panelLight, 0, (float) area.getBottom(), false));
    g.fillRect (area);
    g.setColour (colours::border);
    g.drawHorizontalLine (area.getBottom() - 1, (float) area.getX(), (float) area.getRight());
    g.setColour (colours::text);
    g.setFont (font (13.0f, true));
    g.drawText (title, area.reduced (8, 0), juce::Justification::centredLeft, true);
}

} // namespace sms::theme
