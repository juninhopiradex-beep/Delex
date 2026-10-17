// =============================================================================
//  DELEX by Piradex — presets de fábrica (61) e estilos rápidos
//  Cada preset parte dos valores por defeito e aplica só o que muda.
//  Formato: "id=valor;id=valor" (valores nas unidades mostradas; escolhas por nome).
// =============================================================================
#pragma once

#include "Params.h"
#include "i18n/PiradexLanguage.h"
#include <cstring>

namespace delex
{
struct FactoryPreset
{
    const char* name;
    const char* category;
    const char* description;
    const char* use;
    const char* tags;
    const char* data;
};

inline const juce::StringArray& presetCategories()
{
    static const juce::StringArray c { "Vocals", "Afro House", "Kizomba", "Trap", "Drums", "Guitar", "Synth",
                                       "Ambient", "Lo-Fi", "Dub", "Transition", "Master FX" };
    return c;
}

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> p {
        // ---------------------------------------------------------------- Vocals
        { "Slap Curto", "Vocals", "Slapback curto e quente que dá corpo à voz sem a afastar.",
          "Voz principal (rap, R&B, rock) em insert.", "voz, slapback, curto",
          "sync=0;time=95;feedback=8;space=10;tone=-15;mix=18;high_cut=8000;low_cut=150;warmth=30;drive=10" },
        { "Voz à Frente 1/8", "Vocals", "Colcheia filtrada com ducking: os ecos só aparecem nas pausas.",
          "Voz principal, envio auxiliar a 100 % wet.", "voz, ducking, colcheia",
          "div=1/8;feedback=25;space=35;tone=-20;mix=20;duck=1;duck_amount=10;low_cut=200;high_cut=7000" },
        { "Voz à Frente 1/4", "Vocals", "Semínima escura com ducking forte e um toque de modulação.",
          "Baladas e refrões lentos.", "voz, ducking, semínima",
          "div=1/4;feedback=32;space=40;tone=-25;mix=22;duck=1;duck_amount=12;low_cut=220;high_cut=6500;mod_depth=3" },
        { "Eco de Rádio", "Vocals", "Eco de banda estreita com pico médio e saturação.",
          "Pré-refrões, vozes de efeito, intros.", "voz, telefone, rádio, filtrado",
          "div=1/8D;feedback=35;low_cut=600;high_cut=3500;cut_slope=24 dB/oct;peak_freq=1500;peak_gain=4;drive=25;mix=20;space=15" },
        { "Dupla Larga", "Vocals", "Duplicação estéreo curta e modulada (efeito Haas).",
          "Backing vocals e dobras.", "voz, dobra, largura, haas",
          "sync=0;time=22;r_ratio=1.45;feedback=0;space=80;mix=30;mod_depth=6;mod_rate=0.6;high_cut=12000" },
        { "Throw de Refrão", "Vocals", "Ping-pong longo para automatizar em palavras-chave.",
          "Automatiza o envio na última palavra da frase.", "voz, throw, ping-pong",
          "div=1/4D;feedback=55;space=60;tone=-10;mix=35;routing=Ping-Pong;duck=1;duck_amount=15;diff_amount=20" },
        { "Sussurro Difuso", "Vocals", "Ecos que se dissolvem numa cauda macia.",
          "Vozes sussurradas, pontes.", "voz, difuso, suave",
          "div=1/8;feedback=45;space=70;tone=-30;diff_amount=60;diff_spread=18;mix=25;mod_depth=8;mod_rate=0.3" },

        // ---------------------------------------------------------------- Afro House
        { "Sincopa Dourada", "Afro House", "Ping-pong 1/4 pontuado com ducking: abre a voz sem a afogar.",
          "Voz ou lead em Afro House, envio a 100 % wet.", "voz, ping-pong, ducking, sincopado",
          "div=1/4D;routing=Ping-Pong;feedback=48;space=62;tone=-12;character=30;mix=22;duck=1;duck_amount=9" },
        { "Ping-Pong Pontuado", "Afro House", "Colcheia pontuada a saltar entre lados, filtrada.",
          "Percussões, vocal chops, plucks.", "ping-pong, pontuado, groove",
          "div=1/8D;routing=Ping-Pong;feedback=45;tone=-20;low_cut=250;high_cut=7000;mix=25;space=35" },
        { "Tambor Distante", "Afro House", "Percussão com difusão e cauda escura, longe na mistura.",
          "Congas, djembé, toms.", "percussão, difusão, distante",
          "div=1/4;feedback=40;space=55;tone=-40;diff_amount=45;mix=28;low_cut=120" },
        { "Noite em Luanda", "Afro House", "Cauda atmosférica com rotação lenta.",
          "Pads, vozes e breakdowns.", "atmosfera, rotação, cauda",
          "div=1/2;feedback=58;space=75;rotation=25;diff_amount=50;diff_spread=20;mod_depth=6;mod_rate=0.15;tone=-25;mix=30;warmth=40" },
        { "Groove Espiral", "Afro House", "Tercinas com rotação de 60°: o eco gira no espaço.",
          "Hats abertos, marimbas, kalimbas.", "tercina, rotação, movimento",
          "div=1/8T;feedback=50;rotation=60;space=40;mix=24;tone=-10" },
        { "Shaker Fantasma", "Afro House", "1/16 curto e muito largo para shakers.",
          "Shakers, hi-hats, tops.", "shaker, largo, curto",
          "div=1/16;feedback=30;space=90;out_width=150;low_cut=800;mix=20;r_ratio=1.5" },

        // ---------------------------------------------------------------- Kizomba
        { "Kizomba Suave", "Kizomba", "Semínima quente com ducking leve e modulação lenta.",
          "Voz principal em Kizomba e Zouk.", "voz, quente, ducking",
          "div=1/4;feedback=35;space=45;tone=-30;warmth=50;drive=12;mix=20;duck=1;duck_amount=8;mod_depth=4" },
        { "Abraço Lento", "Kizomba", "Ecos longos, escuros e envolventes.",
          "Refrões e finais de frase.", "longo, escuro, envolvente",
          "div=1/2D;feedback=50;space=55;tone=-35;diff_amount=35;warmth=45;mix=22;mod_depth=5;mod_rate=0.2" },
        { "Guitarra Kizomba", "Kizomba", "Ping-pong com sabor a fita e chorus ligeiro.",
          "Guitarras limpas e teclados.", "guitarra, ping-pong, fita",
          "div=1/8D;feedback=42;routing=Ping-Pong;tone=-15;character=25;mix=26;mod_depth=7;mod_rate=0.9" },
        { "Semba Eco", "Kizomba", "Colcheia saturada e directa.",
          "Guitarras e vozes de Semba.", "colcheia, saturado, directo",
          "div=1/8;feedback=38;space=30;tone=-5;character=40;mix=22;low_cut=180" },
        { "Cetim Estéreo", "Kizomba", "Tercinas largas e sedosas com rotação.",
          "Pads, coros, cordas.", "tercina, largo, sedoso",
          "div=1/4T;feedback=44;space=70;rotation=35;diff_amount=25;tone=-20;mix=24" },

        // ---------------------------------------------------------------- Trap
        { "Throw 1/8D", "Trap", "O clássico throw de colcheia pontuada em ping-pong.",
          "Ad-libs e finais de verso.", "throw, ad-lib, ping-pong",
          "div=1/8D;feedback=45;routing=Ping-Pong;tone=-15;mix=30;duck=1;duck_amount=14;low_cut=300" },
        { "Hi-Hat Ricochete", "Trap", "Semicolcheias brilhantes a ricochetear.",
          "Hi-hats e rolls.", "hi-hat, brilhante, ping-pong",
          "div=1/16;feedback=35;routing=Ping-Pong;low_cut=1500;high_cut=16000;space=50;mix=18" },
        { "808 Sombra", "Trap", "Eco grave, mono e saturado para o 808.",
          "808 e baixos (pouco mix).", "808, mono, grave",
          "div=1/4;feedback=20;low_cut=40;high_cut=1200;mono_safe=1;mix=15;space=0;drive=30;out_width=0" },
        { "Ad-lib Lo-Fi", "Trap", "Ecos digitais degradados para ad-libs.",
          "Ad-libs, vozes de fundo.", "lo-fi, ad-lib, digital",
          "div=1/8;feedback=40;character=-60;tone=-20;space=50;mix=30;low_cut=500" },
        { "Trap Escuro", "Trap", "Cauda escura e larga com ducking.",
          "Melodias e vozes em trap melódico.", "escuro, largo, ducking",
          "div=1/4D;feedback=55;tone=-55;space=60;diff_amount=30;mix=26;duck=1" },
        { "Voz Rasgada", "Trap", "Eco curto muito saturado.",
          "Vozes agressivas e drill.", "saturado, agressivo, curto",
          "div=1/16D;feedback=25;drive=60;warmth=60;low_cut=400;high_cut=5000;mix=28" },

        // ---------------------------------------------------------------- Drums
        { "Dub de Caixa", "Drums", "Caixa a repetir com fita e cauda escura.",
          "Caixa, rimshot.", "caixa, dub, fita",
          "div=1/4D;feedback=65;tone=-25;drive=35;warmth=50;low_cut=250;space=40;mix=30;wow=20" },
        { "Fill Rítmico", "Drums", "Semicolcheias que enchem buracos do groove.",
          "Loops de bateria, fills.", "rítmico, fill, ping-pong",
          "div=1/16;feedback=60;routing=Ping-Pong;space=30;mix=35" },
        { "Sala Dupla", "Drums", "Reflexões curtas e densas, quase uma sala.",
          "Bateria acústica, overheads.", "sala, curto, difuso",
          "sync=0;time=38;r_ratio=1.31;feedback=30;diff_amount=70;diff_spread=14;space=60;mix=22;tone=-15" },
        { "Tom Cascata", "Drums", "Tercinas com rotação negativa.",
          "Toms e fills.", "tercina, toms, rotação",
          "div=1/8T;feedback=55;rotation=-45;space=45;tone=-20;mix=26" },
        { "Ghost Snare", "Drums", "Eco fino e difuso por trás da caixa.",
          "Ghost notes, clap.", "caixa, difuso, subtil",
          "div=1/8;feedback=30;low_cut=500;high_cut=6000;space=80;diff_amount=40;mix=20" },

        // ---------------------------------------------------------------- Guitar
        { "Fita Vintage", "Guitar", "Eco de fita com wow e agudos gastos.",
          "Guitarra limpa, rhodes.", "fita, vintage, wow",
          "div=1/4;feedback=45;character=55;tone=-30;wow=30;mix=28;warmth=55" },
        { "Chorus-Delay", "Guitar", "Delay curto e modulado que soa a chorus.",
          "Guitarras limpas, arpejos.", "chorus, modulação, curto",
          "sync=0;time=28;feedback=15;mod_depth=18;mod_rate=0.9;mod_offset=50;space=60;mix=40;r_ratio=1.12" },
        { "Surf Longo", "Guitar", "Colcheia pontuada com fita, à moda surf.",
          "Guitarras de surf, leads.", "surf, fita, pontuado",
          "div=1/8D;feedback=60;character=35;tone=-10;mix=30;space=30" },
        { "Clean Wide", "Guitar", "Estéreo limpo com tempos L/R diferentes.",
          "Guitarras rítmicas.", "limpo, largo, estéreo",
          "div=1/4;r_ratio=0.75;feedback=35;space=70;mix=25;high_cut=11000" },
        { "Feedback Cantante", "Guitar", "Feedback alto com pico ressonante que canta.",
          "Solos e notas longas.", "ressonante, feedback, solo",
          "div=1/4D;feedback=82;peak_freq=1200;peak_gain=9;peak_q=3;compensate=30;drive=30;tone=-10;mix=30" },

        // ---------------------------------------------------------------- Synth
        { "Arp Espacial", "Synth", "Ping-pong pontuado para arpejos.",
          "Arpejos e sequências.", "arpejo, ping-pong, espacial",
          "div=1/8D;routing=Ping-Pong;feedback=55;space=65;diff_amount=25;mix=30;mod_depth=4" },
        { "Pad Rotativo", "Synth", "Rotação de 72° e difusão: o pad gira à volta.",
          "Pads e texturas.", "pad, rotação, difusão",
          "div=1/2;feedback=65;rotation=72;space=80;diff_amount=60;diff_spread=25;mix=40;mod_depth=10;mod_rate=0.1" },
        { "Lead Dobrado", "Synth", "Dobra estéreo de 60 ms para leads.",
          "Leads e vozes sintéticas.", "lead, dobra, largo",
          "sync=0;time=60;r_ratio=1.25;feedback=20;space=50;mix=25;mod_depth=5" },
        { "Pluck Cristal", "Synth", "Ecos brilhantes com dispersão que faz 'chirp'.",
          "Plucks e sinos.", "pluck, brilhante, dispersão",
          "div=1/16D;feedback=50;low_cut=400;high_cut=18000;disp_spread=12;disp_freq=2500;space=55;mix=28" },
        { "Bass Gentil", "Synth", "Eco discreto e mono nos graves.",
          "Baixos sintetizados.", "baixo, mono, discreto",
          "div=1/8;feedback=25;low_cut=30;high_cut=2500;mono_safe=1;space=15;mix=15" },

        // ---------------------------------------------------------------- Ambient
        { "Nuvem", "Ambient", "Ecos que viram uma nuvem difusa.",
          "Pianos, pads, vozes etéreas.", "nuvem, difuso, longo",
          "div=1/4D;feedback=75;diff_amount=85;diff_spread=30;space=85;tone=-30;mix=45;mod_depth=8;mod_rate=0.2" },
        { "Catedral de Ecos", "Ambient", "Cauda enorme com rotação de 45°.",
          "Ambientes, intros cinematográficas.", "enorme, rotação, cauda",
          "div=1/2;feedback=80;diff_amount=70;diff_spread=40;rotation=45;space=90;tone=-20;mix=40" },
        { "Maré Lenta", "Ambient", "Modulação aleatória lenta e profunda.",
          "Drones e texturas.", "aleatório, lento, drone",
          "div=1/1;feedback=70;mod_depth=20;mod_rate=0.08;mod_shape=Random;space=75;diff_amount=50;mix=40;tone=-25" },
        { "Neblina", "Ambient", "Dispersão grave e difusão escura.",
          "Pianos e guitarras ambientais.", "dispersão, escuro, neblina",
          "div=1/4T;feedback=65;disp_spread=25;disp_freq=300;diff_amount=60;tone=-45;space=70;mix=35" },
        { "Céu Infinito", "Ambient", "Feedback quase infinito e muito largo.",
          "Notas sustentadas, finais.", "infinito, largo, sustentado",
          "div=1/2D;feedback=88;space=95;diff_amount=90;diff_spread=35;tone=-35;mix=50;mod_depth=6;compensate=80" },

        // ---------------------------------------------------------------- Lo-Fi
        { "Cassete Gasta", "Lo-Fi", "Fita cansada com muito wow e agudos gastos.",
          "Beats lo-fi, pianos.", "cassete, wow, gasto",
          "div=1/4;feedback=50;character=80;wow=70;tone=-45;mix=30;drive=40" },
        { "8-bit Eco", "Lo-Fi", "Eco a 6 bits e 11 kHz.",
          "Chiptune, vozes robóticas.", "8-bit, digital, retro",
          "div=1/8;feedback=45;bits=6;rate=11025;mix=30" },
        { "Rádio AM", "Lo-Fi", "Banda estreita, bits reduzidos e saturação.",
          "Samples e vozes de época.", "rádio, banda estreita, retro",
          "div=1/8D;feedback=40;low_cut=500;high_cut=3000;cut_slope=24 dB/oct;bits=10;rate=16000;drive=40;mix=28" },
        { "Poeira Digital", "Lo-Fi", "Degradação digital com jitter.",
          "Texturas e transições.", "digital, jitter, poeira",
          "div=1/4D;feedback=55;character=-50;jitter=60;diff_amount=30;mix=30;tone=-15" },
        { "Bitcrush Dub", "Lo-Fi", "Dub com bitcrush em mistura paralela.",
          "Percussão e vozes em dub digital.", "bitcrush, dub, paralelo",
          "div=1/4D;feedback=70;bits=5;rate=8000;grit_mix=60;tone=-30;drive=30;mix=32" },

        // ---------------------------------------------------------------- Dub
        { "Dub Clássico", "Dub", "Fita, feedback alto e graves cortados.",
          "Caixa, vozes, skanks.", "dub, fita, clássico",
          "div=1/4D;feedback=72;character=50;tone=-35;low_cut=200;wow=25;mix=35" },
        { "Dub Profundo", "Dub", "Mínima longa, escura e saturada.",
          "Vozes e metais em dub.", "dub, escuro, longo",
          "div=1/2;feedback=78;tone=-50;drive=40;warmth=60;space=40;mix=35" },
        { "Sirene Dub", "Dub", "Pico ressonante e modulação: sobe sozinho.",
          "Efeitos de sirene, transições.", "sirene, ressonante, modulação",
          "div=1/8D;feedback=85;peak_freq=2000;peak_gain=10;peak_q=4;compensate=25;mod_depth=25;mod_rate=0.5;mix=35" },
        { "Steppers", "Dub", "Colcheias em ping-pong com fita.",
          "Skanks e percussão.", "steppers, ping-pong, fita",
          "div=1/8;feedback=62;routing=Ping-Pong;character=40;tone=-25;mix=30" },
        { "Sound System", "Dub", "Saturação quente de sistema de som.",
          "Melódica, vozes, metais.", "sound system, quente, saturado",
          "div=1/4;feedback=68;drive=50;warmth=70;low_cut=150;high_cut=6000;space=50;mix=33" },

        // ---------------------------------------------------------------- Transition
        { "Freeze Riser", "Transition", "Prepara uma cauda densa: carrega em FREEZE no fim da frase.",
          "Builds e risers.", "freeze, riser, build",
          "div=1/16;feedback=80;space=80;diff_amount=60;mix=50;tone=20" },
        { "Wash Out", "Transition", "Tudo se lava numa cauda enorme.",
          "Fim de secção, antes do drop.", "wash, cauda, transição",
          "div=1/4D;feedback=90;diff_amount=90;space=95;tone=-40;mix=60;compensate=80" },
        { "Stop Drop", "Transition", "Muda o tempo durante a cauda para um efeito de fita a parar.",
          "Automatiza Time enquanto toca.", "fita, glide, stop",
          "div=1/8;feedback=95;character=-40;tone=-10;mix=55;transition=Tape Glide;glide=1200" },
        { "Túnel", "Transition", "Banda estreita a girar 90° entre lados.",
          "Filtros de transição, breakdowns.", "túnel, rotação, filtrado",
          "div=1/2;feedback=85;low_cut=600;high_cut=2500;rotation=90;space=80;mix=50;disp_spread=30" },

        // ---------------------------------------------------------------- Master FX
        { "Brilho Sutil", "Master FX", "Alarga só os agudos com reflexões muito curtas.",
          "Barramento de mistura (mix baixo).", "subtil, largura, agudos",
          "sync=0;time=12;r_ratio=1.4;feedback=0;space=40;mix=6;low_cut=3000;high_cut=16000;mod_depth=2" },
        { "Largura Fina", "Master FX", "Largura discreta com graves em mono.",
          "Barramentos e stems.", "largura, mono-safe, subtil",
          "sync=0;time=8;r_ratio=1.6;feedback=0;space=60;mix=8;low_cut=800;mono_safe=1" },
        { "Profundidade Mínima", "Master FX", "Uma camada de profundidade quase invisível.",
          "Barramento de vozes ou instrumentos.", "profundidade, subtil, difuso",
          "div=1/8;feedback=20;diff_amount=60;tone=-40;space=50;mix=5;mono_safe=1" },
    };
    return p;
}

