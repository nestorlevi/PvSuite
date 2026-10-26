# Nestor: rllib.lib compilada con MSVC (para los pvservers de los usuarios que eligen MSVC),
# desde pvbrowser-msvc.pro. Incluye ../../rllib/lib/lib_staticlib.pro tal cual; este
# envoltorio solo da una carpeta de compilacion propia, separada de la de MinGW
# (pvsuite.pro compila el mismo .pro con MinGW).
# rllib.lib queda en rllib/lib (DESTDIR del .pro original), junto a librllib.a.
RLLIB_SRC = $$PWD/../../rllib/lib
VPATH       += $$RLLIB_SRC
INCLUDEPATH += $$RLLIB_SRC
include($$RLLIB_SRC/lib_staticlib.pro)
DEFINES += _CRT_SECURE_NO_WARNINGS
