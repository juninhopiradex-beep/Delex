#!/usr/bin/env python3
"""
DELEX by Piradex — gera Source/PresetsGuia.inc a partir do guia
"Delay FX · 200 presets profissionais" (Beatfreak Music / Piradex, Nov. 2026).

  python3 tools/presets_guia.py

tools/guia_200.json tem os valores do guia (algoritmo, tempo, feedback, wet, HPF/LPF,
largura, modulação, ducking, aplicação). Cada preset é traduzido para o motor do DELEX:
modelo, filtros do loop, largura, modulação, ducker e, conforme o nome, as funções do
DELEX que fazem esse som (taps, grooves, pitch, reverse, throws, unmask, matriz…).
Os nomes do guia mantêm-se, excepto os que citam pessoas ou marcas reais.
"""
import json, os, re

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
G = json.load(open(os.path.join(ROOT, "tools", "guia_200.json"), encoding="utf-8"))

RENAME = { 32: "Tape Space Style", 112: "E-Piano Vintage Delay", 172: "Dub Royal Style" }

CAT = {  # categoria do guia → (categoria DELEX, subcategoria)
    "Essential & Classic Delays": ("Classic", ""), "Vocal Delays": ("Vocals", "Studio"),
    "Stereo & Spatial": ("Stereo", ""), "Tape & Vintage": ("Vintage", "Tape"), "Analog & BBD": ("Vintage", "BBD"),
    "Rhythmic & Groove": ("Rhythmic", ""), "Genre: African Music": ("Afro", ""), "Trap, Hip-Hop & R&B": ("Trap", "Hip-Hop & R&B"),
    "House, Techno & EDM": ("EDM", ""), "Guitar & Acoustic": ("Guitar", "Acoustic & Electric"),
    "Drum & Percussion": ("Drums", "Groove"), "Synth, Keys & Piano": ("Synth", "Keys"),
    "Ambient & Cinematic": ("Ambient", "Cinematic"), "Pitch & Harmonic": ("Pitch", ""),
    "Reverse & Experimental": ("Reverse", ""), "Glitch & Creative FX": ("FX", "Glitch"),
    "Modulation & Movement": ("Modulation", ""), "Dub & Feedback": ("Dub", "Feedback"),
    "Mixing & Utility": ("Mixing", ""), "Signature & Next Gen": ("Next Gen", ""),
}
AFRO_SUB = { 61: "Amapiano", 62: "Amapiano", 63: "Kizomba", 64: "Zouk", 65: "Afrobeats", 66: "Afro House",
             67: "Kuduro", 68: "Semba", 69: "Gqom", 70: "Afrobeats" }

USE = {
    "Ambientes cinematográficos e sound design.": "Cinematic ambiences and sound design.",
    "Bateria e percussão.": "Drums and percussion.",
    "Coloração vintage e ambiente orgânico.": "Vintage colour and organic ambience.",
    "Criação de padrões rítmicos sincronizados.": "Building synced rhythmic patterns.",
    "Dance music e transições de energia.": "Dance music and energy transitions.",
    "Dub, ressonância e efeitos ao vivo.": "Dub, resonance and live effects.",
    "Estéreo e profundidade da produção.": "Stereo and depth of the production.",
    "Exploração de técnicas adaptativas.": "Exploring adaptive techniques.",
    "Glitches e manipulação de buffers.": "Glitches and buffer manipulation.",
    "Guitarras e fontes acústicas.": "Guitars and acoustic sources.",
    "Harmonias e ecos afinados.": "Harmonies and pitched echoes.",
    "Mistura geral de instrumentos e vozes.": "General mixing of instruments and vocals.",
    "Movimento e modulação criativa.": "Movement and creative modulation.",
    "Produção africana e grooves sincopados.": "African production and syncopated grooves.",
    "Sintetizadores, teclas e piano.": "Synths, keys and piano.",
    "Textura quente, escura e analógica.": "Warm, dark, analog texture.",
    "Transições e ecos invertidos.": "Transitions and reversed echoes.",
    "Utilidade de mistura e transparência.": "Mixing utility and transparency.",
    "Vocal: throws com espaço entre frases e controlo de sibilância.": "Vocal: throws with space between phrases and controlled sibilance.",
    "Voz e instrumentais urbanos.": "Vocals and urban instrumentals.",
    "Voz principal, adlibs ou finais de frase.": "Lead vocal, adlibs or phrase endings.",
}

