#######################################
# project file for pvbrowser          #
#######################################
CONFIG += warn_on release android
QT     += core gui printsupport            uitools widgets xml svg network printsupport
#QT     += core gui printsupport multimedia uitools widgets xml svg network printsupport

DEFINES += PVB_FOOTPRINT_BASIC
DEFINES += QT_DEPRECATED_WARNINGS
#QT      += core6compat
# LIBS    += /home/lehrig/Qt/6.6.2/android_armv7/lib
# LIBS    += /usr/lib64


DEFINES += QT_DEPRECATED_WARNINGS

android {
  DEFINES    += USE_ANDROID
  DEFINES    += USE_MAEMO
  DEFINES    += NO_WEBKIT
}  

unix:LIBS    += -ldl
QMAKE_LFLAGS += -static-libgcc

HEADERS       = mainwindow.h \
                dlgopt.h \
                opt.h \
                tcputil.h \
                interpreter.h \
                pvserver.h \
                MyWidgets.h \
                MyTextBrowser_without_www.h \
                qimagewidget.h \
                qdrawwidget.h \
                pvglwidget.h \
                qwtwidgets.h \
                qwtplotwidget.h \
                dlgtextbrowser.h \
                webkit_ui_dlgtextbrowser.h \
                pvdefine.h
#                dlgmybrowser.h \

SOURCES       = main.cpp \
                mainwindow.cpp \
                dlgopt.cpp \
                opt.cpp \
                tcputil.cpp \
                interpreter.cpp \
                MyWidgets.cpp \
                MyTextBrowser_without_www.cpp \
                QDrawWidget.cpp \
                QImageWidget.cpp \
                pvglwidget.cpp \
                gldecode.cpp \
                qwtplotwidget.cpp \
                dlgtextbrowser.cpp
#                dlgmybrowser.cpp

# FORMS        += dlgtextbrowser.ui
#               dlgmybrowser.ui

INCLUDEPATH    += ../qwt/src
# LIBS  += /home/lehrig/Downloads/pvb6/qwt/src/build/lib/libqwt_arm64-v8a.a
LIBS    += /home/lehrig/Downloads/pvb6/qwt/lib/libqwt_armeabi-v7a.a

#LIBS           += ../qwt/lib/libqwt.a

RESOURCES       = pvbrowser.qrc
TARGET          = pvbrowser
TEMPLATE        = app


CONFIG += mobility
MOBILITY =

# Default rules for deployment
qnx: target.path = /tmp/$$(TRAGET)/bin
else: unix:android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    android/AndroidManifest.xml \
    android/gradle/wrapper/gradle-wrapper.jar \
    android/gradlew \
    android/res/values/libs.xml \
    android/build.gradle \
    android/gradle/wrapper/gradle-wrapper.properties \
    android/gradlew.bat \
    android/AndroidManifest.xml \
    android/gradle/wrapper/gradle-wrapper.jar \
    android/gradlew \
    android/res/values/libs.xml \
    android/build.gradle \
    android/gradle/wrapper/gradle-wrapper.properties \
    android/gradlew.bat

contains(ANDROID_TARGET_ARCH,armeabi-v7a) {
    ANDROID_PACKAGE_SOURCE_DIR = \
        $$PWD/android
}
