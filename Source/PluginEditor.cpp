#include "PluginEditor.h"

namespace delex::ui
{
using juce::String;
[[maybe_unused]] static String u8 (const char* s) { return String::fromUTF8 (s); }

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
    PresetNameButton() : juce::Button ("preset") { setTooltip (tr ("Abrir o browser de presets", "Open the preset browser")); }
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
        headerLang.accent = col::coral;
        headerLang.background = col::deep;
        headerLang.outline = col::line2;
        headerLang.text = col::text;
        headerLang.textDim = col::muted;
        headerLang.hoverFill = col::panel2;
        headerLang.fontHeight = 11.0f;
        addAndMakeVisible (headerLang);
        login.onUnlocked = [this] { login.setVisible (false); repaint(); };
        addChildComponent (login);
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
        if (simpleView.isVisible() && browser == nullptr && ! login.isVisible())
        {
            g.setColour (col::line.withAlpha (0.7f));
            g.drawHorizontalLine (316, 0, (float) getWidth());
            g.drawHorizontalLine (362, 0, (float) getWidth());
        }
    }

    void resized() override
    {
        // cabeçalho
        headerLang.setBounds (262, 15, 70, 22);
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
        monitorBox->setBounds (402, fy + 2, 112, 28);
        outValue.setBounds (getWidth() - 140, fy, 70, 32);
        outSlider.setBounds (outValue.getX() - 100, fy, 94, 32);
        mixValue.setBounds (outSlider.getX() - 118, fy, 52, 32);
        mixSlider.setBounds (mixValue.getX() - 86, fy, 82, 32);

        if (browser != nullptr) browser->setBounds (getLocalBounds());
        login.setBounds (getLocalBounds());
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
        prevBtn.setTooltip (tr ("Preset anterior", "Previous preset")); nextBtn.setTooltip (tr ("Próximo preset", "Next preset"));
        undoBtn.setTooltip (tr ("Desfazer", "Undo")); redoBtn.setTooltip (tr ("Refazer", "Redo"));
        prevBtn.onClick = [this] { stepPreset (-1); };
        nextBtn.onClick = [this] { stepPreset (+1); };
        undoBtn.onClick = [this] { proc.undo.undo(); };
        redoBtn.onClick = [this] { proc.undo.redo(); };

        favBtn.outline = false;
        favBtn.setTooltip (tr ("Marcar como favorito", "Mark as favourite"));
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
        abA.setTooltip (tr ("Estado A — compara duas versões das definições", "State A — compare two versions of the settings"));
        abB.setTooltip (tr ("Estado B — clica para alternar; Alt+clique copia o estado actual para o outro", "State B — click to switch; Alt+click copies the current state to the other"));
        abA.onClick = [this] { if (juce::ModifierKeys::currentModifiers.isAltDown()) proc.copyToOtherSlot(); else proc.selectSlot (0); refreshHeader(); };
        abB.onClick = [this] { if (juce::ModifierKeys::currentModifiers.isAltDown()) proc.copyToOtherSlot(); else proc.selectSlot (1); refreshHeader(); };

        for (auto* b : { &simpleBtn, &advBtn })
        {
            b->setColour (juce::TextButton::buttonOnColourId, col::coral);
            b->getProperties().set ("solid", true);
            b->getProperties().set ("fontH", 11.5f);
            addAndMakeVisible (b);
        }
        simpleBtn.setTooltip (tr ("Vista simples: os 6 controlos essenciais", "Simple view: the 6 essential controls"));
        advBtn.setTooltip (tr ("Vista avançada: tempo, espaço, EQ do loop, movimento, textura e utilidades", "Advanced view: timing, space, loop EQ, motion, texture and utilities"));
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
        { label (g, tr ("ESTILO", "STYLE"), getLocalBounds().toFloat(), 11.0f, W::Semi, col::muted, juce::Justification::centredLeft, 0.18f); }
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
            b->setTooltip (styleTip (st[(size_t) i]) + tr (" — ponto de partida (não altera tempo, mix nem ducking)", " — starting point (does not change time, mix or ducking)"));
            b->onClick = [this, i] { proc.applyStyle (i); styleSel = i; refreshStyles(); };
            simpleView.addAndMakeVisible (b);
        }
        syncBtn = std::make_unique<ToggleChip> (s, id::sync, "SYNC", col::cyan, tr ("Sincroniza o tempo com o BPM do projecto (divisões musicais)", "Syncs the time to the project BPM (musical divisions)"));
        simpleView.addAndMakeVisible (*syncBtn);
        bpmBtn.getProperties().set ("mono", true);
        bpmBtn.getProperties().set ("fontH", 13.0f);
        bpmBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0f1013));
        bpmBtn.setTooltip (tr ("BPM do projecto (ou do TAP quando o host não envia tempo)", "Project BPM (or from TAP when the host sends no tempo)"));
        bpmBtn.setEnabled (false);
        simpleView.addAndMakeVisible (bpmBtn);
        tapBtn.setTooltip (tr ("Toca 3–4 vezes ao ritmo: define o tempo em ms (desliga o SYNC)", "Tap 3–4 times in rhythm: sets the time in ms (turns SYNC off)"));
        tapBtn.onClick = [this] { proc.tap(); };
        simpleView.addAndMakeVisible (tapBtn);

        timeKnob = std::make_unique<Knob> (s, id::time, "TIME", tr ("Ritmo do eco", "Echo rhythm"), col::coral, Knob::Size::Big, false, tr ("Tempo do eco em milissegundos (1 ms – 8 s)", "Echo time in milliseconds (1 ms – 8 s)"));
        divKnob  = std::make_unique<Knob> (s, id::div, "TIME", tr ("Ritmo do eco", "Echo rhythm"), col::coral, Knob::Size::Big, false, tr ("Divisão musical do eco (D = pontuado, T = tercina)", "Musical division of the echo (D = dotted, T = triplet)"));
        fbKnob   = std::make_unique<Knob> (s, id::feedback, "FEEDBACK", tr ("Repetições", "Repeats"), col::coral, Knob::Size::Big, false, tr ("Quanto do eco volta a entrar — mais feedback, mais repetições. 100 % é estável.", "How much of the echo feeds back — more feedback, more repeats. 100 % is stable."));
        spaceKnob = std::make_unique<Knob> (s, id::space, "SPACE", tr ("Largura · rotação · difusão", "Width · rotation · diffusion"), col::cyan, Knob::Size::Big, false, tr ("Macro: alarga a saída, acrescenta difusão e roda os ecos no estéreo", "Macro: widens the output, adds diffusion and rotates the echoes in stereo"));
        toneKnob = std::make_unique<Knob> (s, id::tone, "TONE", tr ("Escuro ↔ brilhante", "Dark ↔ bright"), col::cyan, Knob::Size::Big, true, tr ("Macro: para a esquerda escurece os ecos, para a direita abrilhanta-os", "Macro: left darkens the echoes, right brightens them"));
        charKnob = std::make_unique<Knob> (s, id::character, "CHARACTER", tr ("Digital ↔ fita", "Digital ↔ tape"), col::coral, Knob::Size::Big, true, tr ("Macro: direita = fita (saturação, warmth, wow) · esquerda = digital (bits e sample rate)", "Macro: right = tape (saturation, warmth, wow) · left = digital (bits and sample rate)"));
        mixKnob  = std::make_unique<Knob> (s, id::mix, "MIX", tr ("Seco ↔ molhado", "Dry ↔ wet"), col::text, Knob::Size::Big, false, tr ("Equilíbrio seco/molhado. Em envio auxiliar usa 100 %.", "Dry/wet balance. On an aux send use 100 %."));
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
    ToggleChip& freezeBtn = *(freezeBtn_ = std::make_unique<ToggleChip> (s, id::freeze, "FREEZE", col::cyan, tr ("Congela a cauda actual: deixa de entrar som novo e os ecos ficam a repetir sem perdas", "Freezes the current tail: no new sound comes in and the echoes repeat without loss")));
    ToggleChip& duckBtn = *(duckBtn_ = std::make_unique<ToggleChip> (s, id::duck, "DUCK", col::coral, tr ("Baixa os ecos enquanto a fonte toca e solta-os nas pausas (ajustes em UTILITIES)", "Lowers the echoes while the source plays and releases them in the gaps (settings in UTILITIES)")));
    ToggleChip& bypassBtn = *(bypassBtn_ = std::make_unique<ToggleChip> (s, id::bypass, "BYPASS", col::soft, tr ("Desliga o efeito sem cliques", "Turns the effect off without clicks")));
    GrBar grBar { proc };
    std::unique_ptr<ChoiceBox> monitorBox;
    FineSlider outSlider, mixSlider;
    juce::Label outValue, mixValue;
    std::unique_ptr<APVTS::SliderAttachment> outAtt, mixAtt;

    void buildBottom()
    {
        for (auto* b : std::initializer_list<juce::Component*> { &freezeBtn, &duckBtn, &grBar, &bypassBtn }) addAndMakeVisible (b);
        monitorBox = std::make_unique<ChoiceBox> (s, id::monitor,
            tr ("Escuta: Normal · Só Wet (só os ecos) · Delta (só o que o delay acrescenta) · Mono · Telemóvel (banda 300 Hz–4 kHz) · Club (graves em mono, sem agudos). Não é guardado nos presets.",
                "Listen: Normal · Wet Solo (echoes only) · Delta (only what the delay adds) · Mono · Phone (300 Hz–4 kHz band) · Club (mono lows, no highs). Not saved in presets."),
            juce::StringArray { tr ("ESCUTA: NORMAL", "LISTEN: NORMAL"), tr ("SÓ WET", "WET SOLO"), "DELTA", "MONO", tr ("TELEMÓVEL", "PHONE"), "CLUB" });
        addAndMakeVisible (*monitorBox);
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
        outSlider.setTooltip (tr ("Ganho de saída", "Output gain") + fineTip());
        mixSlider.setTooltip (tr ("Equilíbrio seco/molhado", "Dry/wet balance") + fineTip());
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
        const juce::StringArray names { "TIMING", "SPACE", "FEEDBACK EQ", "MOTION", "TEXTURE", "MOD", "PERFORM", "UTILITIES" };
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
        buildMod (*pages[5]);
        buildPerform (*pages[6]);
        buildUtilities (*pages[7]);
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
        auto& t   = p.add (knob (s, id::time, "TIME", "", col::coral, K::Small, false, tr ("Tempo do eco em ms (quando o SYNC está desligado)", "Echo time in ms (when SYNC is off)")));
        auto& rr  = p.add (knob (s, id::r_ratio, "R RATIO", "", col::coral, K::Small, false, tr ("Tempo do canal direito em relação ao esquerdo (1.00 = igual)", "Right channel time relative to the left (1.00 = equal)")));
        auto& fb  = p.add (knob (s, id::feedback, "FEEDBACK", "", col::coral, K::Small, false, tr ("Quantidade de realimentação", "Amount of feedback")));
        auto& bal = p.add (knob (s, id::fb_balance, "FB L/R", "", col::coral, K::Small, true, tr ("Equilíbrio do feedback entre esquerda e direita", "Feedback balance between left and right")));
        auto& gl  = p.add (knob (s, id::glide, "GLIDE", "", col::text, K::Small, false, tr ("Duração do deslize quando o tempo muda (modo fita)", "Glide duration when the time changes (tape mode)")));
        auto& sync = p.add (std::make_unique<ToggleChip> (s, id::sync, "SYNC", col::cyan, tr ("Sincroniza com o BPM do projecto", "Syncs to the project BPM")));
        auto& div = p.add (std::make_unique<ChoiceBox> (s, id::div, tr ("Divisão musical (D = pontuado, T = tercina)", "Musical division (D = dotted, T = triplet)")));
        auto& tap = p.add (std::make_unique<juce::TextButton> ("TAP"));
        tap.onClick = [this] { proc.tap(); };
        tap.setTooltip (tr ("Toca ao ritmo para definir o tempo", "Tap in rhythm to set the time"));
        auto& route = p.add (std::make_unique<Segmented> (s, id::routing, juce::StringArray { "DUAL", "PING-PONG", "MONO" }, col::coral,
                             tr ("Dual: L e R independentes · Ping-Pong: ecos alternam entre lados · Mono: soma em mono", "Dual: independent L and R · Ping-Pong: echoes alternate between sides · Mono: summed to mono")));
        auto& trans = p.add (std::make_unique<Segmented> (s, id::transition, juce::StringArray { tr ("FITA (GLIDE)", "TAPE (GLIDE)"), "CROSSFADE", "STRETCH" }, col::coral,
                             tr ("Fita: o tempo desliza com mudança de afinação · Crossfade: troca suave sem mudar a afinação · Stretch: o tempo desliza ao ritmo do GLIDE sem mudar a afinação", "Tape: the time glides with a pitch change · Crossfade: smooth switch without changing pitch · Stretch: the time glides at the GLIDE speed without changing pitch")));
        advTime = &t; advDiv = &div;

        p.extraPaint = [this, &p] (juce::Graphics& g)
        {
            const auto& sec = p.sections[1].r.toFloat();
            g.setColour (col::muted); g.setFont (font (13.0f));
            const int r = (int) std::lround (proc.raw.load().routing);
            const String txt = r == 0 ? tr ("Cada lado tem o seu tempo; a matriz SPACE mistura-os em cada repetição.", "Each side has its own time; the SPACE matrix mixes them on every repeat.")
                             : r == 1 ? tr ("A entrada soma-se em mono e os ecos alternam esquerda → direita → esquerda.", "The input is summed to mono and the echoes alternate left → right → left.")
                                      : tr ("Um único eco em mono, centrado. Ideal para 808 e baixos.", "A single centred mono echo. Ideal for 808 and basses.");
            g.drawFittedText (txt, sec.reduced (14.0f).withTrimmedTop (64.0f).toNearestInt(), juce::Justification::topLeft, 3);
            auto tips = p.sections[0].r.toFloat().reduced (18.0f).withTrimmedTop (250.0f);
            g.setColour (col::dim); g.setFont (font (13.0f));
            g.drawFittedText (tr ("Com SYNC ligado o tempo segue o BPM do projecto (D = pontuado, T = tercina).\n"
                                  "R RATIO dá tempos diferentes a cada lado: 1.50 = relação 3:2, 0.75 = 3:4.\n"
                                  "TAP: toca 3–4 vezes ao ritmo. Shift + arrastar = ajuste fino · duplo clique = repor.",
                                  "With SYNC on the time follows the project BPM (D = dotted, T = triplet).\n"
                                  "R RATIO gives each side a different time: 1.50 = 3:2 ratio, 0.75 = 3:4.\n"
                                  "TAP: tap 3–4 times in rhythm. Shift + drag = fine adjust · double-click = reset."),
                              tips.toNearestInt(), juce::Justification::topLeft, 6, 1.0f);
        };
        p.layout = [&p, &t, &div, &rr, &fb, &bal, &gl, &sync, &tap, &route, &trans]
        {
            p.sections = { { { 0, 0, 560, 392 }, tr ("TEMPO E FEEDBACK", "TIME AND FEEDBACK") }, { { 572, 0, 392, 186 }, tr ("ENCAMINHAMENTO", "ROUTING") }, { { 572, 198, 392, 194 }, tr ("TRANSIÇÃO DE TEMPO", "TIME TRANSITION") } };
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
        auto& rot = p.add (knob (s, id::rotation, "ROTATION", "", col::cyan, K::Small, true, tr ("Ângulo da matriz aplicada a cada repetição: os ecos rodam no campo estéreo", "Angle of the matrix applied on every repeat: the echoes rotate in the stereo field")));
        auto& mode = p.add (std::make_unique<Segmented> (s, id::rot_mode, juce::StringArray { tr ("ROTAÇÃO", "ROTATION"), tr ("REFLEXÃO", "REFLECTION") }, col::cyan,
                            tr ("Rotação mantém o sentido; reflexão espelha o campo a cada repetição", "Rotation keeps the direction; reflection mirrors the field on every repeat")));
        auto& ir = p.add (knob (s, id::in_rot, "IN ROT", "", col::cyan, K::Small, true, tr ("Rotação da entrada (±45°; 45° converte L/R em M/S)", "Input rotation (±45°; 45° converts L/R to M/S)")));
        auto& orr = p.add (knob (s, id::out_rot, "OUT ROT", "", col::cyan, K::Small, true, tr ("Rotação da saída (±45°)", "Output rotation (±45°)")));
        auto& iw = p.add (knob (s, id::in_width, "IN WIDTH", "", col::cyan, K::Small, false, tr ("Largura da entrada antes do delay (0 = mono, 200 % = extra largo)", "Input width before the delay (0 = mono, 200 % = extra wide)")));
        auto& ow = p.add (knob (s, id::out_width, "OUT WIDTH", "", col::cyan, K::Small, false, tr ("Largura dos ecos à saída (nível compensado acima de 100 %)", "Width of the echoes at the output (level-compensated above 100 %)")));
        auto& sp = p.add (knob (s, id::space, "SPACE", "", col::cyan, K::Small, false, tr ("Macro SPACE: largura + difusão + rotação extra", "SPACE macro: width + diffusion + extra rotation")));
        auto& ms = p.add (std::make_unique<ToggleChip> (s, id::mono_safe, "MONO-SAFE", col::cyan, tr ("Põe os graves dos ecos (< 150 Hz) em mono — mais seguro em clubes e no telemóvel", "Puts the echoes' lows (< 150 Hz) in mono — safer in clubs and on phones")));
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
            g.drawText (tr ("L → eco", "L → echo"), juce::Rectangle<float> (r.getX(), r.getBottom() + 2, r.getWidth() * 0.5f, 14), juce::Justification::centred);
            g.setColour (col::cyan);
            g.drawText (tr ("R → eco", "R → echo"), juce::Rectangle<float> (r.getCentreX(), r.getBottom() + 2, r.getWidth() * 0.5f, 14), juce::Justification::centred);
        };
        p.layout = [&p, &rot, &mode, &ir, &orr, &iw, &ow, &sp, &ms]
        {
            p.sections = { { { 0, 0, 470, 392 }, tr ("MATRIZ ESPACIAL", "SPATIAL MATRIX") }, { { 482, 0, 482, 200 }, tr ("ENTRADA / SAÍDA", "INPUT / OUTPUT") }, { { 482, 212, 482, 180 }, tr ("MACRO E MONO", "MACRO AND MONO") } };
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
        auto& lc = p.add (knob (s, id::low_cut, "LOW CUT", "", col::cyan, K::Small, false, tr ("Corta graves dentro do loop: cada repetição fica mais leve", "Cuts lows inside the loop: each repeat gets lighter")));
        auto& slope = p.add (std::make_unique<Segmented> (s, id::cut_slope, juce::StringArray { "6 dB", "12 dB", "24 dB" }, col::text, tr ("Inclinação dos cortes (por oitava)", "Cut slope (per octave)"), true));
        auto& pf = p.add (knob (s, id::peak_freq, "PEAK FREQ", "", col::coral, K::Small, false, tr ("Frequência do pico/vale no loop", "Frequency of the peak/dip in the loop")));
        auto& pg = p.add (knob (s, id::peak_gain, "GAIN", "", col::coral, K::Small, true, tr ("Ganho do pico (±18 dB). Ganhos positivos acumulam-se a cada repetição", "Peak gain (±18 dB). Positive gains build up on every repeat")));
        auto& pq = p.add (knob (s, id::peak_q, "Q", "", col::coral, K::Small, false, tr ("Largura de banda do pico (mais alto = mais estreito)", "Peak bandwidth (higher = narrower)")));
        auto& p2f = p.add (knob (s, id::peak2_freq, "PEAK 2", "", col::gold, K::Small, false, tr ("Frequência do segundo pico (ex.: tirar a nasalidade de uma voz nos ecos)", "Frequency of the second peak (e.g. remove nasal tone from vocal echoes)")));
        auto& p2g = p.add (knob (s, id::peak2_gain, "GAIN", "", col::gold, K::Small, true, tr ("Ganho do segundo pico (±18 dB)", "Second peak gain (±18 dB)")));
        auto& p2q = p.add (knob (s, id::peak2_q, "Q", "", col::gold, K::Small, false, tr ("Largura do segundo pico", "Second peak bandwidth")));
        auto& lsf = p.add (knob (s, id::ls_freq, "LOW SHELF", "", col::violet, K::Small, false, tr ("Frequência do shelf de graves no loop", "Low shelf frequency in the loop")));
        auto& lsg = p.add (knob (s, id::ls_gain, "GAIN", "", col::violet, K::Small, true, tr ("Ganho do shelf de graves (±18 dB) — mais suave que o LOW CUT", "Low shelf gain (±18 dB) — gentler than LOW CUT")));
        auto& hsf = p.add (knob (s, id::hs_freq, "HIGH SHELF", "", col::violet, K::Small, false, tr ("Frequência do shelf de agudos no loop", "High shelf frequency in the loop")));
        auto& hsg = p.add (knob (s, id::hs_gain, "GAIN", "", col::violet, K::Small, true, tr ("Ganho do shelf de agudos (±18 dB): escurece os ecos sem os tornar abafados", "High shelf gain (±18 dB): darkens the echoes without muffling them")));
        auto& cp = p.add (knob (s, id::compensate, "COMPENSATE", "", col::text, K::Small, false, tr ("Reduz o feedback na proporção do maior ganho positivo, para o loop não disparar", "Reduces the feedback in proportion to the largest positive gain, so the loop doesn't run away")));
        auto& hc = p.add (knob (s, id::high_cut, "HIGH CUT", "", col::cyan, K::Small, false, tr ("Corta agudos dentro do loop: ecos mais escuros a cada repetição", "Cuts highs inside the loop: darker echoes on every repeat")));
        auto& order = p.add (std::make_unique<LoopOrderList> (s));
        loopOrderList = &order;

        // selector de banda: mostra os knobs do pico 1, do pico 2 ou dos shelves
        const juce::StringArray bandNames { tr ("PICO 1", "PEAK 1"), tr ("PICO 2", "PEAK 2"), "SHELVES" };
        const juce::Colour bandCols[] = { col::coral, col::gold, col::violet };
        for (int i = 0; i < 3; ++i)
        {
            auto* b = eqBandBtns.add (new juce::TextButton (bandNames[i]));
            b->setColour (juce::TextButton::buttonOnColourId, bandCols[i]);
            b->getProperties().set ("fontH", 11.0f);
            b->getProperties().set ("radius", 0.0f);
            b->setTooltip (tr ("Escolhe que banda mostrar nos knobs (os nós do gráfico funcionam sempre)", "Choose which band the knobs show (the graph nodes always work)"));
            b->onClick = [this, i] { selectEqBand (i); };
            p.addAndMakeVisible (b);
        }
        eqBandKnobs[0] = { &pf, &pg, &pq };
        eqBandKnobs[1] = { &p2f, &p2g, &p2q };
        eqBandKnobs[2] = { &lsf, &lsg, &hsf, &hsg };
        disp.onNodeSelected = [this] (int n) { selectEqBand (n == 1 ? 0 : (n == 3 ? 1 : 2)); };

        p.extraPaint = [this, &p] (juce::Graphics& g)
        {
            auto r = p.sections[1].r.toFloat().reduced (12.0f).withTrimmedTop (196.0f);
            g.setColour (col::muted); g.setFont (font (12.5f));
            const float lim = proc.loopLimit.load();
            g.drawFittedText (tr ("Arrasta para reordenar (120 ordens possíveis, automatizável). Limitador suave e bloqueio DC sempre activos no loop.",
                                  "Drag to reorder (120 possible orders, automatable). Soft limiter and DC blocking always active in the loop.")
                              + (lim < -0.05f ? tr ("\nA limitar agora: ", "\nLimiting now: ") + String (lim, 1) + " dB" : String()),
                              r.toNearestInt(), juce::Justification::topLeft, 5);
        };
        p.layout = [this, &p, &disp, &lc, &slope, &cp, &hc, &order]
        {
            p.sections = { { { 0, 0, 0, 0 }, "" }, { { 712, 0, 252, 392 }, tr ("ORDEM DO LOOP", "LOOP ORDER") } };
            disp.setBounds (0, 0, 700, 226);
            lc.setBounds (0, 258, 92, 130);
            slope.setBounds (juce::Rectangle<int> (92, 258, 80, 130).withSizeKeepingCentre (76, 84));
            int x = 186;
            for (auto* b : eqBandBtns) { b->setBounds (x, 234, 112, 22); x += 112; }
            for (int band = 0; band < 3; ++band)
            {
                const auto& ks = eqBandKnobs[band];
                juce::Rectangle<int> area (176, 258, 352, 130);
                const int w = area.getWidth() / (int) ks.size();
                for (auto* k : ks) k->setBounds (area.removeFromLeft (w));
            }
            cp.setBounds (530, 258, 86, 130);
            hc.setBounds (614, 258, 86, 130);
            order.setBounds (724, 32, 228, 162);
            selectEqBand (eqBand);
        };
    }

    juce::OwnedArray<juce::TextButton> eqBandBtns;
    std::array<std::vector<Knob*>, 3> eqBandKnobs;
    int eqBand = 0;
    LoopOrderList* loopOrderList = nullptr;

    void selectEqBand (int band)
    {
        eqBand = juce::jlimit (0, 2, band);
        for (int i = 0; i < 3; ++i)
        {
            if (i < eqBandBtns.size()) eqBandBtns[i]->setToggleState (i == eqBand, juce::dontSendNotification);
            for (auto* k : eqBandKnobs[i]) k->setVisible (i == eqBand);
        }
    }

    void buildMotion (Page& p)
    {
        auto& dep = p.add (knob (s, id::mod_depth, "DEPTH", "", col::violet, K::Small, false, tr ("Profundidade da modulação do tempo, em cents de afinação", "Depth of the time modulation, in cents of pitch")));
        auto& rate = p.add (knob (s, id::mod_rate, "RATE", "", col::violet, K::Small, false, tr ("Velocidade da modulação", "Modulation speed")));
        auto& mdiv = p.add (std::make_unique<ChoiceBox> (s, id::mod_div, tr ("Duração de um ciclo da modulação, sincronizada", "Length of one modulation cycle, synced")));
        auto& msync = p.add (std::make_unique<ToggleChip> (s, id::mod_sync, "SYNC", col::violet, tr ("Sincroniza a modulação com o BPM", "Syncs the modulation to the BPM")));
        auto& off = p.add (knob (s, id::mod_offset, "OFFSET", "", col::violet, K::Small, false, tr ("Desfasamento da modulação entre L e R (50 % = opostos)", "Modulation phase offset between L and R (50 % = opposite)")));
        auto& wow = p.add (knob (s, id::wow, "WOW", "", col::gold, K::Small, false, tr ("Instabilidade de fita: wow lento + flutter", "Tape instability: slow wow + flutter")));
        auto& shape = p.add (std::make_unique<Segmented> (s, id::mod_shape, juce::StringArray { tr ("SENO", "SINE"), tr ("TRIÂNGULO", "TRIANGLE"), tr ("ALEATÓRIO", "RANDOM") }, col::violet, tr ("Forma de onda da modulação", "Modulation waveform")));
        auto& df = p.add (knob (s, id::disp_freq, "FREQUENCY", "", col::green, K::Small, false, tr ("Frequência central da dispersão", "Dispersion centre frequency")));
        auto& ds = p.add (knob (s, id::disp_spread, "SPREAD", "", col::green, K::Small, false, tr ("Atraso de grupo da dispersão: espalha o eco no tempo (efeito 'chirp')", "Dispersion group delay: smears the echo in time ('chirp' effect)")));
        auto& fa = p.add (knob (s, id::diff_amount, "AMOUNT", "", col::violet, K::Small, false, tr ("Difusão: de eco discreto (0) a cauda densa tipo reverb (100 %)", "Diffusion: from a discrete echo (0) to a dense reverb-like tail (100 %)")));
        auto& fs = p.add (knob (s, id::diff_spread, "SPREAD", "", col::violet, K::Small, false, tr ("Tamanho da difusão em ms", "Diffusion size in ms")));
        advRate = &rate; advModDiv = &mdiv;
        p.layout = [&p, &dep, &rate, &mdiv, &msync, &off, &wow, &shape, &df, &ds, &fa, &fs]
        {
            p.sections = { { { 0, 0, 964, 190 }, tr ("MODULAÇÃO", "MODULATION") }, { { 0, 202, 474, 190 }, tr ("DISPERSÃO", "DISPERSION") }, { { 490, 202, 474, 190 }, tr ("DIFUSÃO", "DIFFUSION") } };
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
        auto& wa = p.add (knob (s, id::warmth, "WARMTH", "", col::gold, K::Small, false, tr ("Pré-ênfase de agudos antes da saturação (sem drive é transparente)", "High pre-emphasis before saturation (transparent without drive)")));
        auto& dr = p.add (knob (s, id::drive, "DRIVE", "", col::gold, K::Small, false, tr ("Saturação de fita no loop (com anti-aliasing)", "Tape saturation in the loop (anti-aliased)")));
        auto& bi = p.add (knob (s, id::bits, "BIT DEPTH", "", col::blue, K::Small, false, tr ("Redução de bits (24 = desligado)", "Bit reduction (24 = off)")));
        auto& ra = p.add (knob (s, id::rate, "SAMPLE RATE", "", col::blue, K::Small, false, tr ("Redução da taxa de amostragem (48 kHz = desligado)", "Sample-rate reduction (48 kHz = off)")));
        auto& ji = p.add (knob (s, id::jitter, "JITTER", "", col::blue, K::Small, false, tr ("Instabilidade do relógio digital", "Digital clock instability")));
        auto& gm = p.add (knob (s, id::grit_mix, "GRIT MIX", "", col::blue, K::Small, false, tr ("Mistura paralela do grit", "Parallel grit mix")));
        auto& ch = p.add (knob (s, id::character, "CHARACTER", "", col::coral, K::Small, true, tr ("Macro: fita (direita) ↔ digital (esquerda)", "Macro: tape (right) ↔ digital (left)")));
        auto& pd = p.add (knob (s, id::pre_drive, "PRE-DRIVE", "", col::text, K::Small, true, tr ("Ganho antes do delay (empurra a saturação)", "Gain before the delay (pushes the saturation)")));
        auto& hi = p.add (knob (s, id::hiss, "HISS", "", col::gold, K::Small, false, tr ("Ruído de fita (rosa) só nos ecos — dá vida a ecos muito limpos", "Tape noise (pink) on the echoes only — brings very clean echoes to life")));
        auto& hq = p.add (std::make_unique<ToggleChip> (s, id::sat_hq, "HQ 2x", col::gold, tr ("Saturação com oversampling 2× (menos aliasing em drive alto; +14 amostras compensadas no tempo)", "Saturation with 2× oversampling (less aliasing at high drive; +14 samples compensated in the time)")));
        auto& model = p.add (std::make_unique<Segmented> (s, id::model, juce::StringArray { tr ("FITA", "TAPE"), "BBD", tr ("MULTI-CAB.", "MULTI-HEAD"), tr ("CASSETE", "CASSETTE"), "DIGITAL 80s" }, col::gold,
                             tr ("Modelo de carácter do loop (combina com DRIVE, WARMTH e o EQ)", "Character model of the loop (combines with DRIVE, WARMTH and the EQ)")));
        auto& heads = p.add (std::make_unique<ChoiceBox> (s, id::heads, tr ("Cabeças de leitura ligadas (1 = 1/3 do tempo, 2 = 2/3, 3 = tempo inteiro)", "Playback heads on (1 = 1/3 of the time, 2 = 2/3, 3 = full time)"),
                             juce::StringArray { tr ("CABEÇA 1", "HEAD 1"), tr ("CABEÇA 2", "HEAD 2"), tr ("CABEÇA 3", "HEAD 3"), tr ("CABEÇAS 1+2", "HEADS 1+2"), tr ("CABEÇAS 1+3", "HEADS 1+3"), tr ("CABEÇAS 2+3", "HEADS 2+3"), tr ("CABEÇAS 1+2+3", "HEADS 1+2+3") }));
        textureHeads = &heads;
        p.extraPaint = [this, &p] (juce::Graphics& g)
        {
            auto r = p.sections[3].r.toFloat().reduced (14.0f).withTrimmedTop (70.0f);
            const int m = juce::jlimit (0, 4, (int) std::lround (proc.raw.load().model));
            static const char* pt[5] = { "Fita: saturação suave e wow (o som clássico do DELEX).",
                                         "BBD: delay analógico de chips. Quanto mais longo o tempo, mais escuro; comprime ligeiramente.",
                                         "Multi-cabeça: eco de fita com três cabeças a 1/3, 2/3 e ao tempo inteiro. Escolhe as cabeças.",
                                         "Cassete: banda mais estreita, wow extra e pequenas variações de nível.",
                                         "Digital 80s: rack digital antigo — 12 bits e banda até 12 kHz, ecos limpos mas com grão." };
            static const char* en[5] = { "Tape: soft saturation and wow (the classic DELEX sound).",
                                         "BBD: analog chip delay. The longer the time, the darker; slight compression.",
                                         "Multi-head: tape echo with three heads at 1/3, 2/3 and the full time. Choose the heads.",
                                         "Cassette: narrower band, extra wow and small level drifts.",
                                         "Digital 80s: vintage digital rack — 12 bits and bandwidth up to 12 kHz, clean echoes with grain." };
            g.setColour (col::muted); g.setFont (font (12.5f));
            g.drawFittedText (tr (pt[m], en[m]), r.withTrimmedRight (130.0f).toNearestInt(), juce::Justification::topLeft, 3, 1.0f);
        };
        p.layout = [&p, &wa, &dr, &bi, &ra, &ji, &gm, &ch, &pd, &hi, &hq, &model, &heads]
        {
            p.sections = { { { 0, 0, 300, 190 }, "TAPE SATURATION" }, { { 312, 0, 652, 190 }, "DIGITAL GRIT" },
                           { { 0, 202, 470, 190 }, tr ("CARÁCTER, ENTRADA E RUÍDO", "CHARACTER, INPUT AND NOISE") }, { { 482, 202, 482, 190 }, tr ("MODELO", "MODEL") } };
            row ({ &wa, &dr }, { 10, 40, 280, 140 });
            row ({ &bi, &ra, &ji, &gm }, { 322, 40, 632, 140 });
            row ({ &ch, &pd, &hi }, { 10, 240, 450, 140 });
            model.setBounds (494, 232, 458, 30);
            hq.setBounds (852, 340, 100, 28);
            heads.setBounds (852, 302, 100, 28);
        };
    }

    // ------------------------------------------------------------------ MOD: matriz + morph
    void buildMod (Page& p)
    {
        std::vector<juce::Component*> rowsC;
        static const char* srcIds[4] = { id::mm1_src, id::mm2_src, id::mm3_src, id::mm4_src };
        static const char* dstIds[4] = { id::mm1_dst, id::mm2_dst, id::mm3_dst, id::mm4_dst };
        static const char* amtIds[4] = { id::mm1_amt, id::mm2_amt, id::mm3_amt, id::mm4_amt };
        const juce::StringArray srcNames { tr ("—", "—"), "LFO", tr ("ENVELOPE", "ENVELOPE"), tr ("ALEATÓRIO", "RANDOM"), "SIDECHAIN" };
        const juce::StringArray dstNames { "—", tr ("TEMPO", "TIME"), "FEEDBACK", "MIX", "ROTATION", "LOW CUT", "HIGH CUT", "PEAK FREQ", "DRIVE",
                                           tr ("DIFUSÃO", "DIFFUSION"), "OUT WIDTH", "PITCH MIX", "REVERSE", "MOD DEPTH", "WET TRIM", "TONE", "SPACE" };
        for (int k = 0; k < 4; ++k)
        {
            auto& sb = p.add (std::make_unique<ChoiceBox> (s, srcIds[k], tr ("Fonte: LFO (seno, RATE abaixo) · Envelope da entrada · Aleatório (sample & hold ao RATE) · Envelope do sidechain",
                                                                           "Source: LFO (sine, RATE below) · input envelope · random (sample & hold at RATE) · sidechain envelope"), srcNames));
            auto& db = p.add (std::make_unique<ChoiceBox> (s, dstIds[k], tr ("Destino. TEMPO multiplica o tempo final (±1 oitava, também com SYNC)", "Destination. TIME multiplies the final time (±1 octave, also with SYNC)"), dstNames));
            auto& am = p.add (std::make_unique<AmountSlider> (s, amtIds[k], col::violet, tr ("Quantidade (±100 % = toda a escala do destino)", "Amount (±100 % = the destination's full range)")));
            auto& mt = p.add (std::make_unique<SourceMeter> (proc, k));
            srcMeters.push_back (&mt);
            for (auto* c : std::initializer_list<juce::Component*> { &sb, &db, &am, &mt }) rowsC.push_back (c);
        }
        auto& rate = p.add (knob (s, id::mm_rate, "LFO RATE", "", col::violet, K::Small, false, tr ("Velocidade do LFO e do aleatório da matriz", "Speed of the matrix LFO and random")));
        auto& morph = p.add (knob (s, id::morph, "MORPH A-B", "", col::cyan, K::Small, false,
                                   tr ("Desliza todos os parâmetros do slot actual para os do outro slot (A/B no cabeçalho). Automatiza para transições entre dois sons.",
                                       "Slides every parameter from the current slot to the other slot's (A/B in the header). Automate it for transitions between two sounds.")));
        p.extraPaint = [&p] (juce::Graphics& g)
        {
            g.setColour (col::muted);
            for (int k = 0; k < 4; ++k)
                label (g, String (k + 1), { 16.0f, 52.0f + 76.0f * (float) k, 20.0f, 28.0f }, 15.0f, W::Bold, col::violet, juce::Justification::centredLeft, 0.0f);
            label (g, tr ("FONTE", "SOURCE"), { 44.0f, 30.0f, 140.0f, 14.0f }, 10.5f, W::Semi, col::dim, juce::Justification::centredLeft, 0.16f);
            label (g, tr ("DESTINO", "DESTINATION"), { 192.0f, 30.0f, 150.0f, 14.0f }, 10.5f, W::Semi, col::dim, juce::Justification::centredLeft, 0.16f);
            label (g, tr ("QUANTIDADE", "AMOUNT"), { 350.0f, 30.0f, 200.0f, 14.0f }, 10.5f, W::Semi, col::dim, juce::Justification::centredLeft, 0.16f);
            auto r = p.sections[1].r.toFloat().reduced (14.0f).withTrimmedTop (196.0f);
            g.setColour (col::muted); g.setFont (font (12.5f));
            g.drawFittedText (tr ("1. Faz um som em A.  2. Carrega em B e faz outro.  3. Volta a A e roda o MORPH: os dois sons fundem-se. Botões e escolhas trocam a meio.",
                                  "1. Make a sound on A.  2. Press B and make another.  3. Go back to A and turn MORPH: the two sounds blend. Switches and choices flip halfway."),
                              r.toNearestInt(), juce::Justification::topLeft, 5, 1.0f);
        };
        p.layout = [&p, rowsC, &rate, &morph]
        {
            p.sections = { { { 0, 0, 712, 392 }, tr ("MATRIZ DE MODULAÇÃO", "MODULATION MATRIX") }, { { 724, 0, 240, 392 }, u8 ("MORPH A ↔ B") } };
            for (int k = 0; k < 4; ++k)
            {
                const int y = 52 + 76 * k;
                rowsC[(size_t) k * 4 + 0]->setBounds (44, y, 140, 28);
                rowsC[(size_t) k * 4 + 1]->setBounds (192, y, 150, 28);
                rowsC[(size_t) k * 4 + 2]->setBounds (350, y, 220, 28);
                rowsC[(size_t) k * 4 + 3]->setBounds (44, y + 34, 298, 10);
            }
            rate.setBounds (590, 140, 110, 130);
            morph.setBounds (774, 40, 140, 150);
        };
    }
    std::vector<SourceMeter*> srcMeters;
    ChoiceBox* textureHeads = nullptr;

    // ------------------------------------------------------------------ PERFORM: auto-throw, pads, pitch, reverse
    void buildPerform (Page& p)
    {
        auto& ta = p.add (std::make_unique<ToggleChip> (s, id::throw_auto, "AUTO-THROW", col::coral,
                          tr ("Detecta o fim de cada frase e atira a última palavra para o delay — no tempo certo, sem automação", "Detects the end of each phrase and throws the last word into the delay — on time, no automation")));
        auto& to = p.add (std::make_unique<ToggleChip> (s, id::throw_only, tr ("SÓ THROWS", "THROWS ONLY"), col::coral,
                          tr ("A entrada só entra no delay nos throws (auto, pad THROW ou parâmetro Throw automatizado)", "The input only enters the delay on throws (auto, THROW pad or automated Throw parameter)")));
        auto& th = p.add (knob (s, id::throw_thresh, "THRESHOLD", "", col::coral, K::Small, false, tr ("Nível acima do qual a voz conta como 'a cantar'", "Level above which the vocal counts as 'singing'")));
        auto& tl = p.add (knob (s, id::throw_len, tr ("TAMANHO", "LENGTH"), "", col::coral, K::Small, false, tr ("Quanto do fim da frase é atirado (≈ uma palavra: 200–350 ms)", "How much of the end of the phrase is thrown (≈ one word: 200–350 ms)")));
        auto& tg = p.add (knob (s, id::throw_gap, tr ("PAUSA", "GAP"), "", col::coral, K::Small, false, tr ("Silêncio mínimo para considerar que a frase acabou", "Minimum silence to consider the phrase finished")));
        const char* padNames[4] = { "THROW", "FREEZE", "SPIN", "STOP" };
        const char* notes[4] = { "C1 · 36", "C#1 · 37", "D1 · 38", "D#1 · 39" };
        const juce::Colour padCols[4] = { col::coral, col::cyan, col::violet, col::gold };
        const juce::String padTips[4] = {
            tr ("Atira já a última frase para o delay e mantém o envio aberto enquanto premires", "Throws the last phrase into the delay now and keeps the send open while held"),
            tr ("Congela a cauda enquanto premires", "Freezes the tail while held"),
            tr ("Os ecos rodam à volta do estéreo (uma volta por compasso) enquanto premires", "The echoes spin around the stereo field (one turn per bar) while held"),
            tr ("Tape stop: os ecos abrandam até parar; ao soltar voltam", "Tape stop: the echoes slow down to a halt; they come back on release") };
        for (int i = 0; i < 4; ++i)
        {
            auto& pb = p.add (std::make_unique<PadButton> (proc, i, padNames[i], u8 (notes[i]), padCols[i], padTips[i] + tr ("  (MIDI: nota ", "  (MIDI: note ") + u8 (notes[i]) + ")"));
            pads.push_back (&pb);
        }
        auto& pi = p.add (knob (s, id::pitch, "PITCH", "", col::cyan, K::Small, true, tr ("Afinação dentro do loop: cada repetição sobe (ou desce) de novo. +12 = shimmer", "Pitch inside the loop: each repeat rises (or falls) again. +12 = shimmer")));
        auto& pm = p.add (knob (s, id::pitch_mix, "PITCH MIX", "", col::cyan, K::Small, false, tr ("Quanto do sinal afinado circula no loop", "How much of the pitched signal circulates in the loop")));
        auto& rv = p.add (knob (s, id::reverse, "REVERSE", "", col::green, K::Small, false, tr ("Ecos ao contrário (segmentos do tamanho do tempo do delay)", "Reversed echoes (segments as long as the delay time)")));
        p.extraPaint = [this, &p] (juce::Graphics& g)
        {
            const int c = proc.throwCounter.load();
            if (c != lastThrows) { lastThrows = c; throwFlashUntil = juce::Time::getMillisecondCounter() + 350; }
            const bool flash = juce::Time::getMillisecondCounter() < throwFlashUntil;
            g.setColour (flash ? col::coral : col::line2);
            g.fillEllipse (juce::Rectangle<float> (10, 10).withCentre ({ 440.0f, 17.0f }));
            label (g, "THROW", { 450.0f, 10.0f, 60.0f, 14.0f }, 10.5f, W::Semi, flash ? col::coral : col::dim, juce::Justification::centredLeft, 0.12f);
            auto r = p.sections[1].r.toFloat().reduced (14.0f).withTrimmedTop (150.0f);
            g.setColour (col::muted); g.setFont (font (12.0f));
            g.drawFittedText (tr ("Liga um teclado MIDI à pista do DELEX: notas C1–D#1 tocam os pads. O parâmetro Throw também se automatiza no DAW.",
                                  "Route a MIDI keyboard to the DELEX track: notes C1–D#1 play the pads. The Throw parameter can also be automated in the DAW."),
                              r.toNearestInt(), juce::Justification::topLeft, 3, 1.0f);
        };
        p.layout = [this, &p, &ta, &to, &th, &tl, &tg, &pi, &pm, &rv]
        {
            p.sections = { { { 0, 0, 520, 190 }, "AUTO-THROW" }, { { 532, 0, 432, 190 }, "PADS" },
                           { { 0, 202, 520, 190 }, tr ("PITCH / SHIMMER", "PITCH / SHIMMER") }, { { 532, 202, 432, 190 }, "REVERSE" } };
            ta.setBounds (120, 4, 110, 26); to.setBounds (236, 4, 110, 26);
            row ({ &th, &tl, &tg }, { 10, 40, 500, 140 });
            int x = 546;
            for (auto* pb : pads) { pb->setBounds (x, 36, 96, 100); x += 104; }
            row ({ &pi, &pm }, { 10, 240, 330, 140 });
            rv.setBounds (560, 240, 130, 140);
        };
    }
    std::vector<PadButton*> pads;
    int lastThrows = 0;
    juce::uint32 throwFlashUntil = 0;

    void buildUtilities (Page& p)
    {
        auto& on = p.add (std::make_unique<ToggleChip> (s, id::duck, "DUCK", col::coral, tr ("Liga o ducker", "Turns the ducker on")));
        auto& th = p.add (knob (s, id::duck_thresh, "THRESHOLD", "", col::coral, K::Small, false, tr ("Nível da fonte a partir do qual os ecos baixam", "Source level above which the echoes duck")));
        auto& am = p.add (knob (s, id::duck_amount, "AMOUNT", "", col::coral, K::Small, false, tr ("Quanto os ecos baixam", "How much the echoes duck")));
        auto& at = p.add (knob (s, id::duck_attack, "ATTACK", "", col::coral, K::Small, false, tr ("Rapidez com que os ecos baixam", "How fast the echoes duck")));
        auto& ho = p.add (knob (s, id::duck_hold, "HOLD", "", col::coral, K::Small, false, tr ("Tempo de espera antes de soltar", "Hold time before releasing")));
        auto& re = p.add (knob (s, id::duck_release, "RELEASE", "", col::coral, K::Small, false, tr ("Rapidez com que os ecos voltam", "How fast the echoes come back")));
        auto& gOn = p.add (std::make_unique<ToggleChip> (s, id::gate, "GATE", col::violet, tr ("Liga o gate dos ecos: a cauda corta-se de repente quando a fonte pára (efeito não linear)", "Turns the echo gate on: the tail cuts off abruptly when the source stops (non-linear effect)")));
        auto& gTh = p.add (knob (s, id::gate_thresh, "THRESHOLD", "", col::violet, K::Small, false, tr ("Nível da fonte que abre o gate", "Source level that opens the gate")));
        auto& gHo = p.add (knob (s, id::gate_hold, "HOLD", "", col::violet, K::Small, false, tr ("Quanto tempo a cauda soa depois de a fonte parar", "How long the tail sounds after the source stops")));
        auto& gRe = p.add (knob (s, id::gate_release, "RELEASE", "", col::violet, K::Small, false, tr ("Rapidez do corte (curto = abrupto)", "How fast it cuts (short = abrupt)")));
        auto& key = p.add (std::make_unique<Segmented> (s, id::key_src, juce::StringArray { tr ("INTERNO", "INTERNAL"), "SIDECHAIN" }, col::coral,
                           tr ("O que controla o ducker e o gate: a própria fonte ou a entrada sidechain (ex.: voz a baixar os ecos do instrumental)", "What drives the ducker and gate: the source itself or the sidechain input (e.g. vocal ducking the beat's echoes)")));
        auto& cl = p.add (std::make_unique<ToggleChip> (s, id::const_loud, tr ("VOLUME CONST.", "CONST. LOUDNESS"), col::cyan, tr ("Mantém o volume dos ecos igual quando mexes no feedback (compara presets sem enganar o ouvido)", "Keeps the echo level the same when you change the feedback (compare presets without fooling your ears)")));
        auto& law = p.add (std::make_unique<Segmented> (s, id::mix_law, juce::StringArray { "LINEAR", tr ("POTÊNCIA CONST.", "CONST. POWER") }, col::cyan, tr ("Curva do MIX: potência constante mantém o volume percebido", "MIX curve: constant power keeps the perceived volume")));
        auto& wt = p.add (knob (s, id::wet_trim, "WET TRIM", "", col::text, K::Small, true, tr ("Ganho só dos ecos", "Gain of the echoes only")));
        auto& ot = p.add (knob (s, id::out_trim, "OUTPUT", "", col::text, K::Small, true, tr ("Ganho de saída", "Output gain")));
        auto& mx = p.add (knob (s, id::mix, "MIX", "", col::text, K::Small, false, tr ("Equilíbrio seco/molhado", "Dry/wet balance")));
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
        auto& logout = p.add (std::make_unique<juce::TextButton> (tr ("TERMINAR SESSÃO", "SIGN OUT")));
        logout.getProperties().set ("fontH", 11.0f);
        logout.setTooltip (tr ("Esquece a senha neste computador (pede-a de novo ao abrir)", "Forgets the password on this computer (asks again when opening)"));
        logout.onClick = [] { lic::License::get().lock(); };
        auto& langSw = p.add (std::make_unique<piradex::i18n::LanguageSwitch>());
        langSw.accent = col::cyan;
        langSw.background = col::deep;
        langSw.outline = col::line2;
        langSw.text = col::text;
        langSw.textDim = col::muted;
        langSw.hoverFill = col::panel2;
        p.extraPaint = [this, &p] (juce::Graphics& g)
        {
            // indicador do sidechain
            if (proc.raw.load().key_src > 0.5f)
            {
                const bool on = proc.sidechainActive.load();
                g.setColour (on ? col::green : col::coral);
                g.fillEllipse (juce::Rectangle<float> (7, 7).withCentre ({ 400.0f, 17.0f }));
                label (g, on ? tr ("SIDECHAIN A RECEBER", "SIDECHAIN RECEIVING") : tr ("SEM SIDECHAIN NO DAW", "NO SIDECHAIN FROM DAW"),
                       { 408.0f, 10.0f, 186.0f, 14.0f }, 10.5f, W::Semi, on ? col::green : col::coral, juce::Justification::centredLeft, 0.1f);
            }
            // idioma (rótulo bilingue para quem estiver "perdido" no outro idioma)
            label (g, tr ("IDIOMA · LANGUAGE", "LANGUAGE · IDIOMA"), { 502.0f, 318.0f, 206.0f, 14.0f }, 11.0f, W::Semi, col::muted, juce::Justification::centredLeft, 0.18f);
            auto r = p.sections[4].r.toFloat().reduced (14.0f).withTrimmedTop (20.0f);
            label (g, "DELEX by Piradex  v" JucePlugin_VersionString, r.removeFromTop (18.0f), 15.0f, W::Bold, col::text, juce::Justification::centredLeft, 0.02f);
            g.setColour (col::text2); g.setFont (font (12.5f));
            g.drawFittedText (tr ("Delay espacial estéreo · VST3 / AU / AAX\nCriador e Desenvolvedor:\nRicardo Amorim / Juninho Piradex\nwww.beatfreakstudio.com\nSuporte: juninhopiradex@hotmail.com",
                                  "Stereo spatial delay · VST3 / AU / AAX\nCreator and Developer:\nRicardo Amorim / Juninho Piradex\nwww.beatfreakstudio.com\nSupport: juninhopiradex@hotmail.com"),
                              r.removeFromTop (76.0f).toNearestInt(), juce::Justification::topLeft, 5, 0.85f);
            label (g, lic::License::get().statusText(), r.removeFromTop (16.0f), 12.5f, W::Semi, col::cyan, juce::Justification::centredLeft, 0.02f);
        };
        logoutBtn = &logout;
        p.layout = [&p, &on, &th, &am, &at, &ho, &re, &gOn, &gTh, &gHo, &gRe, &law, &wt, &ot, &mx, sizes, &logout, &langSw, &key, &cl]
        {
            logout.setBounds (746, 356, 130, 26);
            langSw.setBounds (502, 338, 110, 30);
            p.sections = { { { 0, 0, 600, 190 }, "DUCKER" }, { { 612, 0, 352, 190 }, tr ("GATE DOS ECOS", "ECHO GATE") },
                           { { 0, 202, 474, 190 }, tr ("MISTURA E GANHO", "MIX AND GAIN") }, { { 490, 202, 230, 190 }, "INTERFACE" }, { { 732, 202, 232, 190 }, tr ("SOBRE", "ABOUT") } };
            on.setBounds (84, 4, 62, 26);
            key.setBounds (156, 4, 230, 26);
            row ({ &th, &am, &at, &ho, &re }, { 6, 40, 588, 140 });
            gOn.setBounds (760, 4, 62, 26);
            row ({ &gTh, &gHo, &gRe }, { 618, 40, 340, 140 });
            law.setBounds (16, 232, 240, 28);
            cl.setBounds (266, 232, 196, 28);
            row ({ &wt, &ot, &mx }, { 10, 270, 360, 116 });
            int x = 502, y = 236;
            for (auto* b : *sizes) { b->setBounds (x, y, 64, 28); x += 70; if (x > 680) { x = 502; y += 36; } }
        };
    }
    juce::OwnedArray<juce::TextButton> sizeBtns;
    juce::TextButton* logoutBtn = nullptr;

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

    // ------------------------------------------------------------------ senha
    piradex::i18n::LanguageSwitch headerLang;
    LoginOverlay login;

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
        lic::License::get().checkExpiry();
        const bool locked = ! lic::License::get().isUnlocked();
        if (locked != login.isVisible())
        {
            login.setVisible (locked);
            if (locked) { login.toFront (true); browser.reset(); }
        }
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
        if (logoutBtn != nullptr) logoutBtn->setEnabled (lic::License::get().hasKey());
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
            if (pages[5]->isVisible()) for (auto* m : srcMeters) m->tick();
            if (pages[6]->isVisible()) { for (auto* pb : pads) pb->tick(); pages[6]->repaint (430, 4, 90, 26); }
            if (textureHeads != nullptr) textureHeads->setVisible (std::lround (v.model) == 2);
            if (++slowTick % 6 == 0) pages[currentTab]->repaint();   // textos dinâmicos (ordem, limitador, matriz)
        }
        grBar.tick();
        const bool monOn = v.monitor > 0.5f;
        if (monitorBox->getProperties()["alert"] != juce::var (monOn))
        {
            monitorBox->getProperties().set ("alert", monOn);
            monitorBox->setColour (juce::ComboBox::outlineColourId, monOn ? col::coral : col::line2);
            monitorBox->setColour (juce::ComboBox::textColourId, monOn ? col::coral : col::text);
            monitorBox->repaint();
        }
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
    piradex::i18n::reload();          // outro plugin Piradex pode ter mudado o idioma
    buildUi();
    // mudar de idioma reconstrói a interface (depois do clique que o pediu terminar)
    langWatch.onChange = [safe = juce::Component::SafePointer<DelexEditor> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->buildUi(); safe->resized(); } });
    };

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) kW / (double) kH);
        c->setSizeLimits ((int) (kW * 0.75f), (int) (kH * 0.75f), kW * 2, kH * 2);
    }
    const float sc = juce::jlimit (0.75f, 2.0f, proc.uiScale);
    setSize ((int) std::lround (kW * sc), (int) std::lround (kH * sc));
}

void DelexEditor::buildUi()
{
    ui.reset();
    ui = std::make_unique<delex::ui::DelexUI> (proc);
    ++uiBuilds;
    ui->onScale = [this] (float sc) { setScale (sc); };
    addAndMakeVisible (*ui);
}

DelexEditor::~DelexEditor()
{
    langWatch.onChange = nullptr;
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
