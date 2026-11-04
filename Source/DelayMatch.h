// =============================================================================
//  DELEX by Piradex — Delay Match
//  Ouve um ficheiro (uma referência, ou melhor ainda uma voz isolada, por exemplo
//  um stem de voz do STEMS4U) e estima o delay que lá está: tempo, feedback,
//  brilho dos ecos, largura e se é ping-pong. Depois o DELEX copia essas definições.
//  Trabalha sobre o envelope (5 ms) — rápido mesmo em ficheiros longos.
// =============================================================================
#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <algorithm>
#include <cmath>

namespace delex
{
struct MatchResult
{
    bool  ok = false;
    float timeMs = 0, feedback = 0, tone = 0, widthPct = 100, confidence = 0;
    bool  pingPong = false;
    juce::String error;
};

class DelayMatch
{
public:
    static constexpr double kHop = 0.005;   // 5 ms

    static MatchResult analyse (const juce::AudioBuffer<float>& in, double sr)
    {
        MatchResult r;
        const int n = in.getNumSamples();
        if (n < (int) (sr * 3.0)) { r.error = "short"; return r; }
        const int hop = std::max (1, (int) std::lround (sr * kHop));
        const int frames = n / hop;
        const bool stereo = in.getNumChannels() > 1;
        const float* L = in.getReadPointer (0);
        const float* R = stereo ? in.getReadPointer (1) : L;

        std::vector<float> a ((size_t) frames), aL ((size_t) frames), aR ((size_t) frames), hi ((size_t) frames), db ((size_t) frames);
        std::vector<float> xyF ((size_t) frames), xxF ((size_t) frames), yyF ((size_t) frames);
        float hpz = 0, hpPrev = 0;
        const float hpC = std::exp (-2.0f * juce::MathConstants<float>::pi * 3000.0f / (float) sr);
        for (int f = 0; f < frames; ++f)
        {
            double s = 0, sl = 0, sr2 = 0, sh = 0, xy = 0;
            for (int i = f * hop; i < (f + 1) * hop; ++i)
            {
                const float m = 0.5f * (L[i] + R[i]);
                s += m * m; sl += L[i] * L[i]; sr2 += R[i] * R[i]; xy += L[i] * R[i];
                hpz = hpC * (hpz + m - hpPrev); hpPrev = m;           // passa-alto de 1.ª ordem (3 kHz)
                sh += hpz * hpz;
            }
            a[(size_t) f] = (float) std::sqrt (s / hop);
            aL[(size_t) f] = (float) std::sqrt (sl / hop); aR[(size_t) f] = (float) std::sqrt (sr2 / hop);
            hi[(size_t) f] = (float) (sh / hop);
            db[(size_t) f] = 10.0f * std::log10 ((float) (s / hop) + 1.0e-10f);
            xyF[(size_t) f] = (float) xy; xxF[(size_t) f] = (float) sl; yyF[(size_t) f] = (float) sr2;
        }

        // percentis de nível (caudas = partes calmas)
        std::vector<float> sorted (db); std::sort (sorted.begin(), sorted.end());
        const float p40 = sorted[(size_t) (0.40 * (frames - 1))], p85 = sorted[(size_t) (0.85 * (frames - 1))];
        const float floorDb = std::max (-70.0f, sorted[(size_t) (0.05 * (frames - 1))] + 3.0f);
        if (p85 < -60.0f) { r.error = "silent"; return r; }

        // TEMPO: o eco é uma cópia do mesmo espectro ao fim de T (o ritmo das frases não é).
        // Espectrograma em 40 bandas (100 Hz – 5 kHz, 5 ms), log, sem média por banda e por frame;
        // a correlação entre frames separados por L dá um pico nítido em T.
        const auto spec = bandSpectrogram (L, R, n, sr, hop, frames);
        const int nb = kBands;
        const int lagMin = (int) (0.04 / kHop), lagMax = std::min (frames / 3, (int) (2.0 / kHop));
        std::vector<float> rr ((size_t) lagMax + 2, 0.0f);
        for (int lag = lagMin - 1; lag <= lagMax + 1 && lag < frames; ++lag)
        {
            double s = 0, e1 = 0, e2 = 0;
            for (int f = lag; f < frames; ++f)
            {
                const float* x = &spec[(size_t) f * nb]; const float* y = &spec[(size_t) (f - lag) * nb];
                for (int k = 0; k < nb; ++k) { s += x[k] * y[k]; e1 += x[k] * x[k]; e2 += y[k] * y[k]; }
            }
            if (lag >= 0 && lag < (int) rr.size()) rr[(size_t) lag] = (float) (s / std::sqrt (e1 * e2 + 1e-20));
        }
        int best = -1; float bv = 0;
        for (int lag = lagMin; lag < lagMax; ++lag)
            if (rr[(size_t) lag] > rr[(size_t) lag - 1] && rr[(size_t) lag] >= rr[(size_t) lag + 1] && rr[(size_t) lag] > bv) { bv = rr[(size_t) lag]; best = lag; }
        if (best < 0 || bv < 0.05f) { r.error = "noecho"; r.confidence = bv; return r; }
        // o pico mais forte pode ser um múltiplo de T: prefere o período mais curto com pico quase igual
        for (int div = 4; div >= 2; --div)
        {
            const int cand = (int) std::lround ((double) best / div);
            if (cand < lagMin) continue;
            float m = 0; int at = cand;
            for (int k = std::max (lagMin, cand - 2); k <= std::min (lagMax - 1, cand + 2); ++k) if (rr[(size_t) k] > m) { m = rr[(size_t) k]; at = k; }
            if (m > 0.75f * bv) { best = at; bv = m; break; }
        }
        float lag = (float) best;
        {
            const float y0 = rr[(size_t) best - 1], y1 = rr[(size_t) best], y2 = rr[(size_t) best + 1], den = y0 - 2 * y1 + y2;
            if (std::abs (den) > 1e-9f) lag += juce::jlimit (-0.5f, 0.5f, 0.5f * (y0 - y2) / den);
        }
        r.timeMs = (float) (lag * kHop * 1000.0);

        // FEEDBACK: para cada pico forte, quanto da sua energia volta ao fim de T.
        // Outros sons por cima só aumentam a razão, por isso usa-se o quartil inferior.
        auto ac = [&] (const std::vector<float>& x, const std::vector<float>& y, int lg)
        {
            if (lg <= 0 || lg >= frames - 8) return 0.0f;
            double s = 0, e1 = 0, e2 = 0;
            for (int f = lg; f < frames; ++f) { s += x[(size_t) f] * y[(size_t) f - lg]; e1 += x[(size_t) f] * x[(size_t) f]; e2 += y[(size_t) f - lg] * y[(size_t) f - lg]; }
            return (float) (s / std::sqrt (e1 * e2 + 1e-20));
        };
        auto peakNear = [&] (const std::vector<float>& x, const std::vector<float>& y, float at)
        {
            float m = 0; const int c = (int) std::lround (at);
            for (int k = c - 2; k <= c + 2; ++k) m = std::max (m, ac (x, y, k));
            return m;
        };
        {
            const int li = (int) std::lround (lag);
            const float thr = std::pow (10.0f, p85 / 10.0f);
            std::vector<float> ratios;
            for (int f = 4; f + li + 3 < frames; ++f)
            {
                const float pf = a[(size_t) f] * a[(size_t) f];
                if (pf < thr) continue;
                bool isPeak = true;
                for (int k = -4; k <= 4 && isPeak; ++k) if (k != 0 && a[(size_t) (f + k)] > a[(size_t) f]) isPeak = false;
                if (! isPeak) continue;
                float m = 0; for (int k = -2; k <= 2; ++k) m = std::max (m, a[(size_t) (f + li + k)]);
                ratios.push_back (m / a[(size_t) f]);
            }
            if (ratios.size() >= 6)
            {
                std::sort (ratios.begin(), ratios.end());
                r.feedback = juce::jlimit (0.0f, 0.95f, ratios[ratios.size() / 4]);
            }
            else r.feedback = 0.35f;
        }
        // brilho: fracção de agudos nas caudas (partes calmas) vs nas partes fortes
        double hiT = 0, enT = 0, hiL = 0, enL = 0, xy = 0, xx = 0, yy = 0;
        for (int f = 0; f < frames; ++f)
        {
            const float e = a[(size_t) f] * a[(size_t) f];
            if (db[(size_t) f] < p40 && db[(size_t) f] > floorDb)
            {
                hiT += hi[(size_t) f]; enT += e;
                xy += xyF[(size_t) f]; xx += xxF[(size_t) f]; yy += yyF[(size_t) f];
            }
            else if (db[(size_t) f] > p85) { hiL += hi[(size_t) f]; enL += e; }
        }
        if (enT > 0 && enL > 0 && hiL > 0)
        {
            const double ratio = (hiT / enT) / (hiL / enL);
            r.tone = juce::jlimit (-100.0f, 100.0f, (float) (40.0 * std::log2 (std::max (1e-4, ratio))));
        }
        // largura: correlação L/R nas caudas
        if (stereo && xx > 0 && yy > 0)
        {
            const float corr = (float) (xy / std::sqrt (xx * yy));
            r.widthPct = juce::jlimit (40.0f, 180.0f, 100.0f + (0.6f - corr) * 100.0f);
            // ping-pong: num canal o eco aparece ao fim de T, no outro só ao fim de 2T
            const auto dl = detrend (aL, (int) (0.5 / kHop)), dr = detrend (aR, (int) (0.5 / kHop));
            const float l1 = peakNear (dl, dl, lag), r1 = peakNear (dr, dr, lag);
            const float hiC = std::max (l1, r1), loC = std::min (l1, r1);
            const float lo2 = l1 < r1 ? peakNear (dl, dl, 2.0f * lag) : peakNear (dr, dr, 2.0f * lag);
            r.pingPong = hiC > 0.08f && loC < 0.5f * hiC && lo2 > loC;
        }
        r.confidence = juce::jlimit (0.0f, 1.0f, bv * 1.6f);
        r.ok = true;
        return r;
    }

