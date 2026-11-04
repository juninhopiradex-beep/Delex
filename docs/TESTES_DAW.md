# DELEX by Piradex — checklist de testes em DAW (v1.0.0)

O CI já valida o plugin automaticamente (auval em Intel e Apple Silicon, pluginval, testes de DSP e de interface). Esta lista cobre o que só se vê dentro de um DAW real. Corre-a antes de cada lançamento, ou entrega-a aos beta testers. Marca cada linha com ✅ / ❌ e anota o DAW, a versão, o sistema e o computador.

**Computadores mínimos a cobrir:**
- MacBook Pro 2013 (Intel), Catalina 10.15.7.
- Um Mac Apple Silicon (M1 ou mais recente) com o macOS actual.
- Um PC Windows 10 ou 11 de 64 bits.

## A. Em todos os DAW

| # | Teste | Como | Resultado esperado |
|---|---|---|---|
| A1 | Instalação | Instalador .pkg / Setup.exe; rescan de plugins | Aparece "DELEX by Piradex" (Piradex) em VST3, AU e AAX |
| A2 | Senha | Primeira abertura → senha beta → "Lembrar" | Desbloqueia; ao reabrir o DAW não volta a pedir |
| A3 | Sessão guardada | Mexe em 10 parâmetros, A/B, idioma, tamanho 125 %; grava, fecha e reabre o projecto | Tudo igual (incluindo preset, A/B, vista e aba) |
| A4 | Automação | Escreve automação em FEEDBACK, MIX, ROTATION e loop_order; reproduz | Segue sem cliques; a ordem do loop muda ao tocar |
| A5 | Tempo do projecto | SYNC 1/4D; muda o BPM a meio (90 → 120) | O eco acompanha (glide de fita ou crossfade) |
| A6 | Bounce/export offline | Exporta 30 s mais rápido que tempo real | Igual à reprodução em tempo real (sem eco atrasado ou cortado) |
| A7 | Taxas de amostragem | 44.1, 48, 96 kHz (e 192 se o DAW deixar) | Tempo em ms correcto; sem ruídos |
| A8 | Buffer | 32, 128, 1024 amostras | Sem estalos; CPU razoável |
| A9 | Mono → estéreo | Insere numa pista mono | Saída estéreo com ecos a rodar |
| A10 | Bypass | Bypass do DAW e BYPASS do plugin, com ecos a soar | Sem cliques |
| A11 | Sidechain | Envia outra pista para o sidechain; DUCKER › SIDECHAIN | Indicador "SIDECHAIN A RECEBER"; os ecos baixam com a outra pista |
| A12 | MIDI learn | Clique direito em FEEDBACK → Aprender MIDI; mexe num CC | O knob mostra "CC n" e segue o controlador; continua após reabrir o projecto |
| A13 | Escuta | ESCUTA: Delta, Mono, Telemóvel, Club | Muda o que se ouve; volta a Normal; não fica nos presets |
| A14 | Bloqueios | No browser: BLOQUEAR TEMPO + MIX; percorre presets | Tempo e mix ficam fixos |
| A15 | HQ 2x | Drive 80 %, HQ ligado e desligado | O primeiro eco cai no mesmo sítio; menos aspereza com HQ |
| A16 | Interface | 75 %, 100 %, 200 %; ecrã Retina/HiDPI; arrastar o canto | Nítida, proporções correctas |
| A17 | Várias instâncias | 16 instâncias em pistas diferentes | Estável; CPU anotada |
| A18 | Idioma | PT ↔ EN no cabeçalho | Interface muda logo; automações continuam a funcionar |

## B. Específico de cada DAW

**Logic Pro / GarageBand (AU)**
- B1. O plugin aparece em *Audio Units › Piradex*. Por receber MIDI, também aparece como efeito controlado por MIDI.
- B2. Sidechain: menu *Side Chain* no topo da janela do plugin.
- B3. Para o MIDI learn, escolhe a pista do DELEX como destino MIDI, ou usa um instrumento externo encaminhado para ela.
- B4. Testa também no Catalina com um Logic 10.5–10.6.

**Ableton Live (VST3 / AU)**
- B5. Sidechain: no dispositivo, abre o painel de routing do sidechain e escolhe a pista de origem.
- B6. Para o MIDI learn, cria uma pista MIDI com *MIDI To* = pista do DELEX → DELEX.
- B7. Teste de *Freeze/Flatten* da pista.

**FL Studio (VST3)**
- B8. Sidechain: no Mixer, com a pista de origem seleccionada, clica com o botão direito na seta de envio da pista do DELEX → *Sidechain to this track*. Depois, no wrapper, *Processing → input 3-4*.
- B9. Para o MIDI learn: nas definições do wrapper, define a *MIDI input port* e põe a mesma porta no controlador.
- B10. Teste de *Smart disable*: os ecos não podem ser cortados.

**Pro Tools (AAX)**
- B11. Só abre no Pro Tools normal se o AAX estiver assinado com PACE. Se não estiver, testa no Pro Tools Developer.
- B12. Sidechain: no menu *Key Input* da janela do plugin.
- B13. Testa as versões AudioSuite e de inserção, e a compensação de latência.

**Cubase / Nuendo (VST3)**
- B14. Sidechain: o botão de sidechain na janela do plugin activa a entrada; envia outra pista para ela.
- B15. Testa o *Render in place*.

**Studio One (VST3 / AU)**
- B16. Sidechain: no topo da janela do plugin; escolhe a origem na pista.

**REAPER (VST3 / AU)**
- B17. Sidechain: põe a pista com 4 canais e envia a outra pista para 3/4; o pin do sidechain fica automático.
- B18. Teste de *Render* com o modo "Full-speed offline".

## C. Registo

| Data | Testador | DAW e versão | Sistema / computador | Formato | Falhas (número do teste + nota) |
|---|---|---|---|---|---|
| | | | | | |

Envia os resultados com capturas e, se possível, o projecto para **juninhopiradex@hotmail.com**.
