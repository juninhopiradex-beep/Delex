#!/usr/bin/env python3
"""
DELEX by Piradex — gera Source/PresetsExtra.inc (221 presets de fábrica, PT + EN).

Uso:  python3 tools/presets_extra.py
Cada preset parte dos valores por defeito; "data" usa o mesmo formato dos
presets em Presets.h ("id=valor;..."). O script valida IDs, intervalos e
nomes únicos antes de escrever o ficheiro.
"""
import os, re, sys

# (nome, subcategoria, descrição PT, descrição EN, dados)
P = {}

P["Ambience"] = [
    ("Quarto Pequeno", "", "Reflexões curtas e densas de um quarto pequeno.", "Short, dense reflections of a small room.",
     "sync=0;time=18;r_ratio=1.27;feedback=35;diff_amount=75;diff_spread=8;space=40;tone=-20;mix=22"),
    ("Estúdio Seco", "", "Ambiente de estúdio tratado: dá ar sem se ouvir.", "Treated studio ambience: adds air without being heard.",
     "sync=0;time=24;feedback=25;diff_amount=60;diff_spread=6;tone=-10;mix=18"),
    ("Sala de Ensaio", "", "Sala média e viva, como uma sala de ensaios.", "Medium, lively room, like a rehearsal space.",
     "sync=0;time=35;feedback=40;diff_amount=70;diff_spread=12;mix=22"),
    ("Garagem", "", "Paredes duras e graves cortados: som de garagem.", "Hard walls and trimmed lows: a garage sound.",
     "sync=0;time=42;feedback=50;diff_amount=55;diff_spread=10;low_cut=120;tone=-5;mix=25"),
    ("Escadaria", "", "Reflexões em espiral de uma escadaria de pedra.", "Spiralling reflections of a stone stairwell.",
     "sync=0;time=60;feedback=55;diff_amount=80;diff_spread=20;rotation=20;mix=24"),
    ("Corredor de Betão", "", "Corredor longo e brilhante com ecos laterais.", "Long, bright corridor with side echoes.",
     "sync=0;time=75;r_ratio=1.4;feedback=58;diff_amount=65;diff_spread=18;tone=10;mix=22"),
    ("Câmara de Madeira", "", "Sala de madeira quente e escura.", "Warm, dark wooden room.",
     "sync=0;time=28;feedback=42;diff_amount=85;diff_spread=9;tone=-30;warmth=30;mix=20"),
    ("Cabine Vocal", "", "Cabine pequena: só um toque de espaço à volta da voz.", "Small booth: just a touch of space around the vocal.",
     "sync=0;time=12;feedback=20;diff_amount=70;diff_spread=4;low_cut=200;mix=15"),
    ("Teatro Vazio", "", "Teatro grande e vazio, cauda suave.", "Large empty theatre with a soft tail.",
     "sync=0;time=95;feedback=62;diff_amount=85;diff_spread=28;space=70;tone=-25;mix=26"),
    ("Igreja de Pedra", "", "Nave de pedra com cauda longa e escura.", "Stone nave with a long, dark tail.",
     "sync=0;time=120;feedback=70;diff_amount=90;diff_spread=35;space=80;tone=-30;mix=28"),
    ("Loft Industrial", "", "Espaço aberto de tijolo e metal.", "Open brick-and-metal space.",
     "sync=0;time=55;feedback=52;diff_amount=60;rotation=-25;tone=5;mix=22"),
    ("Ambiente de Rua", "", "Ecos filtrados de rua entre prédios.", "Filtered street echoes between buildings.",
     "sync=0;time=85;feedback=30;diff_amount=50;diff_spread=22;low_cut=300;high_cut=7000;mix=18"),
    ("Parque Subterrâneo", "", "Parque de estacionamento: betão e cauda metálica.", "Underground car park: concrete and a metallic tail.",
     "sync=0;time=110;feedback=66;diff_amount=70;low_cut=150;tone=-10;mix=25"),
    ("Caixa de Vidro", "", "Sala pequena e brilhante com reflexões rápidas.", "Small, bright room with fast reflections.",
     "sync=0;time=20;feedback=48;diff_amount=75;diff_spread=7;low_cut=400;tone=30;mix=18"),
    ("Ar Invisível", "", "Largura e ar quase impercetíveis, graves em mono.", "Almost imperceptible width and air, mono lows.",
     "sync=0;time=30;feedback=22;diff_amount=80;diff_spread=12;space=90;mono_safe=1;mix=10"),
    ("Cúpula", "", "Cúpula alta: a cauda roda à volta de quem ouve.", "High dome: the tail circles around the listener.",
     "sync=0;time=130;feedback=72;diff_amount=85;diff_spread=40;rotation=45;mix=26"),
    ("Sala Viva", "", "Sala média com movimento subtil.", "Medium room with subtle movement.",
     "sync=0;time=45;feedback=45;diff_amount=70;mod_depth=4;mod_rate=0.4;mix=22"),
    ("Sala Escura", "", "Sala abafada, agudos muito suaves.", "Muffled room with very soft highs.",
     "sync=0;time=50;feedback=50;diff_amount=75;tone=-55;mix=22"),
    ("Ambiente Largo", "", "Ambiente curto mas muito largo.", "Short but very wide ambience.",
     "sync=0;time=33;r_ratio=1.6;feedback=38;diff_amount=70;out_width=170;mix=20"),
    ("Bolha", "", "Ambiente denso com dispersão: soa 'dentro de uma bolha'.", "Dense ambience with dispersion: sounds 'inside a bubble'.",
     "sync=0;time=25;feedback=55;diff_amount=90;diff_spread=15;disp_spread=6;disp_freq=900;mix=20"),
]

