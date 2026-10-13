# DELEX by Piradex — Arquitetura, algoritmos e testes

## 1. Organização do código

| Ficheiro | Papel |
|---|---|
| `Source/DelexCore.h` | Motor DSP completo, **sem JUCE** (testável com `g++`). |
| `Source/Params.h` | 57 parâmetros com IDs estáveis, leitura atómica, macros e conversão para o motor. |
| `Source/Presets.h` | 61 presets de fábrica e 9 estilos, em formato `id=valor;…` legível. |
| `Source/PluginProcessor.*` | Ponte JUCE: buses, BPM do host, medidores, estado, presets, A/B, tap. |
| `Source/Look.h`, `Widgets.h`, `PresetBrowser.h`, `PluginEditor.*` | Interface (LookAndFeel próprio, knobs, ECHO FIELD, goniómetro, EQ interactivo, browser). |
| `Source/License.*`, `Source/Login.h` | Acesso por senha offline: só salt + hash PBKDF2-HMAC-SHA256 (60 000 iterações) no binário; verificação numa thread de fundo; token por computador. Novas senhas beta: `python3 tools/nova_senha_beta.py AAAA-MM-DD`. |
| `tests/test_core.cpp` | 22 verificações numéricas do motor. |
| `tests/test_plugin.cpp` | Plugin real: presets, estado, A/B, layouts, precisão dupla, capturas da interface, matriz de parâmetros. |

## 2. Fluxo do sinal

```
entrada ─► largura/rotação de entrada ─► pré-drive ─┐
                                                    ▼
      ┌──────────── (+) ◄── feedback × r[n] ◄──────────────────────┐
      ▼                                                             │
 cadeia do loop (ordem configurável):                               │
 Digital Grit · Feedback EQ · Dispersão · Difusão · Fita            │
      ▼                                                             │
 bloqueio DC + limitador suave                                      │
      ▼                                                             │
 linhas de atraso L/R (Hermite, modulação) ─► matriz 2×2 unitária ─►┘ r[n]
                                                    │
 r[n] ─► ducker ─► rotação/largura de saída ─► wet trim ─► mistura dry/wet ─► bypass suave ─► saída
```

O caminho seco não tem latência; o plugin reporta latência zero.

## 3. Algoritmos e justificação

