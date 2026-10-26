TEMPLATE = subdirs
CONFIG += ordered

# PV Suite: compilar con qmake (Qt 6.9) en una carpeta aparte, por ejemplo:
#   mkdir build && cd build && qmake ../pvsuite.pro CONFIG+=release && make
# Windows: este .pro con el kit MinGW compila todo menos pvbrowser; pvbrowser (Qt WebEngine solo
# existe para MSVC) se compila con msvc/pvbrowser-msvc.pro y el kit MSVC 2022. Ver README.md.

win32:!win32-g++:error("pvsuite.pro en Windows es para el kit MinGW; pvbrowser: msvc/pvbrowser-msvc.pro")

win32 {
    SUBDIRS += rllib/lib/lib_staticlib.pro    # biblioteca estatica en Windows
} else {
    SUBDIRS += rllib/lib/lib.pro              # biblioteca compartida en Linux y macOS
}

SUBDIRS += \
    qwt/src/src.pro \
    rllib/rlfind/rlfind.pro \
    rllib/rlhistory/rlhistory.pro \
    rllib/rlhtml2pdf/rlhtml2pdf.pro \
    rllib/rlsvg/rlsvgcat.pro \
    fake_qmake/fake_qmake.pro \
    pvserver/pvsmt.pro \
    pvdevelop/pvdevelop-with-qwt62.pro

!win32:SUBDIRS += pvbrowser/pvbrowser-with-www.pro

SUBDIRS += \
    language_bindings/lua/lua-5.4.0/src/src.pro \
    language_bindings/lua/pvapplua/pvapplua.pro \
    language_bindings/lua/pvslua/pvslua.pro