P["Bass"] = [
    # Ambience
    ("Sub Ambiente", "Ambience", "Ambiente discreto que não suja o sub.", "Discreet ambience that keeps the sub clean.",
     "sync=0;time=40;feedback=30;diff_amount=70;low_cut=120;high_cut=5000;mono_safe=1;mix=15"),
    ("Baixo no Espaço", "Ambience", "Dá profundidade ao baixo sem perder o ataque.", "Adds depth to the bass without losing attack.",
     "sync=0;time=70;feedback=40;diff_amount=75;low_cut=150;space=50;mono_safe=1;mix=15"),
    ("Fretless Aéreo", "Ambience", "Colcheia suave e modulada para baixo fretless.", "Soft, modulated eighth note for fretless bass.",
     "div=1/8;feedback=35;diff_amount=55;low_cut=200;mod_depth=5;mono_safe=1;mix=18"),
    ("Contrabaixo de Jazz", "Ambience", "Ambiente de clube para contrabaixo acústico.", "Club ambience for upright bass.",
     "sync=0;time=55;feedback=30;diff_amount=80;tone=-20;low_cut=100;mono_safe=1;mix=15"),
    ("Moog Profundo", "Ambience", "Eco escuro de semínima para baixos sintetizados.", "Dark quarter-note echo for synth basses.",
     "div=1/4;feedback=40;diff_amount=45;high_cut=3000;mono_safe=1;mix=14"),
    ("Baixo Distante", "Ambience", "Cauda longa e escura por trás do baixo.", "Long, dark tail behind the bass.",
     "sync=0;time=90;feedback=45;diff_amount=85;low_cut=180;tone=-30;mono_safe=1;mix=16"),
    ("Groove Fantasma", "Ambience", "Semicolcheias leves que reforçam o groove.", "Light sixteenths that reinforce the groove.",
     "div=1/16;feedback=25;diff_amount=40;low_cut=250;mono_safe=1;mix=12"),
    ("Pulso Escuro", "Ambience", "Colcheia pontuada escura para baixos de house.", "Dark dotted eighth for house basses.",
     "div=1/8D;feedback=35;diff_amount=60;high_cut=2500;mono_safe=1;mix=14"),
    # Chorus
    ("Chorus Clássico de Baixo", "Chorus", "Chorus de pedal clássico, graves intocados.", "Classic pedal chorus with untouched lows.",
     "sync=0;time=12;feedback=0;mod_depth=15;mod_rate=0.8;mod_offset=50;low_cut=200;mono_safe=1;mix=40"),
    ("Chorus Anos 80", "Chorus", "Chorus brilhante e largo, ao estilo dos anos 80.", "Bright, wide chorus in an 80s style.",
     "sync=0;time=9;feedback=10;mod_depth=22;mod_rate=1.2;low_cut=250;tone=10;mono_safe=1;mix=45"),
    ("Chorus Lento", "Chorus", "Ondulação lenta e profunda.", "Slow, deep undulation.",
     "sync=0;time=16;feedback=5;mod_depth=18;mod_rate=0.25;low_cut=180;mono_safe=1;mix=40"),
    ("Ensemble Grave", "Chorus", "Várias vozes de chorus com tempos L/R diferentes.", "Several chorus voices with different L/R times.",
     "sync=0;time=20;r_ratio=1.3;feedback=0;mod_depth=25;mod_rate=0.5;mod_shape=Triangle;low_cut=220;mono_safe=1;mix=35"),
    ("Flanger Grave", "Chorus", "Flanger lento com feedback para baixo.", "Slow flanger with feedback for bass.",
     "sync=0;time=3;feedback=55;mod_depth=30;mod_rate=0.15;low_cut=120;mono_safe=1;mix=35"),
    ("Chorus Aleatório", "Chorus", "Modulação aleatória: soa mais orgânico.", "Random modulation: sounds more organic.",
     "sync=0;time=14;feedback=0;mod_depth=20;mod_rate=0.6;mod_shape=Random;low_cut=200;mono_safe=1;mix=38"),
    ("Chorus Quente", "Chorus", "Chorus com saturação de fita.", "Chorus with tape saturation.",
     "sync=0;time=11;feedback=0;mod_depth=14;mod_rate=0.7;warmth=50;drive=15;low_cut=200;mono_safe=1;mix=40"),
    ("Vibrato Total", "Chorus", "Vibrato a 100 %: todo o baixo ondula na afinação.", "100 % vibrato: the whole bass wobbles in pitch.",
     "sync=0;time=6;feedback=0;mod_depth=35;mod_rate=5;mix=100;mix_law=Linear"),
    # Room
    ("Sala Pequena para Baixo", "Room", "Sala pequena que cola o baixo à bateria.", "Small room that glues the bass to the drums.",
     "sync=0;time=22;feedback=30;diff_amount=70;low_cut=150;mono_safe=1;mix=15"),
    ("Amp na Sala", "Room", "Amplificador de baixo a soar numa sala.", "Bass amp sounding in a room.",
     "sync=0;time=30;feedback=35;diff_amount=60;drive=15;low_cut=120;mono_safe=1;mix=18"),
    ("Estúdio A", "Room", "Sala de gravação grande e controlada.", "Large, controlled recording room.",
     "sync=0;time=18;feedback=25;diff_amount=75;tone=-15;mono_safe=1;mix=14"),
    ("Sala de Madeira Grave", "Room", "Madeira quente para baixo acústico.", "Warm wood for acoustic bass.",
     "sync=0;time=26;feedback=38;diff_amount=80;tone=-25;warmth=30;mono_safe=1;mix=15"),
    ("Caixa de Ressonância", "Room", "Ressonância de corpo nos graves-médios.", "Body resonance in the low mids.",
     "sync=0;time=8;feedback=60;diff_amount=50;peak_freq=220;peak_gain=4;peak_q=2;low_cut=80;mono_safe=1;mix=12"),
    ("Sala de Concerto Grave", "Room", "Sala de concerto discreta para baixo.", "Discreet concert hall for bass.",
     "sync=0;time=45;feedback=45;diff_amount=85;low_cut=140;tone=-20;mono_safe=1;mix=16"),
    ("Room Mono", "Room", "Sala em mono, centrada: segura em qualquer sistema.", "Centred mono room: safe on any system.",
     "sync=0;routing=Mono;time=25;feedback=30;diff_amount=70;mix=15"),
    ("Slap de Baixo", "Room", "Slapback curto para dar presença ao baixo.", "Short slapback to give the bass presence.",
     "sync=0;time=70;feedback=10;low_cut=200;high_cut=4000;mono_safe=1;mix=18"),
]