// ---------------------------------------------------------------- estilos rápidos
// ---------------------------------------------------------------------------
// Textos dos presets/estilos em inglês (o nome do preset é o mesmo nos dois idiomas)
struct PresetTextEn { const char* name; const char* description; const char* use; const char* tags; };
inline const PresetTextEn* presetEnglish (const char* name)
{
    static const PresetTextEn t[] {
        { "Slap Curto", "Short, warm slapback that adds body to the vocal without pushing it back.", "Lead vocal (rap, R&B, rock) on an insert.", "vocal, slapback, short" },
        { "Voz à Frente 1/8", "Filtered eighth note with ducking: the echoes only appear in the gaps.", "Lead vocal, aux send at 100 % wet.", "vocal, ducking, eighth" },
        { "Voz à Frente 1/4", "Dark quarter note with strong ducking and a touch of modulation.", "Ballads and slow choruses.", "vocal, ducking, quarter" },
        { "Eco de Rádio", "Narrow-band echo with a mid peak and saturation.", "Pre-choruses, effect vocals, intros.", "vocal, telephone, radio, filtered" },
        { "Dupla Larga", "Short, modulated stereo doubling (Haas effect).", "Backing vocals and doubles.", "vocal, double, width, haas" },
        { "Throw de Refrão", "Long ping-pong to automate on key words.", "Automate the send on the last word of the phrase.", "vocal, throw, ping-pong" },
        { "Sussurro Difuso", "Echoes that dissolve into a soft tail.", "Whispered vocals, bridges.", "vocal, diffuse, soft" },
        { "Sincopa Dourada", "Dotted 1/4 ping-pong with ducking: opens the vocal without drowning it.", "Vocal or lead in Afro House, send at 100 % wet.", "vocal, ping-pong, ducking, syncopated" },
        { "Ping-Pong Pontuado", "Filtered dotted eighth bouncing between sides.", "Percussion, vocal chops, plucks.", "ping-pong, dotted, groove" },
        { "Tambor Distante", "Percussion with diffusion and a dark tail, far back in the mix.", "Congas, djembe, toms.", "percussion, diffusion, distant" },
        { "Noite em Luanda", "Atmospheric tail with slow rotation.", "Pads, vocals and breakdowns.", "atmosphere, rotation, tail" },
        { "Groove Espiral", "Triplets with 60° rotation: the echo spins in space.", "Open hats, marimbas, kalimbas.", "triplet, rotation, movement" },
        { "Shaker Fantasma", "Short, very wide 1/16 for shakers.", "Shakers, hi-hats, tops.", "shaker, wide, short" },
        { "Kizomba Suave", "Warm quarter note with light ducking and slow modulation.", "Lead vocal in Kizomba and Zouk.", "vocal, warm, ducking" },
        { "Abraço Lento", "Long, dark, enveloping echoes.", "Choruses and phrase endings.", "long, dark, enveloping" },
        { "Guitarra Kizomba", "Ping-pong with a tape flavour and light chorus.", "Clean guitars and keys.", "guitar, ping-pong, tape" },
        { "Semba Eco", "Saturated, direct eighth note.", "Semba guitars and vocals.", "eighth, saturated, direct" },
        { "Cetim Estéreo", "Wide, silky triplets with rotation.", "Pads, choirs, strings.", "triplet, wide, silky" },
        { "Throw 1/8D", "The classic dotted-eighth ping-pong throw.", "Ad-libs and verse endings.", "throw, ad-lib, ping-pong" },
        { "Hi-Hat Ricochete", "Bright sixteenths ricocheting around.", "Hi-hats and rolls.", "hi-hat, bright, ping-pong" },
        { "808 Sombra", "Low, mono, saturated echo for the 808.", "808 and basses (low mix).", "808, mono, low" },
        { "Ad-lib Lo-Fi", "Degraded digital echoes for ad-libs.", "Ad-libs, background vocals.", "lo-fi, ad-lib, digital" },
        { "Trap Escuro", "Dark, wide tail with ducking.", "Melodies and vocals in melodic trap.", "dark, wide, ducking" },
        { "Voz Rasgada", "Short, heavily saturated echo.", "Aggressive vocals and drill.", "saturated, aggressive, short" },
        { "Dub de Caixa", "Repeating snare with tape and a dark tail.", "Snare, rimshot.", "snare, dub, tape" },
        { "Fill Rítmico", "Sixteenths that fill the gaps in the groove.", "Drum loops, fills.", "rhythmic, fill, ping-pong" },
        { "Sala Dupla", "Short, dense reflections, almost a room.", "Acoustic drums, overheads.", "room, short, diffuse" },
        { "Tom Cascata", "Triplets with negative rotation.", "Toms and fills.", "triplet, toms, rotation" },
        { "Ghost Snare", "Thin, diffuse echo behind the snare.", "Ghost notes, clap.", "snare, diffuse, subtle" },
        { "Fita Vintage", "Tape echo with wow and worn highs.", "Clean guitar, rhodes.", "tape, vintage, wow" },
        { "Chorus-Delay", "Short modulated delay that sounds like a chorus.", "Clean guitars, arpeggios.", "chorus, modulation, short" },
        { "Surf Longo", "Dotted eighth with tape, surf style.", "Surf guitars, leads.", "surf, tape, dotted" },
        { "Clean Wide", "Clean stereo with different L/R times.", "Rhythm guitars.", "clean, wide, stereo" },
        { "Feedback Cantante", "High feedback with a resonant peak that sings.", "Solos and long notes.", "resonant, feedback, solo" },
        { "Arp Espacial", "Dotted ping-pong for arpeggios.", "Arpeggios and sequences.", "arpeggio, ping-pong, spatial" },
        { "Pad Rotativo", "72° rotation and diffusion: the pad spins around you.", "Pads and textures.", "pad, rotation, diffusion" },
        { "Lead Dobrado", "60 ms stereo double for leads.", "Leads and synth vocals.", "lead, double, wide" },
        { "Pluck Cristal", "Bright echoes with dispersion that 'chirps'.", "Plucks and bells.", "pluck, bright, dispersion" },
        { "Bass Gentil", "Discreet mono echo on the lows.", "Synth basses.", "bass, mono, discreet" },
        { "Nuvem", "Echoes that become a diffuse cloud.", "Pianos, pads, ethereal vocals.", "cloud, diffuse, long" },
        { "Catedral de Ecos", "Huge tail with 45° rotation.", "Ambiences, cinematic intros.", "huge, rotation, tail" },
        { "Maré Lenta", "Slow, deep random modulation.", "Drones and textures.", "random, slow, drone" },
        { "Neblina", "Low dispersion and dark diffusion.", "Ambient pianos and guitars.", "dispersion, dark, mist" },
        { "Céu Infinito", "Almost infinite, very wide feedback.", "Sustained notes, endings.", "infinite, wide, sustained" },
        { "Cassete Gasta", "Tired tape with lots of wow and worn highs.", "Lo-fi beats, pianos.", "cassette, wow, worn" },
        { "8-bit Eco", "6-bit, 11 kHz echo.", "Chiptune, robotic vocals.", "8-bit, digital, retro" },
        { "Rádio AM", "Narrow band, reduced bits and saturation.", "Period samples and vocals.", "radio, narrow band, retro" },
        { "Poeira Digital", "Digital degradation with jitter.", "Textures and transitions.", "digital, jitter, dust" },
        { "Bitcrush Dub", "Dub with bitcrush in parallel mix.", "Percussion and vocals in digital dub.", "bitcrush, dub, parallel" },
        { "Dub Clássico", "Tape, high feedback and cut lows.", "Snare, vocals, skanks.", "dub, tape, classic" },
        { "Dub Profundo", "Long, dark, saturated half note.", "Vocals and horns in dub.", "dub, dark, long" },
        { "Sirene Dub", "Resonant peak and modulation: it rises on its own.", "Siren effects, transitions.", "siren, resonant, modulation" },
        { "Steppers", "Ping-pong eighths with tape.", "Skanks and percussion.", "steppers, ping-pong, tape" },
        { "Sound System", "Warm sound-system saturation.", "Melodica, vocals, horns.", "sound system, warm, saturated" },
        { "Freeze Riser", "Builds a dense tail: press FREEZE at the end of the phrase.", "Builds and risers.", "freeze, riser, build" },
        { "Wash Out", "Everything washes into a huge tail.", "End of a section, before the drop.", "wash, tail, transition" },
        { "Stop Drop", "Changes the time during the tail for a tape-stop effect.", "Automate Time while it plays.", "tape, glide, stop" },
        { "Túnel", "Narrow band spinning 90° between sides.", "Transition filters, breakdowns.", "tunnel, rotation, filtered" },
        { "Brilho Sutil", "Widens only the highs with very short reflections.", "Mix bus (low mix).", "subtle, width, highs" },
        { "Largura Fina", "Discreet width with mono lows.", "Buses and stems.", "width, mono-safe, subtle" },
        { "Profundidade Mínima", "An almost invisible layer of depth.", "Vocal or instrument bus.", "depth, subtle, diffuse" } };
    for (auto& e : t) if (std::strcmp (e.name, name) == 0) return &e;
    return nullptr;
}
inline juce::String presetDescription (const FactoryPreset& p)
{
    if (! piradex::i18n::isPortuguese()) if (auto* e = presetEnglish (p.name)) return juce::String::fromUTF8 (e->description);
    return juce::String::fromUTF8 (p.description);
}
inline juce::String presetUse (const FactoryPreset& p)
{
    if (! piradex::i18n::isPortuguese()) if (auto* e = presetEnglish (p.name)) return juce::String::fromUTF8 (e->use);
    return juce::String::fromUTF8 (p.use);
}
inline juce::String presetTags (const FactoryPreset& p)
{
    if (! piradex::i18n::isPortuguese()) if (auto* e = presetEnglish (p.name)) return juce::String::fromUTF8 (e->tags);
    return juce::String::fromUTF8 (p.tags);
}

