#!/usr/bin/env python3
"""Cria um pack DELEX Signature Series (.dlxpack) a partir de uma pasta de presets.

Uso:
  python3 tools/make_pack.py PASTA_DOS_PRESETS saida.dlxpack --nome "Nome do Pack" --autor "Autor" \
      [--desc-pt "..."] [--desc-en "..."] [--capa capa.png] [--info presets.json]

  presets.json (opcional): { "Nome do preset": { "desc_pt": "...", "desc_en": "...", "tags": "..." } }

O formato (pack.json + presets/*.dlxpreset + cover.png) é o que o DELEX importa em
Presets > IMPORTAR PACK. Cada preset leva o seu SHA-256: um pack alterado é recusado.
"""
import argparse, hashlib, json, pathlib, sys, zipfile

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pasta"); ap.add_argument("saida")
    ap.add_argument("--nome", required=True); ap.add_argument("--autor", default="")
    ap.add_argument("--desc-pt", default=""); ap.add_argument("--desc-en", default="")
    ap.add_argument("--capa"); ap.add_argument("--info")
    a = ap.parse_args()
    pasta = pathlib.Path(a.pasta)
    presets = sorted(pasta.glob("*.dlxpreset"))
    if not presets:
        sys.exit("Nenhum .dlxpreset em " + str(pasta))
    info = json.loads(pathlib.Path(a.info).read_text(encoding="utf-8")) if a.info else {}
    manifest = {"format": "dlxpack", "version": 1, "name": a.nome, "author": a.autor,
                "description_pt": a.desc_pt, "description_en": a.desc_en, "presets": []}
    with zipfile.ZipFile(a.saida, "w", zipfile.ZIP_DEFLATED) as z:
        for p in presets:
            data = p.read_bytes()
            entry = "presets/" + p.name
            z.writestr(entry, data)
            item = {"file": entry, "sha256": hashlib.sha256(data).hexdigest()}
            item.update(info.get(p.stem, {}))
            manifest["presets"].append(item)
        if a.capa:
            z.write(a.capa, "cover.png")
        z.writestr("pack.json", json.dumps(manifest, ensure_ascii=False, indent=2))
    print(f"{a.saida}: {len(presets)} presets")

if __name__ == "__main__":
    main()
