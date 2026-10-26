#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Estilo "Señal" del manual de PV Suite (Nestor Palomeque).

Toma los manuales doc/manual/Manual_PvSuite*.html y les aplica la identidad visual de PV Suite:
  - portada con trazas de tendencia (como las de un registrador) y etiquetas de tags;
  - indice en forma de mapa (una tarjeta por capitulo, con sus secciones);
  - cada capitulo abre con su propia traza, generada a partir del titulo (siempre la misma);
  - pagina con nombre por capitulo: la solapa del margen derecho y el encabezado cambian de capitulo
    (necesita un navegador basado en Chromium 131 o mas nuevo para el PDF: Edge o Chrome).
Se puede ejecutar las veces que haga falta: reconoce lo que ya genero (marcas <!--[...]-->) y lo rehace.

Uso:  python estilo_manual.py [archivo.html ...]      (sin argumentos: todos los Manual_PvSuite*.html de ../manual)
El contenido (texto de cada capitulo) se edita en el HTML; este script solo cambia la presentacion.
"""
import glob
import hashlib
import html
import math
import os
import random
import re
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))

# colores
TINTA = "#0d1b2a"
TINTA2 = "#16283b"
CIAN = "#00b4c4"
AMBAR = "#ffb000"
PAPEL = "#fbfaf6"
GRIS = "#5b6b7a"

TXT = {
    "es": dict(manual="Manual de usuario", indice="Índice", nota="Nota", version="Versión",
               lema="SCADA/HMI libre basado en pvbrowser", seccion="Secciones", licencia="Software libre · GPL / LGPL",
               tags=["PLC1:Temperatura", "PLC1:Nivel", "PLC1:Estado.3", "PLC1:Bomba", "SYS.Ciclo"]),
    "en": dict(manual="User manual", indice="Contents", nota="Note", version="Version",
               lema="Free SCADA/HMI based on pvbrowser", seccion="Sections", licencia="Free software · GPL / LGPL",
               tags=["PLC1:Temperature", "PLC1:Level", "PLC1:Status.3", "PLC1:Pump", "SYS.Cycle"]),
    "pt": dict(manual="Manual do usuário", indice="Índice", nota="Nota", version="Versão",
               lema="SCADA/HMI livre baseado no pvbrowser", seccion="Seções", licencia="Software livre · GPL / LGPL",
               tags=["PLC1:Temperatura", "PLC1:Nivel", "PLC1:Estado.3", "PLC1:Bomba", "SYS.Ciclo"]),
    "de": dict(manual="Benutzerhandbuch", indice="Inhalt", nota="Hinweis", version="Version",
               lema="Freies SCADA/HMI auf Basis von pvbrowser", seccion="Abschnitte", licencia="Freie Software · GPL / LGPL",
               tags=["PLC1:Temperatur", "PLC1:Pegel", "PLC1:Status.3", "PLC1:Pumpe", "SYS.Zyklus"]),
}

CSS = r"""
:root { --tinta:%(TINTA)s; --tinta2:%(TINTA2)s; --cian:%(CIAN)s; --ambar:%(AMBAR)s; --papel:%(PAPEL)s; --gris:%(GRIS)s; }
* { -webkit-print-color-adjust: exact; print-color-adjust: exact; box-sizing: border-box; }
@page { size: A4; margin: 24mm 18mm 22mm 20mm; }
@page portada { margin: 0; }
html { background: var(--papel); }
body { margin: 0; color: #1b2733; background: var(--papel);
  font-family: "Segoe UI", "Helvetica Neue", Arial, sans-serif; font-size: 10.2pt; line-height: 1.5; }