ALGO = {  # algoritmo do guia → (dados, descrição PT, descrição EN)
    "Digital":            ("model=Tape;warmth=0", "Eco digital limpo", "Clean digital echo"),
    "Analog":             ("model=BBD;warmth=30", "Eco analógico quente", "Warm analog echo"),
    "BBD":                ("model=BBD;warmth=20", "Eco BBD escuro", "Dark BBD echo"),
    "Tape":               ("model=Tape;drive=25;warmth=40;wow=20", "Eco de fita", "Tape echo"),
    "Lo-Fi Tape":         ("model=Cassette;drive=20;hiss=25;wow=35", "Cassete gasta", "Worn cassette"),
    "Tape / Digital":     ("model=Tape;drive=12;warmth=25", "Fita com precisão digital", "Tape with digital precision"),
    "Tape / Stereo":      ("model=Tape;drive=10;warmth=30", "Fita estéreo", "Stereo tape"),
    "Dub / Tape":         ("model=Tape;drive=35;warmth=45;wow=15", "Fita de dub", "Dub tape"),
    "Dub / Multi-Tap":    ("model=Multi-Head;heads=1+2+3;drive=20;warmth=30", "Fita multi-cabeça", "Multi-head tape"),
    "Multi-Tap":          ("model=Tape;warmth=0;taps_mode=Multi-Tap;taps_level=85", "Multi-tap", "Multi-tap"),
    "Digital / Multi-Tap":("model=Tape;warmth=0;taps_mode=Multi-Tap;taps_level=80", "Taps rítmicos", "Rhythmic taps"),
    "Stereo / Multi-Tap": ("model=Tape;warmth=0", "Eco espacial", "Spatial echo"),
    "Ping-Pong":          ("model=Tape;warmth=0;routing=Ping-Pong", "Ping-pong", "Ping-pong"),
    "Chorus Delay":       ("model=BBD;mod_depth=15;mod_rate=1;mod_offset=50", "Eco com chorus", "Chorus echo"),
    "Flanger Delay":      ("model=Tape;warmth=0;mod_depth=30;mod_rate=0.25", "Flanger", "Flanger"),
    "Phaser Delay":       ("model=Tape;warmth=0;disp_spread=22;disp_freq=900;mod_depth=8;mod_rate=0.5", "Eco com phaser", "Phaser echo"),
    "Modulated":          ("model=Tape;warmth=10;mod_depth=10;mod_rate=1", "Eco modulado", "Modulated echo"),
    "Digital / Diffuse":  ("model=Tape;warmth=0;diff_amount=45;diff_spread=14", "Eco digital difuso", "Diffuse digital echo"),
    "Diffuse / Granular": ("model=Tape;warmth=10;diff_amount=80;diff_spread=30;mod_shape=Random;mod_depth=8;mod_rate=0.6", "Nuvem difusa", "Diffuse cloud"),
    "Pitch":              ("model=Tape;warmth=0;pitch_mix=40", "Eco afinado", "Pitched echo"),
    "Pitch + Diffusion":  ("model=Tape;warmth=0;pitch_mix=40;diff_amount=60;diff_spread=20", "Eco afinado e difuso", "Pitched, diffuse echo"),
    "Reverse":            ("model=Tape;warmth=0;reverse=100", "Eco invertido", "Reversed echo"),
    "Granular / Buffer":  ("model=Digital 80s;bits=12;rate=24000;grit_mix=70;jitter=20", "Buffer digital partido", "Broken digital buffer"),
    "Digital / Dynamic":  ("model=Tape;warmth=0;unmask=40", "Eco dinâmico", "Dynamic echo"),
    "Adaptive / Hybrid":  ("model=Tape;warmth=10", "Eco adaptativo", "Adaptive echo"),
}

TEMPO = {
    "1/4": ("div=1/4", "semínima", "quarter note"), "1/8": ("div=1/8", "colcheia", "eighth note"),
    "1/8.": ("div=1/8D", "colcheia pontuada", "dotted eighth"), "1/4T": ("div=1/4T", "tercina de semínima", "quarter triplet"),
    "1/2": ("div=1/2", "mínima", "half note"), "1/16": ("div=1/16", "semicolcheia", "sixteenth"),
    "1/32": ("div=1/32", "fusa", "thirty-second"),
    "L 1/8 / R 1/4": ("div=1/8;r_ratio=2", "colcheia à esquerda e semínima à direita", "eighth left, quarter right"),
    "85 ms": ("sync=0;time=85", "85 ms", "85 ms"), "108 ms": ("sync=0;time=108", "108 ms", "108 ms"),
    "75-125 ms": ("sync=0;time=100", "slap de 100 ms", "100 ms slap"), "5-18 ms": ("sync=0;time=7", "7 ms", "7 ms"),
    "14-25 ms": ("sync=0;time=19", "19 ms", "19 ms"),
}
MOD = { "0.0-0.3 Hz": "", "0.4-1.2 Hz": "mod_depth=8;mod_rate=0.8", "0.3-3 Hz": "mod_depth=12;mod_rate=1.2",
        "0.5 Hz": "mod_depth=5;mod_rate=0.5", "0.5-1.5 Hz": "mod_depth=12;mod_rate=1" }
DUCK = { "0": None, "4": 4, "3-6": 5, "4-8": 6, "6-9": 8 }

