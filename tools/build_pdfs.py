#!/usr/bin/env python3
"""Gera os PDFs da documentação (docs/pdf): manual PT e EN, curso e medições.
Usa pandoc (markdown → HTML) e o Chromium do Playwright (HTML → PDF A4)."""
import pathlib, subprocess, tempfile
from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
CSS = """
@page { size: A4; margin: 16mm 15mm 18mm 15mm; }
body { font-family: 'DejaVu Sans', Arial, sans-serif; font-size: 10.2pt; line-height: 1.45; color: #1f1f1e; }
h1 { font-size: 22pt; color: #e8603c; margin: 0 0 6pt; letter-spacing: .5pt; }
h2 { font-size: 14pt; border-bottom: 1.5pt solid #e8603c; padding-bottom: 3pt; margin-top: 18pt; page-break-after: avoid; }
h3 { font-size: 11.5pt; color: #2a78d6; margin-top: 12pt; page-break-after: avoid; }
table { border-collapse: collapse; width: 100%; margin: 6pt 0; font-size: 9.2pt; page-break-inside: avoid; }
th, td { border: 0.6pt solid #d8d6d0; padding: 3pt 5pt; text-align: left; vertical-align: top; }
th { background: #f3f1ec; }
img { max-width: 100%; display: block; margin: 6pt auto; page-break-inside: avoid; border-radius: 4pt; }
code { font-family: 'DejaVu Sans Mono', monospace; font-size: 8.8pt; background: #f3f1ec; padding: 0 2pt; }
hr { border: 0; border-top: 0.6pt solid #d8d6d0; margin: 10pt 0; }
a { color: #2a78d6; text-decoration: none; }
"""
FILES = [("MANUAL.md", "DELEX-Manual-PT.pdf", "DELEX — Manual"), ("MANUAL_EN.md", "DELEX-Manual-EN.pdf", "DELEX — Manual (EN)"),
         ("CURSO.md", "DELEX-Curso.pdf", "Curso DELEX"), ("MEDICOES.md", "DELEX-Medicoes.pdf", "DELEX — Medições"),
         ("MEASUREMENTS.md", "DELEX-Measurements.pdf", "DELEX — Measurements")]

def footer(title):
    return f'<div style="font-size:7pt;color:#8a8984;width:100%;padding:0 15mm;display:flex;justify-content:space-between;font-family:DejaVu Sans"><span>{title} · DELEX by Piradex 1.0.0 · www.beatfreakstudio.com</span><span><span class="pageNumber"></span>/<span class="totalPages"></span></span></div>'

with sync_playwright() as p:
    br = p.chromium.launch()
    for src, out, title in FILES:
        html = subprocess.run(["pandoc", str(DOCS / src), "-f", "gfm", "-t", "html5", "--standalone", "--metadata", f"title={title}"],
                              capture_output=True, text=True, check=True).stdout
        html = html.replace("</head>", f"<style>{CSS}</style></head>").replace('<header id="title-block-header">', '<header style="display:none">')
        tmp = DOCS / f"_tmp_{src}.html"
        tmp.write_text(html, encoding="utf-8")
        page = br.new_page()
        page.goto(tmp.as_uri()); page.wait_for_load_state("networkidle")
        page.pdf(path=str(DOCS / "pdf" / out), format="A4", print_background=True, display_header_footer=True,
                 header_template="<div></div>", footer_template=footer(title), margin={"top": "16mm", "bottom": "18mm", "left": "15mm", "right": "15mm"})
        page.close(); tmp.unlink()
        print(out)
    br.close()
