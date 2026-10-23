@echo off
rem system("pvb_make.bat + name");  (pvdevelop: Ctrl+M)
rem si el servidor sigue abierto no se puede reemplazar el .exe al enlazar
taskkill /F /IM %1.exe >nul 2>&1
CALL "%PVBDIR%\win-mingw\bin\pvb_build.bat" %1
if errorlevel 1 goto failed
echo.
echo %1 compilado: release\%1.exe  (compilador: %PVB_COMPILER%, procesadores: %PVB_JOBS%)
goto end
:failed
echo.
echo No se pudo compilar %1 (ver los mensajes de arriba). Compilador: %PVB_COMPILER%
:end
pause
exit
