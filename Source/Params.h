// =============================================================================
//  DELEX by Piradex — parâmetros (IDs estáveis), macros e conversão para o motor
// =============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DelexCore.h"

namespace delex
{
// ---------------------------------------------------------------- divisões musicais
struct Division { const char* name; double quarters; };

inline const std::vector<Division>& divisions()
{
    static const std::vector<Division> d {
        { "1/1", 4.0 }, { "1/2D", 3.0 }, { "1/2", 2.0 }, { "1/2T", 4.0 / 3.0 },
        { "1/4D", 1.5 }, { "1/4", 1.0 }, { "1/4T", 2.0 / 3.0 },
        { "1/8D", 0.75 }, { "1/8", 0.5 }, { "1/8T", 1.0 / 3.0 },
        { "1/16D", 0.375 }, { "1/16", 0.25 }, { "1/16T", 1.0 / 6.0 },
        { "1/32", 0.125 }, { "1/32T", 1.0 / 12.0 }, { "1/64", 0.0625 }, { "1/128", 0.03125 } };
    return d;
}
inline const std::vector<Division>& modDivisions()
{
    static const std::vector<Division> d {
        { "8 comp.", 32.0 }, { "4 comp.", 16.0 }, { "2 comp.", 8.0 }, { "1 comp.", 4.0 },
        { "1/2", 2.0 }, { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 } };
    return d;
}
inline juce::StringArray names (const std::vector<Division>& v) { juce::StringArray s; for (auto& d : v) s.add (d.name); return s; }

// ---------------------------------------------------------------- formatação
// Nota: no JUCE, String (v, 0) NÃO arredonda (mostra todas as casas decimais).
inline juce::String num (double v, int decimals)
{
    if (decimals <= 0) return juce::String (juce::roundToInt (v));
    return juce::String (v, decimals);
}

// ---------------------------------------------------------------- lista única de parâmetros
//  A ordem desta lista define a ordem dos campos em Values (todos float).
//  IDs estáveis: nunca mudar um nome existente (automação e sessões dos DAWs).
#define DELEX_PARAMS(X) \
    X(sync) X(div) X(time) X(feedback) X(space) X(tone) X(character) X(mix) \
    X(freeze) X(duck) X(bypass) \
    X(routing) X(r_ratio) X(fb_balance) X(transition) X(glide) \
    X(rotation) X(rot_mode) X(in_rot) X(out_rot) X(in_width) X(out_width) X(mono_safe) \
    X(low_cut) X(high_cut) X(cut_slope) X(peak_freq) X(peak_gain) X(peak_q) X(compensate) X(loop_order) \
    X(mod_depth) X(mod_rate) X(mod_sync) X(mod_div) X(mod_offset) X(mod_shape) X(wow) \
    X(disp_freq) X(disp_spread) X(diff_amount) X(diff_spread) \
    X(warmth) X(drive) X(bits) X(rate) X(jitter) X(grit_mix) X(pre_drive) \
    X(duck_thresh) X(duck_amount) X(duck_attack) X(duck_hold) X(duck_release) \
    X(gate) X(gate_thresh) X(gate_hold) X(gate_release) \
    X(mix_law) X(wet_trim) X(out_trim) \
    /* v1.0 — vaga A */ \
    X(key_src) X(monitor) X(const_loud) \
    X(ls_freq) X(ls_gain) X(hs_freq) X(hs_gain) X(peak2_freq) X(peak2_gain) X(peak2_q) \
    X(hiss) X(sat_hq)

namespace id
{
    #define DLX_ID(x) inline constexpr const char* x = #x;
    DELEX_PARAMS (DLX_ID)
    #undef DLX_ID
}

inline const std::vector<const char*>& paramIds()
{
    static const std::vector<const char*> v {
        #define DLX_S(x) #x,
        DELEX_PARAMS (DLX_S)
        #undef DLX_S
    };
    return v;
}

inline const juce::StringArray& loopOrderNames()
{
    static const juce::StringArray s = []
    {
        static const char* mod[5] = { "Grit", "EQ", "Disp", "Dif", "Fita" };
        juce::StringArray out;
        for (auto& perm : loopPermutations())
        {
            juce::StringArray parts;
            for (auto m : perm) parts.add (mod[m]);
            out.add (parts.joinIntoString (" > "));
        }
        return out;
    }();
    return s;
}

// ---------------------------------------------------------------- layout
inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto pid = [] (const char* s) { return ParameterID { s, 1 }; };

