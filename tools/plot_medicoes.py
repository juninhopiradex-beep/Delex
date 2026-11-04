#!/usr/bin/env python3
"""Gera as curvas das medições do DELEX (docs/medicoes/*.png) e as páginas
docs/MEDICOES.md (PT) e docs/MEASUREMENTS.md (EN) a partir dos CSV de tests/measure.cpp.

  g++ -O2 -std=c++17 -ISource tests/measure.cpp -o measure && ./measure docs/medicoes/data
  python3 tools/plot_medicoes.py
"""
import csv, pathlib
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = pathlib.Path(__file__).resolve().parent.parent
DATA = ROOT / "docs/medicoes/data"
OUT = ROOT / "docs/medicoes"
SERIES = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4"]   # ordem fixa (paleta categórica)
INK, MUTED, GRID = "#1f1f1e", "#6b6a66", "#e6e5e1"

plt.rcParams.update({
    "font.family": "DejaVu Sans", "font.size": 10, "axes.edgecolor": GRID, "axes.labelcolor": MUTED,
    "xtick.color": MUTED, "ytick.color": MUTED, "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.8,
    "axes.spines.top": False, "axes.spines.right": False, "legend.frameon": False, "figure.dpi": 160,
    "lines.linewidth": 2.0, "axes.titleweight": "bold", "axes.titlesize": 12, "axes.titlecolor": INK,
})

def read(name):
    with open(DATA / name, encoding="utf-8") as f:
        r = list(csv.reader(f))
    head, rows = r[0], [[float(x) for x in row] for row in r[1:] if row]
    return head, list(zip(*rows))

def save(fig, name):
    fig.tight_layout()
    fig.savefig(OUT / name)
    plt.close(fig)

def summary():
    out = {}
    for line in (DATA / "resumo.txt").read_text(encoding="utf-8").splitlines():
        k, v, u = line.split("=")
        out[k] = (float(v), u)
    return out

