// =============================================================================
//  DELEX by Piradex — cores, tipografia e LookAndFeel
// =============================================================================
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

namespace delex::ui
{
namespace col
{
    inline const juce::Colour bg        { 0xff121317 };
    inline const juce::Colour panel     { 0xff17181d };
    inline const juce::Colour panel2    { 0xff1f2126 };
    inline const juce::Colour deep      { 0xff0c0d10 };
    inline const juce::Colour line      { 0xff24262c };
    inline const juce::Colour line2     { 0xff2c2f36 };
    inline const juce::Colour grid      { 0xff1a1d22 };
    inline const juce::Colour axis      { 0xff2a2e36 };
    inline const juce::Colour track     { 0xff2a2d34 };
    inline const juce::Colour cap       { 0xff202328 };
    inline const juce::Colour capEdge   { 0xff33363e };
    inline const juce::Colour text      { 0xffe9e6e0 };
    inline const juce::Colour text2     { 0xffc9c6c0 };
    inline const juce::Colour soft      { 0xffb5b2ac };
    inline const juce::Colour muted     { 0xff8b8f98 };
    inline const juce::Colour dim       { 0xff6e7380 };
    inline const juce::Colour coral     { 0xffff6b4a };
    inline const juce::Colour coralDeep { 0xff2a1915 };
    inline const juce::Colour cyan      { 0xff3dd6d0 };
    inline const juce::Colour cyanDeep  { 0xff10292a };
    inline const juce::Colour gold      { 0xfff2c14e };
    inline const juce::Colour violet    { 0xffb48cff };
    inline const juce::Colour green     { 0xff4cc38a };
    inline const juce::Colour blue      { 0xff6e7fe0 };
}

// ---------------------------------------------------------------- fontes embutidas (OFL)
struct Fonts
{
    juce::Typeface::Ptr medium, semibold, bold, mono;
    static Fonts& get()
    {
        static Fonts f;
        return f;
    }
private:
    Fonts()
    {
        medium   = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedMedium_ttf,   BinaryData::BarlowSemiCondensedMedium_ttfSize);
        semibold = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedSemiBold_ttf, BinaryData::BarlowSemiCondensedSemiBold_ttfSize);
        bold     = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedBold_ttf,     BinaryData::BarlowSemiCondensedBold_ttfSize);
        mono     = juce::Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoMedium_ttf,         BinaryData::JetBrainsMonoMedium_ttfSize);
    }
};

enum class W { Medium, Semi, Bold };

inline juce::Font font (float h, W w = W::Medium, float tracking = 0.0f)
{
    auto& f = Fonts::get();
    auto tf = w == W::Bold ? f.bold : (w == W::Semi ? f.semibold : f.medium);
    return juce::Font (juce::FontOptions (tf).withHeight (h).withKerningFactor (tracking));
}
inline juce::Font mono (float h) { return juce::Font (juce::FontOptions (Fonts::get().mono).withHeight (h)); }

// texto com espaçamento entre letras (para etiquetas em maiúsculas)
inline void label (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, float h, W w,
                   juce::Colour c, juce::Justification j = juce::Justification::centred, float tracking = 0.12f)
{
    g.setColour (c);
    g.setFont (font (h, w, tracking));
    g.drawText (s, r, j, false);
}

