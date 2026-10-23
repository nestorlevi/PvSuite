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
  static QString qtKitForMinGW(const QString &mingwDir); // kit de Qt compilado con esa version de GCC ("" si no hay)
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
