# Nestor: qwt compilada con MSVC sin modificar los archivos de ../../qwt
# (así qwt se puede actualizar descargándola de nuevo).
# Se incluye su src.pro tal cual y solo se cambia, después, lo que hace falta para MSVC:
#  - DESTDIR: aparte (lib/ de esta carpeta); si no, su qwt.dll reemplaza a la de MinGW en qwt/lib
#  - MOC_DIR: uno por configuración; debug y release en paralelo (jom) escribían el mismo moc_predefs.h
QWT_SRC = $$PWD/../../qwt/src
VPATH       += $$QWT_SRC    # src.pri lista los archivos sin ruta: se buscan en qwt/src
INCLUDEPATH += $$QWT_SRC
include($$QWT_SRC/src.pro)

DESTDIR = $$PWD/lib

CONFIG(debug, debug|release): MOC_DIR = moc/debug
else:                         MOC_DIR = moc/release
