@echo off
rem ###########################################################################
rem # Genera la documentacion de PV Suite:
rem #   doc\pvslib   referencia de las funciones del pvserver (doxygen)
rem #   doc\manual\Manual_PvSuite*.pdf          manual de usuario es/en/pt/de (desde Manual_PvSuite*.html)
rem #   doc\ayuda                               ayuda integrada de pvdevelop (Ayuda > Manual), desde los manuales
rem #   doc\index.html                          portada de la documentacion
rem # Necesita doxygen (https://www.doxygen.nl) en el PATH o en la variable DOXYGEN,
rem # Microsoft Edge o Google Chrome (Chromium 131 o mas nuevo) para el PDF y Python 3 (variable PYTHON) para
rem # el estilo del manual, la ayuda integrada y la portada (doc\herramientas).
rem ###########################################################################
setlocal
cd /d "%~dp0"
if "%DOXYGEN%"=="" set "DOXYGEN=doxygen"
"%DOXYGEN%" --version >nul 2>nul
if errorlevel 1 (
  echo No se encontro doxygen ^(definir DOXYGEN=ruta\doxygen.exe^): no se regenera la referencia para programadores
  goto :sin_doxygen
)

echo ===== Referencia para programadores (doxygen)
pushd doxygen
for %%d in (pvslib) do (
  if exist "..\%%d" rmdir /s /q "..\%%d"
  echo %%d
  "%DOXYGEN%" %%d.doxyfile
  if errorlevel 1 (
    popd
    echo doxygen fallo en %%d
    exit /b 1
  )
)
popd

:sin_doxygen
echo ===== Estilo del manual, ayuda integrada y portada (Python)
if "%PYTHON%"=="" set "PYTHON=python"
"%PYTHON%" --version >nul 2>nul
if errorlevel 1 (
  echo No se encontro Python 3: no se regenera doc\ayuda ni doc\index.html ^(definir PYTHON=ruta\python.exe^)
  goto :pdf_manual
)
"%PYTHON%" herramientas\estilo_manual.py || exit /b 1
"%PYTHON%" herramientas\ayuda_html.py || exit /b 1
"%PYTHON%" herramientas\portal.py --basico || exit /b 1

:pdf_manual
echo ===== Manual de usuario (PDF)
set "BROWSER="
if exist "%ProgramFiles(x86)%\Microsoft\Edge\Application\msedge.exe" set "BROWSER=%ProgramFiles(x86)%\Microsoft\Edge\Application\msedge.exe"
if "%BROWSER%"=="" if exist "%ProgramFiles%\Google\Chrome\Application\chrome.exe" set "BROWSER=%ProgramFiles%\Google\Chrome\Application\chrome.exe"
if "%BROWSER%"=="" (
  echo No se encontro Edge ni Chrome: el PDF no se genero
  exit /b 1
)
set "DOCDIR=%~dp0"
rem Manual en espanol (Manual_PvSuite), ingles (_en), portugues (_pt) y aleman (_de)
for %%m in (Manual_PvSuite Manual_PvSuite_en Manual_PvSuite_pt Manual_PvSuite_de) do call :pdf %%m
echo.
echo Documentacion generada (abrir doc\index.html)
endlocal
goto :eof

:pdf
echo   %1.pdf
set "HTML=%DOCDIR%manual\%1.html"
set "HTML=%HTML:\=/%"
"%BROWSER%" --headless=new --disable-gpu --no-first-run "--user-data-dir=%TEMP%\pvb-doc-pdf" --no-pdf-header-footer "--print-to-pdf=%DOCDIR%manual\%1.pdf" "file:///%HTML%"
goto :eof