h1, h2, h3, .num, .etq, th { font-family: Bahnschrift, "DIN Alternate", "Roboto Condensed", "Arial Narrow", "Segoe UI", sans-serif; }
code, pre, .kbd, .mono { font-family: "Cascadia Mono", Consolas, "DejaVu Sans Mono", "Courier New", monospace; }
a { color: #006f7c; text-decoration: none; }

/* ---------- portada ---------- */
.portada { page: portada; position: relative; width: 210mm; height: 297mm; overflow: hidden; background: var(--tinta);
  color: #fff; break-after: page; }
.portada svg.fondo { position: absolute; inset: 0; width: 100%%; height: 100%%; }
.portada .txt { position: absolute; left: 22mm; right: 22mm; top: 26mm; }
.portada .marca { font-family: "Cascadia Mono", Consolas, monospace; font-size: 9pt; letter-spacing: .28em; color: var(--cian); text-transform: uppercase; }
.portada h1.titulo { font-size: 78pt; line-height: 1; margin: 7mm 0 0; border: 0; padding: 0; color: #fff; letter-spacing: -.01em; display: block; }
.portada h1.titulo b { color: var(--ambar); font-weight: 700; }
.portada .sub { font-family: Bahnschrift, "Segoe UI", sans-serif; font-size: 22pt; margin-top: 4mm; color: #dfeaf2; font-weight: 300; }
.portada .lema { font-size: 11pt; margin-top: 3mm; color: #9fb4c4; }
.portada .pie { position: absolute; left: 22mm; right: 22mm; bottom: 20mm; display: flex; justify-content: space-between;
  align-items: flex-end; font-size: 9.5pt; color: #9fb4c4; border-top: 1px solid #2a4157; padding-top: 4mm; }
.portada .pie b { color: #fff; font-weight: 600; }
.portada .pie .mono { color: var(--cian); }

/* ---------- indice ---------- */
.indice { page: indice; break-after: page; }
.indice h1 { margin-top: 0; }
.mapa { display: grid; grid-template-columns: 1fr 1fr; gap: 4mm 6mm; margin-top: 6mm; }
.tarjeta { break-inside: avoid; border-top: 3px solid var(--tinta); background: #fff; padding: 2.5mm 3.5mm 3mm; border-left: 1px solid #dde3e8; border-right: 1px solid #dde3e8; border-bottom: 1px solid #dde3e8; }
.tarjeta:nth-child(4n+2), .tarjeta:nth-child(4n+3) { border-top-color: var(--cian); }
.tarjeta .cab { display: flex; align-items: baseline; gap: 3mm; }
.tarjeta .cab .num { font-size: 20pt; font-weight: 700; color: var(--cian); min-width: 11mm; line-height: 1; }
.tarjeta .cab a { font-family: Bahnschrift, "Segoe UI", sans-serif; font-size: 11.5pt; font-weight: 600; color: var(--tinta); line-height: 1.15; }
.tarjeta ul { list-style: none; margin: 2mm 0 0; padding: 0; font-size: 8pt; line-height: 1.35; color: var(--gris); }
.tarjeta li { margin: 0; padding-left: 3mm; position: relative; }
.tarjeta li::before { content: ""; position: absolute; left: 0; top: .55em; width: 1.4mm; height: 1.4mm; background: var(--ambar); }
.tarjeta li a { color: var(--gris); }

/* ---------- capitulos ---------- */
.cap { break-before: page; }
.cap > .cabecera { margin: 0 0 4mm; height: 26mm; border-bottom: 2px solid var(--tinta); position: relative; }
.cap > .cabecera svg { width: 100%%; height: 100%%; display: block; }
h1 { display: flex; align-items: baseline; gap: 5mm; font-size: 27pt; line-height: 1.1; color: var(--tinta); margin: 0 0 6mm; padding: 0; border: 0; font-weight: 700; }
h1 .n { font-size: 54pt; font-weight: 700; color: transparent; -webkit-text-stroke: 1.3px var(--cian); line-height: .9; min-width: 18mm; }
h2 { font-size: 15pt; color: var(--tinta); margin: 8mm 0 2.5mm; padding-bottom: 1.2mm; border-bottom: 1px solid #cfd8de; break-after: avoid; font-weight: 600; }
h2 .n { color: var(--cian); font-size: 11pt; font-family: "Cascadia Mono", Consolas, monospace; margin-right: 2.5mm; padding: .5mm 1.6mm; border: 1px solid var(--cian); }
h3 { font-size: 11.5pt; color: var(--tinta2); margin: 5mm 0 1mm; break-after: avoid; font-weight: 600; }
h3::before { content: ""; display: inline-block; width: 2.2mm; height: 2.2mm; background: var(--ambar); margin-right: 2mm; transform: translateY(-.1mm); }
p { margin: 0 0 2.6mm; orphans: 3; widows: 3; }
ul, ol { margin: 0 0 3mm; padding-left: 6mm; }
li { margin: .6mm 0; }
li::marker { color: var(--cian); }
code { background: #e5f4f6; color: #0b3f48; padding: 0 1.2mm; border-radius: 1mm; font-size: 9pt; }
pre { background: var(--tinta); color: #e6f0f6; border-left: 1.6mm solid var(--cian); border-radius: 0 1.5mm 1.5mm 0; padding: 3mm 4mm;
  font-size: 8.4pt; line-height: 1.4; white-space: pre-wrap; break-inside: avoid; margin: 3mm 0 4mm; }
pre code { background: none; color: inherit; padding: 0; font-size: inherit; }
table { border-collapse: collapse; width: 100%%; margin: 3mm 0 4.5mm; font-size: 9pt; }
th { background: var(--tinta); color: #fff; text-align: left; font-weight: 600; letter-spacing: .02em; padding: 1.6mm 2.4mm; font-size: 8.6pt; }
td { padding: 1.5mm 2.4mm; vertical-align: top; border-bottom: 1px solid #d9e1e6; }
tr:nth-child(even) td { background: #f2f6f8; }
tr { break-inside: avoid; }
td code { font-size: 8.4pt; }
.kbd { display: inline-block; padding: 0 1.6mm; border: 1px solid #9fb0bc; border-bottom-width: 2px; border-radius: 1.2mm; background: #fff; font-size: 8.4pt; color: var(--tinta); }
.nota { position: relative; margin: 3.5mm 0 4.5mm; padding: 2.6mm 4mm 2.6mm 4mm; background: #fff6df; border-left: 1.6mm solid var(--ambar); break-inside: avoid; }
.nota::before { content: attr(data-etq); display: block; font-family: Bahnschrift, "Segoe UI", sans-serif; text-transform: uppercase; letter-spacing: .14em;
  font-size: 7.4pt; font-weight: 700; color: #8a5d00; margin-bottom: .6mm; }
img { max-width: 100%%; }

/* ---------- pantalla ---------- */
@media screen {
  body { max-width: 940px; margin: 0 auto; padding: 0 16px 40px; }
  .portada { width: 100%%; height: auto; aspect-ratio: 210 / 297; max-height: 92vh; margin: 16px auto 24px; }
  .portada .txt { left: 6%%; right: 6%%; top: 6%%; }
  .portada h1.titulo { font-size: clamp(34px, 9vw, 78px); }
  .portada .sub { font-size: clamp(14px, 3vw, 22px); }
  .portada .pie { left: 6%%; right: 6%%; bottom: 5%%; font-size: clamp(9px, 1.6vw, 12px); }
  .cap { margin-top: 56px; }
  .mapa { grid-template-columns: repeat(auto-fill, minmax(280px, 1fr)); }
  pre { overflow-x: auto; }
  table { display: block; overflow-x: auto; }
}
""" % dict(TINTA=TINTA, TINTA2=TINTA2, CIAN=CIAN, AMBAR=AMBAR, PAPEL=PAPEL, GRIS=GRIS)


def traza(semilla, ancho, alto, color, grosor=1.6, pasos=170, relleno=False):
    """Camino SVG de una tendencia (senoidales, escalones y pulsos) siempre igual para la misma semilla."""
    r = random.Random(semilla)
    f1, f2, f3 = r.uniform(1.0, 2.6), r.uniform(3.0, 6.5), r.uniform(7, 13)
    p1, p2, p3 = r.uniform(0, 6.28), r.uniform(0, 6.28), r.uniform(0, 6.28)
    a1, a2, a3 = r.uniform(.45, .8), r.uniform(.15, .3), r.uniform(.03, .1)
    # tramos en escalon / pulso, como un registrador
    tramos = []
    for _ in range(r.randint(1, 3)):
        c = r.uniform(.15, .85)
        tramos.append((c, c + r.uniform(.05, .14), r.choice([-1, 1]) * r.uniform(.35, .7)))
    pts = []
    for i in range(pasos + 1):
        t = i / pasos
        v = a1 * math.sin(2 * math.pi * f1 * t + p1) + a2 * math.sin(2 * math.pi * f2 * t + p2) + a3 * math.sin(2 * math.pi * f3 * t + p3)
        for (t0, t1, h) in tramos:
            if t0 <= t <= t1:
                v = h
        pts.append((t * ancho, alto / 2 - v * alto * .42))
    d = "M" + " L".join("%.1f %.1f" % p for p in pts)
    s = '<path d="%s" fill="none" stroke="%s" stroke-width="%s" stroke-linejoin="round" stroke-linecap="round"/>' % (d, color, grosor)
    if relleno:
        s = '<path d="%s L%.1f %.1f L0 %.1f Z" fill="%s" fill-opacity=".10" stroke="none"/>' % (d, ancho, alto, alto, color) + s
    return s, pts[-1]


def semilla(texto):
    return int(hashlib.sha1(texto.encode("utf-8")).hexdigest()[:8], 16)


def svg_portada(tags):
    """Fondo de la portada: cuadricula, trazas de tendencia y etiquetas de tags al final de cada una."""
    w, h = 210, 297
    o = ['<svg class="fondo" viewBox="0 0 %d %d" preserveAspectRatio="xMidYMid slice" xmlns="http://www.w3.org/2000/svg">' % (w, h)]
    o.append('<defs><linearGradient id="vel" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="%s" stop-opacity="0"/>'
             '<stop offset="1" stop-color="%s" stop-opacity=".95"/></linearGradient></defs>' % (TINTA, TINTA))
    o.append('<rect width="%d" height="%d" fill="%s"/>' % (w, h, TINTA))
    for x in range(0, w + 1, 10):
        o.append('<line x1="%d" y1="0" x2="%d" y2="%d" stroke="#fff" stroke-opacity="%s" stroke-width=".15"/>' % (x, x, h, ".10" if x % 50 == 0 else ".045"))
    for y in range(0, h + 1, 10):
        o.append('<line x1="0" y1="%d" x2="%d" y2="%d" stroke="#fff" stroke-opacity="%s" stroke-width=".15"/>' % (y, w, y, ".10" if y % 50 == 0 else ".045"))
    colores = [CIAN, AMBAR, "#7be0c3", "#8fb4ff", CIAN]
    y0 = 118
    for i, tg in enumerate(tags):
        c = colores[i % len(colores)]
        g, (xe, ye) = traza(1000 + i * 77, 150, 26, c, grosor=.55 if i % 2 else .8, relleno=(i == 0))
        yy = y0 + i * 29
        o.append('<g transform="translate(14 %d)">%s</g>' % (yy, g))
        # bandera con el nombre del tag al final de la traza
        ex, ey = 14 + xe, yy + ye
        o.append('<circle cx="%.1f" cy="%.1f" r="1.1" fill="%s"/>' % (ex, ey, c))
        ancho = 3.2 * len(tg) + 6
        o.append('<g transform="translate(%.1f %.1f)"><rect x="3" y="-3.1" width="%.1f" height="6.2" rx="1" fill="none" stroke="%s" stroke-width=".3"/>'
                 '<text x="6" y="1.3" font-family="Cascadia Mono, Consolas, monospace" font-size="3.1" fill="%s">%s</text></g>'
                 % (ex, ey, ancho, c, c, html.escape(tg)))
    o.append('<rect y="%d" width="%d" height="%d" fill="url(#vel)"/>' % (h - 60, w, 60))
    o.append('</svg>')
    return "\n".join(o)


def svg_cabecera(titulo, n):
    s = semilla(titulo)
    a, _ = traza(s, 180, 26, CIAN, grosor=.5, pasos=200, relleno=True)
    b, _ = traza(s + 1, 180, 26, AMBAR, grosor=.3, pasos=200)
    marcas = "".join('<line x1="%d" y1="22" x2="%d" y2="%s" stroke="%s" stroke-width=".25"/>' % (x, x, 25 if x % 30 else 23.5, TINTA) for x in range(0, 181, 6))
    return ('<svg viewBox="0 0 180 26" preserveAspectRatio="none" xmlns="http://www.w3.org/2000/svg">%s%s%s'
            '<rect x="0" y="0" width="2.2" height="26" fill="%s"/></svg>') % (a, b, marcas, AMBAR)


def esc_css(t):
    return t.replace("\\", "\\\\").replace('"', '\\"')


def reglas_paginas(caps, tx):
    """@page por capitulo: encabezado con el nombre del capitulo y solapa en el margen derecho."""
    base = ('font-family: Bahnschrift, "Segoe UI", sans-serif; font-size: 8pt; color: %s; ' % GRIS)
    out = []
    comunes = ('@bottom-left { content: "PV Suite · %s"; ' + base + 'vertical-align: top; padding-top: 5mm; border-top: 0.6pt solid #cfd8de; margin-top: 0; }\n'
               '  @bottom-right { content: counter(page); font-family: "Cascadia Mono", Consolas, monospace; font-size: 9pt; color: ' + TINTA + '; '
               'vertical-align: top; padding-top: 4.4mm; border-top: 2pt solid ' + AMBAR + '; }\n')
    def pagina(nombre, rotulo, num):
        s = ('@page %s {\n  background: ' + PAPEL + ';\n  @top-left { content: "%s"; %s vertical-align: bottom; padding-bottom: 4mm; letter-spacing: .18em; text-transform: uppercase; }\n') % (
            nombre, esc_css(rotulo), base)
        s += '  ' + comunes % esc_css(tx["manual"])
        if num is None:
            s += ('  @right-middle { content: "%s"; writing-mode: vertical-rl; white-space: nowrap; background: %s; color: %s; '
                  'font-family: Bahnschrift, "Segoe UI", sans-serif; font-weight: 700; font-size: 8.5pt; letter-spacing: .16em; '
                  'text-transform: uppercase; text-align: center; vertical-align: middle; padding: 3mm 0; width: 7mm; }\n') % (esc_css(rotulo), banda(CIAN, 24), TINTA)
        else:
            s += ('  @right-middle { content: "%s"; white-space: nowrap; background: %s; color: %s; '
                  'font-family: Bahnschrift, "Segoe UI", sans-serif; font-weight: 700; font-size: 11pt; '
                  'text-align: center; vertical-align: middle; padding: 2.5mm 0; width: 9mm; }\n') % (num, banda(CIAN, 8), TINTA)
        s += '}\n'
        return s
    out.append(pagina("indice", tx["indice"], None))
    for (n, tid, titulo) in caps:
        out.append(pagina("c%s" % n, titulo, "%02d" % n))
        out.append('section.cap[data-n="%s"] { page: c%s; }\n' % (n, n))
    return "".join(out)


def banda(color, mitad):
    """Fondo de una solapa de 2*mitad mm centrada en el margen (la caja del margen ocupa todo el alto)."""
    return ("linear-gradient(to bottom, transparent calc(50%% - %dmm), %s calc(50%% - %dmm), %s calc(50%% + %dmm), transparent calc(50%% + %dmm))"
            % (mitad, color, mitad, color, mitad, mitad)).replace("%%", "%")


def version_suite():
    """La version de PV Suite: PVSUITE_VERSION de pvbrowser/pvbupdate.h (no se escribe a mano en ningun manual)."""
    h = os.path.join(AQUI, "..", "..", "pvbrowser", "pvbupdate.h")
    m = re.search(r'#define\s+PVSUITE_VERSION\s+"([^"]+)"', open(h, encoding="utf-8").read())
    if not m:
        raise SystemExit("no se encontro PVSUITE_VERSION en %s" % h)
    return m.group(1)


def quitar_tags(t):
    return re.sub(r"<[^>]+>", "", t).strip()


def procesar(ruta):
    s = open(ruta, encoding="utf-8").read()
    m = re.search(r'<html lang="(\w+)"', s)
    lang = m.group(1) if m else "es"
    tx = TXT.get(lang, TXT["es"])

    # --- datos que se conservan (meta o portada vieja)
    producto = "PV Suite"
    version = version_suite()
    anio = "2026"

    # nombre de los instaladores con la version vigente (PvSuite-<version>-Setup-...)
    s = re.sub(r"PvSuite-[0-9][^<\s]*?-Setup", "PvSuite-%s-Setup" % version, s)
    # --- cuerpo: sacar lo generado antes y la portada/indice viejos
    cuerpo = s[s.index("<body>") + 6: s.rindex("</body>")]
    cuerpo = re.sub(r"<!--\[portada\]-->.*?<!--\[/portada\]-->", "", cuerpo, flags=re.S)
    cuerpo = re.sub(r'<div class="portada">.*?</div>\s*(?=<h1 class="first")', "", cuerpo, flags=re.S)
    cuerpo = re.sub(r'<h1 class="first" id="indice">.*?</ol>', "", cuerpo, flags=re.S)
    cuerpo = re.sub(r"<!--\[indice\]-->.*?<!--\[/indice\]-->", "", cuerpo, flags=re.S)
    cuerpo = re.sub(r"<!--\[cab\]-->.*?<!--\[/cab\]-->", "", cuerpo, flags=re.S)
    cuerpo = re.sub(r'<section class="cap"[^>]*>\s*<!--\[cap\]-->', "", cuerpo)
    cuerpo = cuerpo.replace("<!--[/cap]-->\n</section>", "").replace("<!--[/cap]--></section>", "")
    cuerpo = re.sub(r'(<h1 id="[^"]+">)<span class="n">0*(\d+)</span>', r"\1\2. ", cuerpo)
    cuerpo = re.sub(r'<span class="n">([^<]*)</span>\s*', r"\1 ", cuerpo)       # numeros de h2 otra vez en texto
    cuerpo = re.sub(r"<!-- =+ -->\s*", "", cuerpo)

    # --- capitulos
    partes = re.split(r'(?=<h1 id=")', cuerpo)
    previo = partes[0].strip()
    caps = []
    for p in partes[1:]:
        m = re.match(r'<h1 id="([^"]+)">\s*(\d+)\.\s*(.*?)</h1>', p, re.S)
        if not m:
            raise SystemExit("%s: h1 sin numero: %s" % (ruta, p[:80]))
        caps.append((int(m.group(2)), m.group(1), quitar_tags(m.group(3)), p[m.end():].strip("\n")))
    if previo:
        print("  aviso: texto antes del primer capitulo (se conserva)")

    indice = []
    salida = []
    for (n, tid, titulo, resto) in caps:
        k = [0]
        h2s = []

        def h2(mm):
            k[0] += 1
            texto = mm.group(1)
            mn = re.match(r"\s*(\d+\.\d+)\s+(.*)", texto, re.S)
            hid = "s%d-%d" % (n, k[0])
            if mn:
                h2s.append((hid, mn.group(2)))
                return '<h2 id="%s"><span class="n">%s</span>%s</h2>' % (hid, mn.group(1), mn.group(2))
            h2s.append((hid, quitar_tags(texto)))
            return '<h2 id="%s">%s</h2>' % (hid, texto)
        resto = re.sub(r'<h2(?: id="[^"]*")?>(.*?)</h2>', h2, resto, flags=re.S)
        resto = re.sub(r'<div class="nota">', '<div class="nota" data-etq="%s">' % html.escape(tx["nota"]), resto)
        resto = re.sub(r'<div class="nota" data-etq="[^"]*" data-etq="[^"]*">', '<div class="nota" data-etq="%s">' % html.escape(tx["nota"]), resto)
        indice.append((n, tid, titulo, h2s))
        salida.append(
            '<!-- ======================================================================= -->\n'
            '<section class="cap" data-n="%d"><!--[cap]-->\n<!--[cab]--><div class="cabecera">%s</div><!--[/cab]-->\n'
            '<h1 id="%s"><span class="n">%02d</span>%s</h1>\n%s\n<!--[/cap]-->\n</section>\n'
            % (n, svg_cabecera(titulo, n), tid, n, html.escape(titulo, quote=False) if "&" not in titulo else titulo, resto))

    # --- portada
    t1 = producto.split()
    nombre = html.escape(producto)
    if len(t1) >= 3 or producto.endswith("6"):
        nombre = "%s <b>%s</b>" % (html.escape(" ".join(t1[:-1])), html.escape(t1[-1]))
    portada = ('<!--[portada]-->\n<section class="portada">\n%s\n<div class="txt">\n<div class="marca">pvbrowser · pvdevelop · pvserver · rllib</div>\n'
               '<h1 class="titulo">%s</h1>\n<div class="sub">%s</div>\n<div class="lema">%s</div>\n</div>\n'
               '<div class="pie"><div><b>Néstor Palomeque</b><br>%s · %s</div><div class="mono" style="text-align:right">%s %s<br>%s</div></div>\n'
               '</section>\n<!--[/portada]-->\n') % (svg_portada(tx["tags"]), nombre, html.escape(tx["manual"]), html.escape(tx["lema"]),
                                                    html.escape(tx["licencia"]), anio, tx["version"], html.escape(version), anio)

    # --- indice (mapa)
    tarj = []
    for (n, tid, titulo, h2s) in indice:
        li = "".join('<li><a href="#%s">%s</a></li>' % (hid, t) for hid, t in h2s)
        tarj.append('<div class="tarjeta"><div class="cab"><span class="num">%02d</span><a href="#%s">%s</a></div><ul>%s</ul></div>'
                    % (n, tid, titulo, li))
    ind = ('<!--[indice]-->\n<section class="indice"><h1 id="indice">%s</h1>\n<div class="mapa">\n%s\n</div></section>\n<!--[/indice]-->\n'
           % (html.escape(tx["indice"]), "\n".join(tarj)))

    titulo_doc = "%s - %s" % (producto, tx["manual"])
    cabeza = ('<!DOCTYPE html>\n<html lang="%s">\n<head>\n<meta charset="utf-8">\n<meta name="viewport" content="width=device-width, initial-scale=1">\n'
              '<meta name="pv-producto" content="%s">\n<meta name="pv-version" content="%s">\n<meta name="generator" content="doc/herramientas/estilo_manual.py">\n'
              '<title>%s</title>\n<style>\n%s\n/* ---- paginas por capitulo (generadas) ---- */\n%s</style>\n</head>\n<body>\n\n'
              % (lang, html.escape(producto), html.escape(version), html.escape(titulo_doc), CSS,
                 reglas_paginas([(n, tid, t) for (n, tid, t, _) in indice], tx)))
    out = cabeza + portada + "\n" + ind + "\n" + "\n".join(salida) + "\n</body>\n</html>\n"
    open(ruta, "w", encoding="utf-8", newline="\n").write(out)
    print("%s: %d capitulos" % (os.path.basename(ruta), len(caps)))


def main():
    args = sys.argv[1:]
    if not args:
        args = sorted(glob.glob(os.path.join(AQUI, "..", "manual", "Manual_PvSuite*.html")))
    for a in args:
        procesar(a)


if __name__ == "__main__":
    main()
