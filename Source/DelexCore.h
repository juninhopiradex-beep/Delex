// =============================================================================
//  DELEX by Piradex — núcleo DSP (sem dependências do JUCE, testável à parte)
//
//  Fluxo do sinal (por amostra, estéreo):
//
//   entrada ─► largura/rotação de entrada ─► pré-drive ─┐
//                                                       ▼
//        ┌──────────── (+) ◄── feedback × r[n-1] ◄──────────────┐
//        ▼                                                      │
//   cadeia do loop (ordem configurável):                        │
//   Digital Grit · Feedback EQ · Dispersão · Difusão · Fita     │
//        ▼                                                      │
//   bloqueio DC + limitador suave de segurança                  │
//        ▼                                                      │
//   linhas de atraso L/R (interpolação Hermite, modulação) ─► matriz 2×2
//                                         unitária (rotação/reflexão) ─► r[n]
//                                                                    │
//   r[n] ─► ducker ─► rotação/largura de saída ─► trim wet ─► mistura dry/wet ─► saída
//
//  Estabilidade: a matriz é ortogonal (não acrescenta energia), os filtros do
//  loop têm ganho ≤ 1 excepto o pico do EQ, cujo ganho é compensado no
//  feedback, e um limitador suave + bloqueio DC + verificação NaN/Inf
//  impedem qualquer explosão mesmo com feedback de 100 %.
// =============================================================================
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace delex
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr double kMaxDelaySeconds = 8.0;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float clampf (float v, float lo, float hi) noexcept { return std::min (hi, std::max (lo, v)); }
inline bool  finite (float v) noexcept { return std::isfinite (v); }

// -----------------------------------------------------------------------------
//  Parâmetros do motor (valores finais, já com macros aplicadas pelo plugin)
// -----------------------------------------------------------------------------
enum class Routing    { Dual = 0, PingPong = 1, Mono = 2 };
enum class Transition { TapeGlide = 0, Crossfade = 1, Stretch = 2 };
enum class ModShape   { Sine = 0, Triangle = 1, Random = 2 };
enum class CutSlope   { dB6 = 0, dB12 = 1, dB24 = 2 };
enum class Monitor    { Normal = 0, WetSolo = 1, Delta = 2, Mono = 3, Phone = 4, Club = 5 };
enum class Model      { Tape = 0, BBD = 1, MultiHead = 2, Cassette = 3, Digital80 = 4 };
enum class TapsMode   { Off = 0, Sculpt = 1, Multi = 2 };

// Um tap (Echo Sculpt ou Multi-Tap). pos: repetição n (Sculpt) ou posição em semínimas (Multi).
struct Tap { float pos = 1, gain = 0, pan = 0, tone = 1, semis = 0; };
struct TapSet
{
    static constexpr int kMax = 32;
    std::array<Tap, kMax> taps {};
    int count = 0;
};

// Ordem dos 5 módulos do loop: 0 Grit · 1 EQ · 2 Dispersão · 3 Difusão · 4 Fita.
// As 4 primeiras permutações são as ordens das versões anteriores (compatibilidade).
inline const std::vector<std::array<uint8_t, 5>>& loopPermutations()
{
    static const std::vector<std::array<uint8_t, 5>> perms = []
    {
        std::vector<std::array<uint8_t, 5>> v { { 0, 1, 2, 3, 4 }, { 1, 4, 3, 2, 0 }, { 4, 1, 3, 2, 0 }, { 3, 2, 1, 4, 0 } };
        std::array<uint8_t, 5> a { 0, 1, 2, 3, 4 };
        do
        {
            if (std::find (v.begin(), v.end(), a) == v.end()) v.push_back (a);
        } while (std::next_permutation (a.begin(), a.end()));
        return v;
    }();
    return perms;
}

struct Params
{
    // Tempo
    float timeMsL = 375.0f, timeMsR = 375.0f;   // 1 … 8000 ms
    float feedback = 0.4f;                      // 0 … 1
    float fbBalance = 0.0f;                     // -1 (só L) … +1 (só R)
    Routing routing = Routing::Dual;
    Transition transition = Transition::TapeGlide;
    float glideMs = 250.0f;
    bool  freeze = false;

    // Modulação
    float modDepthCents = 0.0f, modRateHz = 0.3f, modOffset = 0.5f;   // offset = desfasamento L/R (0…1 ciclo)
    ModShape modShape = ModShape::Sine;
    float wow = 0.0f;                           // 0 … 1 (wow/flutter de fita)

    // Espaço
    float rotDeg = 0.0f;  bool reflection = false;
    float inRotDeg = 0.0f, outRotDeg = 0.0f;    // ±45° (45° = conversão L/R ⇄ M/S)
    float inWidth = 1.0f, outWidth = 1.0f;      // 0 … 2
    bool  monoSafe = false;                     // graves do wet em mono (< 150 Hz)

    // Feedback EQ
    float lowCutHz = 20.0f, highCutHz = 20000.0f;
    CutSlope cutSlope = CutSlope::dB12;
    float peakHz = 1000.0f, peakDb = 0.0f, peakQ = 1.0f, compensate = 0.5f;
    float lsHz = 150.0f, lsDb = 0.0f, hsHz = 6000.0f, hsDb = 0.0f;   // shelves
    float pk2Hz = 3000.0f, pk2Db = 0.0f, pk2Q = 1.0f;                 // segundo pico

    // Dispersão / difusão
    float dispHz = 500.0f, dispSpreadMs = 0.0f;
    float diffAmount = 0.0f, diffSpreadMs = 10.0f;

    // Fita
    float tapeWarmth = 0.0f, tapeDrive = 0.0f;

    // Digital grit
    float gritBits = 24.0f, gritRateHz = 192000.0f, gritJitter = 0.0f, gritMix = 1.0f;

    // Ordem do loop (índice em loopPermutations(), 0…119)
    int loopOrder = 0;

    // Ganhos
    float preDriveDb = 0.0f, wetTrimDb = 0.0f, outTrimDb = 0.0f;

    // Ducker (atua no wet)
    bool  duckOn = false;
    float duckThreshDb = -24.0f, duckAmountDb = 9.0f;
    float duckAttackMs = 5.0f, duckHoldMs = 50.0f, duckReleaseMs = 250.0f;

    // Gate dos ecos (efeito "não linear": a cauda corta-se de repente)
    bool  gateOn = false;
    float gateThreshDb = -30.0f, gateHoldMs = 250.0f, gateReleaseMs = 40.0f;

    // Mistura
    float mix = 0.25f;  bool equalPower = true;
    bool  bypass = false;

    // Escuta e qualidade
    Monitor monitor = Monitor::Normal;
    bool  constLoudness = false;    // volume dos ecos independente do feedback
    float hiss = 0.0f;              // 0 … 1 (ruído de fita no sinal com efeito)
    bool  satHQ = false;            // saturação com oversampling 2×

    // --- v1.0 vaga B
    bool  throwAuto = false;        // detecta o fim das frases e atira a última palavra para o delay
    bool  throwOnly = false;        // a entrada só entra no delay nos throws
    bool  throwNow = false;         // throw manual (automatizável / pad)
    float throwThreshDb = -30.0f, throwLenMs = 250.0f, throwGapMs = 120.0f;
    float pitchSemi = 0.0f, pitchMix = 0.0f;   // pitch no loop (shimmer com +12)
    float reverse = 0.0f;           // 0 … 1 (ecos invertidos)
    Model model = Model::Tape;
    int   headsMask = 7;            // multi-cabeça: bits 0..2 = cabeças 1, 2, 3
    bool  padStop = false;          // tape stop (pad)

    // --- v1.0 vaga C
    TapsMode tapsMode = TapsMode::Off;
    float tapsLevel = 1.0f;
    float bpm = 120.0f;
};

// -----------------------------------------------------------------------------
//  Matriz espacial 2×2 (ortogonal ⇒ preserva energia)
// -----------------------------------------------------------------------------
struct Mat2 { float a = 1, b = 0, c = 0, d = 1; };

inline Mat2 spatialMatrix (float deg, bool reflection) noexcept
{
    const float t = deg * (kPi / 180.0f);
    const float cs = std::cos (t), sn = std::sin (t);
    if (reflection) return { cs, sn, sn, -cs };   // reflexão (det = −1)
    return { cs, -sn, sn, cs };                   // rotação  (det = +1)
}
inline Mat2 rotation (float deg) noexcept { return spatialMatrix (deg, false); }

// -----------------------------------------------------------------------------
//  Utilitários de filtragem
// -----------------------------------------------------------------------------
struct OnePole   // suavização de parâmetros
{
    float y = 0, k = 0;
    void setTime (float ms, float sr) noexcept { k = (ms <= 0.0f) ? 1.0f : 1.0f - std::exp (-1.0f / (0.001f * ms * sr)); }
    void reset (float v) noexcept { y = v; }
    float next (float target) noexcept { y += k * (target - y); return y; }
};

// SVF topológico (Simper) — estável com modulação de parâmetros
struct SVF
{
    float ic1 = 0, ic2 = 0;
    float a1 = 0, a2 = 0, a3 = 0, m0 = 0, m1 = 0, m2 = 0;

    static float g (float hz, float sr) noexcept { return std::tan (kPi * clampf (hz, 5.0f, 0.49f * sr) / sr); }

