/****************************************************************************
**
** Copyright (C) 2026 Nestor Palomeque.
**
** This file is part of PV Suite, a SCADA suite based on the pvbrowser
** project (Copyright (C) Lehrig Software Engineering).
**
** This file is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2 of the License, or (at your
** option) any later version. See the file LICENSE.GPL.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/
/****************************************************************************
** Nestor: configuracion de compilacion de pvdevelop (compilador, carpetas,
** cantidad de procesadores) y del editor (texto predictivo).
** Se guarda con QSettings y se pasa a los .bat (pvb_build.bat, pvb_make.bat,
** pvb_server.bat, ...) como variables de entorno del proceso de pvdevelop.
****************************************************************************/
#ifndef COMPILERCONFIG_H
#define COMPILERCONFIG_H

#include <QString>

struct CompilerConfig
{
  QString compiler;   // "msvc" o "mingw" (solo Windows)
  QString vsDir;      // Visual Studio 2022 (carpeta con VC\Auxiliary\Build\vcvars64.bat)
  QString qtMsvc;     // Qt para MSVC (opcional: si esta vacio, fake_qmake genera el Makefile)
  QString mingwDir;   // MinGW (carpeta con bin\mingw32-make.exe)
  QString qtMingw;    // Qt para MinGW (opcional)
  QString jom;        // jom.exe (opcional: compila en paralelo con MSVC)
  int     jobs = 1;            // procesadores a usar al compilar
  bool    autocomplete = true; // texto predictivo en el editor
  int     indent = 2;          // sangria del editor: espacios por nivel (0 = tabulacion)
  bool    projectMinGW = false; // mingwDir/qtMingw vienen del .pvproject (save() no los guarda como predeterminados)

  // QSettings (la primera vez: entorno y deteccion automatica). withProject: con el MinGW guardado en el
  // proyecto abierto; sin el, los predeterminados (ventana Compilador)
  static CompilerConfig load(bool withProject = true);
  void save() const;

  // Windows: MinGW de cada proyecto (lineas mingw= y qtmingw= del .pvproject). setProject() se llama al leer el
  // proyecto: si la carpeta guardada ya no existe (se actualizo MinGW) toma la mas nueva de la misma arquitectura,
  // la guarda en el proyecto y deja un aviso (takeNotice). setProjectMinGW("") vuelve al predeterminado.
  static void setProject(const QString &pvprojectFile);
  static QString projectFile();
  static QString projectMinGWDir();
  static bool setProjectMinGW(const QString &mingwDir, const QString &qtMingw);
  static QString takeNotice();
  static QStringList findMinGWs();                     // C:\Qt\Tools\mingw* (32 y 64 bits), la mas nueva primero
  static QString minGWLabel(const QString &mingwDir);  // "MinGW 13.1 (64 bits)"
  static int minGWMajor(const QString &mingwDir);      // version mayor de GCC (13), -1 si no se sabe
  static int minGWBits(const QString &mingwDir);       // Nestor: 32 o 64 (0 si no se sabe)
  static QString qtKitForMinGW(const QString &mingwDir); // kit de Qt compilado con esa version de GCC ("" si no hay)
  // Linux: Qt de cada proyecto (linea qt= del .pvproject: carpeta de un Qt o "system" = el de la distribucion).
  // setProjectQt("") vuelve al predeterminado (el de Action > Compilador...)
  static bool setProjectQt(const QString &qtDir);
  static QString projectQtDir();
  static QStringList findQts();                        // ~/Qt/<version>/<kit> y /opt/Qt/<version>/<kit>, el mas nuevo primero
  static QString qtLabel(const QString &qtDir);        // "Qt 6.9.2 (gcc_64)"; "" o "system": "Qt del sistema (6.10.2)"
  bool    systemQt = false; // Linux: el proyecto pide el Qt de la distribucion aunque el predeterminado sea otro
  void autodetect(bool overwrite);   // completa las carpetas que esten vacias (o todas)
  QString qtDir() const;             // Qt del compilador elegido
  void applyEnvironment() const;     // variables de entorno para los .bat
  QString check(const QString &what) const; // "" si la carpeta sirve, o el motivo
  QString compilerProblem(const QString &which) const; // "" si "msvc"/"mingw" se puede usar, o el motivo
};

int maxJobs();                       // procesadores de la maquina

// Windows: compiladores para los que la instalacion trae las bibliotecas de pvserver
// (%SUITEDIR%\win-mingw\bin: serverlib.lib = MSVC, libserverlib.a = MinGW).
// Sin SUITEDIR o sin esa carpeta (desarrollo desde el repositorio) no se restringe nada.
bool compilerAvailable(const QString &compiler);
class QComboBox;
// Deshabilita los items "msvc"/"mingw" que no se pueden usar: sin bibliotecas y, si se pasa
// la configuracion, tambien si el compilador no esta en la carpeta configurada (barra
// "Compilacion"). La ventana Compilador no pasa la configuracion: ahi se eligen las carpetas.
void markUnavailableCompilers(QComboBox *combo, const CompilerConfig *config = nullptr);

#endif
