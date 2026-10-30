@echo off
rem Nestor: compila pvbrowser para Android y genera el APK.
rem   build_android.bat [arm64-v8a|x86_64] [release]
rem arm64-v8a = celulares (por defecto); x86_64 = emulador de la PC.
rem Variables (con valores por defecto): QTDIR, ANDROID_SDK_ROOT, ANDROID_NDK_ROOT, JAVA_HOME, PVB_ANDROID_BUILD.
rem release: APK firmado; necesita PVB_KEYSTORE, PVB_KEYALIAS, PVB_STOREPASS (y PVB_KEYPASS si es otra).
setlocal
set ABI=%1
if "%ABI%"=="" set ABI=arm64-v8a
set QTABI=%ABI:-=_%
if "%QTDIR%"=="" set QTDIR=C:\Qt\6.9.2
if "%ANDROID_SDK_ROOT%"=="" set ANDROID_SDK_ROOT=%LOCALAPPDATA%\Android\Sdk
if "%ANDROID_NDK_ROOT%"=="" set ANDROID_NDK_ROOT=%ANDROID_SDK_ROOT%\ndk\27.2.12479018
if "%JAVA_HOME%"=="" for /d %%j in ("%USERPROFILE%\.jdks\*-17*" "%USERPROFILE%\.jdks\*-21*") do set JAVA_HOME=%%j
if "%PVB_ANDROID_BUILD%"=="" set PVB_ANDROID_BUILD=%~dp0..\..\Compilar\pvbrowser-android
set BUILD=%PVB_ANDROID_BUILD%\%ABI%
set PATH=%QTDIR%\mingw_64\bin;%QTDIR%\..\Tools\mingw1310_64\bin;%JAVA_HOME%\bin;%SystemRoot%\system32;%SystemRoot%
rem PATH limpio: con el sh.exe de Git en el PATH, mingw32-make usa sh y rompe las rutas con \
set SHELL=

if not exist "%QTDIR%\android_%QTABI%\bin\qmake.bat" (echo Falta Qt para Android: %QTDIR%\android_%QTABI% & exit /b 1)
if not exist "%ANDROID_NDK_ROOT%" (echo Falta el NDK: %ANDROID_NDK_ROOT% & exit /b 1)
if not exist "%JAVA_HOME%\bin\java.exe" (echo Falta el JDK 17: definir JAVA_HOME & exit /b 1)

if not exist "%BUILD%" mkdir "%BUILD%"
cd /d "%BUILD%" || exit /b 1
call "%QTDIR%\android_%QTABI%\bin\qmake.bat" "%~dp0pvbrowser-android.pro" "ANDROID_ABIS=%ABI%" || exit /b 1
mingw32-make -j8 || exit /b 1

set DEPLOY=--android-platform android-35 --gradle
if "%PVB_KEYPASS%"=="" set PVB_KEYPASS=%PVB_STOREPASS%
if /i "%2"=="release" (
  set DEPLOY=%DEPLOY% --release --sign "%PVB_KEYSTORE%" %PVB_KEYALIAS% --storepass %PVB_STOREPASS% --keypass %PVB_KEYPASS%
)
cd app || exit /b 1
mingw32-make -f Makefile.pvbrowser-app INSTALL_ROOT=android-build install || exit /b 1
androiddeployqt --input android-pvbrowser-deployment-settings.json --output android-build %DEPLOY% || exit /b 1
echo APK en: %BUILD%\app\android-build\build\outputs\apk
endlocal