- **Linhas de atraso** — buffer potência de 2 por canal (8 s + margem à taxa actual), leitura com interpolação **Hermite de 4 pontos** (boa resposta em agudos, barata). Tempos 1 ms – 8 s.
- **Transições de tempo** — *Fita*: o tempo desliza por um filtro de 1.ª ordem (GLIDE), como uma cabeça de fita, mudando a afinação. *Crossfade*: duas cabeças de leitura e transição de 45 ms em cosseno, sem mudar a afinação.
- **Matriz espacial** — rotação `[[c,−s],[s,c]]` ou reflexão `[[c,s],[s,−c]]`, ambas ortogonais (determinante ±1): **não acrescentam energia**, por isso a estabilidade do loop depende só do ganho do feedback e dos filtros. Ping-pong = reflexão a 90° (troca L/R) com entrada somada num lado; Mono = identidade com entrada somada.
- **EQ do loop** — filtros de estado variável topológicos (Simper/TPT), estáveis sob modulação de parâmetros: 6 dB (1.ª ordem), 12 dB (Butterworth) e 24 dB (dois Butterworth em cascata). Pico em sino; **COMPENSATE** reduz o feedback em `ganho_pico × compensate` dB.
- **Dispersão** — 8 passa-tudo de 2.ª ordem na mesma frequência; o Q é calculado para que o atraso de grupo no centro seja o SPREAD pedido (`τ = N·2Q/(π·f0)`).
- **Difusão** — 4 passa-tudo de Schroeder por canal (g = 0,62) com tempos em proporções não múltiplas, misturados em paralelo pelo AMOUNT; o atraso médio introduzido é compensado no tempo do eco.
- **Fita** — pré-ênfase de agudos (shelf de 1.ª ordem) → `tanh` com **anti-aliasing por antiderivada (ADAA)** → de-ênfase **exactamente inversa** (sem drive, warmth é transparente: verificado a 1e-7). Ganho de pequeno sinal = 1 (o loop não ganha energia); a compensação de nível é aplicada só ao sinal molhado, fora do loop. Escolheu-se ADAA em vez de oversampling porque oversampling por blocos dentro de um loop de feedback acrescentaria latência ao próprio loop.
- **Digital grit** — redução de taxa por amostragem-e-retenção com filtro anti-aliasing a 0,45 × taxa alvo, jitter do relógio, quantização de bits, mistura paralela.
- **Modulação** — LFO seno/triângulo/aleatório suave; a profundidade em cents é convertida na amplitude de modulação do tempo (`A = (1 − 2^(−c/1200)) / (2π·f)` para seno). Wow (0,55 Hz) + flutter (6,3 Hz) + componente aleatória.
- **Ducker** — detector de pico na entrada seca, joelho de 6 dB, ataque/hold/release, aplicado só ao molhado.
- **Segurança** — bloqueio DC no loop, limitador suave (linear até 0 dBFS, satura até +6 dBFS), zeros exactos abaixo de 1e-20 (sem denormais), verificação NaN/Inf com reinício de emergência. `processBlock` sem alocações, locks nem I/O.
- **Macros** — SPACE (largura de saída ×(1+0,8·s), difusão, +30°·s de rotação em Dual), TONE (high cut ×2^(3,3·t) para t<0; low cut ×2^(3t) e high cut ×2^(0,8t) para t>0), CHARACTER (drive/warmth/wow para c>0; bits até 6 e sample rate até ÷5,7 para c<0).

## 4. Testes (resultados desta versão)

Motor (`delex_core_test`), 22 verificações:
- matriz unitária (erro 1,2e-7) · warmth transparente sem drive · tempos exactos (100 ms → amostra 4800) · ping-pong L→R
- feedback 100 % + pico +18 dB sem compensação + drive/grit/difusão/dispersão no máximo, 20 s: pico 0,39, sem NaN
- freeze mantém a energia (razão 1,01 após 8 s) e decai ao sair · NaN/Inf na entrada recupera · ducker reduz 12,00 dB pedidos
- mudança de tempo sem cliques (fita e crossfade) · mix 0 e bypass bit-exactos · silêncio após 60 s · fuzz em 44,1/48/96/192 kHz com parâmetros e blocos aleatórios
- CPU: ≈3 % de um núcleo (48 kHz, todos os módulos activos)

Plugin (`delex_plugin_test`): 61 presets com áudio finito e limitado · estado guardar/restaurar · A/B · layouts mono→estéreo / recusa estéreo→mono · precisão dupla a 96 kHz · 1/4 a 120 BPM = 500 ms · redimensionamento · capturas em `docs/`.

CI (GitHub Actions): testes do motor, build universal arm64 + x86_64 com macOS mínimo 10.13 verificado por `otool`, **auval** nativo e em Rosetta (Intel), **pluginval** nível 5 em macOS e Windows, verificação de que o VST3 Windows não depende do VC++ Redistributable, instaladores `.pkg/.dmg` e Inno Setup.

## 5. Limitações conhecidas

- Sidechain externo do ducker ainda não implementado (detector interno).
- Ordem do loop escolhida entre 4 combinações (não arrastável livremente).
- Sem AAX; sem assinatura/notarização Apple paga (instalação pede "Abrir" na primeira vez).
- Bloqueio de parâmetros entre presets e hiss de fita ainda não implementados.

## 6. Roadmap

1. Sidechain externo no ducker (bus auxiliar VST3/AU).
2. Ordem do loop arrastável e bloqueio de parâmetros.
3. Notarização Apple e licenças por cliente (ligadas à loja).
4. Hiss de fita opcional e modo de oversampling ×2 para o caminho molhado.
5. Standalone para teste rápido.