def plots(lang):
    pt = lang == "pt"
    t = (lambda a, b: a if pt else b)
    sfx = "" if pt else "_en"
    names_en = {"Fita": "Tape", "Multi-cabeca": "Multi-head", "Cassete": "Cassette"}
    nm = lambda s: (s.replace("Multi-cabeca", "Multi-cabeça") if pt else names_en.get(s, s))

    # 1) cauda por modelo
    h, c = read("cauda_modelos.csv")
    fig, ax = plt.subplots(figsize=(7.5, 3.6))
    for i, name in enumerate(h[1:]):
        ax.plot(c[0], c[i + 1], color=SERIES[i], marker="o", markersize=4, label=nm(name))
    ax.set_xlabel(t("repetição", "repeat")); ax.set_ylabel("dB")
    ax.set_title(t("Nível de cada repetição por modelo (feedback 50 %)", "Level of each repeat per model (feedback 50 %)"), loc="left")
    ax.legend(ncol=5, loc="lower left", fontsize=9)
    save(fig, f"cauda_modelos{sfx}.png")

    # 2) resposta das repetições
    h, c = read("resposta_repeticoes.csv")
    fig, ax = plt.subplots(figsize=(7.5, 3.6))
    labels = [t("1.ª repetição", "1st repeat"), t("3.ª repetição", "3rd repeat"), t("6.ª repetição", "6th repeat")]
    import math
    ref = None
    for i in range(3):
        ys = list(c[i + 1])
        k1 = min(range(len(c[0])), key=lambda k: abs(c[0][k] - 1000))
        ys = [y - ys[k1] for y in ys]
        ax.semilogx(c[0], ys, color=SERIES[i], label=labels[i])
        ax.annotate(labels[i], (c[0][-1], ys[-1]), xytext=(4, 0), textcoords="offset points", color=INK, fontsize=8, va="center")
    ax.set_xlim(20, 20000); ax.set_ylim(-36, 6)
    ax.set_xlabel("Hz"); ax.set_ylabel(t("dB (0 = 1 kHz)", "dB (0 = 1 kHz)"))
    ax.set_title(t("Resposta das repetições (cortes 60 Hz e 14 kHz)", "Response of the repeats (60 Hz and 14 kHz cuts)"), loc="left")
    ax.legend(loc="lower center", fontsize=9)
    save(fig, f"resposta_repeticoes{sfx}.png")

    # 3) resposta por modelo
    h, c = read("resposta_modelos.csv")
    fig, ax = plt.subplots(figsize=(7.5, 3.6))
    for i, name in enumerate(h[1:]):
        ax.semilogx(c[0], c[i + 1], color=SERIES[i], label=nm(name))
    ax.set_xlim(20, 20000); ax.set_ylim(-30, 6)
    ax.set_xlabel("Hz"); ax.set_ylabel(t("dB (0 = 1 kHz)", "dB (0 = 1 kHz)"))
    ax.set_title(t("1.ª repetição por modelo (eco de 500 ms)", "1st repeat per model (500 ms echo)"), loc="left")
    ax.legend(ncol=5, loc="lower left", fontsize=9)
    save(fig, f"resposta_modelos{sfx}.png")

    # 4) THD vs drive + harmónicos
    h, c = read("thd_drive.csv")
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(9, 3.4))
    a1.plot(c[0], c[1], color=SERIES[0], marker="o", markersize=4, label="Normal")
    a1.plot(c[0], c[2], color=SERIES[1], marker="o", markersize=4, label="HQ 2x")
    a1.set_xlabel("DRIVE (%)"); a1.set_ylabel("THD (%)")
    a1.set_title(t("THD da fita (1 kHz, −6 dBFS)", "Tape THD (1 kHz, −6 dBFS)"), loc="left"); a1.legend(fontsize=9)
    h2, c2 = read("harmonicos.csv")
    a2.bar([int(x) for x in c2[0]], [max(v, -120) + 120 for v in c2[1]], bottom=-120, color=SERIES[0], width=0.6)
    a2.set_ylim(-120, 5); a2.set_xticks(range(1, 10))
    a2.set_xlabel(t("harmónico", "harmonic")); a2.set_ylabel("dB")
    a2.set_title(t("Harmónicos a DRIVE 60 %", "Harmonics at DRIVE 60 %"), loc="left")
    save(fig, f"thd{sfx}.png")

    # 5) aliasing
    h, c = read("aliasing.csv")
    fig, ax = plt.subplots(figsize=(7.5, 3.4))
    ax.plot(c[0], c[1], color=SERIES[1], linewidth=1.0, label=t("Normal (ADAA)", "Normal (ADAA)"))
    ax.plot(c[0], c[2], color=SERIES[0], linewidth=1.0, label="HQ 2x")
    ax.set_xlim(0, 24000); ax.set_ylim(-160, 5)
    ax.set_xlabel("Hz"); ax.set_ylabel("dB")
    ax.set_title(t("Aliasing: seno de 9,1 kHz com DRIVE 80 %", "Aliasing: 9.1 kHz sine at DRIVE 80 %"), loc="left")
    ax.legend(fontsize=9, loc="upper right")
    save(fig, f"aliasing{sfx}.png")

    # 6) ducker
    h, c = read("ducker.csv")
    fig, ax = plt.subplots(figsize=(5.5, 3.4))
    ax.plot(c[0], [-v for v in c[1]], color=SERIES[0])
    ax.axvline(-26, color=MUTED, linewidth=1, linestyle="--")
    ax.annotate("THRESHOLD −26 dB", (-26, -1), xytext=(4, 0), textcoords="offset points", color=MUTED, fontsize=8)
    ax.set_xlabel(t("nível da fonte (dBFS)", "source level (dBFS)")); ax.set_ylabel(t("ganho dos ecos (dB)", "echo gain (dB)"))
    ax.set_title(t("Curva do ducker (AMOUNT 9 dB, joelho 6 dB)", "Ducker curve (AMOUNT 9 dB, 6 dB knee)"), loc="left")
    save(fig, f"ducker{sfx}.png")

    # 7) CPU
    with open(DATA / "cpu.csv", encoding="utf-8") as f:
        rows = list(csv.reader(f))[1:]
    tr = {"Base (delay + matriz + EQ)": "Base (delay + matrix + EQ)", "Saturação de fita": "Tape saturation", "Saturação HQ 2x": "HQ 2x saturation",
          "Difusão + dispersão": "Diffusion + dispersion", "16 taps (4 harmonizados)": "16 taps (4 harmonised)", "Tudo ligado": "Everything on"}
    fig, ax = plt.subplots(figsize=(7.5, 3.6))
    labels = [r[0] if pt else tr.get(r[0], r[0]) for r in rows]
    vals = [float(r[1]) for r in rows]
    ax.barh(labels[::-1], vals[::-1], color=SERIES[0], height=0.6)
    for y, v in enumerate(vals[::-1]):
        ax.annotate(f"{v:.1f} %", (v, y), xytext=(4, 0), textcoords="offset points", va="center", fontsize=8, color=INK)
    ax.set_xlabel(t("% de um núcleo (48 kHz estéreo)", "% of one core (48 kHz stereo)"))
    ax.set_title(t("Custo de CPU por função", "CPU cost per feature"), loc="left")
    ax.grid(axis="y", visible=False)
    save(fig, f"cpu{sfx}.png")

