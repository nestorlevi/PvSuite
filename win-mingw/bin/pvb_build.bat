@echo off
rem #########################################################################################
rem # pvb_build.bat <proyecto>: genera el Makefile y compila el proyecto con el compilador  #
rem # elegido en pvdevelop (Action > Compilador... o la barra "Compilacion").                #
rem # Lo usan pvb_make.bat (Ctrl+M) y pvb_server.bat (Ctrl+T). Devuelve errorlevel 1 si falla.#
rem #   PVB_COMPILER = msvc | mingw     PVB_JOBS = procesadores     JOM = jom.exe (opcional)  #
rem #########################################################################################
CALL "%PVBDIR%\win-mingw\bin\pvenv.bat"
if "%PVB_JOBS%"=="" set "PVB_JOBS=%NUMBER_OF_PROCESSORS%"

rem Al cambiar de compilador se borra lo generado por el otro: .qmake.stash y Makefile de qmake
rem (si no, qmake falla: "QMAKE_MSC_VER isn't set") y el .exe (si no, make no vuelve a enlazar).
rem Tambien al cambiar de MinGW (p. ej. GCC 13 -> MinGW 8.1 de 32 bits): .qmake.stash guarda la version de
rem GCC (QMAKE_GCC_MAJOR_VERSION elige las bibliotecas) y los .o de otro GCC u otros bits no enlazan.
set "PVB_KEY=%PVB_COMPILER%"
if /i "%PVB_COMPILER%"=="mingw" set "PVB_KEY=mingw;%MINGWDIR%"
set "PVB_LAST="
if exist .pvb_compiler set /p PVB_LAST=<.pvb_compiler
if /i "%PVB_LAST%"=="%PVB_KEY%" goto generate
if not "%PVB_LAST%"=="" echo Cambio de compilador (%PVB_LAST% -^> %PVB_KEY%): se borran .qmake.stash, Makefile.win*, los .o y release\%1.exe
rem uno por uno: si una ruta no existe, "del" con varios archivos no borra ninguno
for %%f in (.qmake.stash Makefile.win Makefile.win.Debug Makefile.win.Release release\%1.exe debug\%1.exe) do (
  if exist "%%f" del /q "%%f"
)
if not "%PVB_LAST%"=="" (
  if exist release\*.o del /q release\*.o
  if exist debug\*.o del /q debug\*.o
)
>.pvb_compiler echo %PVB_KEY%

:generate
fake_qmake %1.pro -o Makefile.win
if errorlevel 1 goto qmake_failed
if /i "%PVB_COMPILER%"=="msvc" goto msvc

rem ---- MinGW ----
if exist "%MINGWDIR%\bin\mingw32-make.exe" goto mingw
echo No se encontro %MINGWDIR%\bin\mingw32-make.exe
echo Configurar la carpeta de MinGW en pvdevelop: Action ^> Compilador...
exit /b 1
:mingw
mingw32-make.exe -j%PVB_JOBS% -f Makefile.win
exit /b %ERRORLEVEL%

rem ---- Visual Studio ----
:msvc
where cl.exe >nul 2>nul
if not errorlevel 1 goto msvc_ok
echo No se encontro el compilador de Visual Studio (cl.exe).
echo Configurar la carpeta de Visual Studio en pvdevelop: Action ^> Compilador...
exit /b 1
:msvc_ok
if "%JOM%"=="" goto nmake
if not exist "%JOM%" goto nmake
rem jom compila en paralelo (nmake usa un solo procesador)
"%JOM%" /NOLOGO -j %PVB_JOBS% /f Makefile.win
exit /b %ERRORLEVEL%
:nmake
nmake /nologo /f Makefile.win
exit /b %ERRORLEVEL%

:qmake_failed
echo.
echo No se pudo generar el Makefile (ver los mensajes de arriba).
exit /b 1
