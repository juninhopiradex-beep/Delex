// Medições objectivas do DELEX (metodologia do ANALYSER by Piradex):
//   impulso → resposta de cada repetição (nível, frequência, fase), seno → THD e harmónicos,
//   seno agudo → aliasing, varrimento de nível → curva do ducker, silêncio → ruído,
//   precisão do tempo, do pitch e custo de CPU.
//   g++ -O2 -std=c++17 -ISource tests/measure.cpp -o measure && ./measure docs/medicoes/data
#include "DelexCore.h"
#include <chrono>
#include <complex>
#include <cstdio>
#include <fstream>
#include <functional>
#include <random>
#include <string>
#include <vector>

using namespace delex;
static constexpr double SR = 48000.0;
static std::string outDir = "docs/medicoes/data";

static Params wet()
{
    Params p; p.mix = 1.0f; p.equalPower = true; p.feedback = 0.0f; p.tapeWarmth = 0.0f;
    p.lowCutHz = 20.0f; p.highCutHz = 20000.0f;
    return p;
}

static std::vector<float> run (Params p, int n, const std::function<float (int)>& in, std::vector<float>* right = nullptr, const TapSet* taps = nullptr)
{
    Engine e; e.prepare (SR, 256); e.setParams (p);
    if (taps != nullptr) e.setTaps (*taps);
    std::vector<float> L ((size_t) n), R ((size_t) n);
    for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = in (i);
    for (int pos = 0; pos < n; pos += 256) e.process (L.data() + pos, R.data() + pos, std::min (256, n - pos));
    if (right != nullptr) *right = R;
    return L;
}

// DFT de uma janela (zero-padding até N, potência de 2)
static std::vector<std::complex<double>> fft (std::vector<std::complex<double>> a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2 * 3.14159265358979 / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len) { std::complex<double> w (1); for (size_t k = 0; k < len / 2; ++k) { auto u = a[i + k], v = a[i + k + len / 2] * w; a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl; } }
    }
    return a;
}

static std::vector<double> magDb (const std::vector<float>& x, int from, int len, int N = 1 << 15, bool hann = false)
{
    std::vector<std::complex<double>> a ((size_t) N);
    for (int i = 0; i < len && from + i < (int) x.size(); ++i)
    {
        const double w = hann ? 0.5 - 0.5 * std::cos (2 * 3.14159265358979 * i / (len - 1)) : 1.0;
        a[(size_t) i] = x[(size_t) (from + i)] * w;
    }
    const auto X = fft (a);
    std::vector<double> m ((size_t) N / 2);
    for (int k = 0; k < N / 2; ++k) m[(size_t) k] = 20 * std::log10 (std::abs (X[(size_t) k]) + 1e-12);
    return m;
}

