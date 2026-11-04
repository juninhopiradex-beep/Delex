// =============================================================================
//  DELEX by Piradex — componentes da interface
// =============================================================================
#pragma once

#include "Look.h"
#include "PluginProcessor.h"
#include <complex>

namespace delex::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

inline juce::String fineTip() { return tr ("\n(Duplo clique repõe · Shift arrasta com precisão · duplo clique no valor para escrever)", "\n(Double-click resets · Shift-drag for precision · double-click the value to type)"); }

// ---------------------------------------------------------------- MIDI learn (clique direito)
inline DelexProcessor* processorOf (APVTS& s) { return dynamic_cast<DelexProcessor*> (&s.processor); }

inline void showParamMenu (APVTS& s, const juce::String& pid, juce::Component* target, std::function<void()> afterLearn = {})
{
    auto* proc = processorOf (s);
    auto* prm = s.getParameter (pid);
    if (proc == nullptr || prm == nullptr) return;
    juce::PopupMenu m;
    const int cc = proc->midiCcFor (pid);
    m.addSectionHeader (prm->getName (40));
    m.addItem (1, proc->isLearning (pid) ? tr ("A aprender... (mexe num controlador)", "Learning... (move a controller)")
                                         : tr ("Aprender MIDI", "MIDI learn"));
    if (cc >= 0) m.addItem (2, tr ("Esquecer MIDI (CC ", "Forget MIDI (CC ") + juce::String (cc) + ")");
    m.addSeparator();
    m.addItem (3, tr ("Repor valor", "Reset value"));
    juce::Component::SafePointer<juce::Component> safe (target);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target),
        [proc, prm, pid, safe, afterLearn] (int r)
        {
            if (r == 1) { proc->startMidiLearn (pid); if (afterLearn) afterLearn(); }
            else if (r == 2) proc->forgetMidi (pid);
            else if (r == 3) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getDefaultValue()); prm->endChangeGesture(); }
            if (safe != nullptr) safe->repaint();
        });
}

// ---------------------------------------------------------------- slider com ajuste fino (Shift)
class FineSlider : public juce::Slider
{
public:
    std::function<void()> onRightClick;
    FineSlider()
    {
        setSliderStyle (RotaryHorizontalVerticalDrag);
        setTextBoxStyle (NoTextBox, false, 0, 0);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        setVelocityModeParameters (0.35, 1, 0.0, false);
        setMouseDragSensitivity (220);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && onRightClick) { popupClick = true; onRightClick(); return; }
        popupClick = false;
        setVelocityBasedMode (e.mods.isShiftDown());
        juce::Slider::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (! popupClick) juce::Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override { if (! popupClick) juce::Slider::mouseUp (e); popupClick = false; }
private:
    bool popupClick = false;
};

// ---------------------------------------------------------------- knob com nome, valor editável e dica
class Knob : public juce::Component, private juce::Timer
{
public:
    enum class Size { Big, Small };

    Knob (APVTS& s, const char* pid, const juce::String& nm, const juce::String& hintText, juce::Colour accent,
          Size sz, bool bipolar, const juce::String& tip)
        : state (s), paramId (pid), name (nm), hint (hintText), colour (accent), size (sz)
    {
        slider.onRightClick = [this] { showParamMenu (state, paramId, this, [this] { startTimerHz (10); }); };
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.getProperties().set ("bipolar", bipolar);
        addAndMakeVisible (slider);
        att = std::make_unique<APVTS::SliderAttachment> (s, pid, slider);
        if (auto* p = s.getParameter (pid))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (tip + fineTip() + tr ("\n(Clique direito: MIDI learn)", "\n(Right-click: MIDI learn)"));

        value.setJustificationType (juce::Justification::centred);
        value.setFont (size == Size::Big ? mono (15.0f) : mono (12.5f));
        value.setColour (juce::Label::textColourId, accent);
        value.setEditable (false, true, false);
        value.setTooltip (tr ("Duplo clique para escrever um valor", "Double-click to type a value"));
        value.onTextChange = [this]
        {
            // um valor escrito à mão é um gesto próprio (passo de desfazer separado)
            juce::Slider::ScopedDragNotification gesture (slider);
            slider.setValue (slider.getValueFromText (value.getText()), juce::sendNotificationSync);
            refresh();
        };
        addAndMakeVisible (value);
        slider.onValueChange = [this] { refresh(); };
        refresh();
    }

    void refresh() { value.setText (slider.getTextFromValue (slider.getValue()), juce::dontSendNotification); }