def page(lang):
    pt = lang == "pt"
    t = (lambda a, b: a if pt else b)
    sfx = "" if pt else "_en"
    s = summary()
    keys_en = {
        "Fita: queda por repetição (feedback 50 %)": "Tape: drop per repeat (feedback 50 %)",
        "1.ª repetição: 60 Hz em relação a 1 kHz": "1st repeat: 60 Hz relative to 1 kHz",
        "1.ª repetição: 14 kHz em relação a 1 kHz": "1st repeat: 14 kHz relative to 1 kHz",
        "6.ª repetição: 10 kHz em relação a 1 kHz": "6th repeat: 10 kHz relative to 1 kHz",
        "THD a DRIVE 60 % (1 kHz, −6 dBFS)": "THD at DRIVE 60 % (1 kHz, −6 dBFS)",
        "Alias a 2,5 kHz (normal)": "Alias at 2.5 kHz (normal)", "Alias a 2,5 kHz (HQ 2x)": "Alias at 2.5 kHz (HQ 2x)",
        "Ruído de fundo (silêncio, feedback 90 %)": "Noise floor (silence, feedback 90 %)", "HISS no máximo": "HISS at maximum",
        "Erro máximo do tempo do eco (1 ms … 8 s)": "Max echo time error (1 ms … 8 s)",
        "Soma mono dos ecos em contrafase, sem GUARD": "Mono sum of out-of-phase echoes, GUARD off",
        "Soma mono dos ecos em contrafase, com GUARD": "Mono sum of out-of-phase echoes, GUARD on",
        "CPU base (um núcleo, 48 kHz estéreo)": "Base CPU (one core, 48 kHz stereo)", "CPU com tudo ligado": "CPU with everything on",
        "Latência do plugin": "Plugin latency" }
    def fmt(v, u):
        if v <= -200: return t("silêncio digital", "digital silence")
        if u == "amostras": return f"{v:.0f} " + t("amostras", "samples")
        return f"{v:.1f} {u}"
    lines = [t("# DELEX by Piradex — medições", "# DELEX by Piradex — measurements"), "",
             t("Medições objectivas do motor do DELEX 1.0, feitas com a metodologia do **ANALYSER by Piradex**: impulso e ruído curto para a resposta de cada repetição, senos para THD, harmónicos e aliasing, varrimento de nível para o ducker, silêncio para o ruído de fundo. Tudo a 48 kHz, em precisão simples, como no plugin. Os números são gerados por `tests/measure.cpp` e as curvas por `tools/plot_medicoes.py` — qualquer pessoa pode repetir.",
               "Objective measurements of the DELEX 1.0 engine, following the **ANALYSER by Piradex** methodology: impulse and short noise for each repeat's response, sines for THD, harmonics and aliasing, a level sweep for the ducker, silence for the noise floor. All at 48 kHz, single precision, as in the plugin. The numbers come from `tests/measure.cpp` and the curves from `tools/plot_medicoes.py` — anyone can repeat them."), "",
             t("## Resumo", "## Summary"), "", t("| Medição | Resultado |", "| Measurement | Result |"), "|---|---|"]
    for k, (v, u) in s.items():
        lines.append(f"| {k if pt else keys_en.get(k, k)} | {fmt(v, u)} |")
    blocks = [
        ("cauda_modelos", t("Cauda de ecos por modelo", "Echo tail per model"),
         t("Com feedback 50 % cada repetição perde 6 dB (a matemática pede 20·log10(0,5) = −6,02 dB): o FEEDBACK faz exactamente o que diz. A Cassete perde um pouco mais porque a banda estreita e a saturação actuam a cada volta.",
           "At 50 % feedback each repeat drops 6 dB (the maths says 20·log10(0.5) = −6.02 dB): FEEDBACK does exactly what it says. Cassette loses a little more because its narrow band and saturation act on every pass.")),
        ("resposta_repeticoes", t("Resposta das repetições", "Response of the repeats"),
         t("Low cut 60 Hz e high cut 14 kHz (12 dB/oitava) dentro do loop. Os cortes somam-se a cada volta: as repetições escurecem e perdem graves de forma progressiva, como num delay analógico bem regulado.",
           "Low cut 60 Hz and high cut 14 kHz (12 dB/octave) inside the loop. The cuts add up on every pass: repeats get progressively darker and lighter in the lows, like a well-tuned analog delay.")),
        ("resposta_modelos", t("Carácter dos modelos", "Character of the models"),
         t("Primeira repetição de cada modelo, normalizada a 1 kHz.", "First repeat of each model, normalised at 1 kHz.")),
        ("thd", t("Saturação de fita", "Tape saturation"),
         t("A distorção cresce de forma suave com o DRIVE e é dominada pelos harmónicos ímpares baixos (carácter de fita). Com DRIVE a 0 % o loop é linear.",
           "Distortion rises smoothly with DRIVE and is dominated by low odd harmonics (tape character). At 0 % DRIVE the loop is linear.")),
        ("aliasing", "Aliasing",
         t("Mesmo sem oversampling a saturação usa anti-aliasing por derivada (ADAA). O HQ 2x leva o aliasing para baixo do ruído de fundo.",
           "Even without oversampling the saturation uses antiderivative anti-aliasing (ADAA). HQ 2x pushes aliasing below the noise floor.")),
        ("ducker", "Ducker", t("Redução em função do nível da fonte, com joelho suave de 6 dB a partir do THRESHOLD.", "Reduction versus source level, with a soft 6 dB knee from the THRESHOLD.")),
        ("cpu", "CPU", t("Medido num núcleo moderno; um MacBook Pro 2013 gasta cerca do dobro, ainda folgado.", "Measured on a modern core; a 2013 MacBook Pro uses roughly twice as much, still comfortable.")),
    ]
    for f, title, text in blocks:
        lines += ["", f"## {title}", "", text, "", f"![{title}](medicoes/{f}{sfx}.png)"]
    lines += ["", t("Repetir: `g++ -O2 -std=c++17 -ISource tests/measure.cpp -o measure && ./measure docs/medicoes/data && python3 tools/plot_medicoes.py`",
                   "Repeat: `g++ -O2 -std=c++17 -ISource tests/measure.cpp -o measure && ./measure docs/medicoes/data && python3 tools/plot_medicoes.py`"), ""]
    (ROOT / "docs" / ("MEDICOES.md" if pt else "MEASUREMENTS.md")).write_text("\n".join(lines), encoding="utf-8")

for lang in ("pt", "en"):
    plots(lang)
    page(lang)
print("ok")