struct Style { const char* name; const char* tip; const char* data; const char* tipEn = ""; };
inline juce::String styleTip (const struct Style& st);

inline const std::vector<Style>& styles()
{
    static const std::vector<Style> s {
        { "CLEAN",     "Eco limpo e directo", "feedback=35;space=10;tone=0;character=0;warmth=0;routing=Dual;rotation=0", "Clean, direct echo" },
        { "PING-PONG", "Ecos alternados entre lados", "routing=Ping-Pong;feedback=45;space=30;tone=-10", "Echoes alternating between sides" },
        { "WIDE",      "Tempos L/R diferentes e rotação", "routing=Dual;r_ratio=1.33;space=75;rotation=20;feedback=40", "Different L/R times and rotation" },
        { "DUB",       "Feedback alto, fita e escuro", "feedback=70;character=45;tone=-35;low_cut=200;wow=20", "High feedback, tape and dark" },
        { "DREAM",     "Difuso, largo e modulado", "feedback=65;diff_amount=70;diff_spread=25;space=80;tone=-25;mod_depth=8;mod_rate=0.2", "Diffuse, wide and modulated" },
        { "TAPE",      "Saturação e wow de fita", "character=60;warmth=50;wow=35;tone=-20;feedback=45", "Tape saturation and wow" },
        { "DIGITAL",   "Bits e sample rate reduzidos", "character=-55;bits=10;rate=16000;feedback=45", "Reduced bits and sample rate" },
        { "RHYTHMIC",  "Ping-pong com relação 3:2", "feedback=50;routing=Ping-Pong;r_ratio=1.5;tone=-5", "Ping-pong with a 3:2 ratio" },
        { "FREEZE",    "Cauda densa congelada", "feedback=80;diff_amount=50;space=70;freeze=1", "Dense frozen tail" },
    };
    return s;
}

