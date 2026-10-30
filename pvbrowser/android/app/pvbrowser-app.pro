# Nestor: la app pvbrowser para Android. Usa los mismos fuentes que el pvbrowser de escritorio
# (../../), con la variante sin navegador web (como pvbrowser-with-basic-footprint.pro).
PVB = $$PWD/../..

TEMPLATE  = app
TARGET    = pvbrowser
CONFIG   += warn_on release
QT       += widgets xml svg network printsupport multimedia

DEFINES  += PVB_FOOTPRINT_BASIC USE_ANDROID USE_MAEMO NO_WEBKIT

VPATH       += $$PVB
INCLUDEPATH += $$PVB $$PVB/../qwt/src

HEADERS   = mainwindow.h \
            dlgopt.h \
            opt.h \
            tcputil.h \
            interpreter.h \
            pvserver.h \
            MyWidgets.h \
            MyTextBrowser_without_www.h \
            qimagewidget.h \
            qdrawwidget.h \
            qwtwidgets.h \
            qwtplotwidget.h \
            dlgtextbrowser.h \
            webkit_ui_dlgtextbrowser.h \
            dlgmybrowser_without_www.h \
            pvdefine.h \
            pvblanguage.h \
            pvbupdate.h

SOURCES   = main.cpp \
            mainwindow.cpp \
            dlgopt.cpp \
            opt.cpp \
            tcputil.cpp \
            interpreter.cpp \
            MyWidgets.cpp \
            MyTextBrowser_without_www.cpp \
            QDrawWidget.cpp \
            QImageWidget.cpp \
            qwtplotwidget.cpp \
            dlgtextbrowser.cpp \
            dlgmybrowser_without_www.cpp \
            pvblanguage.cpp \
            pvbupdate.cpp

RESOURCES    = $$PVB/pvbrowser.qrc
TRANSLATIONS = $$PVB/translations/pvbrowser_es.ts $$PVB/translations/pvbrowser_en.ts \
               $$PVB/translations/pvbrowser_pt.ts $$PVB/translations/pvbrowser_de.ts
CONFIG      += lrelease embed_translations

# traduccion de los dialogos estandar de Qt (Si/No, Cancelar...): en Android no hay carpeta translations,
# van en los recursos (:/qttranslations, ver loadQtTranslation en pvblanguage.cpp)
QT_TR_DIR = $$[QT_INSTALL_TRANSLATIONS]
qttranslations.files  = $$QT_TR_DIR/qtbase_es.qm $$QT_TR_DIR/qtbase_pt_BR.qm $$QT_TR_DIR/qtbase_de.qm
qttranslations.base   = $$QT_TR_DIR
qttranslations.prefix = /qttranslations
RESOURCES += qttranslations

# qwt estatica de ../qwt (se compila antes, ver ../pvbrowser-android.pro)
LIBS           += -L$$OUT_PWD/../qwt/lib -lqwt_$${QT_ARCH}
PRE_TARGETDEPS += $$OUT_PWD/../qwt/lib/libqwt_$${QT_ARCH}.a

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/../package
ANDROID_VERSION_NAME = 1.0.0
ANDROID_VERSION_CODE = 1
