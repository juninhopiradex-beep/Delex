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
enum class Transition { TapeGlide = 0, Crossfade = 1 };
enum class ModShape   { Sine = 0, Triangle = 1, Random = 2 };
enum class CutSlope   { dB6 = 0, dB12 = 1, dB24 = 2 };

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

    // Dispersão / difusão
    float dispHz = 500.0f, dispSpreadMs = 0.0f;
    float diffAmount = 0.0f, diffSpreadMs = 10.0f;

    // Fita
    float tapeWarmth = 0.0f, tapeDrive = 0.0f;

    // Digital grit
    float gritBits = 24.0f, gritRateHz = 192000.0f, gritJitter = 0.0f, gritMix = 1.0f;

    // Ordem do loop (0…3)
    int loopOrder = 0;

    // Ganhos
    float preDriveDb = 0.0f, wetTrimDb = 0.0f, outTrimDb = 0.0f;

    // Ducker (atua no wet)
    bool  duckOn = false;
    float duckThreshDb = -24.0f, duckAmountDb = 9.0f;
    float duckAttackMs = 5.0f, duckHoldMs = 50.0f, duckReleaseMs = 250.0f;

    // Mistura
    float mix = 0.25f;  bool equalPower = true;
    bool  bypass = false;
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
        reset();
    }

    void reset()
    {
        for (auto& d : dl) d.clear();
        for (auto& ch : diff) for (auto& s : ch) s.line.clear();
        for (int c = 0; c < 2; ++c)
        {
            for (auto& f : cutLo[c]) f.reset();
            for (auto& f : cutHi[c]) f.reset();
            lo1[c].reset(); hi1[c].reset(); peak[c].reset();
            for (auto& f : disp[c]) f.reset();
            emph[c].reset(); deemph[c].reset(); sat[c].reset();
            gritAA[c].reset(); gritHold[c] = 0; dcX[c] = dcY[c] = 0;
            monoLP[c].reset();
            r[c] = 0;
        }
        gritPhase = 0;
        lfo = LFO{}; lfoR = LFO{}; wowLfo = LFO{};
        lfoR.rng.s = 0x2545F491u; wowLfo.rng.s = 0x6C8E9CF5u;   // sementes diferentes por gerador
        lfoPhase = wowPhase = flutPhase = 0;
        envDuck = 0; duckGainDb = 0; holdCount = 0;
        haveTargets = false;
        loopLimitPeak = 0;
    }

    void setParams (const Params& p) noexcept { tgt = p; }

    // ------------------------------------------------------------ medição
    float duckReductionDb() const noexcept { return -duckGainDb; }   // ≥ 0
    float loopLimiterDb() const noexcept   { return loopLimitDbOut; }
    int   resets() const noexcept          { return safetyResets; }

    // ------------------------------------------------------------ processo
    void process (float* L, float* R, int n) noexcept
    {
        if (! haveTargets) snapToTargets();
        loopLimitPeak = 0;

        for (int i = 0; i < n; ++i)
        {
            if ((ctl++ & 15) == 0) updateControl();

            const float dryL = L[i], dryR = R[i];

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
            iL *= preG * inGate;  iR *= preG * inGate;

            // --- leitura das linhas de atraso (com modulação)
            float modL = 0, modR = 0;
            modulation (modL, modR);
            const float dL = readChannel (0, modL);
            const float dR = readChannel (1, modR);

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

            const float procL = loopChain (0, xL), procR = loopChain (1, xR);
            xL += (1.0f - frz) * (procL - xL);    // em freeze, loop sem perdas
            xR += (1.0f - frz) * (procR - xR);

            xL = safety (0, xL); xR = safety (1, xR);
            if (! finite (xL) || ! finite (xR)) { emergencyReset(); xL = xR = 0; yL = yR = 0; }

            dl[0].push (xL); dl[1].push (xR);

            // --- wet: ducker, rotação/largura de saída
            float wL = yL, wR = yR;
            const float duckG = ducker (dryL, dryR);
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
            const float mk = sm.makeup.next (makeup);   // ganho de compensação da saturação (fora do loop)
            wL *= wetG * mk; wR *= wetG * mk;

            // --- mistura
            float gd, gw;
            if (tgt.equalPower) { gd = std::cos (mixS * 0.5f * kPi); gw = std::sin (mixS * 0.5f * kPi); }
            else                { gd = 1.0f - mixS; gw = mixS; }
            float oL = (gd * dryL + gw * wL) * outG;
            float oR = (gd * dryR + gw * wR) * outG;

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
        return dbToGain (-std::max (0.0f, p.peakDb) * p.compensate);
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
                ma, mb, mc, md, ia, ib, ic, id, oa, ob, oc, od, balL, balR, pingpong, mono, makeup;
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
        for (int c = 0; c < 2; ++c)
        {
            const float t = msToSamples (c == 0 ? tgt.timeMsL : timeR());
            heads[c].a = heads[c].b = heads[c].glide = clampf (t, 4.0f, (float) dl[c].capacity());
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
                             &sm.oa, &sm.ob, &sm.oc, &sm.od, &sm.balL, &sm.balR, &sm.pingpong, &sm.mono, &sm.makeup })
                s->setTime (25.0f, sr);
            sm.bypass.setTime (15.0f, sr);
            sm.freeze.setTime (60.0f, sr);
            for (auto* s : { &cLo, &cHi, &cPk, &cPkDb, &cPkQ, &cDispHz, &cDispQ, &cDispW, &cDiffAmt, &cDiffSpread,
                             &cWarm, &cDrive, &cBits, &cRate, &cGritMix, &cJit, &cModAmpL, &cModAmpR, &cModRate })
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

        // --- modulação: amplitude (s) para obter o desvio de afinação pedido
        const float rateHz = v (cModRate, clampf (tgt.modRateHz, 0.01f, 20.0f));
        const float ratio = 1.0f - std::pow (2.0f, -clampf (tgt.modDepthCents, 0.0f, 100.0f) / 1200.0f);
        float ampS = tgt.modShape == ModShape::Triangle ? ratio / (4.0f * rateHz) : ratio / (kTwoPi * rateHz);
        ampS = std::min (ampS, 0.025f);
        modAmp = v (cModAmpL, ampS * sr);
        modRate = rateHz;
        wowAmp = v (cModAmpR, tgt.wow * 0.0016f * sr);   // até ~1.6 ms (wow lento + flutter)
    }

    bool gritActive = false;
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

    float readChannel (int c, float mod) noexcept
    {
        auto& h = heads[c];
        const float cap = (float) dl[c].capacity() - 4.0f;
        const float compensationDiff = diffAmt > 0.0f ? 0.5f * diffSum * diffAmt : 0.0f;   // difusão adiciona atraso no loop
        const float target = clampf (msToSamples (c == 0 ? tgt.timeMsL : timeR()) - compensationDiff, 4.0f, cap);

        if (tgt.transition == Transition::TapeGlide)
        {
            // glide de fita: o tempo desliza (com alteração de afinação), sem cliques
            h.glide += glideK * (target - h.glide);
            h.a = h.b = h.glide; h.fading = false; h.xf = h.onB ? 1.0f : 0.0f;
            return dl[c].readHermite (h.glide + mod);
        }

        // crossfade digital: duas cabeças de leitura, afinação preservada
        h.glide = target;
        const float active = h.onB ? h.b : h.a;
        if (! h.fading && std::fabs (target - active) > 0.5f)
        {
            if (h.onB) h.a = target; else h.b = target;
            h.fading = true;
        }
        if (! h.fading) return dl[c].readHermite (active + mod);

        const float step = 1.0f / (0.045f * sr);   // 45 ms
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
        return peak[c].process (x);
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
        if (satMix > 0.0f)
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
        switch (tgt.loopOrder)
        {
            case 1:  x = stEq (c, x);   x = stTape (c, x); x = stDiff (c, x); x = stDisp (c, x); x = stGrit (c, x); break;
            case 2:  x = stTape (c, x); x = stEq (c, x);   x = stDiff (c, x); x = stDisp (c, x); x = stGrit (c, x); break;
            case 3:  x = stDiff (c, x); x = stDisp (c, x); x = stEq (c, x);   x = stTape (c, x); x = stGrit (c, x); break;
            default: x = stGrit (c, x); x = stEq (c, x);   x = stDisp (c, x); x = stDiff (c, x); x = stTape (c, x); break;
        }
        return x;
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
    static float gainToDbFast (float g) noexcept { return 8.6858896f * std::log (std::max (g, 1.0e-6f)); }

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
