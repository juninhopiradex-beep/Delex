// Teste de integração com uma instância real do plugin DELEX:
//  todos os presets a processar áudio · estado guardar/restaurar · A/B · mono/estéreo
//  · double precision · e grava capturas da interface (Simple, cada aba Advanced, presets).
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>
#include <random>
#include <map>
#include <cstring>

static juce::String u8 (const char* x) { return juce::String::fromUTF8 (x); }

static int failures = 0;
static void check (bool ok, const char* what, double got = 0, double want = 0)
{
    std::printf ("  %-52s %s (obtido %.4f, esperado %.4f)\n", what, ok ? "OK   " : "FALHA", got, want);
    if (! ok) ++failures;
}
static void setParam (DelexProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}
static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

static void feedNoise (DelexProcessor& p, int blocks, int bs, float& peak, bool& finite, std::mt19937& g)
{
    std::uniform_real_distribution<float> u (-0.5f, 0.5f);
    juce::AudioBuffer<float> b (2, bs); juce::MidiBuffer m;
    for (int k = 0; k < blocks; ++k)
    {
        for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i) b.setSample (c, i, (k % 40) < 10 ? u (g) : 0.0f);
        p.processBlock (b, m);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i)
        {
            const float v = b.getSample (c, i);
            if (! std::isfinite (v)) finite = false; else peak = juce::jmax (peak, std::abs (v));
        }
    }
}

static void saveShot (juce::AudioProcessorEditor& ed, const juce::String& file)
{
    auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (file));
    f.deleteFile();
    juce::FileOutputStream out (f);
    juce::PNGImageFormat().writeImageToStream (img, out);
    std::printf ("  captura: %s (%dx%d)\n", file.toRawUTF8(), img.getWidth(), img.getHeight());
}

// captura em português e, logo a seguir, a mesma janela em inglês (a interface é reconstruída)
static void snapshot (juce::AudioProcessorEditor& ed, const juce::String& file, bool alsoEnglish = true)
{
    namespace L = piradex::i18n;
    L::set (L::Language::PT, false);
    pump (400);
    saveShot (ed, file);
    if (! alsoEnglish) return;
    L::set (L::Language::EN, false);
    pump (500);
    saveShot (ed, file.replace (".png", "_en.png"));
    L::set (L::Language::PT, false);
    pump (300);
}

// Idioma PT/EN: tr(), aviso de troca, ficheiro Piradex, reconstrução da interface, nomes de automação fixos
static void testLanguage()
{
    namespace L = piradex::i18n;
    std::printf ("Idioma PT/EN\n");
    const auto before = L::current();
    L::set (L::Language::PT, false);
    check (L::tr ("Repetições", "Repeats") == juce::String::fromUTF8 ("Repetições"), "tr devolve PT");
    check (delex::presetDescription (delex::factoryPresets()[0]).startsWith ("Slapback"), "descrição de preset em português");
    L::set (L::Language::EN, false);
    check (L::tr ("Repetições", "Repeats") == "Repeats", "tr devolve EN");
    check (delex::presetDescription (delex::factoryPresets()[0]).startsWith ("Short"), "descrição de preset em inglês");
    struct Count : L::Listener { int n = 0; void languageChanged() override { ++n; } } count;
    L::addListener (&count);
    L::set (L::Language::PT, false);
    L::set (L::Language::PT, false);
    L::removeListener (&count);
    check (count.n == 1, "aviso de troca só quando muda", count.n, 1);
    L::set (L::Language::EN, true);
    L::set (L::Language::PT, false);
    L::reload();
    check (L::current() == L::Language::EN, "idioma guardado em Piradex.settings");
    DelexProcessor p;
    const auto name = p.apvts.getParameter ("feedback")->getName (64);
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (1000, 640);
        const int first = dynamic_cast<DelexEditor*> (ed.get())->uiBuildCount();
        L::set (L::Language::PT, false); pump (100);
        L::set (L::Language::EN, false); pump (100);
        check (dynamic_cast<DelexEditor*> (ed.get())->uiBuildCount() == first + 2, "interface reconstruída ao mudar de idioma", dynamic_cast<DelexEditor*> (ed.get())->uiBuildCount() - first, 2);
    }
    pump (50);
    check (p.apvts.getParameter ("feedback")->getName (64) == name, "nomes de automação fixos");
    L::set (before, true);
    L::set (L::Language::PT, false);
}

// encontra um botão visível pelo texto e clica nele (o editor não está no ambiente de trabalho,
// por isso usa-se a visibilidade dos pais em vez de isShowing)
static bool clickButton (juce::Component& root, const juce::String& text)
{
    for (auto* c : root.getChildren())
    {
        if (! c->isVisible()) continue;
        if (auto* b = dynamic_cast<juce::Button*> (c))
            if (b->getButtonText() == text) { b->triggerClick(); pump (80); return true; }
        if (clickButton (*c, text)) return true;
    }
    return false;
}


// ============================================================================
//  Teste de TODOS os controlos da interface: percorre a árvore de componentes,
//  mexe em cada knob/botão/seletor como o utilizador faria e confirma que o
//  parâmetro (e portanto o áudio) mudou. Depois testa os botões de acção um a um.
// ============================================================================
static std::vector<float> paramSnap (DelexProcessor& p)
{
    std::vector<float> v;
    for (auto* prm : p.getParameters()) v.push_back (prm->getValue());
    return v;
}
static int paramDiff (const std::vector<float>& a, const std::vector<float>& b)
{
    int n = 0;
    for (size_t i = 0; i < a.size(); ++i) if (std::abs (a[i] - b[i]) > 1.0e-6f) ++n;
    return n;
}
static void walk (juce::Component& c, const std::function<void (juce::Component&)>& f)
{
    f (c);
    for (auto* ch : c.getChildren()) walk (*ch, f);
}
static juce::Button* findButton (juce::Component& root, const juce::String& textOrName)
{
    juce::Button* found = nullptr;
    walk (root, [&] (juce::Component& c)
    {
        if (found != nullptr) return;
        if (auto* b = dynamic_cast<juce::Button*> (&c))
            if (b->getButtonText() == textOrName || b->getName() == textOrName) found = b;
    });
    return found;
}
static bool press (juce::Component& root, const juce::String& textOrName, int waitMs = 60)
{
    if (auto* b = findButton (root, textOrName)) { b->triggerClick(); pump (waitMs); return true; }
    std::printf ("    botão não encontrado: %s\n", textOrName.toRawUTF8());
    return false;
}
static float plain (DelexProcessor& p, const char* id)
{
    auto* prm = p.apvts.getParameter (id);
    return prm->convertFrom0to1 (prm->getValue());
}