inline juce::String styleTip (const Style& st)
{
    return juce::String::fromUTF8 (piradex::i18n::isPortuguese() || *st.tipEn == 0 ? st.tip : st.tipEn);
}

// Parâmetros que um estilo NÃO altera (tempo, mistura, ducking, ganhos)
inline bool styleKeeps (const juce::String& pid)
{
    static const juce::StringArray keep { id::sync, id::div, id::time, id::mix, id::duck, id::duck_thresh, id::duck_amount,
                                          id::duck_attack, id::duck_hold, id::duck_release, id::mix_law, id::wet_trim,
                                          id::out_trim, id::bypass, id::transition, id::glide };
    return keep.contains (pid);
}

// ---------------------------------------------------------------- aplicar
//  Define um parâmetro a partir de texto ("1/4D", "Ping-Pong", "48").
inline void setParamFromText (juce::RangedAudioParameter& prm, const juce::String& text)
{
    float norm;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (&prm))
    {
        int idx = c->choices.indexOf (text, true);
        if (idx < 0) idx = text.getIntValue();
        norm = c->convertTo0to1 ((float) juce::jlimit (0, c->choices.size() - 1, idx));
    }
    else if (dynamic_cast<juce::AudioParameterBool*> (&prm) != nullptr)
        norm = text.getFloatValue() > 0.5f ? 1.0f : 0.0f;
    else
        norm = prm.convertTo0to1 (text.getFloatValue());
    prm.beginChangeGesture();
    prm.setValueNotifyingHost (norm);
    prm.endChangeGesture();
}