// ---------------------------------------------------------------- LookAndFeel
class LnF : public juce::LookAndFeel_V4
{
public:
    LnF()
    {
        setDefaultSansSerifTypeface (Fonts::get().medium);
        using namespace juce;
        setColour (ResizableWindow::backgroundColourId, col::bg);
        setColour (Slider::rotarySliderFillColourId, col::coral);
        setColour (Slider::textBoxTextColourId, col::text);
        setColour (Label::textColourId, col::text);
        setColour (Label::textWhenEditingColourId, col::text);
        setColour (Label::backgroundWhenEditingColourId, col::deep);
        setColour (Label::outlineWhenEditingColourId, col::cyan);
        setColour (TextEditor::backgroundColourId, col::deep);
        setColour (TextEditor::textColourId, col::text);
        setColour (TextEditor::outlineColourId, col::line2);
        setColour (TextEditor::focusedOutlineColourId, col::cyan);
        setColour (TextEditor::highlightColourId, col::cyan.withAlpha (0.3f));
        setColour (CaretComponent::caretColourId, col::cyan);
        setColour (ComboBox::backgroundColourId, col::deep);
        setColour (ComboBox::textColourId, col::text);
        setColour (ComboBox::outlineColourId, col::line2);
        setColour (ComboBox::arrowColourId, col::muted);
        setColour (PopupMenu::backgroundColourId, col::panel);
        setColour (PopupMenu::textColourId, col::text2);
        setColour (PopupMenu::highlightedBackgroundColourId, col::coralDeep);
        setColour (PopupMenu::highlightedTextColourId, col::coral);
        setColour (PopupMenu::headerTextColourId, col::muted);
        setColour (TooltipWindow::backgroundColourId, col::panel2);
        setColour (TooltipWindow::textColourId, col::text);
        setColour (TooltipWindow::outlineColourId, col::line2);
        setColour (ListBox::backgroundColourId, col::bg);
        setColour (ScrollBar::thumbColourId, col::line2);
        setColour (AlertWindow::backgroundColourId, col::panel);
        setColour (AlertWindow::textColourId, col::text);
        setColour (AlertWindow::outlineColourId, col::line2);
        setColour (TextButton::buttonColourId, col::panel2);
        setColour (TextButton::buttonOnColourId, col::coral);
        setColour (TextButton::textColourOffId, col::text);
        setColour (TextButton::textColourOnId, col::coral);
    }

    // knob: trilho, arco de valor (a partir do início ou do centro), tampa e ponteiro
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        using namespace juce;
        const bool bipolar = s.getProperties()["bipolar"];
        const auto accent = s.findColour (Slider::rotarySliderFillColourId);
        const float size = (float) jmin (w, h);
        const auto c = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
        const float r = size * 0.5f - size * 0.06f;
        const float thick = jmax (2.5f, size * 0.062f);
        const float ang = a0 + pos * (a1 - a0);
        const bool hot = s.isMouseOverOrDragging() && s.isEnabled();

