# DELEX by Piradex — v1.0 BETA

Analog Delay Engine — delay estéreo completo.

## Funcionalidades
- Delay Core: Time, Feedback, Mix, Spread
- Tap Tempo (toca várias vezes para definir o ritmo)
- Modulation: Rate, Depth, forma de onda Sine/Tri
- Filter: Hi Cut, Lo Cut
- Analog: Drive (saturação analógica), Lofi
- Delay EQ: Low, Mid, High
- Ping Pong on/off
- 6 presets de fábrica
- VU meter colorido (verde/amarelo/vermelho)
- Botão "i" com info do criador
- Ativação por chave serial (beta)

## Design
Layout em cartões com VU meter de barras coloridas no topo.

## Chaves beta (20)
```
DLX-HWGQ-496R-AFHU    DLX-F4LJ-V42K-N2AG
DLX-G6A9-WP3S-YY2E    DLX-2EDY-YBB8-WV6W
DLX-XFUU-7KVB-ZZ75    DLX-H97L-KP5X-7QE3
DLX-F39H-593S-48BQ    DLX-CNMC-D6N8-BJPJ
DLX-JDG5-8K8Q-H78S    DLX-LM2T-54F2-VMYM
DLX-3V83-5NUT-ACK7    DLX-S794-U7MW-86HT
DLX-9ZNS-9795-CUW7    DLX-GPQA-F7X4-MBYL
DLX-G6S6-M58E-T9ZG    DLX-YPHT-PUTJ-GSG5
DLX-N4K6-AFA6-FNRH    DLX-LGDY-MGWT-EUWL
DLX-CTYK-AW9J-Q8JU    DLX-SSEA-4S4A-DGRB
```

## Como subir ao GitHub
Substitui os ficheiros no repositório delex-piradex e corre o build no Codemagic.

## Instalar (macOS Intel)
```
sudo xattr -rd com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Delex.vst3"
sudo xattr -rd com.apple.quarantine "/Library/Audio/Plug-Ins/Components/Delex.component"
```
