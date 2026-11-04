# DELEX by Piradex — matriz de parâmetros

Gerada automaticamente a partir do código (tests/test_plugin.cpp). IDs estáveis — não mudar entre versões.

| ID | Nome | Intervalo | Defeito | Curva | Automação |
|---|---|---|---|---|---|
| `sync` | Sync | off / on | On | on/off | sim |
| `div` | Division | 1/1 / 1/2D / 1/2 / 1/2T / 1/4D / 1/4 / 1/4T / 1/8D / 1/8 / 1/8T / 1/16D / 1/16 / 1/16T / 1/32 / 1/32T / 1/64 / 1/128 | 1/4 | escolha | sim |
| `time` | Time | 1.0 ms … 8.00 s | 375 ms | logarítmica (skew) | sim |
| `feedback` | Feedback | 0 % … 100 % | 40 % | linear | sim |
| `space` | Space | 0 % … 100 % | 30 % | linear | sim |
| `tone` | Tone | -100 … +100 | 0 | linear | sim |
| `character` | Character | DIGI 100 … TAPE 100 | 0 | linear | sim |
| `mix` | Mix | 0 % … 100 % | 25 % | linear | sim |
| `freeze` | Freeze | off / on | Off | on/off | sim |
| `duck` | Duck | off / on | Off | on/off | sim |
| `bypass` | Bypass | off / on | Off | on/off | sim |
| `routing` | Routing | Dual / Ping-Pong / Mono | Dual | escolha | sim |
| `r_ratio` | R Time Ratio | R x 0.25 … R x 2.00 | R x 1.00 | logarítmica (skew) | sim |
| `fb_balance` | Feedback L/R | L 100 … R 100 | C | linear | sim |
| `transition` | Time Transition | Tape Glide / Crossfade | Tape Glide | escolha | sim |
| `glide` | Glide | 10 ms … 2.00 s | 250 ms | logarítmica (skew) | sim |
| `rotation` | Rotation | -180.0° … 180.0° | 0.0° | linear | sim |
| `rot_mode` | Matrix Mode | Rotation / Reflection | Rotation | escolha | sim |
| `in_rot` | In Rotation | -45.0° … 45.0° | 0.0° | linear | sim |
| `out_rot` | Out Rotation | -45.0° … 45.0° | 0.0° | linear | sim |
| `in_width` | In Width | 0 % … 200 % | 100 % | linear | sim |
| `out_width` | Out Width | 0 % … 200 % | 100 % | linear | sim |
| `mono_safe` | Mono-Safe Bass | off / on | Off | on/off | sim |
| `low_cut` | Low Cut | 20.0 Hz … 2.00 kHz | 60.0 Hz | logarítmica (skew) | sim |
| `high_cut` | High Cut | 1.00 kHz … 20.0 kHz | 14.0 kHz | logarítmica (skew) | sim |
| `cut_slope` | Cut Slope | 6 dB/oct / 12 dB/oct / 24 dB/oct | 12 dB/oct | escolha | sim |
| `peak_freq` | Peak Freq | 100 Hz … 10.0 kHz | 1000 Hz | logarítmica (skew) | sim |
| `peak_gain` | Peak Gain | -18.0 dB … +18.0 dB | 0.0 dB | linear | sim |
| `peak_q` | Peak Q | 0.30 … 10.00 | 1.00 | logarítmica (skew) | sim |
| `compensate` | Compensate | 0 % … 100 % | 50 % | linear | sim |
| `loop_order` | Loop Order | Grit > EQ > Disp > Dif > Fita / EQ > Fita > Dif > Disp > Grit / Fita > EQ > Dif > Disp > Grit / Dif > Disp > EQ > Fita > Grit / Grit > EQ > Disp > Fita > Dif / Grit > EQ > Dif > Disp > Fita / Grit > EQ > Dif > Fita > Disp / Grit > EQ > Fita > Disp > Dif / Grit > EQ > Fita > Dif > Disp / Grit > Disp > EQ > Dif > Fita / Grit > Disp > EQ > Fita > Dif / Grit > Disp > Dif > EQ > Fita / Grit > Disp > Dif > Fita > EQ / Grit > Disp > Fita > EQ > Dif / Grit > Disp > Fita > Dif > EQ / Grit > Dif > EQ > Disp > Fita / Grit > Dif > EQ > Fita > Disp / Grit > Dif > Disp > EQ > Fita / Grit > Dif > Disp > Fita > EQ / Grit > Dif > Fita > EQ > Disp / Grit > Dif > Fita > Disp > EQ / Grit > Fita > EQ > Disp > Dif / Grit > Fita > EQ > Dif > Disp / Grit > Fita > Disp > EQ > Dif / Grit > Fita > Disp > Dif > EQ / Grit > Fita > Dif > EQ > Disp / Grit > Fita > Dif > Disp > EQ / EQ > Grit > Disp > Dif > Fita / EQ > Grit > Disp > Fita > Dif / EQ > Grit > Dif > Disp > Fita / EQ > Grit > Dif > Fita > Disp / EQ > Grit > Fita > Disp > Dif / EQ > Grit > Fita > Dif > Disp / EQ > Disp > Grit > Dif > Fita / EQ > Disp > Grit > Fita > Dif / EQ > Disp > Dif > Grit > Fita / EQ > Disp > Dif > Fita > Grit / EQ > Disp > Fita > Grit > Dif / EQ > Disp > Fita > Dif > Grit / EQ > Dif > Grit > Disp > Fita / EQ > Dif > Grit > Fita > Disp / EQ > Dif > Disp > Grit > Fita / EQ > Dif > Disp > Fita > Grit / EQ > Dif > Fita > Grit > Disp / EQ > Dif > Fita > Disp > Grit / EQ > Fita > Grit > Disp > Dif / EQ > Fita > Grit > Dif > Disp / EQ > Fita > Disp > Grit > Dif / EQ > Fita > Disp > Dif > Grit / EQ > Fita > Dif > Grit > Disp / Disp > Grit > EQ > Dif > Fita / Disp > Grit > EQ > Fita > Dif / Disp > Grit > Dif > EQ > Fita / Disp > Grit > Dif > Fita > EQ / Disp > Grit > Fita > EQ > Dif / Disp > Grit > Fita > Dif > EQ / Disp > EQ > Grit > Dif > Fita / Disp > EQ > Grit > Fita > Dif / Disp > EQ > Dif > Grit > Fita / Disp > EQ > Dif > Fita > Grit / Disp > EQ > Fita > Grit > Dif / Disp > EQ > Fita > Dif > Grit / Disp > Dif > Grit > EQ > Fita / Disp > Dif > Grit > Fita > EQ / Disp > Dif > EQ > Grit > Fita / Disp > Dif > EQ > Fita > Grit / Disp > Dif > Fita > Grit > EQ / Disp > Dif > Fita > EQ > Grit / Disp > Fita > Grit > EQ > Dif / Disp > Fita > Grit > Dif > EQ / Disp > Fita > EQ > Grit > Dif / Disp > Fita > EQ > Dif > Grit / Disp > Fita > Dif > Grit > EQ / Disp > Fita > Dif > EQ > Grit / Dif > Grit > EQ > Disp > Fita / Dif > Grit > EQ > Fita > Disp / Dif > Grit > Disp > EQ > Fita / Dif > Grit > Disp > Fita > EQ / Dif > Grit > Fita > EQ > Disp / Dif > Grit > Fita > Disp > EQ / Dif > EQ > Grit > Disp > Fita / Dif > EQ > Grit > Fita > Disp / Dif > EQ > Disp > Grit > Fita / Dif > EQ > Disp > Fita > Grit / Dif > EQ > Fita > Grit > Disp / Dif > EQ > Fita > Disp > Grit / Dif > Disp > Grit > EQ > Fita / Dif > Disp > Grit > Fita > EQ / Dif > Disp > EQ > Grit > Fita / Dif > Disp > Fita > Grit > EQ / Dif > Disp > Fita > EQ > Grit / Dif > Fita > Grit > EQ > Disp / Dif > Fita > Grit > Disp > EQ / Dif > Fita > EQ > Grit > Disp / Dif > Fita > EQ > Disp > Grit / Dif > Fita > Disp > Grit > EQ / Dif > Fita > Disp > EQ > Grit / Fita > Grit > EQ > Disp > Dif / Fita > Grit > EQ > Dif > Disp / Fita > Grit > Disp > EQ > Dif / Fita > Grit > Disp > Dif > EQ / Fita > Grit > Dif > EQ > Disp / Fita > Grit > Dif > Disp > EQ / Fita > EQ > Grit > Disp > Dif / Fita > EQ > Grit > Dif > Disp / Fita > EQ > Disp > Grit > Dif / Fita > EQ > Disp > Dif > Grit / Fita > EQ > Dif > Grit > Disp / Fita > Disp > Grit > EQ > Dif / Fita > Disp > Grit > Dif > EQ / Fita > Disp > EQ > Grit > Dif / Fita > Disp > EQ > Dif > Grit / Fita > Disp > Dif > Grit > EQ / Fita > Disp > Dif > EQ > Grit / Fita > Dif > Grit > EQ > Disp / Fita > Dif > Grit > Disp > EQ / Fita > Dif > EQ > Grit > Disp / Fita > Dif > EQ > Disp > Grit / Fita > Dif > Disp > Grit > EQ / Fita > Dif > Disp > EQ > Grit | Grit > EQ > Disp > Dif > Fita | escolha | sim |
| `mod_depth` | Mod Depth | 0.0 cents … 50.0 cents | 0.0 cents | logarítmica (skew) | sim |
| `mod_rate` | Mod Rate | 0.01 Hz … 10.00 Hz | 0.35 Hz | logarítmica (skew) | sim |
| `mod_sync` | Mod Sync | off / on | Off | on/off | sim |
| `mod_div` | Mod Division | 8 comp. / 4 comp. / 2 comp. / 1 comp. / 1/2 / 1/4 / 1/8 / 1/16 | 1 comp. | escolha | sim |
| `mod_offset` | Mod Offset | 0 % … 100 % | 50 % | linear | sim |
| `mod_shape` | Mod Shape | Sine / Triangle / Random | Sine | escolha | sim |
| `wow` | Wow & Flutter | 0 % … 100 % | 0 % | linear | sim |
| `disp_freq` | Dispersion Freq | 50.0 Hz … 5.00 kHz | 500 Hz | logarítmica (skew) | sim |
| `disp_spread` | Dispersion Spread | 0.0 ms … 50 ms | 0.0 ms | linear | sim |
| `diff_amount` | Diffusion | 0 % … 100 % | 0 % | linear | sim |
| `diff_spread` | Diffusion Spread | 1.0 ms … 50 ms | 12 ms | logarítmica (skew) | sim |
| `warmth` | Warmth | 0 % … 100 % | 20 % | linear | sim |
| `drive` | Drive | 0 % … 100 % | 0 % | linear | sim |
| `bits` | Bit Depth | 1.0 bit … Off | Off | linear | sim |
| `rate` | Sample Rate | 1.0 kHz … Off | Off | logarítmica (skew) | sim |
| `jitter` | Jitter | 0 % … 100 % | 0 % | linear | sim |
| `grit_mix` | Grit Mix | 0 % … 100 % | 100 % | linear | sim |
| `pre_drive` | Pre-Drive | -12.0 dB … +12.0 dB | 0.0 dB | linear | sim |
| `duck_thresh` | Duck Threshold | -60.0 dB … 0.0 dB | -26.0 dB | linear | sim |
| `duck_amount` | Duck Amount | 0.0 dB … 30.0 dB | 9.0 dB | linear | sim |
| `duck_attack` | Duck Attack | 0.1 ms … 100 ms | 4.0 ms | logarítmica (skew) | sim |
| `duck_hold` | Duck Hold | 0.0 ms … 500 ms | 60 ms | linear | sim |
| `duck_release` | Duck Release | 10 ms … 2.00 s | 300 ms | logarítmica (skew) | sim |
| `gate` | Gate | off / on | Off | on/off | sim |
| `gate_thresh` | Gate Threshold | -60.0 dB … 0.0 dB | -30.0 dB | linear | sim |
| `gate_hold` | Gate Hold | 10 ms … 1.00 s | 250 ms | logarítmica (skew) | sim |
| `gate_release` | Gate Release | 2.0 ms … 500 ms | 40 ms | logarítmica (skew) | sim |
| `mix_law` | Mix Law | Linear / Equal Power | Equal Power | escolha | sim |
| `wet_trim` | Wet Trim | -24.0 dB … +12.0 dB | 0.0 dB | linear | sim |
| `out_trim` | Output | -24.0 dB … +12.0 dB | 0.0 dB | linear | sim |
| `key_src` | Key Source | Internal / Sidechain | Internal | escolha | sim |
| `monitor` | Monitor | Normal / Wet Solo / Delta / Mono / Phone / Club | Normal | escolha | sim |
| `const_loud` | Constant Loudness | off / on | Off | on/off | sim |
| `ls_freq` | Low Shelf Freq | 30.0 Hz … 1.00 kHz | 150 Hz | logarítmica (skew) | sim |
| `ls_gain` | Low Shelf Gain | -18.0 dB … +18.0 dB | 0.0 dB | linear | sim |
| `hs_freq` | High Shelf Freq | 1.00 kHz … 16.0 kHz | 6.00 kHz | logarítmica (skew) | sim |
| `hs_gain` | High Shelf Gain | -18.0 dB … +18.0 dB | 0.0 dB | linear | sim |
| `peak2_freq` | Peak 2 Freq | 100 Hz … 10.0 kHz | 3.00 kHz | logarítmica (skew) | sim |
| `peak2_gain` | Peak 2 Gain | -18.0 dB … +18.0 dB | 0.0 dB | linear | sim |
| `peak2_q` | Peak 2 Q | 0.30 … 10.00 | 1.00 | logarítmica (skew) | sim |
| `hiss` | Tape Hiss | 0 % … 100 % | 0 % | linear | sim |
| `sat_hq` | Saturation HQ (2x) | off / on | Off | on/off | sim |
