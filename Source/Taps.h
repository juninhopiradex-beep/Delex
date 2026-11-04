// =============================================================================
//  DELEX by Piradex — padrões de taps (Echo Sculpt e Multi-Tap), grooves e harmonia
//  O padrão vive dentro do estado do APVTS (filho "TAPS"): entra no desfazer,
//  no A/B, nos presets do utilizador e na sessão do DAW sem código extra.
// =============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DelexCore.h"

namespace delex::taps
{
constexpr int kSculptSteps = 16;    // repetições 1…16
constexpr int kMultiSteps  = 32;    // 32 passos da grelha (posição 0 = o próprio som, não usada)

// harmonia de cada tap
enum Interval { None = 0, Third, Fifth, OctUp, OctDown, Fourth, Sixth, Seventh, kNumIntervals };
inline juce::StringArray intervalNames() { return { "-", "3", "5", "8", "-8", "4", "6", "7" }; }

struct Step { float gain = 0, pan = 0, tone = 1; int interval = 0; };
struct Pattern
{
    std::array<Step, kSculptSteps> sculpt {};
    std::array<Step, kMultiSteps>  multi {};
    bool operator== (const Pattern& o) const noexcept
    {
        auto eq = [] (const Step& a, const Step& b) { return a.gain == b.gain && a.pan == b.pan && a.tone == b.tone && a.interval == b.interval; };
        for (int i = 0; i < kSculptSteps; ++i) if (! eq (sculpt[(size_t) i], o.sculpt[(size_t) i])) return false;
        for (int i = 0; i < kMultiSteps; ++i)  if (! eq (multi[(size_t) i], o.multi[(size_t) i]))  return false;
        return true;
    }
    bool operator!= (const Pattern& o) const noexcept { return ! (*this == o); }
};

inline Pattern defaultPattern()
{
    Pattern p;
    for (int i = 0; i < kSculptSteps; ++i)
    {
        auto& s = p.sculpt[(size_t) i];
        s.gain = i < 10 ? 0.8f * std::pow (0.78f, (float) i) : 0.0f;
        s.pan = (i % 2 == 0 ? -0.4f : 0.4f);
        s.tone = 1.0f - 0.05f * (float) i;
    }
    // tresillo (3-3-2) em semicolcheias
    const int st[] = { 3, 6, 8, 11, 14 };
    float g = 0.8f;
    for (int k : st) { auto& s = p.multi[(size_t) k]; s.gain = g; s.pan = (k % 2 ? 0.45f : -0.45f); g *= 0.8f; }
    return p;
}

// ---------------------------------------------------------------- grelhas
inline double gridQuarters (int grid) { return grid == 1 ? 1.0 / 6.0 : (grid == 2 ? 0.5 : 0.25); }

// ---------------------------------------------------------------- ValueTree
inline const juce::Identifier& treeId() { static const juce::Identifier i ("TAPS"); return i; }

inline juce::ValueTree toTree (const Pattern& p)
{
    juce::ValueTree t (treeId());
    auto put = [&] (const char* type, const Step& s, int idx)
    {
        juce::ValueTree c (type);
        c.setProperty ("i", idx, nullptr); c.setProperty ("g", s.gain, nullptr);
        c.setProperty ("p", s.pan, nullptr); c.setProperty ("t", s.tone, nullptr); c.setProperty ("h", s.interval, nullptr);
        t.appendChild (c, nullptr);
    };
    for (int i = 0; i < kSculptSteps; ++i) put ("S", p.sculpt[(size_t) i], i);
    for (int i = 0; i < kMultiSteps; ++i)  put ("M", p.multi[(size_t) i], i);
    return t;
}

inline Pattern fromTree (const juce::ValueTree& t)
{
    Pattern p = defaultPattern();
    if (! t.isValid()) return p;
    for (auto c : t)
    {
        const int i = c.getProperty ("i", -1);
        Step s { (float) c.getProperty ("g", 0.0f), (float) c.getProperty ("p", 0.0f), (float) c.getProperty ("t", 1.0f), (int) c.getProperty ("h", 0) };
        s.gain = juce::jlimit (0.0f, 1.0f, s.gain); s.pan = juce::jlimit (-1.0f, 1.0f, s.pan);
        s.tone = juce::jlimit (0.0f, 1.0f, s.tone); s.interval = juce::jlimit (0, kNumIntervals - 1, s.interval);
        if (c.hasType ("S") && juce::isPositiveAndBelow (i, kSculptSteps)) p.sculpt[(size_t) i] = s;
        if (c.hasType ("M") && juce::isPositiveAndBelow (i, kMultiSteps))  p.multi[(size_t) i] = s;
    }
    return p;
}

// garante que o estado do APVTS tem o filho TAPS e devolve-o
inline juce::ValueTree ensureTree (juce::ValueTree state)
{
    auto t = state.getChildWithName (treeId());
    if (! t.isValid()) { t = toTree (defaultPattern()); state.appendChild (t, nullptr); }
    return t;
}

// edita um passo (com desfazer)
inline void setStep (juce::ValueTree state, bool sculpt, int index, const Step& s, juce::UndoManager* um)
{
    auto t = ensureTree (state);
    for (auto c : t)
        if (c.hasType (sculpt ? "S" : "M") && (int) c.getProperty ("i", -1) == index)
        {
            c.setProperty ("g", s.gain, um); c.setProperty ("p", s.pan, um);
            c.setProperty ("t", s.tone, um); c.setProperty ("h", s.interval, um);
            return;
        }
}

inline void replaceMulti (juce::ValueTree state, const std::array<Step, kMultiSteps>& m, juce::UndoManager* um)
{
    for (int i = 0; i < kMultiSteps; ++i) setStep (state, false, i, m[(size_t) i], um);
}

// ---------------------------------------------------------------- harmonia
//  Escala maior/menor natural. A partir da nota detectada (ou da tónica) calcula o intervalo
//  diatónico: a "3.ª" pode ser maior ou menor consoante o grau — o eco fica dentro do tom.
inline float intervalSemis (int interval, int keyRoot, bool minor, int note)
{
    static const int major[7] = { 0, 2, 4, 5, 7, 9, 11 }, minorS[7] = { 0, 2, 3, 5, 7, 8, 10 };
    const int* sc = minor ? minorS : major;
    if (interval == None) return 0.0f;
    if (interval == OctUp) return 12.0f;
    if (interval == OctDown) return -12.0f;
    const int steps = interval == Third ? 2 : interval == Fifth ? 4 : interval == Fourth ? 3 : interval == Sixth ? 5 : 6;
    // grau mais próximo da nota na escala
    const int rel = note >= 0 ? ((note - keyRoot) % 12 + 12) % 12 : 0;
    int deg = 0, best = 99;
    for (int d = 0; d < 7; ++d) { const int dist = std::abs (sc[d] - rel); if (dist < best) { best = dist; deg = d; } }
    const int target = deg + steps;
    return (float) (sc[target % 7] + 12 * (target / 7) - sc[deg]);
}

// ---------------------------------------------------------------- conversão para o motor
struct HarmonyInfo { int keyRoot = 0; bool minor = false; int note = -1; };

inline TapSet toTapSet (const Pattern& p, TapsMode mode, int grid, float swing, const HarmonyInfo& h)
{
    TapSet out;
    if (mode == TapsMode::Off) return out;
    if (mode == TapsMode::Sculpt)
    {
        for (int i = 0; i < kSculptSteps && out.count < TapSet::kMax; ++i)
        {
            const auto& s = p.sculpt[(size_t) i];
            if (s.gain < 0.001f) continue;
            out.taps[(size_t) out.count++] = { (float) (i + 1), s.gain, s.pan, s.tone, intervalSemis (s.interval, h.keyRoot, h.minor, h.note) };
        }
        return out;
    }
    const double q = gridQuarters (grid);
    for (int i = 1; i < kMultiSteps && out.count < TapSet::kMax; ++i)
    {
        const auto& s = p.multi[(size_t) i];
        if (s.gain < 0.001f) continue;
        double pos = i * q;
        if (grid == 0 && (i % 2) == 1) pos += 0.5 * q * juce::jlimit (0.0f, 1.0f, swing);   // swing nas semicolcheias fracas
        out.taps[(size_t) out.count++] = { (float) pos, s.gain, s.pan, s.tone, intervalSemis (s.interval, h.keyRoot, h.minor, h.note) };
    }
    return out;
}

// ---------------------------------------------------------------- grooves (inspirados em ritmos africanos e da diáspora)
//  Posições em passos da grelha indicada; ganho e pan por passo.
struct Groove { const char* name; const char* descPt; const char* descEn; int grid; std::vector<std::array<float, 3>> steps; };

inline const std::vector<Groove>& grooves()
{
    static const std::vector<Groove> g {
        { "Kizomba", "Pulso pesado com a resposta no contratempo, como o bombo e a caixa da kizomba.", "Heavy pulse answered on the off-beat, like kizomba kick and snare.", 0,
          { { 3, 0.75f, -0.4f }, { 6, 0.55f, 0.4f }, { 8, 0.7f, 0.0f }, { 11, 0.45f, -0.4f }, { 14, 0.4f, 0.4f } } },
        { "Semba", "Balanço do semba: respostas curtas que puxam para a frente.", "Semba swing: short answers pushing forward.", 0,
          { { 2, 0.7f, 0.35f }, { 3, 0.5f, -0.35f }, { 6, 0.6f, 0.35f }, { 8, 0.55f, -0.2f }, { 10, 0.45f, 0.35f }, { 14, 0.35f, -0.35f } } },
        { "Kuduro", "Rápido e partido: ecos em rajada, bom em vozes curtas e percussão.", "Fast and broken: bursts of echoes, great on short vocals and percussion.", 0,
          { { 2, 0.7f, -0.5f }, { 4, 0.6f, 0.5f }, { 6, 0.55f, -0.5f }, { 7, 0.4f, 0.5f }, { 10, 0.5f, -0.3f }, { 12, 0.45f, 0.3f }, { 15, 0.3f, 0.0f } } },
        { "Afro House", "Síncopa larga de afro house: ecos que dançam entre os tempos.", "Wide afro house syncopation: echoes dancing between the beats.", 0,
          { { 3, 0.7f, -0.5f }, { 6, 0.6f, 0.5f }, { 10, 0.55f, -0.3f }, { 12, 0.45f, 0.3f }, { 14, 0.4f, 0.0f }, { 19, 0.3f, -0.4f }, { 22, 0.25f, 0.4f } } },
        { "Amapiano (log drum)", "Atrasos à maneira do log drum do amapiano, com espaço entre frases.", "Log-drum style delays from amapiano, with space between phrases.", 0,
          { { 3, 0.75f, 0.0f }, { 6, 0.55f, -0.4f }, { 10, 0.6f, 0.4f }, { 14, 0.45f, 0.0f }, { 22, 0.35f, -0.4f }, { 26, 0.3f, 0.4f } } },
        { "Gqom", "Escuro e partido em 3+3+3+3+4: tensão constante.", "Dark and broken in 3+3+3+3+4: constant tension.", 0,
          { { 3, 0.7f, -0.4f }, { 6, 0.65f, 0.4f }, { 9, 0.55f, -0.4f }, { 12, 0.5f, 0.4f }, { 16, 0.4f, 0.0f } } },
        { "Afrobeats 3-3-2", "Tresillo clássico (3-3-2), a base de muito afrobeats e dancehall.", "Classic tresillo (3-3-2), the backbone of much afrobeats and dancehall.", 0,
          { { 3, 0.75f, -0.45f }, { 6, 0.6f, 0.45f }, { 8, 0.55f, 0.0f }, { 11, 0.45f, -0.45f }, { 14, 0.4f, 0.45f } } },
        { "Highlife", "Guitarra de highlife: ecos leves que respondem no meio do tempo.", "Highlife guitar: light echoes answering mid-beat.", 0,
          { { 2, 0.6f, 0.4f }, { 5, 0.5f, -0.4f }, { 8, 0.55f, 0.4f }, { 10, 0.45f, -0.4f }, { 13, 0.35f, 0.0f } } },
        { "Coupé-Décalé", "Energia de coupé-décalé: dois ecos rápidos e um largo.", "Coupé-décalé energy: two quick echoes and a wide one.", 0,
          { { 1, 0.55f, -0.5f }, { 2, 0.5f, 0.5f }, { 6, 0.65f, 0.0f }, { 9, 0.45f, -0.5f }, { 10, 0.4f, 0.5f }, { 14, 0.4f, 0.0f } } },
        { "Sino 12/8", "Padrão de sino em 12/8 (tercinas), comum na música da África Ocidental.", "12/8 bell pattern (triplets), common in West African music.", 1,
          { { 4, 0.7f, -0.4f }, { 8, 0.6f, 0.4f }, { 10, 0.55f, -0.2f }, { 14, 0.5f, 0.2f }, { 18, 0.45f, -0.4f }, { 22, 0.4f, 0.4f } } },
        { "Marrabenta", "Saltitante como a marrabenta: respostas curtas em colcheia.", "Bouncy like marrabenta: short eighth-note answers.", 0,
          { { 2, 0.65f, 0.4f }, { 4, 0.5f, -0.4f }, { 6, 0.55f, 0.4f }, { 10, 0.45f, -0.4f }, { 12, 0.4f, 0.4f } } },
        { "Tarraxinha", "Lento e sensual: ecos espaçados com muito ar.", "Slow and sensual: spaced echoes with lots of air.", 0,
          { { 6, 0.7f, -0.3f }, { 8, 0.45f, 0.3f }, { 14, 0.5f, 0.0f }, { 22, 0.35f, -0.3f }, { 30, 0.25f, 0.3f } } } };
    return g;
}

inline std::array<Step, kMultiSteps> grooveSteps (const Groove& g)
{
    std::array<Step, kMultiSteps> m {};
    for (auto& st : g.steps)
    {
        const int i = juce::jlimit (1, kMultiSteps - 1, (int) st[0]);
        m[(size_t) i] = { st[1], st[2], 1.0f, 0 };
    }
    return m;
}

// passos a partir de inícios captados (em semínimas, relativos ao primeiro)
inline std::array<Step, kMultiSteps> stepsFromOnsets (const std::vector<double>& onsetsQ, int grid)
{
    std::array<Step, kMultiSteps> m {};
    if (onsetsQ.size() < 2) return m;
    const double q = gridQuarters (grid);
    float g = 0.85f; bool left = true;
    for (size_t k = 1; k < onsetsQ.size(); ++k)
    {
        const int i = (int) std::lround ((onsetsQ[k] - onsetsQ[0]) / q);
        if (i <= 0 || i >= kMultiSteps || m[(size_t) i].gain > 0.0f) continue;
        m[(size_t) i] = { g, left ? -0.45f : 0.45f, 1.0f, 0 };
        g = std::max (0.3f, g * 0.88f); left = ! left;
    }
    return m;
}
} // namespace delex::taps
