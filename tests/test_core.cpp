// Testes numéricos do núcleo DSP do DELEX (sem JUCE)
//   g++ -O2 -std=c++17 -ISource tests/test_core.cpp -o test_core && ./test_core
#include "DelexCore.h"
#include <chrono>
#include <cstdio>
#include <random>

using namespace delex;

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (! (cond)) { ++failures; std::printf ("  FALHOU: " __VA_ARGS__); std::printf ("\n"); } } while (0)

static constexpr double SR = 48000.0;

struct Run
{
    Engine e;
    std::vector<float> L, R;
    explicit Run (const Params& p, double sr = SR) { e.prepare (sr, 512); e.setParams (p); }
    // processa n amostras com gerador de entrada
    template <typename Gen> void go (int n, Gen gen, int block = 256)
    {
        std::vector<float> l ((size_t) block), r ((size_t) block);
        for (int pos = 0; pos < n; pos += block)
        {
            const int b = std::min (block, n - pos);
            for (int i = 0; i < b; ++i) gen (pos + i, l[(size_t) i], r[(size_t) i]);
            e.process (l.data(), r.data(), b);
            L.insert (L.end(), l.begin(), l.begin() + b);
            R.insert (R.end(), r.begin(), r.begin() + b);
        }
    }
};

static Params wetOnly()
{
    Params p;
    p.mix = 1.0f; p.equalPower = true;   // 100 % wet
    p.feedback = 0.0f;
    return p;
}

