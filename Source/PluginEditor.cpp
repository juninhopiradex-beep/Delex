#include "PluginEditor.h"

namespace delex::ui
{
using juce::String;
static String u8 (const char* s) { return String::fromUTF8 (s); }

// =============================================================================
//  Página com secções desenhadas (caixas com título)
// =============================================================================
class Page : public juce::Component
{
public:
    struct Section { juce::Rectangle<int> r; String title; };
    std::vector<Section> sections;
    std::function<void (juce::Graphics&)> extraPaint;

    void paint (juce::Graphics& g) override
    {
        for (auto& s : sections)
        {
            auto r = s.r.toFloat();
            g.setColour (col::panel); g.fillRoundedRectangle (r, 8.0f);
            g.setColour (col::line); g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
            label (g, s.title, { r.getX() + 12.0f, r.getY() + 8.0f, r.getWidth() - 24.0f, 14.0f }, 11.0f, W::Semi, col::muted, juce::Justification::centredLeft, 0.18f);
        }
        if (extraPaint) extraPaint (g);
    }
    void resized() override { if (layout) layout(); }
    std::function<void()> layout;

    template <typename C> C& add (std::unique_ptr<C> c) { auto& ref = *c; addAndMakeVisible (ref); owned.push_back (std::move (c)); return ref; }

private:
    std::vector<std::unique_ptr<juce::Component>> owned;
};

// botão que mostra a categoria + nome do preset
class PresetNameButton : public juce::Button
{
public:
    PresetNameButton() : juce::Button ("preset") { setTooltip ("Abrir o browser de presets"); }
    String category, name;
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (over ? col::deep.brighter (0.06f) : juce::Colour (0xff0f1013)); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::line2); g.drawRoundedRectangle (r, 6.0f, 1.0f);
        auto in = r.reduced (10.0f, 0.0f);
        if (category.isNotEmpty())
        {
            const float w = juce::GlyphArrangement::getStringWidth (font (10.5f, W::Semi, 0.08f), category.toUpperCase()) + 12.0f;
            auto chip = in.removeFromLeft (w).withSizeKeepingCentre (w, 17.0f);
            g.setColour (col::cyan); g.fillRoundedRectangle (chip, 3.0f);
            label (g, category.toUpperCase(), chip, 10.5f, W::Semi, col::bg, juce::Justification::centred, 0.08f);
            in.removeFromLeft (8.0f);
        }
        auto arrow = in.removeFromRight (12.0f);
        juce::Path p; const auto c = arrow.getCentre();
        p.startNewSubPath (c.x - 4, c.y - 2); p.lineTo (c.x, c.y + 2); p.lineTo (c.x + 4, c.y - 2);
        g.setColour (col::muted); g.strokePath (p, juce::PathStrokeType (1.5f));
        label (g, name, in, 15.0f, W::Medium, col::text, juce::Justification::centredLeft, 0.0f);
    }
};

// =============================================================================
//  Interface completa (1000 × 640, escalada pelo editor)
// =============================================================================
class DelexUI : public juce::Component, private juce::Timer
{
public:
    std::function<void (float)> onScale;

    explicit DelexUI (DelexProcessor& p) : proc (p), s (p.apvts)
    {
        buildHeader();
        buildSimple();
        buildBottom();
        buildAdvanced();
        setSize (DelexEditor::kW, DelexEditor::kH);
        setView (proc.advancedView);
        selectTab (proc.advancedTab);
        startTimerHz (30);
    }

    ~DelexUI() override { stopTimer(); }

    // ------------------------------------------------------------------ paint
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (col::bg);
        // cabeçalho
        g.setColour (col::panel); g.fillRect (0, 0, getWidth(), 52);
        g.setColour (col::line); g.drawHorizontalLine (52, 0, (float) getWidth());
        icons::logo (g, { 18.0f, 18.0f, 18.0f, 16.0f });
        label (g, "DELEX", { 44.0f, 13.0f, 90.0f, 26.0f }, 23.0f, W::Bold, col::text, Justification::centredLeft, 0.22f);
        g.setColour (col::muted); g.setFont (font (12.5f, W::Medium, 0.05f));
        g.drawText ("by Piradex", Rectangle<float> (128.0f, 19.0f, 80.0f, 16.0f), Justification::centredLeft);
        // rodapé
        g.setColour (col::panel); g.fillRect (0, getHeight() - 52, getWidth(), 52);
        g.setColour (col::line); g.drawHorizontalLine (getHeight() - 52, 0, (float) getWidth());
        g.setColour (col::muted); g.setFont (font (12.5f, W::Medium, 0.08f));
        g.drawText ("OUTPUT", Rectangle<int> (outSlider.getX() - 62, getHeight() - 52, 56, 52), Justification::centredRight);
        if (advView.isVisible())
            g.drawText ("MIX", Rectangle<int> (mixSlider.getX() - 40, getHeight() - 52, 34, 52), Justification::centredRight);
        g.setColour (col::dim); g.setFont (mono (11.0f));
        g.drawText ("v" JucePlugin_VersionString, Rectangle<int> (getWidth() - 64, getHeight() - 52, 50, 52), Justification::centredRight);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (simpleView.isVisible() && browser == nullptr)
        {
            g.setColour (col::line.withAlpha (0.7f));
            g.drawHorizontalLine (316, 0, (float) getWidth());
            g.drawHorizontalLine (362, 0, (float) getWidth());
        }
    }

    void resized() override
    {
        // cabeçalho
        prevBtn.setBounds (350, 11, 30, 30);
        presetBtn.setBounds (386, 11, 270, 30);
        nextBtn.setBounds (662, 11, 30, 30);
        favBtn.setBounds (696, 11, 30, 30);
        abA.setBounds (734, 12, 30, 28); abB.setBounds (764, 12, 30, 28);
        undoBtn.setBounds (802, 12, 28, 28); redoBtn.setBounds (834, 12, 28, 28);
        simpleBtn.setBounds (870, 12, 54, 28); advBtn.setBounds (924, 12, 64, 28);

        simpleView.setBounds (0, 53, getWidth(), getHeight() - 53 - 52);
        advView.setBounds (simpleView.getBounds());
        layoutSimple();
        layoutAdvanced();

        // rodapé
        const int fy = getHeight() - 42;
        freezeBtn.setBounds (18, fy, 92, 32);
        duckBtn.setBounds (118, fy, 70, 32);
        grBar.setBounds (194, fy, 110, 32);
        bypassBtn.setBounds (310, fy, 84, 32);
        outValue.setBounds (getWidth() - 140, fy, 70, 32);
        outSlider.setBounds (outValue.getX() - 132, fy, 126, 32);
        mixValue.setBounds (outSlider.getX() - 128, fy, 52, 32);
        mixSlider.setBounds (mixValue.getX() - 104, fy, 100, 32);

        if (browser != nullptr) browser->setBounds (getLocalBounds());
    }

