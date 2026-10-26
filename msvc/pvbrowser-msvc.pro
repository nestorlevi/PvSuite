TEMPLATE = subdirs
CONFIG += ordered

# pvbrowser con navegador web (Qt WebEngine, solo existe para MSVC) y, para quien compile
# sus pvservers con Visual Studio, rllib.lib y serverlib.lib. Kit "Desktop Qt 6.x MSVC2022 64bit".

!win32-msvc*:error("pvbrowser-msvc.pro es para el kit MSVC (Qt WebEngine no existe para MinGW)")

SUBDIRS += \
    qwt-msvc/qwt-msvc.pro \
    ../pvbrowser/pvbrowser-with-www.pro \
    rllib-msvc/rllib-msvc.pro \
    pvserver-msvc/pvserver-msvc.pro