    void setLowpass (float hz, float q, float sr) noexcept  { set (g (hz, sr), 1.0f / q); m0 = 0; m1 = 0; m2 = 1; }
    void setHighpass (float hz, float q, float sr) noexcept { const float k = 1.0f / q; set (g (hz, sr), k); m0 = 1; m1 = -k; m2 = -1; }
    void setAllpass (float hz, float q, float sr) noexcept  { const float k = 1.0f / q; set (g (hz, sr), k); m0 = 1; m1 = -2.0f * k; m2 = 0; }
    void setBell (float hz, float q, float db, float sr) noexcept
    {
        const float A = std::pow (10.0f, db / 40.0f);
        const float k = 1.0f / (q * A);
        set (g (hz, sr), k); m0 = 1; m1 = k * (A * A - 1.0f); m2 = 0;
    }
    void setLowShelf (float hz, float q, float db, float sr) noexcept
    {
        const float A = std::pow (10.0f, db / 40.0f);
        const float k = 1.0f / q;
        set (g (hz, sr) / std::sqrt (A), k); m0 = 1; m1 = k * (A - 1.0f); m2 = A * A - 1.0f;
    }
    void setHighShelf (float hz, float q, float db, float sr) noexcept
    {
        const float A = std::pow (10.0f, db / 40.0f);
        const float k = 1.0f / q;
        set (g (hz, sr) * std::sqrt (A), k); m0 = A * A; m1 = k * (1.0f - A) * A; m2 = 1.0f - A * A;
    }
    void set (float gg, float k) noexcept { a1 = 1.0f / (1.0f + gg * (gg + k)); a2 = gg * a1; a3 = gg * a2; }
    void reset() noexcept { ic1 = ic2 = 0; }
    float process (float v0) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return m0 * v0 + m1 * v1 + m2 * v2;
    }
};

struct OnePoleTPT   // passa-baixo / passa-alto de 1.ª ordem (6 dB/oct)
{
    float s = 0, G = 0;
    void set (float hz, float sr) noexcept { const float gg = SVF::g (hz, sr); G = gg / (1.0f + gg); }
    void reset() noexcept { s = 0; }
    float lp (float x) noexcept { const float v = (x - s) * G; const float y = v + s; s = y + v; return y; }
    float hp (float x) noexcept { return x - lp (x); }
};

// Secção de 1.ª ordem genérica (para pré/de-ênfase exactamente inversas)
struct FirstOrder
{
    float b0 = 1, b1 = 0, a1 = 0, z = 0;
    void reset() noexcept { z = 0; }
    float process (float x) noexcept { const float y = b0 * x + z; z = b1 * x - a1 * y; return y; }
};

// tanh com anti-aliasing por antiderivada (ADAA de 1.ª ordem): sem latência,
// reduz o aliasing da saturação dentro do loop (onde oversampling por blocos
// introduziria latência).
struct TanhADAA
{
    float x1 = 0;
    static float F (float x) noexcept
    {
        const float ax = std::fabs (x);
        return ax > 15.0f ? ax - 0.69314718f : std::log (std::cosh (x));
    }
    void reset() noexcept { x1 = 0; }
    float process (float x) noexcept
    {
        const float d = x - x1;
        float y;
        if (std::fabs (d) < 1.0e-4f) y = std::tanh (0.5f * (x + x1));
        else                          y = (F (x) - F (x1)) / d;
        x1 = x;
        return y;
    }
};

// -----------------------------------------------------------------------------
//  Oversampling 2× para a saturação (FIR de fase linear, janela de Kaiser).
//  Latência total (subir + descer) = kLatency amostras à taxa base; é
//  descontada no tempo de atraso, por isso o eco continua exactamente a tempo.
// -----------------------------------------------------------------------------
struct Oversampler2x
{
    static constexpr int kTaps = 31;           // ímpar; atraso de grupo (kTaps-1)/2 à taxa 2×
    static constexpr int kLatency = 14;        // medido (tests/test_core.cpp): pico do impulso e correlação a 1 kHz
    float h[kTaps] {};
    float up[kTaps] {};   int upPos = 0;       // histórico à taxa 2× (inserção de zeros)
    float dn[kTaps] {};   int dnPos = 0;

    static double bessel0 (double x) noexcept
    {
        double sum = 1, t = 1;
        for (int k = 1; k < 30; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += t; }
        return sum;
    }
    Oversampler2x()
    {
        const double beta = 8.0, M = kTaps - 1;
        double sum = 0;
        for (int n = 0; n < kTaps; ++n)
        {
            const double m = n - M / 2.0;
            const double sinc = m == 0 ? 0.5 : std::sin (3.14159265358979 * 0.5 * m) / (3.14159265358979 * m);   // corte em fs/4 (à taxa 2×)
            const double r = 2.0 * n / M - 1.0;
            const double w = bessel0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / bessel0 (beta);
            h[n] = (float) (sinc * w);
            sum += h[n];
        }
        for (auto& c : h) c = (float) (c / sum);
    }
    void reset() noexcept { std::fill (std::begin (up), std::end (up), 0.0f); std::fill (std::begin (dn), std::end (dn), 0.0f); upPos = dnPos = 0; }

    float firUp() const noexcept
    {
        float y = 0;
        for (int k = 0; k < kTaps; ++k) y += h[k] * up[(upPos - k + kTaps) % kTaps];
        return y;
    }
    // processa uma amostra: sobe a 2×, aplica f às duas sub-amostras, desce
    template <typename F> float process (float x, F&& f) noexcept
    {
        float s2[2];
        for (int j = 0; j < 2; ++j)
        {
            upPos = (upPos + 1) % kTaps;
            up[upPos] = j == 0 ? 2.0f * x : 0.0f;   // ganho 2 compensa os zeros
            s2[j] = f (firUp());
        }
        float y = 0;
        for (int j = 0; j < 2; ++j)
        {
            dnPos = (dnPos + 1) % kTaps;
            dn[dnPos] = s2[j];
            if (j == 1)
                for (int k = 0; k < kTaps; ++k) y += h[k] * dn[(dnPos - k + kTaps) % kTaps];
        }
        return y;
    }
};

// -----------------------------------------------------------------------------
//  Linha de atraso (potência de 2, leitura Hermite de 4 pontos)
// -----------------------------------------------------------------------------
class DelayLine
{
public:
    void allocate (int maxSamples)
    {
        int size = 1;
        while (size < maxSamples + 8) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }
    void clear() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); }
    int  capacity() const noexcept { return mask - 4; }

    void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // amostra escrita há k amostras (0 = a última) e escrita retroactiva nessa posição
    float ago (int k) const noexcept { return buf[(size_t) ((w - 1 - k) & mask)]; }
    void  addAgo (int k, float v) noexcept { buf[(size_t) ((w - 1 - k) & mask)] += v; }

    // atraso em amostras (≥ 4) medido a partir da próxima escrita
    float readHermite (float delay) const noexcept
    {
        delay = clampf (delay, 4.0f, (float) capacity());
        const float pos = (float) w - delay;
        const float fl = std::floor (pos);
        const float f = pos - fl;
        const int i = (int) fl;
        const float xm1 = buf[(size_t) ((i - 1) & mask)];
        const float x0  = buf[(size_t) (i & mask)];
        const float x1  = buf[(size_t) ((i + 1) & mask)];
        const float x2  = buf[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    float readLinear (float delay) const noexcept
    {
        delay = clampf (delay, 1.0f, (float) capacity());
        const float pos = (float) w - delay;
        const float fl = std::floor (pos);
        const float f = pos - fl;
        const int i = (int) fl;
        const float a = buf[(size_t) (i & mask)], b = buf[(size_t) ((i + 1) & mask)];
        return a + f * (b - a);
    }

private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

// -----------------------------------------------------------------------------
//  Gerador aleatório leve (xorshift) — sem alocações nem locks
// -----------------------------------------------------------------------------
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    float next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) * (1.0f / 16777216.0f) * 2.0f - 1.0f; }
};

// -----------------------------------------------------------------------------
//  LFO (seno / triângulo / aleatório suave)
// -----------------------------------------------------------------------------
struct LFO
{
    float phase = 0;  Rng rng;  float r0 = 0, r1 = 0, r2 = 0, r3 = 0;  float rPhase = 0;

    // devolve valor em [-1, 1] para a fase dada (0…1)
    static float shape (ModShape s, float ph) noexcept
    {
        ph -= std::floor (ph);
        if (s == ModShape::Triangle) return 1.0f - 4.0f * std::fabs (ph - 0.5f);
        return std::sin (kTwoPi * ph);
    }
    // aleatório suave (interpolação cúbica entre pontos aleatórios)
    float random (float inc) noexcept
    {
        rPhase += inc;
        if (rPhase >= 1.0f) { rPhase -= 1.0f; r0 = r1; r1 = r2; r2 = r3; r3 = rng.next(); }
        const float f = rPhase;
        const float c1 = 0.5f * (r2 - r0);
        const float c2 = r0 - 2.5f * r1 + 2.0f * r2 - 0.5f * r3;
        const float c3 = 0.5f * (r3 - r0) + 1.5f * (r1 - r2);
        return clampf (((c3 * f + c2) * f + c1) * f + r1, -1.0f, 1.0f);
    }
};