P["Drums"] = [
    # Fills
    ("Fill em Cascata", "Fills", "Semicolcheias em ping-pong que caem em cascata.", "Ping-pong sixteenths that cascade down.",
     "div=1/16;routing=Ping-Pong;feedback=65;mix=30"),
    ("Rolo de Tercinas", "Fills", "Tercinas que rodam no estéreo.", "Triplets that rotate in the stereo field.",
     "div=1/16T;feedback=60;rotation=30;mix=28"),
    ("Fill Dub", "Fills", "Fill de fita escuro, ao estilo dub.", "Dark tape fill, dub style.",
     "div=1/8D;feedback=70;character=40;tone=-30;mix=30"),
    ("Fill Ping-Pong", "Fills", "Colcheias a saltar entre lados.", "Eighths bouncing between sides.",
     "div=1/8;routing=Ping-Pong;feedback=55;mix=30"),
    ("Breakdown 1/32", "Fills", "Repetições muito rápidas para breakdowns.", "Very fast repeats for breakdowns.",
     "div=1/32;feedback=75;tone=-20;mix=25"),
    ("Fill Filtrado", "Fills", "Fill em banda estreita, tipo rádio.", "Narrow-band, radio-like fill.",
     "div=1/16;feedback=60;low_cut=600;high_cut=4000;cut_slope=24 dB/oct;mix=30"),
    ("Fill Espiral", "Fills", "Cada repetição roda 72° no espaço.", "Each repeat rotates 72° in space.",
     "div=1/16D;feedback=62;rotation=72;space=60;mix=28"),
    ("Fill Digital", "Fills", "Repetições que se degradam digitalmente.", "Repeats that degrade digitally.",
     "div=1/16;feedback=58;character=-60;mix=28"),
    ("Fill Fita", "Fills", "Tercinas de fita com wow.", "Tape triplets with wow.",
     "div=1/8T;feedback=64;character=55;wow=25;mix=28"),
    ("Fill Largo", "Fills", "Relação 3:2 entre lados para fills largos.", "3:2 ratio between sides for wide fills.",
     "div=1/16;r_ratio=1.5;feedback=55;space=80;mix=25"),
    ("Stutter", "Fills", "Repetição muito curta que gagueja.", "Very short stuttering repeat.",
     "div=1/64;feedback=80;low_cut=300;mix=35"),
    # Snare
    ("Caixa Slap", "Snare", "Slapback seco para a caixa.", "Dry slapback for the snare.",
     "sync=0;time=85;feedback=15;low_cut=250;mix=22"),
    ("Caixa 1/8D", "Snare", "Colcheia pontuada filtrada atrás da caixa.", "Filtered dotted eighth behind the snare.",
     "div=1/8D;feedback=35;low_cut=300;tone=-15;mix=22"),
    ("Caixa Dub Seca", "Snare", "Caixa dub com fita e graves cortados.", "Dub snare with tape and trimmed lows.",
     "div=1/4D;feedback=55;character=35;tone=-35;low_cut=300;mix=25"),
    ("Caixa Larga", "Snare", "Reflexões laterais que alargam a caixa.", "Side reflections that widen the snare.",
     "sync=0;time=40;r_ratio=1.4;feedback=20;diff_amount=40;space=70;mix=20"),
    ("Caixa Anos 80", "Snare", "Caixa grande e difusa dos anos 80.", "Big, diffuse 80s snare.",
     "sync=0;time=60;feedback=40;diff_amount=85;diff_spread=22;tone=10;mix=28"),
    ("Caixa Brilhante", "Snare", "Eco brilhante só nos agudos.", "Bright echo on the highs only.",
     "div=1/8;feedback=30;low_cut=800;high_cut=16000;tone=30;mix=18"),
    ("Caixa Escura", "Snare", "Eco escuro e discreto.", "Dark, discreet echo.",
     "div=1/4;feedback=40;tone=-55;mix=22"),
    ("Caixa Fantasma", "Snare", "Eco fino e difuso para ghost notes.", "Thin, diffuse echo for ghost notes.",
     "div=1/16;feedback=25;diff_amount=60;low_cut=600;mix=15"),
    ("Caixa Ressonante", "Snare", "Pico ressonante que dá tom à caixa.", "Resonant peak that gives the snare a pitch.",
     "div=1/8D;feedback=65;peak_freq=200;peak_gain=8;peak_q=4;compensate=40;mix=22"),
    ("Clap Espacial", "Snare", "Clap que roda e se espalha no estéreo.", "Clap that rotates and spreads in stereo.",
     "div=1/8;feedback=40;rotation=60;space=80;diff_amount=30;mix=22"),
    ("Rimshot Eco", "Snare", "Tercinas finas para rimshots.", "Thin triplets for rimshots.",
     "div=1/8T;feedback=45;low_cut=900;mix=20"),
    # Percussion
    ("Congas ao Sol", "Percussion", "Colcheia pontuada quente para congas.", "Warm dotted eighth for congas.",
     "div=1/8D;feedback=40;tone=-10;space=50;mix=22"),
    ("Djembé Distante", "Percussion", "Djembé lá atrás, com cauda escura.", "Djembe far back, with a dark tail.",
     "div=1/4;feedback=45;diff_amount=55;tone=-35;mix=22"),
    ("Shaker Largo", "Percussion", "Shaker muito largo, só agudos.", "Very wide shaker, highs only.",
     "div=1/16;feedback=30;out_width=180;low_cut=1500;mix=18"),
    ("Tamborim", "Percussion", "Ping-pong rápido para tamborim e pandeiro.", "Fast ping-pong for tamborim and pandeiro.",
     "div=1/16D;routing=Ping-Pong;feedback=40;low_cut=800;mix=20"),
    ("Marimba Eco", "Percussion", "Colcheias que rodam à volta da marimba.", "Eighths circling around the marimba.",
     "div=1/8;feedback=45;rotation=30;mix=22"),
    ("Kalimba Cristal", "Percussion", "Dispersão brilhante para kalimba.", "Bright dispersion for kalimba.",
     "div=1/8D;feedback=50;disp_spread=8;disp_freq=2000;mix=24"),
    ("Bongós Ping-Pong", "Percussion", "Bongós a saltar entre lados.", "Bongos bouncing between sides.",
     "div=1/16;routing=Ping-Pong;feedback=45;mix=22"),
    ("Cowbell Dub", "Percussion", "Cowbell com fita e cauda escura.", "Cowbell with tape and a dark tail.",
     "div=1/4D;feedback=60;character=40;tone=-20;mix=22"),
    ("Timbales", "Percussion", "Eco brilhante e aberto para timbales.", "Bright, open echo for timbales.",
     "div=1/8;feedback=35;tone=10;space=40;mix=20"),
    ("Hand Drum Room", "Percussion", "Sala média para percussão de mão.", "Medium room for hand drums.",
     "sync=0;time=35;feedback=40;diff_amount=75;mix=20"),
    ("Percussão Rodopio", "Percussion", "Tercinas a rodar 90° em cada repetição.", "Triplets rotating 90° on every repeat.",
     "div=1/8T;feedback=55;rotation=90;space=70;mix=24"),
]

P["FX"] = [
    ("Tape Stop", "", "Feedback alto com glide de fita longo: muda o TIME enquanto toca para travar a fita.",
     "High feedback with a long tape glide: change TIME while it plays to stop the tape.",
     "div=1/4;feedback=85;transition=Tape Glide;glide=2000;character=40;mix=40"),
    ("Eco Infinito", "", "Repetições que quase não morrem (compensação total).", "Repeats that almost never die (full compensation).",
     "div=1/4;feedback=98;compensate=100;diff_amount=40;tone=-20;mix=40"),
    ("Rádio Perdido", "", "Rádio de onda curta: banda estreita, bits, jitter e wow.", "Shortwave radio: narrow band, bits, jitter and wow.",
     "div=1/8D;feedback=70;low_cut=700;high_cut=2500;bits=8;rate=8000;jitter=40;wow=50;mix=40"),
    ("Espiral Psicadélica", "", "Tercinas rápidas a rodar 120° com modulação forte.", "Fast triplets rotating 120° with heavy modulation.",
     "div=1/16T;feedback=85;rotation=120;mod_depth=30;mod_rate=3;mix=45"),
]

P["Guitar"] = [
    ("Slapback Rockabilly", "", "Slapback de fita curto, à moda rockabilly.", "Short tape slapback, rockabilly style.",
     "sync=0;time=110;feedback=12;character=30;high_cut=6000;mix=25"),
    ("Eco de Fita Solo", "", "Semínima pontuada de fita para solos.", "Dotted tape quarter note for solos.",
     "div=1/4D;feedback=50;character=50;wow=25;mix=28"),
    ("Dobra Estéreo", "", "Dobra curta e modulada para guitarras rítmicas.", "Short, modulated double for rhythm guitars.",
     "sync=0;time=18;r_ratio=1.5;feedback=0;mod_depth=8;space=80;mix=35"),
    ("Ambient Swell", "", "Cauda larga e difusa para swells de volume.", "Wide, diffuse tail for volume swells.",
     "div=1/4;feedback=70;diff_amount=60;space=80;tone=-20;mix=40"),
    ("Arpejo Pontuado", "", "Ping-pong pontuado para arpejos limpos.", "Dotted ping-pong for clean arpeggios.",
     "div=1/8D;routing=Ping-Pong;feedback=45;tone=-10;mix=30"),
    ("Solo de Arena", "", "Tempos L/R diferentes para solos grandes.", "Different L/R times for big solos.",
     "div=1/4;r_ratio=0.75;feedback=55;space=70;mix=28"),
    ("Blues Quente", "", "Eco quente e escuro de 280 ms.", "Warm, dark 280 ms echo.",
     "sync=0;time=280;feedback=30;character=40;tone=-30;mix=22"),
    ("Shoegaze", "", "Parede de ecos modulados e difusos.", "Wall of modulated, diffuse echoes.",
     "div=1/4D;feedback=80;diff_amount=80;mod_depth=15;mod_rate=0.5;tone=-15;mix=50"),
    ("Country Clean", "", "Colcheia limpa e brilhante.", "Clean, bright eighth note.",
     "div=1/8;feedback=25;high_cut=9000;mix=20"),
    ("Reggae Skank", "", "Colcheia pontuada de fita para skanks.", "Dotted tape eighth for skanks.",
     "div=1/8D;feedback=60;character=40;low_cut=300;tone=-25;mix=30"),
    ("Funk Fino", "", "Semicolcheias discretas para guitarra funk.", "Discreet sixteenths for funk guitar.",
     "div=1/16;feedback=20;space=50;mix=15"),
    ("Cristal Modulado", "", "Colcheia modulada com dispersão brilhante.", "Modulated eighth with bright dispersion.",
     "div=1/8;feedback=50;mod_depth=12;mod_rate=1.2;disp_spread=6;mix=30"),
    ("Metal Lead", "", "Eco filtrado com ducking forte: só aparece nas pausas.", "Filtered echo with strong ducking: only appears in gaps.",
     "div=1/8;feedback=35;low_cut=400;high_cut=5000;duck=1;duck_amount=15;mix=25"),
]

