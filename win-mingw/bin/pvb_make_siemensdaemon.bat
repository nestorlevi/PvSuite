@echo off
rem compatibilidad: ahora compila pvb_make_daemon.bat (nombre opcional, por defecto siemensdaemon)
set "N=%~1"
if "%N%"=="" set "N=siemensdaemon"
"%~dp0pvb_make_daemon.bat" "%N%"