private:
    DelexProcessor& proc;
    APVTS& s;

    // ------------------------------------------------------------------ cabeçalho
    IconButton prevBtn { "anterior", [] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) { icons::chevron (g, r, c, false); } };
    IconButton nextBtn { "seguinte", [] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) { icons::chevron (g, r, c, true); } };
    IconButton favBtn  { "favorito", [this] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) { icons::star (g, r, c, favBtn.getToggleState()); } };
    IconButton undoBtn { "desfazer", [] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) { icons::undoArrow (g, r, c, false); } };
    IconButton redoBtn { "refazer",  [] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) { icons::undoArrow (g, r, c, true); } };
    PresetNameButton presetBtn;
    juce::TextButton abA { "A" }, abB { "B" }, simpleBtn { "SIMPLE" }, advBtn { "ADVANCED" };

    void buildHeader()
    {
        for (auto* b : { &prevBtn, &nextBtn, &undoBtn, &redoBtn }) addAndMakeVisible (b);
        prevBtn.setTooltip ("Preset anterior"); nextBtn.setTooltip (u8 ("Próximo preset"));
        undoBtn.setTooltip ("Desfazer"); redoBtn.setTooltip ("Refazer");
        prevBtn.onClick = [this] { stepPreset (-1); };
        nextBtn.onClick = [this] { stepPreset (+1); };
        undoBtn.onClick = [this] { proc.undo.undo(); };
        redoBtn.onClick = [this] { proc.undo.redo(); };

        favBtn.outline = false;
        favBtn.setTooltip ("Marcar como favorito");
        favBtn.onClick = [this] { proc.toggleFavourite (proc.currentPresetName()); refreshHeader(); };
        addAndMakeVisible (favBtn);

        presetBtn.onClick = [this] { openBrowser(); };
        addAndMakeVisible (presetBtn);

        for (auto* b : { &abA, &abB })
        {
            b->setColour (juce::TextButton::buttonOnColourId, col::text);
            b->getProperties().set ("fontH", 13.0f);
            addAndMakeVisible (b);
        }
        abA.setTooltip (u8 ("Estado A — compara duas versões das definições"));
        abB.setTooltip (u8 ("Estado B — clica para alternar; Alt+clique copia o estado actual para o outro"));
        abA.onClick = [this] { if (juce::ModifierKeys::currentModifiers.isAltDown()) proc.copyToOtherSlot(); else proc.selectSlot (0); refreshHeader(); };
        abB.onClick = [this] { if (juce::ModifierKeys::currentModifiers.isAltDown()) proc.copyToOtherSlot(); else proc.selectSlot (1); refreshHeader(); };

        for (auto* b : { &simpleBtn, &advBtn })
        {
            b->setColour (juce::TextButton::buttonOnColourId, col::coral);
            b->getProperties().set ("solid", true);
            b->getProperties().set ("fontH", 11.5f);
            addAndMakeVisible (b);
        }
        simpleBtn.setTooltip (u8 ("Vista simples: os 6 controlos essenciais"));
        advBtn.setTooltip (u8 ("Vista avançada: tempo, espaço, EQ do loop, movimento, textura e utilidades"));
        simpleBtn.onClick = [this] { setView (false); };
        advBtn.onClick = [this] { setView (true); };
        refreshHeader();
    }

    void refreshHeader()
    {
        const auto name = proc.currentPresetName();
        const auto& fp = factoryPresets();
        const int cur = proc.getCurrentProgram();
        presetBtn.name = name;
        presetBtn.category = juce::isPositiveAndBelow (cur, (int) fp.size()) && name == String::fromUTF8 (fp[(size_t) cur].name)
                               ? String::fromUTF8 (fp[(size_t) cur].category) : (name == "Init" ? String() : String ("USER"));
        presetBtn.repaint();
        favBtn.setToggleState (proc.isFavourite (name), juce::dontSendNotification);
        favBtn.repaint();
        abA.setToggleState (proc.currentSlot() == 0, juce::dontSendNotification);
        abB.setToggleState (proc.currentSlot() == 1, juce::dontSendNotification);
    }

    void stepPreset (int d)
    {
        const int n = proc.getNumPrograms();
        int cur = proc.getCurrentProgram();
        cur = cur < 0 ? 0 : (cur + d + n) % n;
        proc.loadFactoryPreset (cur);
        styleSel = -1;
        refreshHeader();
    }

    // ------------------------------------------------------------------ vista SIMPLE
    juce::Component simpleView;
    EchoField echo { proc, false };
    PhasePanel phase { proc, false };
    Meter inMeter { &proc.inPeak[0], &proc.inPeak[1], "IN", false };
    Meter outMeter { &proc.outPeak[0], &proc.outPeak[1], "OUT", true };
    struct StyleLabel : public juce::Component
    {
        StyleLabel() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override
        { label (g, "ESTILO", getLocalBounds().toFloat(), 11.0f, W::Semi, col::muted, juce::Justification::centredLeft, 0.18f); }
    } styleLabel;
    juce::OwnedArray<juce::TextButton> styleBtns;
    int styleSel = -1;
    std::unique_ptr<ToggleChip> syncBtn;
    juce::TextButton tapBtn { "TAP" }, bpmBtn;
    std::unique_ptr<Knob> timeKnob, divKnob, fbKnob, spaceKnob, toneKnob, charKnob, mixKnob;

    void buildSimple()
    {
        addAndMakeVisible (simpleView);
        for (auto* c : std::initializer_list<juce::Component*> { &echo, &phase, &inMeter, &outMeter, &styleLabel }) simpleView.addAndMakeVisible (c);

        const auto& st = styles();
        for (int i = 0; i < (int) st.size() - 1; ++i)   // FREEZE fica no botão próprio
        {
            auto* b = styleBtns.add (new juce::TextButton (st[(size_t) i].name));
            b->getProperties().set ("radius", 13.0f);
            b->getProperties().set ("flat", true);
            b->getProperties().set ("fontH", 12.0f);
            b->setColour (juce::TextButton::buttonOnColourId, col::coral);
            b->setColour (juce::TextButton::textColourOffId, col::soft);
            b->setTooltip (String::fromUTF8 (st[(size_t) i].tip) + u8 (" — ponto de partida (não altera tempo, mix nem ducking)"));
            b->onClick = [this, i] { proc.applyStyle (i); styleSel = i; refreshStyles(); };
            simpleView.addAndMakeVisible (b);
        }
        syncBtn = std::make_unique<ToggleChip> (s, id::sync, "SYNC", col::cyan, u8 ("Sincroniza o tempo com o BPM do projecto (divisões musicais)"));
        simpleView.addAndMakeVisible (*syncBtn);
        bpmBtn.getProperties().set ("mono", true);
        bpmBtn.getProperties().set ("fontH", 13.0f);
        bpmBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0f1013));
        bpmBtn.setTooltip (u8 ("BPM do projecto (ou do TAP quando o host não envia tempo)"));
        bpmBtn.setEnabled (false);
        simpleView.addAndMakeVisible (bpmBtn);
        tapBtn.setTooltip (u8 ("Toca 3–4 vezes ao ritmo: define o tempo em ms (desliga o SYNC)"));
        tapBtn.onClick = [this] { proc.tap(); };
        simpleView.addAndMakeVisible (tapBtn);

        timeKnob = std::make_unique<Knob> (s, id::time, "TIME", "Ritmo do eco", col::coral, Knob::Size::Big, false, u8 ("Tempo do eco em milissegundos (1 ms – 8 s)"));
        divKnob  = std::make_unique<Knob> (s, id::div, "TIME", "Ritmo do eco", col::coral, Knob::Size::Big, false, u8 ("Divisão musical do eco (D = pontuado, T = tercina)"));
        fbKnob   = std::make_unique<Knob> (s, id::feedback, "FEEDBACK", u8 ("Repetições"), col::coral, Knob::Size::Big, false, u8 ("Quanto do eco volta a entrar — mais feedback, mais repetições. 100 % é estável."));
        spaceKnob = std::make_unique<Knob> (s, id::space, "SPACE", u8 ("Largura · rotação · difusão"), col::cyan, Knob::Size::Big, false, u8 ("Macro: alarga a saída, acrescenta difusão e roda os ecos no estéreo"));
        toneKnob = std::make_unique<Knob> (s, id::tone, "TONE", u8 ("Escuro ↔ brilhante"), col::cyan, Knob::Size::Big, true, u8 ("Macro: para a esquerda escurece os ecos, para a direita abrilhanta-os"));
        charKnob = std::make_unique<Knob> (s, id::character, "CHARACTER", u8 ("Digital ↔ fita"), col::coral, Knob::Size::Big, true, u8 ("Macro: direita = fita (saturação, warmth, wow) · esquerda = digital (bits e sample rate)"));
        mixKnob  = std::make_unique<Knob> (s, id::mix, "MIX", u8 ("Seco ↔ molhado"), col::text, Knob::Size::Big, false, u8 ("Equilíbrio seco/molhado. Em envio auxiliar usa 100 %."));
        for (auto* k : { timeKnob.get(), divKnob.get(), fbKnob.get(), spaceKnob.get(), toneKnob.get(), charKnob.get(), mixKnob.get() })
            simpleView.addAndMakeVisible (k);
    }

    void refreshStyles()
    {
        for (int i = 0; i < styleBtns.size(); ++i)
        {
            styleBtns[i]->setToggleState (i == styleSel, juce::dontSendNotification);
            styleBtns[i]->getProperties().set ("flat", i != styleSel);
        }
    }

    void layoutSimple()
    {
        inMeter.setBounds (18, 14, 14, 238);
        echo.setBounds (42, 14, 708, 238);
        phase.setBounds (760, 14, 196, 238);
        outMeter.setBounds (966, 14, 14, 238);

        styleLabel.setBounds (18, 273, 58, 26);
        int x = 76;
        for (auto* b : styleBtns)
        {
            const int w = (int) juce::GlyphArrangement::getStringWidth (font (12.0f, W::Bold, 0.09f), b->getButtonText()) + 22;
            b->setBounds (x, 273, w, 26);
            x += w + 6;
        }
        tapBtn.setBounds (simpleView.getWidth() - 18 - 52, 273, 52, 26);
        bpmBtn.setBounds (tapBtn.getX() - 92, 273, 86, 26);
        syncBtn->setBounds (bpmBtn.getX() - 62, 273, 56, 26);

        auto row = juce::Rectangle<int> (24, 318, simpleView.getWidth() - 48, simpleView.getHeight() - 318 - 6);
        const int w = row.getWidth() / 6;
        timeKnob->setBounds (row.removeFromLeft (w));
        divKnob->setBounds (timeKnob->getBounds());
        for (auto* k : { fbKnob.get(), spaceKnob.get(), toneKnob.get(), charKnob.get(), mixKnob.get() })
            k->setBounds (row.removeFromLeft (w));
    }

    // ------------------------------------------------------------------ rodapé
    std::unique_ptr<ToggleChip> freezeBtn_, duckBtn_, bypassBtn_;
    ToggleChip& freezeBtn = *(freezeBtn_ = std::make_unique<ToggleChip> (s, id::freeze, "FREEZE", col::cyan, u8 ("Congela a cauda actual: deixa de entrar som novo e os ecos ficam a repetir sem perdas")));
    ToggleChip& duckBtn = *(duckBtn_ = std::make_unique<ToggleChip> (s, id::duck, "DUCK", col::coral, u8 ("Baixa os ecos enquanto a fonte toca e solta-os nas pausas (ajustes em UTILITIES)")));
    ToggleChip& bypassBtn = *(bypassBtn_ = std::make_unique<ToggleChip> (s, id::bypass, "BYPASS", col::soft, u8 ("Desliga o efeito sem cliques")));
    GrBar grBar { proc };
    FineSlider outSlider, mixSlider;
    juce::Label outValue, mixValue;
    std::unique_ptr<APVTS::SliderAttachment> outAtt, mixAtt;

    void buildBottom()
    {
        for (auto* b : std::initializer_list<juce::Component*> { &freezeBtn, &duckBtn, &grBar, &bypassBtn }) addAndMakeVisible (b);
        for (auto* sl : { &outSlider, &mixSlider })
        {
            sl->setSliderStyle (juce::Slider::LinearHorizontal);
            sl->setColour (juce::Slider::rotarySliderFillColourId, col::text);
            addAndMakeVisible (sl);
        }
        outAtt = std::make_unique<APVTS::SliderAttachment> (s, id::out_trim, outSlider);
        mixAtt = std::make_unique<APVTS::SliderAttachment> (s, id::mix, mixSlider);
        outSlider.setDoubleClickReturnValue (true, 0.0);
        mixSlider.setDoubleClickReturnValue (true, 25.0);
        outSlider.setTooltip (u8 ("Ganho de saída") + fineTip);
        mixSlider.setTooltip (u8 ("Equilíbrio seco/molhado") + fineTip);
        for (auto* l : { &outValue, &mixValue })
        {
            l->setFont (mono (13.0f));
            l->setColour (juce::Label::textColourId, col::text);
            l->setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (l);
        }
        outSlider.onValueChange = [this] { outValue.setText (outSlider.getTextFromValue (outSlider.getValue()), juce::dontSendNotification); };
        mixSlider.onValueChange = [this] { mixValue.setText (mixSlider.getTextFromValue (mixSlider.getValue()), juce::dontSendNotification); };
        outSlider.onValueChange(); mixSlider.onValueChange();
    }

    // ------------------------------------------------------------------ vista ADVANCED
    juce::Component advView;
    EchoField echoC { proc, true };
    PhasePanel phaseC { proc, true };
    juce::OwnedArray<juce::TextButton> tabs;
    juce::OwnedArray<Page> pages;
    EqDisplay* eq = nullptr;
    Knob* advRate = nullptr; ChoiceBox* advModDiv = nullptr;
    Knob* advTime = nullptr; ChoiceBox* advDiv = nullptr;
    int currentTab = 2;

    template <typename... Args> std::unique_ptr<Knob> knob (Args&&... a) { return std::make_unique<Knob> (std::forward<Args> (a)...); }

    void buildAdvanced()
    {
        addAndMakeVisible (advView);
        advView.addAndMakeVisible (echoC);
        advView.addAndMakeVisible (phaseC);
        const juce::StringArray names { "TIMING", "SPACE", "FEEDBACK EQ", "MOTION", "TEXTURE", "UTILITIES" };
        for (int i = 0; i < names.size(); ++i)
        {
            auto* b = tabs.add (new juce::TextButton (names[i]));
            b->getProperties().set ("flat", true);
            b->getProperties().set ("noEdge", true);
            b->getProperties().set ("fontH", 13.0f);
            b->setColour (juce::TextButton::buttonOnColourId, col::text);
            b->setColour (juce::TextButton::textColourOffId, col::muted);
            b->onClick = [this, i] { selectTab (i); };
            advView.addAndMakeVisible (b);
            auto* p = pages.add (new Page());
            advView.addChildComponent (p);
        }
        buildTiming (*pages[0]);
        buildSpace (*pages[1]);
        buildEq (*pages[2]);
        buildMotion (*pages[3]);
        buildTexture (*pages[4]);
        buildUtilities (*pages[5]);
    }

    using K = Knob::Size;

    // grelha de knobs pequenos numa secção
    static void row (std::initializer_list<juce::Component*> items, juce::Rectangle<int> r, int each = 0)
    {
        const int n = (int) items.size();
        const int w = each > 0 ? each : r.getWidth() / juce::jmax (1, n);
        for (auto* c : items) c->setBounds (r.removeFromLeft (w));
    }

    void buildTiming (Page& p)
    {
        auto& t   = p.add (knob (s, id::time, "TIME", "", col::coral, K::Small, false, u8 ("Tempo do eco em ms (quando o SYNC está desligado)")));
        auto& rr  = p.add (knob (s, id::r_ratio, "R RATIO", "", col::coral, K::Small, false, u8 ("Tempo do canal direito em relação ao esquerdo (1.00 = igual)")));
        auto& fb  = p.add (knob (s, id::feedback, "FEEDBACK", "", col::coral, K::Small, false, u8 ("Quantidade de realimentação")));
        auto& bal = p.add (knob (s, id::fb_balance, "FB L/R", "", col::coral, K::Small, true, u8 ("Equilíbrio do feedback entre esquerda e direita")));
        auto& gl  = p.add (knob (s, id::glide, "GLIDE", "", col::text, K::Small, false, u8 ("Duração do deslize quando o tempo muda (modo fita)")));
        auto& sync = p.add (std::make_unique<ToggleChip> (s, id::sync, "SYNC", col::cyan, u8 ("Sincroniza com o BPM do projecto")));
        auto& div = p.add (std::make_unique<ChoiceBox> (s, id::div, u8 ("Divisão musical (D = pontuado, T = tercina)")));
        auto& tap = p.add (std::make_unique<juce::TextButton> ("TAP"));
        tap.onClick = [this] { proc.tap(); };
        tap.setTooltip (u8 ("Toca ao ritmo para definir o tempo"));
        auto& route = p.add (std::make_unique<Segmented> (s, id::routing, juce::StringArray { "DUAL", "PING-PONG", "MONO" }, col::coral,
                             u8 ("Dual: L e R independentes · Ping-Pong: ecos alternam entre lados · Mono: soma em mono")));
        auto& trans = p.add (std::make_unique<Segmented> (s, id::transition, juce::StringArray { "FITA (GLIDE)", "CROSSFADE" }, col::coral,
                             u8 ("Fita: o tempo desliza com mudança de afinação · Crossfade: troca suave sem mudar a afinação")));
        advTime = &t; advDiv = &div;

        p.extraPaint = [this, &p] (juce::Graphics& g)
        {
            const auto& sec = p.sections[1].r.toFloat();
            g.setColour (col::muted); g.setFont (font (13.0f));
            const int r = (int) std::lround (proc.raw.load().routing);
            const String txt = r == 0 ? u8 ("Cada lado tem o seu tempo; a matriz SPACE mistura-os em cada repetição.")
                             : r == 1 ? u8 ("A entrada soma-se em mono e os ecos alternam esquerda → direita → esquerda.")
                                      : u8 ("Um único eco em mono, centrado. Ideal para 808 e baixos.");
            g.drawFittedText (txt, sec.reduced (14.0f).withTrimmedTop (64.0f).toNearestInt(), juce::Justification::topLeft, 3);
            auto tips = p.sections[0].r.toFloat().reduced (18.0f).withTrimmedTop (250.0f);
            g.setColour (col::dim); g.setFont (font (13.0f));
            g.drawFittedText (u8 ("Com SYNC ligado o tempo segue o BPM do projecto (D = pontuado, T = tercina).\n"
                                  "R RATIO dá tempos diferentes a cada lado: 1.50 = relação 3:2, 0.75 = 3:4.\n"
                                  "TAP: toca 3–4 vezes ao ritmo. Shift + arrastar = ajuste fino · duplo clique = repor."),
                              tips.toNearestInt(), juce::Justification::topLeft, 6, 1.0f);
        };
        p.layout = [&p, &t, &div, &rr, &fb, &bal, &gl, &sync, &tap, &route, &trans]
        {
            p.sections = { { { 0, 0, 560, 392 }, "TEMPO E FEEDBACK" }, { { 572, 0, 392, 186 }, "ENCAMINHAMENTO" }, { { 572, 198, 392, 194 }, u8 ("TRANSIÇÃO DE TEMPO") } };
            sync.setBounds (16, 36, 60, 28); div.setBounds (84, 36, 110, 28); tap.setBounds (202, 36, 56, 28);
            row ({ &t, &rr, &fb, &bal }, { 10, 84, 540, 130 });
            div.toFront (false);
            route.setBounds (586, 36, 364, 30);
            trans.setBounds (586, 234, 364, 30);
            gl.setBounds (586 + 120, 276, 124, 110);
        };
    }

    void buildSpace (Page& p)
    {
        auto& rot = p.add (knob (s, id::rotation, "ROTATION", "", col::cyan, K::Small, true, u8 ("Ângulo da matriz aplicada a cada repetição: os ecos rodam no campo estéreo")));
        auto& mode = p.add (std::make_unique<Segmented> (s, id::rot_mode, juce::StringArray { u8 ("ROTAÇÃO"), u8 ("REFLEXÃO") }, col::cyan,
                            u8 ("Rotação mantém o sentido; reflexão espelha o campo a cada repetição")));
        auto& ir = p.add (knob (s, id::in_rot, "IN ROT", "", col::cyan, K::Small, true, u8 ("Rotação da entrada (±45°; 45° converte L/R em M/S)")));
        auto& orr = p.add (knob (s, id::out_rot, "OUT ROT", "", col::cyan, K::Small, true, u8 ("Rotação da saída (±45°)")));
        auto& iw = p.add (knob (s, id::in_width, "IN WIDTH", "", col::cyan, K::Small, false, u8 ("Largura da entrada antes do delay (0 = mono, 200 % = extra largo)")));
        auto& ow = p.add (knob (s, id::out_width, "OUT WIDTH", "", col::cyan, K::Small, false, u8 ("Largura dos ecos à saída (nível compensado acima de 100 %)")));
        auto& sp = p.add (knob (s, id::space, "SPACE", "", col::cyan, K::Small, false, u8 ("Macro SPACE: largura + difusão + rotação extra")));
        auto& ms = p.add (std::make_unique<ToggleChip> (s, id::mono_safe, "MONO-SAFE", col::cyan, u8 ("Põe os graves dos ecos (< 150 Hz) em mono — mais seguro em clubes e no telemóvel")));
        p.extraPaint = [this, &p] (juce::Graphics& g)
        {
            // mini-vista da matriz: eixos L/R e a sua imagem após uma repetição
            auto r = p.sections[0].r.toFloat().removeFromRight (230.0f).reduced (24.0f, 40.0f);
            const auto c = r.getCentre(); const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.45f;
            g.setColour (col::axis); g.drawEllipse (juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c), 1.0f);
            const auto v = proc.raw.load();
            const auto M = spatialMatrix (v.rotation, v.rot_mode > 0.5f);
            auto arrow = [&] (float x, float y, juce::Colour colr, float th)
            {
                juce::Line<float> ln (c, { c.x + x * rad, c.y - y * rad });
                g.setColour (colr); g.drawArrow (ln, th, 8.0f, 8.0f);
            };
            arrow (-0.7071f, 0.7071f, col::dim, 1.2f); arrow (0.7071f, 0.7071f, col::dim, 1.2f);
            // L = (1,0) → (M.a, M.c) ; R = (0,1) → (M.b, M.d), desenhados no plano L/R rodado 45°
            auto toXY = [] (float l, float rr) { return juce::Point<float> ((rr - l) * 0.7071f, (l + rr) * 0.7071f); };
            const auto pl = toXY (M.a, M.c), pr = toXY (M.b, M.d);
            arrow (pl.x, pl.y, col::coral, 2.0f); arrow (pr.x, pr.y, col::cyan, 2.0f);
            g.setFont (font (11.0f)); g.setColour (col::coral);
            g.drawText (u8 ("L → eco"), juce::Rectangle<float> (r.getX(), r.getBottom() + 2, r.getWidth() * 0.5f, 14), juce::Justification::centred);
            g.setColour (col::cyan);
            g.drawText (u8 ("R → eco"), juce::Rectangle<float> (r.getCentreX(), r.getBottom() + 2, r.getWidth() * 0.5f, 14), juce::Justification::centred);
        };
        p.layout = [&p, &rot, &mode, &ir, &orr, &iw, &ow, &sp, &ms]
        {
            p.sections = { { { 0, 0, 470, 392 }, "MATRIZ ESPACIAL" }, { { 482, 0, 482, 200 }, u8 ("ENTRADA / SAÍDA") }, { { 482, 212, 482, 180 }, "MACRO E MONO" } };
            mode.setBounds (16, 36, 210, 30);
            rot.setBounds (30, 96, 180, 200);
            row ({ &ir, &orr, &iw, &ow }, { 492, 40, 462, 140 });
            sp.setBounds (500, 246, 120, 130);
            ms.setBounds (650, 300, 120, 30);
        };
    }

    void buildEq (Page& p)
    {
        auto& disp = p.add (std::make_unique<EqDisplay> (proc));
        eq = &disp;
        auto& lc = p.add (knob (s, id::low_cut, "LOW CUT", "", col::cyan, K::Small, false, u8 ("Corta graves dentro do loop: cada repetição fica mais leve")));
        auto& slope = p.add (std::make_unique<Segmented> (s, id::cut_slope, juce::StringArray { "6 dB", "12 dB", "24 dB" }, col::text, u8 ("Inclinação dos cortes (por oitava)"), true));
        auto& pf = p.add (knob (s, id::peak_freq, "PEAK FREQ", "", col::coral, K::Small, false, u8 ("Frequência do pico/vale no loop")));
        auto& pg = p.add (knob (s, id::peak_gain, "GAIN", "", col::coral, K::Small, true, u8 ("Ganho do pico (±18 dB). Ganhos positivos acumulam-se a cada repetição")));
        auto& pq = p.add (knob (s, id::peak_q, "Q", "", col::coral, K::Small, false, u8 ("Largura de banda do pico (mais alto = mais estreito)")));
        auto& cp = p.add (knob (s, id::compensate, "COMPENSATE", "", col::text, K::Small, false, u8 ("Reduz o feedback na proporção do ganho do pico, para o loop não disparar")));
        auto& hc = p.add (knob (s, id::high_cut, "HIGH CUT", "", col::cyan, K::Small, false, u8 ("Corta agudos dentro do loop: ecos mais escuros a cada repetição")));
        auto& order = p.add (std::make_unique<ChoiceBox> (s, id::loop_order, u8 ("Ordem dos módulos dentro do loop de feedback")));
        p.extraPaint = [this, &p] (juce::Graphics& g)
        {
            auto r = p.sections[1].r.toFloat().reduced (12.0f).withTrimmedTop (62.0f);
            const int ord = (int) std::lround (proc.raw.load().loop_order);
            static const char* orders[4][5] = { { "Digital Grit", "Feedback EQ", "Dispersion", "Diffusion", "Tape" },
                                                { "Feedback EQ", "Tape", "Diffusion", "Dispersion", "Digital Grit" },
                                                { "Tape", "Feedback EQ", "Diffusion", "Dispersion", "Digital Grit" },
                                                { "Diffusion", "Dispersion", "Feedback EQ", "Tape", "Digital Grit" } };
            auto colourOf = [] (const String& n)
            {
                if (n == "Digital Grit") return col::blue;
                if (n == "Feedback EQ") return col::coral;
                if (n == "Dispersion") return col::green;
                if (n == "Diffusion") return col::violet;
                return col::gold;
            };
            for (int i = 0; i < 5; ++i)
            {
                const String n = orders[juce::jlimit (0, 3, ord)][i];
                auto b = r.removeFromTop (28.0f); r.removeFromTop (5.0f);
                const bool me = n == "Feedback EQ";
                g.setColour (me ? col::coralDeep : col::panel2); g.fillRoundedRectangle (b, 6.0f);
                g.setColour (me ? col::coral : col::line2); g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
                g.setColour (colourOf (n)); g.fillRoundedRectangle (juce::Rectangle<float> (8, 8).withCentre ({ b.getX() + 14, b.getCentreY() }), 2.0f);
                label (g, String (i + 1) + ".  " + n, b.withTrimmedLeft (26.0f), 13.5f, W::Semi, col::text, juce::Justification::centredLeft, 0.0f);
            }
            r.removeFromTop (6.0f);
            g.setColour (col::muted); g.setFont (font (12.5f));
            const float lim = proc.loopLimit.load();
            g.drawFittedText (u8 ("Limitador suave e bloqueio DC sempre activos no loop.") + (lim < -0.05f ? u8 ("\nA limitar agora: ") + String (lim, 1) + " dB" : String()),
                              r.toNearestInt(), juce::Justification::topLeft, 3);
        };
        p.layout = [&p, &disp, &lc, &slope, &pf, &pg, &pq, &cp, &hc, &order]
        {
            p.sections = { { { 0, 0, 0, 0 }, "" }, { { 712, 0, 252, 392 }, "ORDEM DO LOOP" } };
            disp.setBounds (0, 0, 700, 236);
            row ({ &lc, &slope, &pf, &pg, &pq, &cp, &hc }, { 0, 250, 700, 132 });
            slope.setBounds (slope.getBounds().withSizeKeepingCentre (76, 84));
            order.setBounds (724, 30, 228, 28);
        };
    }

    void buildMotion (Page& p)
    {
        auto& dep = p.add (knob (s, id::mod_depth, "DEPTH", "", col::violet, K::Small, false, u8 ("Profundidade da modulação do tempo, em cents de afinação")));
        auto& rate = p.add (knob (s, id::mod_rate, "RATE", "", col::violet, K::Small, false, u8 ("Velocidade da modulação")));
        auto& mdiv = p.add (std::make_unique<ChoiceBox> (s, id::mod_div, u8 ("Duração de um ciclo da modulação, sincronizada")));
        auto& msync = p.add (std::make_unique<ToggleChip> (s, id::mod_sync, "SYNC", col::violet, u8 ("Sincroniza a modulação com o BPM")));
        auto& off = p.add (knob (s, id::mod_offset, "OFFSET", "", col::violet, K::Small, false, u8 ("Desfasamento da modulação entre L e R (50 % = opostos)")));
        auto& wow = p.add (knob (s, id::wow, "WOW", "", col::gold, K::Small, false, u8 ("Instabilidade de fita: wow lento + flutter")));
        auto& shape = p.add (std::make_unique<Segmented> (s, id::mod_shape, juce::StringArray { "SENO", u8 ("TRIÂNGULO"), u8 ("ALEATÓRIO") }, col::violet, u8 ("Forma de onda da modulação")));
        auto& df = p.add (knob (s, id::disp_freq, "FREQUENCY", "", col::green, K::Small, false, u8 ("Frequência central da dispersão")));
        auto& ds = p.add (knob (s, id::disp_spread, "SPREAD", "", col::green, K::Small, false, u8 ("Atraso de grupo da dispersão: espalha o eco no tempo (efeito 'chirp')")));
        auto& fa = p.add (knob (s, id::diff_amount, "AMOUNT", "", col::violet, K::Small, false, u8 ("Difusão: de eco discreto (0) a cauda densa tipo reverb (100 %)")));
        auto& fs = p.add (knob (s, id::diff_spread, "SPREAD", "", col::violet, K::Small, false, u8 ("Tamanho da difusão em ms")));
        advRate = &rate; advModDiv = &mdiv;
        p.layout = [&p, &dep, &rate, &mdiv, &msync, &off, &wow, &shape, &df, &ds, &fa, &fs]
        {
            p.sections = { { { 0, 0, 964, 190 }, u8 ("MODULAÇÃO") }, { { 0, 202, 474, 190 }, u8 ("DISPERSÃO") }, { { 490, 202, 474, 190 }, u8 ("DIFUSÃO") } };
            shape.setBounds (150, 6, 260, 26);
            msync.setBounds (420, 6, 60, 26);
            row ({ &dep, &rate, &off, &wow }, { 10, 40, 600, 140 });
            mdiv.setBounds (rate.getBounds().withSizeKeepingCentre (120, 28).withY (rate.getY() + 46));
            row ({ &df, &ds }, { 10, 238, 300, 140 });
            row ({ &fa, &fs }, { 500, 238, 300, 140 });
        };
    }

    void buildTexture (Page& p)
    {
        auto& wa = p.add (knob (s, id::warmth, "WARMTH", "", col::gold, K::Small, false, u8 ("Pré-ênfase de agudos antes da saturação (sem drive é transparente)")));
        auto& dr = p.add (knob (s, id::drive, "DRIVE", "", col::gold, K::Small, false, u8 ("Saturação de fita no loop (com anti-aliasing)")));
        auto& bi = p.add (knob (s, id::bits, "BIT DEPTH", "", col::blue, K::Small, false, u8 ("Redução de bits (24 = desligado)")));
        auto& ra = p.add (knob (s, id::rate, "SAMPLE RATE", "", col::blue, K::Small, false, u8 ("Redução da taxa de amostragem (48 kHz = desligado)")));
        auto& ji = p.add (knob (s, id::jitter, "JITTER", "", col::blue, K::Small, false, u8 ("Instabilidade do relógio digital")));
        auto& gm = p.add (knob (s, id::grit_mix, "GRIT MIX", "", col::blue, K::Small, false, u8 ("Mistura paralela do grit")));
        auto& ch = p.add (knob (s, id::character, "CHARACTER", "", col::coral, K::Small, true, u8 ("Macro: fita (direita) ↔ digital (esquerda)")));
        auto& pd = p.add (knob (s, id::pre_drive, "PRE-DRIVE", "", col::text, K::Small, true, u8 ("Ganho antes do delay (empurra a saturação)")));
        p.layout = [&p, &wa, &dr, &bi, &ra, &ji, &gm, &ch, &pd]
        {
            p.sections = { { { 0, 0, 300, 190 }, "TAPE SATURATION" }, { { 312, 0, 652, 190 }, "DIGITAL GRIT" }, { { 0, 202, 964, 190 }, u8 ("CARÁCTER E ENTRADA") } };
            row ({ &wa, &dr }, { 10, 40, 280, 140 });
            row ({ &bi, &ra, &ji, &gm }, { 322, 40, 632, 140 });
            row ({ &ch, &pd }, { 10, 240, 300, 140 });
        };
    }

    void buildUtilities (Page& p)
    {
        auto& on = p.add (std::make_unique<ToggleChip> (s, id::duck, "DUCK", col::coral, u8 ("Liga o ducker")));
        auto& th = p.add (knob (s, id::duck_thresh, "THRESHOLD", "", col::coral, K::Small, false, u8 ("Nível da fonte a partir do qual os ecos baixam")));
        auto& am = p.add (knob (s, id::duck_amount, "AMOUNT", "", col::coral, K::Small, false, u8 ("Quanto os ecos baixam")));
        auto& at = p.add (knob (s, id::duck_attack, "ATTACK", "", col::coral, K::Small, false, u8 ("Rapidez com que os ecos baixam")));
        auto& ho = p.add (knob (s, id::duck_hold, "HOLD", "", col::coral, K::Small, false, u8 ("Tempo de espera antes de soltar")));
        auto& re = p.add (knob (s, id::duck_release, "RELEASE", "", col::coral, K::Small, false, u8 ("Rapidez com que os ecos voltam")));
        auto& law = p.add (std::make_unique<Segmented> (s, id::mix_law, juce::StringArray { "LINEAR", u8 ("POTÊNCIA CONST.") }, col::cyan, u8 ("Curva do MIX: potência constante mantém o volume percebido")));
        auto& wt = p.add (knob (s, id::wet_trim, "WET TRIM", "", col::text, K::Small, true, u8 ("Ganho só dos ecos")));
        auto& ot = p.add (knob (s, id::out_trim, "OUTPUT", "", col::text, K::Small, true, u8 ("Ganho de saída")));
        auto& mx = p.add (knob (s, id::mix, "MIX", "", col::text, K::Small, false, u8 ("Equilíbrio seco/molhado")));
        juce::OwnedArray<juce::TextButton>* sizes = &sizeBtns;
        for (float sc : { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f })
        {
            auto* b = sizeBtns.add (new juce::TextButton (String ((int) std::lround (sc * 100)) + "%"));
            b->getProperties().set ("mono", true);
            b->getProperties().set ("fontH", 12.0f);
            b->setColour (juce::TextButton::buttonOnColourId, col::cyan);
            b->onClick = [this, sc] { if (onScale) onScale (sc); };
            p.addAndMakeVisible (b);
        }
        p.extraPaint = [&p] (juce::Graphics& g)
        {
            auto r = p.sections[3].r.toFloat().reduced (14.0f).withTrimmedTop (22.0f);
            label (g, "DELEX by Piradex  v" JucePlugin_VersionString, r.removeFromTop (20.0f), 15.0f, W::Bold, col::text, juce::Justification::centredLeft, 0.02f);
            g.setColour (col::text2); g.setFont (font (13.0f));
            g.drawFittedText (u8 ("Delay espacial estéreo · VST3 / AU\nwww.beatfreakstudio.com\nSuporte: juninhopiradex@hotmail.com"),
                              r.removeFromTop (60.0f).toNearestInt(), juce::Justification::topLeft, 3);
        };
        p.layout = [&p, &on, &th, &am, &at, &ho, &re, &law, &wt, &ot, &mx, sizes]
        {
            p.sections = { { { 0, 0, 964, 190 }, "DUCKER" }, { { 0, 202, 474, 190 }, "MISTURA E GANHO" }, { { 490, 202, 230, 190 }, "INTERFACE" }, { { 732, 202, 232, 190 }, "SOBRE" } };
            on.setBounds (110, 4, 62, 26);
            row ({ &th, &am, &at, &ho, &re }, { 10, 40, 760, 140 });
            law.setBounds (16, 232, 240, 28);
            row ({ &wt, &ot, &mx }, { 10, 270, 360, 116 });
            int x = 502, y = 236;
            for (auto* b : *sizes) { b->setBounds (x, y, 64, 28); x += 70; if (x > 680) { x = 502; y += 36; } }
        };
    }
    juce::OwnedArray<juce::TextButton> sizeBtns;

    void layoutAdvanced()
    {
        echoC.setBounds (18, 8, 750, 80);
        phaseC.setBounds (778, 8, 204, 80);
        int x = 18;
        for (auto* t : tabs)
        {
            const int w = (int) juce::GlyphArrangement::getStringWidth (font (13.0f, W::Bold, 0.09f), t->getButtonText()) + 32;
            t->setBounds (x, 94, w, 34);
            x += w + 2;
        }
        for (auto* p : pages) p->setBounds (18, 136, 964, 392);
    }

    void selectTab (int i)
    {
        currentTab = juce::jlimit (0, pages.size() - 1, i);
        proc.advancedTab = currentTab;
        for (int k = 0; k < pages.size(); ++k)
        {
            pages[k]->setVisible (k == currentTab);
            tabs[k]->setToggleState (k == currentTab, juce::dontSendNotification);
        }
        advView.repaint();
    }

    void setView (bool advanced)
    {
        proc.advancedView = advanced;
        simpleView.setVisible (! advanced);
        advView.setVisible (advanced);
        mixSlider.setVisible (advanced);
        mixValue.setVisible (advanced);
        simpleBtn.setToggleState (! advanced, juce::dontSendNotification);
        advBtn.setToggleState (advanced, juce::dontSendNotification);
        repaint();
    }

    // ------------------------------------------------------------------ browser
    std::unique_ptr<PresetBrowser> browser;

    void openBrowser()
    {
        browser = std::make_unique<PresetBrowser> (proc);
        juce::Component::SafePointer<DelexUI> safe (this);
        browser->onClose = [safe] { juce::MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->browser.reset(); safe->refreshHeader(); } }); };
        browser->onLoaded = [this] { styleSel = -1; refreshStyles(); refreshHeader(); };
        addAndMakeVisible (*browser);
        browser->setBounds (getLocalBounds());
        browser->grabKeyboardFocus();
    }

    // ------------------------------------------------------------------ timer
    void timerCallback() override
    {
        const auto v = proc.raw.load();
        const bool sync = v.sync > 0.5f;
        timeKnob->setVisible (! sync); divKnob->setVisible (sync);
        if (advTime != nullptr) { advTime->setEnabled (! sync); advDiv->setEnabled (sync); }
        if (advRate != nullptr) { advRate->setVisible (v.mod_sync < 0.5f); advModDiv->setVisible (v.mod_sync > 0.5f); }

        const String bpm = String (proc.hostBpm.load(), 1) + " BPM";
        if (bpmBtn.getButtonText() != bpm) bpmBtn.setButtonText (bpm);

        undoBtn.setEnabled (proc.undo.canUndo());
        redoBtn.setEnabled (proc.undo.canRedo());
        if (presetBtn.name != proc.currentPresetName()) refreshHeader();
        static constexpr float scales[] = { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };
        for (int i = 0; i < sizeBtns.size(); ++i)
            sizeBtns[i]->setToggleState (std::abs (proc.uiScale - scales[i]) < 0.02f, juce::dontSendNotification);

        if (simpleView.isVisible())
        {
            echo.tick(); phase.tick(); inMeter.tick(); outMeter.tick();
        }
        else
        {
            echoC.tick(); phaseC.tick();
            if (eq != nullptr && pages[2]->isVisible()) eq->tick();
            if (++slowTick % 6 == 0) pages[currentTab]->repaint();   // textos dinâmicos (ordem, limitador, matriz)
        }
        grBar.tick();
    }
    int slowTick = 0;
};

} // namespace delex::ui

// =============================================================================
//  Editor: escala a interface 75 % – 200 % mantendo a proporção
// =============================================================================
DelexEditor::DelexEditor (DelexProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    ui = std::make_unique<delex::ui::DelexUI> (p);
    ui->onScale = [this] (float sc) { setScale (sc); };
    addAndMakeVisible (*ui);

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) kW / (double) kH);
        c->setSizeLimits ((int) (kW * 0.75f), (int) (kH * 0.75f), kW * 2, kH * 2);
    }
    const float sc = juce::jlimit (0.75f, 2.0f, proc.uiScale);
    setSize ((int) std::lround (kW * sc), (int) std::lround (kH * sc));
}

DelexEditor::~DelexEditor()
{
    ui.reset();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void DelexEditor::setScale (float s)
{
    setSize ((int) std::lround (kW * s), (int) std::lround (kH * s));
}

void DelexEditor::paint (juce::Graphics& g) { g.fillAll (delex::ui::col::bg); }

void DelexEditor::resized()
{
    const float s = (float) getWidth() / (float) kW;
    proc.uiScale = s;
    ui->setTransform (juce::AffineTransform::scale (s));
    ui->setBounds (0, 0, kW, kH);
}