// =============================================================================
//  Motor
// =============================================================================
class Engine
{
public:
    // ---------------------------------------------------------------- setup
    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = (float) sampleRate;
        const int maxDelay = (int) std::ceil (kMaxDelaySeconds * sampleRate) + (int) (0.1 * sampleRate);
        for (auto& d : dl) d.allocate (maxDelay);
        const int diffMax = (int) std::ceil (0.060 * sampleRate) + 8;
        for (auto& ch : diff) for (auto& s : ch) s.line.allocate (diffMax);
        for (auto& h : hist) h.allocate ((int) std::ceil (2.2 * sampleRate));
        for (auto& ps : pitchBuf) ps.allocate ((int) std::ceil (0.12 * sampleRate));
        for (auto& t : tapDl) t.allocate ((int) std::ceil (kMaxDelaySeconds * sampleRate) + (int) (0.2 * sampleRate));
        reset();
    }

    void reset()
    {
        for (auto& d : dl) d.clear();
        for (auto& ch : diff) for (auto& s : ch) s.line.clear();
        for (auto& h : hist) h.clear();
        for (auto& ps : pitchBuf) ps.clear();
        for (auto& t : tapDl) t.clear();
        for (auto& st : tapState) st = TapRun {};
        thrEnv = 0; thrAbove = false; thrCount = thrBelow = 0; throwPrev = false;
        pitchPh = 0; revPh[0] = revPh[1] = 0;
        stopSpeed = 1; stopExtra = 0; stopGain = 1; stopping = false;
        for (int c = 0; c < 2; ++c)
        {
            for (auto* f : { &bbdA[c], &bbdB[c], &casA[c], &casB[c], &casHP[c], &digA[c], &digB[c], &mhLP[c] }) f->reset();
            lastDelay[c] = 0;
        }
        for (int c = 0; c < 2; ++c)
        {
            for (auto& f : cutLo[c]) f.reset();
            for (auto& f : cutHi[c]) f.reset();
            lo1[c].reset(); hi1[c].reset(); peak[c].reset();
            for (auto& f : disp[c]) f.reset();
            emph[c].reset(); deemph[c].reset(); sat[c].reset();
            gritAA[c].reset(); gritHold[c] = 0; dcX[c] = dcY[c] = 0;
            monoLP[c].reset();
            lsF[c].reset(); hsF[c].reset(); pk2F[c].reset(); os[c].reset();
            phoneHP[c].reset(); phoneLP[c].reset(); clubLP[c].reset(); clubLP2[c].reset();
            pink[c][0] = pink[c][1] = pink[c][2] = 0;
            r[c] = 0;
        }
        gritPhase = 0;
        lfo = LFO{}; lfoR = LFO{}; wowLfo = LFO{};
        lfoR.rng.s = 0x2545F491u; wowLfo.rng.s = 0x6C8E9CF5u;   // sementes diferentes por gerador
        hissRng[0].s = 0x1234567u; hissRng[1].s = 0x7654321u;
        lfoPhase = wowPhase = flutPhase = 0;
        envDuck = 0; duckGainDb = 0; holdCount = 0;
        envGate = 0; gateG = 1; gateHold = 0;
        haveTargets = false;
        loopLimitPeak = 0;
    }

    void setParams (const Params& p) noexcept { tgt = p; }
    // padrão de taps (copiado; chamado na thread de áudio antes de process)
    void setTaps (const TapSet& t) noexcept { taps = t; }

    // ------------------------------------------------------------ medição
    float duckReductionDb() const noexcept { return -duckGainDb; }   // ≥ 0
    float gateGain() const noexcept        { return gateG; }         // 0 (fechado) … 1 (aberto)
    float loopLimiterDb() const noexcept   { return loopLimitDbOut; }
    int   resets() const noexcept          { return safetyResets; }
    int   throwCount() const noexcept      { return throws; }     // cada throw (auto ou manual) incrementa

    // ------------------------------------------------------------ processo
    // keyL/keyR: sinal de sidechain (opcional) para ducker e gate; nulo = a própria entrada
    void process (float* L, float* R, int n, const float* keyL = nullptr, const float* keyR = nullptr) noexcept
    {
        if (! haveTargets) snapToTargets();
        loopLimitPeak = 0;

        for (int i = 0; i < n; ++i)
        {
            if ((ctl++ & 15) == 0) updateControl();

            const float dryL = L[i], dryR = R[i];
            const float kL = keyL != nullptr ? keyL[i] : dryL;
            const float kR = keyR != nullptr ? keyR[i] : (keyL != nullptr ? keyL[i] : dryR);

            // --- suavização por amostra
            const float fbS   = sm.fb.next (cur.fbTarget);
            const float mixS  = sm.mix.next (tgt.mix);
            const float frz   = sm.freeze.next (tgt.freeze ? 1.0f : 0.0f);
            const float byp   = sm.bypass.next (tgt.bypass ? 1.0f : 0.0f);
            const float wetG  = sm.wet.next (dbToGainFast (tgt.wetTrimDb));
            const float outG  = sm.out.next (dbToGainFast (tgt.outTrimDb));
            const float preG  = sm.pre.next (dbToGainFast (tgt.preDriveDb));
            const float inW   = sm.inW.next (tgt.inWidth);
            const float outW  = sm.outW.next (tgt.outWidth);
            const Mat2  M     { sm.ma.next (cur.M.a), sm.mb.next (cur.M.b), sm.mc.next (cur.M.c), sm.md.next (cur.M.d) };
            const Mat2  Rin   { sm.ia.next (cur.Rin.a), sm.ib.next (cur.Rin.b), sm.ic.next (cur.Rin.c), sm.id.next (cur.Rin.d) };
            const Mat2  Rout  { sm.oa.next (cur.Rout.a), sm.ob.next (cur.Rout.b), sm.oc.next (cur.Rout.c), sm.od.next (cur.Rout.d) };
            const float balL  = sm.balL.next (cur.balL), balR = sm.balR.next (cur.balR);
            const float pp    = sm.pingpong.next (tgt.routing == Routing::PingPong ? 1.0f : 0.0f);
            const float mono  = sm.mono.next (tgt.routing == Routing::Mono ? 1.0f : 0.0f);

            // --- entrada: largura e rotação
            float iL = dryL, iR = dryR;
            {
                const float m = 0.5f * (iL + iR), s = 0.5f * (iL - iR) * inW;
                iL = m + s; iR = m - s;
                const float rl = Rin.a * iL + Rin.b * iR, rr = Rin.c * iL + Rin.d * iR;
                iL = rl; iR = rr;
            }
            // encaminhamento: ping-pong entra só pela esquerda (soma mono); mono soma tudo
            {
                const float sum = 0.5f * (iL + iR);
                const float ppL = 0.0f, ppR = sum * 1.41421356f;   // 1.º eco sai à esquerda
                iL += pp * (ppL - iL);  iR += pp * (ppR - iR);
                iL += mono * (sum - iL); iR += mono * (sum - iR);
            }
            const float inGate = 1.0f - frz;    // freeze: deixa de entrar sinal novo
            iL *= preG; iR *= preG;
            hist[0].push (iL); hist[1].push (iR);
            autoThrow (dryL, dryR, frz > 0.5f);
            const float sendS = sm.send.next (tgt.throwOnly && ! tgt.throwNow ? 0.0f : 1.0f);
            iL *= inGate * sendS;  iR *= inGate * sendS;
            tapDl[0].push (iL); tapDl[1].push (iR);

            // --- leitura das linhas de atraso (com modulação)
            float modL = 0, modR = 0;
            modulation (modL, modR);
            const float rg = tapeStop();
            modL += stopExtra; modR += stopExtra;
            float dL = readChannel (0, modL);
            float dR = readChannel (1, modR);
            if (mhW > 0.001f) { dL += mhW * (multiHead (0, modL) - dL); dR += mhW * (multiHead (1, modR) - dR); }
            if (revW > 0.001f) { dL += revW * (readReverse (0, modL) - dL); dR += revW * (readReverse (1, modR) - dR); }
            dL *= rg; dR *= rg;

            // --- matriz espacial (em ping-pong troca canais; em mono identidade)
            float yL = M.a * dL + M.b * dR, yR = M.c * dL + M.d * dR;
            {
                const float sL = dR, sR = dL;     // troca (reflexão a 90°)
                yL += pp * (sL - yL); yR += pp * (sR - yR);
                const float idL = dL, idR = dR;
                yL += mono * (idL - yL); yR += mono * (idR - yR);
            }
            r[0] = yL; r[1] = yR;

            // --- loop de feedback
            const float fbFreeze = fbS + frz * (1.0f - fbS);     // freeze ⇒ ganho 1, sem perdas
            float xL = iL + fbFreeze * (balL + frz * (1.0f - balL)) * yL;
            float xR = iR + fbFreeze * (balR + frz * (1.0f - balR)) * yR;

            float procL = loopChain (0, xL), procR = loopChain (1, xR);
            if (pitchW > 0.0005f) { procL += pitchW * (pitchShift (0, procL) - procL); procR += pitchW * (pitchShift (1, procR) - procR); }
            else { pitchBuf[0].push (procL); pitchBuf[1].push (procR); }
            procL = modelStage (0, procL); procR = modelStage (1, procR);
            xL += (1.0f - frz) * (procL - xL);    // em freeze, loop sem perdas
            xR += (1.0f - frz) * (procR - xR);

            xL = safety (0, xL); xR = safety (1, xR);
            if (! finite (xL) || ! finite (xR)) { emergencyReset(); xL = xR = 0; yL = yR = 0; }

            dl[0].push (xL); dl[1].push (xR);

            // --- wet: ducker, rotação/largura de saída
            float wL = yL, wR = yR;
            {
                const float sc = sm.sculpt.next (tgt.tapsMode == TapsMode::Sculpt ? 1.0f : 0.0f);
                const float tw = sm.taps.next (tgt.tapsMode == TapsMode::Off ? 0.0f : tgt.tapsLevel);
                wL *= 1.0f - sc; wR *= 1.0f - sc;                 // Echo Sculpt substitui os ecos do loop
                if (tw > 1.0e-4f && taps.count > 0)
                {
                    float tL = 0, tR = 0;
                    renderTaps (tL, tR);
                    wL += tw * tL; wR += tw * tR;
                }
            }
            const float duckG = ducker (kL, kR) * gate (kL, kR);
            wL *= duckG; wR *= duckG;
            {
                const float rl = Rout.a * wL + Rout.b * wR, rr = Rout.c * wL + Rout.d * wR;
                const float m = 0.5f * (rl + rr);
                float s = 0.5f * (rl - rr) * outW;
                if (tgt.monoSafe) s -= monoLP[0].lp (s);          // graves do wet em mono
                // larguras > 100 %: normaliza a energia (sinal descorrelacionado mantém nível)
                const float norm = outW > 1.0f ? std::sqrt (2.0f / (1.0f + outW * outW)) : 1.0f;
                wL = (m + s) * norm;
                wR = (m - s) * norm;
            }
            const float mk = sm.makeup.next (makeup) * sm.loud.next (loudComp);   // saturação + volume constante (fora do loop)
            wL *= wetG * mk; wR *= wetG * mk;
            if (hissGain > 0.0f) { wL += hissGain * hissNoise (0); wR += hissGain * hissNoise (1); }

            // --- mistura
            float gd, gw;
            if (tgt.equalPower) { gd = std::cos (mixS * 0.5f * kPi); gw = std::sin (mixS * 0.5f * kPi); }
            else                { gd = 1.0f - mixS; gw = mixS; }
            float oL = (gd * dryL + gw * wL) * outG;
            float oR = (gd * dryR + gw * wR) * outG;

            // --- escuta (monitorização)
            monitorStage (oL, oR, wL * outG, wR * outG, gw);

            // --- bypass sem cliques (os ecos continuam a correr por baixo)
            oL += byp * (dryL - oL);
            oR += byp * (dryR - oR);

            L[i] = oL; R[i] = oR;
        }
        loopLimitDbOut = loopLimitPeak > 1.0f ? -gainToDb (loopLimitPeak / softLimit (loopLimitPeak)) : 0.0f;
    }

    // ---------------------------------------------------------------- mapa de ecos
    struct Echo { float timeMs, l, r; };
    // Calcula as primeiras N repetições de um impulso (para o visualizador).
    static int echoMap (const Params& p, Echo* out, int maxEchoes) noexcept
    {
        float vL = 1.0f, vR = 1.0f;
        { const float m = 0.5f * (vL + vR), s = 0.5f * (vL - vR) * p.inWidth; vL = m + s; vR = m - s; }
        { const Mat2 Ri = rotation (p.inRotDeg); const float a = Ri.a * vL + Ri.b * vR, b = Ri.c * vL + Ri.d * vR; vL = a; vR = b; }
        if (p.routing == Routing::PingPong) { vR = 0.5f * (vL + vR) * 1.41421356f; vL = 0; }
        if (p.routing == Routing::Mono)     { vL = vR = 0.5f * (vL + vR); }
        vL *= 0.7071f; vR *= 0.7071f;
        const Mat2 M = spatialMatrix (p.rotDeg, p.reflection);
        const Mat2 Ro = rotation (p.outRotDeg);
        const float loopG = std::min (1.0f, p.feedback * compensation (p) * eqGainAt1k (p));
        const float balL = p.fbBalance > 0 ? 1.0f - p.fbBalance : 1.0f;
        const float balR = p.fbBalance < 0 ? 1.0f + p.fbBalance : 1.0f;
        const float t = 0.5f * (p.timeMsL + (p.routing == Routing::Mono ? p.timeMsL : p.timeMsR));
        int n = 0;
        for (; n < maxEchoes; ++n)
        {
            float a, b;
            if (p.routing == Routing::PingPong)  { a = vR; b = vL; }
            else if (p.routing == Routing::Mono) { a = vL; b = vR; }
            else { a = M.a * vL + M.b * vR; b = M.c * vL + M.d * vR; }
            float oL = Ro.a * a + Ro.b * b, oR = Ro.c * a + Ro.d * b;
            const float m = 0.5f * (oL + oR), s = 0.5f * (oL - oR) * p.outWidth;
            out[n] = { t * (float) (n + 1), m + s, m - s };
            vL = a * loopG * balL; vR = b * loopG * balR;
            if (std::fabs (vL) + std::fabs (vR) < 1.0e-3f) { ++n; break; }
        }
        return n;
    }

    static float compensation (const Params& p) noexcept
    {
        const float boost = std::max ({ 0.0f, p.peakDb, p.pk2Db, p.lsDb, p.hsDb });
        return dbToGain (-boost * p.compensate);
    }
    static float eqGainAt1k (const Params& p) noexcept
    {
        // aproximação do ganho do EQ do loop a 1 kHz (para o visualizador)
        const float f = 1000.0f;
        float g = 1.0f;
        const float lc = f / std::max (1.0f, p.lowCutHz), hc = std::max (1.0f, p.highCutHz) / f;
        g *= lc / std::sqrt (1.0f + lc * lc);
        g *= hc / std::sqrt (1.0f + hc * hc);
        const float oct = std::log2 (f / p.peakHz);
        g *= dbToGain (p.peakDb * std::exp (-oct * oct * p.peakQ * p.peakQ * 2.0f));
        return g;
    }