        Path track; track.addCentredArc (c.x, c.y, r, r, 0, a0, a1, true);
        g.setColour (col::track);
        g.strokePath (track, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));

        const float from = bipolar ? (a0 + a1) * 0.5f : a0;
        if (std::abs (ang - from) > 0.001f)
        {
            Path arc; arc.addCentredArc (c.x, c.y, r, r, 0, jmin (from, ang), jmax (from, ang), true);
            g.setColour (s.isEnabled() ? accent : col::dim);
            g.strokePath (arc, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));
        }
        if (bipolar)
        {
            g.setColour (col::dim);
            g.fillEllipse (Rectangle<float> (3.0f, 3.0f).withCentre (c.getPointOnCircumference (r + thick * 1.25f, (a0 + a1) * 0.5f)));
        }

        const float capR = r * 0.72f;
        g.setColour (hot ? col::cap.brighter (0.12f) : col::cap);
        g.fillEllipse (Rectangle<float> (capR * 2, capR * 2).withCentre (c));
        g.setColour (col::capEdge);
        g.drawEllipse (Rectangle<float> (capR * 2, capR * 2).withCentre (c), 1.0f);

        Path ptr;
        ptr.startNewSubPath (c.getPointOnCircumference (capR * 0.30f, ang));
        ptr.lineTo (c.getPointOnCircumference (capR * 0.80f, ang));
        g.setColour (col::text.brighter (0.1f));
        g.strokePath (ptr, PathStrokeType (jmax (1.8f, size * 0.032f), PathStrokeType::curved, PathStrokeType::rounded));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        using namespace juce;
        const float cy = (float) y + (float) h * 0.5f;
        auto tr = Rectangle<float> ((float) x, cy - 2.0f, (float) w, 4.0f);
        g.setColour (col::track); g.fillRoundedRectangle (tr, 2.0f);
        g.setColour (s.findColour (Slider::rotarySliderFillColourId).withAlpha (0.8f));
        const float zero = (float) x + (float) w * (float) s.valueToProportionOfLength (0.0);
        g.fillRoundedRectangle (Rectangle<float> (jmin (zero, pos), cy - 2.0f, std::abs (pos - zero), 4.0f), 2.0f);
        g.setColour (s.isMouseOverOrDragging() ? Colours::white : col::text);
        g.fillEllipse (Rectangle<float> (14.0f, 14.0f).withCentre ({ pos, cy }));
    }

    // botões em pílula: ligados = tinta do acento
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const float rad = b.getProperties().contains ("radius") ? (float) b.getProperties()["radius"] : 6.0f;
        const auto accent = b.findColour (TextButton::buttonOnColourId);
        const bool on = b.getToggleState();
        const bool solid = b.getProperties()["solid"];
        Colour fill, edge;
        if (on && solid) { fill = accent; edge = accent; }
        else if (on)     { fill = accent.withAlpha (0.12f).overlaidWith (col::bg.withAlpha (0.0f)); edge = accent; }
        else             { fill = b.getProperties()["flat"] ? Colours::transparentBlack : b.findColour (TextButton::buttonColourId); edge = col::line2; }
        if (over && ! on) fill = fill.brighter (0.08f), edge = edge.brighter (0.2f);
        if (down) fill = fill.darker (0.1f);
        g.setColour (fill); g.fillRoundedRectangle (r, rad);
        if (! b.getProperties()["noEdge"]) { g.setColour (edge); g.drawRoundedRectangle (r, rad, 1.0f); }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool) override
    {
        using namespace juce;
        const bool on = b.getToggleState();
        const bool solid = b.getProperties()["solid"];
        Colour c = on ? (solid ? col::bg : b.findColour (TextButton::buttonOnColourId)) : b.findColour (TextButton::textColourOffId);
        if (! b.isEnabled()) c = c.withAlpha (0.4f);
        if (over && ! on) c = c.brighter (0.15f);
        const float h = b.getProperties().contains ("fontH") ? (float) b.getProperties()["fontH"] : 12.0f;
        const bool isMono = b.getProperties()["mono"];
        g.setColour (c);
        g.setFont (isMono ? mono (h) : font (h, W::Bold, 0.09f));
        if (b.getProperties()["left"]) g.drawText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (12).withTrimmedRight (10), Justification::centredLeft, false);
        else g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred, false);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (col::deep); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (box.hasKeyboardFocus (false) ? col::cyan : col::line2); g.drawRoundedRectangle (r, 6.0f, 1.0f);
        Path p; const float cx = (float) w - 14.0f, cy = (float) h * 0.5f;
        p.startNewSubPath (cx - 4, cy - 2); p.lineTo (cx, cy + 2); p.lineTo (cx + 4, cy - 2);
        g.setColour (col::muted); g.strokePath (p, PathStrokeType (1.5f));
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return mono (13.0f); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& l) override
    {
        l.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
        l.setFont (getComboBoxFont (box));
        l.setJustificationType (juce::Justification::centredLeft);
    }
    juce::Font getPopupMenuFont() override { return font (15.0f, W::Medium); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        using namespace juce;
        auto r = Rectangle<float> (0, 0, (float) w, (float) h);
        g.setColour (col::panel2); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (col::line2); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (col::text); g.setFont (font (14.0f));
        g.drawFittedText (text, Rectangle<int> (w, h).reduced (8, 4), Justification::centredLeft, 4);
    }
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override
    {
        const int w = juce::jmin (320, (int) juce::GlyphArrangement::getStringWidth (font (14.0f), text) + 20);
        const int lines = (int) std::ceil (juce::GlyphArrangement::getStringWidth (font (14.0f), text) / 300.0f);
        const int h = 14 + juce::jmax (1, lines) * 17;
        return juce::Rectangle<int> (pos.x + 12, pos.y + 18, w, h).constrainedWithin (parent);
    }
};

} // namespace delex::ui