    auto pct = [] (float v, int) { return num (v, 0) + " %"; };
    auto ms  = [] (float v, int) { return v < 1000.0f ? num (v, v < 10.0f ? 1 : 0) + " ms" : num (v / 1000.0f, 2) + " s"; };
    auto hz  = [] (float v, int) { return v < 1000.0f ? num (v, v < 100.0f ? 1 : 0) + " Hz" : num (v / 1000.0f, v < 10000.0f ? 2 : 1) + " kHz"; };
    auto db  = [] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; };
    auto deg = [] (float v, int) { return String (v, 1) + juce::String::fromUTF8 ("\xc2\xb0"); };
    auto bip = [] (float v, int) { const int i = juce::roundToInt (v); return (i > 0 ? "+" : "") + String (i); };
    auto dbAmt = [] (float v, int) { return num (v, 1) + " dB"; };   // quantidade (sem sinal +)

    auto addF = [&] (const char* i, const char* name, NormalisableRange<float> r, float def, std::function<String (float, int)> fmt, const char* unit = "")
    {
        p.push_back (std::make_unique<AudioParameterFloat> (pid (i), name, r, def,
                     AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction (fmt)));
    };
    auto addB = [&] (const char* i, const char* name, bool def) { p.push_back (std::make_unique<AudioParameterBool> (pid (i), name, def)); };
    auto addC = [&] (const char* i, const char* name, StringArray c, int def) { p.push_back (std::make_unique<AudioParameterChoice> (pid (i), name, c, def)); };
    auto skewed = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };

    // principal
    addB (id::sync, "Sync", true);
    addC (id::div, "Division", names (divisions()), 5);
    addF (id::time, "Time", skewed (1.0f, 8000.0f, 400.0f), 375.0f, ms);
    addF (id::feedback, "Feedback", { 0.0f, 100.0f }, 40.0f, pct);
    addF (id::space, "Space", { 0.0f, 100.0f }, 30.0f, pct);
    addF (id::tone, "Tone", { -100.0f, 100.0f }, 0.0f, bip);
    addF (id::character, "Character", { -100.0f, 100.0f }, 0.0f,
          [] (float v, int) { return std::abs (v) < 0.5f ? String ("0") : (v > 0 ? "TAPE " : "DIGI ") + num (std::abs (v), 0); });
    addF (id::mix, "Mix", { 0.0f, 100.0f }, 25.0f, pct);
    addB (id::freeze, "Freeze", false);
    addB (id::duck, "Duck", false);
    addB (id::bypass, "Bypass", false);

    // timing
    addC (id::routing, "Routing", { "Dual", "Ping-Pong", "Mono" }, 0);
    addF (id::r_ratio, "R Time Ratio", skewed (0.25f, 2.0f, 1.0f), 1.0f, [] (float v, int) { return "R x " + String (v, 2); });
    addF (id::fb_balance, "Feedback L/R", { -100.0f, 100.0f }, 0.0f,
          [] (float v, int) { return std::abs (v) < 0.5f ? String ("C") : (v < 0 ? "L " : "R ") + num (std::abs (v), 0); });
    addC (id::transition, "Time Transition", { "Tape Glide", "Crossfade" }, 0);
    addF (id::glide, "Glide", skewed (10.0f, 2000.0f, 250.0f), 250.0f, ms);

    // espaço
    addF (id::rotation, "Rotation", { -180.0f, 180.0f }, 0.0f, deg);
    addC (id::rot_mode, "Matrix Mode", { "Rotation", "Reflection" }, 0);
    addF (id::in_rot, "In Rotation", { -45.0f, 45.0f }, 0.0f, deg);
    addF (id::out_rot, "Out Rotation", { -45.0f, 45.0f }, 0.0f, deg);
    addF (id::in_width, "In Width", { 0.0f, 200.0f }, 100.0f, pct);
    addF (id::out_width, "Out Width", { 0.0f, 200.0f }, 100.0f, pct);
    addB (id::mono_safe, "Mono-Safe Bass", false);

    // feedback EQ
    addF (id::low_cut, "Low Cut", skewed (20.0f, 2000.0f, 200.0f), 60.0f, hz);
    addF (id::high_cut, "High Cut", skewed (1000.0f, 20000.0f, 6000.0f), 14000.0f, hz);
    addC (id::cut_slope, "Cut Slope", { "6 dB/oct", "12 dB/oct", "24 dB/oct" }, 1);
    addF (id::peak_freq, "Peak Freq", skewed (100.0f, 10000.0f, 1000.0f), 1000.0f, hz);
    addF (id::peak_gain, "Peak Gain", { -18.0f, 18.0f }, 0.0f, db);
    addF (id::peak_q, "Peak Q", skewed (0.3f, 10.0f, 1.5f), 1.0f, [] (float v, int) { return String (v, 2); });
    addF (id::compensate, "Compensate", { 0.0f, 100.0f }, 50.0f, pct);
    addC (id::loop_order, "Loop Order", loopOrderNames(), 0);

    // movimento
    addF (id::mod_depth, "Mod Depth", skewed (0.0f, 50.0f, 10.0f), 0.0f, [] (float v, int) { return String (v, 1) + " cents"; });
    addF (id::mod_rate, "Mod Rate", skewed (0.01f, 10.0f, 0.8f), 0.35f, [] (float v, int) { return String (v, 2) + " Hz"; });
    addB (id::mod_sync, "Mod Sync", false);
    addC (id::mod_div, "Mod Division", names (modDivisions()), 3);
    addF (id::mod_offset, "Mod Offset", { 0.0f, 100.0f }, 50.0f, pct);
    addC (id::mod_shape, "Mod Shape", { "Sine", "Triangle", "Random" }, 0);
    addF (id::wow, "Wow & Flutter", { 0.0f, 100.0f }, 0.0f, pct);
    addF (id::disp_freq, "Dispersion Freq", skewed (50.0f, 5000.0f, 500.0f), 500.0f, hz);
    addF (id::disp_spread, "Dispersion Spread", { 0.0f, 50.0f }, 0.0f, ms);
    addF (id::diff_amount, "Diffusion", { 0.0f, 100.0f }, 0.0f, pct);
    addF (id::diff_spread, "Diffusion Spread", skewed (1.0f, 50.0f, 12.0f), 12.0f, ms);

    // textura
    addF (id::warmth, "Warmth", { 0.0f, 100.0f }, 20.0f, pct);
    addF (id::drive, "Drive", { 0.0f, 100.0f }, 0.0f, pct);
    addF (id::bits, "Bit Depth", { 1.0f, 24.0f }, 24.0f, [] (float v, int) { return v > 23.5f ? String ("Off") : String (v, 1) + " bit"; });
    addF (id::rate, "Sample Rate", skewed (1000.0f, 48000.0f, 12000.0f), 48000.0f, [] (float v, int) { return v > 47900.0f ? String ("Off") : String (v / 1000.0f, 1) + " kHz"; });
    addF (id::jitter, "Jitter", { 0.0f, 100.0f }, 0.0f, pct);
    addF (id::grit_mix, "Grit Mix", { 0.0f, 100.0f }, 100.0f, pct);
    addF (id::pre_drive, "Pre-Drive", { -12.0f, 12.0f }, 0.0f, db);

    // utilidades
    addF (id::duck_thresh, "Duck Threshold", { -60.0f, 0.0f }, -26.0f, db);
    addF (id::duck_amount, "Duck Amount", { 0.0f, 30.0f }, 9.0f, dbAmt);
    addF (id::duck_attack, "Duck Attack", skewed (0.1f, 100.0f, 8.0f), 4.0f, ms);
    addF (id::duck_hold, "Duck Hold", { 0.0f, 500.0f }, 60.0f, ms);
    addF (id::duck_release, "Duck Release", skewed (10.0f, 2000.0f, 300.0f), 300.0f, ms);
    addB (id::gate, "Gate", false);
    addF (id::gate_thresh, "Gate Threshold", { -60.0f, 0.0f }, -30.0f, db);
    addF (id::gate_hold, "Gate Hold", skewed (10.0f, 1000.0f, 250.0f), 250.0f, ms);
    addF (id::gate_release, "Gate Release", skewed (2.0f, 500.0f, 40.0f), 40.0f, ms);
    addC (id::mix_law, "Mix Law", { "Linear", "Equal Power" }, 1);
    addF (id::wet_trim, "Wet Trim", { -24.0f, 12.0f }, 0.0f, db);
    addF (id::out_trim, "Output", { -24.0f, 12.0f }, 0.0f, db);

    // --- v1.0 (vaga A)
    addC (id::key_src, "Key Source", { "Internal", "Sidechain" }, 0);
    addC (id::monitor, "Monitor", { "Normal", "Wet Solo", "Delta", "Mono", "Phone", "Club" }, 0);
    addB (id::const_loud, "Constant Loudness", false);
    addF (id::ls_freq, "Low Shelf Freq", skewed (30.0f, 1000.0f, 150.0f), 150.0f, hz);
    addF (id::ls_gain, "Low Shelf Gain", { -18.0f, 18.0f }, 0.0f, db);
    addF (id::hs_freq, "High Shelf Freq", skewed (1000.0f, 16000.0f, 5000.0f), 6000.0f, hz);
    addF (id::hs_gain, "High Shelf Gain", { -18.0f, 18.0f }, 0.0f, db);
    addF (id::peak2_freq, "Peak 2 Freq", skewed (100.0f, 10000.0f, 1000.0f), 3000.0f, hz);
    addF (id::peak2_gain, "Peak 2 Gain", { -18.0f, 18.0f }, 0.0f, db);
    addF (id::peak2_q, "Peak 2 Q", skewed (0.3f, 10.0f, 1.5f), 1.0f, [] (float v, int) { return String (v, 2); });
    addF (id::hiss, "Tape Hiss", { 0.0f, 100.0f }, 0.0f, pct);
    addB (id::sat_hq, "Saturation HQ (2x)", false);

    return { p.begin(), p.end() };
}

