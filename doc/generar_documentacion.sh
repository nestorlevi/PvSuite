#!/bin/sh
# Genera la documentacion de PV Suite (Linux / macOS):
#   doc/pvslib   referencia de las funciones del pvserver (doxygen)
#   doc/manual/Manual_PvSuite*.pdf          manual de usuario es/en/pt/de (desde Manual_PvSuite*.html)
#   doc/ayuda                               ayuda integrada de pvdevelop (Ayuda > Manual), desde los manuales
#   doc/index.html                          portada de la documentacion
# Necesita doxygen, python3 (estilo del manual, ayuda integrada y portada: doc/herramientas) y, para el PDF,
# chromium o google-chrome (131 o mas nuevo).
set -e
cd "$(dirname "$0")"
DOXYGEN=${DOXYGEN:-doxygen}
if command -v "$DOXYGEN" >/dev/null 2>&1; then
  echo "===== Referencia para programadores (doxygen)"
  cd doxygen
  for d in pvslib; do
    rm -rf "../$d"
    echo "$d"
    "$DOXYGEN" "$d.doxyfile"
  done
  cd ..
else
  echo "No se encontro doxygen (DOXYGEN=/ruta/doxygen): no se regenera la referencia para programadores"
fi

echo "===== Estilo del manual, ayuda integrada y portada (Python)"
PYTHON=${PYTHON:-python3}
if command -v "$PYTHON" >/dev/null 2>&1; then
  "$PYTHON" herramientas/estilo_manual.py
  "$PYTHON" herramientas/ayuda_html.py
  "$PYTHON" herramientas/portal.py --basico
else
  echo "No se encontro python3: no se regenera doc/ayuda ni doc/index.html (PYTHON=/ruta/python3)"
fi

echo "===== Manual de usuario (PDF)"
BROWSER=""
for b in chromium chromium-browser google-chrome "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"; do
  if command -v "$b" >/dev/null 2>&1 || [ -x "$b" ]; then BROWSER="$b"; break; fi
done
if [ -z "$BROWSER" ]; then echo "No se encontro chromium ni chrome: el PDF no se genero"; exit 1; fi
# Manual en espanol (Manual_PvSuite), ingles (_en), portugues (_pt) y aleman (_de)
for m in Manual_PvSuite Manual_PvSuite_en Manual_PvSuite_pt Manual_PvSuite_de; do
  echo "  $m.pdf"
  "$BROWSER" --headless=new --disable-gpu --no-pdf-header-footer \
    --print-to-pdf="$PWD/manual/$m.pdf" "file://$PWD/manual/$m.html"
done
echo "Documentacion generada (abrir doc/index.html)"
