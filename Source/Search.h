// =============================================================================
//  DELEX by Piradex — pesquisa de presets por descrição
//  "voz escura e larga", "dark wide vocal", "eco curto para caixa"…
//  Cada palavra da pesquisa expande-se em sinónimos (PT e EN); cada preset recebe
//  pontos pela categoria (4), nome (3), etiquetas (2) e descrição/uso (1), nas duas línguas.
// =============================================================================
#pragma once

#include "Presets.h"

namespace delex::search
{
inline juce::String fold (juce::String s)
{
    s = s.toLowerCase();
    static const char* from[] = { "á", "à", "â", "ã", "é", "ê", "í", "ó", "ô", "õ", "ú", "ç", "-" };
    static const char* to[]   = { "a", "a", "a", "a", "e", "e", "i", "o", "o", "o", "u", "c", "" };
    for (size_t i = 0; i < sizeof (from) / sizeof (from[0]); ++i) s = s.replace (juce::String::fromUTF8 (from[i]), to[i]);
    return s;
}

inline const std::vector<juce::StringArray>& synonymGroups()
{
    static const std::vector<juce::StringArray> g = []
    {
        const char* raw[] = {
            "escuro,escura,dark,darker,abafado,abafada,quente,warm,fechado,muffled",
            "brilhante,bright,aberto,aberta,claro,clara,air,ar,cristalino,crisp,shiny",
            "largo,larga,wide,espacoso,espacosa,estereo,stereo,amplo,ampla,spacious,width",
            "curto,curta,short,slap,slapback,rapido,rapida,fast,tight,apertado",
            "longo,longa,long,cauda,tail,infinito,infinite,enorme,huge,gigante,epic,epico",
            "voz,vozes,vocal,vocals,vox,cantor,cantora,singer,rap,refrao,chorus",
            "bateria,drums,drum,caixa,snare,kick,bombo,perc,percussao,percussion,toms,tom,clap",
            "baixo,bass,808,sub,grave,graves,low",
            "guitarra,guitar,viola,cordas",
            "sintetizador,synth,pad,pads,teclas,keys,piano,teclado,lead",
            "sujo,suja,dirty,lofi,degradado,grit,crush,bitcrush,digital,gritty",
            "fita,tape,analogico,analog,vintage,saturado,saturation,warmth",
            "ritmo,ritmico,ritmica,rhythmic,groove,sincopado,sincopa,syncopated,swing",
            "pingpong,ping,pong,alterna,alternate,bounce",
            "reverso,reverse,contrario,backwards,invertido",
            "shimmer,oitava,octave,angelical,celestial",
            "dub,reggae,siren,sirene",
            "transicao,transition,riser,build,drop,subida",
            "subtil,subtle,discreto,discreta,leve,light,suave,soft",
            "afro,kizomba,amapiano,semba,kuduro,afrobeats,tarraxinha",
            "trap,drill,hiphop,hip,throw",
            "ambiente,ambience,ambient,atmosfera,atmosphere,textura,texture",
            "mono,centro,center,centrado",
            "orquestra,orkestra,orchestral,strings,hall,sala,camara,chamber",
            "master,mistura,mix,bus" };
        std::vector<juce::StringArray> out;
        for (auto* r : raw) out.push_back (juce::StringArray::fromTokens (juce::String (r), ",", ""));
        return out;
    }();
    return g;
}

inline juce::StringArray expand (const juce::String& token)
{
    juce::StringArray v { token };
    for (auto& grp : synonymGroups())
        for (auto& w : grp)
            if (w == token || (token.length() >= 4 && w.startsWith (token)))
            {
                v.addArray (grp);
                break;
            }
    v.removeDuplicates (false);
    return v;
}

inline juce::StringArray queryTokens (const juce::String& q)
{
    static const juce::StringArray stop { "de", "da", "do", "para", "com", "um", "uma", "the", "a", "o", "e", "and", "for", "with",
                                          "que", "muito", "very", "of", "in", "em", "no", "na", "eco", "echo", "delay" };
    auto t = juce::StringArray::fromTokens (fold (q), " ,.;/", "");
    t.removeEmptyStrings();
    juce::StringArray out;
    for (auto& w : t) if (! stop.contains (w)) out.add (w);
    if (out.isEmpty()) out = t;   // pesquisa só com palavras comuns: usa-as na mesma
    return out;
}

// palavra da pesquisa presente no texto (início de palavra)
inline bool hit (const juce::String& text, const juce::StringArray& variants)
{
    for (auto& v : variants)
    {
        if (v.length() < 2) continue;
        int i = text.indexOf (v);
        while (i >= 0)
        {
            if (i == 0 || ! juce::CharacterFunctions::isLetterOrDigit (text[i - 1])) return true;
            i = text.indexOf (i + 1, v);
        }
    }
    return false;
}

struct Fields { juce::String name, cat, tags, desc; };

inline Fields fieldsOf (const FactoryPreset& p)
{
    Fields f;
    f.name = fold (juce::String::fromUTF8 (p.name));
    f.cat  = fold (juce::String::fromUTF8 (p.category) + " " + juce::String::fromUTF8 (p.sub));
    juce::String tags = juce::String::fromUTF8 (p.tags), desc = juce::String::fromUTF8 (p.description) + " " + juce::String::fromUTF8 (p.use);
    if (p.tagsEn != nullptr) tags << " " << juce::String::fromUTF8 (p.tagsEn);
    if (p.descEn != nullptr) desc << " " << juce::String::fromUTF8 (p.descEn);
    if (p.useEn != nullptr)  desc << " " << juce::String::fromUTF8 (p.useEn);
    if (auto* e = presetEnglish (p.name)) { tags << " " << juce::String::fromUTF8 (e->tags); desc << " " << juce::String::fromUTF8 (e->description) << " " << juce::String::fromUTF8 (e->use); }
    f.tags = fold (tags); f.desc = fold (desc);
    return f;
}

// pontuação (0 = não corresponde). Exige pelo menos metade das palavras encontradas.
inline float score (const Fields& f, const juce::StringArray& tokens)
{
    if (tokens.isEmpty()) return 1.0f;
    float s = 0; int found = 0;
    for (auto& t : tokens)
    {
        const auto v = expand (t);
        float best = 0;
        if (hit (f.cat, v)) best = 4.0f;                 // a categoria diz qual é a fonte: pesa mais
        else if (hit (f.name, v)) best = 3.0f;
        else if (hit (f.tags, v)) best = 2.0f;
        else if (hit (f.desc, v)) best = 1.0f;
        if (best > 0) { ++found; s += best; }
    }
    if (found * 2 < tokens.size()) return 0.0f;
    return s + (float) found * 2.0f;
}
} // namespace delex::search