inline juce::StringPairArray parsePresetData (const char* data)
{
    juce::StringPairArray kv;
    for (auto& tok : juce::StringArray::fromTokens (juce::String::fromUTF8 (data), ";", ""))
        if (tok.containsChar ('='))
            kv.set (tok.upToFirstOccurrenceOf ("=", false, false).trim(), tok.fromFirstOccurrenceOf ("=", false, false).trim());
    return kv;
}

// Aplica dados: todos os parâmetros vão ao valor por defeito (excepto os mantidos), depois os definidos.
inline void applyData (juce::AudioProcessorValueTreeState& s, const char* data, bool isStyle)
{
    const auto kv = parsePresetData (data);
    for (auto* prm : s.processor.getParameters())
    {
        auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm);
        if (r == nullptr) continue;
        const auto pid = r->getParameterID();
        if (pid == id::bypass) continue;
        if (isStyle && styleKeeps (pid)) continue;
        if (kv.containsKey (pid)) setParamFromText (*r, kv[pid]);
        else
        {
            if (isStyle && pid == id::freeze) { r->beginChangeGesture(); r->setValueNotifyingHost (0.0f); r->endChangeGesture(); continue; }
            r->beginChangeGesture(); r->setValueNotifyingHost (r->getDefaultValue()); r->endChangeGesture();
        }
    }
}

} // namespace delex
