@echo off
rem #########################################################################################
rem # pvenv.bat: prepara el compilador elegido en start_pvdevelop.bat (PVB_COMPILER).       #
rem # Lo llaman pvb_make.bat, pvb_server.bat, pvmake.bat y los pvb_make_*daemon.bat.        #
rem #########################################################################################
if /i "%PVB_COMPILER%"=="msvc" goto msvc

rem ---- MinGW ----
where gcc.exe >nul 2>nul
if not errorlevel 1 goto end
set "PATH=%MINGWDIR%\bin;%PATH%"
goto end

:msvc
rem ---- Visual Studio: cl.exe, link.exe, nmake.exe (solo la primera vez) ----
where cl.exe >nul 2>nul
if not errorlevel 1 goto end
if exist "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" goto vcvars
echo No se encontro Visual Studio en VSDIR=%VSDIR%
echo Ajustar VSDIR (o PVB_COMPILER=mingw) en %PVBDIR%\win-mingw\bin\start_pvdevelop.bat
goto end
:vcvars
rem vcvars64.bat usa vswhere.exe del instalador de Visual Studio
set "PATH=%PATH%;%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul

:end