P["Mega"] = [
    ("Horizonte de Néon", "", "Cauda cinematográfica larga, à moda dos sintetizadores dos anos 80.", "Wide cinematic tail in the style of 80s synthesizers.",
     "div=1/4D;feedback=80;diff_amount=75;diff_spread=30;rotation=35;mod_depth=12;mod_rate=0.2;space=90;tone=-15;mix=45"),
    ("Céu de Metal", "", "Mínima enorme e difusa para brass sintetizado.", "Huge, diffuse half note for synth brass.",
     "div=1/2;feedback=85;diff_amount=85;diff_spread=40;space=95;tone=-20;mix=50"),
    ("Cidade Chuvosa", "", "Ecos escuros e lentos, atmosfera noturna.", "Slow, dark echoes, a night-time atmosphere.",
     "div=1/4;feedback=78;diff_amount=70;mod_depth=15;mod_rate=0.15;tone=-35;mix=45"),
    ("Brass Cósmico", "", "Ping-pong grande para fanfarras sintetizadas.", "Big ping-pong for synth fanfares.",
     "div=1/8D;routing=Ping-Pong;feedback=70;diff_amount=50;space=80;mix=40"),
    ("Oceano Analógico", "", "Cauda quase infinita com modulação aleatória.", "Almost infinite tail with random modulation.",
     "div=1/2D;feedback=88;compensate=70;diff_amount=90;diff_spread=45;mod_shape=Random;mod_depth=10;mix=50"),
    ("Fanfarra Etérea", "", "Tercinas que rodam e se dissolvem.", "Triplets that rotate and dissolve.",
     "div=1/4T;feedback=75;diff_amount=60;rotation=50;space=85;mix=42"),
    ("Catedral Sintética", "", "1,5 s de difusão total: um espaço gigante.", "1.5 s of full diffusion: a giant space.",
     "sync=0;time=1500;feedback=82;diff_amount=95;diff_spread=50;space=100;tone=-25;mix=50"),
    ("Aurora", "", "Semibreve lenta e modulada para pads épicos.", "Slow, modulated whole note for epic pads.",
     "div=1/1;feedback=90;compensate=80;diff_amount=80;mod_depth=20;mod_rate=0.08;tone=-10;mix=55"),
]

_G = "gate=1;gate_thresh=-30;"
P["NonLin"] = [
    # Snare
    ("Gated Clássico", "Snare", "Cauda densa que corta de repente (gate), ao estilo dos anos 80.", "Dense tail that cuts off abruptly (gate), 80s style.",
     "sync=0;time=25;feedback=75;diff_amount=95;diff_spread=15;" + _G + "gate_hold=250;gate_release=15;tone=10;mix=35"),
    ("Gated Curto", "Snare", "Explosão curta e apertada.", "Short, tight burst.",
     "sync=0;time=20;feedback=70;diff_amount=90;diff_spread=10;" + _G + "gate_hold=120;gate_release=8;mix=35"),
    ("Gated Longo", "Snare", "Cauda gated longa para caixas grandes.", "Long gated tail for big snares.",
     "sync=0;time=35;feedback=80;diff_amount=95;diff_spread=22;" + _G + "gate_hold=450;gate_release=25;mix=35"),
    ("Gated Escuro", "Snare", "Gated reverb escuro e encorpado.", "Dark, full-bodied gated reverb.",
     "sync=0;time=28;feedback=76;diff_amount=95;diff_spread=16;" + _G + "gate_hold=300;gate_release=18;tone=-30;mix=35"),
    ("Gated Brilhante", "Snare", "Gated brilhante, sem graves.", "Bright gated sound without lows.",
     "sync=0;time=22;feedback=72;diff_amount=92;diff_spread=12;" + _G + "gate_hold=220;gate_release=12;low_cut=400;tone=30;mix=32"),
    ("Gated Largo", "Snare", "Gated muito largo no estéreo.", "Very wide gated sound.",
     "sync=0;time=26;r_ratio=1.3;feedback=75;diff_amount=95;" + _G + "gate_hold=280;gate_release=15;space=90;out_width=180;mix=34"),
    ("Gated Sujo", "Snare", "Gated com saturação de fita.", "Gated sound with tape saturation.",
     "sync=0;time=24;feedback=74;diff_amount=90;" + _G + "gate_hold=260;gate_release=15;drive=50;character=40;mix=34"),
    ("Gated Explosão", "Snare", "Cauda enorme que pára num instante.", "Huge tail that stops in an instant.",
     "sync=0;time=30;feedback=88;compensate=60;diff_amount=100;diff_spread=25;" + _G + "gate_hold=350;gate_release=12;mix=40"),
    # Toms
    ("Toms Gated", "Toms", "Toms com cauda gated clássica.", "Toms with a classic gated tail.",
     "sync=0;time=30;feedback=78;diff_amount=92;diff_spread=18;low_cut=80;" + _G + "gate_hold=380;gate_release=30;mix=32"),
    ("Toms Arena", "Toms", "Toms de arena, gate longo.", "Arena toms with a long gate.",
     "sync=0;time=45;feedback=82;diff_amount=95;diff_spread=30;" + _G + "gate_hold=500;gate_release=30;mix=32"),
    ("Toms Curtos", "Toms", "Gate curto para fills rápidos.", "Short gate for fast fills.",
     "sync=0;time=22;feedback=70;diff_amount=88;" + _G + "gate_hold=200;gate_release=15;mix=30"),
    ("Toms Rolantes", "Toms", "Cauda gated que roda entre lados.", "Gated tail that rotates between sides.",
     "sync=0;time=32;feedback=78;diff_amount=90;rotation=60;" + _G + "gate_hold=350;gate_release=25;mix=32"),
    ("Toms Escuros", "Toms", "Toms profundos com gate escuro.", "Deep toms with a dark gate.",
     "sync=0;time=34;feedback=80;diff_amount=93;" + _G + "gate_hold=400;gate_release=30;tone=-40;mix=32"),
    ("Toms Anos 80", "Toms", "O som de toms gigantes dos anos 80.", "The giant 80s tom sound.",
     "sync=0;time=38;feedback=82;diff_amount=100;diff_spread=26;" + _G + "gate_hold=420;gate_release=20;tone=10;mix=38"),
    ("Toms Fechados", "Toms", "Gate muito curto e seco.", "Very short, dry gate.",
     "sync=0;time=18;feedback=65;diff_amount=85;" + _G + "gate_hold=150;gate_release=5;mix=30"),
    ("Toms Gigantes", "Toms", "Cauda enorme com gate longo e largo.", "Huge tail with a long, wide gate.",
     "sync=0;time=60;feedback=85;diff_amount=100;diff_spread=40;" + _G + "gate_hold=650;gate_release=40;space=90;mix=38"),
]