private:
    // ---------------------------------------------------------------- estado
    float sr = 48000.0f;
    Params tgt;
    bool haveTargets = false;
    uint32_t ctl = 0;

    struct Derived
    {
        float fbTarget = 0;  Mat2 M, Rin, Rout;  float balL = 1, balR = 1;
    } cur;

    struct Smooth
    {
        OnePole fb, mix, freeze, bypass, wet, out, pre, inW, outW,
                ma, mb, mc, md, ia, ib, ic, id, oa, ob, oc, od, balL, balR, pingpong, mono, makeup, loud, send, sculpt, taps;
    } sm;

    // filtros smoothed a ritmo de controlo
    OnePole cLo, cHi, cPk, cPkDb, cPkQ, cDispHz, cDispQ, cDispW, cDiffAmt, cDiffSpread,
            cWarm, cDrive, cBits, cRate, cGritMix, cJit, cModAmpL, cModAmpR, cModRate;

    // linhas principais e transições
    DelayLine dl[2];
    struct Heads { float a = 0, b = 0, xf = 0; bool onB = false, fading = false; float glide = 0; } heads[2];
    float r[2] {};

    // modulação
    LFO lfo, lfoR, wowLfo;  float lfoPhase = 0, wowPhase = 0;
    float glideK = 0.001f;

    // EQ do loop
    std::array<SVF, 2> cutLo[2], cutHi[2];
    OnePoleTPT lo1[2], hi1[2];
    SVF peak[2];
    int slope = 1;
    float compGain = 1;

    // dispersão
    static constexpr int kDisp = 8;
    std::array<SVF, kDisp> disp[2];
    float dispW = 0;

    // difusão
    struct AP { DelayLine line; float d = 100; };
    std::array<AP, 4> diff[2];
    float diffAmt = 0, diffSum = 0;
    static constexpr float kDiffG = 0.62f;

    // fita
    FirstOrder emph[2], deemph[2];
    TanhADAA sat[2];
    float drive = 0, satMix = 0, satGain = 1, makeup = 1;

    // grit
    SVF gritAA[2];
    float gritHold[2] {}, gritPhase = 0, gritInc = 1, bitsStep = 0, gritW = 0, jitter = 0;
    Rng gritRng;

    // segurança
    float dcX[2] {}, dcY[2] {}, dcR = 0.9995f;
    float loopLimitPeak = 0, loopLimitDbOut = 0;
    int safetyResets = 0;

    // ducker
    float envDuck = 0, duckGainDb = 0;  int holdCount = 0;

    // mono-safe
    OnePoleTPT monoLP[2];

    // EQ extra (shelves + 2.º pico)
    SVF lsF[2], hsF[2], pk2F[2];
    bool lsOn = false, hsOn = false, pk2On = false;
    OnePole cLs, cLsDb, cHs, cHsDb, cPk2, cPk2Db, cPk2Q;

    // saturação HQ
    Oversampler2x os[2];

    // volume constante e hiss
    float loudComp = 1.0f, hissGain = 0.0f;
    Rng hissRng[2];
    float pink[2][3] {};

    // escuta
    SVF phoneHP[2], phoneLP[2], clubLP[2], clubLP2[2];

    // vaga C
    TapSet taps;
    DelayLine tapDl[2];

    // vaga B
    DelayLine hist[2], pitchBuf[2];
    float thrEnv = 0; bool thrAbove = false, throwPrev = false; int thrCount = 0, thrBelow = 0, throws = 0;
    float pitchRatio = 1, pitchPh = 0, pitchW = 0, revW = 0, mhW = 0, revPh[2] {};
    float modelW[5] { 1, 0, 0, 0, 0 };
    float lastDelay[2] {};
    SVF bbdA[2], bbdB[2], casA[2], casB[2], casHP[2], digA[2], digB[2], mhLP[2];
    LFO casLfo; float casDrift = 0;
    float stopSpeed = 1, stopExtra = 0, stopGain = 1; bool stopping = false;
    OnePole cPitch, cPitchMix, cRev, cMH, cMod[5];

    // ---------------------------------------------------------------- helpers
    static float dbToGainFast (float db) noexcept { return std::exp (db * 0.11512925f); }

    static float softLimit (float x) noexcept
    {
        // linear até ±1, depois satura suavemente até ±2 (+6 dBFS)
        const float ax = std::fabs (x);
        if (ax <= 1.0f) return x;
        const float y = 1.0f + std::tanh (ax - 1.0f);
        return x < 0 ? -y : y;
    }

    float msToSamples (float ms) const noexcept { return ms * 0.001f * sr; }

    void snapToTargets() noexcept
    {
        updateControl (true);
        auto snap = [] (OnePole& s, float v) { s.reset (v); };
        snap (sm.fb, cur.fbTarget); snap (sm.mix, tgt.mix); snap (sm.freeze, tgt.freeze ? 1.0f : 0.0f);
        snap (sm.bypass, tgt.bypass ? 1.0f : 0.0f); snap (sm.wet, dbToGainFast (tgt.wetTrimDb));
        snap (sm.out, dbToGainFast (tgt.outTrimDb)); snap (sm.pre, dbToGainFast (tgt.preDriveDb));
        snap (sm.inW, tgt.inWidth); snap (sm.outW, tgt.outWidth);
        snap (sm.ma, cur.M.a); snap (sm.mb, cur.M.b); snap (sm.mc, cur.M.c); snap (sm.md, cur.M.d);
        snap (sm.ia, cur.Rin.a); snap (sm.ib, cur.Rin.b); snap (sm.ic, cur.Rin.c); snap (sm.id, cur.Rin.d);
        snap (sm.oa, cur.Rout.a); snap (sm.ob, cur.Rout.b); snap (sm.oc, cur.Rout.c); snap (sm.od, cur.Rout.d);
        snap (sm.balL, cur.balL); snap (sm.balR, cur.balR);
        snap (sm.pingpong, tgt.routing == Routing::PingPong ? 1.0f : 0.0f);
        snap (sm.mono, tgt.routing == Routing::Mono ? 1.0f : 0.0f);
        snap (sm.makeup, makeup);
        snap (sm.loud, loudComp);
        snap (sm.send, tgt.throwOnly && ! tgt.throwNow ? 0.0f : 1.0f);
        snap (sm.sculpt, tgt.tapsMode == TapsMode::Sculpt ? 1.0f : 0.0f);
        snap (sm.taps, tgt.tapsMode == TapsMode::Off ? 0.0f : tgt.tapsLevel);
        for (int c = 0; c < 2; ++c)
        {
            const float t = targetDelay (c);   // já com as compensações (difusão, oversampling)
            heads[c].a = heads[c].b = heads[c].glide = t;
            heads[c].xf = 0; heads[c].onB = false; heads[c].fading = false;
        }
        haveTargets = true;
        updateControl (true);
    }

    float timeR() const noexcept { return tgt.routing == Routing::Mono ? tgt.timeMsL : tgt.timeMsR; }

    // ritmo de controlo: a cada 16 amostras
    void updateControl (bool snap = false) noexcept
    {
        const float ctlRate = sr / 16.0f;
        auto setT = [&] (OnePole& s, float ms) { s.setTime (ms, ctlRate); };
        if (snap)
        {
            for (auto* s : { &sm.fb, &sm.mix, &sm.freeze, &sm.wet, &sm.out, &sm.pre, &sm.inW, &sm.outW,
                             &sm.ma, &sm.mb, &sm.mc, &sm.md, &sm.ia, &sm.ib, &sm.ic, &sm.id,
                             &sm.oa, &sm.ob, &sm.oc, &sm.od, &sm.balL, &sm.balR, &sm.pingpong, &sm.mono, &sm.makeup, &sm.loud, &sm.sculpt, &sm.taps })
                s->setTime (25.0f, sr);
            sm.send.setTime (4.0f, sr);
            sm.bypass.setTime (15.0f, sr);
            sm.freeze.setTime (60.0f, sr);
            for (auto* s : { &cLo, &cHi, &cPk, &cPkDb, &cPkQ, &cDispHz, &cDispQ, &cDispW, &cDiffAmt, &cDiffSpread,
                             &cWarm, &cDrive, &cBits, &cRate, &cGritMix, &cJit, &cModAmpL, &cModAmpR, &cModRate,
                             &cLs, &cLsDb, &cHs, &cHsDb, &cPk2, &cPk2Db, &cPk2Q, &cPitch, &cPitchMix, &cRev, &cMH,
                             &cMod[0], &cMod[1], &cMod[2], &cMod[3], &cMod[4] })
                setT (*s, 30.0f);
            setT (cDiffSpread, 120.0f);
        }

        // --- derivados
        glideK = 1.0f - std::exp (-1.0f / (0.001f * std::max (5.0f, tgt.glideMs) * sr));
        compGain = compensation (tgt);
        cur.fbTarget = clampf (tgt.feedback, 0.0f, 1.0f) * compGain;
        cur.balL = tgt.fbBalance > 0 ? 1.0f - tgt.fbBalance : 1.0f;
        cur.balR = tgt.fbBalance < 0 ? 1.0f + tgt.fbBalance : 1.0f;
        cur.M    = spatialMatrix (tgt.rotDeg, tgt.reflection);
        cur.Rin  = rotation (tgt.inRotDeg);
        cur.Rout = rotation (tgt.outRotDeg);

        auto v = [&] (OnePole& s, float target) { if (snap) s.reset (target); return s.next (target); };

        // --- EQ do loop
        const float lo = v (cLo, std::log (clampf (tgt.lowCutHz, 10.0f, 0.45f * sr)));
        const float hi = v (cHi, std::log (clampf (tgt.highCutHz, 20.0f, 0.45f * sr)));
        const float pk = v (cPk, std::log (clampf (tgt.peakHz, 20.0f, 0.45f * sr)));
        const float pdb = v (cPkDb, tgt.peakDb);
        const float pq = v (cPkQ, tgt.peakQ);
        slope = (int) tgt.cutSlope;
        const float loHz = std::exp (lo), hiHz = std::exp (hi);
        for (int c = 0; c < 2; ++c)
        {
            lo1[c].set (loHz, sr); hi1[c].set (hiHz, sr);
            if (slope == 1) { cutLo[c][0].setHighpass (loHz, 0.70710678f, sr); cutHi[c][0].setLowpass (hiHz, 0.70710678f, sr); }
            else if (slope == 2)
            {
                cutLo[c][0].setHighpass (loHz, 0.5411961f, sr); cutLo[c][1].setHighpass (loHz, 1.3065630f, sr);
                cutHi[c][0].setLowpass (hiHz, 0.5411961f, sr);  cutHi[c][1].setLowpass (hiHz, 1.3065630f, sr);
            }
            peak[c].setBell (std::exp (pk), clampf (pq, 0.1f, 20.0f), pdb, sr);
        }
        {
            const float lsHz = std::exp (v (cLs, std::log (clampf (tgt.lsHz, 20.0f, 0.4f * sr)))), lsDb = v (cLsDb, tgt.lsDb);
            const float hsHz = std::exp (v (cHs, std::log (clampf (tgt.hsHz, 200.0f, 0.4f * sr)))), hsDb = v (cHsDb, tgt.hsDb);
            const float p2Hz = std::exp (v (cPk2, std::log (clampf (tgt.pk2Hz, 20.0f, 0.45f * sr)))), p2Db = v (cPk2Db, tgt.pk2Db), p2Q = v (cPk2Q, tgt.pk2Q);
            lsOn = std::fabs (lsDb) > 0.01f || std::fabs (tgt.lsDb) > 0.01f;
            hsOn = std::fabs (hsDb) > 0.01f || std::fabs (tgt.hsDb) > 0.01f;
            pk2On = std::fabs (p2Db) > 0.01f || std::fabs (tgt.pk2Db) > 0.01f;
            for (int c = 0; c < 2; ++c)
            {
                lsF[c].setLowShelf (lsHz, 0.70710678f, lsDb, sr);
                hsF[c].setHighShelf (hsHz, 0.70710678f, hsDb, sr);
                pk2F[c].setBell (p2Hz, clampf (p2Q, 0.1f, 20.0f), p2Db, sr);
            }
        }

        // --- volume constante: a energia de uma cauda com ganho de loop g é 1/(1−g²);
        //     normaliza para a de feedback 40 % (o volume "de referência")
        if (tgt.constLoudness)
        {
            const float g = clampf (tgt.feedback * compGain * eqGainAt1k (tgt), 0.0f, 0.985f);
            loudComp = clampf (std::sqrt ((1.0f - g * g) / (1.0f - 0.16f)), 0.125f, 2.0f);
        }
        else loudComp = 1.0f;
        hissGain = tgt.hiss > 0.0f ? dbToGain (-92.0f + 44.0f * tgt.hiss) : 0.0f;   // −92 … −48 dBFS

        if (snap || monitorPrev != (int) tgt.monitor)
        {
            monitorPrev = (int) tgt.monitor;
            for (int c = 0; c < 2; ++c)
            {
                phoneHP[c].setHighpass (400.0f, 0.70710678f, sr); phoneLP[c].setLowpass (3500.0f, 0.70710678f, sr);
                clubLP[c].setLowpass (120.0f, 0.70710678f, sr);   clubLP2[c].setLowpass (120.0f, 0.70710678f, sr);
            }
        }

        // --- dispersão: N allpass de 2.ª ordem; atraso de grupo no centro = N·2Q/(π·f0)
        const float dHz = std::exp (v (cDispHz, std::log (clampf (tgt.dispHz, 20.0f, 0.4f * sr))));
        const float tau = std::max (0.0f, tgt.dispSpreadMs) * 0.001f;
        const float dq = v (cDispQ, clampf (tau * kPi * dHz / (2.0f * kDisp), 0.1f, 25.0f));
        dispW = v (cDispW, clampf (tgt.dispSpreadMs / 1.0f, 0.0f, 1.0f));
        for (int c = 0; c < 2; ++c) for (auto& f : disp[c]) f.setAllpass (dHz, dq, sr);

        // --- difusão
        diffAmt = v (cDiffAmt, clampf (tgt.diffAmount, 0.0f, 1.0f));
        const float spread = v (cDiffSpread, clampf (tgt.diffSpreadMs, 0.5f, 50.0f));
        static constexpr float ratios[4] = { 1.0f, 0.683f, 0.471f, 0.317f };
        diffSum = 0;
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 4; ++k)
            {
                diff[c][(size_t) k].d = std::max (1.0f, msToSamples (spread * ratios[k] * (c == 0 ? 1.0f : 1.071f)));
                if (c == 0) diffSum += diff[c][(size_t) k].d;
            }

        // --- fita: pré-ênfase (shelf de agudos) e de-ênfase exactamente inversa
        const float warm = v (cWarm, clampf (tgt.tapeWarmth, 0.0f, 1.0f));
        drive = v (cDrive, clampf (tgt.tapeDrive, 0.0f, 1.0f));
        satMix = clampf (drive * 8.0f, 0.0f, 1.0f);
        satGain = 1.0f + drive * 5.0f;                               // ganho de pequeno sinal = 1 (loop estável)
        makeup = 1.0f + satMix * (std::pow (satGain, 0.35f) - 1.0f); // compensação aplicada só ao wet
        {
            const float G = dbToGain (warm * 12.0f);
            const float w = std::tan (kPi * std::min (2500.0f, 0.4f * sr) / sr);
            const float n0 = (G + w) / (1.0f + w), n1 = (w - G) / (1.0f + w), d1 = (w - 1.0f) / (1.0f + w);
            for (int c = 0; c < 2; ++c)
            {
                emph[c].b0 = n0; emph[c].b1 = n1; emph[c].a1 = d1;
                // inverso: (1 + d1 z⁻¹)/(n0 + n1 z⁻¹), normalizado por n0
                deemph[c].b0 = 1.0f / n0; deemph[c].b1 = d1 / n0; deemph[c].a1 = n1 / n0;
            }
        }

        // --- grit
        const float bits = v (cBits, clampf (tgt.gritBits, 1.0f, 24.0f));
        const float rate = v (cRate, clampf (tgt.gritRateHz, 500.0f, 192000.0f));
        gritW = v (cGritMix, clampf (tgt.gritMix, 0.0f, 1.0f));
        jitter = v (cJit, clampf (tgt.gritJitter, 0.0f, 1.0f));
        gritInc = std::min (1.0f, rate / sr);
        bitsStep = bits >= 23.5f ? 0.0f : 2.0f / std::pow (2.0f, bits);
        for (int c = 0; c < 2; ++c) gritAA[c].setLowpass (std::min (0.45f * rate, 0.45f * sr), 0.70710678f, sr);
        gritActive = (bitsStep > 0.0f || gritInc < 0.999f) && gritW > 0.001f;

        // --- vaga C: taps (tempo, pan, tom e afinação de cada um)
        updateTaps();

        // --- vaga B: pitch, reverse, modelos
        {
            const float semi = v (cPitch, clampf (tgt.pitchSemi, -24.0f, 24.0f));
            pitchRatio = std::pow (2.0f, semi / 12.0f);
            pitchW = v (cPitchMix, clampf (tgt.pitchMix, 0.0f, 1.0f));
            revW = v (cRev, clampf (tgt.reverse, 0.0f, 1.0f));
            for (int m = 0; m < 5; ++m) modelW[m] = v (cMod[m], (int) tgt.model == m ? 1.0f : 0.0f);
            mhW = v (cMH, tgt.model == Model::MultiHead ? 1.0f : 0.0f);
            const float tms = 0.5f * (tgt.timeMsL + timeR());
            const float bbdHz = clampf (9000.0f * std::sqrt (250.0f / std::max (20.0f, tms)), 2500.0f, 9000.0f);
            const float nyq = 0.45f * sr;
            for (int c = 0; c < 2; ++c)
            {
                bbdA[c].setLowpass (std::min (bbdHz, nyq), 0.54f, sr); bbdB[c].setLowpass (std::min (bbdHz, nyq), 1.31f, sr);
                casA[c].setLowpass (std::min (7000.0f, nyq), 0.7071f, sr); casB[c].setLowpass (std::min (9000.0f, nyq), 0.6f, sr);
                casHP[c].setHighpass (55.0f, 0.7071f, sr);
                digA[c].setLowpass (std::min (12000.0f, nyq), 0.54f, sr); digB[c].setLowpass (std::min (12000.0f, nyq), 1.31f, sr);
                mhLP[c].setLowpass (std::min (8500.0f, nyq), 0.7071f, sr);
            }
        }

        // --- modulação: amplitude (s) para obter o desvio de afinação pedido
        const float rateHz = v (cModRate, clampf (tgt.modRateHz, 0.01f, 20.0f));
        const float ratio = 1.0f - std::pow (2.0f, -clampf (tgt.modDepthCents, 0.0f, 100.0f) / 1200.0f);
        float ampS = tgt.modShape == ModShape::Triangle ? ratio / (4.0f * rateHz) : ratio / (kTwoPi * rateHz);
        ampS = std::min (ampS, 0.025f);
        modAmp = v (cModAmpL, ampS * sr);
        modRate = rateHz;
        const float wowT = clampf (tgt.wow + (tgt.model == Model::Cassette ? 0.35f : 0.0f), 0.0f, 1.3f);   // cassete: wow extra
        wowAmp = v (cModAmpR, wowT * 0.0016f * sr);   // até ~1.6 ms (wow lento + flutter)
    }

    bool gritActive = false;
    int monitorPrev = -1;
    float modAmp = 0, modRate = 0.3f, wowAmp = 0;
    float flutPhase = 0;

    void modulation (float& mL, float& mR) noexcept
    {
        const float inc = modRate / sr;
        lfoPhase += inc; if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        float a, b;
        if (tgt.modShape == ModShape::Random)
        {
            // offset = quão independentes são os dois canais (0 = iguais, 1 = independentes)
            a = lfo.random (inc);
            const float other = lfoR.random (inc);
            b = a + clampf (tgt.modOffset * 2.0f, 0.0f, 1.0f) * (other - a);
        }
        else
        {
            a = LFO::shape (tgt.modShape, lfoPhase);
            b = LFO::shape (tgt.modShape, lfoPhase + tgt.modOffset);
        }
        // as leituras usam (atraso − mod); mantém sempre positivo somando a amplitude
        mL = modAmp * (1.0f + a);
        mR = modAmp * (1.0f + b);
        if (wowAmp > 0.0f)
        {
            wowPhase += 0.55f / sr; if (wowPhase >= 1.0f) wowPhase -= 1.0f;
            flutPhase += 6.3f / sr; if (flutPhase >= 1.0f) flutPhase -= 1.0f;
            const float w = 0.8f * std::sin (kTwoPi * wowPhase) + 0.2f * std::sin (kTwoPi * flutPhase) + 0.25f * wowLfo.random (2.0f / sr);
            mL += wowAmp * (1.0f + w);
            mR += wowAmp * (1.0f + w * 0.92f);
        }
    }

    // tempo de leitura pedido, descontando o atraso que os módulos do loop acrescentam
    float targetDelay (int c) const noexcept
    {
        const float cap = (float) dl[c].capacity() - 4.0f;
        const float compensationDiff = diffAmt > 0.0f ? 0.5f * diffSum * diffAmt : 0.0f;   // difusão
        const float osLatency = tgt.satHQ ? (float) Oversampler2x::kLatency : 0.0f;          // oversampling da saturação
        return clampf (msToSamples (c == 0 ? tgt.timeMsL : timeR()) - compensationDiff - osLatency, 4.0f, cap);
    }

    float readChannel (int c, float mod) noexcept
    {
        auto& h = heads[c];
        const float target = targetDelay (c);

        if (tgt.transition == Transition::TapeGlide)
        {
            // glide de fita: o tempo desliza (com alteração de afinação), sem cliques
            h.glide += glideK * (target - h.glide);
            h.a = h.b = h.glide; h.fading = false; h.xf = h.onB ? 1.0f : 0.0f;
            lastDelay[c] = h.glide;
            return dl[c].readHermite (h.glide + mod);
        }

        // crossfade digital: duas cabeças de leitura, afinação preservada.
        // Stretch: o tempo desliza como no glide, mas em pequenos passos com crossfade
        // (a afinação não muda — o eco "estica").
        const bool stretch = tgt.transition == Transition::Stretch;
        if (stretch) h.glide += glideK * (target - h.glide); else h.glide = target;
        const float want = h.glide;
        const float active = h.onB ? h.b : h.a;
        lastDelay[c] = active;
        if (! h.fading && std::fabs (want - active) > 0.5f)
        {
            if (h.onB) h.a = want; else h.b = want;
            h.fading = true;
        }
        if (! h.fading) return dl[c].readHermite (active + mod);

        const float step = 1.0f / ((stretch ? 0.022f : 0.045f) * sr);   // 22 ms por passo / 45 ms
        if (h.onB) { h.xf -= step; if (h.xf <= 0.0f) { h.xf = 0.0f; h.onB = false; h.fading = false; } }
        else       { h.xf += step; if (h.xf >= 1.0f) { h.xf = 1.0f; h.onB = true;  h.fading = false; } }
        const float w = 0.5f - 0.5f * std::cos (kPi * h.xf);
        return (1.0f - w) * dl[c].readHermite (h.a + mod) + w * dl[c].readHermite (h.b + mod);
    }

    // --------------------------------------------------- módulos do loop
    float stGrit (int c, float x) noexcept
    {
        if (! gritActive) return x;
        float y = x;
        if (gritInc < 0.999f)
        {
            const float aa = gritAA[c].process (x);
            if (c == 0)
            {
                gritPhase += gritInc * (1.0f + jitter * 0.5f * gritRng.next());
                gritTick = gritPhase >= 1.0f;
                if (gritTick) gritPhase -= std::floor (gritPhase);
            }
            if (gritTick) gritHold[c] = aa;
            y = gritHold[c];
        }
        if (bitsStep > 0.0f)
            y = bitsStep * std::floor (y / bitsStep + 0.5f);
        return x + gritW * (y - x);
    }
    bool gritTick = false;

    float stEq (int c, float x) noexcept
    {
        if (slope == 0) { x = lo1[c].hp (x); x = hi1[c].lp (x); }
        else
        {
            x = cutLo[c][0].process (x); x = cutHi[c][0].process (x);
            if (slope == 2) { x = cutLo[c][1].process (x); x = cutHi[c][1].process (x); }
        }
        x = peak[c].process (x);
        if (lsOn)  x = lsF[c].process (x);
        if (hsOn)  x = hsF[c].process (x);
        if (pk2On) x = pk2F[c].process (x);
        return x;
    }

    float stDisp (int c, float x) noexcept
    {
        if (dispW <= 0.0001f) { for (auto& f : disp[c]) f.process (x); return x; }   // mantém estado quente
        float y = x;
        for (auto& f : disp[c]) y = f.process (y);
        return x + dispW * (y - x);
    }

    float stDiff (int c, float x) noexcept
    {
        float y = x;
        for (auto& s : diff[c])
        {
            const float vd = s.line.readLinear (s.d);
            const float vv = y + kDiffG * vd;
            y = vd - kDiffG * vv;
            s.line.push (vv);
        }
        return x + diffAmt * (y - x);
    }

    float stTape (int c, float x) noexcept
    {
        const float e = emph[c].process (x);
        float s = e;
        if (tgt.satHQ)
        {
            // 2× sempre ligado em HQ (latência constante, já descontada no tempo)
            const float g = satGain, mixv = satMix;
            s = os[c].process (e, [g, mixv] (float u) { return u + mixv * (std::tanh (u * g) / g - u); });
        }
        else if (satMix > 0.0f)
        {
            const float g = satGain;
            const float t = sat[c].process (e * g) / g;
            s = e + satMix * (t - e);
        }
        else sat[c].x1 = e;
        return deemph[c].process (s);
    }

    float loopChain (int c, float x) noexcept
    {
        const auto& perms = loopPermutations();
        const auto& order = perms[(size_t) std::min ((int) perms.size() - 1, std::max (0, tgt.loopOrder))];
        for (auto m : order)
        {
            switch (m)
            {
                case 0:  x = stGrit (c, x); break;
                case 1:  x = stEq (c, x);   break;
                case 2:  x = stDisp (c, x); break;
                case 3:  x = stDiff (c, x); break;
                default: x = stTape (c, x); break;
            }
        }
        return x;
    }

    // ---------------------------------------------------------------- vaga C: taps
    struct TapRun { float d = 0, dTarget = 0, gl = 0, gr = 0, lpG = 1, ratio = 1, ph = 0, z[2] {}; bool pitched = false; };
    std::array<TapRun, TapSet::kMax> tapState {};

    void updateTaps() noexcept
    {
        const float cap = (float) tapDl[0].capacity() - 8.0f;
        const float qMs = 60000.0f / std::max (20.0f, tgt.bpm);
        for (int k = 0; k < taps.count; ++k)
        {
            const auto& t = taps.taps[(size_t) k];
            auto& r = tapState[(size_t) k];
            const float ms = tgt.tapsMode == TapsMode::Sculpt ? t.pos * tgt.timeMsL : t.pos * qMs;
            r.dTarget = clampf (msToSamples (ms), 4.0f, cap);
            if (r.d <= 0.0f) r.d = r.dTarget;
            const float a = (clampf (t.pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;
            r.gl = t.gain * std::cos (a) * 1.41421356f;
            r.gr = t.gain * std::sin (a) * 1.41421356f;
            const float hz = 200.0f * std::pow (100.0f, clampf (t.tone, 0.0f, 1.0f));     // 200 Hz … 20 kHz
            r.lpG = t.tone >= 0.999f ? 1.0f : 1.0f - std::exp (-kTwoPi * std::min (hz, 0.45f * sr) / sr);
            r.ratio = std::pow (2.0f, clampf (t.semis, -24.0f, 24.0f) / 12.0f);
            r.pitched = std::fabs (t.semis) > 0.01f;
        }
    }

    void renderTaps (float& outL, float& outR) noexcept
    {
        const float W = 0.06f * sr;                          // grão do harmonizador
        for (int k = 0; k < taps.count; ++k)
        {
            auto& r = tapState[(size_t) k];
            if (r.gl + r.gr < 1.0e-5f) continue;
            r.d += (r.dTarget - r.d) * 0.002f;                // muda de tempo sem cliques (≈10 ms)
            float l, rr;
            if (! r.pitched) { l = tapDl[0].readHermite (r.d); rr = tapDl[1].readHermite (r.d); }
            else
            {
                // harmonia: dois grãos que lêem à volta do tempo do tap com outra velocidade
                r.ph += (1.0f - r.ratio) / W; r.ph -= std::floor (r.ph);
                l = rr = 0;
                for (int g = 0; g < 2; ++g)
                {
                    float p = r.ph + 0.5f * (float) g; if (p >= 1.0f) p -= 1.0f;
                    const float w = std::sin (kPi * p), w2 = w * w;
                    const float dd = std::max (4.0f, r.d + (p - 0.5f) * W);   // centrado no tempo do tap
                    l += w2 * tapDl[0].readHermite (dd); rr += w2 * tapDl[1].readHermite (dd);
                }
            }
            if (r.lpG < 1.0f) { r.z[0] += r.lpG * (l - r.z[0]); r.z[1] += r.lpG * (rr - r.z[1]); l = r.z[0]; rr = r.z[1]; }
            outL += r.gl * l; outR += r.gr * rr;
        }
    }

    // ---------------------------------------------------------------- vaga B
    // multi-cabeça (eco de fita com 3 cabeças a 1/3, 2/3 e 3/3 do tempo)
    float multiHead (int c, float mod) const noexcept
    {
        float sum = 0; int n = 0;
        for (int k = 1; k <= 3; ++k)
            if ((tgt.headsMask >> (k - 1)) & 1) { sum += dl[c].readHermite (lastDelay[c] * (float) k / 3.0f + mod); ++n; }
        return n > 0 ? sum / (float) n : 0.0f;
    }

    // eco invertido: dois grãos que lêem para trás (janela seno², soma constante)
    float readReverse (int c, float mod) noexcept
    {
        const float cap = (float) dl[c].capacity() - 8.0f;
        const float S = clampf (lastDelay[c], 0.02f * sr, 0.5f * (cap - 8.0f));
        revPh[c] += 1.0f / S; if (revPh[c] >= 1.0f) revPh[c] -= 1.0f;
        float out = 0;
        for (int g = 0; g < 2; ++g)
        {
            float p = revPh[c] + 0.5f * (float) g; if (p >= 1.0f) p -= 1.0f;
            const float w = std::sin (kPi * p);
            out += w * w * dl[c].readHermite (4.0f + 2.0f * S * p + mod);
        }
        return out;
    }

    // pitch shifter de dois grãos (50 ms) dentro do loop: cada repetição sobe/desce de novo
    float pitchShift (int c, float x) noexcept
    {
        pitchBuf[c].push (x);
        const float W = 0.05f * sr;
        if (c == 0) { pitchPh += (1.0f - pitchRatio) / W; pitchPh -= std::floor (pitchPh); }
        float out = 0;
        for (int g = 0; g < 2; ++g)
        {
            float p = pitchPh + 0.5f * (float) g; if (p >= 1.0f) p -= 1.0f;
            const float w = std::sin (kPi * p);
            out += w * w * pitchBuf[c].readHermite (4.0f + p * W);
        }
        return out;
    }

    // modelos de carácter (filtros sempre a correr; transição suave entre modelos)
    float modelStage (int c, float x) noexcept
    {
        float y = x;
        { const float f = bbdB[c].process (bbdA[c].process (x)); const float t = std::tanh (1.3f * f) / 1.3f; y += modelW[1] * (t - x); }
        { const float f = mhLP[c].process (x); y += modelW[2] * (f - x); }
        {
            if (c == 0) casDrift = casLfo.random (0.7f / sr);
            const float f = casHP[c].process (casB[c].process (casA[c].process (x)));
            const float t = std::tanh (1.5f * f) / 1.5f * (1.0f + 0.06f * casDrift);
            y += modelW[3] * (t - x);
        }
        {
            const float f = digB[c].process (digA[c].process (x));
            const float q = (2.0f / 4096.0f) * std::floor (f * 2048.0f + 0.5f);   // 12 bits
            y += modelW[4] * (q - x);
        }
        return y;
    }

    // tape stop (pad): a fita abranda até parar; ao soltar, os ecos voltam com fade
    float tapeStop() noexcept
    {
        if (tgt.padStop)
        {
            stopping = true;
            stopSpeed = std::max (0.0f, stopSpeed - 1.0f / (0.8f * sr));
            stopExtra = std::min (stopExtra + (1.0f - stopSpeed), 3.0f * sr);
            stopGain = std::min (1.0f, stopGain + 1.0f / (0.01f * sr));
        }
        else if (stopping)
        {
            stopGain -= 1.0f / (0.02f * sr);
            if (stopGain <= 0.0f) { stopGain = 0.0f; stopExtra = 0.0f; stopSpeed = 1.0f; stopping = false; }
        }
        else stopGain = std::min (1.0f, stopGain + 1.0f / (0.03f * sr));
        return stopGain * (stopping ? std::sqrt (stopSpeed) : 1.0f);
    }

    // Auto-Throw: quando a frase acaba (nível abaixo do limiar durante GAP), escreve os últimos
    // LEN ms da entrada directamente na linha de atraso, no sítio onde teriam entrado.
    // O eco sai no tempo certo, como se o envio tivesse sido aberto antes da palavra.
    void autoThrow (float dryL, float dryR, bool frozen) noexcept
    {
        const int lenS = std::max (16, (int) (tgt.throwLenMs * 0.001f * sr));
        if (tgt.throwNow && ! throwPrev && ! frozen) retroThrow (1, lenS);    // throw manual: a última frase já
        throwPrev = tgt.throwNow;
        if (! tgt.throwAuto || frozen) { thrAbove = false; return; }
        const float in = std::max (std::fabs (dryL), std::fabs (dryR));
        thrEnv = in > thrEnv ? in : thrEnv * std::exp (-1.0f / (0.03f * sr));
        if (gainToDbFast (thrEnv) > tgt.throwThreshDb)
        {
            if (! thrAbove) { thrAbove = true; thrCount = 0; }
            ++thrCount; thrBelow = 0;
        }
        else if (thrAbove)
        {
            const int gapS = std::max (1, (int) (tgt.throwGapMs * 0.001f * sr));
            if (++thrBelow >= gapS)
            {
                if (thrCount >= (int) (0.08f * sr)) retroThrow (gapS, std::min (lenS, thrCount + gapS / 2));
                thrAbove = false;
            }
        }
    }

    void retroThrow (int startAgo, int len) noexcept
    {
        const int histCap = hist[0].capacity() - 4;
        len = std::min (len, histCap - startAgo);
        if (len <= 16) return;
        // o eco tem de cair à frente da cabeça de leitura: se o tempo for curto, desloca a janela
        const int D = (int) std::min (lastDelay[0], lastDelay[1]);
        const int shift = std::max (0, startAgo + len - (D - 32));
        const int fade = std::min (len / 4, (int) (0.005f * sr));
        for (int j = 0; j < len; ++j)
        {
            const int k = startAgo + j;           // amostras atrás na história (0 = actual)
            const int dk = k - 1 - shift;          // posição na linha de atraso (0 = última escrita)
            if (dk < 0) continue;
            float w = 1.0f;
            if (j < fade) w = (float) j / (float) fade;
            else if (j > len - fade) w = (float) (len - j) / (float) fade;
            dl[0].addAgo (dk, w * hist[0].ago (k));
            dl[1].addAgo (dk, w * hist[1].ago (k));
        }
        ++throws;
    }

    float safety (int c, float x) noexcept
    {
        // bloqueio DC (≈ 4 Hz)
        const float y = x - dcX[c] + dcR * dcY[c];
        dcX[c] = x; dcY[c] = y;
        const float a = std::fabs (y);
        if (a > loopLimitPeak) loopLimitPeak = a;
        float out = softLimit (y);
        if (std::fabs (out) < 1.0e-20f) out = 0.0f;   // sem denormais
        return out;
    }

    float ducker (float dL, float dR) noexcept
    {
        if (! tgt.duckOn)
        {
            duckGainDb += (0.0f - duckGainDb) * 0.002f;
            return dbToGainFast (duckGainDb);
        }
        const float in = std::max (std::fabs (dL), std::fabs (dR));
        const float rel = std::exp (-1.0f / (0.010f * sr));
        envDuck = in > envDuck ? in : envDuck * rel;                       // detector de pico
        const float lvl = gainToDbFast (envDuck);
        const float over = clampf ((lvl - tgt.duckThreshDb) / 6.0f, 0.0f, 1.0f);   // joelho de 6 dB
        const float target = -tgt.duckAmountDb * over;
        const float atk = 1.0f - std::exp (-1.0f / (0.001f * std::max (0.1f, tgt.duckAttackMs) * sr));
        const float relc = 1.0f - std::exp (-1.0f / (0.001f * std::max (5.0f, tgt.duckReleaseMs) * sr));
        if (target < duckGainDb) { duckGainDb += atk * (target - duckGainDb); holdCount = (int) (tgt.duckHoldMs * 0.001f * sr); }
        else if (holdCount > 0)  { --holdCount; }
        else                     { duckGainDb += relc * (target - duckGainDb); }
        return dbToGainFast (duckGainDb);
    }
    // Gate: abre (≈0,5 ms) quando a fonte passa o limiar, mantém HOLD e fecha em RELEASE.
    // Desligado, volta suavemente a 1 (sem cliques).
    float gate (float dL, float dR) noexcept
    {
        if (! tgt.gateOn)
        {
            gateG += (1.0f - gateG) * 0.003f;
            return gateG;
        }
        const float in = std::max (std::fabs (dL), std::fabs (dR));
        envGate = in > envGate ? in : envGate * std::exp (-1.0f / (0.005f * sr));
        if (gainToDbFast (envGate) > tgt.gateThreshDb)
        {
            gateHold = (int) (std::max (0.0f, tgt.gateHoldMs) * 0.001f * sr);
            gateG += (1.0f - gateG) * (1.0f - std::exp (-1.0f / (0.0005f * sr)));
        }
        else if (gateHold > 0) --gateHold;
        else gateG *= std::exp (-1.0f / (0.001f * std::max (2.0f, tgt.gateReleaseMs) * sr / 6.9f));   // −60 dB em RELEASE
        if (gateG < 1.0e-6f) gateG = 0.0f;
        return gateG;
    }
    float envGate = 0, gateG = 1;  int gateHold = 0;

    static float gainToDbFast (float g) noexcept { return 8.6858896f * std::log (std::max (g, 1.0e-6f)); }

    // ruído rosa (filtro de Paul Kellet, versão económica)
    float hissNoise (int c) noexcept
    {
        const float w = hissRng[c].next();
        auto& b = pink[c];
        b[0] = 0.99765f * b[0] + w * 0.0990460f;
        b[1] = 0.96300f * b[1] + w * 0.2965164f;
        b[2] = 0.57000f * b[2] + w * 1.0526913f;
        return (b[0] + b[1] + b[2] + w * 0.1848f) * 0.25f;
    }

    // Escuta: Normal · Só ecos · Delta (só o que o delay acrescenta, à mistura actual) ·
    //         Mono · Telemóvel (mono 400 Hz–3,5 kHz, ligeira saturação) · Clube (graves em mono)
    void monitorStage (float& oL, float& oR, float wL, float wR, float gw) noexcept
    {
        switch (tgt.monitor)
        {
            case Monitor::Normal: break;
            case Monitor::WetSolo: oL = wL; oR = wR; break;
            case Monitor::Delta:   oL = gw * wL; oR = gw * wR; break;
            case Monitor::Mono:    { const float m = 0.5f * (oL + oR); oL = oR = m; } break;
            case Monitor::Phone:
            {
                float m = 0.5f * (oL + oR);
                m = phoneLP[0].process (phoneLP[1].process (phoneHP[0].process (phoneHP[1].process (m))));
                m = std::tanh (1.6f * m) * 0.8f;
                oL = oR = m;
            } break;
            case Monitor::Club:
            {
                const float lowL = clubLP[0].process (oL), lowR = clubLP2[0].process (oR);
                const float lowM = 0.5f * (lowL + lowR) * 1.25f;
                oL = (oL - lowL) + lowM; oR = (oR - lowR) + lowM;
            } break;
        }
    }

    void emergencyReset() noexcept
    {
        ++safetyResets;
        const Params keep = tgt;
        reset();
        tgt = keep;
        snapToTargets();
    }
};

} // namespace delex