    void setAccent (juce::Colour c)
    {
        colour = c;
        slider.setColour (juce::Slider::rotarySliderFillColourId, c);
        value.setColour (juce::Label::textColourId, c);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (size == Size::Big)
        {
            label (g, name, r.removeFromTop (20.0f), 14.5f, W::Bold, col::text, juce::Justification::centred, 0.14f);
            g.setColour (col::muted); g.setFont (font (12.5f));
            g.drawText (hint, getLocalBounds().removeFromBottom (18), juce::Justification::centred, false);
        }
        else
            label (g, name, juce::Rectangle<float> (0, (float) value.getY() - 16.0f, r.getWidth(), 16.0f), 12.0f, W::Semi, col::text2, juce::Justification::centred, 0.08f);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        auto* proc = processorOf (state);
        if (proc == nullptr) return;
        if (proc->isLearning (paramId))
        {
            g.setColour (col::cyan.withAlpha (blink ? 0.9f : 0.35f));
            g.drawEllipse (slider.getBounds().toFloat().reduced (2.0f), 2.0f);
            g.setFont (mono (9.5f)); g.drawText ("MIDI", slider.getBounds().removeFromTop (12), juce::Justification::centred);
        }
        else if (const int cc = proc->midiCcFor (paramId); cc >= 0)
        {
            g.setColour (col::cyan.withAlpha (0.8f)); g.setFont (mono (9.0f));
            g.drawText ("CC" + juce::String (cc), slider.getBounds().removeFromBottom (11).translated (0, 2), juce::Justification::centred);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        if (size == Size::Big)
        {
            r.removeFromTop (20);
            r.removeFromBottom (18);
            auto v = r.removeFromBottom (22);
            value.setBounds (v);
            const int k = juce::jmin (r.getWidth(), r.getHeight());
            slider.setBounds (r.withSizeKeepingCentre (k, k));
        }
        else
        {
            auto v = r.removeFromBottom (17);
            value.setBounds (v);
            r.removeFromBottom (16);
            const int k = juce::jmin (r.getWidth(), r.getHeight());
            slider.setBounds (r.withSizeKeepingCentre (k, k));
        }
    }

    FineSlider slider;
    juce::Label value;
    juce::String getParamId() const { return paramId; }

private:
    void timerCallback() override
    {
        blink = ! blink; repaint();
        auto* proc = processorOf (state);
        if (proc == nullptr || ! proc->isLearning (paramId)) { stopTimer(); repaint(); }
    }
    APVTS& state;
    juce::String paramId;
    bool blink = false;
    juce::String name, hint;
    juce::Colour colour;
    Size size;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// ---------------------------------------------------------------- botão-pílula ligado a parâmetro bool
class ToggleChip : public juce::TextButton
{
public:
    ToggleChip (APVTS& s, const char* pid, const juce::String& txt, juce::Colour accent, const juce::String& tip)
        : juce::TextButton (txt)
    {
        setClickingTogglesState (true);
        setColour (buttonOnColourId, accent);
        setTooltip (tip);
        att = std::make_unique<APVTS::ButtonAttachment> (s, pid, *this);
        state = &s; paramId = pid;
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { popupClick = true; showParamMenu (*state, paramId, this); return; }
        popupClick = false; juce::TextButton::mouseDown (e);
    }
    void mouseUp (const juce::MouseEvent& e) override { if (! popupClick) juce::TextButton::mouseUp (e); popupClick = false; }
    void mouseDrag (const juce::MouseEvent& e) override { if (! popupClick) juce::TextButton::mouseDrag (e); }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        juce::TextButton::paintButton (g, over, down);
        if (auto* proc = processorOf (*state))
        {
            const bool learn = proc->isLearning (paramId);
            if (learn || proc->midiCcFor (paramId) >= 0)
            {
                g.setColour (col::cyan); g.fillEllipse (juce::Rectangle<float> (5, 5).withCentre ({ (float) getWidth() - 6.0f, 6.0f }));
            }
        }
    }
private:
    APVTS* state = nullptr;
    juce::String paramId;
    bool popupClick = false;
    std::unique_ptr<APVTS::ButtonAttachment> att;
};

// ---------------------------------------------------------------- selector segmentado (parâmetro de escolha)
class Segmented : public juce::Component
{
public:
    Segmented (APVTS& s, const char* pid, const juce::StringArray& labels, juce::Colour accent, const juce::String& tip, bool vertical = false)
        : isVertical (vertical)
    {
        auto* prm = s.getParameter (pid);
        jassert (prm != nullptr);
        for (int i = 0; i < labels.size(); ++i)
        {
            auto* b = buttons.add (new juce::TextButton (labels[i]));
            b->setColour (juce::TextButton::buttonOnColourId, accent);
            b->getProperties().set ("radius", 0.0f);
            b->getProperties().set ("noEdge", true);
            b->getProperties().set ("fontH", 11.5f);
            b->setTooltip (tip);
            b->onClick = [this, i] { att->setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (b);
        }
        att = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v) { select ((int) std::lround (v)); }, s.undoManager);
        att->sendInitialUpdate();
    }

    void select (int idx) { for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (i == idx, juce::dontSendNotification); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (col::deep); g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    }
    void paintOverChildren (juce::Graphics& g) override
    {
        g.setColour (col::line2); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (1);
        const int n = buttons.size();
        for (int i = 0; i < n; ++i)
        {
            if (isVertical) buttons[i]->setBounds (r.removeFromTop (r.getHeight() / (n - i)));
            else            buttons[i]->setBounds (r.removeFromLeft (r.getWidth() / (n - i)));
        }
    }

private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> att;
    bool isVertical;
};

// ---------------------------------------------------------------- caixa de escolha ligada a parâmetro
class ChoiceBox : public juce::ComboBox
{
public:
    ChoiceBox (APVTS& s, const char* pid, const juce::String& tip, const juce::StringArray& labels = {})
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (pid)))
            addItemList (labels.size() == c->choices.size() ? labels : c->choices, 1);
        setTooltip (tip);
        att = std::make_unique<APVTS::ComboBoxAttachment> (s, pid, *this);
    }
private:
    std::unique_ptr<APVTS::ComboBoxAttachment> att;
};

// ---------------------------------------------------------------- botão com ícone desenhado
class IconButton : public juce::Button
{
public:
    using Drawer = std::function<void (juce::Graphics&, juce::Rectangle<float>, juce::Colour)>;
    IconButton (const juce::String& nm, Drawer d) : juce::Button (nm), draw (std::move (d)) {}
    juce::Colour colour = col::soft;
    bool outline = true;
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        if (outline)
        {
            g.setColour (over ? col::panel2.brighter (0.08f) : juce::Colours::transparentBlack);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (col::line2); g.drawRoundedRectangle (r, 6.0f, 1.0f);
        }
        auto c = getToggleState() ? col::gold : colour;
        if (over) c = c.brighter (0.2f);
        if (down) c = c.darker (0.2f);
        if (! isEnabled()) c = c.withAlpha (0.35f);
        draw (g, r, c);
    }
private:
    Drawer draw;
};