P["Orkestra"] = [
    # Chamber
    ("Quarteto de Cordas", "Chamber", "Sala de câmara íntima para cordas.", "Intimate chamber room for strings.",
     "sync=0;time=45;feedback=55;diff_amount=85;diff_spread=20;tone=-20;mix=22"),
    ("Sala de Câmara", "Chamber", "Sala de música de câmara, equilibrada.", "Balanced chamber-music hall.",
     "sync=0;time=55;feedback=60;diff_amount=88;diff_spread=25;tone=-25;mix=24"),
    ("Piano de Câmara", "Chamber", "Espaço quente e próximo para piano.", "Warm, close space for piano.",
     "sync=0;time=40;feedback=50;diff_amount=80;tone=-15;mix=22"),
    ("Madeiras", "Chamber", "Sala clara para flautas, oboés e clarinetes.", "Clear room for flutes, oboes and clarinets.",
     "sync=0;time=35;feedback=50;diff_amount=85;tone=-10;low_cut=150;mix=20"),
    ("Coro Íntimo", "Chamber", "Pequeno coro numa capela.", "Small choir in a chapel.",
     "sync=0;time=60;feedback=62;diff_amount=90;diff_spread=28;tone=-20;mod_depth=3;mix=25"),
    ("Harpa", "Chamber", "Espaço cintilante para harpa.", "Shimmering space for harp.",
     "sync=0;time=50;feedback=58;diff_amount=75;disp_spread=5;tone=-5;mix=25"),
    # Big Hall
    ("Grande Auditório", "Big Hall", "Auditório grande com cauda longa.", "Large auditorium with a long tail.",
     "sync=0;time=120;feedback=78;diff_amount=95;diff_spread=45;space=85;tone=-30;mix=30"),
    ("Sinfónica", "Big Hall", "Sala sinfónica para orquestra completa.", "Symphony hall for a full orchestra.",
     "sync=0;time=140;feedback=80;diff_amount=95;diff_spread=50;space=90;tone=-25;mix=30"),
    ("Metais na Sala Grande", "Big Hall", "Metais com brilho e grandeza.", "Brass with shine and grandeur.",
     "sync=0;time=110;feedback=75;diff_amount=90;space=80;tone=-20;mix=28"),
    ("Percussão Sinfónica", "Big Hall", "Tímpanos e pratos numa sala grande.", "Timpani and cymbals in a big hall.",
     "sync=0;time=100;feedback=72;diff_amount=92;low_cut=60;mix=26"),
    ("Coro Catedral", "Big Hall", "Coro numa catedral enorme.", "Choir in a huge cathedral.",
     "sync=0;time=160;feedback=84;diff_amount=98;diff_spread=50;space=95;tone=-35;mix=32"),
    ("Cordas Épicas", "Big Hall", "Semínima difusa para cordas de cinema.", "Diffuse quarter note for cinematic strings.",
     "div=1/4;feedback=70;diff_amount=85;space=85;tone=-25;mix=30"),
    ("Trilha Sonora", "Big Hall", "Mínima larga e rotativa para bandas sonoras.", "Wide, rotating half note for film scores.",
     "div=1/2;feedback=75;diff_amount=90;rotation=25;space=90;tone=-20;mix=32"),
]

P["Synth"] = [
    ("Arp Ping-Pong 1/16", "", "Semicolcheias a saltar entre lados.", "Sixteenths bouncing between sides.", "div=1/16;routing=Ping-Pong;feedback=50;mix=28"),
    ("Arp Tercina", "", "Tercinas que rodam 40°.", "Triplets rotating 40°.", "div=1/8T;feedback=55;rotation=40;mix=28"),
    ("Sequência Larga", "", "Relação 3:2 entre lados para sequências.", "3:2 ratio between sides for sequences.", "div=1/8D;r_ratio=1.5;feedback=50;space=75;mix=30"),
    ("Pad Lento", "", "Mínima difusa e modulada.", "Diffuse, modulated half note.", "div=1/2;feedback=70;diff_amount=70;mod_depth=10;mod_rate=0.15;tone=-20;mix=40"),
    ("Pad Glaciar", "", "Cauda fria, longa e escura.", "Cold, long, dark tail.", "div=1/1;feedback=80;diff_amount=85;tone=-30;mix=45"),
    ("Lead Analógico", "", "Colcheia pontuada com sabor a fita.", "Dotted eighth with a tape flavour.", "div=1/8D;feedback=40;character=35;mix=25"),
    ("Lead Digital", "", "Colcheia com grão digital.", "Eighth note with digital grain.", "div=1/8;feedback=45;character=-40;mix=25"),
    ("Pluck Bolha", "", "Dispersão que faz 'bloop' nos plucks.", "Dispersion that makes plucks 'bloop'.", "div=1/16D;feedback=55;disp_spread=10;disp_freq=1500;mix=28"),
    ("Pluck Sincopado", "", "Ping-pong pontuado e brilhante.", "Bright dotted ping-pong.", "div=1/8D;routing=Ping-Pong;feedback=45;tone=10;mix=26"),
    ("Chiptune", "", "4 bits e 8 kHz: consola antiga.", "4 bits and 8 kHz: old game console.", "div=1/16;feedback=50;bits=4;rate=8000;mix=30"),
    ("Synthwave", "", "Semínima pontuada larga, fita e modulação.", "Wide dotted quarter, tape and modulation.", "div=1/4D;feedback=60;mod_depth=10;mod_rate=0.3;character=30;space=70;mix=35"),
    ("Acid Eco", "", "Pico ressonante que canta com a linha acid.", "Resonant peak that sings with the acid line.", "div=1/8D;feedback=70;peak_freq=1200;peak_gain=9;peak_q=5;compensate=40;drive=30;mix=30"),
    ("Techno Dub", "", "Dub de techno escuro, com fita.", "Dark techno dub with tape.", "div=1/8D;feedback=72;character=40;tone=-35;low_cut=300;mix=30"),
    ("House Stab", "", "Semínima pontuada filtrada para stabs.", "Filtered dotted quarter for stabs.", "div=1/4D;feedback=40;low_cut=400;tone=-15;space=50;mix=25"),
    ("Trance Gate", "", "Semicolcheias com gate curto.", "Sixteenths with a short gate.", "div=1/16;feedback=60;gate=1;gate_thresh=-30;gate_hold=80;gate_release=20;mix=30"),
    ("Brass Sintético", "", "Colcheia aberta para brass.", "Open eighth note for brass.", "div=1/8;feedback=35;space=60;mix=22"),
    ("Bell Cristal", "", "Tercinas brilhantes para sinos.", "Bright triplets for bells.", "div=1/8T;feedback=55;low_cut=800;high_cut=16000;disp_spread=4;mix=28"),
    ("Wobble", "", "Modulação forte e rápida: o eco oscila.", "Strong, fast modulation: the echo wobbles.", "sync=0;time=250;feedback=50;mod_depth=40;mod_rate=2;mix=30"),
    ("Detune Largo", "", "Desafinação estéreo tipo supersaw.", "Supersaw-like stereo detune.", "sync=0;time=25;r_ratio=1.2;feedback=10;mod_depth=15;mod_rate=0.4;space=90;mix=35"),
    ("Espiral 72°", "", "Cada repetição roda 72°.", "Each repeat rotates 72°.", "div=1/8;feedback=65;rotation=72;space=70;mix=30"),
    ("Reflexão", "", "Matriz em modo reflexão: o campo espelha a cada repetição.", "Matrix in reflection mode: the field mirrors on every repeat.", "div=1/8D;feedback=60;rot_mode=Reflection;rotation=45;mix=30"),
    ("Pad Fita", "", "Pad com fita gasta e wow.", "Pad with worn tape and wow.", "div=1/2;feedback=70;character=60;wow=40;tone=-30;mix=38"),
    ("Lo-Fi Keys", "", "Teclas lo-fi, digitais e escuras.", "Lo-fi keys, digital and dark.", "div=1/4;feedback=45;character=-45;tone=-20;wow=20;mix=28"),
    ("Nuvem Digital", "", "Repetições minúsculas que formam uma nuvem.", "Tiny repeats that form a cloud.", "div=1/32;feedback=80;diff_amount=60;bits=10;jitter=50;mix=35"),
    ("Sub Bass Synth", "", "Eco discreto e mono para sub.", "Discreet mono echo for sub.", "div=1/8;feedback=20;mono_safe=1;high_cut=3000;mix=12"),
    ("Arpejo Gelo", "", "Tercinas frias e brilhantes.", "Cold, bright triplets.", "div=1/16T;feedback=55;tone=25;diff_amount=30;mix=26"),
    ("Saw Lead Largo", "", "Lead largo com relação 4:3.", "Wide lead with a 4:3 ratio.", "div=1/8D;r_ratio=1.33;feedback=45;space=80;mix=25"),
    ("Pad Aleatório", "", "Modulação aleatória e cauda difusa.", "Random modulation and a diffuse tail.", "div=1/4;feedback=70;mod_shape=Random;mod_depth=15;mod_rate=0.5;diff_amount=60;mix=40"),
    ("FM Bell", "", "Pico agudo para sinos FM.", "High peak for FM bells.", "div=1/8D;feedback=50;peak_freq=3000;peak_gain=5;mix=25"),
    ("Retro Eco", "", "12 bits e 22 kHz: sampler antigo.", "12 bits and 22 kHz: vintage sampler.", "div=1/4;feedback=55;bits=12;rate=22050;character=-20;mix=28"),
    ("Sync Lead", "", "Ping-pong rápido para leads.", "Fast ping-pong for leads.", "div=1/16D;routing=Ping-Pong;feedback=45;mix=25"),
    ("Modular", "", "Modulação aleatória rápida e rotação negativa.", "Fast random modulation and negative rotation.", "div=1/8T;feedback=65;mod_shape=Random;mod_depth=10;mod_rate=2;rotation=-30;mix=30"),
    ("Ambient Pluck", "", "Plucks que se dissolvem numa cauda escura.", "Plucks that dissolve into a dark tail.", "div=1/4D;feedback=70;diff_amount=75;tone=-25;mix=35"),
    ("Órgão Rotativo", "", "Modulação rápida estilo altifalante rotativo.", "Fast rotary-speaker-style modulation.", "sync=0;time=5;feedback=0;mod_depth=30;mod_rate=6;mod_offset=25;mix=40"),
    ("Drone", "", "Feedback quase infinito para drones.", "Almost infinite feedback for drones.", "div=1/1;feedback=92;compensate=80;diff_amount=90;tone=-40;mix=50"),
    ("Supersaw Espaço", "", "Semínima larga e difusa para supersaws.", "Wide, diffuse quarter note for supersaws.", "div=1/4;feedback=60;space=90;diff_amount=50;rotation=20;mix=35"),
]

