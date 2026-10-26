#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Portada de la documentacion (doc/index.html), con el estilo "Señal" del manual (estilo_manual.py).

Muestra solo lo que existe en esta carpeta doc: el manual en los 4 idiomas (PDF y HTML), la referencia de las
funciones del pvserver (pvslib), la de rllib y la de pvdevelop (doxygen), el primer de pvbrowser y los atajos de teclado.
Uso:  python portal.py [--basico]        (escribe ../index.html; --basico: PvSuite sin lupa, procesos ni instalador)
"""
import html
import os
import re
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
from estilo_manual import TINTA, TINTA2, CIAN, AMBAR, PAPEL, GRIS, traza  # noqa: E402

DOC = os.path.normpath(os.path.join(AQUI, ".."))

ATAJOS = [
    ("Ctrl+N / Ctrl+O / Ctrl+S", "Nuevo proyecto / abrir / guardar", "New project / open / save"),
    ("Ctrl+E / Ctrl+D", "Editor / Designer", "Editor / Designer"),
    ("Ctrl+M", "Compilar el proyecto", "Build the project"),
    ("Ctrl+T", "Compilar y ejecutar el pvserver", "Build and run the pvserver"),
    ("Ctrl+P", "Abrir pvbrowser", "Open pvbrowser"),
    ("Ctrl+J", "Mostrar u ocultar la consola de compilación", "Show or hide the build console"),
    ("Ctrl+L", "Lupa del Designer", "Designer loupe"),
    ("Ctrl+F / Ctrl+R", "Buscar / reemplazar en el editor", "Find / replace in the editor"),
    ("Ctrl+Espacio", "Texto predictivo en el editor", "Code completion in the editor"),
    ("Tab / Shift+Tab", "Sangrar / quitar sangría (varias líneas)", "Indent / unindent (several lines)"),
    ("Ctrl+Z", "Deshacer (el editor, por pestaña; el Designer, el último movimiento)", "Undo (editor per tab; Designer last move)"),
    ("Ctrl+Shift+P", "Procesos del proyecto", "Project processes"),
    ("Ctrl+Shift+I", "Generar instalador del proyecto", "Generate project installer"),
    ("Ctrl+H / F1", "Ayuda: manual / ayuda de la ventana actual", "Help: manual / help of the current window"),
]
SOLO_DEV = ("Ctrl+L", "Ctrl+Shift+P", "Ctrl+Shift+I")
DISENO = [
    ("Doble clic en un widget", "Propiedades", "Properties"),
    ("Doble clic en un lugar vacío / Shift+clic", "Insertar widget", "Insert widget"),
    ("Ctrl+Shift+clic", "Insertar otra vez el último tipo de widget", "Insert the last widget type again"),
    ("Flechas / Ctrl+flechas", "Mover / cambiar el tamaño", "Move / resize"),
    ("P  ·  Alt+clic", "Propiedades del widget", "Widget properties"),
    ("Supr", "Borrar los widgets seleccionados", "Delete selected widgets"),
    ("Ctrl+C / Ctrl+V", "Copiar y pegar widgets", "Copy and paste widgets"),
]


def tarjeta(titulo, sub, href, etiqueta=None, color=CIAN):
    return ('<a class="tar" href="%s" style="--c:%s"><span class="t">%s</span><span class="s">%s</span>%s</a>'
            % (html.escape(href), color, html.escape(titulo), html.escape(sub), ('<span class="e">%s</span>' % html.escape(etiqueta)) if etiqueta else ""))


def tabla(filas):
    return '<table>' + ''.join('<tr><td class="k"><code>%s</code></td><td>%s<br><span class="en">%s</span></td></tr>'
                               % (html.escape(a), html.escape(b), html.escape(c)) for a, b, c in filas) + '</table>'


def main():
    basico = "--basico" in sys.argv   # PvSuite basico: sin lupa, procesos ni instalador del proyecto
    atajos = [a for a in ATAJOS if not (basico and a[0] in SOLO_DEV)]
    existe = lambda p: os.path.exists(os.path.join(DOC, p))
    m = re.search(r'name="pv-producto" content="([^"]*)"', open(os.path.join(DOC, "manual", "Manual_PvSuite.html"), encoding="utf-8").read()) \
        if existe("manual/Manual_PvSuite.html") else None
    producto = m.group(1) if m else "PV Suite"
    idiomas = [("", "Castellano"), ("_en", "English"), ("_pt", "Português"), ("_de", "Deutsch")]

    manual = []
    for suf, nombre in idiomas:
        if not existe("manual/Manual_PvSuite%s.html" % suf):
            continue
        pdf = "manual/Manual_PvSuite%s.pdf" % suf
        manual.append(tarjeta(nombre, "Manual de usuario · HTML", "manual/Manual_PvSuite%s.html" % suf, "HTML"))
        if existe(pdf):
            manual.append(tarjeta(nombre, "Manual de usuario · PDF", pdf, "PDF", AMBAR))
    ref = []
    if existe("pvslib/index.html"):
        ref.append(tarjeta("pvslib", "Funciones del pvserver (pvSetText, pvSetStyleSheet, ...)", "pvslib/index.html"))
    if existe("rllib/index.html"):
        ref.append(tarjeta("rllib", "Tags, clientes Modbus / Siemens / OPC UA, memoria compartida", "rllib/index.html"))
    if existe("pvdevelop/index.html"):
        ref.append(tarjeta("pvdevelop", "Referencia interna del código de pvdevelop", "pvdevelop/index.html"))
    if existe("primer/html/en_index.html"):
        ref.append(tarjeta("Primer (English)", "Introducción a pvbrowser", "primer/html/en_index.html", None, GRIS))
    if existe("primer/html/de_index.html"):
        ref.append(tarjeta("Primer (Deutsch)", "Einführung in pvbrowser", "primer/html/de_index.html", None, GRIS))

    traz = []
    for i, (col, ancho) in enumerate([(CIAN, 1.0), (AMBAR, .7), ("#7be0c3", .6)]):
        g, _ = traza(4000 + i * 31, 1000, 90, col, grosor=1.6 * ancho, pasos=260, relleno=(i == 0))
        traz.append('<g transform="translate(0 %d)">%s</g>' % (i * 38, g))
    hero = ('<svg viewBox="0 0 1000 210" preserveAspectRatio="none" xmlns="http://www.w3.org/2000/svg" class="tr">'
            '%s</svg>' % "".join(traz))

    css = """