namespace icons
{
    inline void chevron (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool right)
    {
        juce::Path p; const auto cc = r.getCentre(); const float s = 4.0f;
        p.startNewSubPath (cc.x + (right ? -s * 0.5f : s * 0.5f), cc.y - s);
        p.lineTo (cc.x + (right ? s * 0.5f : -s * 0.5f), cc.y);
        p.lineTo (cc.x + (right ? -s * 0.5f : s * 0.5f), cc.y + s);
        g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    inline void undoArrow (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool redo)
    {
        juce::Path p; const auto cc = r.getCentre();
        const float d = redo ? -1.0f : 1.0f;
        p.startNewSubPath (cc.x - 4.5f * d, cc.y - 2.0f);
        p.lineTo (cc.x + 1.5f * d, cc.y - 2.0f);
        p.cubicTo (cc.x + 5.5f * d, cc.y - 2.0f, cc.x + 5.5f * d, cc.y + 4.5f, cc.x + 1.5f * d, cc.y + 4.5f);
        p.lineTo (cc.x - 1.5f * d, cc.y + 4.5f);
        p.startNewSubPath (cc.x - 2.0f * d, cc.y - 4.5f); p.lineTo (cc.x - 4.5f * d, cc.y - 2.0f); p.lineTo (cc.x - 2.0f * d, cc.y + 0.5f);
        g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    inline void star (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool filled)
    {
        juce::Path p; p.addStar (r.getCentre(), 5, 3.2f, 7.5f, 0.0f);
        g.setColour (c);
        if (filled) g.fillPath (p); else g.strokePath (p, juce::PathStrokeType (1.3f));
    }
    inline void cross (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        const auto cc = r.getCentre(); juce::Path p;
        p.startNewSubPath (cc.x - 4, cc.y - 4); p.lineTo (cc.x + 4, cc.y + 4);
        p.startNewSubPath (cc.x + 4, cc.y - 4); p.lineTo (cc.x - 4, cc.y + 4);
        g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    inline void logo (juce::Graphics& g, juce::Rectangle<float> r)
    {
        juce::Path p;
        p.startNewSubPath (r.getX() + 2, r.getBottom() - 2); p.lineTo (r.getX() + 8, r.getY() + 2);
        p.startNewSubPath (r.getX() + 8, r.getBottom() - 2); p.lineTo (r.getX() + 14, r.getY() + 2);
        g.setColour (col::coral);
        g.strokePath (p, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col::cyan);
        g.fillEllipse (r.getX() + 14.2f, r.getBottom() - 4.8f, 3.6f, 3.6f);
    }
}

// ---------------------------------------------------------------- medidor estéreo vertical
class Meter : public juce::Component
{
public:
    Meter (std::atomic<float>* l, std::atomic<float>* r, juce::String txt, bool colourful)
        : pl (l), pr (r), text (std::move (txt)), colourBars (colourful) {}
    void tick()
    {
        const float a = norm (pl->load()), b = norm (pr->load());
        if (std::abs (a - vl) + std::abs (b - vr) > 0.003f) { vl = a; vr = b; repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        label (g, text, r.removeFromBottom (14.0f), 10.0f, W::Medium, col::muted, juce::Justification::centred, 0.0f);
        r.removeFromBottom (3.0f);
        const float bw = 5.0f, gap = 2.0f;
        auto bars = r.withSizeKeepingCentre (bw * 2 + gap, r.getHeight());
        for (int i = 0; i < 2; ++i)
        {
            auto b = bars.removeFromLeft (bw); bars.removeFromLeft (gap);
            g.setColour (col::grid); g.fillRoundedRectangle (b, 2.0f);
            const float v = i == 0 ? vl : vr;
            auto f = b.withTop (b.getBottom() - b.getHeight() * v);
            if (colourBars)
            {
                juce::ColourGradient cg (col::cyan, 0, b.getBottom(), col::coral, 0, b.getY(), false);
                cg.addColour (0.78, col::cyan); cg.addColour (0.9, col::gold);
                g.setGradientFill (cg);
            }
            else g.setColour (juce::Colour (0xff5d6370));
            g.fillRoundedRectangle (f, 2.0f);
        }
    }
private:
    static float norm (float pk) { const float db = juce::Decibels::gainToDecibels (pk, -60.0f); return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f); }
    std::atomic<float>* pl; std::atomic<float>* pr;
    juce::String text; bool colourBars;
    float vl = 0, vr = 0;
};

// ---------------------------------------------------------------- ECHO FIELD: posição estéreo de cada repetição
class EchoField : public juce::Component
{
public:
    EchoField (DelexProcessor& p, bool isCompact) : proc (p), compact (isCompact) {}

    void tick()
    {
        const auto v = proc.raw.load();
        const auto params = toEngine (v, proc.hostBpm.load(), proc.currentSampleRate.load());
        Engine::Echo e[24];
        const int n = Engine::echoMap (params, e, compact ? 14 : 14);
        bool changed = n != count;
        for (int i = 0; i < n && ! changed; ++i)
            changed = std::abs (e[i].l - echoes[(size_t) i].l) > 1e-4f || std::abs (e[i].r - echoes[(size_t) i].r) > 1e-4f;
        const float lvl = juce::jmax (proc.outPeak[0].load(), proc.outPeak[1].load());
        if (std::abs (lvl - live) > 0.01f) { live = lvl; changed = true; }
        const juce::String lbl = timeLabel (v, params);
        if (lbl != timeText) { timeText = lbl; changed = true; }
        frozen = v.freeze > 0.5f;
        if (changed)
        {
            count = n;
            for (int i = 0; i < n; ++i) echoes[(size_t) i] = e[i];
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        g.setColour (col::deep); g.fillRoundedRectangle (r, 8.0f);
        g.setColour (col::line); g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

        label (g, "ECHO FIELD", { 12.0f, compact ? 5.0f : 8.0f, 120.0f, 16.0f }, compact ? 10.5f : 11.0f, W::Semi, col::muted, Justification::centredLeft, 0.18f);
        g.setColour (frozen ? col::cyan : col::muted); g.setFont (mono (11.0f));
        g.drawText (frozen ? String ("FREEZE  ") + timeText : timeText, Rectangle<float> (r.getWidth() - 260.0f, compact ? 5.0f : 8.0f, 248.0f, 16.0f), Justification::centredRight);

        const float left = compact ? 30.0f : 46.0f, right = r.getWidth() - 14.0f;
        const float top = compact ? 22.0f : 40.0f, bottom = r.getHeight() - (compact ? 8.0f : 34.0f);
        const float mid = (top + bottom) * 0.5f, half = (bottom - top) * 0.5f;
        if (! compact)
        {
            g.setColour (col::dim); g.setFont (font (11.0f));
            g.drawText ("L", Rectangle<float> (14, top - 6, 12, 12), Justification::centred);
            g.drawText ("R", Rectangle<float> (14, bottom - 6, 12, 12), Justification::centred);
            Path dash; float dl[] = { 2.0f, 4.0f };
            for (float yy : { top, bottom })
            {
                Path ln; ln.startNewSubPath (left - 10, yy); ln.lineTo (right, yy);
                PathStrokeType (1.0f).createDashedStroke (dash, ln, dl, 2);
            }
            g.setColour (Colour (0xff1c1f25)); g.fillPath (dash);
        }
        g.setColour (col::axis); g.drawHorizontalLine ((int) mid, left - 10, right);

        if (count <= 0) return;
        const float step = (right - left - 10.0f) / 13.0f;
        // ponto 0 = sinal original
        auto pt = [&] (int i) -> Point<float>
        {
            const auto& e = echoes[(size_t) i];
            const float pan = (std::abs (e.r) - std::abs (e.l)) / juce::jmax (1.0e-6f, std::abs (e.l) + std::abs (e.r));
            return { left + step * (float) (i + 1), mid + pan * half };
        };
        Path trail; trail.startNewSubPath (left, mid);
        for (int i = 0; i < count; ++i) trail.lineTo (pt (i));
        g.setColour (Colour (0xff3a3f49)); g.strokePath (trail, PathStrokeType (1.2f));

        const float boost = 0.75f + 0.25f * jlimit (0.0f, 1.0f, live * 2.0f);
        g.setColour (col::coral); g.fillEllipse (Rectangle<float> (compact ? 12.0f : 22.0f, compact ? 12.0f : 22.0f).withCentre ({ left, mid }));
        for (int i = 0; i < count; ++i)
        {
            const auto& e = echoes[(size_t) i];
            const float amp = jlimit (0.0f, 1.0f, std::sqrt (0.5f * (e.l * e.l + e.r * e.r)));
            const auto p = pt (i);
            const auto c = col::coral.interpolatedWith (col::cyan, (float) i / 13.0f);
            const float alpha = (0.35f + 0.65f * std::sqrt (amp)) * boost;
            g.setColour (c.withAlpha (alpha * 0.9f));
            g.drawLine (p.x, mid, p.x, p.y, 2.0f);
            const float rad = (compact ? 2.0f : 3.0f) + (compact ? 4.0f : 8.5f) * std::sqrt (amp);
            const bool inverted = e.l * e.r < 0.0f && juce::jmin (std::abs (e.l), std::abs (e.r)) > 0.35f * juce::jmax (std::abs (e.l), std::abs (e.r));
            auto dot = Rectangle<float> (rad * 2, rad * 2).withCentre (p);
            if (inverted) { g.setColour (c.withAlpha (alpha)); g.drawEllipse (dot, 2.0f); }   // fase invertida entre L e R
            else          { g.setColour (c.withAlpha (alpha)); g.fillEllipse (dot); }
        }
        if (! compact)
        {
            g.setColour (col::dim); g.setFont (mono (10.0f));
            for (int i = 0; i <= 13; i += 2)
                g.drawText (String (i), Rectangle<float> (left + step * (float) i - 10.0f, r.getHeight() - 24.0f, 20.0f, 14.0f), Justification::centred);
        }
    }

private:
    static juce::String timeLabel (const Values& v, const Params& p)
    {
        juce::String ms = p.timeMsL < 1000.0f ? num (p.timeMsL, 0) + " ms" : juce::String (p.timeMsL / 1000.0f, 2) + " s";
        if (v.sync > 0.5f)
        {
            const auto& d = divisions();
            return juce::String (d[(size_t) juce::jlimit (0, (int) d.size() - 1, (int) std::lround (v.div))].name) + " = " + ms;
        }
        return ms;
    }

    DelexProcessor& proc;
    bool compact;
    std::array<Engine::Echo, 24> echoes {};
    int count = -1;
    float live = 0;
    bool frozen = false;
    juce::String timeText;
};

// ---------------------------------------------------------------- goniómetro + correlação
class PhasePanel : public juce::Component
{
public:
    explicit PhasePanel (DelexProcessor& p, bool isCompact) : proc (p), compact (isCompact) {}

    void tick()
    {
        int s1, n1, s2, n2;
        const int ready = proc.scopeFifo.getNumReady();
        proc.scopeFifo.prepareToRead (ready, s1, n1, s2, n2);
        auto take = [&] (int start, int num)
        {
            for (int i = 0; i < num; ++i)
            {
                pts[(size_t) head] = { proc.scopeData[(size_t) (start + i) * 2], proc.scopeData[(size_t) (start + i) * 2 + 1] };
                head = (head + 1) % (int) pts.size();
            }
        };
        take (s1, n1); take (s2, n2);
        proc.scopeFifo.finishedRead (n1 + n2);
        corr = proc.correlation.load();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        g.setColour (col::deep); g.fillRoundedRectangle (r, 8.0f);
        g.setColour (col::line); g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        auto in = r.reduced (12.0f, compact ? 8.0f : 9.0f);

        if (! compact)
        {
            label (g, tr ("FASE", "PHASE"), in.removeFromTop (16.0f), 11.0f, W::Semi, col::muted, Justification::centredLeft, 0.18f);
            auto sc = in.removeFromTop (in.getHeight() - 56.0f).reduced (4.0f);
            const float s = jmin (sc.getWidth(), sc.getHeight()) * 0.5f;
            const auto c = sc.getCentre();
            Path dia; dia.startNewSubPath (c.x, c.y - s); dia.lineTo (c.x + s, c.y); dia.lineTo (c.x, c.y + s); dia.lineTo (c.x - s, c.y); dia.closeSubPath();
            g.setColour (col::axis); g.strokePath (dia, PathStrokeType (1.0f));
            g.setColour (Colour (0xff1f2228));
            g.drawVerticalLine ((int) c.x, c.y - s, c.y + s); g.drawHorizontalLine ((int) c.y, c.x - s, c.x + s);
            g.setColour (col::dim); g.setFont (font (10.0f));
            g.drawText ("M", Rectangle<float> (c.x - 6, c.y - s + 2, 12, 12), Justification::centred);
            g.drawText ("L", Rectangle<float> (c.x - s * 0.62f - 6, c.y - s * 0.62f, 12, 12), Justification::centred);
            g.drawText ("R", Rectangle<float> (c.x + s * 0.62f - 6, c.y - s * 0.62f, 12, 12), Justification::centred);
            float peak = 1.0e-3f;
            for (auto& p : pts) peak = jmax (peak, std::abs (p.x), std::abs (p.y));
            const float k = s * 0.9f / jmax (0.25f, peak);
            for (size_t i = 0; i < pts.size(); ++i)
            {
                const auto& p = pts[i];
                const float x = (p.y - p.x) * 0.7071f * k, y = -(p.x + p.y) * 0.7071f * k;
                const float age = (float) (((int) i - head + (int) pts.size()) % (int) pts.size()) / (float) pts.size();
                g.setColour (col::cyan.withAlpha (0.08f + 0.5f * age));
                g.fillRect (c.x + jlimit (-s, s, x), c.y + jlimit (-s, s, y), 1.6f, 1.6f);
            }
        }

        // barra de correlação
        auto row = in.removeFromTop (14.0f);
        g.setFont (mono (10.0f)); g.setColour (col::muted);
        if (compact) g.drawText (tr ("CORRELAÇÃO", "CORRELATION"), row, Justification::centredLeft);
        else { g.drawText (String::fromUTF8 ("\xe2\x88\x92") + "1", row, Justification::centredLeft); g.drawText ("+1", row, Justification::centredRight); }
        g.setColour (corr < -0.2f ? col::coral : col::cyan);
        g.drawText ((compact ? String() : tr ("CORRELAÇÃO ", "CORRELATION ")) + (corr >= 0 ? "+" : "") + String (corr, 2), row, compact ? Justification::centredRight : Justification::centred);
        in.removeFromTop (4.0f);
        auto bar = in.removeFromTop (6.0f);
        g.setColour (Colour (0xff1c1f25)); g.fillRoundedRectangle (bar, 3.0f);
        const float mid = bar.getCentreX(), x = mid + corr * bar.getWidth() * 0.5f;
        g.setColour (corr < -0.2f ? col::coral : col::cyan);
        g.fillRoundedRectangle (Rectangle<float> (jmin (mid, x), bar.getY(), jmax (2.0f, std::abs (x - mid)), 6.0f), 3.0f);
        in.removeFromTop (compact ? 6.0f : 8.0f);

        // badge mono / limitador do loop
        if (compact)
        {
            auto row2 = in.removeFromTop (14.0f);
            g.setColour (col::muted); g.setFont (mono (10.0f));
            g.drawText ("LOOP", row2, Justification::centredLeft);
            const float lim = proc.loopLimit.load();
            g.drawText (lim < -0.05f ? String (lim, 1) + " dB lim." : String ("ok"), row2, Justification::centredRight);
        }
        else
        {
            const bool bad = corr < -0.2f;
            const String t = bad ? tr ("FASE INVERTIDA", "PHASE INVERTED") : String ("MONO OK");
            auto badge = in.removeFromTop (17.0f).withWidth (bad ? 98.0f : 62.0f);
            g.setColour ((bad ? col::coral : col::cyan).withAlpha (0.35f)); g.drawRoundedRectangle (badge, 3.0f, 1.0f);
            label (g, t, badge, 10.5f, W::Semi, bad ? col::coral : col::cyan, Justification::centred, 0.1f);
        }
    }

private:
    DelexProcessor& proc;
    bool compact;
    std::array<juce::Point<float>, 700> pts {};
    int head = 0;
    float corr = 0;
};

// ---------------------------------------------------------------- curva do EQ do loop (6 nós arrastáveis)
//  0 low cut · 1 pico 1 · 2 high cut · 3 pico 2 · 4 low shelf · 5 high shelf
class EqDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void (int band)> onNodeSelected;   // 1 = pico 1, 3 = pico 2, 4/5 = shelves

    explicit EqDisplay (DelexProcessor& p) : proc (p)
    {
        setTooltip (tr ("Arrasta os nós: cortes, dois picos e dois shelves. Roda do rato num pico muda o Q. Duplo clique repõe.",
                        "Drag the nodes: cuts, two peaks and two shelves. Mouse wheel on a peak changes Q. Double-click resets."));
    }

    void tick()
    {
        const auto v = proc.raw.load();
        float sig = 0; const float* d = v.data();
        for (int i = 0; i < Values::count(); ++i) sig += d[i] * (float) ((i % 13) + 1) * 0.001f;
        if (std::abs (sig - lastSig) > 1.0e-6f) { lastSig = sig; vals = v; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        g.setColour (col::deep); g.fillRoundedRectangle (r, 8.0f);
        g.setColour (col::line); g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        plot = r.reduced (36.0f, 12.0f).withTrimmedBottom (10.0f);

        g.setColour (col::grid);
        for (float f : { 100.0f, 1000.0f, 10000.0f }) g.drawVerticalLine ((int) xOf (f), r.getY(), r.getBottom());
        for (float d : { -12.0f, 12.0f }) g.drawHorizontalLine ((int) yOf (d), r.getX(), r.getRight());
        g.setColour (col::axis); g.drawHorizontalLine ((int) yOf (0.0f), r.getX(), r.getRight());
        g.setColour (col::dim); g.setFont (mono (10.0f));
        g.drawText ("100", Rectangle<float> (xOf (100) + 3, r.getBottom() - 16, 40, 12), Justification::centredLeft);
        g.drawText ("1k", Rectangle<float> (xOf (1000) + 3, r.getBottom() - 16, 40, 12), Justification::centredLeft);
        g.drawText ("10k", Rectangle<float> (xOf (10000) + 3, r.getBottom() - 16, 40, 12), Justification::centredLeft);
        g.drawText ("+12", Rectangle<float> (6, yOf (12) - 6, 28, 12), Justification::centredLeft);
        g.drawText ("0", Rectangle<float> (6, yOf (0) - 6, 28, 12), Justification::centredLeft);
        g.drawText (String::fromUTF8 ("\xe2\x88\x92") + "12", Rectangle<float> (6, yOf (-12) - 6, 28, 12), Justification::centredLeft);

        auto curve = [&] (float lc, float hc, bool withComp)
        {
            Path p;
            const float comp = withComp ? std::max ({ 0.0f, vals.peak_gain, vals.peak2_gain, vals.ls_gain, vals.hs_gain }) * vals.compensate * 0.01f : 0.0f;
            for (int i = 0; i <= 240; ++i)
            {
                const float f = 20.0f * std::pow (1000.0f, (float) i / 240.0f);
                const float db = magDb (f, lc, hc) - comp;
                const auto pt = Point<float> (xOf (f), jlimit (r.getY() + 2, r.getBottom() - 2, yOf (db)));
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            return p;
        };
        const auto eff = toEngine (vals, 120.0, 48000.0);
        if (std::abs (vals.tone) > 0.5f)
        {
            g.setColour (col::cyan.withAlpha (0.55f));
            g.strokePath (curve (eff.lowCutHz, eff.highCutHz, false), PathStrokeType (1.2f));
        }
        auto main = curve (vals.low_cut, vals.high_cut, false);
        Path fill (main); fill.lineTo (xOf (20000), r.getBottom()); fill.lineTo (xOf (20), r.getBottom()); fill.closeSubPath();
        g.setColour (col::coral.withAlpha (0.10f)); g.fillPath (fill);
        g.setColour (col::coral); g.strokePath (main, PathStrokeType (2.0f));
        if (vals.compensate > 0.5f && std::max ({ vals.peak_gain, vals.peak2_gain, vals.ls_gain, vals.hs_gain }) > 0.05f)
        {
            Path dashed; float dl[] = { 3.0f, 3.0f };
            PathStrokeType (1.0f).createDashedStroke (dashed, curve (vals.low_cut, vals.high_cut, true), dl, 2);
            g.setColour (col::muted.withAlpha (0.6f)); g.fillPath (dashed);
        }

        static const char* tags[6] = { "LC", "1", "HC", "2", "LS", "HS" };
        for (int i = 0; i < 6; ++i)
        {
            const auto c = node (i);
            const bool hot = i == dragging || i == hover;
            const auto colr = nodeColour (i);
            g.setColour (col::bg); g.fillEllipse (Rectangle<float> (16, 16).withCentre (c));
            g.setColour (colr); g.drawEllipse (Rectangle<float> (16, 16).withCentre (c), hot ? 2.6f : 2.0f);
            g.setFont (mono (8.5f)); g.drawText (tags[i], Rectangle<float> (16, 16).withCentre (c), Justification::centred);
        }
        if (std::abs (vals.tone) > 0.5f)
        {
            g.setColour (col::cyan); g.setFont (mono (10.0f));
            g.drawText ("TONE " + num (vals.tone, 0) + tr (" (efectivo)", " (effective)"), r.reduced (10, 6), Justification::topRight);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { const int h = hit (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = hit (e.position);
        if (dragging < 0) return;
        if (onNodeSelected && dragging != 0 && dragging != 2) onNodeSelected (dragging);
        proc.undo.beginNewTransaction();
        for (auto* p : params (dragging)) p->beginChangeGesture();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging < 0) return;
        const float f = freqOf (e.position.x);
        const float db = juce::jlimit (-18.0f, 18.0f, dbOf (e.position.y));
        switch (dragging)
        {
            case 0: set (id::low_cut, f); break;
            case 2: set (id::high_cut, f); break;
            case 1: set (id::peak_freq, f);  set (id::peak_gain, db); break;
            case 3: set (id::peak2_freq, f); set (id::peak2_gain, db); break;
            case 4: set (id::ls_freq, f);    set (id::ls_gain, db); break;
            default: set (id::hs_freq, f);   set (id::hs_gain, db); break;
        }
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging >= 0) for (auto* p : params (dragging)) p->endChangeGesture();
        dragging = -1; repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int h = hit (e.position);
        if (h < 0) return;
        for (auto* p : params (h)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int h = hit (e.position);
        if (h != 1 && h != 3) return;
        auto* q = proc.apvts.getParameter (h == 1 ? id::peak_q : id::peak2_q);
        q->beginChangeGesture(); q->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, q->getValue() + w.deltaY * 0.15f)); q->endChangeGesture();
    }

    // magnitude da resposta completa (aproximação analógica de cada secção)
    float magDb (float f, float lc, float hc) const
    {
        using C = std::complex<float>;
        const int n = vals.cut_slope < 0.5f ? 1 : (vals.cut_slope < 1.5f ? 2 : 4);
        const float a = std::pow (f / lc, (float) n), b = std::pow (f / hc, (float) n);
        float m = a / std::sqrt (1.0f + a * a) / std::sqrt (1.0f + b * b);
        auto bell = [f] (float f0, float gDb, float Q)
        {
            const float A = std::pow (10.0f, gDb / 40.0f), w = f, d = f0 * f0 - w * w;
            const float num = d * d + std::pow (w * A * f0 / Q, 2.0f), den = d * d + std::pow (w * f0 / (A * Q), 2.0f);
            return std::sqrt (num / juce::jmax (1.0e-12f, den));
        };
        auto shelf = [f] (float f0, float gDb, bool low)
        {
            const float A = std::pow (10.0f, gDb / 40.0f), sq = std::sqrt (A) / 0.70710678f;
            const C s (0.0f, f / f0);
            const C hlow  = A * (s * s + sq * s + A) / (A * s * s + sq * s + 1.0f);
            const C hhigh = A * (A * s * s + sq * s + 1.0f) / (s * s + sq * s + A);
            return std::abs (low ? hlow : hhigh);
        };
        m *= bell (vals.peak_freq, vals.peak_gain, vals.peak_q);
        m *= bell (vals.peak2_freq, vals.peak2_gain, vals.peak2_q);
        m *= shelf (vals.ls_freq, vals.ls_gain, true);
        m *= shelf (vals.hs_freq, vals.hs_gain, false);
        return juce::Decibels::gainToDecibels (m, -60.0f);
    }

private:
    static juce::Colour nodeColour (int i)
    {
        switch (i) { case 1: return col::coral; case 3: return col::gold; case 4: case 5: return col::violet; default: return col::cyan; }
    }
    float xOf (float f) const { return plot.getX() + plot.getWidth() * std::log10 (f / 20.0f) / 3.0f; }
    float freqOf (float x) const { return 20.0f * std::pow (10.0f, 3.0f * juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth())); }
    float yOf (float db) const { return plot.getCentreY() - db / 18.0f * plot.getHeight() * 0.5f; }
    float dbOf (float y) const { return (plot.getCentreY() - y) / (plot.getHeight() * 0.5f) * 18.0f; }
    juce::Point<float> node (int i) const
    {
        switch (i)
        {
            case 0: return { xOf (vals.low_cut), yOf (magDb (vals.low_cut, vals.low_cut, vals.high_cut)) };
            case 2: return { xOf (vals.high_cut), yOf (magDb (vals.high_cut, vals.low_cut, vals.high_cut)) };
            case 1: return { xOf (vals.peak_freq), yOf (vals.peak_gain) };
            case 3: return { xOf (vals.peak2_freq), yOf (vals.peak2_gain) };
            case 4: return { xOf (vals.ls_freq), yOf (vals.ls_gain * 0.5f) };
            default: return { xOf (vals.hs_freq), yOf (vals.hs_gain * 0.5f) };
        }
    }
    int hit (juce::Point<float> p) const
    {
        int best = -1; float bd = 16.0f;
        for (int i = 0; i < 6; ++i) { const float d = node (i).getDistanceFrom (p); if (d < bd) { bd = d; best = i; } }
        return best;
    }
    std::vector<juce::RangedAudioParameter*> params (int i)
    {
        auto& a = proc.apvts;
        switch (i)
        {
            case 0: return { a.getParameter (id::low_cut) };
            case 2: return { a.getParameter (id::high_cut) };
            case 1: return { a.getParameter (id::peak_freq),  a.getParameter (id::peak_gain) };
            case 3: return { a.getParameter (id::peak2_freq), a.getParameter (id::peak2_gain) };
            case 4: return { a.getParameter (id::ls_freq),    a.getParameter (id::ls_gain) };
            default: return { a.getParameter (id::hs_freq),   a.getParameter (id::hs_gain) };
        }
    }
    void set (const char* pid, float v)
    {
        auto* p = proc.apvts.getParameter (pid);
        p->setValueNotifyingHost (p->convertTo0to1 (p->getNormalisableRange().snapToLegalValue (v)));
    }

    DelexProcessor& proc;
    Values vals {};
    float lastSig = -1.0f;
    juce::Rectangle<float> plot;
    int dragging = -1, hover = -1;
};

// ---------------------------------------------------------------- ordem do loop: lista arrastável
//  Arrasta um módulo para cima/baixo; a ordem é guardada no parâmetro loop_order
//  (índice da permutação, automatizável).
class LoopOrderList : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LoopOrderList (APVTS& s)
    {
        auto* prm = s.getParameter (id::loop_order);
        att = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v) { index = juce::jlimit (0, (int) loopPermutations().size() - 1, (int) std::lround (v)); repaint(); }, s.undoManager);
        att->sendInitialUpdate();
        setTooltip (tr ("Arrasta os módulos para mudar a ordem dentro do loop de feedback", "Drag the modules to change their order inside the feedback loop"));
    }
    static juce::String moduleName (int m)
    {
        switch (m) { case 0: return "Digital Grit"; case 1: return "Feedback EQ"; case 2: return tr ("Dispersão", "Dispersion"); case 3: return tr ("Difusão", "Diffusion"); default: return tr ("Fita", "Tape"); }
    }
    static juce::Colour moduleColour (int m)
    {
        switch (m) { case 0: return col::blue; case 1: return col::coral; case 2: return col::green; case 3: return col::violet; default: return col::gold; }
    }
    void paint (juce::Graphics& g) override
    {
        auto order = currentOrder();
        for (int i = 0; i < 5; ++i)
        {
            auto b = rowRect (i).toFloat();
            if (dragFrom >= 0 && i == dragFrom) b = b.withY (b.getY() + dragDy);
            const int m = order[(size_t) i];
            const bool me = m == 1 || i == dragFrom;
            g.setColour (me ? col::coralDeep : col::panel2); g.fillRoundedRectangle (b, 6.0f);
            g.setColour (i == dragFrom ? col::cyan : (me ? col::coral : col::line2)); g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (moduleColour (m)); g.fillRoundedRectangle (juce::Rectangle<float> (8, 8).withCentre ({ b.getX() + 14, b.getCentreY() }), 2.0f);
            label (g, juce::String (i + 1) + ".  " + moduleName (m), b.withTrimmedLeft (26.0f), 13.5f, W::Semi, col::text, juce::Justification::centredLeft, 0.0f);
            g.setColour (col::dim); g.setFont (font (12.0f));
            g.drawText (juce::String::fromUTF8 ("\xe2\x8b\xae\xe2\x8b\xae"), b.withTrimmedRight (10.0f), juce::Justification::centredRight);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override { dragFrom = rowAt (e.y); dragDy = 0; startY = e.y; }
    void mouseDrag (const juce::MouseEvent& e) override { if (dragFrom >= 0) { dragDy = (float) (e.y - startY); repaint(); } }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragFrom < 0) return;
        const int to = juce::jlimit (0, 4, rowAt (juce::jlimit (0, getHeight() - 1, e.y)));
        if (to != dragFrom) moveModule (dragFrom, to);
        dragFrom = -1; dragDy = 0; repaint();
    }
    // move o módulo na posição 'from' para a posição 'to' (usado também pelos testes)
    void moveModule (int from, int to)
    {
        auto order = currentOrder();
        const auto m = order[(size_t) from];
        std::vector<uint8_t> v (order.begin(), order.end());
        v.erase (v.begin() + from); v.insert (v.begin() + to, m);
        const auto& perms = loopPermutations();
        for (size_t k = 0; k < perms.size(); ++k)
            if (std::equal (v.begin(), v.end(), perms[k].begin())) { att->setValueAsCompleteGesture ((float) k); break; }
    }
    std::array<uint8_t, 5> currentOrder() const { return loopPermutations()[(size_t) index]; }

private:
    juce::Rectangle<int> rowRect (int i) const { return { 0, i * 33, getWidth(), 28 }; }
    int rowAt (int y) const { return juce::jlimit (0, 4, y / 33); }
    std::unique_ptr<juce::ParameterAttachment> att;
    int index = 0, dragFrom = -1, startY = 0;
    float dragDy = 0;
};

// ---------------------------------------------------------------- pad de performance (momentâneo)
class PadButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    PadButton (DelexProcessor& p, int padIndex, const juce::String& nm, const juce::String& note, juce::Colour c, const juce::String& tip)
        : proc (p), pad (padIndex), name (nm), noteName (note), colour (c) { setTooltip (tip); }
    void mouseDown (const juce::MouseEvent&) override { proc.setPad (pad, true); repaint(); }
    void mouseUp (const juce::MouseEvent&) override   { proc.setPad (pad, false); repaint(); }
    void tick() { const bool a = proc.padActive (pad); if (a != lit) { lit = a; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (lit ? colour : col::deep); g.fillRoundedRectangle (r, 10.0f);
        g.setColour (lit ? colour.brighter (0.3f) : colour.withAlpha (0.55f)); g.drawRoundedRectangle (r, 10.0f, 1.6f);
        label (g, name, r.withTrimmedBottom (r.getHeight() * 0.3f), 17.0f, W::Bold, lit ? col::bg : colour, juce::Justification::centred, 0.14f);
        g.setColour (lit ? col::bg.withAlpha (0.7f) : col::muted); g.setFont (mono (10.5f));
        g.drawText (noteName, r.removeFromBottom (r.getHeight() * 0.38f), juce::Justification::centred);
    }
private:
    DelexProcessor& proc;
    int pad;
    juce::String name, noteName;
    juce::Colour colour;
    bool lit = false;
};

// ---------------------------------------------------------------- medidor de uma fonte de modulação (−1…+1)
class SourceMeter : public juce::Component
{
public:
    SourceMeter (DelexProcessor& p, int slotIndex) : proc (p), slot (slotIndex) { setInterceptsMouseClicks (false, false); }
    void tick() { const float v = proc.modSourceView[slot].load(); if (std::abs (v - shown) > 0.004f) { shown = v; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (col::deep); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (col::line2); g.drawRoundedRectangle (r, 3.0f, 1.0f);
        const float cx = r.getCentreX(), x = cx + juce::jlimit (-1.0f, 1.0f, shown) * (r.getWidth() * 0.5f - 2.0f);
        g.setColour (col::violet); g.fillRect (juce::Rectangle<float>::leftTopRightBottom (std::min (cx, x), r.getY() + 3, std::max (cx, x), r.getBottom() - 3));
        g.setColour (col::axis); g.drawVerticalLine ((int) cx, r.getY(), r.getBottom());
    }
private:
    DelexProcessor& proc;
    int slot;
    float shown = 0;
};

// ---------------------------------------------------------------- fader horizontal com valor (bipolar)
class AmountSlider : public juce::Component
{
public:
    AmountSlider (APVTS& s, const char* pid, juce::Colour accent, const juce::String& tip) : state (s), paramId (pid)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.setColour (juce::Slider::trackColourId, accent);
        slider.setTooltip (tip + fineTip());
        slider.onRightClick = [this] { showParamMenu (state, paramId, this); };
        att = std::make_unique<APVTS::SliderAttachment> (s, pid, slider);
        if (auto* p = s.getParameter (pid)) slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        value.setFont (mono (12.0f)); value.setColour (juce::Label::textColourId, accent);
        value.setJustificationType (juce::Justification::centredRight);
        slider.onValueChange = [this] { value.setText (slider.getTextFromValue (slider.getValue()) + " %", juce::dontSendNotification); };
        slider.onValueChange();
        addAndMakeVisible (slider); addAndMakeVisible (value);
    }
    void resized() override { auto r = getLocalBounds(); value.setBounds (r.removeFromRight (56)); slider.setBounds (r); }
    FineSlider slider;
private:
    APVTS& state;
    juce::String paramId;
    juce::Label value;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// ---------------------------------------------------------------- barra de redução do ducker
class GrBar : public juce::Component
{
public:
    explicit GrBar (DelexProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
    void tick()
    {
        const float v = proc.duckGr.load();
        const bool on = proc.raw.load().duck > 0.5f;
        if (std::abs (v - gr) > 0.05f || on != wasOn) { gr = v; wasOn = on; repaint(); }
    }
    bool wasOn = false;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto bar = r.removeFromLeft (44.0f).withSizeKeepingCentre (44.0f, 6.0f);
        const auto v = proc.raw.load();
        const bool on = v.duck > 0.5f;
        const auto c = on ? col::coral : col::dim;
        g.setColour (c.withAlpha (0.18f)); g.fillRoundedRectangle (bar, 3.0f);
        const float amt = juce::jmax (1.0f, v.duck_amount);
        g.setColour (c);
        g.fillRoundedRectangle (bar.withLeft (bar.getRight() - bar.getWidth() * juce::jlimit (0.0f, 1.0f, gr / amt)), 3.0f);
        g.setFont (mono (11.0f));
        const bool zero = gr < 0.05f;
        g.drawText ((zero ? juce::String() : juce::String::fromUTF8 ("\xe2\x88\x92")) + juce::String (zero ? 0.0f : gr, 1) + " dB", r.withTrimmedLeft (6.0f), juce::Justification::centredLeft);
    }
private:
    DelexProcessor& proc;
    float gr = 0;
};

} // namespace delex::ui
