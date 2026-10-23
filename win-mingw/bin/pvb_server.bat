@echo off
rem system("pvb_server.bat " + name);  (pvdevelop: Ctrl+T)
rem cerrar el servidor anterior: si sigue abierto no se puede reemplazar el .exe al enlazar
taskkill /F /IM %1.exe >nul 2>&1
taskkill /F /IM pvs.exe >nul 2>&1
CALL "%PVBDIR%\win-mingw\bin\pvb_build.bat" %1
if errorlevel 1 goto error
if not exist release\%1.exe goto error
release\%1
exit

:error
echo.
echo No se pudo compilar %1 (ver los mensajes de arriba). Compilador: %PVB_COMPILER%
pause
exit
