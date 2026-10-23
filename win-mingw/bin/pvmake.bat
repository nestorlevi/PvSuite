@echo off
rem pvmake.bat <proyecto>: compila el proyecto (igual que Ctrl+M en pvdevelop)
rem pvmake.bat           : compila con el Makefile.win existente
IF [%1] == [] GOTO MAKE_ONLY
CALL "%PVBDIR%\win-mingw\bin\pvb_build.bat" %1
goto end

:MAKE_ONLY
CALL "%PVBDIR%\win-mingw\bin\pvenv.bat"
if "%PVB_JOBS%"=="" set "PVB_JOBS=%NUMBER_OF_PROCESSORS%"
if /i "%PVB_COMPILER%"=="msvc" goto msvc
mingw32-make.exe -j%PVB_JOBS% -f Makefile.win
goto end
:msvc
nmake /nologo /f Makefile.win
:end
pause
exit
