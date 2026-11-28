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

juce::StringArray schemeNames()
{
    return { "Studio Dunkel", "Graphit", "Mitternacht", "Hell" };
}

void applyScheme (int index)
{
    using namespace colours;
    struct Scheme { juce::uint32 window, panel, panelDark, panelLight, header, headerLight, border, separator,
                    laneA, laneB, laneSelected, gridBar, gridBeat, text, textDim, textDark, accent, accentDim,
                    playhead, lcd, lcdText; };

    static const Scheme schemes[] = {
        // Studio Dunkel (default)
        { 0xff1b1c1f, 0xff2a2c31, 0xff222428, 0xff34373d, 0xff3b3f46, 0xff474b53, 0xff111214, 0xff3f434a,
          0xff26282c, 0xff232529, 0xff2f3540, 0xff3a3e45, 0xff2e3136, 0xffd9dce1, 0xff8d929b, 0xff1a1b1e,
          0xff3d9bff, 0xff2a5f99, 0xffe9eef5, 0xff111316, 0xff9fd2ff },
        // Graphit: neutral grey, orange accent
        { 0xff202020, 0xff2e2e2e, 0xff262626, 0xff393939, 0xff424242, 0xff4e4e4e, 0xff141414, 0xff474747,
          0xff2a2a2a, 0xff262626, 0xff3a342d, 0xff434343, 0xff333333, 0xffe0e0e0, 0xff969696, 0xff1a1a1a,
          0xffff8f2e, 0xff8a5424, 0xfff2f2f2, 0xff121212, 0xffffc58a },
        // Mitternacht: dark blue, cyan accent
        { 0xff10141c, 0xff1b2230, 0xff151b27, 0xff242d3d, 0xff2a3447, 0xff354158, 0xff090c12, 0xff2f3a4e,
          0xff19202c, 0xff161c27, 0xff22324a, 0xff2c374b, 0xff222b3b, 0xffd6e2f2, 0xff8496b0, 0xff0d1118,
          0xff44c8ff, 0xff1f5f80, 0xffe6f4ff, 0xff0a0e15, 0xff8fe1ff },
        // Hell
        { 0xffd4d7dc, 0xffe4e6ea, 0xffcfd3d8, 0xffeef0f2, 0xffc7cbd1, 0xffb8bdc5, 0xff9aa0a8, 0xffb2b7be,
          0xffe8eaed, 0xffe0e3e7, 0xffcfdaeb, 0xffb5bbc3, 0xffd0d4d9, 0xff1d2026, 0xff59606b, 0xff111214,
          0xff2f7fe0, 0xff9fc4f0, 0xff20242a, 0xff1b1f25, 0xff9fd2ff },
    };

    const auto& sc = schemes[(size_t) juce::jlimit (0, (int) std::size (schemes) - 1, index)];
    window = juce::Colour (sc.window);       panel = juce::Colour (sc.panel);
    panelDark = juce::Colour (sc.panelDark); panelLight = juce::Colour (sc.panelLight);
    header = juce::Colour (sc.header);       headerLight = juce::Colour (sc.headerLight);
    border = juce::Colour (sc.border);       separator = juce::Colour (sc.separator);
    laneA = juce::Colour (sc.laneA);         laneB = juce::Colour (sc.laneB);
    laneSelected = juce::Colour (sc.laneSelected);
    gridBar = juce::Colour (sc.gridBar);     gridBeat = juce::Colour (sc.gridBeat);
    text = juce::Colour (sc.text);           textDim = juce::Colour (sc.textDim);
    textDark = juce::Colour (sc.textDark);
    accent = juce::Colour (sc.accent);       accentDim = juce::Colour (sc.accentDim);
    playhead = juce::Colour (sc.playhead);
    lcd = juce::Colour (sc.lcd);             lcdText = juce::Colour (sc.lcdText);
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

namespace icons
{
    juce::Path detach()
    {
        // window with an arrow pointing out (top right)
        juce::Path frame;
        frame.startNewSubPath (5.0f, 2.0f);
        frame.lineTo (1.5f, 2.0f);
        frame.lineTo (1.5f, 8.5f);
        frame.lineTo (8.0f, 8.5f);
        frame.lineTo (8.0f, 5.0f);
        juce::Path p;
        juce::PathStrokeType (1.1f).createStrokedPath (p, frame);
        juce::Path arrow;
        arrow.startNewSubPath (4.5f, 5.5f);
        arrow.lineTo (8.8f, 1.2f);
        juce::Path a;
        juce::PathStrokeType (1.1f).createStrokedPath (a, arrow);
        p.addPath (a);
        p.addTriangle (6.0f, 1.0f, 9.0f, 1.0f, 9.0f, 4.0f);
        return p;
    }

    juce::Path dock()
    {
        // window with an arrow pointing in
        juce::Path frame;
        frame.startNewSubPath (5.0f, 2.0f);
        frame.lineTo (1.5f, 2.0f);
        frame.lineTo (1.5f, 8.5f);
        frame.lineTo (8.0f, 8.5f);
        frame.lineTo (8.0f, 5.0f);
        juce::Path p;
        juce::PathStrokeType (1.1f).createStrokedPath (p, frame);
        juce::Path arrow;
        arrow.startNewSubPath (8.8f, 1.2f);
        arrow.lineTo (5.2f, 4.8f);
        juce::Path a;
        juce::PathStrokeType (1.1f).createStrokedPath (a, arrow);
        p.addPath (a);
        p.addTriangle (4.0f, 3.0f, 4.0f, 6.0f, 7.0f, 6.0f);
        return p;
    }
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    refreshColours();
}

void LookAndFeel::refreshColours()
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
        const auto cx = bounds.getCentreX();
        const auto top = (float) s.getPositionOfValue (s.getMaximum());
        const auto bottom = (float) s.getPositionOfValue (s.getMinimum());

        // dB scale ticks left and right of the fader slot
        if (s.getProperties().contains ("dbScale"))
        {
            g.setFont (font (9.5f));
            for (double db : { 6.0, 0.0, -6.0, -12.0, -24.0, -48.0 })
            {
                if (db < s.getMinimum() || db > s.getMaximum())
                    continue;
                const auto ty = (float) s.getPositionOfValue (db);
                g.setColour (juce::exactlyEqual (db, 0.0) ? colours::textDim : colours::separator);
                g.drawHorizontalLine ((int) ty, cx - 12.0f, cx - 6.0f);
                g.drawHorizontalLine ((int) ty, cx + 6.0f, cx + 12.0f);
                if (bounds.getWidth() >= 64.0f)
                {
                    g.setColour (colours::textDim);
                    g.drawText (juce::String ((int) db), juce::Rectangle<float> (cx - 40.0f, ty - 6.0f, 24.0f, 12.0f),
                                juce::Justification::centredRight);
                }
            }
        }

        // slot
        auto slot = juce::Rectangle<float>::leftTopRightBottom (cx - 2.5f, top, cx + 2.5f, bottom);
        g.setColour (colours::border);
        g.fillRoundedRectangle (slot.expanded (1.0f), 3.0f);
        g.setColour (colours::panelDark.darker (0.3f));
        g.fillRoundedRectangle (slot, 2.5f);
        g.setColour (fill.withAlpha (0.75f));
        g.fillRoundedRectangle (slot.withTop (pos), 2.5f);

        // cap: wide and tall like a hardware fader knob
        const auto capW = juce::jmin (bounds.getWidth() - 2.0f, faderCapWidth);
        auto cap = juce::Rectangle<float> (capW, faderCapHeight).withCentre ({ cx, pos });
        const bool active = s.isMouseOverOrDragging();

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 3.0f);

        auto body = colours::headerLight.brighter (active ? 0.25f : 0.12f);
        g.setGradientFill (juce::ColourGradient (body.brighter (0.25f), 0, cap.getY(),
                                                 body.darker (0.35f), 0, cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 3.0f);

        // grip ridges above and below the centre line
        for (int i = 1; i <= 3; ++i)
        {
            const auto off = (float) i * 4.5f + 2.0f;
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRect (cap.getX() + 4.0f, pos - off, capW - 8.0f, 1.0f);
            g.fillRect (cap.getX() + 4.0f, pos + off, capW - 8.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRect (cap.getX() + 4.0f, pos - off + 1.0f, capW - 8.0f, 1.0f);
            g.fillRect (cap.getX() + 4.0f, pos + off + 1.0f, capW - 8.0f, 1.0f);
        }

        // centre line marks the exact value
        g.setColour (active ? colours::accent.brighter (0.3f) : juce::Colours::white.withAlpha (0.9f));
        g.fillRect (cap.getX() + 2.0f, pos - 1.0f, capW - 4.0f, 2.0f);

        g.setColour (colours::border);
        g.drawRoundedRectangle (cap, 3.0f, 1.0f);
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

int LookAndFeel::getSliderThumbRadius (juce::Slider& s)
{
    // keep the whole fader cap inside the slider at both ends
    if (s.getSliderStyle() == juce::Slider::LinearVertical)
        return (int) std::ceil (faderCapHeight * 0.5f) + 1;

    return LookAndFeel_V4::getSliderThumbRadius (s);
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
StateButton::StateButton (const juce::String& letter, juce::Colour on, const juce::String& tip)
    : juce::Button (letter), onColour (on)
{
    setTooltip (tip);
    setWantsKeyboardFocus (false);
}

void StateButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();

    auto bg = on ? onColour : colours::panelDark;
    if (down)
        bg = bg.darker (0.2f);
    else if (highlighted)
        bg = bg.brighter (on ? 0.1f : 0.25f);

    g.setColour (bg);
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (on ? onColour.darker (0.5f) : colours::border);
    g.drawRoundedRectangle (r, 2.5f, 1.0f);

    const bool lightTheme = colours::panelDark.getPerceivedBrightness() > 0.5f;
    const auto offColour = lightTheme ? onColour.darker (0.9f) : onColour.withMultipliedSaturation (0.75f).withMultipliedBrightness (0.9f);
    g.setColour (on ? colours::textDark : offColour);
    g.setFont (font (juce::jmin (13.0f, (float) getHeight() * 0.62f), true));
    g.drawText (getName(), getLocalBounds(), juce::Justification::centred);
}

void makeValueLabel (juce::Label& l, float fontHeight)
{
    l.setFont (font (fontHeight));
    l.setEditable (true, true, false); // single click starts editing
    l.setColour (juce::Label::textColourId, colours::text.withAlpha (0.8f));
    l.setColour (juce::Label::backgroundColourId, colours::panelDark.withAlpha (0.6f));
    l.setColour (juce::Label::outlineColourId, colours::border.withAlpha (0.6f));
    l.setColour (juce::Label::backgroundWhenEditingColourId, colours::lcd);
    l.setColour (juce::Label::textWhenEditingColourId, colours::lcdText);
    l.setColour (juce::Label::outlineWhenEditingColourId, colours::accent);
    l.setMouseCursor (juce::MouseCursor::IBeamCursor);
    l.setTooltip (u8"Klicken und Wert eintippen, Enter bestätigt");
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