P["VIP"] = [
    ("Voz Pop de Rádio", "", "Abordagem típica de grandes misturas pop: semínima filtrada e com ducking, presente mas invisível.",
     "Typical big pop-mix approach: filtered quarter note with ducking, present but invisible.",
     "div=1/4;feedback=20;low_cut=400;high_cut=5000;duck=1;duck_amount=12;mix=18"),
    ("Hip-Hop Anos 90", "", "Colcheia pontuada de fita, escura, como nas produções clássicas de hip-hop.",
     "Dark tape dotted eighth, as in classic hip-hop productions.",
     "div=1/8D;feedback=30;character=40;tone=-30;mix=20"),
    ("Throw de R&B", "", "Throw em ping-pong com ducking, técnica comum em R&B moderno.",
     "Ping-pong throw with ducking, a common technique in modern R&B.",
     "div=1/4D;routing=Ping-Pong;feedback=45;duck=1;tone=-15;mix=28"),
    ("Slap Rock Clássico", "", "Slapback de fita de 120 ms usado em gravações de rock clássico.",
     "120 ms tape slapback used on classic rock records.",
     "sync=0;time=120;feedback=10;character=35;high_cut=5000;mix=22"),
    ("Cauda de Balada", "", "Semínima difusa com ducking para baladas.", "Diffuse quarter note with ducking for ballads.",
     "div=1/4;feedback=55;diff_amount=50;tone=-25;duck=1;mix=25"),
    ("Dub Mestre", "", "Feedback alto de fita e graves cortados, à maneira dos mestres do dub.",
     "High tape feedback and cut lows, the way the dub masters do it.",
     "div=1/4D;feedback=75;character=50;tone=-35;low_cut=250;mix=30"),
    ("EDM Lead", "", "Ping-pong filtrado com ducking forte para leads de festival.", "Filtered ping-pong with strong ducking for festival leads.",
     "div=1/8D;routing=Ping-Pong;feedback=50;low_cut=500;duck=1;duck_amount=15;mix=28"),
    ("Afrobeats Vocal", "", "Colcheia pontuada leve com ducking para vozes de Afrobeats.", "Light dotted eighth with ducking for Afrobeats vocals.",
     "div=1/8D;feedback=35;tone=-10;space=50;duck=1;mix=22"),
    ("Largura de Masterização", "", "Truque de masterização: alarga só os agudos, graves em mono.", "Mastering trick: widens only the highs, mono lows.",
     "sync=0;time=10;r_ratio=1.5;feedback=0;low_cut=2000;mono_safe=1;mix=6"),
]