:root { --tinta:%(t)s; --tinta2:%(t2)s; --cian:%(c)s; --ambar:%(a)s; --papel:%(p)s; --gris:%(g)s; --txt:#1b2733; --card:#fff; --linea:#d9e1e6; }
@media (prefers-color-scheme: dark) { :root { --papel:#0a141f; --txt:#e6edf3; --card:#101f2e; --linea:#22384c; --gris:#9fb4c4; } }
* { box-sizing: border-box; }
body { margin: 0; background: var(--papel); color: var(--txt); font-family: "Segoe UI", "Helvetica Neue", Arial, sans-serif; line-height: 1.5; }
h1, h2, .t { font-family: Bahnschrift, "DIN Alternate", "Roboto Condensed", "Segoe UI", sans-serif; }
code { font-family: "Cascadia Mono", Consolas, "DejaVu Sans Mono", monospace; font-size: .86em; }
header { position: relative; background: var(--tinta); color: #fff; padding: 56px 16px 70px; overflow: hidden; }
header .tr { position: absolute; inset: auto 0 0 0; width: 100%%; height: 62%%; opacity: .5; }
header .in { position: relative; max-width: 1000px; margin: 0 auto; }
header .m { font-family: "Cascadia Mono", Consolas, monospace; font-size: 12px; letter-spacing: .28em; color: var(--cian); text-transform: uppercase; }
header h1 { font-size: clamp(40px, 9vw, 82px); line-height: 1; margin: 12px 0 6px; letter-spacing: -.01em; }
header h1 b { color: var(--ambar); }
header p { margin: 0; color: #b7c7d4; font-size: clamp(14px, 2.4vw, 19px); }
main { max-width: 1000px; margin: 0 auto; padding: 8px 16px 56px; }
h2 { font-size: 22px; margin: 36px 0 12px; padding-bottom: 6px; border-bottom: 2px solid var(--tinta); }
@media (prefers-color-scheme: dark) { h2 { border-color: var(--cian); } }
h2 small { font-size: 13px; color: var(--gris); font-weight: 400; margin-left: 8px; }
.grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(230px, 1fr)); gap: 12px; }
.tar { position: relative; display: block; background: var(--card); border: 1px solid var(--linea); border-top: 3px solid var(--c); padding: 12px 14px 12px; color: inherit; text-decoration: none; transition: transform .12s, box-shadow .12s; }
.tar:hover { transform: translateY(-2px); box-shadow: 0 6px 18px rgba(0,0,0,.14); }
.tar .t { display: block; font-size: 19px; font-weight: 700; }
.tar .s { display: block; font-size: 13px; color: var(--gris); margin-top: 2px; }
.tar .e { position: absolute; top: 10px; right: 10px; font-family: "Cascadia Mono", Consolas, monospace; font-size: 11px; font-weight: 700; padding: 1px 6px; background: var(--c); color: var(--tinta); }
table { border-collapse: collapse; width: 100%%; background: var(--card); border: 1px solid var(--linea); }
td { padding: 6px 10px; border-bottom: 1px solid var(--linea); vertical-align: top; font-size: 14px; }
td.k { white-space: nowrap; width: 1%%; }
td.k code { background: rgba(0,180,196,.14); padding: 1px 6px; }
.en { color: var(--gris); font-size: 12.5px; }
footer { max-width: 1000px; margin: 0 auto; padding: 0 16px 40px; color: var(--gris); font-size: 13px; }
""" % dict(t=TINTA, t2=TINTA2, c=CIAN, a=AMBAR, p=PAPEL, g=GRIS)

    nombre = html.escape(producto)
    partes = nombre.rsplit(" ", 1)
    titulo = ("%s <b>%s</b>" % tuple(partes)) if len(partes) == 2 and partes[1].isdigit() else nombre
    out = ['<!DOCTYPE html>\n<html lang="es">\n<head>\n<meta charset="utf-8">\n<meta name="viewport" content="width=device-width, initial-scale=1">\n'
           '<meta name="generator" content="doc/herramientas/portal.py">\n<title>%s - Documentación</title>\n<style>%s</style>\n</head>\n<body>\n' % (nombre, css),
           '<header>%s<div class="in"><div class="m">pvbrowser · pvdevelop · pvserver · rllib</div><h1>%s</h1><p>Documentación · Documentation · Documentação · Dokumentation</p></div></header>\n<main>\n' % (hero, titulo)]
    if manual:
        out.append('<h2>Manual de usuario <small>User manual · Manual do usuário · Benutzerhandbuch</small></h2>\n<div class="grid">%s</div>\n' % "".join(manual))
    if ref:
        out.append('<h2>Referencia y guías <small>Reference</small></h2>\n<div class="grid">%s</div>\n' % "".join(ref))
    out.append('<h2>Atajos de teclado <small>Keyboard shortcuts</small></h2>\n%s\n<h2>En el Designer <small>In the Designer</small></h2>\n%s\n' % (tabla(atajos), tabla(DISENO)))
    out.append('</main>\n<footer>La ayuda integrada está en pvdevelop: <b>Ayuda &gt; Manual</b> (Ctrl+H) y <b>F1</b> en cualquier ventana. '
               'Para regenerar esta documentación: <code>generar_documentacion.bat</code> (Windows) o <code>generar_documentacion.sh</code> (Linux).</footer>\n</body>\n</html>\n')
    open(os.path.join(DOC, "index.html"), "w", encoding="utf-8", newline="\n").write("".join(out))
    print("index.html: %d manuales, %d referencias" % (len(manual), len(ref)))


if __name__ == "__main__":
    main()
