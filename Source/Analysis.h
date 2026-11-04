// =============================================================================
//  DELEX by Piradex — análise da entrada (fora da thread de áudio)
//   · tom (cromagrama + perfis de Krumhansl-Kessler, 24 tonalidades)
//   · afinação da nota actual (YIN) — para o Harmonic Echo seguir a melodia
//   · tempo da entrada (envelope de ataques + autocorrelação) — Tempo Follow
//   · tipo de fonte (voz, bateria, baixo, guitarra/teclas, pad, mistura) — estimativa
//  A thread de áudio só escreve amostras reduzidas (≈12 kHz) num FIFO.
// =============================================================================
#pragma once

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace delex
{
class InputAnalyser
{
public:
    static constexpr int kFifo = 1 << 15, kWin = 4096, kOrder = 12;

    void prepare (double sampleRate)
    {
        decim = std::max (1, (int) std::lround (sampleRate / 12000.0));
        rate = sampleRate / decim;
        acc = 0; accN = 0;
        fifo.reset();
        reset();
    }
    void reset()
    {
        chroma.fill (0.0f); ring.fill (0.0f); ringPos = 0;
        key.store (-1); keyConfidence.store (0.0f); note.store (-1);
        onsetEnv.fill (0.0f); onsetPos = 0; hopAcc = 0; hopN = 0; prevLogE = -100.0f;
        followBpm.store (0.0f); bpmConfidence.store (0.0f); bpmCandidate = 0; bpmAgree = 0;
        source.store (-1); sourceConfidence.store (0.0f);
        fLow = fCent = fFlat = fPitched = fRate = fVib = 0; f0Hist.fill (0.0f); f0N = 0;
    }

    // thread de áudio: mono, sem alocações
    void push (const float* l, const float* r, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            acc += 0.5f * (l[i] + r[i]);
            if (++accN >= decim)
            {
                tmp[(size_t) tmpN++] = acc / (float) decim; acc = 0; accN = 0;
                if (tmpN == (int) tmp.size()) flush();
            }
        }
        flush();
    }

    // thread de mensagens (timer): consome o FIFO e actualiza tom e nota
    void analyse()
    {
        int s1, n1, s2, n2;
        const int ready = fifo.getNumReady();
        if (ready <= 0) return;
        fifo.prepareToRead (ready, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) addToRing (data[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) addToRing (data[(size_t) (s2 + i)]);
        fifo.finishedRead (n1 + n2);
        newSamples += n1 + n2;
        if (newSamples < (int) (rate * 0.1)) return;     // ~10 análises por segundo
        newSamples = 0;
        updateTempo();

        // janela mais recente, ordenada
        std::array<float, kWin> w {};
        double e = 0;
        for (int i = 0; i < kWin; ++i) { w[(size_t) i] = ring[(size_t) ((ringPos + i) & (kWin - 1))]; e += w[(size_t) i] * w[(size_t) i]; }
        const double rms = std::sqrt (e / kWin);
        if (rms < 0.003) { note.store (-1); return; }     // silêncio (< −50 dBFS)

        detectPitch (w);
        updateChroma (w);
        updateSource();
    }

    std::atomic<int> key { -1 };            // 0…11 maior (C…B), 12…23 menor
    std::atomic<float> keyConfidence { 0 };
    std::atomic<int> note { -1 };           // nota MIDI actual (−1 = sem afinação clara)
    std::atomic<float> followBpm { 0 };     // tempo seguido (0 = ainda sem certeza)
    std::atomic<float> bpmConfidence { 0 };
    enum Source { Vocal = 0, Drums, Bass, GuitarKeys, Pad, FullMix, kSources };
    std::atomic<int> source { -1 };
    std::atomic<float> sourceConfidence { 0 };

    static juce::String sourceName (int s, bool pt)
    {
        static const char* p[kSources] = { "Voz", "Bateria / percussão", "Baixo / 808", "Guitarra / teclas", "Pad / sintetizador", "Mistura completa" };
        static const char* e[kSources] = { "Vocal", "Drums / percussion", "Bass / 808", "Guitar / keys", "Pad / synth", "Full mix" };
        return s < 0 ? juce::String ("-") : juce::String::fromUTF8 ((pt ? p : e)[s]);
    }
    // categoria de presets que combina com cada fonte
    static const char* sourceCategory (int s)
    {
        static const char* c[kSources] = { "Vocals", "Drums", "Bass", "Guitar", "Synth", "Master FX" };
        return s < 0 ? "" : c[s];
    }

    static juce::String keyName (int k, bool portuguese)
    {
        if (k < 0) return "-";
        static const char* en[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        static const char* pt[12] = { "Dó", "Dó#", "Ré", "Mib", "Mi", "Fá", "Fá#", "Sol", "Láb", "Lá", "Sib", "Si" };
        const bool minor = k >= 12;
        const auto n = juce::String::fromUTF8 ((portuguese ? pt : en)[k % 12]);
        return portuguese ? n + (minor ? " menor" : " maior") : n + (minor ? " minor" : " major");
    }

private:
    void flush() noexcept
    {
        if (tmpN == 0) return;
        int s1, n1, s2, n2;
        const int n = std::min (tmpN, fifo.getFreeSpace());
        fifo.prepareToWrite (n, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) data[(size_t) (s1 + i)] = tmp[(size_t) i];
        for (int i = 0; i < n2; ++i) data[(size_t) (s2 + i)] = tmp[(size_t) (n1 + i)];
        fifo.finishedWrite (n1 + n2);
        tmpN = 0;
    }
    void addToRing (float x)
    {
        ring[(size_t) ringPos] = x; ringPos = (ringPos + 1) & (kWin - 1);
        // envelope de ataques: fluxo de energia em saltos de 10 ms
        hopAcc += x * x;
        if (++hopN >= (int) (rate * 0.01))
        {
            const float logE = 10.0f * std::log10 (hopAcc / (float) hopN + 1.0e-10f);
            onsetEnv[(size_t) onsetPos] = std::max (0.0f, logE - prevLogE) * (logE > -55.0f ? 1.0f : 0.0f);
            onsetPos = (onsetPos + 1) % kOnset;
            prevLogE = logE; hopAcc = 0; hopN = 0;
        }
    }

    // Tempo Follow: autocorrelação do envelope de ataques (6 s), 70–180 BPM, com peso no dobro do período
    void updateTempo()
    {
        std::array<float, kOnset> o {};
        float mean = 0;
        for (int i = 0; i < kOnset; ++i) { o[(size_t) i] = onsetEnv[(size_t) ((onsetPos + i) % kOnset)]; mean += o[(size_t) i]; }
        mean /= kOnset;
        if (mean < 0.05f) return;                         // sem ataques suficientes
        for (auto& x : o) x -= mean;
        auto ac = [&] (int lag) { float r = 0; for (int i = lag; i < kOnset; ++i) r += o[(size_t) i] * o[(size_t) i - lag]; return r / (float) (kOnset - lag); };
        float r0 = ac (0); if (r0 <= 0) return;
        int best = -1; float bv = -1e9f; std::array<float, 200> sc {};
        for (int L = 33; L <= 86; ++L)
        {
            sc[(size_t) L] = ac (L) + 0.5f * ac (2 * L) + 0.25f * ac (L / 2);
            if (sc[(size_t) L] > bv) { bv = sc[(size_t) L]; best = L; }
        }
        if (best < 0) return;
        float lag = (float) best;
        if (best > 33 && best < 86)
        {
            const float a = sc[(size_t) best - 1], b = sc[(size_t) best], c = sc[(size_t) best + 1], den = a - 2 * b + c;
            if (std::abs (den) > 1e-9f) lag += 0.5f * (a - c) / den;
        }
        const float bpm = 6000.0f / lag;
        const float conf = juce::jlimit (0.0f, 1.0f, bv / r0);
        if (bpmCandidate > 0 && std::abs (bpm - bpmCandidate) / bpmCandidate < 0.03f) ++bpmAgree; else { bpmAgree = 0; bpmCandidate = bpm; }
        bpmCandidate = 0.7f * bpmCandidate + 0.3f * bpm;
        if (bpmAgree >= 3 && conf > 0.15f) { followBpm.store (bpmCandidate); bpmConfidence.store (conf); }
    }

    // Tipo de fonte: regras simples sobre graves, brilho, ruído, afinação, vibrato e ataques
    void updateSource()
    {
        // taxa de ataques nos últimos 4 s
        int peaks = 0;
        for (int i = 1; i < 400; ++i)
        {
            const float a = onsetEnv[(size_t) ((onsetPos - i + kOnset) % kOnset)], b = onsetEnv[(size_t) ((onsetPos - i - 1 + kOnset) % kOnset)];
            if (a > 4.0f && b <= 4.0f) ++peaks;
        }
        const float rateHz = (float) peaks / 4.0f;
        auto ema = [] (float& f, float v) { f += 0.12f * (v - f); };
        ema (fLow, lastLow); ema (fCent, lastCent); ema (fFlat, lastFlat); ema (fPitched, note.load() >= 0 ? 1.0f : 0.0f); ema (fRate, rateHz);
        // vibrato: desvio (cents) da afinação nos últimos ~1 s
        if (f0N >= 6)
        {
            float m = 0; for (int i = 0; i < f0N; ++i) m += f0Hist[(size_t) i]; m /= (float) f0N;
            float v = 0; for (int i = 0; i < f0N; ++i) v += (f0Hist[(size_t) i] - m) * (f0Hist[(size_t) i] - m);
            ema (fVib, std::sqrt (v / (float) f0N) * 100.0f);
        }
        auto ramp = [] (float x, float a, float b) { return juce::jlimit (0.0f, 1.0f, (x - a) / (b - a)); };
        float sc[kSources];
        sc[Bass]       = ramp (fLow, 0.45f, 0.75f);
        sc[Drums]      = ramp (fFlat, 0.12f, 0.35f) * (1.0f - fPitched) * ramp (fRate, 0.8f, 3.0f);
        sc[Vocal]      = fPitched * ramp (fVib, 6.0f, 20.0f) * (1.0f - ramp (fLow, 0.3f, 0.6f)) * (1.0f - ramp (fCent, 3500.0f, 6000.0f));
        sc[GuitarKeys] = fPitched * (1.0f - ramp (fVib, 6.0f, 20.0f)) * ramp (fRate, 0.4f, 1.2f) * (1.0f - ramp (fLow, 0.3f, 0.6f));
        sc[Pad]        = ramp (fPitched, 0.3f, 0.8f) * (1.0f - ramp (fVib, 8.0f, 25.0f)) * (1.0f - ramp (fRate, 0.3f, 1.0f)) * (1.0f - ramp (fLow, 0.4f, 0.7f));
        sc[FullMix]    = ramp (fRate, 1.0f, 3.0f) * ramp (fLow, 0.12f, 0.3f) * (1.0f - ramp (fLow, 0.55f, 0.75f)) * (1.0f - std::abs (fPitched - 0.5f) * 1.2f) * ramp (fFlat, 0.05f, 0.2f);
        int best = 0; float sum = 0;
        for (int i = 0; i < kSources; ++i) { sc[i] = std::max (0.0f, sc[i]); sum += sc[i]; if (sc[i] > sc[best]) best = i; }
        if (sum < 0.05f) return;
        source.store (best); sourceConfidence.store (sc[best] / sum);
    }

    void updateChroma (const std::array<float, kWin>& w)
    {
        std::array<float, kWin * 2> buf {};
        for (int i = 0; i < kWin; ++i)
            buf[(size_t) i] = w[(size_t) i] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (kWin - 1)));
        fft.performFrequencyOnlyForwardTransform (buf.data());
        {
            double tot = 0, low = 0, cent = 0, logSum = 0; int nb = 0;
            for (int b = 1; b < kWin / 2; ++b)
            {
                const double f = b * rate / kWin, pw = (double) buf[(size_t) b] * buf[(size_t) b];
                if (f < 30.0 || f > 5500.0) continue;
                tot += pw; cent += pw * f; if (f < 180.0) low += pw;
                if (f > 100.0) { logSum += std::log (pw + 1e-12); ++nb; }
            }
            if (tot > 0)
            {
                lastLow = (float) (low / tot); lastCent = (float) (cent / tot);
                double am = 0; int n2 = 0;
                for (int b = 1; b < kWin / 2; ++b) { const double f = b * rate / kWin; if (f > 100.0 && f <= 5500.0) { am += (double) buf[(size_t) b] * buf[(size_t) b]; ++n2; } }
                am /= std::max (1, n2);
                lastFlat = nb > 0 && am > 0 ? (float) (std::exp (logSum / nb) / am) : 0.0f;
            }
        }
        std::array<float, 12> c {};
        for (int b = 1; b < kWin / 2; ++b)
        {
            const double f = b * rate / kWin;
            if (f < 55.0 || f > 2000.0) continue;
            const double midi = 69.0 + 12.0 * std::log2 (f / 440.0);
            const int pc = ((int) std::lround (midi) % 12 + 12) % 12;
            c[(size_t) pc] += buf[(size_t) b] * buf[(size_t) b];
        }
        float sum = 0; for (float v : c) sum += v;
        if (sum <= 0) return;
        for (int i = 0; i < 12; ++i) chroma[(size_t) i] = 0.985f * chroma[(size_t) i] + c[(size_t) i] / sum;

        // perfis de Krumhansl-Kessler
        static const float maj[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
        static const float min[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
        auto corr = [&] (const float* prof, int root)
        {
            float mx = 0, my = 0;
            for (int i = 0; i < 12; ++i) { mx += chroma[(size_t) i]; my += prof[i]; }
            mx /= 12; my /= 12;
            float sxy = 0, sxx = 0, syy = 0;
            for (int i = 0; i < 12; ++i)
            {
                const float x = chroma[(size_t) ((i + root) % 12)] - mx, y = prof[i] - my;
                sxy += x * y; sxx += x * x; syy += y * y;
            }
            return sxx > 0 ? sxy / std::sqrt (sxx * syy) : 0.0f;
        };
        int best = -1; float b1 = -2, b2 = -2;
        for (int k = 0; k < 24; ++k)
        {
            const float r = corr (k < 12 ? maj : min, k % 12);
            if (r > b1) { b2 = b1; b1 = r; best = k; } else if (r > b2) b2 = r;
        }
        key.store (best);
        keyConfidence.store (juce::jlimit (0.0f, 1.0f, (b1 - b2) * 8.0f + 0.3f * b1));
    }

    // YIN (de Cheveigné e Kawahara), 70–1000 Hz, últimos 2048 pontos
    void detectPitch (const std::array<float, kWin>& w)
    {
        constexpr int N = 2048;
        const int tauMin = (int) (rate / 1000.0), tauMax = std::min (N / 2, (int) (rate / 70.0));
        const float* x = w.data() + (kWin - N);
        std::array<float, N / 2 + 1> d {};
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0;
            for (int i = 0; i < N / 2; ++i) { const float df = x[i] - x[i + tau]; s += df * df; }
            d[(size_t) tau] = s;
        }
        float run = 0; d[0] = 1;
        for (int tau = 1; tau <= tauMax; ++tau) { run += d[(size_t) tau]; d[(size_t) tau] = run > 0 ? d[(size_t) tau] * (float) tau / run : 1.0f; }
        int t = -1;
        for (int tau = std::max (2, tauMin); tau < tauMax; ++tau)
            if (d[(size_t) tau] < 0.15f) { while (tau + 1 < tauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau; t = tau; break; }
        if (t < 0) { note.store (-1); return; }
        const float a = d[(size_t) t - 1], b = d[(size_t) t], c = d[(size_t) t + 1];
        const float den = a - 2 * b + c;
        const float tt = (float) t + (std::abs (den) > 1e-9f ? 0.5f * (a - c) / den : 0.0f);
        const double f = rate / tt;
        const double midi = 69.0 + 12.0 * std::log2 (f / 440.0);
        note.store ((int) std::lround (midi));
        // histórico curto da afinação (para o vibrato); reinicia quando a nota muda muito
        if (f0N > 0 && std::abs (midi - f0Hist[(size_t) (f0N - 1)]) > 0.8) f0N = 0;
        if (f0N == (int) f0Hist.size()) { std::rotate (f0Hist.begin(), f0Hist.begin() + 1, f0Hist.end()); --f0N; }
        f0Hist[(size_t) f0N++] = (float) midi;
    }

    int decim = 4; double rate = 12000.0;
    float acc = 0; int accN = 0;
    std::array<float, 256> tmp {}; int tmpN = 0;
    juce::AbstractFifo fifo { kFifo };
    std::array<float, kFifo> data {};
    std::array<float, kWin> ring {}; int ringPos = 0, newSamples = 0;
    std::array<float, 12> chroma {};
    juce::dsp::FFT fft { kOrder };
    static constexpr int kOnset = 600;              // 6 s de envelope de ataques (10 ms)
    std::array<float, kOnset> onsetEnv {}; int onsetPos = 0, hopN = 0;
    float hopAcc = 0, prevLogE = -100.0f, bpmCandidate = 0; int bpmAgree = 0;
    float lastLow = 0, lastCent = 0, lastFlat = 0;
    float fLow = 0, fCent = 0, fFlat = 0, fPitched = 0, fRate = 0, fVib = 0;
    std::array<float, 10> f0Hist {}; int f0N = 0;
};
} // namespace delex
