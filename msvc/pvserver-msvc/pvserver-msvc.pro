# Nestor: serverlib.lib compilada con MSVC (para los pvservers de los usuarios que eligen MSVC),
# desde pvbrowser-msvc.pro. Incluye ../../pvserver/pvsmt.pro tal cual; este envoltorio
# solo da una carpeta de compilacion propia, separada de la de MinGW
# (pvsuite.pro compila el mismo .pro con MinGW).
# serverlib.lib queda en pvserver (DESTDIR del .pro original), junto a libserverlib.a.
PVSERVER_SRC = $$PWD/../../pvserver
VPATH       += $$PVSERVER_SRC
INCLUDEPATH += $$PVSERVER_SRC
include($$PVSERVER_SRC/pvsmt.pro)
DEFINES += _CRT_SECURE_NO_WARNINGS
