@echo off
rem compatibilidad: ahora compila pvb_make_daemon.bat (nombre opcional, por defecto modbusdaemon)
set "N=%~1"
if "%N%"=="" set "N=modbusdaemon"
"%~dp0pvb_make_daemon.bat" "%N%"
