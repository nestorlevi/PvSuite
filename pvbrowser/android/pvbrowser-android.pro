# Nestor: pvbrowser para Android (Qt 6 para Android).
# Abrir con un kit "Android Qt 6.x" o compilar con build_android.bat / build_android.sh.
# Sin navegador web integrado (Qt WebEngine no existe en Android): los enlaces se abren
# en el navegador del sistema.
TEMPLATE = subdirs
CONFIG  += ordered
!android:error("pvbrowser-android.pro es para un kit de Android")

SUBDIRS += qwt/qwt-android.pro \
           app/pvbrowser-app.pro
