# murx.rl.was.here.oct.2023 QT     += printsupport uitools widgets opengl svg
# murx.rl nov 2024 QT     += printsupport widgets opengl svg
QT     += printsupport uitools widgets svg
QT     -= opengl

CONFIG += warn_on release

#Here Nestor: añadir la librería ws2_32 para windows
win32 {
    LIBS += -lws2_32
}
   
contains(QMAKE_CXX, g++) {
  QMAKE_LFLAGS += -static-libgcc
}
INCLUDEPATH  += ../pvbrowser
INCLUDEPATH  += ../qwt/src

win32-msvc* {   # Nestor: MSVC usa la biblioteca de importación de qwt.dll
    DEFINES += QWT_DLL
    LIBS += $$PWD/../Compilar/pvsuite-win64/qwt-msvc/lib/qwt.lib   # compilada por Compilar/pvsuite-win64/qwt-msvc (qwt sin modificar)
    LIBS += -luser32 -lshell32
    # pvdevelop y ../pvbrowser tienen archivos con el mismo nombre (main.cpp, mainwindow.cpp, opt.cpp, dlgopt.cpp):
    # con las reglas por directorio de nmake/jom se compilaba el de pvbrowser. Una regla por archivo:
    CONFIG += no_batch
} else:win32 {     #Nestor aquí
    LIBS += $$PWD/../qwt/lib/libqwt.a
} else:unix {
    LIBS += -L$$PWD/../qwt/lib -lqwt
    QMAKE_RPATHDIR += $$PWD/../qwt/lib
}

#LIBS         += /usr/lib64/libQt6UiTools.so
DEFINES      += PVDEVELOP
DEFINES      += NO_WEBKIT
#DEFINES     += NO_QWT
macx:DEFINES += PVMAC
macx:DEFINES += unix
unix:LIBS    += -ldl
#DEFINES      += PVB_FOOTPRINT_OLD_VERSION_BEFORE_QWT62
DEFINES      += PVB_FOOTPRINT_BASIC

HEADERS       = mainwindow.h \
                compilerconfig.h \
                dlgcompiler.h \
                codecompleter.h \
                opt.h \
                editor.h \
                designer.h \
                syntax.h \
                cutil.h \
                dlgopt.h \
                dlgnewprj.h \
                dlgsearchreplace.h \
                dlginsertFunction.h \
                dlgpastewidget.h \
                ../pvbrowser/dlgtextbrowser.h \
                dlgdaemon.h \
                dlgproperty.h \
                dlginsertwidget.h \
                dlgeditlayout.h \
                widgetgenerator.h \
                generatepython.h \
                generatephp.h \
                generateperl.h \
                generatetcl.h \
                generatelua.h \
                ui_dlgnewprj.h \
                ui_dlgsearchreplace.h \
                ui_dlginsertfunction.h \
                ui_dlgpastewidget.h \
                ../pvbrowser/webkit_ui_dlgtextbrowser.h \
                ui_dlgdaemon.h \
                ui_dlgproperty.h \
                ui_dlginsertwidget.h \
                ui_dlgeditlayout.h \
                ../pvbrowser/opt.h \
                ../pvbrowser/MyWidgets.h \
                ../pvbrowser/MyTextBrowser_without_www.h \
                ../pvbrowser/qwtwidgets.h \
                ../pvbrowser/qwtplotwidget.h \
                ../pvbrowser/qdrawwidget.h \
                ../pvbrowser/qimagewidget.h \
                ../pvbrowser/pvglwidget.h \
                ../pvbrowser/interpreter.h \
                ../pvbrowser/pvserver.h

SOURCES       = main.cpp \
                compilerconfig.cpp \
                dlgcompiler.cpp \
                codecompleter.cpp \
                mainwindow.cpp \
                opt.cpp \
                editor.cpp \
                designer.cpp \
                syntax.cpp \
                cutil.cpp \
                dlgopt.cpp \
                dlgnewprj.cpp \
                dlgsearchreplace.cpp \
                dlginsertFunction.cpp \
                dlgpastewidget.cpp \
                ../pvbrowser/dlgtextbrowser.cpp \
                dlgdaemon.cpp \
                dlgproperty.cpp \
                dlginsertwidget.cpp \
                dlgeditlayout.cpp \
                gmodbus.cpp \
                gppi.cpp \
                gsiemens.cpp \
                widgetgenerator.cpp \
                ../pvbrowser/MyWidgets.cpp \
                ../pvbrowser/MyTextBrowser_without_www.cpp \
                ../pvbrowser/qwtplotwidget.cpp \
                ../pvbrowser/QImageWidget.cpp \
                ../pvbrowser/QDrawWidget.cpp \
                ../pvbrowser/pvglwidget.cpp \
                ../pvbrowser/tcputil.cpp

