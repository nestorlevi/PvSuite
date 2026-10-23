@echo off
rem #########################################################################################
rem # This is the file start_pvdevelop.bat                                                  #
rem #                                                                                       #
rem # Configuracion del entorno de pvdevelop. Se puede editar despues de instalar.          #
rem #                                                                                       #
rem # IMPORTANTE: el compilador, sus carpetas y la cantidad de procesadores se eligen en     #
rem # pvdevelop (Accion > Compilador... y la barra "Compilacion") y se pueden cambiar en     #
rem # cualquier momento. Los valores de abajo solo se usan la primera vez que se abre        #
rem # pvdevelop (despues manda lo elegido en esa ventana).                                  #
rem #                                                                                       #
rem # PVB_COMPILER : compilador para los proyectos pvserver en C/C++                        #
rem #    mingw = MinGW (por defecto). Ajustar MINGWDIR (debe incluir mingw32-make.exe).     #
rem #    msvc  = Visual Studio 2022 (Build Tools o Community). Ajustar VSDIR.               #
rem # QTDIR (opcional): Qt SDK del MISMO compilador para usar el qmake real.                #
rem #    Si queda vacio, fake_qmake.exe genera el Makefile (suficiente para un pvserver).   #
rem # PYDIR (opcional): Python.                                                             #
rem # Si solo se desarrolla en Lua no hace falta ningun compilador.                         #
rem #                                                                                       #
rem # Las variables se pueden definir tambien en el sistema (Panel de control): los         #
rem # valores de abajo solo se usan si no estan definidas.                                  #
rem #########################################################################################

if "%PVB_COMPILER%"=="" set "PVB_COMPILER=mingw"
if "%VSDIR%"==""        set "VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
rem Visual Studio Community:  set "VSDIR=C:\Program Files\Microsoft Visual Studio\2022\Community"
if "%MINGWDIR%"==""     set "MINGWDIR=C:\Qt\Tools\mingw1310_64"
rem Qt SDK opcional (MSVC: C:\Qt\6.9.2\msvc2022_64   MinGW: C:\Qt\6.9.2\mingw_64)
rem set "QTDIR=C:\Qt\6.9.2\mingw_64"
if "%PYDIR%"==""        set "PYDIR=C:\Python25"

rem ######### Compatibility with pvbrowser qt5 #############
set "PVBDIR=%SUITEDIR%"

set "PATH=%SUITEDIR%\win-mingw\bin;c:\windows;c:\windows\system32"
if not "%QTDIR%"=="" set "PATH=%QTDIR%\bin;%PATH%"
set "PYTHONPATH=%SUITEDIR%\win-mingw\bin;%PYTHONPATH%"

rem Entorno del compilador elegido (cl.exe/nmake o gcc/mingw32-make)
call "%SUITEDIR%\win-mingw\bin\pvenv.bat"

echo  SUITEDIR     = %SUITEDIR%
echo  PVBDIR       = %PVBDIR%
echo  PVB_COMPILER = %PVB_COMPILER%
echo  VSDIR        = %VSDIR%
echo  MINGWDIR     = %MINGWDIR%
echo  QTDIR        = %QTDIR%
echo  PYDIR        = %PYDIR%
echo  PYTHONPATH   = %PYTHONPATH%
echo  starting pvdevelop
echo  Please verify if the above environment variables are correct on your system.
echo  If not please adjust by editing file
echo    %SUITEDIR%\win-mingw\bin\start_pvdevelop.bat
start pvdevelop6
:end
pause