// ---------------------------------------------------------------- valores lidos
//  Cópia plana de todos os parâmetros (usada no áudio e no visualizador).
//  Só floats, pela ordem de DELEX_PARAMS → acesso genérico por índice.
struct Values
{
    #define DLX_F(x) float x = 0.0f;
    DELEX_PARAMS (DLX_F)
    #undef DLX_F

    static constexpr int count() noexcept
    {
        int n = 0;
        #define DLX_C(x) ++n;
        DELEX_PARAMS (DLX_C)
        #undef DLX_C
        return n;
    }
    float*       data() noexcept       { return &sync; }
    const float* data() const noexcept { return &sync; }
};
static_assert (sizeof (Values) == sizeof (float) * (size_t) Values::count(), "Values tem de ser só floats");

class RawParams
{
public:
    explicit RawParams (juce::AudioProcessorValueTreeState& s)
    {
        const auto& ids = paramIds();
        for (size_t i = 0; i < ids.size(); ++i)
        {
            ptr[i] = s.getRawParameterValue (ids[i]);
            prm[i] = s.getParameter (ids[i]);
            jassert (ptr[i] != nullptr);
        }
    }

    Values load() const noexcept
    {
        Values v;
        float* d = v.data();
        for (int i = 0; i < Values::count(); ++i) d[i] = ptr[(size_t) i]->load (std::memory_order_relaxed);
        return v;
    }

