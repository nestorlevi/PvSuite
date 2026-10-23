@echo off
rem compatibilidad: ahora compila pvb_make_daemon.bat (nombre opcional, por defecto ppidaemon)
set "N=%~1"
if "%N%"=="" set "N=ppidaemon"
"%~dp0pvb_make_daemon.bat" "%N%" ppi