FORMS         = dlgnewprj.ui \
                dlgsearchreplace.ui \
                dlginsertfunction.ui \
                dlgpastewidget.ui \
                dlgdaemon.ui \
                dlgproperty.ui \
                dlginsertwidget.ui \
                dlgeditlayout.ui
#               ../pvbrowser/dlgtextbrowser.ui \

RESOURCES     = pvdevelop.qrc

TARGET        = pvdevelop
# install
target.path   = /usr/local/bin
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS pvdevelop-with-basic-footprint.pro images
sources.path  = /opt/pvb/pvdevelop
INSTALLS     += target sources

# Nestor: tags de los daemons (modbus, siemensTCP, OPC UA); el mismo codigo lo usa rllib
HEADERS      += ../rllib/lib/rltagdef.h ../rllib/lib/rltag.h
# Nestor: rltagdef, rltag, rlsharedmemory y rlwthread vienen de librllib (una sola compilacion:
# compilarlos aca con otros defines daba clases de distinto tamanio y memoria pisada)
#SOURCES     += ../rllib/lib/rltagdef.cpp
# Nestor: monitor de tags (Daemon > Monitor de tags): lee la memoria compartida de los daemons
HEADERS      += ../rllib/lib/rlsharedmemory.h ../rllib/lib/rlwthread.h
#SOURCES     += ../rllib/lib/rltag.cpp ../rllib/lib/rlsharedmemory.cpp ../rllib/lib/rlwthread.cpp
# Nestor: el monitor de tags tambien escribe (clientes Modbus, Siemens y OPC UA de rllib, por el mailbox)
HEADERS      += ../rllib/lib/rlmodbusclient.h ../rllib/lib/rlsiemenstcpclient.h
win32-g++:LIBS   += $$PWD/../rllib/lib/librllib.a -lws2_32 -liphlpapi -ladvapi32
win32-msvc*:LIBS += $$PWD/../rllib/lib/rllib.lib ws2_32.lib iphlpapi.lib advapi32.lib
unix:LIBS        += -L$$PWD/../rllib/lib -lrllib
unix:!macx:QMAKE_RPATHDIR += $$PWD/../rllib/lib

HEADERS      += dlgtags.h dlgtagmonitor.h buildconsole.h pvbindgen.h dlgbind.h pvtablesort.h pvimages.h doctabs.h dlghelp.h pvtaginfo.h dlgnewtag.h
SOURCES      += dlgtags.cpp dlgtagmonitor.cpp buildconsole.cpp pvbindgen.cpp dlgbind.cpp pvtablesort.cpp pvimages.cpp doctabs.cpp dlghelp.cpp pvtaginfo.cpp dlgnewtag.cpp
FORMS        += dlgtags.ui
FORMS        += dlgbind.ui   # Nestor
FORMS        += dlghelp.ui dlgnewtag.ui

# Nestor: idiomas (Qt Linguist: translations/*.ts; los .qm van dentro del ejecutable, :/i18n/)
HEADERS      += ../pvbrowser/pvblanguage.h ../pvbrowser/pvbupdate.h
SOURCES      += ../pvbrowser/pvblanguage.cpp ../pvbrowser/pvbupdate.cpp
QT           += network   # Nestor: Ayuda > Buscar actualizaciones
TRANSLATIONS += translations/pvdevelop_es.ts translations/pvdevelop_en.ts translations/pvdevelop_pt.ts translations/pvdevelop_de.ts
CONFIG       += lrelease embed_translations
# Nestor: los .qm en qm/ y rcc despues de lrelease (con make -j y una carpeta nueva, rcc empaquetaba
# los .qm antes de que existieran: qmake no ve la dependencia porque al correr todavia no estan)
LRELEASE_DIR = qm
QMAKE_DIR_REPLACE -= LRELEASE_DIR   # tambien con debug_and_release (Windows): qm/, no qm/release/
for(ts, TRANSLATIONS): rcc.depends += $$OUT_PWD/qm/$$replace($$list($$basename(ts)), \.ts$, .qm)
