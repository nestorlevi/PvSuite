#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Ayuda integrada de pvdevelop (Ayuda > Manual).

Convierte los manuales doc/manual/Manual_PvSuite{,_en,_pt,_de}.html en paginas sueltas por capitulo que
entiende QTextBrowser (HTML 4 con estilos simples, sin CSS moderno ni SVG) y un indice para el arbol:

    doc/ayuda/<es|en|pt|de>/inicio.html
    doc/ayuda/<es|en|pt|de>/NN_<id>.html      un archivo por capitulo
    doc/ayuda/<es|en|pt|de>/indice.tsv        archivo <TAB> ancla <TAB> nivel (1 capitulo, 2 seccion) <TAB> titulo

Los capitulos y secciones se arman a partir de lo que dice el manual: si se agrega una seccion al manual, aparece sola.
Uso:  python ayuda_html.py            (todos)       python ayuda_html.py ruta/Manual_PvSuite.html ...
"""
import glob
import html
import os
import re
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
from estilo_manual import version_suite  # noqa: E402
DOC = os.path.normpath(os.path.join(AQUI, ".."))
CODIGO = {"": "es", "_en": "en", "_pt": "pt", "_de": "de"}

TINTA, CIAN, AMBAR = "#0d1b2a", "#00b4c4", "#ffb000"
INICIO = {
    "es": ("Ayuda de PV Suite", "Elegí un capítulo del árbol, o buscá una palabra arriba. <b>F1</b> abre la ayuda del diálogo o la vista actual.", "Capítulos"),
    "en": ("PV Suite Help", "Pick a chapter in the tree, or search for a word above. <b>F1</b> opens the help of the current dialog or view.", "Chapters"),
    "pt": ("Ajuda do PV Suite", "Escolha um capítulo na árvore, ou busque uma palavra acima. <b>F1</b> abre a ajuda do diálogo ou da vista atual.", "Capítulos"),
    "de": ("PV Suite Hilfe", "Wählen Sie ein Kapitel im Baum oder suchen Sie oben ein Wort. <b>F1</b> öffnet die Hilfe des aktuellen Dialogs oder der Ansicht.", "Kapitel"),
}


def sin_tags(t):
    return html.unescape(re.sub(r"<[^>]+>", "", t)).strip()


def convertir(cuerpo):
    """HTML del manual -> HTML que se ve bien en QTextBrowser."""
    c = cuerpo
    c = re.sub(r"<!--.*?-->", "", c, flags=re.S)
    c = re.sub(r'<div class="cabecera">.*?</div>', "", c, flags=re.S)
    # titulos: ancla con name (QTextBrowser) ademas del id
    c = re.sub(r'<h1 id="([^"]+)"><span class="n">0*(\d+)</span>(.*?)</h1>',
               lambda m: '<a name="%s"></a><h1>%s. %s</h1>' % (m.group(1), m.group(2), m.group(3)), c, flags=re.S)
    c = re.sub(r'<h2 id="([^"]+)"><span class="n">([^<]*)</span>(.*?)</h2>',
               lambda m: '<a name="%s"></a><h2>%s&nbsp; %s</h2>' % (m.group(1), m.group(2), m.group(3)), c, flags=re.S)
    c = re.sub(r'<h2 id="([^"]+)">(.*?)</h2>', lambda m: '<a name="%s"></a><h2>%s</h2>' % (m.group(1), m.group(2)), c, flags=re.S)
    # codigo en linea (antes de <pre>: dentro de pre queda tal cual)
    partes = re.split(r"(<pre>.*?</pre>)", c, flags=re.S)
    for i, p in enumerate(partes):
        if p.startswith("<pre>"):
            interior = re.sub(r"</?code>", "", p[5:-6])
            partes[i] = ('<table width="100%%" cellspacing="0" cellpadding="8" bgcolor="%s"><tr><td width="5" bgcolor="%s"></td><td>'
                         '<pre style="color:#e6f0f6; font-family:Consolas,monospace; font-size:9pt">%s</pre></td></tr></table><p></p>') % (TINTA, CIAN, interior)
        else:
            p = re.sub(r"<code>(.*?)</code>",
                       r'<span style="font-family:Consolas,monospace; background-color:#e5f4f6; color:#0b3f48">\1</span>', p, flags=re.S)
            p = re.sub(r'<span class="kbd">(.*?)</span>',
                       r'<span style="font-family:Consolas,monospace; background-color:#e8edf1; color:%s">&nbsp;\1&nbsp;</span>' % TINTA, p, flags=re.S)
            p = re.sub(r'<div class="nota"[^>]*>(.*?)</div>',
                       r'<table width="100%%" cellspacing="0" cellpadding="8" bgcolor="#fff6df"><tr><td width="5" bgcolor="%s"></td><td>\1</td></tr></table><p></p>' % AMBAR,
                       p, flags=re.S)
            # tablas
            p = p.replace("<table>", '<table width="100%" cellspacing="0" cellpadding="5" border="0">')
            p = re.sub(r"<th>", '<th align="left" bgcolor="%s" style="color:#ffffff; background-color:%s">' % (TINTA, TINTA), p)
            p = re.sub(r"<td>", '<td style="border-bottom:1px solid #d9e1e6">', p)
            partes[i] = p
    return "".join(partes)


def pagina(titulo, cuerpo):
    return ('<!DOCTYPE html>\n<html><head><meta charset="utf-8"><title>%s</title></head>\n<body>\n%s\n</body></html>\n'
            % (html.escape(titulo), cuerpo))


def procesar(ruta):
    suf = re.search(r"Manual_PvSuite(_\w+)?\.html$", ruta).group(1) or ""
    lg = CODIGO[suf]
    s = open(ruta, encoding="utf-8").read()
    caps = re.findall(r'<section class="cap" data-n="(\d+)">(.*?)</section>', s, flags=re.S)
    if not caps:
        raise SystemExit("%s: sin capitulos (aplicar antes estilo_manual.py)" % ruta)
    salida = os.path.join(DOC, "ayuda", lg)
    os.makedirs(salida, exist_ok=True)
    for f in glob.glob(os.path.join(salida, "*")):
        os.remove(f)
    # ancla -> archivo
    ancla = {}
    datos = []
    for (n, cuerpo) in caps:
        n = int(n)
        m = re.search(r'<h1 id="([^"]+)"[^>]*>(?:<span class="n">[^<]*</span>)?(.*?)</h1>', cuerpo, flags=re.S)
        cid, titulo = m.group(1), sin_tags(m.group(2))
        archivo = "%02d_%s.html" % (n, cid)
        ancla[cid] = archivo
        secs = []
        for h in re.finditer(r'<h2 id="([^"]+)">(?:<span class="n">([^<]*)</span>)?(.*?)</h2>', cuerpo, flags=re.S):
            ancla[h.group(1)] = archivo
            secs.append((h.group(1), (h.group(2) + " " if h.group(2) else "") + sin_tags(h.group(3))))
        datos.append((n, cid, titulo, archivo, cuerpo, secs))

    def enlaces(t):
        def r(m):
            a = m.group(1)
            return 'href="%s#%s"' % (ancla[a], a) if a in ancla else m.group(0)
        return re.sub(r'href="#([^"]+)"', r, t)

    tsv = ["inicio.html\t\t1\t%s" % INICIO[lg][0]]
    filas = []
    for (n, cid, titulo, archivo, cuerpo, secs) in datos:
        html_c = enlaces(convertir(cuerpo))
        open(os.path.join(salida, archivo), "w", encoding="utf-8", newline="\n").write(pagina(titulo, html_c))
        tsv.append("%s\t%s\t1\t%d. %s" % (archivo, cid, n, titulo))
        for (a, t) in secs:
            tsv.append("%s\t%s\t2\t%s" % (archivo, a, t))
        filas.append('<tr><td width="40"><span style="font-size:18pt; color:%s"><b>%02d</b></span></td><td><a href="%s">%s</a></td></tr>' % (CIAN, n, archivo, html.escape(titulo)))
    open(os.path.join(salida, "indice.tsv"), "w", encoding="utf-8", newline="\n").write("\n".join(tsv) + "\n")
    t, sub, cap = INICIO[lg]
    inicio = ('<h1>%s</h1><p><b>PV Suite %s</b></p><p>%s</p><h2>%s</h2><table cellspacing="0" cellpadding="4">%s</table>' % (t, version_suite(), sub, cap, "\n".join(filas)))
    open(os.path.join(salida, "inicio.html"), "w", encoding="utf-8", newline="\n").write(pagina(t, inicio))
    print("%s: %d capitulos, %d secciones" % (lg, len(datos), sum(len(d[5]) for d in datos)))


def main():
    args = sys.argv[1:] or sorted(glob.glob(os.path.join(DOC, "manual", "Manual_PvSuite*.html")))
    for a in args:
        procesar(a)


if __name__ == "__main__":
    main()