# Funções do DELEX por preset: (dados extra, frase PT, frase EN)
X = {
    5: ("model=Tape;drive=10;warmth=25", "som clássico de rockabilly e rádio", "classic rockabilly and radio sound"),
    6: ("routing=Mono", "um só eco centrado", "a single centred echo"),
    7: ("", "dois tempos que se respondem", "two times answering each other"),
    8: ("mod_depth=5;mod_rate=0.5", "repetições que escurecem", "repeats that darken"),
    9: ("diff_amount=20", "cauda longa", "long tail"),
    10: ("duck=1;duck_amount=6", "ducking para a voz respirar", "ducking so the vocal breathes"),
    14: ("throw_auto=1;throw_only=1;throw_len=280", "Auto-Throw: só o fim das frases vai para o delay", "Auto-Throw: only phrase endings go into the delay"),
    15: ("routing=Ping-Pong;r_ratio=1", "adlibs a saltar entre os lados", "adlibs bouncing between sides"),
    16: ("taps_mode=Multi-Tap;groove=Afrobeats 3-3-2;taps_level=60", "groove 3-3-2 nos taps", "3-3-2 groove in the taps"),
    17: ("diff_amount=25;warmth=30", "cauda romântica e quente", "warm, romantic tail"),
    18: ("diff_amount=40;high_cut=9000;low_cut=400", "eco sussurrado e arejado", "airy, whispered echo"),
    19: ("time=24;r_ratio=1.35;feedback=0;mod_depth=6;mod_rate=0.7", "dobra a voz sem se ouvir como eco", "doubles the vocal without sounding like an echo"),
    20: ("throw_auto=1;throw_only=1;throw_len=400;diff_amount=30", "Auto-Throw para finais de música", "Auto-Throw for song endings"),
    21: ("r_ratio=1.5;rotation=20", "estéreo largo em 3:2", "wide 3:2 stereo"),
    22: ("routing=Ping-Pong", "ping-pong clássico", "classic ping-pong"),
    23: ("routing=Ping-Pong;out_width=180;mono_safe=1", "ping-pong extra largo, graves em mono", "extra-wide ping-pong, mono lows"),
    24: ("in_rot=45;out_rot=-45", "eco processado em mid/side", "echo processed in mid/side"),
    25: ("rotation=30", "os ecos cruzam de lado a cada repetição", "echoes cross sides on every repeat"),
    26: ("rotation=60;diff_amount=20", "espiral estéreo", "stereo spiral"),
    27: ("routing=Ping-Pong", "esquerda e direita em perseguição", "left and right chasing"),
    28: ("mm1_src=LFO;mm1_dst=Rotation;mm1_amt=35;mm_rate=0.3", "rotação lenta pela matriz (LFO)", "slow rotation via the matrix (LFO)"),
    29: ("r_ratio=1.6;feedback=0;out_width=160;mono_safe=1", "efeito Haas: largura sem eco audível", "Haas effect: width without an audible echo"),
    30: ("rotation=90", "cada eco roda 90°", "each echo rotates 90°"),
    31: ("wow=30", "fita dos anos 70", "70s tape"),
    32: ("model=Multi-Head;heads=1+3;wow=25", "eco de fita com duas cabeças", "two-head tape echo"),
    33: ("hiss=35", "cassete gasta com hiss", "worn cassette with hiss"),
    34: ("drive=65", "cabeça de fita suja e saturada", "dirty, saturated tape head"),
    35: ("warmth=65;drive=15", "bobina quente", "warm reel"),
    36: ("wow=65", "flutter forte", "heavy flutter"),
    37: ("low_cut=300;feedback=45", "dub da velha escola", "old-school dub"),
    38: ("drive=70;sat_hq=1", "saturação forte com HQ 2x", "heavy saturation with HQ 2x"),
    39: ("bits=12;rate=22000;grit_mix=50", "fita lo-fi com grão digital", "lo-fi tape with digital grain"),
    40: ("model=Multi-Head;heads=1+2+3", "ritmo de três cabeças", "three-head rhythm"),
    41: ("tone=-20", "brigade escura", "dark brigade"),
    43: ("mod_depth=15;mod_rate=1", "BBD com chorus", "BBD with chorus"),
    44: ("tone=-35", "repetições muito escuras", "very dark repeats"),
    45: ("drive=45", "BBD saturado", "driven BBD"),
    46: ("mod_depth=6;mod_rate=0.6", "eco de pedal clássico com modulação", "classic pedal echo with modulation"),
    47: ("drive=20", "pedal vintage", "vintage pedal"),
    48: ("peak_freq=1500;peak_gain=8;peak_q=3;compensate=30", "pico ressonante no loop", "resonant peak in the loop"),
    49: ("hiss=55", "circuito ruidoso", "noisy circuit"),
    50: ("feedback=60;compensate=20", "feedback analógico generoso", "generous analog feedback"),
    51: ("taps=1:0.7:-0.5,2:0.55:0.5,3:0.4:-0.5,4:0.3:0.5;taps_grid=1/16", "taps em semicolcheias", "sixteenth-note taps"),
    52: ("taps=3:0.7:-0.5,6:0.5:0.5,9:0.35:-0.5,12:0.25:0.5;taps_grid=1/16", "taps em colcheia pontuada", "dotted-eighth taps"),
    53: ("taps=4:0.7:-0.4,8:0.5:0.4,12:0.35:-0.4;taps_grid=1/16T", "taps em tercina", "triplet taps"),
    54: ("taps=2:0.6:-0.4,3:0.5:0.4,6:0.4:-0.4,7:0.3:0.4;taps_grid=1/16;taps_swing=60", "taps com swing", "swung taps"),
    55: ("taps=2:0.65:-0.4,6:0.5:0.4,8:0.4:-0.4,12:0.3:0.4;taps_grid=1/16T", "shuffle em tercinas", "triplet shuffle"),
    56: ("taps=4:0.65:-0.4,8:0.5:0.4,12:0.4:0,16:0.3:-0.4,20:0.25:0.4;taps_grid=1/16T", "polirritmia 3 contra 4", "3-against-4 polyrhythm"),
    57: ("taps=2:0.7:-0.5,6:0.55:0.5,10:0.4:-0.5,14:0.3:0.5;taps_grid=1/16", "ecos no contratempo", "off-beat echoes"),
    58: ("taps=3:0.7:-0.4,5:0.5:0.4,10:0.45:-0.3,13:0.35:0.3;taps_grid=1/16", "groove sincopado", "syncopated groove"),
    59: ("taps=1:0.5:-0.6,3:0.6:0.6,4:0.45:-0.3,6:0.5:0.3,9:0.35:-0.6,11:0.3:0.6;taps_grid=1/16", "padrão de percussão em taps", "percussion pattern in taps"),
    60: ("taps=3:0.65:-0.4,6:0.55:0.4,10:0.45:-0.4,13:0.35:0.4;taps_grid=1/16", "ritmo euclidiano (5 em 16)", "Euclidean rhythm (5 in 16)"),
    61: ("groove=Amapiano (log drum);model=BBD", "groove de log drum do amapiano", "amapiano log-drum groove"),
    62: ("taps=2:0.6:-0.5,3:0.45:0.5,6:0.55:-0.3,10:0.5:0.3,14:0.35:0;taps_grid=1/16;taps_swing=30", "percussão de amapiano com swing", "swung amapiano percussion"),
    63: ("groove=Kizomba;warmth=30;duck=1;duck_amount=5", "groove de kizomba com ducking", "kizomba groove with ducking"),
    64: ("taps=3:0.6:-0.4,6:0.5:0.4,8:0.55:0,14:0.35:0.3;taps_grid=1/16;warmth=20", "fluxo de guitarra de zouk", "zouk guitar flow"),
    65: ("groove=Afrobeats 3-3-2", "guitarra de afrobeats em 3-3-2", "afrobeats guitar in 3-3-2"),
    66: ("groove=Afro House;div=1/4D;sync=1", "pulso de afro house", "afro house pulse"),
    67: ("groove=Kuduro", "stutter de kuduro", "kuduro stutter"),
    68: ("groove=Semba;warmth=20", "espaço de guitarra de semba", "semba guitar space"),
    69: ("groove=Gqom;tone=-20", "eco escuro de gqom", "dark gqom echo"),
    70: ("taps_mode=Off;throw_auto=1;throw_only=1;throw_len=260", "Auto-Throw para vozes afro", "Auto-Throw for afro vocals"),
    71: ("tone=-15;duck=1;duck_amount=5", "eco escuro de trap", "dark trap echo"),
    72: ("routing=Ping-Pong", "salto de drill", "drill bounce"),
    73: ("routing=Mono;mono_safe=1", "eco fantasma de 808, em mono", "ghost 808 echo, in mono"),
    74: ("routing=Ping-Pong;high_cut=9000", "hi-hats espalhados", "scattered hi-hats"),
    75: ("warmth=45;diff_amount=25", "veludo de R&B", "R&B velvet"),
    76: ("warmth=55;drive=15", "calor de neo soul", "neo-soul warmth"),
    77: ("drive=30;bits=14;grit_mix=40", "fita de boom bap", "boom-bap tape"),
    78: ("throw_auto=1;throw_only=1;throw_len=220", "Auto-Throw com punch", "punchy Auto-Throw"),
    79: ("diff_amount=40;space=50;duck=1;duck_amount=5", "espaço de trap melódico", "melodic trap space"),
    80: ("div=1/2;feedback=30", "eco em meio tempo", "half-time echo"),
    81: ("duck=1;duck_amount=4", "delay de deep house", "deep house delay"),
    83: ("diff_amount=50;low_cut=300", "túnel de techno dub", "dub techno tunnel"),
    84: ("mm1_src=LFO;mm1_dst=Feedback;mm1_amt=20;mm_rate=0.05", "build progressivo (feedback a subir e descer)", "progressive build (feedback rising and falling)"),
    85: ("routing=Ping-Pong;mod_depth=6;mod_rate=0.3", "hipnótico de trance", "hypnotic trance"),
    86: ("", "ping-pong de EDM", "EDM ping-pong"),
    87: ("pitch=1;pitch_mix=30", "riser: cada repetição sobe um semitom", "riser: each repeat rises a semitone"),
    88: ("peak_freq=1200;peak_gain=10;peak_q=5;compensate=30;mm1_src=LFO;mm1_dst=Peak Freq;mm1_amt=40;mm_rate=0.25", "ressonância ácida em movimento", "moving acid resonance"),
    90: ("throw_auto=1;throw_only=1;throw_len=350", "throw de festival", "festival throw"),
    91: ("model=Tape;drive=15", "slap de rockabilly", "rockabilly slap"),
    93: ("diff_amount=50;mm1_src=Envelope;mm1_dst=Mix;mm1_amt=-40", "swell: os ecos crescem depois do ataque", "swell: echoes grow after the attack"),
    94: ("out_width=150;r_ratio=1.5", "guitarra limpa e larga", "clean, wide guitar"),
    97: ("routing=Ping-Pong;low_cut=400", "eco de skank de reggae", "reggae skank echo"),
    98: ("warmth=40;diff_amount=25", "espaço de guitarra de jazz", "jazz guitar space"),
    99: ("diff_amount=80;reverse=20;mod_depth=15;mod_rate=0.4", "parede de shoegaze", "shoegaze wall"),
    100: ("duck=1;duck_amount=5", "cauda para solos", "tail for solos"),
    101: ("taps_mode=Off;model=Tape;drive=10", "slap de caixa", "snare slap"),
    102: ("taps_mode=Off;routing=Ping-Pong", "palmas em estéreo", "stereo claps"),
    103: ("taps=2:0.7:-0.6:0.9,4:0.55:-0.2:0.7,6:0.45:0.2:0.55,8:0.35:0.6:0.4;taps_grid=1/16", "cascata de timbalões a escurecer", "darkening tom cascade"),
    104: ("groove=Semba", "groove de percussão", "percussion groove"),
    105: ("taps_mode=Off;routing=Mono;mono_safe=1", "eco fantasma do bombo", "ghost kick echo"),
    106: ("taps_mode=Off", "hi-hat em colcheia pontuada", "dotted-eighth hi-hat"),
    107: ("taps_mode=Off;model=Multi-Head;heads=1+2+3;drive=20", "dub de bateria", "drum dub"),
    108: ("taps_mode=Off;throw_auto=1;throw_only=1;throw_len=120", "throw de aro", "rimshot throw"),
    109: ("reverse=100", "caixa invertida", "reversed snare"),
    110: ("taps=1:0.4:-0.7,3:0.5:0.6,5:0.35:-0.2,7:0.45:0.5,10:0.3:-0.6,13:0.25:0.7;taps_grid=1/16;mm1_src=Random;mm1_dst=Rotation;mm1_amt=40;mm_rate=4", "percussão espalhada ao acaso", "randomly scattered percussion"),
    111: ("diff_amount=55;tone=-10", "piano de sonho", "dreamy piano"),
    112: ("model=Tape;drive=15;warmth=40;wow=15", "piano eléctrico vintage", "vintage electric piano"),
    113: ("routing=Ping-Pong", "arpejo em movimento", "moving arpeggio"),
    114: ("routing=Ping-Pong;diff_amount=20", "plucks a saltar", "bouncing plucks"),
    115: ("diff_amount=75;diff_spread=30", "pad infinito", "infinite pad"),
    116: ("r_ratio=1.5", "teclas largas", "wide keys"),
    117: ("pitch=12;pitch_mix=35;diff_amount=50", "sinos com shimmer", "shimmering bells"),
    118: ("routing=Ping-Pong", "lead em perseguição", "chasing lead"),
    119: ("model=Tape;drive=20;warmth=40;wow=20", "piano eléctrico em fita", "electric piano on tape"),
    120: ("taps_mode=Multi-Tap;taps=3:0.6:-0.4,6:0.5:0.4,8:0.45:0,11:0.35:-0.3,14:0.3:0.3;taps_grid=1/16", "sequenciador em taps", "sequencer in taps"),
    121: ("diff_amount=90", "atmosfera infinita", "infinite atmosphere"),
    122: ("rotation=45", "eco galáctico", "galactic echo"),
    123: ("space=60", "rastos cinematográficos", "cinematic trails"),
    124: ("tone=-15;mod_depth=10", "nuvem de sonho", "dream cloud"),
    125: ("feedback=96", "espaço congelado", "frozen space"),
    126: ("tone=-40", "ambiente escuro", "dark ambience"),
    127: ("pitch=12;pitch_mix=20", "véu etéreo", "ethereal veil"),
    128: ("mod_depth=20;mod_rate=0.3", "deriva cósmica", "cosmic drift"),
    129: ("reverse=30;pitch=-12;pitch_mix=20", "transição de filme", "film transition"),
    130: ("reverse=30;mod_depth=15", "universo granular", "granular universe"),
    131: ("pitch=12", "oitava acima a cada repetição", "an octave up on every repeat"),
    132: ("pitch=-12", "oitava abaixo a cada repetição", "an octave down on every repeat"),
    133: ("pitch=7", "quinta acima", "a fifth up"),
    134: ("pitch=3", "terceira menor acima", "a minor third up"),
    135: ("pitch=4", "terceira maior acima", "a major third up"),
    136: ("pitch=12;pitch_mix=45", "shimmer clássico", "classic shimmer"),
    137: ("pitch_mix=0;taps_mode=Multi-Tap;taps_level=80;taps=4:0.7:-0.5:1:3,8:0.6:0.5:1:4;taps_grid=1/16", "dois taps: oitava acima e abaixo", "two taps: an octave up and down"),
    138: ("pitch=-2;pitch_mix=50", "cada repetição desce", "each repeat dives"),
    139: ("pitch=2;pitch_mix=50", "cada repetição sobe", "each repeat rises"),
    140: ("pitch_mix=0;taps_mode=Echo Sculpt;sculpt=0.8:-0.3:1,0.65:0.3:2,0.5:-0.3:3,0.35:0.3:0", "repetições harmonizadas (3.ª, 5.ª, oitava) no tom", "harmonised repeats (3rd, 5th, octave) in key"),
    142: ("reverse=80;diff_amount=30", "fantasma invertido da voz", "reversed vocal ghost"),
    143: ("feedback=40", "swell invertido", "reverse swell"),
    144: ("reverse=80;pitch=12;pitch_mix=30", "cascata invertida e afinada", "pitched reverse cascade"),
    145: ("rotation=45", "fluxo estéreo invertido", "reversed stereo flow"),
    146: ("reverse=50", "repetição fantasma", "ghost repetition"),
    147: ("reverse=70;diff_amount=50;tone=-20", "sonho ao contrário", "backward dream"),
    148: ("reverse=70;pitch=12;pitch_mix=35;diff_amount=50", "shimmer invertido", "reverse shimmer"),
    150: ("reverse=70;diff_amount=80", "atmosfera invertida", "reversed atmosphere"),
    151: ("grit_mix=60", "repetição glitch", "glitch repeat"),
    152: ("div=1/32;jitter=40", "micro-stutter", "micro stutter"),
    153: ("bits=6;rate=8000", "colapso digital", "digital breakdown"),
    154: ("taps_mode=Multi-Tap;taps=1:0.5:-0.6,2:0.4:0.6,5:0.45:-0.2,7:0.35:0.5,9:0.3:-0.5,12:0.25:0.6;taps_grid=1/16;mm1_src=Random;mm1_dst=Time;mm1_amt=25;mm_rate=4", "taps aleatórios a cada passo", "random taps on every step"),
    155: ("bits=8;rate=16000", "eco bitcrushed", "bitcrushed echo"),
    156: ("model=Tape;bits=24;rate=48000;grit_mix=0;wow=60;mm1_src=LFO;mm1_dst=Time;mm1_amt=-50;mm_rate=0.25", "o tempo afunda como uma fita a parar", "the time sinks like a stopping tape"),
    157: ("reverse=40;mod_shape=Random;mod_depth=30;mod_rate=3;diff_amount=40", "grãos espalhados", "scattered grains"),
    158: ("feedback=96;diff_amount=50", "buffer congelado", "frozen buffer"),
    159: ("model=Cassette;jitter=80;rate=6000;wow=80", "máquina avariada", "broken machine"),
    160: ("mm1_src=Random;mm1_dst=Feedback;mm1_amt=40;mm2_src=Random;mm2_dst=Rotation;mm2_amt=80;mm3_src=Random;mm3_dst=Time;mm3_amt=20;mm_rate=3", "motor de caos (matriz aleatória)", "chaos engine (random matrix)"),
    162: ("time=6;feedback=60", "flanger com feedback", "flanger with feedback"),
    163: ("", "phaser nas repetições", "phaser on the repeats"),
    164: ("mm1_src=LFO;mm1_dst=Mix;mm1_amt=30;mm_rate=5", "tremolo nos ecos", "tremolo on the echoes"),
    165: ("mm1_src=LFO;mm1_dst=Rotation;mm1_amt=50;mm_rate=0.5", "auto-pan pela matriz", "auto-pan via the matrix"),
    166: ("mm1_src=LFO;mm1_dst=High Cut;mm1_amt=40;mm_rate=0.3", "filtro a varrer (LFO)", "sweeping filter (LFO)"),
    167: ("mm1_src=Envelope;mm1_dst=Feedback;mm1_amt=40", "o feedback segue a dinâmica", "feedback follows the dynamics"),
    168: ("peak_freq=1000;peak_gain=9;peak_q=4;peak2_freq=2500;peak2_gain=6;peak2_q=4;compensate=40;mm1_src=LFO;mm1_dst=Peak Freq;mm1_amt=30;mm_rate=0.4", "formantes de vogal em movimento", "moving vowel formants"),
    169: ("mm1_src=LFO;mm1_dst=Rotation;mm1_amt=80;mm_rate=6;mod_rate=6", "coluna rotativa", "rotary speaker"),
    170: ("mm1_src=LFO;mm1_dst=Feedback;mm1_amt=25;mm_rate=0.2", "feedback modulado", "modulated feedback"),
    171: ("peak_freq=900;peak_gain=12;peak_q=4;compensate=0;feedback=85;mm1_src=LFO;mm1_dst=Peak Freq;mm1_amt=50;mm_rate=0.5", "sirene de dub", "dub siren"),
    172: ("low_cut=300;high_cut=2500;peak_freq=800;peak_gain=6;compensate=40", "dub de mesa clássico, médios à frente", "classic desk dub, mids forward"),
    173: ("model=Multi-Head;heads=1+2+3", "dub espacial de três cabeças", "three-head space dub"),
    174: ("disp_spread=20;disp_freq=1200;diff_amount=30", "mola de dub com chirp", "chirping dub spring"),
    175: ("feedback=90;compensate=20;peak_gain=6;peak_freq=1200", "explosão de feedback controlada", "controlled feedback explosion"),
    176: ("low_cut=500;high_cut=2000", "dub filtrado", "filtered dub"),
    177: ("throw_only=1;throw_auto=1;throw_len=300", "throw de drop de dub", "dub drop throw"),
    178: ("model=Multi-Head;heads=1+2+3;diff_amount=40", "câmara de fita de reggae", "reggae tape chamber"),
    179: ("feedback=94", "loop de dub infinito", "infinite dub loop"),
    180: ("mm1_src=Envelope;mm1_dst=Feedback;mm1_amt=-30", "feedback que cede quando a fonte toca", "feedback that backs off when the source plays"),
    181: ("duck_amount=8", "ducking forte para voz", "strong vocal ducking"),
    182: ("unmask=0", "ducking limpo (pronto para sidechain)", "clean ducking (sidechain-ready)"),
    183: ("unmask=70;duck_amount=4", "ducking por frequência (Spectral Unmask)", "frequency ducking (Spectral Unmask)"),
    184: ("out_width=110;guard=1;guard_amt=60;mono_safe=1", "largura segura em mono (Translation Guard)", "mono-safe width (Translation Guard)"),
    185: ("low_cut=400", "graves limpos nos ecos", "clean lows in the echoes"),
    186: ("tone=-10", "eco escuro de fundo", "dark background echo"),
    187: ("duck_attack=0.5;duck_hold=20", "respeita os transientes", "keeps the transients"),
    188: ("diff_amount=30;space=40", "profundidade subtil", "subtle depth"),
    189: ("low_cut=500;cut_slope=24 dB/oct", "repetições cada vez mais finas", "ever-thinner repeats"),
    190: ("guard=1;mono_safe=1", "FX de master limpo e seguro", "clean, safe master FX"),
    191: ("unmask=50;guard=1;mm1_src=Envelope;mm1_dst=Mix;mm1_amt=-30", "adapta-se: recua quando a fonte toca, cresce nas pausas", "adapts: backs off while the source plays, grows in the gaps"),
    192: ("throw_auto=1;throw_only=1;throw_len=280;unmask=40", "throw inteligente da última palavra", "smart throw of the last word"),
    193: ("taps_mode=Multi-Tap;groove=Afro House;taps_level=60;mm1_src=Envelope;mm1_dst=Feedback;mm1_amt=25", "groove que responde à dinâmica", "a groove that answers the dynamics"),
    194: ("unmask=80;diff_amount=60;pitch=12;pitch_mix=15", "fantasma espectral", "spectral ghost"),
    195: ("mm1_src=LFO;mm1_dst=Time;mm1_amt=15;mm2_src=LFO;mm2_dst=Tone;mm2_amt=40;mm_rate=0.1;transition=Stretch", "o delay transforma-se devagar", "the delay slowly morphs"),
    196: ("mm1_src=LFO;mm1_dst=Rotation;mm1_amt=40;mm_rate=0.2;guard=1", "movimento estéreo protegido em mono", "mono-protected stereo motion"),
    197: ("taps_mode=Echo Sculpt;sculpt=0.8:-0.3:1,0.65:0.3:2,0.5:-0.3:3", "harmonia inteligente no tom detectado", "intelligent harmony in the detected key"),
    198: ("duck=1;duck_amount=6;duck_attack=0.3;duck_hold=10;duck_release=120", "atento aos transientes", "transient-aware"),
    199: ("taps_mode=Multi-Tap;taps=2:0.5:-0.5,3:0.4:0.5,6:0.55:0,9:0.35:-0.5,11:0.3:0.5,14:0.4:0;taps_grid=1/16;mm1_src=Random;mm1_dst=Rotation;mm1_amt=60;mm2_src=Random;mm2_dst=Feedback;mm2_amt=20;mm_rate=2", "ritmo generativo que nunca se repete igual", "generative rhythm that never repeats the same"),
    200: ("pitch=12;pitch_mix=25;diff_amount=70;reverse=20;guard=1;space=60", "shimmer, difusão e reverse num só motor", "shimmer, diffusion and reverse in one engine"),
}