    juce::RangedAudioParameter* parameter (int index) const noexcept { return prm[(size_t) index]; }
    static int indexOf (const juce::String& pid)
    {
        const auto& ids = paramIds();
        for (size_t i = 0; i < ids.size(); ++i) if (pid == ids[i]) return (int) i;
        return -1;
    }

private:
    std::array<std::atomic<float>*, (size_t) Values::count()> ptr {};
    std::array<juce::RangedAudioParameter*, (size_t) Values::count()> prm {};
};

// ---------------------------------------------------------------- tempo
inline double delayMsL (const Values& v, double bpm)
{
    if (v.sync > 0.5f)
    {
        const auto& d = divisions();
        const int i = juce::jlimit (0, (int) d.size() - 1, (int) std::lround (v.div));
        return juce::jlimit (1.0, 8000.0, 60000.0 / juce::jmax (20.0, bpm) * d[(size_t) i].quarters);
    }
    return juce::jlimit (1.0, 8000.0, (double) v.time);
}

// ---------------------------------------------------------------- macros + conversão
//  SPACE     → largura de saída, difusão e rotação extra (modo Dual)
//  TONE      → < 0 escurece (desce o high cut), > 0 abrilhanta (sobe o low cut e o high cut)
//  CHARACTER → > 0 fita (drive, warmth, wow), < 0 digital (bits e sample rate)
inline Params toEngine (const Values& v, double bpm, double sr)
{
    Params p;
    p.timeMsL = (float) delayMsL (v, bpm);
    p.timeMsR = juce::jlimit (1.0f, 8000.0f, p.timeMsL * v.r_ratio);
    p.feedback = v.feedback * 0.01f;
    p.fbBalance = v.fb_balance * 0.01f;
    p.routing = (Routing) juce::jlimit (0, 2, (int) std::lround (v.routing));
    p.transition = (Transition) juce::jlimit (0, 1, (int) std::lround (v.transition));
    p.glideMs = v.glide;
    p.freeze = v.freeze > 0.5f;

    p.modDepthCents = v.mod_depth;
    if (v.mod_sync > 0.5f)
    {
        const auto& d = modDivisions();
        const int i = juce::jlimit (0, (int) d.size() - 1, (int) std::lround (v.mod_div));
        p.modRateHz = (float) (juce::jmax (20.0, bpm) / 60.0 / d[(size_t) i].quarters);
    }
    else p.modRateHz = v.mod_rate;
    p.modOffset = v.mod_offset * 0.01f;
    p.modShape = (ModShape) juce::jlimit (0, 2, (int) std::lround (v.mod_shape));
    p.wow = v.wow * 0.01f;

    p.rotDeg = v.rotation;  p.reflection = v.rot_mode > 0.5f;
    p.inRotDeg = v.in_rot;  p.outRotDeg = v.out_rot;
    p.inWidth = v.in_width * 0.01f;  p.outWidth = v.out_width * 0.01f;
    p.monoSafe = v.mono_safe > 0.5f;

    p.lowCutHz = v.low_cut;  p.highCutHz = v.high_cut;
    p.cutSlope = (CutSlope) juce::jlimit (0, 2, (int) std::lround (v.cut_slope));
    p.peakHz = v.peak_freq;  p.peakDb = v.peak_gain;  p.peakQ = v.peak_q;  p.compensate = v.compensate * 0.01f;
    p.loopOrder = juce::jlimit (0, (int) loopPermutations().size() - 1, (int) std::lround (v.loop_order));
    p.lsHz = v.ls_freq; p.lsDb = v.ls_gain; p.hsHz = v.hs_freq; p.hsDb = v.hs_gain;
    p.pk2Hz = v.peak2_freq; p.pk2Db = v.peak2_gain; p.pk2Q = v.peak2_q;

    p.dispHz = v.disp_freq;  p.dispSpreadMs = v.disp_spread;
    p.diffAmount = v.diff_amount * 0.01f;  p.diffSpreadMs = v.diff_spread;

    p.tapeWarmth = v.warmth * 0.01f;  p.tapeDrive = v.drive * 0.01f;
    p.gritBits = v.bits;
    p.gritRateHz = v.rate > 47900.0f ? (float) sr * 4.0f : v.rate;
    p.gritJitter = v.jitter * 0.01f;  p.gritMix = v.grit_mix * 0.01f;
    p.preDriveDb = v.pre_drive;

    p.duckOn = v.duck > 0.5f;
    p.duckThreshDb = v.duck_thresh; p.duckAmountDb = v.duck_amount;
    p.duckAttackMs = v.duck_attack; p.duckHoldMs = v.duck_hold; p.duckReleaseMs = v.duck_release;
    p.gateOn = v.gate > 0.5f;
    p.gateThreshDb = v.gate_thresh; p.gateHoldMs = v.gate_hold; p.gateReleaseMs = v.gate_release;

    p.mix = v.mix * 0.01f;  p.equalPower = v.mix_law > 0.5f;
    p.wetTrimDb = v.wet_trim;  p.outTrimDb = v.out_trim;
    p.bypass = v.bypass > 0.5f;
    p.monitor = (Monitor) juce::jlimit (0, 5, (int) std::lround (v.monitor));
    p.constLoudness = v.const_loud > 0.5f;
    p.hiss = v.hiss * 0.01f;
    p.satHQ = v.sat_hq > 0.5f;

    // --- macros
    const float s = juce::jlimit (0.0f, 1.0f, v.space * 0.01f);
    p.outWidth = juce::jmin (2.0f, p.outWidth * (1.0f + 0.8f * s));
    p.diffAmount = 1.0f - (1.0f - p.diffAmount) * (1.0f - 0.55f * s);
    if (p.routing == Routing::Dual) p.rotDeg = juce::jlimit (-180.0f, 180.0f, p.rotDeg + 30.0f * s);

    const float t = juce::jlimit (-1.0f, 1.0f, v.tone * 0.01f);
    if (t < 0.0f) p.highCutHz = juce::jmax (400.0f, p.highCutHz * std::pow (2.0f, t * 3.3f));
    else
    {
        p.lowCutHz = juce::jmin (4000.0f, p.lowCutHz * std::pow (2.0f, t * 3.0f));
        p.highCutHz = juce::jmin (20000.0f, p.highCutHz * std::pow (2.0f, t * 0.8f));
    }

    const float c = juce::jlimit (-1.0f, 1.0f, v.character * 0.01f);
    if (c > 0.0f)
    {
        p.tapeDrive = 1.0f - (1.0f - p.tapeDrive) * (1.0f - 0.85f * c);
        p.tapeWarmth = juce::jmax (p.tapeWarmth, 0.6f * c);
        p.wow = juce::jmax (p.wow, 0.25f * c);
    }
    else if (c < 0.0f)
    {
        const float a = -c;
        p.gritBits = juce::jmin (p.gritBits, 24.0f - a * 18.0f);
        p.gritRateHz = juce::jmin (p.gritRateHz, (float) sr * std::pow (2.0f, -2.5f * a) * (a > 0.02f ? 1.0f : 4.0f));
        p.gritJitter = juce::jmax (p.gritJitter, 0.3f * a);
        p.gritMix = juce::jmax (p.gritMix, a);
    }
    return p;
}

} // namespace delex
