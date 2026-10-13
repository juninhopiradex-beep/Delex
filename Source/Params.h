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

// ---------------------------------------------------------------- IDs
namespace id
{
    #define DLX_ID(x) inline constexpr const char* x = #x;
    DLX_ID(sync) DLX_ID(div) DLX_ID(time) DLX_ID(feedback) DLX_ID(space) DLX_ID(tone) DLX_ID(character) DLX_ID(mix)
    DLX_ID(freeze) DLX_ID(duck) DLX_ID(bypass)
    DLX_ID(routing) DLX_ID(r_ratio) DLX_ID(fb_balance) DLX_ID(transition) DLX_ID(glide)
    DLX_ID(rotation) DLX_ID(rot_mode) DLX_ID(in_rot) DLX_ID(out_rot) DLX_ID(in_width) DLX_ID(out_width) DLX_ID(mono_safe)
    DLX_ID(low_cut) DLX_ID(high_cut) DLX_ID(cut_slope) DLX_ID(peak_freq) DLX_ID(peak_gain) DLX_ID(peak_q) DLX_ID(compensate) DLX_ID(loop_order)
    DLX_ID(mod_depth) DLX_ID(mod_rate) DLX_ID(mod_sync) DLX_ID(mod_div) DLX_ID(mod_offset) DLX_ID(mod_shape) DLX_ID(wow)
    DLX_ID(disp_freq) DLX_ID(disp_spread) DLX_ID(diff_amount) DLX_ID(diff_spread)
    DLX_ID(warmth) DLX_ID(drive) DLX_ID(bits) DLX_ID(rate) DLX_ID(jitter) DLX_ID(grit_mix) DLX_ID(pre_drive)
    DLX_ID(duck_thresh) DLX_ID(duck_amount) DLX_ID(duck_attack) DLX_ID(duck_hold) DLX_ID(duck_release)
    DLX_ID(mix_law) DLX_ID(wet_trim) DLX_ID(out_trim)
    #undef DLX_ID
}

inline const juce::StringArray& loopOrderNames()
{
    static const juce::StringArray s { "Grit > EQ > Disp > Dif > Fita", "EQ > Fita > Dif > Disp > Grit",
                                       "Fita > EQ > Dif > Disp > Grit", "Dif > Disp > EQ > Fita > Grit" };
    return s;
}

// ---------------------------------------------------------------- layout
inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto pid = [] (const char* s) { return ParameterID { s, 1 }; };

    auto pct = [] (float v, int) { return String (v, 0) + " %"; };
    auto ms  = [] (float v, int) { return v < 1000.0f ? String (v, v < 10.0f ? 1 : 0) + " ms" : String (v / 1000.0f, 2) + " s"; };
    auto hz  = [] (float v, int) { return v < 1000.0f ? String (v, v < 100.0f ? 1 : 0) + " Hz" : String (v / 1000.0f, v < 10000.0f ? 2 : 1) + " kHz"; };
    auto db  = [] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; };
    auto deg = [] (float v, int) { return String (v, 1) + juce::String::fromUTF8 ("\xc2\xb0"); };
    auto bip = [] (float v, int) { return (v > 0.5f ? "+" : "") + String (v, 0); };

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
          [] (float v, int) { return std::abs (v) < 0.5f ? String ("0") : (v > 0 ? "TAPE " : "DIGI ") + String (std::abs (v), 0); });
    addF (id::mix, "Mix", { 0.0f, 100.0f }, 25.0f, pct);
    addB (id::freeze, "Freeze", false);
    addB (id::duck, "Duck", false);
    addB (id::bypass, "Bypass", false);

    // timing
    addC (id::routing, "Routing", { "Dual", "Ping-Pong", "Mono" }, 0);
    addF (id::r_ratio, "R Time Ratio", skewed (0.25f, 2.0f, 1.0f), 1.0f, [] (float v, int) { return "R x " + String (v, 2); });
    addF (id::fb_balance, "Feedback L/R", { -100.0f, 100.0f }, 0.0f,
          [] (float v, int) { return std::abs (v) < 0.5f ? String ("C") : (v < 0 ? "L " : "R ") + String (std::abs (v), 0); });
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
    addF (id::duck_amount, "Duck Amount", { 0.0f, 30.0f }, 9.0f, db);
    addF (id::duck_attack, "Duck Attack", skewed (0.1f, 100.0f, 8.0f), 4.0f, ms);
    addF (id::duck_hold, "Duck Hold", { 0.0f, 500.0f }, 60.0f, ms);
    addF (id::duck_release, "Duck Release", skewed (10.0f, 2000.0f, 300.0f), 300.0f, ms);
    addC (id::mix_law, "Mix Law", { "Linear", "Equal Power" }, 1);
    addF (id::wet_trim, "Wet Trim", { -24.0f, 12.0f }, 0.0f, db);
    addF (id::out_trim, "Output", { -24.0f, 12.0f }, 0.0f, db);

    return { p.begin(), p.end() };
}