P["Vocals"] = [
    ("Rap Seco 1/8", "", "Colcheia muito discreta para rap.", "Very discreet eighth note for rap.", "div=1/8;feedback=15;low_cut=300;high_cut=6000;mix=12"),
    ("Rap Throw", "", "Throw em ping-pong para o fim das barras.", "Ping-pong throw for the end of bars.", "div=1/4D;routing=Ping-Pong;feedback=40;duck=1;duck_amount=15;mix=25"),
    ("Pop Brilhante", "", "Colcheia pontuada aberta com ducking.", "Open dotted eighth with ducking.", "div=1/8D;feedback=30;tone=10;duck=1;mix=18"),
    ("Pop Íntimo", "", "Eco escuro e próximo para versos.", "Dark, close echo for verses.", "div=1/8;feedback=20;tone=-20;duck=1;mix=15"),
    ("Balada Lenta", "", "Semínima difusa para baladas.", "Diffuse quarter note for ballads.", "div=1/4;feedback=45;diff_amount=40;tone=-30;duck=1;mix=22"),
    ("R&B Sedoso", "", "Colcheia pontuada suave e modulada.", "Smooth, modulated dotted eighth.", "div=1/8D;feedback=35;mod_depth=4;tone=-20;duck=1;mix=20"),
    ("Gospel Coro", "", "Espaço grande e quente para coros gospel.", "Big, warm space for gospel choirs.", "sync=0;time=70;feedback=50;diff_amount=80;space=70;tone=-15;mix=25"),
    ("Coro Largo", "", "Dobra muito larga para coros.", "Very wide double for choirs.", "sync=0;time=25;r_ratio=1.5;feedback=10;mod_depth=8;space=85;mix=30"),
    ("Ad-lib Espacial", "", "Ad-libs que saltam e rodam.", "Ad-libs that bounce and rotate.", "div=1/8D;routing=Ping-Pong;feedback=55;rotation=30;mix=35"),
    ("Ad-lib Telefone", "", "Ad-libs em banda estreita e saturados.", "Narrow-band, saturated ad-libs.", "div=1/8;feedback=40;low_cut=800;high_cut=3000;cut_slope=24 dB/oct;drive=30;mix=35"),
    ("Voz Rádio Antigo", "", "Eco de rádio antigo com bits reduzidos.", "Old radio echo with reduced bits.", "div=1/4;feedback=35;bits=10;rate=11025;low_cut=500;high_cut=3500;mix=30"),
    ("Kizomba Romântica", "", "Semínima quente e escura com ducking.", "Warm, dark quarter note with ducking.", "div=1/4;feedback=40;tone=-30;warmth=50;duck=1;mix=20"),
    ("Semba Vocal", "", "Colcheia pontuada com sabor a fita.", "Dotted eighth with a tape flavour.", "div=1/8D;feedback=30;character=30;mix=18"),
    ("Afro Pop Vocal", "", "Ping-pong pontuado leve com ducking.", "Light dotted ping-pong with ducking.", "div=1/8D;routing=Ping-Pong;feedback=35;duck=1;tone=-10;mix=20"),
    ("Amapiano Vocal", "", "Semínima pontuada difusa e escura.", "Diffuse, dark dotted quarter.", "div=1/4D;feedback=45;diff_amount=30;tone=-20;duck=1;mix=22"),
    ("Rock Slap", "", "Slapback de fita para voz de rock.", "Tape slapback for rock vocals.", "sync=0;time=100;feedback=10;character=40;mix=22"),
    ("Grito de Rock", "", "Eco saturado para vozes gritadas.", "Saturated echo for screamed vocals.", "sync=0;time=140;feedback=20;drive=40;low_cut=400;mix=20"),
    ("Soul Vintage", "", "Fita gasta e quente, à moda soul.", "Warm, worn tape, soul style.", "div=1/8D;feedback=35;character=50;wow=20;tone=-30;mix=22"),
    ("Jazz Club", "", "Eco difuso de clube de jazz.", "Diffuse jazz-club echo.", "sync=0;time=120;feedback=25;diff_amount=50;tone=-25;mix=15"),
    ("Spoken Word", "", "Espaço discreto para voz falada.", "Discreet space for spoken word.", "sync=0;time=60;feedback=10;diff_amount=40;low_cut=300;mix=10"),
    ("Podcast Presença", "", "Ambiente mínimo que dá presença à voz.", "Minimal ambience that gives the voice presence.", "sync=0;time=15;feedback=0;diff_amount=70;low_cut=200;mix=8"),
    ("Dobra Haas", "", "Dobra de 14 ms (efeito Haas).", "14 ms double (Haas effect).", "sync=0;time=14;r_ratio=1.3;feedback=0;space=70;mix=30"),
    ("Dobra Modulada", "", "Dobra com modulação, como uma segunda voz.", "Modulated double, like a second voice.", "sync=0;time=22;feedback=0;mod_depth=10;mod_rate=0.8;space=70;mix=30"),
    ("Harmonia Etérea", "", "Cauda longa e larga para harmonias.", "Long, wide tail for harmonies.", "div=1/4;feedback=65;diff_amount=75;space=85;tone=-25;mix=35"),
    ("Sussurro Largo", "", "Espaço largo e difuso para sussurros.", "Wide, diffuse space for whispers.", "sync=0;time=40;feedback=30;diff_amount=80;space=90;tone=-10;mix=25"),
    ("Refrão Grande", "", "Semínima pontuada larga para refrões.", "Wide dotted quarter for choruses.", "div=1/4D;feedback=45;space=70;diff_amount=40;duck=1;mix=25"),
    ("Verso Seco", "", "Semicolcheia quase impercetível.", "Almost imperceptible sixteenth.", "div=1/16;feedback=10;low_cut=400;mix=10"),
    ("Pré-Refrão Filtrado", "", "Eco filtrado que prepara o refrão.", "Filtered echo that sets up the chorus.", "div=1/8;feedback=45;low_cut=700;high_cut=4000;mix=28"),
    ("Ponte Atmosférica", "", "Mínima difusa e escura para pontes.", "Diffuse, dark half note for bridges.", "div=1/2;feedback=65;diff_amount=70;tone=-30;mix=30"),
    ("Fim de Frase", "", "Throw longo com ducking forte.", "Long throw with strong ducking.", "div=1/4D;routing=Ping-Pong;feedback=60;tone=-20;duck=1;duck_amount=18;mix=35"),
    ("Voz Dub", "", "Voz em dub com fita e cauda escura.", "Dub vocal with tape and a dark tail.", "div=1/4D;feedback=70;character=45;tone=-35;low_cut=300;mix=30"),
    ("Voz Lo-Fi", "", "Eco digital degradado.", "Degraded digital echo.", "div=1/8;feedback=40;character=-50;tone=-20;mix=25"),
    ("Voz Robô", "", "Repetições minúsculas a 6 bits.", "Tiny 6-bit repeats.", "div=1/32;feedback=70;bits=6;rate=8000;mix=35"),
    ("Trap Melódico", "", "Ping-pong pontuado, escuro e difuso.", "Dark, diffuse dotted ping-pong.", "div=1/8D;routing=Ping-Pong;feedback=45;tone=-20;diff_amount=30;duck=1;mix=25"),
    ("Drill Escuro", "", "Semicolcheia pontuada escura e saturada.", "Dark, saturated dotted sixteenth.", "div=1/16D;feedback=35;tone=-45;drive=30;mix=20"),
    ("Reggaeton", "", "Ping-pong filtrado com ducking.", "Filtered ping-pong with ducking.", "div=1/8D;routing=Ping-Pong;feedback=35;low_cut=400;duck=1;mix=20"),
    ("Fado", "", "Espaço quente e escuro para fado.", "Warm, dark space for fado.", "sync=0;time=90;feedback=35;diff_amount=60;tone=-30;warmth=40;mix=18"),
    ("Morna Suave", "", "Semínima suave e escura.", "Soft, dark quarter note.", "div=1/4;feedback=40;diff_amount=40;tone=-35;mix=20"),
    ("Ópera", "", "Sala grande para voz lírica.", "Large hall for operatic voice.", "sync=0;time=110;feedback=70;diff_amount=92;diff_spread=40;space=85;tone=-25;mix=28"),
    ("Backing Largo", "", "Backing vocals abertos para os lados.", "Backing vocals spread to the sides.", "sync=0;time=30;r_ratio=1.4;feedback=20;space=90;diff_amount=40;mix=30"),
    ("Voz em Túnel", "", "Banda estreita a rodar 60°.", "Narrow band rotating 60°.", "div=1/8T;feedback=70;low_cut=600;high_cut=2500;rotation=60;mix=35"),
    ("Eco Cantante", "", "Pico ressonante que canta com a voz.", "Resonant peak that sings with the voice.", "div=1/4;feedback=80;peak_freq=1500;peak_gain=6;peak_q=3;compensate=40;mix=30"),
    ("Pronto para Freeze", "", "Cauda densa: carrega em FREEZE na última palavra.", "Dense tail: press FREEZE on the last word.", "div=1/8;feedback=80;diff_amount=70;space=80;mix=40"),
    ("Slap Duplo", "", "Dois slapbacks com tempos diferentes.", "Two slapbacks with different times.", "sync=0;time=80;r_ratio=1.5;feedback=15;mix=22"),
    ("Voz Gated", "", "Ambiente denso que corta quando a voz pára.", "Dense ambience that cuts when the voice stops.", "sync=0;time=30;feedback=75;diff_amount=90;gate=1;gate_thresh=-35;gate_hold=200;gate_release=20;mix=25"),
]

EXPECTED = {"Ambience": 20, "Bass": 24, "Drums": 33, "FX": 4, "Guitar": 13, "Mega": 8,
            "NonLin": 16, "Orkestra": 13, "Synth": 36, "VIP": 9, "Vocals": 45}

# uso sugerido por categoria / subcategoria (PT, EN)
USE = {
    ("Ambience", ""): ("Qualquer fonte que precise de espaço curto; insert ou envio.", "Any source that needs a short space; insert or send."),
    ("Bass", "Ambience"): ("Baixo eléctrico ou sintetizado; graves ficam em mono.", "Electric or synth bass; lows stay mono."),
    ("Bass", "Chorus"): ("Baixo em insert (o MIX já está acertado).", "Bass on an insert (MIX is already set)."),
    ("Bass", "Room"): ("Baixo em insert, mix baixo.", "Bass on an insert, low mix."),
    ("Drums", "Fills"): ("Envio automatizado nos fills e viradas.", "Automated send on fills and turnarounds."),
    ("Drums", "Snare"): ("Caixa, clap ou rimshot num envio.", "Snare, clap or rimshot on a send."),
    ("Drums", "Percussion"): ("Percussões e loops num envio.", "Percussion and loops on a send."),
    ("FX", ""): ("Efeitos e transições; automatiza os controlos.", "Effects and transitions; automate the controls."),
    ("Guitar", ""): ("Guitarra em insert ou envio.", "Guitar on an insert or send."),
    ("Mega", ""): ("Pads, brass e leads sintetizados em envio a 100 % wet.", "Synth pads, brass and leads on a send at 100 % wet."),
    ("NonLin", "Snare"): ("Caixa num envio; ajusta o GATE em UTILITIES.", "Snare on a send; adjust the GATE in UTILITIES."),
    ("NonLin", "Toms"): ("Toms num envio; ajusta o GATE em UTILITIES.", "Toms on a send; adjust the GATE in UTILITIES."),
    ("Orkestra", "Chamber"): ("Instrumentos acústicos e grupos pequenos.", "Acoustic instruments and small ensembles."),
    ("Orkestra", "Big Hall"): ("Orquestra, coros e bandas sonoras.", "Orchestra, choirs and film scores."),
    ("Synth", ""): ("Sintetizadores em insert ou envio.", "Synths on an insert or send."),
    ("VIP", ""): ("Ponto de partida rápido para a técnica descrita.", "Quick starting point for the technique described."),
    ("Vocals", ""): ("Voz principal ou de apoio, normalmente num envio.", "Lead or backing vocal, usually on a send."),
}