    // lê até 90 s de um ficheiro de áudio (WAV, AIFF, FLAC, OGG e, no Mac, MP3/M4A)
    static MatchResult analyseFile (const juce::File& file)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (file));
        MatchResult r;
        if (rd == nullptr) { r.error = "format"; return r; }
        const int len = (int) std::min<juce::int64> (rd->lengthInSamples, (juce::int64) (rd->sampleRate * 90.0));
        juce::AudioBuffer<float> b ((int) std::min (2u, rd->numChannels), std::max (1, len));
        rd->read (&b, 0, len, 0, true, b.getNumChannels() > 1);
        return analyse (b, rd->sampleRate);
    }

private:
    static constexpr int kBands = 40;

    // espectrograma em bandas log (100 Hz – 5 kHz), 5 ms, normalizado por banda e por frame
    static std::vector<float> bandSpectrogram (const float* L, const float* R, int n, double sr, int hop, int frames)
    {
        constexpr int order = 10, N = 1 << order;            // 1024 pontos
        juce::dsp::FFT fft (order);
        std::vector<float> out ((size_t) frames * kBands, 0.0f);
        std::array<int, kBands + 1> edge {};
        for (int b = 0; b <= kBands; ++b)
        {
            const double f = 100.0 * std::pow (50.0, (double) b / kBands);
            edge[(size_t) b] = juce::jlimit (1, N / 2 - 1, (int) std::lround (f * N / sr));
        }
        std::vector<float> buf ((size_t) N * 2), win ((size_t) N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (N - 1));
        for (int f = 0; f < frames; ++f)
        {
            const int c = f * hop + hop / 2 - N / 2;
            std::fill (buf.begin(), buf.end(), 0.0f);
            for (int i = 0; i < N; ++i)
            {
                const int k = c + i;
                if (k >= 0 && k < n) buf[(size_t) i] = 0.5f * (L[k] + R[k]) * win[(size_t) i];
            }
            fft.performFrequencyOnlyForwardTransform (buf.data(), true);
            float* row = &out[(size_t) f * kBands];
            for (int b = 0; b < kBands; ++b)
            {
                double e = 0;
                for (int k = edge[(size_t) b]; k <= std::max (edge[(size_t) b], edge[(size_t) b + 1] - 1); ++k) e += (double) buf[(size_t) k] * buf[(size_t) k];
                row[b] = (float) std::log (e + 1e-9);
            }
        }
        // tira a média de cada banda (cor do som) e de cada frame (volume)
        for (int b = 0; b < kBands; ++b)
        {
            double m = 0; for (int f = 0; f < frames; ++f) m += out[(size_t) f * kBands + b]; m /= frames;
            for (int f = 0; f < frames; ++f) out[(size_t) f * kBands + b] -= (float) m;
        }
        for (int f = 0; f < frames; ++f)
        {
            float* row = &out[(size_t) f * kBands];
            float m = 0; for (int b = 0; b < kBands; ++b) m += row[b]; m /= kBands;
            for (int b = 0; b < kBands; ++b) row[b] -= m;
        }
        return out;
    }

    static std::vector<float> detrend (const std::vector<float>& x, int win)
    {
        const int n = (int) x.size();
        std::vector<float> out ((size_t) n);
        double acc = 0; int cnt = 0;
        std::vector<double> pre ((size_t) n + 1, 0.0);
        for (int i = 0; i < n; ++i) pre[(size_t) i + 1] = pre[(size_t) i] + x[(size_t) i];
        for (int i = 0; i < n; ++i)
        {
            const int a = std::max (0, i - win / 2), b = std::min (n, i + win / 2 + 1);
            acc = (pre[(size_t) b] - pre[(size_t) a]) / (b - a); cnt = b - a;
            out[(size_t) i] = x[(size_t) i] - (float) acc;
        }
        juce::ignoreUnused (cnt);
        return out;
    }
};
} // namespace delex
