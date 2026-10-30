# Nestor: qwt para Android, estatica, sin modificar los archivos de ../../../qwt
# (igual que Compilar/pvb6-mingw-www-64/qwt-msvc: se incluye su src.pro tal cual).
QWT_SRC = $$PWD/../../../qwt/src
VPATH       += $$QWT_SRC
INCLUDEPATH += $$QWT_SRC
include($$QWT_SRC/src.pro)

CONFIG  -= dll shared
CONFIG  += staticlib
DEFINES -= QT_DLL QWT_DLL QWT_MAKEDLL
DESTDIR  = $$OUT_PWD/lib
INSTALLS =