TAG_CAT = {"Ambience": ("ambiente", "ambience"), "Bass": ("baixo", "bass"), "Drums": ("bateria", "drums"), "FX": ("efeito", "fx"),
           "Guitar": ("guitarra", "guitar"), "Mega": ("cinematográfico", "cinematic"), "NonLin": ("não linear", "non-linear"),
           "Orkestra": ("orquestra", "orchestra"), "Synth": ("sintetizador", "synth"), "VIP": ("técnica pro", "pro technique"),
           "Vocals": ("voz", "vocal")}
TAG_SUB = {"Ambience": ("ambiente", "ambience"), "Chorus": ("chorus", "chorus"), "Room": ("sala", "room"), "Fills": ("fills", "fills"),
           "Snare": ("caixa", "snare"), "Percussion": ("percussão", "percussion"), "Toms": ("toms", "toms"),
           "Chamber": ("câmara", "chamber"), "Big Hall": ("sala grande", "big hall")}

# ---------------------------------------------------------------- validação
NUM = {  # id: (min, max)
    "time": (1, 8000), "feedback": (0, 100), "space": (0, 100), "tone": (-100, 100), "character": (-100, 100), "mix": (0, 100),
    "r_ratio": (0.25, 2), "fb_balance": (-100, 100), "glide": (10, 2000), "rotation": (-180, 180), "in_rot": (-45, 45),
    "out_rot": (-45, 45), "in_width": (0, 200), "out_width": (0, 200), "low_cut": (20, 2000), "high_cut": (1000, 20000),
    "peak_freq": (100, 10000), "peak_gain": (-18, 18), "peak_q": (0.3, 10), "compensate": (0, 100), "mod_depth": (0, 50),
    "mod_rate": (0.01, 10), "mod_offset": (0, 100), "wow": (0, 100), "disp_freq": (50, 5000), "disp_spread": (0, 50),
    "diff_amount": (0, 100), "diff_spread": (1, 50), "warmth": (0, 100), "drive": (0, 100), "bits": (1, 24), "rate": (1000, 48000),
    "jitter": (0, 100), "grit_mix": (0, 100), "pre_drive": (-12, 12), "duck_thresh": (-60, 0), "duck_amount": (0, 30),
    "duck_attack": (0.1, 100), "duck_hold": (0, 500), "duck_release": (10, 2000), "wet_trim": (-24, 12), "out_trim": (-24, 12),
    "gate_thresh": (-60, 0), "gate_hold": (10, 1000), "gate_release": (2, 500),
}
BOOL = {"sync", "freeze", "duck", "bypass", "mono_safe", "mod_sync", "gate"}
CHOICE = {
    "div": ["1/1", "1/2D", "1/2", "1/2T", "1/4D", "1/4", "1/4T", "1/8D", "1/8", "1/8T", "1/16D", "1/16", "1/16T", "1/32", "1/32T", "1/64", "1/128"],
    "routing": ["Dual", "Ping-Pong", "Mono"], "transition": ["Tape Glide", "Crossfade"], "rot_mode": ["Rotation", "Reflection"],
    "cut_slope": ["6 dB/oct", "12 dB/oct", "24 dB/oct"], "mod_shape": ["Sine", "Triangle", "Random"], "mix_law": ["Linear", "Equal Power"],
    "loop_order": ["0", "1", "2", "3"], "mod_div": [str(i) for i in range(8)],
}

def validate(data, where):
    for tok in data.split(";"):
        k, v = tok.split("=", 1)
        if k in NUM:
            x = float(v); lo, hi = NUM[k]
            assert lo <= x <= hi, f"{where}: {k}={v} fora de [{lo},{hi}]"
        elif k in BOOL:
            assert v in ("0", "1"), f"{where}: {k}={v}"
        elif k in CHOICE:
            assert v in CHOICE[k], f"{where}: {k}={v} não é uma escolha válida"
        else:
            raise AssertionError(f"{where}: parâmetro desconhecido {k}")

def existing_names(path):
    src = open(path, encoding="utf-8").read()
    body = src.split("factoryPresets()")[1].split("presetEnglish")[0]
    return set(re.findall(r'\{ "([^"]+)", "[^"]+", "', body))

def c(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'

def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    names = existing_names(os.path.join(root, "Source", "Presets.h"))
    assert len(names) == 61, len(names)
    out, total = [], 0
    for cat, items in P.items():
        assert len(items) == EXPECTED[cat], (cat, len(items), EXPECTED[cat])
        out.append(f"        // ------------------------------------------------ {cat} ({len(items)})")
        for name, sub, pt, en, data in items:
            assert name not in names, f"nome repetido: {name}"
            names.add(name)
            validate(data, name)
            usePt, useEn = USE[(cat, sub)] if (cat, sub) in USE else USE[(cat, "")]
            tp, te = [TAG_CAT[cat][0]], [TAG_CAT[cat][1]]
            if sub: tp.append(TAG_SUB[sub][0]); te.append(TAG_SUB[sub][1])
            kv = dict(t.split("=", 1) for t in data.split(";"))
            for key, pt_t, en_t in (("routing", "ping-pong", "ping-pong"), ("gate", "gate", "gate"), ("duck", "ducking", "ducking")):
                if kv.get(key) in ("Ping-Pong", "1"): tp.append(pt_t); te.append(en_t)
            if float(kv.get("character", 0)) >= 30 or float(kv.get("wow", 0)) >= 25: tp.append("fita"); te.append("tape")
            if float(kv.get("character", 0)) <= -30 or float(kv.get("bits", 24)) < 16: tp.append("digital"); te.append("digital")
            if float(kv.get("diff_amount", 0)) >= 70: tp.append("difuso"); te.append("diffuse")
            if abs(float(kv.get("rotation", 0))) >= 30: tp.append("rotação"); te.append("rotation")
            if float(kv.get("mod_depth", 0)) >= 10: tp.append("modulação"); te.append("modulation")
            out.append(f"        {{ {c(name)}, {c(cat)}, {c(pt)}, {c(usePt)}, {c(', '.join(tp))}, {c(data)}, {c(sub)}, {c(en)}, {c(useEn)}, {c(', '.join(te))} }},")
            total += 1
    assert total == 221, total
    hdr = f"""// =============================================================================
//  DELEX by Piradex — presets de fábrica adicionais ({total})
//  GERADO por tools/presets_extra.py — não editar à mão.
//  Incluído dentro da lista de factoryPresets() em Presets.h.
//  Campos: nome, categoria, descrição, uso, etiquetas, dados, subcategoria,
//          descrição (EN), uso (EN), etiquetas (EN)
// =============================================================================
"""
    path = os.path.join(root, "Source", "PresetsExtra.inc")
    with open(path, "w", encoding="utf-8") as f:
        f.write(hdr + "\n".join(out) + "\n")
    print(f"{total} presets escritos em {os.path.relpath(path, root)}")

if __name__ == "__main__":
    main()
