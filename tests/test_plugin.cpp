// Teste de integração com uma instância real do plugin DELEX:
//  todos os presets a processar áudio · estado guardar/restaurar · A/B · mono/estéreo
//  · double precision · e grava capturas da interface (Simple, cada aba Advanced, presets).
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>
#include <random>

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
        check (bad == 0, "61 presets: áudio finito e limitado", worst, 6.0);
        check (p.getNumPrograms() == 61, "número de presets de fábrica", p.getNumPrograms(), 61);
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
        l.inputBuses.add (juce::AudioChannelSet::mono()); l.outputBuses.add (juce::AudioChannelSet::stereo());
        check (p.checkBusesLayoutSupported (l), "layout mono → estéreo aceite");
        juce::AudioProcessor::BusesLayout bad;
        bad.inputBuses.add (juce::AudioChannelSet::stereo()); bad.outputBuses.add (juce::AudioChannelSet::mono());
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
        check (count == 57, "parametros documentados", count, 57);
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
            const char* tabs[] = { "TIMING", "SPACE", "FEEDBACK EQ", "MOTION", "TEXTURE", "UTILITIES" };
            const char* files[] = { "docs/ui_adv_timing.png", "docs/ui_adv_space.png", "docs/ui_adv_feedback_eq.png",
                                    "docs/ui_adv_motion.png", "docs/ui_adv_texture.png", "docs/ui_adv_utilities.png" };
            for (int i = 0; i < 6; ++i) { check (clickButton (*ed, tabs[i]), tabs[i]); feedLive(); snapshot (*ed, files[i]); }
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
