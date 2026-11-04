// =============================================================================
//  DELEX by Piradex — análise da entrada (fora da thread de áudio)
//   · tom (cromagrama + perfis de Krumhansl-Kessler, 24 tonalidades)
//   · afinação da nota actual (YIN) — para o Harmonic Echo seguir a melodia
//  A thread de áudio só escreve amostras reduzidas (≈12 kHz) num FIFO.
// =============================================================================
#pragma once

#include <juce_dsp/juce_dsp.h>
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

        // janela mais recente, ordenada
        std::array<float, kWin> w {};
        double e = 0;
        for (int i = 0; i < kWin; ++i) { w[(size_t) i] = ring[(size_t) ((ringPos + i) & (kWin - 1))]; e += w[(size_t) i] * w[(size_t) i]; }
        const double rms = std::sqrt (e / kWin);
        if (rms < 0.003) { note.store (-1); return; }     // silêncio (< −50 dBFS)

        detectPitch (w);
        updateChroma (w);
    }

    std::atomic<int> key { -1 };            // 0…11 maior (C…B), 12…23 menor
    std::atomic<float> keyConfidence { 0 };
    std::atomic<int> note { -1 };           // nota MIDI actual (−1 = sem afinação clara)

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
    void addToRing (float x) { ring[(size_t) ringPos] = x; ringPos = (ringPos + 1) & (kWin - 1); }

    void updateChroma (const std::array<float, kWin>& w)
    {
        std::array<float, kWin * 2> buf {};
        for (int i = 0; i < kWin; ++i)
            buf[(size_t) i] = w[(size_t) i] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (kWin - 1)));
        fft.performFrequencyOnlyForwardTransform (buf.data());
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
        note.store ((int) std::lround (69.0 + 12.0 * std::log2 (f / 440.0)));
    }

    int decim = 4; double rate = 12000.0;
    float acc = 0; int accN = 0;
    std::array<float, 256> tmp {}; int tmpN = 0;
    juce::AbstractFifo fifo { kFifo };
    std::array<float, kFifo> data {};
    std::array<float, kWin> ring {}; int ringPos = 0, newSamples = 0;
    std::array<float, 12> chroma {};
    juce::dsp::FFT fft { kOrder };
};
} // namespace delex