// ---------------------------------------------------------------- valores lidos
//  Cópia plana de todos os parâmetros (usada no áudio e no visualizador)
struct Values
{
    float sync, div, time, feedback, space, tone, character, mix, freeze, duck, bypass,
          routing, r_ratio, fb_balance, transition, glide,
          rotation, rot_mode, in_rot, out_rot, in_width, out_width, mono_safe,
          low_cut, high_cut, cut_slope, peak_freq, peak_gain, peak_q, compensate, loop_order,
          mod_depth, mod_rate, mod_sync, mod_div, mod_offset, mod_shape, wow,
          disp_freq, disp_spread, diff_amount, diff_spread,
          warmth, drive, bits, rate, jitter, grit_mix, pre_drive,
          duck_thresh, duck_amount, duck_attack, duck_hold, duck_release,
          mix_law, wet_trim, out_trim;
};

class RawParams
{
public:
    explicit RawParams (juce::AudioProcessorValueTreeState& s)
    {
        #define DLX_P(x) x = s.getRawParameterValue (id::x); jassert (x != nullptr);
        DLX_P(sync) DLX_P(div) DLX_P(time) DLX_P(feedback) DLX_P(space) DLX_P(tone) DLX_P(character) DLX_P(mix)
        DLX_P(freeze) DLX_P(duck) DLX_P(bypass) DLX_P(routing) DLX_P(r_ratio) DLX_P(fb_balance) DLX_P(transition) DLX_P(glide)
        DLX_P(rotation) DLX_P(rot_mode) DLX_P(in_rot) DLX_P(out_rot) DLX_P(in_width) DLX_P(out_width) DLX_P(mono_safe)
        DLX_P(low_cut) DLX_P(high_cut) DLX_P(cut_slope) DLX_P(peak_freq) DLX_P(peak_gain) DLX_P(peak_q) DLX_P(compensate) DLX_P(loop_order)
        DLX_P(mod_depth) DLX_P(mod_rate) DLX_P(mod_sync) DLX_P(mod_div) DLX_P(mod_offset) DLX_P(mod_shape) DLX_P(wow)
        DLX_P(disp_freq) DLX_P(disp_spread) DLX_P(diff_amount) DLX_P(diff_spread)
        DLX_P(warmth) DLX_P(drive) DLX_P(bits) DLX_P(rate) DLX_P(jitter) DLX_P(grit_mix) DLX_P(pre_drive)
        DLX_P(duck_thresh) DLX_P(duck_amount) DLX_P(duck_attack) DLX_P(duck_hold) DLX_P(duck_release)
        DLX_P(mix_law) DLX_P(wet_trim) DLX_P(out_trim)
        #undef DLX_P
    }

    Values load() const noexcept
    {
        Values v;
        #define DLX_L(x) v.x = x->load (std::memory_order_relaxed);
        DLX_L(sync) DLX_L(div) DLX_L(time) DLX_L(feedback) DLX_L(space) DLX_L(tone) DLX_L(character) DLX_L(mix)
        DLX_L(freeze) DLX_L(duck) DLX_L(bypass) DLX_L(routing) DLX_L(r_ratio) DLX_L(fb_balance) DLX_L(transition) DLX_L(glide)
        DLX_L(rotation) DLX_L(rot_mode) DLX_L(in_rot) DLX_L(out_rot) DLX_L(in_width) DLX_L(out_width) DLX_L(mono_safe)
        DLX_L(low_cut) DLX_L(high_cut) DLX_L(cut_slope) DLX_L(peak_freq) DLX_L(peak_gain) DLX_L(peak_q) DLX_L(compensate) DLX_L(loop_order)
        DLX_L(mod_depth) DLX_L(mod_rate) DLX_L(mod_sync) DLX_L(mod_div) DLX_L(mod_offset) DLX_L(mod_shape) DLX_L(wow)
        DLX_L(disp_freq) DLX_L(disp_spread) DLX_L(diff_amount) DLX_L(diff_spread)
        DLX_L(warmth) DLX_L(drive) DLX_L(bits) DLX_L(rate) DLX_L(jitter) DLX_L(grit_mix) DLX_L(pre_drive)
        DLX_L(duck_thresh) DLX_L(duck_amount) DLX_L(duck_attack) DLX_L(duck_hold) DLX_L(duck_release)
        DLX_L(mix_law) DLX_L(wet_trim) DLX_L(out_trim)
        #undef DLX_L
        return v;
    }

private:
    std::atomic<float>* sync, * div, * time, * feedback, * space, * tone, * character, * mix, * freeze, * duck, * bypass,
                      * routing, * r_ratio, * fb_balance, * transition, * glide,
                      * rotation, * rot_mode, * in_rot, * out_rot, * in_width, * out_width, * mono_safe,
                      * low_cut, * high_cut, * cut_slope, * peak_freq, * peak_gain, * peak_q, * compensate, * loop_order,
                      * mod_depth, * mod_rate, * mod_sync, * mod_div, * mod_offset, * mod_shape, * wow,
                      * disp_freq, * disp_spread, * diff_amount, * diff_spread,
                      * warmth, * drive, * bits, * rate, * jitter, * grit_mix, * pre_drive,
                      * duck_thresh, * duck_amount, * duck_attack, * duck_hold, * duck_release,
                      * mix_law, * wet_trim, * out_trim;
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
    p.loopOrder = juce::jlimit (0, 3, (int) std::lround (v.loop_order));

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

    p.mix = v.mix * 0.01f;  p.equalPower = v.mix_law > 0.5f;
    p.wetTrimDb = v.wet_trim;  p.outTrimDb = v.out_trim;
    p.bypass = v.bypass > 0.5f;

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