static int argmaxAbs (const std::vector<float>& v, int from, int to)
{
    int best = from; float m = -1;
    for (int i = from; i < to && i < (int) v.size(); ++i) if (std::fabs (v[(size_t) i]) > m) { m = std::fabs (v[(size_t) i]); best = i; }
    return best;
}
static float peakAbs (const std::vector<float>& v, int from = 0, int to = -1)
{
    if (to < 0) to = (int) v.size();
    float m = 0; for (int i = from; i < to; ++i) m = std::max (m, std::fabs (v[(size_t) i])); return m;
}
static double energy (const std::vector<float>& v, int from, int to)
{
    double s = 0; for (int i = from; i < to; ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return s;
}
static bool allFinite (const std::vector<float>& v) { for (float x : v) if (! std::isfinite (x)) return false; return true; }

int main()
{
    std::printf ("DELEX — testes do núcleo DSP\n");

    // 1) Matriz espacial ortogonal (rotação e reflexão preservam energia)
    {
        double worst = 0;
        for (int deg = -180; deg <= 180; deg += 7)
            for (int refl = 0; refl < 2; ++refl)
            {
                const Mat2 m = spatialMatrix ((float) deg, refl != 0);
                worst = std::max (worst, (double) std::fabs (m.a * m.a + m.c * m.c - 1.0f));
                worst = std::max (worst, (double) std::fabs (m.b * m.b + m.d * m.d - 1.0f));
                worst = std::max (worst, (double) std::fabs (m.a * m.b + m.c * m.d));
            }
        CHECK (worst < 1e-5, "matriz não ortogonal (erro %g)", worst);
        std::printf ("[ok] matriz unitária: erro máx %.2e\n", worst);
    }

    // 2) Pré/de-ênfase da fita são exactamente inversas (drive 0 ⇒ transparente)
    {
        Params p = wetOnly(); p.timeMsL = p.timeMsR = 10; p.tapeWarmth = 1.0f; p.tapeDrive = 0.0f;
        p.lowCutHz = 10; p.highCutHz = 22000; p.cutSlope = CutSlope::dB6;
        Params q = p; q.tapeWarmth = 0.0f;
        Run a (p), b (q);
        auto imp = [] (int i, float& l, float& r) { l = r = (i == 100) ? 1.0f : 0.0f; };
        a.go (4800, imp); b.go (4800, imp);
        double diff = 0; for (size_t i = 0; i < a.L.size(); ++i) diff = std::max (diff, (double) std::fabs (a.L[i] - b.L[i]));
        CHECK (diff < 2e-3, "ênfase/de-ênfase não se anulam (dif %g)", diff);
        std::printf ("[ok] warmth sem drive é transparente: dif máx %.2e\n", diff);
    }

    // 3) Impulso: primeiro eco no tempo certo (modo Dual, matriz identidade)
    {
        Params p = wetOnly(); p.timeMsL = 100; p.timeMsR = 250; p.lowCutHz = 10; p.highCutHz = 22000;
        Run r (p);
        r.go (20000, [] (int i, float& l, float& rr) { l = (i == 0) ? 1.0f : 0.0f; rr = (i == 0) ? 1.0f : 0.0f; });
        const int iL = argmaxAbs (r.L, 10, 19000), iR = argmaxAbs (r.R, 10, 19000);
        CHECK (std::abs (iL - 4800) <= 2, "eco L em %d (esperado 4800)", iL);
        CHECK (std::abs (iR - 12000) <= 2, "eco R em %d (esperado 12000)", iR);
        CHECK (std::fabs (r.L[(size_t) iL]) > 0.7f, "nível do eco L %f"   /* o corte de agudos a 21.6 kHz espalha o impulso */, r.L[(size_t) iL]);
        std::printf ("[ok] tempos: eco L @%d (100 ms), eco R @%d (250 ms), nível %.3f\n", iL, iR, r.L[(size_t) iL]);
    }

    // 4) Ping-pong: 1.º eco à esquerda, 2.º à direita
    {
        Params p = wetOnly(); p.routing = Routing::PingPong; p.feedback = 0.7f; p.timeMsL = p.timeMsR = 50;
        p.lowCutHz = 10; p.highCutHz = 22000;
        Run r (p);
        r.go (12000, [] (int i, float& l, float& rr) { l = rr = (i == 0) ? 1.0f : 0.0f; });
        const float e1L = peakAbs (r.L, 2300, 2500), e1R = peakAbs (r.R, 2300, 2500);
        const float e2L = peakAbs (r.L, 4700, 4900), e2R = peakAbs (r.R, 4700, 4900);
        CHECK (e1L > 0.5f && e1R < 0.01f, "1.º eco devia ser só L (L=%f R=%f)", e1L, e1R);
        CHECK (e2R > 0.3f && e2L < 0.01f, "2.º eco devia ser só R (L=%f R=%f)", e2L, e2R);
        std::printf ("[ok] ping-pong: eco1 L=%.3f R=%.3f · eco2 L=%.3f R=%.3f\n", e1L, e1R, e2L, e2R);
    }

    // 5) Pior caso: feedback 100 %, pico +18 dB sem compensação, drive e grit no máximo
    {
        Params p = wetOnly(); p.feedback = 1.0f; p.peakDb = 18; p.peakQ = 4; p.compensate = 0; p.peakHz = 900;
        p.tapeDrive = 1; p.tapeWarmth = 1; p.gritBits = 4; p.gritRateHz = 4000; p.diffAmount = 1; p.dispSpreadMs = 30;
        p.timeMsL = 30; p.timeMsR = 47; p.rotDeg = 37; p.modDepthCents = 40;
        Run r (p);
        std::mt19937 g (1); std::uniform_real_distribution<float> u (-1, 1);
        r.go ((int) SR * 20, [&] (int i, float& l, float& rr) { const bool burst = (i % 48000) < 4800; l = burst ? u (g) : 0; rr = burst ? u (g) : 0; });
        const float pk = std::max (peakAbs (r.L), peakAbs (r.R));
        CHECK (allFinite (r.L) && allFinite (r.R), "saída não finita");
        CHECK (pk < 4.5f, "pico %f acima do limite de segurança", pk);
        CHECK (r.e.resets() == 0, "reinícios de emergência: %d", r.e.resets());
        std::printf ("[ok] estabilidade com feedback 100 %% + pico +18 dB: pico máx %.2f, sem NaN\n", pk);
    }

    // 6) Freeze: a cauda mantém-se (sem perdas) com a entrada em silêncio
    {
        Params p = wetOnly(); p.feedback = 0.5f; p.timeMsL = 300; p.timeMsR = 410; p.rotDeg = 30;
        Run r (p);
        std::mt19937 g (2); std::uniform_real_distribution<float> u (-0.5f, 0.5f);
        r.go ((int) SR, [&] (int, float& l, float& rr) { l = u (g); rr = u (g); });
        p.freeze = true; r.e.setParams (p);
        r.go ((int) SR * 10, [] (int, float& l, float& rr) { l = rr = 0; });
        const int s = (int) SR;
        const double e2 = energy (r.L, s * 2, s * 3) + energy (r.R, s * 2, s * 3);
        const double e10 = energy (r.L, s * 10, s * 11) + energy (r.R, s * 10, s * 11);
        CHECK (e2 > 1.0 && e10 / e2 > 0.6, "freeze perde energia (%g → %g)", e2, e10);
        std::printf ("[ok] freeze: energia s2=%.1f · s10=%.1f (razão %.2f)\n", e2, e10, e10 / e2);
        // ao sair do freeze com feedback 50 % a cauda decai
        p.freeze = false; r.e.setParams (p);
        r.go ((int) SR * 6, [] (int, float& l, float& rr) { l = rr = 0; });
        const double eEnd = energy (r.L, s * 16, s * 17);
        CHECK (eEnd < e10 * 0.01, "após freeze não decai (%g)", eEnd);
    }

    // 7) Entrada NaN/Inf: recupera sem propagar
    {
        Params p = wetOnly(); p.feedback = 0.8f; p.timeMsL = p.timeMsR = 20;
        Run r (p);
        r.go (48000, [] (int i, float& l, float& rr) { l = (i == 1000) ? NAN : 0.1f * std::sin (0.01f * i); rr = (i == 2000) ? INFINITY : l; });
        size_t firstBad = 0; bool bad = false;
        for (size_t i = 3000; i < r.L.size(); ++i) if (! std::isfinite (r.L[i]) || ! std::isfinite (r.R[i])) { bad = true; firstBad = i; break; }
        CHECK (! bad, "NaN propagou-se (amostra %zu)", firstBad);
        std::printf ("[ok] NaN/Inf na entrada: recuperação (%d reinícios de segurança)\n", r.e.resets());
    }

    // 8) Ducker: entrada alta e contínua ⇒ redução ≈ amount
    {
        Params p = wetOnly(); p.duckOn = true; p.duckThreshDb = -30; p.duckAmountDb = 12; p.feedback = 0.3f;
        Run r (p);
        r.go ((int) SR, [] (int i, float& l, float& rr) { l = rr = 0.5f * std::sin (0.05f * i); });
        CHECK (std::fabs (r.e.duckReductionDb() - 12.0f) < 0.5f, "redução do ducker %f dB", r.e.duckReductionDb());
        std::printf ("[ok] ducker: redução %.2f dB (pedido 12)\n", r.e.duckReductionDb());
    }

    // 9) Mudanças de tempo sem cliques (glide de fita e crossfade)
    for (int mode = 0; mode < 2; ++mode)
    {
        Params p = wetOnly(); p.feedback = 0.4f; p.timeMsL = p.timeMsR = 200;
        p.transition = mode == 0 ? Transition::TapeGlide : Transition::Crossfade;
        Run r (p);
        auto sine = [] (int i, float& l, float& rr) { l = rr = 0.5f * std::sin (kTwoPi * 220.0f * (float) i / (float) SR); };
        r.go ((int) SR, sine);
        // mede o maior salto entre amostras numa janela estável
        auto maxStep = [&] (int from, int to) { float m = 0; for (int i = from + 1; i < to; ++i) m = std::max (m, std::fabs (r.L[(size_t) i] - r.L[(size_t) i - 1])); return m; };
        const float base = maxStep ((int) (SR * 0.6), (int) SR);
        p.timeMsL = p.timeMsR = 130; r.e.setParams (p);
        r.go ((int) SR, sine);
        const float during = maxStep ((int) SR, (int) (SR * 2));
        CHECK (during < base * 2.2f, "%s: salto %f vs base %f", mode == 0 ? "glide" : "crossfade", during, base);
        std::printf ("[ok] mudança de tempo (%s): salto máx %.4f (base %.4f)\n", mode == 0 ? "fita" : "crossfade", during, base);
    }

    // 10) Bypass e mix 0 devolvem o sinal seco
    {
        Params p; p.mix = 0.0f; p.feedback = 0.6f;
        Run r (p);
        r.go (24000, [] (int i, float& l, float& rr) { l = 0.3f * std::sin (0.02f * i); rr = -l; });
        double err = 0; for (int i = 12000; i < 24000; ++i) err = std::max (err, (double) std::fabs (r.L[(size_t) i] - 0.3f * std::sin (0.02f * i)));
        CHECK (err < 1e-5, "mix 0 não é seco (erro %g)", err);
        Params b; b.mix = 0.8f; b.bypass = true; b.feedback = 0.7f;
        Run rb (b);
        rb.go (24000, [] (int i, float& l, float& rr) { l = 0.3f * std::sin (0.02f * i); rr = l; });
        double eb = 0; for (int i = 12000; i < 24000; ++i) eb = std::max (eb, (double) std::fabs (rb.L[(size_t) i] - 0.3f * std::sin (0.02f * i)));
        CHECK (eb < 1e-5, "bypass não é seco (erro %g)", eb);
        std::printf ("[ok] mix 0 e bypass: erro máx %.1e / %.1e\n", err, eb);
    }

    // 11) Silêncio prolongado ⇒ zeros exactos (sem denormais)
    {
        Params p = wetOnly(); p.feedback = 0.9f; p.diffAmount = 0.5f; p.dispSpreadMs = 10;
        Run r (p);
        r.go ((int) SR * 60, [] (int i, float& l, float& rr) { l = rr = (i < 100) ? 0.5f : 0.0f; });
        const float tail = peakAbs (r.L, (int) SR * 59, (int) SR * 60);
        CHECK (tail < 1e-6f, "cauda não chega a silêncio (%g)", tail);
        std::printf ("[ok] silêncio: cauda após 60 s = %.1e\n", tail);
    }

    // 12) Fuzz: parâmetros aleatórios em cada bloco, tamanhos de bloco variáveis, várias taxas
    {
        std::mt19937 g (7); std::uniform_real_distribution<float> u (0, 1);
        int bad = 0; float worst = 0;
        for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            Engine e; e.prepare (sr, 4096);
            std::vector<float> l (4096), r (4096);
            for (int blk = 0; blk < 600; ++blk)
            {
                Params p;
                p.timeMsL = 1 + 7999 * u (g) * u (g); p.timeMsR = 1 + 7999 * u (g) * u (g);
                p.feedback = u (g); p.fbBalance = u (g) * 2 - 1;
                p.routing = (Routing) (int) (u (g) * 2.99f); p.transition = (Transition) (int) (u (g) * 1.99f);
                p.freeze = u (g) > 0.9f; p.modDepthCents = 100 * u (g); p.modRateHz = 0.01f + 19 * u (g);
                p.modShape = (ModShape) (int) (u (g) * 2.99f); p.wow = u (g);
                p.rotDeg = 360 * u (g) - 180; p.reflection = u (g) > 0.5f; p.inRotDeg = 90 * u (g) - 45; p.outRotDeg = 90 * u (g) - 45;
                p.inWidth = 2 * u (g); p.outWidth = 2 * u (g); p.monoSafe = u (g) > 0.5f;
                p.lowCutHz = 20 + 1980 * u (g); p.highCutHz = 1000 + 19000 * u (g); p.cutSlope = (CutSlope) (int) (u (g) * 2.99f);
                p.peakHz = 100 + 9900 * u (g); p.peakDb = 36 * u (g) - 18; p.peakQ = 0.3f + 9.7f * u (g); p.compensate = u (g);
                p.dispHz = 50 + 4950 * u (g); p.dispSpreadMs = 50 * u (g); p.diffAmount = u (g); p.diffSpreadMs = 1 + 49 * u (g);
                p.tapeWarmth = u (g); p.tapeDrive = u (g); p.gritBits = 1 + 23 * u (g); p.gritRateHz = 1000 + 47000 * u (g);
                p.gritJitter = u (g); p.gritMix = u (g); p.loopOrder = (int) (u (g) * 3.99f);
                p.preDriveDb = 24 * u (g) - 12; p.wetTrimDb = 36 * u (g) - 24; p.outTrimDb = 0;
                p.duckOn = u (g) > 0.5f; p.mix = u (g); p.equalPower = u (g) > 0.5f; p.bypass = u (g) > 0.95f;
                e.setParams (p);
                const int n = 1 + (int) (u (g) * 4095);
                for (int i = 0; i < n; ++i) { l[(size_t) i] = 0.8f * (u (g) * 2 - 1); r[(size_t) i] = 0.8f * (u (g) * 2 - 1); }
                e.process (l.data(), r.data(), n);
                for (int i = 0; i < n; ++i)
                {
                    if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) ++bad;
                    else worst = std::max (worst, std::max (std::fabs (l[(size_t) i]), std::fabs (r[(size_t) i])));
                }
            }
        }
        CHECK (bad == 0, "fuzz: %d amostras não finitas", bad);
        CHECK (worst < 40.0f, "fuzz: pico %f", worst);   // inclui +12 dB de pré-drive e +12 dB de wet trim
        std::printf ("[ok] fuzz (4 taxas × 600 blocos): pico máx %.2f, sem NaN\n", worst);
    }

    // 13) Mapa de ecos coerente com o motor (ping-pong alterna lados)
    {
        Params p; p.routing = Routing::PingPong; p.feedback = 0.6f; p.timeMsL = p.timeMsR = 300;
        Engine::Echo ec[16];
        const int n = Engine::echoMap (p, ec, 16);
        CHECK (n > 4 && std::fabs (ec[0].l) > std::fabs (ec[0].r) && std::fabs (ec[1].r) > std::fabs (ec[1].l), "mapa de ecos incoerente");
        std::printf ("[ok] mapa de ecos: %d ecos, 1.º à esquerda, 2.º à direita\n", n);
    }

    // 14) Custo de CPU com todos os módulos activos
    {
        Params p; p.feedback = 0.7f; p.modDepthCents = 20; p.wow = 0.5f; p.diffAmount = 0.6f; p.dispSpreadMs = 15;
        p.tapeDrive = 0.5f; p.tapeWarmth = 0.5f; p.gritBits = 10; p.gritRateHz = 16000; p.peakDb = 6; p.cutSlope = CutSlope::dB24;
        p.duckOn = true; p.rotDeg = 40; p.transition = Transition::Crossfade; p.mix = 0.3f;
        Engine e; e.prepare (48000, 256); e.setParams (p);
        std::vector<float> l (256), r (256);
        std::mt19937 g (3); std::uniform_real_distribution<float> u (-0.5f, 0.5f);
        const int blocks = 48000 * 30 / 256;
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b) { for (int i = 0; i < 256; ++i) { l[(size_t) i] = u (g); r[(size_t) i] = u (g); } e.process (l.data(), r.data(), 256); }
        const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("[i]  CPU: 30 s de áudio estéreo 48 kHz em %.3f s → %.1f %% de um núcleo\n", s, 100.0 * s / 30.0);
    }

    std::printf ("\n%d verificações, %d falhas\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