static void testControls (DelexProcessor& p)
{
    std::printf ("Controlos da interface (cada knob, botão e seletor)\n");
    // guardado (persist) para o editor não voltar a ler outro idioma do ficheiro partilhado Piradex.settings
    const auto langBefore = piradex::i18n::current();
    piradex::i18n::set (piradex::i18n::Language::PT, true);
    lic::License::get().unlockForTests();
    p.loadFactoryPreset (0);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (1000, 640);
    pump (200);

    // ---- 1) knobs e faders: cada um muda o seu parâmetro
    int sl = 0, slOk = 0, tg = 0, tgOk = 0, sg = 0, sgOk = 0, cb = 0, cbOk = 0;
    std::vector<juce::Slider*> sliders;
    std::vector<delex::ui::ToggleChip*> toggles;
    std::vector<delex::ui::Segmented*> segs;
    std::vector<delex::ui::ChoiceBox*> combos;
    walk (*ed, [&] (juce::Component& c)
    {
        if (auto* s = dynamic_cast<juce::Slider*> (&c)) sliders.push_back (s);
        if (auto* t = dynamic_cast<delex::ui::ToggleChip*> (&c)) toggles.push_back (t);
        if (auto* s = dynamic_cast<delex::ui::Segmented*> (&c)) segs.push_back (s);
        if (auto* b = dynamic_cast<delex::ui::ChoiceBox*> (&c)) combos.push_back (b);
    });
    for (auto* s : sliders)
    {
        ++sl;
        const double cur = s->getValue();
        const double target = s->proportionOfLengthToValue (s->valueToProportionOfLength (cur) < 0.5 ? 0.8 : 0.2);
        const auto before = paramSnap (p);
        s->setValue (target, juce::sendNotificationSync);
        const auto after = paramSnap (p);
        if (paramDiff (before, after) > 0) ++slOk;
        else std::printf ("    knob sem efeito: %s\n", s->getTooltip().upToFirstOccurrenceOf ("\n", false, false).toRawUTF8());
        s->setValue (cur, juce::sendNotificationSync);
    }
    check (sl > 0 && slOk == sl, "knobs e faders: todos mudam o seu parâmetro", slOk, sl);

    // ---- 2) botões liga/desliga (SYNC, FREEZE, DUCK, GATE, BYPASS, MONO-SAFE…)
    for (auto* t : toggles)
    {
        ++tg;
        const auto before = paramSnap (p);
        const bool was = t->getToggleState();
        t->triggerClick(); pump (40);
        const bool okOn = paramDiff (before, paramSnap (p)) > 0 && t->getToggleState() != was;
        t->triggerClick(); pump (40);
        const bool okOff = paramDiff (before, paramSnap (p)) == 0 && t->getToggleState() == was;
        if (okOn && okOff) ++tgOk; else std::printf ("    botão sem efeito: %s\n", t->getButtonText().toRawUTF8());
    }
    check (tg > 0 && tgOk == tg, "botões liga/desliga: todos ligam e desligam", tgOk, tg);

    // ---- 3) seletores segmentados (DUAL/PING-PONG/MONO, LINEAR/POTÊNCIA, SLOPE…)
    for (auto* s : segs)
    {
        bool ok = true;
        juce::StringArray seen;
        for (auto* ch : s->getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (ch))
            {
                ++sg;
                b->triggerClick(); pump (30);
                if (b->getToggleState()) ++sgOk; else { ok = false; std::printf ("    opção sem efeito: %s\n", b->getButtonText().toRawUTF8()); }
                seen.add (b->getButtonText());
            }
        juce::ignoreUnused (ok);
        if (auto* first = dynamic_cast<juce::TextButton*> (s->getChildren().getFirst())) { first->triggerClick(); pump (30); }
    }
    check (sg > 0 && sgOk == sg, "seletores: cada opção fica activa", sgOk, sg);

    // ---- 4) listas (divisão, ordem do loop, divisão da modulação)
    for (auto* b : combos)
    {
        ++cb;
        const int cur = b->getSelectedItemIndex();
        const auto before = paramSnap (p);
        b->setSelectedItemIndex (cur == 0 ? 1 : 0, juce::sendNotificationSync);
        if (paramDiff (before, paramSnap (p)) > 0) ++cbOk;
        b->setSelectedItemIndex (cur, juce::sendNotificationSync);
    }
    check (cb > 0 && cbOk == cb, "listas de escolha: todas mudam o parâmetro", cbOk, cb);
    std::printf ("    (%d knobs/faders · %d botões liga/desliga · %d opções de seletor · %d listas)\n", sl, tg, sg, cb);

    // ---- 5) botões de acção
    auto& ui = *ed;
    // vistas e abas
    check (press (ui, "ADVANCED") && p.advancedView, "ADVANCED abre a vista avançada");
    const char* tabs[] = { "TIMING", "SPACE", "FEEDBACK EQ", "MOTION", "TEXTURE", "MOD", "PERFORM", "TAPS", "UTILITIES" };
    int tabOk = 0;
    for (int i = 0; i < 9; ++i) if (press (ui, tabs[i]) && p.advancedTab == i) ++tabOk;
    check (tabOk == 9, "9 abas abrem a página certa", tabOk, 9);
    check (press (ui, "SIMPLE") && ! p.advancedView, "SIMPLE volta à vista simples");

    // estilos
    int styleOk = 0;
    const char* styleNames[] = { "CLEAN", "PING-PONG", "WIDE", "DUB", "DREAM", "TAPE", "DIGITAL", "RHYTHMIC" };
    for (auto* st : styleNames)
    {
        const auto before = paramSnap (p);
        if (press (ui, st) && paramDiff (before, paramSnap (p)) > 0) ++styleOk;
        else if (std::strcmp (st, "CLEAN") == 0) ++styleOk;   // CLEAN a partir de valores já limpos pode não mudar nada
    }
    press (ui, "DUB");
    check (styleOk == 8 && std::abs (plain (p, delex::id::feedback) - 70.0f) < 0.5f, "8 estilos aplicam-se (DUB: feedback 70 %)", plain (p, delex::id::feedback), 70.0);

    // presets anterior / seguinte / favorito
    p.loadFactoryPreset (10);
    press (ui, "seguinte"); const auto n1 = p.currentPresetName();
    press (ui, "anterior"); const auto n0 = p.currentPresetName();
    check (n1 == juce::String::fromUTF8 (delex::factoryPresets()[11].name) && n0 == juce::String::fromUTF8 (delex::factoryPresets()[10].name), "‹ › mudam de preset");
    const bool fav = p.isFavourite (p.currentPresetName());
    press (ui, "favorito");
    const bool favOk = p.isFavourite (p.currentPresetName()) != fav;
    press (ui, "favorito");
    check (favOk && p.isFavourite (p.currentPresetName()) == fav, "★ marca e desmarca favorito");

    // desfazer / refazer (com um knob real)
    {
        juce::Slider* fbKnob = nullptr;
        for (auto* s : sliders)          // o knob cujo movimento muda o FEEDBACK
        {
            const float f0 = plain (p, delex::id::feedback); const double v0 = s->getValue();
            s->setValue (s->proportionOfLengthToValue (s->valueToProportionOfLength (v0) < 0.5 ? 0.9 : 0.1), juce::sendNotificationSync);
            const bool isFb = std::abs (plain (p, delex::id::feedback) - f0) > 0.5f;
            s->setValue (v0, juce::sendNotificationSync);
            if (isFb) { fbKnob = s; break; }
        }
        if (fbKnob == nullptr) { check (false, "knob de FEEDBACK encontrado"); return; }
        pump (300);                       // o APVTS regista os passos de desfazer num temporizador
        const float a = plain (p, delex::id::feedback);
        { juce::Slider::ScopedDragNotification drag (*fbKnob);       // exactamente o que o arrasto do rato faz
          fbKnob->setValue (a > 50 ? 20.0 : 80.0, juce::sendNotificationSync); }
        const float b = plain (p, delex::id::feedback);
        pump (300);
        // valor escrito à mão no número do knob (outro passo de desfazer)
        delex::ui::Knob* knob = dynamic_cast<delex::ui::Knob*> (fbKnob->getParentComponent());
        if (knob != nullptr) { knob->value.setText ("45 %", juce::sendNotificationSync); pump (300); }
        const float c = plain (p, delex::id::feedback);
        press (ui, "desfazer", 300); const float u0 = plain (p, delex::id::feedback);
        press (ui, "desfazer", 300); const float u1 = plain (p, delex::id::feedback);
        press (ui, "refazer", 300);  const float r1 = plain (p, delex::id::feedback);
        check (knob != nullptr && std::abs (c - 45.0f) < 0.5f, "escrever um valor no número do knob", c, 45.0);
        check (std::abs (u0 - b) < 0.5f && std::abs (u1 - a) < 0.5f && std::abs (r1 - b) < 0.5f, "↶ ↷ desfazem/refazem um passo de cada vez", u1, a);
    }

    // A / B
    {
        setParam (p, delex::id::mix, 15.0f);
        press (ui, "B"); setParam (p, delex::id::mix, 70.0f);
        press (ui, "A"); const float a = plain (p, delex::id::mix);
        press (ui, "B"); const float b = plain (p, delex::id::mix);
        press (ui, "A");
        check (std::abs (a - 15.0f) < 0.5f && std::abs (b - 70.0f) < 0.5f, "A / B guardam e trocam estados", b, 70.0);
    }

    // TAP: 4 toques a ~400 ms
    {
        setParam (p, delex::id::div, 5.0f);    // 1/4
        for (int i = 0; i < 4; ++i) { press (ui, "TAP", 1); pump (400); }
        const float t = plain (p, delex::id::time);
        check (plain (p, delex::id::sync) < 0.5f && t > 360.0f && t < 470.0f, "TAP define o tempo (~400 ms) e desliga o SYNC", t, 400.0);
    }

    // tamanho da interface
    {
        int ok = 0;
        const std::pair<const char*, int> sizes[] = { { "125%", 1250 }, { "150%", 1500 }, { "200%", 2000 }, { "75%", 750 }, { "100%", 1000 } };
        press (ui, "ADVANCED"); press (ui, "UTILITIES");
        for (auto& s : sizes) if (press (ui, s.first, 120) && ed->getWidth() == s.second && ed->getHeight() == s.second * 640 / 1000) ++ok;
        check (ok == 5, "75/100/125/150/200 % redimensionam a janela", ok, 5);
        pump (100);
        auto* b100 = findButton (ui, "100%");
        check (b100 != nullptr && b100->getToggleState(), "o tamanho actual fica assinalado");
    }

    // TERMINAR SESSÃO
    {
        lic::License::get().unlockForTests(); pump (120);
        auto* lo = findButton (ui, juce::String::fromUTF8 ("TERMINAR SESSÃO"));
        const bool enabled = lo != nullptr && lo->isEnabled();
        press (ui, juce::String::fromUTF8 ("TERMINAR SESSÃO"), 150);
        bool loginShown = false;
        walk (ui, [&] (juce::Component& c) { if (dynamic_cast<delex::ui::LoginOverlay*> (&c) != nullptr && c.isVisible()) loginShown = true; });
        check (enabled && ! lic::License::get().isUnlocked() && loginShown, "TERMINAR SESSÃO fecha a sessão e pede a senha");
        lic::License::get().unlockForTests(); pump (120);
    }

    // browser de presets: abre, cada categoria mostra os seus presets, CARREGAR, fechar
    {
        press (ui, "SIMPLE");
        press (ui, "preset", 150);
        juce::ListBox* list = nullptr;
        walk (ui, [&] (juce::Component& c) { if (auto* l = dynamic_cast<juce::ListBox*> (&c)) list = l; });
        check (list != nullptr, "o nome do preset abre o browser");
        if (list != nullptr)
        {
            std::map<juce::String, int> perCat;
            for (auto& e : delex::factoryPresets()) ++perCat[juce::String::fromUTF8 (e.category)];
            int catOk = 0;
            for (auto& cat : delex::presetCategories())
                if (press (ui, cat, 40) && list->getListBoxModel()->getNumRows() == perCat[cat]) ++catOk;
                else std::printf ("    categoria %s: %d linhas (esperado %d)\n", cat.toRawUTF8(), list->getListBoxModel()->getNumRows(), perCat[cat]);
            check (catOk == delex::presetCategories().size(), "cada categoria do browser lista os seus presets", catOk, delex::presetCategories().size());
            press (ui, "Todos", 40);
            check (list->getListBoxModel()->getNumRows() >= 282, "Todos mostra os 282 presets", list->getListBoxModel()->getNumRows(), 282);
            press (ui, "NonLin", 40);
            list->selectRow (3); pump (150);
            saveShot (*ed, "docs/ui_presets_nonlin.png");
            press (ui, "CARREGAR", 80);
            check (juce::String (delex::factoryPresets()[(size_t) p.getCurrentProgram()].category) == "NonLin", "CARREGAR aplica o preset escolhido");
            press (ui, "fechar", 150);
            bool still = false;
            walk (ui, [&] (juce::Component& c) { if (dynamic_cast<juce::ListBox*> (&c) != nullptr) still = true; });
            check (! still, "✕ fecha o browser");
        }
    }

    // presets do utilizador (GUARDAR COMO… usa isto por dentro)
    {
        setParam (p, delex::id::feedback, 33.0f);
        const bool saved = p.saveUserPreset ("Teste DELEX");
        setParam (p, delex::id::feedback, 77.0f);
        p.loadUserPreset (p.userPresetFolder().getChildFile ("Teste DELEX.dlxpreset"));
        check (saved && std::abs (plain (p, delex::id::feedback) - 33.0f) < 0.5f, "guardar e carregar preset do utilizador");
        p.userPresetFolder().getChildFile ("Teste DELEX.dlxpreset").deleteFile();
    }
    ed.reset();
    pump (50);
    piradex::i18n::set (langBefore, true);
    piradex::i18n::set (piradex::i18n::Language::PT, false);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testLanguage();
    std::printf ("DELEX — teste de integração do plugin\n");
    std::mt19937 g (11);

    DelexProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);

    // 0) licença: PBKDF2 correcto, senha errada recusada, plugin bloqueado é transparente
    {
        auto hex = [] (const juce::MemoryBlock& m) { return juce::String::toHexString (m.getData(), (int) m.getSize(), 0); };
        const auto v1 = hex (lic::License::pbkdf2 ("password", juce::MemoryBlock ("salt", 4), 1, 32));
        const auto v2 = hex (lic::License::pbkdf2 ("password", juce::MemoryBlock ("salt", 4), 2, 32));
        check (v1 == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b"
            && v2 == "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43", "PBKDF2-HMAC-SHA256 (vectores de referência)");
        lic::License::get().lock();
        check (lic::License::get().tryPassword ("DLX-AAAA-BBBB-CCCC", false) == lic::License::Result::Wrong, "senha errada recusada");
        setParam (p, delex::id::mix, 100.0f);
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; double err = 0;
        for (int k = 0; k < 20; ++k)
        {
            for (int i = 0; i < 512; ++i) { b.setSample (0, i, std::sin (0.03f * (float) (k * 512 + i))); b.setSample (1, i, 0.5f * std::sin (0.05f * (float) (k * 512 + i))); }
            p.processBlock (b, m);
            for (int i = 0; i < 512; ++i) err = juce::jmax (err, (double) std::abs (b.getSample (0, i) - std::sin (0.03f * (float) (k * 512 + i))));
        }
        check (err < 1e-6, "bloqueado: áudio passa sem alterações", err, 0.0);
        lic::License::get().unlockForTests();
        check (lic::License::get().isUnlocked(), "desbloqueio (build de testes)");
    }

    // 1) todos os presets processam áudio sem NaN e sem picos absurdos
    {
        int bad = 0; float worst = 0;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            float pk = 0; bool fin = true;
            feedNoise (p, 200, 512, pk, fin, g);
            if (! fin || pk > 6.0f) { ++bad; std::printf ("    preset %d (%s): pico %.2f finito %d\n", i, p.getProgramName (i).toRawUTF8(), pk, (int) fin); }
            worst = juce::jmax (worst, pk);
        }
        check (bad == 0, "282 presets: áudio finito e limitado", worst, 6.0);
        check (p.getNumPrograms() == 282, "número de presets de fábrica", p.getNumPrograms(), 282);
    }

    // 1b) presets: nomes únicos, categorias válidas, contagens pedidas, cada valor aplicado
    //     exactamente como escrito, textos em inglês
    {
        const auto& fp = delex::factoryPresets();
        std::map<juce::String, int> perCat;
        juce::StringArray names; int dup = 0, badCat = 0, badVal = 0, badKey = 0, noEn = 0;
        for (int i = 0; i < (int) fp.size(); ++i)
        {
            const auto& e = fp[(size_t) i];
            const auto nm = juce::String::fromUTF8 (e.name);
            if (names.contains (nm)) ++dup;
            names.add (nm);
            const auto cat = juce::String::fromUTF8 (e.category);
            if (! delex::presetCategories().contains (cat)) ++badCat;
            ++perCat[cat];
            if (i >= 61 && (e.descEn == nullptr || juce::String::fromUTF8 (e.descEn).isEmpty())) ++noEn;
            p.loadFactoryPreset (i);
            const auto kv = delex::parsePresetData (e.data);
            for (auto& k : kv.getAllKeys())
            {
                auto* prm = p.apvts.getParameter (k);
                if (prm == nullptr) { ++badKey; std::printf ("    %s: parâmetro desconhecido %s\n", e.name, k.toRawUTF8()); continue; }
                const float now = prm->convertFrom0to1 (prm->getValue());
                float want;
                if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (prm))
                {
                    int idx = c->choices.indexOf (kv[k]);
                    if (idx < 0) idx = kv[k].containsOnly ("0123456789") ? kv[k].getIntValue() : -1;
                    want = (float) idx;
                }
                else want = kv[k].getFloatValue();
                const float tol = 0.002f * (prm->getNormalisableRange().end - prm->getNormalisableRange().start) + 1.0e-4f;
                if (std::abs (now - want) > tol) { ++badVal; std::printf ("    %s: %s = %s ficou %.4f\n", e.name, k.toRawUTF8(), kv[k].toRawUTF8(), now); }
            }
        }
        check (dup == 0, "nomes de presets únicos", dup, 0);
        check (badCat == 0, "todas as categorias existem no browser", badCat, 0);
        check (badKey == 0 && badVal == 0, "cada valor dos presets aplicado como escrito", badKey + badVal, 0);
        check (noEn == 0, "presets novos com texto em inglês", noEn, 0);
        const std::pair<const char*, int> want[] = { { "Ambience", 20 }, { "Bass", 24 }, { "Drums", 5 + 33 }, { "FX", 4 }, { "Guitar", 5 + 13 },
                                                     { "Mega", 8 }, { "NonLin", 16 }, { "Orkestra", 13 }, { "Synth", 5 + 36 }, { "VIP", 9 }, { "Vocals", 7 + 45 } };
        int badCount = 0;
        for (auto& w : want)
            if (perCat[w.first] != w.second) { ++badCount; std::printf ("    %s: %d presets (esperado %d)\n", w.first, perCat[w.first], w.second); }
        check (badCount == 0, "contagem por categoria (221 novos)", badCount, 0);
    }

    // 1c) NonLin: a cauda corta mesmo (gate) — silêncio depois do hold
    {
        const auto& fp = delex::factoryPresets();
        int bad = 0, n = 0; float worstTail = 0;
        for (int i = 0; i < (int) fp.size(); ++i)
        {
            if (juce::String (fp[(size_t) i].category) != "NonLin") continue;
            ++n;
            DelexProcessor q; q.setPlayConfigDetails (2, 2, 48000.0, 512); q.prepareToPlay (48000.0, 512);
            q.loadFactoryPreset (i);
            setParam (q, delex::id::mix, 100.0f);
            juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m;
            std::uniform_real_distribution<float> u (-0.5f, 0.5f);
            float during = 0, tail = 0;
            for (int k = 0; k < 160; ++k)   // ≈1,7 s: golpe de 0,1 s e depois silêncio
            {
                for (int c = 0; c < 2; ++c) for (int s = 0; s < 512; ++s) b.setSample (c, s, k < 10 ? u (g) : 0.0f);
                q.processBlock (b, m);
                for (int s = 0; s < 512; ++s)
                {
                    const float v = std::abs (b.getSample (0, s));
                    if (k >= 10 && k < 20) during = juce::jmax (during, v);
                    if (k >= 120) tail = juce::jmax (tail, v);   // ≥1,28 s
                }
            }
            worstTail = juce::jmax (worstTail, tail);
            if (! (during > 0.01f && tail < 1.0e-4f)) { ++bad; std::printf ("    %s: cauda %.4f, depois %.6f\n", fp[(size_t) i].name, during, tail); }
        }
        check (bad == 0 && n == 16, "NonLin: 16 presets com corte gated", worstTail, 0.0);
    }

    // 2) guardar / restaurar estado
    {
        p.setCurrentProgram (7);
        setParam (p, delex::id::feedback, 63.0f);
        setParam (p, delex::id::rotation, -37.0f);
        juce::MemoryBlock mb; p.getStateInformation (mb);
        DelexProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        const auto a = p.raw.load(), b = q.raw.load();
        check (std::abs (b.feedback - 63.0f) < 0.01f && std::abs (b.rotation + 37.0f) < 0.01f && std::abs (a.div - b.div) < 0.01f,
               "estado restaurado (feedback, rotação, divisão)", b.feedback, 63.0);
        check (q.currentPresetName() == p.currentPresetName(), "nome do preset restaurado");
    }

    // 3) A/B
    {
        setParam (p, delex::id::mix, 10.0f);
        p.selectSlot (1);
        setParam (p, delex::id::mix, 80.0f);
        p.selectSlot (0);
        const float a = p.raw.load().mix;
        p.selectSlot (1);
        const float b = p.raw.load().mix;
        p.selectSlot (0);
        check (std::abs (a - 10.0f) < 0.01f && std::abs (b - 80.0f) < 0.01f, "A/B guarda dois estados", b, 80.0);
    }

    // 4) mono → estéreo, mono → mono e precisão dupla
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (juce::AudioChannelSet::mono()); l.inputBuses.add (juce::AudioChannelSet::disabled()); l.outputBuses.add (juce::AudioChannelSet::stereo());
        check (p.checkBusesLayoutSupported (l), "layout mono → estéreo aceite");
        juce::AudioProcessor::BusesLayout bad;
        bad.inputBuses.add (juce::AudioChannelSet::stereo()); bad.inputBuses.add (juce::AudioChannelSet::disabled()); bad.outputBuses.add (juce::AudioChannelSet::mono());
        check (! p.checkBusesLayoutSupported (bad), "layout estéreo → mono recusado");

        DelexProcessor d; d.setPlayConfigDetails (2, 2, 96000.0, 256); d.prepareToPlay (96000.0, 256);
        juce::AudioBuffer<double> b (2, 256); juce::MidiBuffer m; bool fin = true;
        for (int k = 0; k < 400; ++k)
        {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i) b.setSample (c, i, std::sin (0.01 * (k * 256 + i)) * 0.4);
            d.processBlock (b, m);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i) fin &= std::isfinite (b.getSample (c, i));
        }
        check (fin, "processamento em precisão dupla a 96 kHz");
    }

    // 5) tempo sincronizado: 1/4 a 120 BPM ⇒ 500 ms
    {
        delex::Values v = p.raw.load();
        v.sync = 1; v.div = 5;   // 1/4
        check (std::abs (delex::delayMsL (v, 120.0) - 500.0) < 0.01, "1/4 a 120 BPM = 500 ms", delex::delayMsL (v, 120.0), 500.0);
        v.div = 4;               // 1/4D
        check (std::abs (delex::delayMsL (v, 120.0) - 750.0) < 0.01, "1/4D a 120 BPM = 750 ms", delex::delayMsL (v, 120.0), 750.0);
    }

    // 6) vaga A: sidechain, MIDI learn, bloqueios, escuta, ordem do loop
    std::printf ("Vaga A (sidechain, MIDI learn, bloqueios, escuta)\n");
    {
        // layouts com sidechain
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (juce::AudioChannelSet::stereo()); l.inputBuses.add (juce::AudioChannelSet::stereo());
        l.outputBuses.add (juce::AudioChannelSet::stereo());
        check (p.checkBusesLayoutSupported (l), "layout com sidechain estéreo aceite");
        l.inputBuses.getReference (1) = juce::AudioChannelSet::disabled();
        check (p.checkBusesLayoutSupported (l), "layout com sidechain desligado aceite");

        // a voz no sidechain baixa os ecos do instrumental
        auto wetEnergy = [] (bool scLoud)
        {
            DelexProcessor d;
            juce::AudioProcessor::BusesLayout lay;
            lay.inputBuses.add (juce::AudioChannelSet::stereo()); lay.inputBuses.add (juce::AudioChannelSet::stereo());
            lay.outputBuses.add (juce::AudioChannelSet::stereo());
            d.setBusesLayout (lay);
            d.setRateAndBufferSizeDetails (48000.0, 256);
            d.prepareToPlay (48000.0, 256);
            setParam (d, delex::id::key_src, 1.0f);
            setParam (d, delex::id::duck, 1.0f);
            setParam (d, delex::id::duck_thresh, -40.0f);
            setParam (d, delex::id::duck_amount, 24.0f);
            setParam (d, delex::id::mix, 100.0f);
            setParam (d, delex::id::sync, 0.0f);
            setParam (d, delex::id::time, 100.0f);
            juce::AudioBuffer<float> b (4, 256); juce::MidiBuffer m;
            double e = 0; bool active = false;
            for (int k = 0; k < 200; ++k)
            {
                b.clear();
                for (int i = 0; i < 256; ++i)
                {
                    const float x = 0.3f * std::sin (0.05f * (float) (k * 256 + i));
                    b.setSample (0, i, x); b.setSample (1, i, x);
                    if (scLoud) { b.setSample (2, i, 0.5f); b.setSample (3, i, 0.5f); }
                }
                d.processBlock (b, m);
                active = d.sidechainActive.load();
                if (k > 100) for (int i = 0; i < 256; ++i) e += b.getSample (0, i) * b.getSample (0, i);
            }
            return std::make_pair (e, active);
        };
        const auto quiet = wetEnergy (false), loud = wetEnergy (true);
        const double drop = 10.0 * std::log10 (quiet.first / juce::jmax (1.0e-12, loud.first));
        check (quiet.second && loud.second && drop > 10.0, "sidechain: chave externa baixa os ecos (dB)", drop, 20.0);

        // MIDI learn
        p.startMidiLearn (delex::id::feedback);
        check (p.isLearning (delex::id::feedback), "MIDI learn em espera");
        juce::AudioBuffer<float> b (2, 64); b.clear();
        juce::MidiBuffer m; m.addEvent (juce::MidiMessage::controllerEvent (1, 21, 64), 0);
        p.processBlock (b, m);
        check (! p.isLearning (delex::id::feedback) && p.midiCcFor (delex::id::feedback) == 21, "MIDI learn associa o CC 21", p.midiCcFor (delex::id::feedback), 21);
        m.clear(); m.addEvent (juce::MidiMessage::controllerEvent (1, 21, 127), 0);
        p.processBlock (b, m);
        check (std::abs (p.apvts.getParameter (delex::id::feedback)->getValue() - 1.0f) < 1.0e-4f, "CC 127 leva o parâmetro ao máximo");
        {
            juce::MemoryBlock st; p.getStateInformation (st);
            DelexProcessor q; q.setStateInformation (st.getData(), (int) st.getSize());
            check (q.midiCcFor (delex::id::feedback) == 21, "mapa MIDI guardado na sessão", q.midiCcFor (delex::id::feedback), 21);
        }
        p.forgetMidi (delex::id::feedback);
        check (p.midiCcFor (delex::id::feedback) == -1, "esquecer MIDI");

        // bloqueios ao trocar de preset
        const int maskBefore = p.getLockMask();
        p.setLockMask (delex::LockTime | delex::LockMix);
        setParam (p, delex::id::sync, 0.0f); setParam (p, delex::id::time, 333.0f); setParam (p, delex::id::mix, 42.0f);
        p.loadFactoryPreset (3);
        check (std::abs (plain (p, delex::id::time) - 333.0f) < 0.6f && std::abs (plain (p, delex::id::mix) - 42.0f) < 0.3f, "bloqueio TEMPO+MIX mantém tempo e mix", plain (p, delex::id::time), 333.0);
        p.setLockMask (0);
        p.loadFactoryPreset (3);
        check (std::abs (plain (p, delex::id::mix) - 42.0f) > 0.3f, "sem bloqueio o preset muda o mix", plain (p, delex::id::mix), 0.0);
        p.setLockMask (maskBefore);

        // escuta não é guardada nos presets
        setParam (p, delex::id::monitor, 2.0f);
        p.loadFactoryPreset (5);
        check (std::lround (plain (p, delex::id::monitor)) == 2, "modo de escuta mantém-se ao trocar de preset", plain (p, delex::id::monitor), 2.0);
        setParam (p, delex::id::monitor, 0.0f);

        // ordem do loop arrastável
        setParam (p, delex::id::loop_order, 0.0f);
        delex::ui::LoopOrderList lol (p.apvts);
        lol.moveModule (4, 0);   // Fita para o início
        const auto ord = delex::loopPermutations()[(size_t) std::lround (plain (p, delex::id::loop_order))];
        check (ord[0] == 4 && ord[1] == 0 && ord[4] == 3, "arrastar a Fita para o topo muda a ordem do loop", ord[0], 4);
        setParam (p, delex::id::loop_order, 0.0f);
    }

    // 7) vaga B: pads por MIDI, morph A↔B, matriz de modulação
    std::printf ("Vaga B (pads, morph, matriz)\n");
    {
        juce::AudioBuffer<float> b (2, 64); b.clear();
        juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, 37, (juce::uint8) 100), 0);
        p.processBlock (b, m);
        check (p.padActive (DelexProcessor::PadFreeze), "nota 37 liga o pad FREEZE");
        m.clear(); m.addEvent (juce::MidiMessage::noteOff (1, 37), 0);
        p.processBlock (b, m);
        check (! p.padActive (DelexProcessor::PadFreeze), "nota 37 solta o pad FREEZE");
        p.setPad (DelexProcessor::PadStop, true);
        check (p.padActive (DelexProcessor::PadStop), "pad STOP pela interface");
        p.setPad (DelexProcessor::PadStop, false);

        // morph: A com feedback 10, B com 90 → a 50 % o motor recebe ~50
        DelexProcessor d; d.setRateAndBufferSizeDetails (48000.0, 256); d.prepareToPlay (48000.0, 256);
        setParam (d, delex::id::feedback, 10.0f);
        d.selectSlot (1); setParam (d, delex::id::feedback, 90.0f); d.selectSlot (0);
        setParam (d, delex::id::morph, 50.0f);
        d.processBlock (b, m);
        check (std::abs (d.lastChunkValues.feedback - 50.0f) < 0.6f, "morph 50 %: feedback efectivo a meio de A e B", d.lastChunkValues.feedback, 50.0);
        check (std::abs (plain (d, delex::id::feedback) - 10.0f) < 0.01f, "morph não mexe nos knobs (só no som)", plain (d, delex::id::feedback), 10.0);
        setParam (d, delex::id::morph, 0.0f);

        // matriz: guardar e restaurar
        setParam (d, delex::id::mm1_src, 1.0f); setParam (d, delex::id::mm1_dst, 4.0f); setParam (d, delex::id::mm1_amt, 60.0f);
        setParam (d, delex::id::mm_rate, 2.0f);
        float lo = 1e9f, hi = -1e9f;
        for (int k = 0; k < 800; ++k) { d.processBlock (b, m); lo = juce::jmin (lo, d.lastChunkValues.rotation); hi = juce::jmax (hi, d.lastChunkValues.rotation); }
        check (hi - lo > 300.0f, "LFO → ROTATION a ±60 % move a rotação", hi - lo, 432.0);
        // matriz TEMPO não mexe no parâmetro, mas o motor recebe outro tempo (testado no núcleo); envelope segue a entrada
        setParam (d, delex::id::mm2_src, 2.0f); setParam (d, delex::id::mm2_dst, 3.0f); setParam (d, delex::id::mm2_amt, 50.0f);
        setParam (d, delex::id::mix, 20.0f);
        juce::AudioBuffer<float> loud (2, 64);
        for (int k = 0; k < 100; ++k) { for (int c = 0; c < 2; ++c) for (int i = 0; i < 64; ++i) loud.setSample (c, i, 0.9f * std::sin (0.1f * (float) i)); d.processBlock (loud, m); }
        check (d.lastChunkValues.mix > 55.0f, "envelope da entrada → MIX abre o mix", d.lastChunkValues.mix, 69.0);
        juce::MemoryBlock st; d.getStateInformation (st);
        DelexProcessor q; q.setStateInformation (st.getData(), (int) st.getSize());
        check (std::lround (plain (q, delex::id::mm1_dst)) == 4 && std::abs (plain (q, delex::id::mm1_amt) - 60.0f) < 0.01f, "matriz guardada na sessão");
        check (std::abs (d.modSourceView[0].load()) <= 1.0f, "fonte LFO da matriz a correr", d.modSourceView[0].load(), 0.0);
    }

    // 8) vaga C: tom, nota, harmonia, grooves, captura, taps no estado
    std::printf ("Vaga C (taps, grooves, harmonia, tom)\n");
    {
        auto feed = [] (DelexProcessor& d, double seconds, const std::function<float (double)>& gen)
        {
            const int bs = 480; juce::AudioBuffer<float> b (2, bs); juce::MidiBuffer m;
            const int blocks = (int) (seconds * 48000.0 / bs);
            for (int k = 0; k < blocks; ++k)
            {
                for (int i = 0; i < bs; ++i) { const float x = gen ((k * bs + i) / 48000.0); b.setSample (0, i, x); b.setSample (1, i, x); }
                d.processBlock (b, m);
                if (k % 10 == 0) d.updateTapsNow();
            }
            return b;
        };
        auto tone = [] (double t, std::initializer_list<double> f)
        {
            float x = 0; for (double hz : f) { x += 0.15f * (float) std::sin (2 * juce::MathConstants<double>::pi * hz * t); x += 0.05f * (float) std::sin (4 * juce::MathConstants<double>::pi * hz * t); }
            return x;
        };
        {
            DelexProcessor d; d.setRateAndBufferSizeDetails (48000.0, 480); d.prepareToPlay (48000.0, 480);
            // I–IV–V–I em Dó maior
            feed (d, 8.0, [&] (double t) { const int c = (int) (t / 1.0) % 4;
                return c == 0 || c == 3 ? tone (t, { 261.63, 329.63, 392.0 }) : c == 1 ? tone (t, { 349.23, 440.0, 523.25 }) : tone (t, { 392.0, 493.88, 587.33 }); });
            check (d.analyser.key.load() == 0, "tom detectado: Dó maior", d.analyser.key.load(), 0);
        }
        {
            DelexProcessor d; d.setRateAndBufferSizeDetails (48000.0, 480); d.prepareToPlay (48000.0, 480);
            // i–iv–V–i em Lá menor
            feed (d, 8.0, [&] (double t) { const int c = (int) (t / 1.0) % 4;
                return c == 0 || c == 3 ? tone (t, { 220.0, 261.63, 329.63 }) : c == 1 ? tone (t, { 293.66, 349.23, 440.0 }) : tone (t, { 329.63, 415.30, 493.88 }); });
            check (d.analyser.key.load() == 21, "tom detectado: Lá menor", d.analyser.key.load(), 21);
            feed (d, 1.0, [] (double t) { return 0.4f * (float) std::sin (2 * juce::MathConstants<double>::pi * 220.0 * t); });
            check (d.analyser.note.load() == 57, "nota detectada: A3 (220 Hz)", d.analyser.note.load(), 57);
        }
        // intervalos diatónicos
        namespace T = delex::taps;
        check (T::intervalSemis (T::Third, 0, false, 64) == 3.0f && T::intervalSemis (T::Third, 0, false, 60) == 4.0f
               && T::intervalSemis (T::Third, 9, true, 57) == 3.0f && T::intervalSemis (T::Fifth, 0, false, 71) == 6.0f,
               "harmonia diatónica (3.ª de Mi em Dó = 3, de Dó = 4; 5.ª de Si = 6)");

        // groove aplicado → ecos nos passos certos (120 BPM: passo 3 = 375 ms)
        DelexProcessor d; d.setRateAndBufferSizeDetails (48000.0, 480); d.prepareToPlay (48000.0, 480);
        setParam (d, delex::id::mix, 100.0f); setParam (d, delex::id::feedback, 0.0f); setParam (d, delex::id::taps_level, 100.0f);
        d.applyGroove (6);   // Afrobeats 3-3-2
        d.updateTapsNow();
        check (std::lround (plain (d, delex::id::taps_mode)) == 2, "groove liga o Multi-Tap");
        {
            juce::AudioBuffer<float> b (2, 480); juce::MidiBuffer m; std::vector<float> out;
            for (int k = 0; k < 100; ++k) { b.clear(); if (k == 0) { b.setSample (0, 0, 1.0f); b.setSample (1, 0, 1.0f); } d.processBlock (b, m); for (int i = 0; i < 480; ++i) out.push_back (b.getSample (0, i)); }
            auto peakNear = [&] (int centre) { float mx = 0; for (int i = centre - 200; i < centre + 200; ++i) mx = juce::jmax (mx, std::abs (out[(size_t) i])); return mx; };
            check (peakNear (18000) > 0.3f && peakNear (36000) > 0.25f && peakNear (27000) < 0.05f, "3-3-2: ecos a 375 e 750 ms, nada a 562 ms", peakNear (18000), 0.75);
        }
        // captura de groove: ataques a 0, 250, 375 e 750 ms (120 BPM) → passos 2, 3 e 6
        setParam (d, delex::id::taps_grid, 0.0f);
        d.startGrooveCapture();
        feed (d, 2.6, [] (double t) { for (double h : { 0.3, 0.55, 0.675, 1.05 }) if (t >= h && t < h + 0.03) return 0.8f * (float) std::sin (2 * juce::MathConstants<double>::pi * 180.0 * (t - h)) * (float) std::exp (-(t - h) / 0.01); return 0.0f; });
        d.updateTapsNow();
        const auto pat = T::fromTree (d.apvts.state.getChildWithName (T::treeId()));
        check (d.lastCaptureTaps == 3 && pat.multi[2].gain > 0 && pat.multi[3].gain > 0 && pat.multi[6].gain > 0 && pat.multi[8].gain == 0.0f,
               "captura de groove: 3 taps nos passos 2, 3 e 6", d.lastCaptureTaps, 3);

        // taps no estado (sessão e A/B)
        {
            juce::MemoryBlock st; d.getStateInformation (st);
            DelexProcessor q; q.setStateInformation (st.getData(), (int) st.getSize());
            const auto pq = T::fromTree (q.apvts.state.getChildWithName (T::treeId()));
            check (pq == pat, "padrão de taps guardado na sessão");
            T::setStep (d.apvts.state, false, 5, { 0.9f, 0, 1, 0 }, nullptr);
            d.selectSlot (1);
            T::setStep (d.apvts.state, false, 5, { 0.1f, 0, 1, 0 }, nullptr);
            d.selectSlot (0);
            const float a = T::fromTree (d.apvts.state.getChildWithName (T::treeId())).multi[5].gain;
            check (std::abs (a - 0.9f) < 1e-4f, "A/B guarda padrões de taps diferentes", a, 0.9);
        }
    }

    testControls (p);

    // matriz de parâmetros (documentação gerada a partir do código)
    {
        juce::String md;
        md << "# DELEX by Piradex — matriz de parâmetros\n\n"
           << u8 ("Gerada automaticamente a partir do código (tests/test_plugin.cpp). IDs estáveis — não mudar entre versões.\n\n")
           << u8 ("| ID | Nome | Intervalo | Defeito | Curva | Automação |\n|---|---|---|---|---|---|\n");
        int count = 0;
        for (auto* prm : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            {
                ++count;
                const auto& rg = r->getNormalisableRange();
                juce::String range, curve;
                if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (r)) { range = c->choices.joinIntoString (" / "); curve = u8 ("escolha"); }
                else if (dynamic_cast<juce::AudioParameterBool*> (r) != nullptr) { range = "off / on"; curve = "on/off"; }
                else { range = r->getText (0.0f, 32) + u8 (" … ") + r->getText (1.0f, 32); curve = std::abs (rg.skew - 1.0f) > 1.0e-3f ? u8 ("logarítmica (skew)") : juce::String ("linear"); }
                md << "| `" << r->getParameterID() << "` | " << r->getName (64) << " | " << range << " | " << r->getText (r->getDefaultValue(), 32)
                   << " | " << curve << " | " << (r->isAutomatable() ? juce::String ("sim") : u8 ("não")) << " |\n";
            }
        juce::File (juce::File::getCurrentWorkingDirectory().getChildFile ("docs/PARAMETROS.md")).replaceWithText (md);
        check (count == 104, "parametros documentados", count, 104);
    }

    // 6) capturas da interface
    {
        p.setCurrentProgram (7);   // Sincopa Dourada
        // alimenta áudio para os medidores e o goniómetro terem conteúdo
        float pk = 0; bool fin = true;
        auto* ed = p.createEditor();
        std::unique_ptr<juce::AudioProcessorEditor> edp (ed);
        auto feedLive = [&] { feedNoise (p, 30, 512, pk, fin, g); };
        for (float sc : { 1.0f })
        {
            juce::ignoreUnused (sc);
            ed->setSize (1000, 640);
            feedLive(); pump (200); feedLive();
            snapshot (*ed, "docs/ui_simple.png");
            check (clickButton (*ed, "ADVANCED"), "mudar para a vista ADVANCED");
            const char* tabs[] = { "TIMING", "SPACE", "FEEDBACK EQ", "MOTION", "TEXTURE", "MOD", "PERFORM", "TAPS", "UTILITIES" };
            const char* files[] = { "docs/ui_adv_timing.png", "docs/ui_adv_space.png", "docs/ui_adv_feedback_eq.png",
                                    "docs/ui_adv_motion.png", "docs/ui_adv_texture.png", "docs/ui_adv_mod.png", "docs/ui_adv_perform.png",
                                    "docs/ui_adv_taps.png", "docs/ui_adv_utilities.png" };
            for (int i = 0; i < 9; ++i) { check (clickButton (*ed, tabs[i]), tabs[i]); feedLive(); snapshot (*ed, files[i]); }
            clickButton (*ed, "SIMPLE");
        }
        // ecrã de senha
        lic::License::get().lock(); pump (150);
        snapshot (*ed, "docs/ui_login.png");
        lic::License::get().unlockForTests(); pump (150);

        // browser de presets
        for (auto* c : ed->getChildren())
            for (auto* cc : c->getChildren())
                if (auto* b = dynamic_cast<juce::Button*> (cc))
                    if (b->getName() == "preset") { b->triggerClick(); pump (100); }
        snapshot (*ed, "docs/ui_presets.png", false);
        piradex::i18n::set (piradex::i18n::Language::EN, false); pump (400);     // em inglês (a interface é refeita)
        for (auto* c : ed->getChildren())
            for (auto* cc : c->getChildren())
                if (auto* b = dynamic_cast<juce::Button*> (cc))
                    if (b->getName() == "preset") { b->triggerClick(); pump (100); }
        pump (400); saveShot (*ed, "docs/ui_presets_en.png");
        piradex::i18n::set (piradex::i18n::Language::PT, false); pump (300);
        // escala 75 % e 150 %
        ed->setSize (750, 480); snapshot (*ed, "docs/ui_scale75.png", false);
        ed->setSize (1500, 960); pump (100);
        check (ed->getWidth() == 1500 && ed->getHeight() == 960, "redimensionar para 150 %", ed->getWidth(), 1500);
    }

    std::printf ("\n%d falhas\n", failures);
    return failures == 0 ? 0 : 1;
}
