// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace sms::theme
{

// Dark, low-contrast studio look with a blue accent.
namespace colours
{
    inline const juce::Colour window       { 0xff1b1c1f };
    inline const juce::Colour panel        { 0xff2a2c31 };
    inline const juce::Colour panelDark    { 0xff222428 };
    inline const juce::Colour panelLight   { 0xff34373d };
    inline const juce::Colour header       { 0xff3b3f46 };
    inline const juce::Colour headerLight  { 0xff474b53 };
    inline const juce::Colour border       { 0xff111214 };
    inline const juce::Colour separator    { 0xff3f434a };
    inline const juce::Colour laneA        { 0xff26282c };
    inline const juce::Colour laneB        { 0xff232529 };
    inline const juce::Colour laneSelected { 0xff2f3540 };
    inline const juce::Colour gridBar      { 0xff3a3e45 };
    inline const juce::Colour gridBeat     { 0xff2e3136 };
    inline const juce::Colour text         { 0xffd9dce1 };
    inline const juce::Colour textDim      { 0xff8d929b };
    inline const juce::Colour textDark     { 0xff1a1b1e };
    inline const juce::Colour accent       { 0xff3d9bff };
    inline const juce::Colour accentDim    { 0xff2a5f99 };
    inline const juce::Colour play         { 0xff4cc36a };
    inline const juce::Colour record       { 0xffe5483f };
    inline const juce::Colour mute         { 0xff3f86f2 }; // blue
    inline const juce::Colour solo         { 0xfff5cf2e }; // yellow
    inline const juce::Colour playhead     { 0xffe9eef5 };
    inline const juce::Colour lcd          { 0xff111316 };
    inline const juce::Colour lcdText      { 0xff9fd2ff };
}

/** Colours offered for new tracks (cycled). */
juce::Colour trackColour (int index);

juce::FontOptions font (float height, bool bold = false);

//==============================================================================
/** Small vector icons used by tool and transport buttons. */
namespace icons
{
    juce::Path play();
    juce::Path stop();
    juce::Path record();
    juce::Path rewind();
    juce::Path forward();
    juce::Path toStart();
    juce::Path loop();
    juce::Path metronome();
    juce::Path arrowTool();
    juce::Path rangeTool();
    juce::Path splitTool();
    juce::Path eraserTool();
    juce::Path pencilTool();
    juce::Path muteTool();
    juce::Path undo();
    juce::Path redo();
    juce::Path plus();
    juce::Path instrument();
    juce::Path waveform();
    juce::Path detach();
    juce::Path dock();
}

//==============================================================================
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& background,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override;

    /** Size of the cap of vertical (mixer) faders. */
    static constexpr float faderCapWidth = 30.0f;
    static constexpr float faderCapHeight = 44.0f;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    int getDefaultScrollbarWidth() override { return 10; }

    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawMenuBarBackground (juce::Graphics&, int w, int h, bool isMouseOver, juce::MenuBarComponent&) override;
    juce::Font getMenuBarFont (juce::MenuBarComponent&, int itemIndex, const juce::String& itemText) override;

    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
};

//==============================================================================
/** Flat button with a vector icon. Toggle buttons light up in `onColour`. */
class IconButton final : public juce::Button
{
public:
    IconButton (const juce::String& name, juce::Path icon, juce::Colour onColour = colours::accent);

    void setIcon (juce::Path p) { icon = std::move (p); repaint(); }
    void setDrawBackground (bool b) { drawBackground = b; repaint(); }

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::Path icon;
    juce::Colour onColour;
    bool drawBackground = true;
};

/** Small square toggle with a letter (M / S / R). The letter is shown in the
    button's colour when off, and dark on the filled colour when on, so the
    buttons stay distinguishable in both states. */
class StateButton final : public juce::Button
{
public:
    StateButton (const juce::String& letter, juce::Colour onColour, const juce::String& tooltip);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::Colour onColour;
};

/** Styles a label as an editable numeric value (click to type). */
void makeValueLabel (juce::Label&, float fontHeight = 12.5f);

/** Paints a panel title bar (used at the top of Inspector, Browser ...). */
void drawPanelHeader (juce::Graphics&, juce::Rectangle<int> area, const juce::String& title);

} // namespace sms::theme