def merge(*parts):
    kv = {}
    for p in parts:
        if not p: continue
        for t in p.split(";"):
            k, v = t.split("=", 1); kv[k] = v
    # tempos em ms → sem sync
    if "time" in kv and "div" not in kv: kv["sync"] = "0"
    if kv.get("sync") == "1": kv.pop("time", None)
    return kv

def c(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'

def main():
    ids = set(re.findall(r"X\((\w+)\)", open(os.path.join(ROOT, "Source", "Params.h"), encoding="utf-8").read()))
    extra = {"groove", "taps", "sculpt"}
    out, names = [], set()
    for p in G:
        n = p["n"]
        name = RENAME.get(n, p["name"])
        cat, sub = CAT[p["cat"]]
        if cat == "Afro": sub = AFRO_SUB.get(n, "")
        algo_d, apt, aen = ALGO[p["algo"]]
        tdat, tpt, ten = TEMPO[p["tempo"]]
        base = f"{tdat};feedback={p['fb']};mix={p['wet']};low_cut={min(2000, max(20, p['hpf']))};high_cut={min(20000, max(1000, p['lpf']))}"
        if p["width"] != 100: base += f";out_width={p['width'] if p['width'] > 0 else 0}"
        if p["width"] == 0: base += ";routing=Mono"
        d = DUCK[p["duck"]]
        duck = f"duck=1;duck_amount={d}" if d else ""
        x, fpt, fen = X.get(n, ("", "", ""))
        kv = merge(algo_d, base, MOD[p["mod"]], duck, x)
        if kv.get("taps_mode") == "Multi-Tap" and not any(k in kv for k in ("taps", "groove")):
            kv["taps"] = "3:0.6:-0.4,6:0.45:0.4,8:0.35:0"; kv.setdefault("taps_grid", "1/16")
        for k in kv:
            assert k in ids or k in extra, f"{n} {name}: parâmetro desconhecido {k}"
        data = ";".join(f"{k}={v}" for k, v in kv.items())
        fb = kv.get("feedback", p["fb"])
        dpt = f"{apt} em {tpt}, feedback {fb} %" + (f": {fpt}." if fpt else ".")
        art = "an" if ten[0] in "aeiou" else "a"
        den = f"{aen} on {art} {ten}, {fb} % feedback" + (f": {fen}." if fen else ".")
        if p["tempo"].endswith("ms") or p["tempo"].startswith("L "):
            den = f"{aen}, {ten}, {fb} % feedback" + (f": {fen}." if fen else ".")
        usePt, useEn = p["app"], USE[p["app"]]
        tags_pt = [cat.lower(), p["algo"].lower()]; tags_en = list(tags_pt)
        if sub: tags_pt.append(sub.lower()); tags_en.append(sub.lower())
        for key, tp, te in (("throw_auto", "throw", "throw"), ("duck", "ducking", "ducking"), ("unmask", "unmask", "unmask"),
                            ("reverse", "reverse", "reverse"), ("guard", "mono", "mono")):
            if kv.get(key, "0") not in ("0", ""): tags_pt.append(tp); tags_en.append(te)
        if "groove" in kv: tags_pt.append("groove " + kv["groove"].lower()); tags_en.append("groove " + kv["groove"].lower())
        assert name not in names, name; names.add(name)
        out.append(f"        {{ {c(name)}, {c(cat)}, {c(dpt)}, {c(usePt)}, {c(', '.join(tags_pt))}, {c(data)}, {c(sub)}, {c(den)}, {c(useEn)}, {c(', '.join(tags_en))} }},")
    hdr = f"""// =============================================================================
//  DELEX by Piradex — {len(out)} presets do guia "Delay FX · 200 presets profissionais"
//  (Beatfreak Music / Piradex, Novembro de 2026)
//  GERADO por tools/presets_guia.py — não editar à mão.
// =============================================================================
"""
    path = os.path.join(ROOT, "Source", "PresetsGuia.inc")
    open(path, "w", encoding="utf-8").write(hdr + "\n".join(out) + "\n")
    print(f"{len(out)} presets escritos em Source/PresetsGuia.inc")

if __name__ == "__main__":
    main()
