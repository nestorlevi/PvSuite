@echo off
rem #########################################################################################
rem # pvb_make_daemon.bat nombre [ppi]: compila nombre.cpp en nombre.exe (carpeta actual)    #
rem # con MinGW o Visual Studio segun PVB_COMPILER (pvenv.bat). "ppi" agrega libnodave.      #
rem # pvdevelop lo ejecuta en su consola de compilacion con PVB_NOPAUSE=1 (sin pausa; si    #
rem # PVB_KILL=1 cierra antes el daemon en ejecucion). Solo, abre su ventana y pregunta.     #
rem #########################################################################################
setlocal
set "RESULT=1"
set "SRC=%~1"
if "%SRC%"=="" set "SRC=modbusdaemon"
for %%F in ("%SRC%") do set "EXE=%%~nF.exe"
if not defined PVB_NOPAUSE title %EXE%
CALL "%PVBDIR%\win-mingw\bin\pvenv.bat"

rem si el daemon esta en ejecucion no se puede reemplazar el .exe
if "%PVB_KILL%"=="1" goto kill
if defined PVB_NOPAUSE goto compile
tasklist /nh /fi "imagename eq %EXE%" 2>nul | find /i "%EXE%" >nul
if errorlevel 1 goto compile
echo %EXE% esta en ejecucion: hay que cerrarlo para reemplazarlo.
choice /c SN /m "Cerrar %EXE% y compilar"
if errorlevel 2 goto end
:kill
taskkill /f /im "%EXE%" >nul 2>nul
ping -n 2 127.0.0.1 >nul

:compile
if /i "%PVB_COMPILER%"=="msvc" goto msvc
rem librllib.a del mismo GCC: MinGW 8.1 (proyectos con Qt 5.15) usa la de win-mingw\bin\mingw81_64
rem (mingw81_32 con el MinGW 8.1 de 32 bits: gcc -dumpmachine = i686-w64-mingw32)
set "RLLIB=%PVBDIR%\win-mingw\bin\librllib.a"
set "GCCVER="
set "GCCMACH="
for /f %%v in ('gcc -dumpversion 2^>nul') do set "GCCVER=%%v"
for /f %%m in ('gcc -dumpmachine 2^>nul') do set "GCCMACH=%%m"
if "%GCCVER:~0,2%"=="8." set "RLLIB=%PVBDIR%\win-mingw\bin\mingw81_64\librllib.a"
if "%GCCVER:~0,2%"=="8." if /i "%GCCMACH:~0,4%"=="i686" set "RLLIB=%PVBDIR%\win-mingw\bin\mingw81_32\librllib.a"
if /i not "%~2"=="ppi" goto mingw
rem nodave.c (version de Windows, incluye setPort)
gcc -c -O2 -D_WIN32 -DBCCWIN "%PVBDIR%\win-mingw\nodave.c" "-I%PVBDIR%\win-mingw" "-I%PVBDIR%\rllib\lib" -o nodave_mingw.o
if errorlevel 1 goto error
echo g++ "%SRC%.cpp" -o "%EXE%"
g++ "%SRC%.cpp" nodave_mingw.o "%RLLIB%" -lws2_32 -ladvapi32 "-I%PVBDIR%\rllib\lib" -static-libgcc -o "%EXE%"
goto result
:mingw
echo g++ "%SRC%.cpp" -o "%EXE%"
g++ "%SRC%.cpp" "%RLLIB%" -lws2_32 "-I%PVBDIR%\rllib\lib" -ladvapi32 -static-libgcc -o "%EXE%"
goto result

:msvc
set "NODAVE="
if /i "%~2"=="ppi" set NODAVE=/D_WIN32 /DBCCWIN "%PVBDIR%\win-mingw\nodave.c" "/I%PVBDIR%\win-mingw"
echo cl "%SRC%.cpp" /Fe:"%EXE%"
cl /nologo /O2 /MD /EHsc "%SRC%.cpp" %NODAVE% "/I%PVBDIR%\rllib\lib" /Fe:"%EXE%" /link "%PVBDIR%\win-mingw\bin\rllib.lib" ws2_32.lib advapi32.lib user32.lib

:result
if errorlevel 1 goto error
echo.
echo %EXE%: compilado
set "RESULT=0"
goto end
:error
echo.
echo %EXE%: ERROR al compilar

:end
if defined PVB_NOPAUSE exit /b %RESULT%
pause
exit
