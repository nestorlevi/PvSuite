@echo off
rem -------------------------------------------------------
rem set environment variables for running build batch files
rem adjust this
rem -------------------------------------------------------

rem set directories for needed tools
rem directories where qt and mingw are installed

:MINGW
set MINGWDIR=C:\Qt\Qt5.4.2\Tools\mingw491_32
set QTDIR=C:\Qt\Qt5.4.2
set QMAKESPEC=C:\Qt\Qt5.4.2\5.4\mingw491_32\mkspecs\win32-g++
set swigdir=C:\rl\swigwin-3.0.12
set PATH=%MINGWDIR%\bin;C:\Qt\Qt5.4.2\5.4\mingw491_32\bin;c:\windows;c:\windows\system32;%PATH%
:END

rem tools necessary for language bindings
rem SET SWIGDIR=c:\rl\swigwin-3.0.12
rem SET VTKDIR=z:\Downloads\vtk\VTK5.10.1
rem SET TCLDIR=c:\Tcl
rem SET TCLLIBPATH=c:\Tcl\Tcl8.5;z:/Downloads/vtk/bin/Wrapping/Tcl

rem change to directory with our sources
SET PVBDIR=c:\rl\cc\pvb6
c:
cd  %PVBDIR%\win-mingw

rem setup path variables
SET PATH=%PVBDIR%\win-mingw\bin;%PATH%

echo The following has been set:
echo Is this correct on your machine ?
echo If not then edit this file.
echo QTDIR=%QTDIR%
echo MINGWDIR=%MINGWDIR%
echo SWIGDIR=%SWIGDIR%
echo PATH=%PATH%
echo PVBDIR=%PVBDIR%
echo now run: 
echo build-pvserver-side.bat  // in order to build pvserver-side tools and libaries with mingw
echo                          // for 64 Bit pvbrowser client and pvdevelop with MS Visual C++ use qtcreator
echo                          // you might also use older qt based tools