static double goertzel (const std::vector<float>& x, int from, int to, double f)
{
    double s1 = 0, s2 = 0; const double c = 2 * std::cos (2 * 3.14159265358979 * f / SR);
    for (int i = from; i < to; ++i) { const double s0 = x[(size_t) i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) * 2.0 / (to - from);
}

static std::ofstream csv (const std::string& name) { std::ofstream f (outDir + "/" + name); f.precision (6); return f; }

int main (int argc, char** argv)
{
    if (argc > 1) outDir = argv[1];
    std::ofstream summary (outDir + "/resumo.txt");
    auto say = [&] (const std::string& k, double v, const char* unit)
    {
        summary << k << "=" << v << "=" << unit << "\n";
        std::printf ("  %-48s %10.3f %s\n", k.c_str(), v, unit);
    };

    // 1) Cauda de ecos por modelo (feedback 50 %, 250 ms): nível de cada repetição
    {
        auto f = csv ("cauda_modelos.csv");
        f << "repeticao,Fita,BBD,Multi-cabeca,Cassete,Digital 80s\n";
        const Model models[5] = { Model::Tape, Model::BBD, Model::MultiHead, Model::Cassette, Model::Digital80 };
        std::vector<std::vector<double>> lv (5);
        for (int m = 0; m < 5; ++m)
        {
            Params p = wet(); p.timeMsL = p.timeMsR = 250; p.feedback = 0.5f; p.model = models[m]; p.headsMask = 4;
            const int T = 12000;
            // ruído rosa curto (10 ms) como sinal de teste: mede energia por repetição
            std::mt19937 g (1); std::uniform_real_distribution<float> u (-1, 1);
            const auto y = run (p, T * 13, [&] (int i) { return i < 480 ? 0.5f * u (g) : 0.0f; });
            double e0 = 0; for (int i = T - 200; i < T + 1200; ++i) e0 += (double) y[(size_t) i] * y[(size_t) i];
            for (int k = 1; k <= 12; ++k)
            {
                double e = 0; for (int i = k * T - 200; i < k * T + 1200; ++i) e += (double) y[(size_t) i] * y[(size_t) i];
                lv[(size_t) m].push_back (10 * std::log10 (e / e0 + 1e-15));
            }
        }
        for (int k = 0; k < 12; ++k) { f << (k + 1); for (int m = 0; m < 5; ++m) f << "," << lv[(size_t) m][(size_t) k]; f << "\n"; }
        say ("Fita: queda por repetição (feedback 50 %)", (lv[0][11] - lv[0][0]) / 11.0, "dB");
    }

    // 2) Resposta em frequência da 1.ª, 3.ª e 6.ª repetição (EQ do loop por defeito do plugin)
    {
        Params p = wet(); p.timeMsL = p.timeMsR = 200; p.feedback = 0.7f; p.lowCutHz = 60; p.highCutHz = 14000; p.tapeWarmth = 0.2f;
        const int T = 9600;
        const auto y = run (p, T * 7, [] (int i) { return i == 0 ? 1.0f : 0.0f; });
        auto f = csv ("resposta_repeticoes.csv");
        f << "hz,rep1,rep3,rep6\n";
        const int N = 1 << 14;
        const auto m1 = magDb (y, T - 2000, 6000, N), m3 = magDb (y, 3 * T - 2000, 6000, N), m6 = magDb (y, 6 * T - 2000, 6000, N);
        for (int k = 1; k < N / 2; ++k)
        {
            const double hz = k * SR / N;
            if (hz < 15 || hz > 22000) continue;
            if (k % 2 != 0 && hz > 1000) continue;
            f << hz << "," << m1[(size_t) k] << "," << m3[(size_t) k] << "," << m6[(size_t) k] << "\n";
        }
        auto at = [&] (const std::vector<double>& m, double hz) { return m[(size_t) std::lround (hz * N / SR)]; };
        say ("1.ª repetição: 60 Hz em relação a 1 kHz", at (m1, 60) - at (m1, 1000), "dB");
        say ("1.ª repetição: 14 kHz em relação a 1 kHz", at (m1, 14000) - at (m1, 1000), "dB");
        say ("6.ª repetição: 10 kHz em relação a 1 kHz", at (m6, 10000) - at (m6, 1000), "dB");
    }

    // 3) Resposta da 1.ª repetição por modelo
    {
        auto f = csv ("resposta_modelos.csv");
        f << "hz,Fita,BBD,Multi-cabeca,Cassete,Digital 80s\n";
        const Model models[5] = { Model::Tape, Model::BBD, Model::MultiHead, Model::Cassette, Model::Digital80 };
        const int N = 1 << 14, T = 24000;
        std::vector<std::vector<double>> m (5);
        for (int k = 0; k < 5; ++k)
        {
            Params p = wet(); p.timeMsL = p.timeMsR = 500; p.model = models[k]; p.headsMask = 4;
            const auto y = run (p, T + 9000, [] (int i) { return i == 0 ? 0.25f : 0.0f; });
            m[(size_t) k] = magDb (y, T - 3000, 9000, N);
        }
        for (int k = 1; k < N / 2; ++k)
        {
            const double hz = k * SR / N;
            if (hz < 20 || hz > 22000 || (k % 2 != 0 && hz > 1000)) continue;
            f << hz; for (int j = 0; j < 5; ++j) f << "," << m[(size_t) j][(size_t) k] - m[(size_t) j][(size_t) std::lround (1000.0 * N / SR)]; f << "\n";
        }
    }

    // 4) THD da saturação de fita (1 kHz, −6 dBFS) em função do DRIVE, normal e HQ 2x
    {
        auto f = csv ("thd_drive.csv");
        f << "drive,thd_normal,thd_hq\n";
        for (int d = 0; d <= 100; d += 10)
        {
            double thd[2];
            for (int hq = 0; hq < 2; ++hq)
            {
                Params p = wet(); p.timeMsL = p.timeMsR = 50; p.tapeDrive = d / 100.0f; p.satHQ = hq == 1;
                const auto y = run (p, 48000, [] (int i) { return 0.5f * (float) std::sin (2 * 3.14159265358979 * 1000.0 * i / SR); });
                const double h1 = goertzel (y, 24000, 48000, 1000.0);
                double hs = 0; for (int h = 2; h <= 9; ++h) { const double v = goertzel (y, 24000, 48000, 1000.0 * h); hs += v * v; }
                thd[hq] = 100.0 * std::sqrt (hs) / (h1 + 1e-12);
            }
            f << d << "," << thd[0] << "," << thd[1] << "\n";
            if (d == 60) say ("THD a DRIVE 60 % (1 kHz, −6 dBFS)", thd[0], "%");
        }
        // espectro harmónico a DRIVE 60 %
        Params p = wet(); p.timeMsL = p.timeMsR = 50; p.tapeDrive = 0.6f;
        const auto y = run (p, 48000, [] (int i) { return 0.5f * (float) std::sin (2 * 3.14159265358979 * 1000.0 * i / SR); });
        auto h = csv ("harmonicos.csv"); h << "harmonico,db\n";
        const double h1 = goertzel (y, 24000, 48000, 1000.0);
        for (int k = 1; k <= 9; ++k) h << k << "," << 20 * std::log10 (goertzel (y, 24000, 48000, 1000.0 * k) / h1 + 1e-12) << "\n";
    }

    // 5) Aliasing: 9,1 kHz com DRIVE 80 % — espectro normal vs HQ 2x
    {
        auto f = csv ("aliasing.csv"); f << "hz,normal,hq\n";
        std::vector<double> m[2];
        for (int hq = 0; hq < 2; ++hq)
        {
            Params p = wet(); p.timeMsL = p.timeMsR = 50; p.tapeDrive = 0.8f; p.satHQ = hq == 1;
            const auto y = run (p, 1 << 16, [] (int i) { return 0.5f * (float) std::sin (2 * 3.14159265358979 * 9100.0 * i / SR); });
            m[hq] = magDb (y, 16384, 32768, 32768, true);
        }
        const double ref = *std::max_element (m[0].begin(), m[0].end());
        for (int k = 1; k < 16384; k += 4) f << k * SR / 32768 << "," << m[0][(size_t) k] - ref << "," << m[1][(size_t) k] - ref << "\n";
        auto band = [&] (int hq, double hz) { const int k = (int) std::lround (hz * 32768 / SR); double mx = -300; for (int j = k - 3; j <= k + 3; ++j) mx = std::max (mx, m[hq][(size_t) j]); return mx - ref; };
        say ("Alias a 2,5 kHz (normal)", band (0, 2500), "dB");
        say ("Alias a 2,5 kHz (HQ 2x)", band (1, 2500), "dB");
    }

    // 6) Curva estática do ducker (threshold −26 dB, amount 9 dB)
    {
        auto f = csv ("ducker.csv"); f << "entrada_db,reducao_db\n";
        for (int dB = -60; dB <= 0; dB += 2)
        {
            Params p = wet(); p.duckOn = true; p.duckThreshDb = -26; p.duckAmountDb = 9; p.timeMsL = p.timeMsR = 100;
            Engine e; e.prepare (SR, 256); e.setParams (p);
            std::vector<float> l (256), r (256);
            const float a = std::pow (10.0f, dB / 20.0f);
            for (int b = 0; b < 200; ++b) { for (int i = 0; i < 256; ++i) l[(size_t) i] = r[(size_t) i] = a * (float) std::sin (0.13 * (b * 256 + i)); e.process (l.data(), r.data(), 256); }
            f << dB << "," << e.duckReductionDb() << "\n";
        }
    }

    // 7) Ruído de fundo (silêncio à entrada) e hiss no máximo
    {
        Params p = wet(); p.feedback = 0.9f; p.diffAmount = 0.5f; p.tapeDrive = 0.5f;
        const auto y = run (p, 96000, [] (int) { return 0.0f; });
        double e = 0; for (int i = 48000; i < 96000; ++i) e += (double) y[(size_t) i] * y[(size_t) i];
        say ("Ruído de fundo (silêncio, feedback 90 %)", 10 * std::log10 (e / 48000 + 1e-30), "dBFS");
        p.hiss = 1.0f;
        const auto z = run (p, 96000, [] (int) { return 0.0f; });
        e = 0; for (int i = 48000; i < 96000; ++i) e += (double) z[(size_t) i] * z[(size_t) i];
        say ("HISS no máximo", 10 * std::log10 (e / 48000 + 1e-30), "dBFS RMS");
    }

    // 8) Precisão do tempo (1 ms … 8 s): erro do pico do 1.º eco
    {
        auto f = csv ("precisao_tempo.csv"); f << "ms,erro_amostras\n";
        double worst = 0;
        for (double ms : { 1.0, 2.5, 10.0, 47.3, 125.0, 333.3, 500.0, 1000.0, 2000.0, 4000.0, 7999.0 })
        {
            Params p = wet(); p.timeMsL = p.timeMsR = (float) ms;
            const int want = (int) std::lround (ms * SR / 1000.0);
            const auto y = run (p, want + 2000, [] (int i) { return i == 0 ? 1.0f : 0.0f; });
            int best = 0; float mv = 0;
            for (int i = std::max (1, want - 300); i < want + 300 && i < (int) y.size(); ++i) if (std::fabs (y[(size_t) i]) > mv) { mv = std::fabs (y[(size_t) i]); best = i; }
            const int err = ms < 4.0 ? best - std::max (4, want) : best - want;
            f << ms << "," << err << "\n";
            worst = std::max (worst, (double) std::abs (err));
        }
        say ("Erro máximo do tempo do eco (1 ms … 8 s)", worst, "amostras");
    }

    // 9) Pitch no loop: +12, +7, −12 semitons
    {
        auto f = csv ("pitch.csv"); f << "semitons,esperado_hz,medido_hz\n";
        for (float st : { -12.0f, -5.0f, 7.0f, 12.0f })
        {
            Params p = wet(); p.timeMsL = p.timeMsR = 200; p.pitchSemi = st; p.pitchMix = 1;
            const auto y = run (p, 48000, [] (int i) { return 0.4f * (float) std::sin (2 * 3.14159265358979 * 440.0 * i / SR); });
            int zc = 0; for (int i = 24001; i < 48000; ++i) if ((y[(size_t) i - 1] < 0) != (y[(size_t) i] < 0)) ++zc;
            const double hz = 0.5 * zc * SR / 24000, want = 440.0 * std::pow (2.0, st / 12.0);
            f << st << "," << want << "," << hz << "\n";
        }
    }

    // 10) Mono: ecos em contrafase (L = −R) — energia da soma mono, sem e com Translation Guard
    {
        for (int gd = 0; gd < 2; ++gd)
        {
            Params p = wet(); p.timeMsL = p.timeMsR = 120; p.feedback = 0.4f; p.guard = gd == 1;
            Engine e; e.prepare (SR, 256); e.setParams (p);
            std::vector<float> l (256), r (256);
            double st = 0, mo = 0;
            for (int b = 0; b < 375; ++b)
            {
                for (int i = 0; i < 256; ++i) { const float x = 0.3f * (float) std::sin (0.031 * (b * 256 + i)); l[(size_t) i] = x; r[(size_t) i] = -x; }
                e.process (l.data(), r.data(), 256);
                if (b > 280) for (int i = 0; i < 256; ++i) { st += 0.5 * (l[(size_t) i] * l[(size_t) i] + r[(size_t) i] * r[(size_t) i]); const double m = 0.5 * (l[(size_t) i] + r[(size_t) i]); mo += m * m; }
            }
            say (gd ? "Soma mono dos ecos em contrafase, com GUARD" : "Soma mono dos ecos em contrafase, sem GUARD", 10 * std::log10 (mo / st + 1e-15), "dB");
        }
    }

    // 11) CPU por função (30 s estéreo a 48 kHz, um núcleo)
    {
        auto f = csv ("cpu.csv"); f << "funcao,percent\n";
        auto cost = [&] (const char* name, Params p, const TapSet* t = nullptr)
        {
            Engine e; e.prepare (SR, 256); e.setParams (p); if (t) e.setTaps (*t);
            std::vector<float> l (256), r (256); std::mt19937 g (3); std::uniform_real_distribution<float> u (-0.5f, 0.5f);
            const int blocks = (int) (SR * 20 / 256);
            const auto t0 = std::chrono::steady_clock::now();
            for (int b = 0; b < blocks; ++b) { for (int i = 0; i < 256; ++i) { l[(size_t) i] = u (g); r[(size_t) i] = u (g); } e.process (l.data(), r.data(), 256); }
            const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            f << name << "," << 100.0 * s / 20.0 << "\n";
            return 100.0 * s / 20.0;
        };
        Params base; base.feedback = 0.5f; base.mix = 0.3f;
        const double b0 = cost ("Base (delay + matriz + EQ)", base);
        Params p = base; p.tapeDrive = 0.5f; cost ("Saturação de fita", p);
        p.satHQ = true; cost ("Saturação HQ 2x", p);
        p = base; p.diffAmount = 0.6f; p.dispSpreadMs = 15; cost ("Difusão + dispersão", p);
        p = base; p.pitchMix = 0.5f; p.pitchSemi = 12; cost ("Pitch / shimmer", p);
        p = base; p.reverse = 0.6f; cost ("Reverse", p);
        p = base; p.unmask = 0.6f; cost ("Spectral Unmask", p);
        p = base; p.guard = true; cost ("Translation Guard", p);
        TapSet t; t.count = 16; for (int k = 0; k < 16; ++k) t.taps[(size_t) k] = { (float) (k + 1) * 0.25f, 0.5f, 0, 0.8f, k % 4 == 0 ? 7.0f : 0.0f };
        p = base; p.tapsMode = TapsMode::Multi; cost ("16 taps (4 harmonizados)", p, &t);
        p = base; p.tapeDrive = 0.5f; p.satHQ = true; p.diffAmount = 0.6f; p.pitchMix = 0.5f; p.reverse = 0.3f; p.unmask = 0.5f; p.guard = true; p.tapsMode = TapsMode::Multi;
        p.modDepthCents = 20; p.wow = 0.5f; p.duckOn = true; p.model = Model::Cassette;
        const double all = cost ("Tudo ligado", p, &t);
        say ("CPU base (um núcleo, 48 kHz estéreo)", b0, "%");
        say ("CPU com tudo ligado", all, "%");
    }
    say ("Latência do plugin", 0, "amostras");
    std::printf ("dados em %s\n", outDir.c_str());
    return 0;
}
