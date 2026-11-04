// Gera uma pré-escuta (WAV estéreo, 6 s, 48 kHz) de cada preset de fábrica, com um sinal seco
// adequado à categoria (voz sintética, bateria, baixo, guitarra, pad). Usado pela página de audição.
//   delex_render <pasta de saída>
#include "PluginProcessor.h"
#include <random>

static constexpr double SR = 48000.0, BPM = 120.0;
static constexpr double PI = 3.14159265358979;

enum Src { Voice, Drums, Bass, Guitar, Pad, Mix };

static Src sourceFor (const juce::String& cat)
{
    if (cat == "Drums" || cat == "NonLin") return Drums;
    if (cat == "Bass") return Bass;
    if (cat == "Guitar") return Guitar;
    if (cat == "Synth" || cat == "Ambience" || cat == "Ambient" || cat == "Orkestra" || cat == "Mega") return Pad;
    if (cat == "FX" || cat == "Transition" || cat == "Master FX" || cat == "Lo-Fi" || cat == "Dub") return Mix;
    return Voice;   // Vocals, Afro House, Kizomba, Trap, VIP
}

// sinal seco (mono) com 2,6 s de material e o resto em silêncio para ouvir a cauda
static std::vector<float> dry (Src s, int n)
{
    std::vector<float> x ((size_t) n, 0.0f);
    std::mt19937 g (7); std::uniform_real_distribution<float> u (-1, 1);
    const double beat = 60.0 / BPM;
    auto add = [&] (double t0, double dur, const std::function<float (double)>& f)
    {
        for (int i = (int) (t0 * SR); i < (int) ((t0 + dur) * SR) && i < n; ++i) x[(size_t) i] += f (i / SR - t0);
    };
    auto voice = [&] (double t0, double dur, double f0)
    {
        double ph = 0; float z1 = 0, z2 = 0, z3 = 0;
        add (t0, dur, [&, f0, dur] (double t) mutable
        {
            const double f = f0 * (1.0 + 0.006 * std::sin (2 * PI * 5.5 * t));
            ph += f / SR; ph -= std::floor (ph);
            const float saw = (float) (2 * ph - 1);
            // três formantes simples (a ~ "a")
            z1 += 0.10f * (saw - z1); z2 += 0.35f * (saw - z2); z3 += 0.6f * (saw - z3);
            const float v = 0.5f * z1 + 0.35f * (z2 - z1) + 0.15f * (z3 - z2);
            const double env = std::min (1.0, t / 0.03) * std::min (1.0, (dur - t) / 0.06);
            return (float) (0.55 * env) * v;
        });
    };
    switch (s)
    {
        case Voice:
            voice (0.05, 0.32, 220); voice (0.42, 0.22, 247); voice (0.68, 0.45, 262);
            voice (1.35, 0.25, 247); voice (1.65, 0.6, 220);
            break;
        case Drums:
            for (int k = 0; k < 8; ++k)
            {
                const double t = k * beat * 0.5;
                if (k > 4) break;
                if (k % 4 == 0) add (t, 0.35, [] (double tt) { return (float) (0.9 * std::sin (2 * PI * (50 + 120 * std::exp (-tt * 25)) * tt) * std::exp (-tt * 9)); });
                if (k % 4 == 2) add (t, 0.25, [&] (double tt) { return (float) ((0.5 * u (g) + 0.4 * std::sin (2 * PI * 190 * tt)) * std::exp (-tt * 18)); });
                add (t + beat * 0.25, 0.06, [&] (double tt) { return (float) (0.25 * u (g) * std::exp (-tt * 60)); });
            }
            break;
        case Bass:
            for (int k = 0; k < 3; ++k)
                add (k * beat * 1.5, beat * 1.2, [k] (double t) { const double f = (k == 1 ? 49.0 : 41.2); return (float) (0.7 * std::tanh (1.6 * std::sin (2 * PI * f * t + 3 * std::exp (-t * 30))) * std::exp (-t * 2.2)); });
            break;
        case Guitar:
        {
            const double notes[] = { 196, 247, 294, 392, 294 };
            for (int k = 0; k < 5; ++k)
            {
                const int L = (int) (SR / notes[k]);
                std::vector<float> buf ((size_t) L); for (auto& b : buf) b = u (g) * 0.5f;
                int pos = 0;
                add (k * beat * 0.5, 1.6, [buf, pos, L] (double) mutable { const float y = buf[(size_t) pos]; const int nx = (pos + 1) % L; buf[(size_t) pos] = 0.497f * (buf[(size_t) pos] + buf[(size_t) nx]); pos = nx; return y; });
            }
        } break;
        case Pad:
            for (int c = 0; c < 2; ++c)
            {
                const double ch[2][3] = { { 220, 261.6, 329.6 }, { 196, 246.9, 293.7 } };
                for (double f : ch[c])
                    add (c * 1.0, 0.9, [f] (double t) { return (float) (0.12 * (std::sin (2 * PI * f * t) + 0.4 * std::sin (4 * PI * f * t)) * std::min (1.0, t / 0.08) * std::min (1.0, (0.9 - t) / 0.2)); });
            }
            break;
        case Mix:
        {
            auto v = dry (Voice, n), d = dry (Drums, n);
            for (int i = 0; i < n; ++i) x[(size_t) i] = 0.7f * v[(size_t) i] + 0.6f * d[(size_t) i];
        } break;
    }
    return x;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory().getChildFile ("render"));
    out.createDirectory();
    lic::License::get().unlockForTests();
    const auto& fp = delex::factoryPresets();
    // catálogo para a página de audição
    {
        juce::Array<juce::var> arr;
        piradex::i18n::set (piradex::i18n::Language::PT, false);
        for (int i = 0; i < (int) fp.size(); ++i)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("i", i);
            o->setProperty ("name", juce::String::fromUTF8 (fp[(size_t) i].name));
            o->setProperty ("cat", juce::String::fromUTF8 (fp[(size_t) i].category));
            o->setProperty ("sub", juce::String::fromUTF8 (fp[(size_t) i].sub));
            o->setProperty ("desc", delex::presetDescription (fp[(size_t) i]));
            o->setProperty ("use", delex::presetUse (fp[(size_t) i]));
            o->setProperty ("data", juce::String::fromUTF8 (fp[(size_t) i].data));
            arr.add (juce::var (o));
        }
        out.getChildFile ("presets.json").replaceWithText (juce::JSON::toString (juce::var (arr), true));
        if (argc > 2 && juce::String (argv[2]) == "--json") return 0;
    }
    const int n = (int) (6.0 * SR), bs = 256;
    juce::WavAudioFormat wav;
    juce::String index;
    for (int i = 0; i < (int) fp.size(); ++i)
    {
        DelexProcessor p;
        p.setRateAndBufferSizeDetails (SR, bs); p.prepareToPlay (SR, bs);
        p.loadFactoryPreset (i);
        const auto cat = juce::String::fromUTF8 (fp[(size_t) i].category);
        const auto x = dry (sourceFor (cat), n);
        juce::AudioBuffer<float> all (2, n), b (2, bs); juce::MidiBuffer m;
        for (int pos = 0; pos < n; pos += bs)
        {
            for (int k = 0; k < bs; ++k) { const float v = pos + k < n ? x[(size_t) (pos + k)] : 0.0f; b.setSample (0, k, v); b.setSample (1, k, v); }
            p.processBlock (b, m);
            for (int c = 0; c < 2; ++c) all.copyFrom (c, pos, b, c, 0, std::min (bs, n - pos));
        }
        // normaliza ao pico −1 dBFS (as pré-escutas ficam com volume comparável)
        const float pk = std::max (all.getMagnitude (0, 0, n), all.getMagnitude (1, 0, n));
        if (pk > 1.0e-6f) all.applyGain (0.89f / pk);
        auto f = out.getChildFile (juce::String (i).paddedLeft ('0', 3) + ".wav");
        f.deleteFile();
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), SR, 2, 16, {}, 0));
        if (w != nullptr) w->writeFromAudioSampleBuffer (all, 0, n);
        index << i << "\t" << juce::String::fromUTF8 (fp[(size_t) i].name) << "\t" << cat << "\n";
        if (i % 40 == 0) std::printf ("%d/%d\n", i, (int) fp.size());
    }
    out.getChildFile ("index.tsv").replaceWithText (index);
    std::printf ("ok: %d pré-escutas em %s\n", (int) fp.size(), out.getFullPathName().toRawUTF8());
    return 0;
}
