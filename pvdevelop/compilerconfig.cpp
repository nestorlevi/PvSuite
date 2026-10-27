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
** Nestor: configuracion de compilacion de pvdevelop (ver compilerconfig.h)
****************************************************************************/
#include "compilerconfig.h"
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QVersionNumber>
#include <QThread>
#include <QByteArray>
#include <QComboBox>
#include <QStandardItemModel>
#include <QRegularExpression>
#include <algorithm>

int maxJobs()
{
  int n = QThread::idealThreadCount();
  return (n > 0) ? n : 1;
}

static QString env(const char *name)
{
  return QString::fromLocal8Bit(qgetenv(name)).trimmed();
}

static bool fileExists(const QString &path)
{
  return !path.isEmpty() && QFileInfo(path).isFile();
}

bool compilerAvailable(const QString &compiler)
{
#ifdef _WIN32
  const QString suitedir = env("SUITEDIR");
  if(suitedir.isEmpty()) return true;
  const QString bin = suitedir + "/win-mingw/bin";
  if(!QFileInfo(bin).isDir()) return true;
  const bool msvc  = fileExists(bin + "/serverlib.lib");
  const bool mingw = fileExists(bin + "/libserverlib.a");
  if(!msvc && !mingw) return true; // instalacion incompleta: no se adivina
  return (compiler == "mingw") ? mingw : msvc;
#else
  (void) compiler;
  return true;
#endif
}

void markUnavailableCompilers(QComboBox *combo, const CompilerConfig *config)
{
  if(combo == nullptr) return;
  QStandardItemModel *model = qobject_cast<QStandardItemModel *>(combo->model());
  for(int i = 0; i < combo->count(); i++)
  {
    const QString which = combo->itemData(i).toString();
    QString problem;
    if(config != nullptr)             problem = config->compilerProblem(which);
    else if(!compilerAvailable(which)) problem = QObject::tr("no instalado en esta PV Suite (faltan sus bibliotecas en win-mingw\\bin)");
    if(model != nullptr && model->item(i) != nullptr) model->item(i)->setEnabled(problem.isEmpty());
    combo->setItemData(i, problem.isEmpty() ? QVariant() : QVariant(problem), Qt::ToolTipRole);
  }
}

#ifdef _WIN32
// Carpetas C:\Qt\<version>\<kit> que tengan bin\qmake.exe, de la version mas nueva a la mas vieja
static QString findQtKit(const QString &pattern)
{
  QDir qt("C:/Qt");
  QStringList versions = qt.entryList(QStringList() << "[0-9]*", QDir::Dirs | QDir::NoDotAndDotDot);
  std::sort(versions.begin(), versions.end(), [](const QString &a, const QString &b) {
    return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
  });
  for(const QString &v : versions)
  {
    QDir vdir(qt.filePath(v));
    const QStringList kits = vdir.entryList(QStringList() << pattern, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
    for(const QString &k : kits)
    {
      const QString dir = QDir::toNativeSeparators(vdir.filePath(k));
      if(fileExists(dir + "/bin/qmake.exe")) return dir;
    }
  }
  return QString();
}

static QString findVisualStudio()
{
  const QString pf86 = env("ProgramFiles(x86)").isEmpty() ? QString("C:\\Program Files (x86)") : env("ProgramFiles(x86)");
  const QString vswhere = pf86 + "\\Microsoft Visual Studio\\Installer\\vswhere.exe";
  if(fileExists(vswhere))
  {
    QProcess p;
    p.start(vswhere, QStringList() << "-latest" << "-products" << "*"
                                   << "-requires" << "Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
                                   << "-property" << "installationPath");
    if(p.waitForFinished(10000))
    {
      const QString path = QString::fromLocal8Bit(p.readAllStandardOutput()).split('\n').value(0).trimmed();
      if(fileExists(path + "\\VC\\Auxiliary\\Build\\vcvars64.bat")) return path;
    }
  }
  const QString pf = env("ProgramFiles").isEmpty() ? QString("C:\\Program Files") : env("ProgramFiles");
  const QStringList candidates = {
    pf86 + "\\Microsoft Visual Studio\\2022\\BuildTools",
    pf   + "\\Microsoft Visual Studio\\2022\\Community",
    pf   + "\\Microsoft Visual Studio\\2022\\Professional",
    pf   + "\\Microsoft Visual Studio\\2022\\Enterprise" };
  for(const QString &c : candidates)
  {
    if(fileExists(c + "\\VC\\Auxiliary\\Build\\vcvars64.bat")) return c;
  }
  return QString();
}

static QString findMinGW()
{
  QDir tools("C:/Qt/Tools");
  const QStringList dirs = tools.entryList(QStringList() << "mingw*_64", QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
  for(const QString &d : dirs)
  {
    const QString dir = QDir::toNativeSeparators(tools.filePath(d));
    if(fileExists(dir + "/bin/mingw32-make.exe")) return dir;
  }
  return QString();
}

static QString findJom()
{
  const QString qtc = "C:\\Qt\\Tools\\QtCreator\\bin\\jom\\jom.exe";
  if(fileExists(qtc)) return qtc;
  return QDir::toNativeSeparators(QStandardPaths::findExecutable("jom"));
}
#endif

void CompilerConfig::autodetect(bool overwrite)
{
#ifdef _WIN32
  if(overwrite || vsDir.isEmpty())    vsDir    = findVisualStudio();
  if(overwrite || qtMsvc.isEmpty())   qtMsvc   = findQtKit("msvc*_64");
  if(overwrite || mingwDir.isEmpty()) mingwDir = findMinGW();
  if(overwrite || qtMingw.isEmpty())  qtMingw  = findQtKit("mingw*_64");
  if(overwrite || jom.isEmpty())      jom      = findJom();
#else
  (void) overwrite;
#endif
}

/***************************************************************************
** MinGW de cada proyecto
***************************************************************************/
static QString projFile, projMingw, projQt, projNotice;

struct MinGWVersion { int major = -1, minor = 0, patch = 0, bits = 64; };

// por el nombre de la carpeta de Qt: mingw1310_64 = 13.1.0 de 64 bits, mingw810_32 = 8.1.0 de 32 bits
static MinGWVersion minGWVersion(const QString &dir)
{
  MinGWVersion v;
  const QString name = QFileInfo(QDir::cleanPath(dir)).fileName().toLower();
  if(name.endsWith("_32") || name.endsWith("32")) v.bits = 32;
  QRegularExpressionMatch m = QRegularExpression("^mingw(\\d{1,2})(\\d)(\\d)_(32|64)$").match(name);
  if(m.hasMatch())
  {
    v.major = m.captured(1).toInt();
    v.minor = m.captured(2).toInt();
    v.patch = m.captured(3).toInt();
  }
  return v;
}

#ifdef _WIN32
static bool isMinGW(const QString &dir)
{
  return fileExists(dir + "/bin/mingw32-make.exe");
}
#endif

QStringList CompilerConfig::findMinGWs()
{
  QStringList ret;
#ifdef _WIN32
  QDir tools("C:/Qt/Tools");
  for(const QString &d : tools.entryList(QStringList{ "mingw*" }, QDir::Dirs | QDir::NoDotAndDotDot))
  {
    const QString dir = QDir::toNativeSeparators(tools.filePath(d));
    if(isMinGW(dir)) ret << dir;
  }
  std::sort(ret.begin(), ret.end(), [](const QString &a, const QString &b) {
    const MinGWVersion x = minGWVersion(a), y = minGWVersion(b);
    if(x.major != y.major) return x.major > y.major;
    if(x.minor != y.minor) return x.minor > y.minor;
    if(x.patch != y.patch) return x.patch > y.patch;
    return x.bits > y.bits;
  });
#endif
  return ret;
}

QString CompilerConfig::minGWLabel(const QString &mingwDir)
{
  const MinGWVersion v = minGWVersion(mingwDir);
  if(v.major < 0) return QFileInfo(QDir::cleanPath(mingwDir)).fileName();
  return QObject::tr("MinGW %1.%2 (%3 bits)").arg(v.major).arg(v.minor).arg(v.bits);
}

int CompilerConfig::minGWMajor(const QString &mingwDir)
{
  return minGWVersion(mingwDir).major;
}

int CompilerConfig::minGWBits(const QString &mingwDir)
{
  // por el compilador que trae (i686 = 32 bits); si no, por el nombre de la carpeta (mingw810_32)
  if(fileExists(mingwDir + "/bin/i686-w64-mingw32-gcc.exe")) return 32;
  if(fileExists(mingwDir + "/bin/x86_64-w64-mingw32-gcc.exe")) return 64;
  const QString n = QFileInfo(QDir::cleanPath(mingwDir)).fileName();
  if(n.endsWith("32")) return 32;
  if(n.endsWith("64")) return 64;
  return 0;
}

QString CompilerConfig::qtKitForMinGW(const QString &mingwDir)
{
#ifdef _WIN32
  const MinGWVersion v = minGWVersion(mingwDir);
  if(v.major < 0) return QString();
  QDir qt("C:/Qt");
  QStringList versions = qt.entryList(QStringList() << "[0-9]*", QDir::Dirs | QDir::NoDotAndDotDot);
  std::sort(versions.begin(), versions.end(), [](const QString &a, const QString &b) {
    return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
  });
  for(const QString &ver : versions)
  {
    QDir vdir(qt.filePath(ver));
    for(const QString &k : vdir.entryList(QStringList{ "mingw*" }, QDir::Dirs | QDir::NoDotAndDotDot))
    {
      const QString dir = QDir::toNativeSeparators(vdir.filePath(k));
      if(!fileExists(dir + "/bin/qmake.exe")) continue;
      // mkspecs/qconfig.pri: QT_GCC_MAJOR_VERSION = 13, QT_ARCH = x86_64 | i386
      QFile f(dir + "/mkspecs/qconfig.pri");
      if(!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
      int major = -1, bits = 0;
      while(!f.atEnd())
      {
        const QString line = QString::fromUtf8(f.readLine()).simplified();
        if(line.startsWith("QT_GCC_MAJOR_VERSION")) major = line.section('=', 1).trimmed().toInt();
        else if(line.startsWith("QT_ARCH ") || line.startsWith("QT_ARCH=")) bits = line.section('=', 1).contains("64") ? 64 : 32;
      }
      if(major == v.major && bits == v.bits) return dir;
    }
  }
#else
  (void) mingwDir;
#endif
  return QString();
}

QString CompilerConfig::projectFile()    { return projFile; }
QString CompilerConfig::projectMinGWDir() { return projMingw; }

QString CompilerConfig::takeNotice()
{
  QString n = projNotice;
  projNotice.clear();
  return n;
}

// reescribe en el .pvproject las lineas que empiezan con keys (las demas quedan como estan) y agrega add
static bool rewriteProjectLines(const QStringList &keys, const QStringList &add)
{
  if(projFile.isEmpty()) return false;
  QStringList lines;
  QFile in(projFile);
  if(in.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    for(const QString &l : QString::fromUtf8(in.readAll()).split('\n'))
    {
      bool keep = true;
      for(const QString &k : keys) if(l.startsWith(k)) keep = false;
      if(keep) lines << l;
    }
    in.close();
  }
  while(!lines.isEmpty() && lines.last().trimmed().isEmpty()) lines.removeLast();
  lines << add;
  QFile out(projFile);
  if(!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
  out.write((lines.join('\n') + "\n").toUtf8());
  out.close();
  return true;
}

bool CompilerConfig::setProjectMinGW(const QString &mingwDir, const QString &qtMingw)
{
  QStringList add;
  if(!mingwDir.isEmpty())
  {
    add << "mingw=" + QDir::toNativeSeparators(mingwDir);
    if(!qtMingw.isEmpty()) add << "qtmingw=" + QDir::toNativeSeparators(qtMingw);
  }
  if(!rewriteProjectLines(QStringList{ "mingw=", "qtmingw=" }, add)) return false;
  projMingw = mingwDir.isEmpty() ? QString() : QDir::toNativeSeparators(mingwDir);
  projQt    = (mingwDir.isEmpty() || qtMingw.isEmpty()) ? QString() : QDir::toNativeSeparators(qtMingw);
  return true;
}

// Linux: qt=<carpeta> o qt=system; projQt guarda lo mismo (projMingw queda vacio)
bool CompilerConfig::setProjectQt(const QString &qtDir)
{
  QStringList add;
  if(!qtDir.isEmpty()) add << "qt=" + (qtDir == "system" ? qtDir : QDir::cleanPath(qtDir));
  if(!rewriteProjectLines(QStringList{ "qt=" }, add)) return false;
  projQt = add.isEmpty() ? QString() : add.first().mid(3);
  return true;
}

QString CompilerConfig::projectQtDir() { return projQt; }

QStringList CompilerConfig::findQts()
{
  QStringList ret;
#ifndef _WIN32
  for(const QString &base : QStringList{ QDir::homePath() + "/Qt", "/opt/Qt" })
  {
    QDir b(base);
    for(const QString &v : b.entryList(QStringList{ "5.*", "6.*" }, QDir::Dirs | QDir::NoDotAndDotDot))
    {
      QDir vd(b.filePath(v));
      for(const QString &k : vd.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        if(fileExists(vd.filePath(k) + "/bin/qmake")) ret << QDir::cleanPath(vd.filePath(k));
    }
  }
  std::sort(ret.begin(), ret.end(), [](const QString &a, const QString &b) {
    const QVersionNumber x = QVersionNumber::fromString(QFileInfo(a).dir().dirName());
    const QVersionNumber y = QVersionNumber::fromString(QFileInfo(b).dir().dirName());
    return x != y ? x > y : a < b;
  });
#endif
  return ret;
}

QString CompilerConfig::qtLabel(const QString &qtDir)
{
  if(qtDir.isEmpty() || qtDir == "system")
  {
    // version del qmake de Qt 6 de la distribucion (los mismos que busca fake_qmake), una sola vez
    static QString version = []() {
      for(const char *q : { "/usr/bin/qmake6", "/usr/lib/qt6/bin/qmake", "/usr/lib64/qt6/bin/qmake", "/usr/bin/qmake-qt6", "/usr/bin/qmake" })
      {
        if(!fileExists(q)) continue;
        QProcess p;
        p.start(q, QStringList{ "-query", "QT_VERSION" });
        if(p.waitForFinished(3000)) return QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();
      }
      return QString();
    }();
    return version.isEmpty() ? QObject::tr("Qt del sistema (no se encontro qmake)") : QObject::tr("Qt del sistema (%1)").arg(version);
  }
  const QFileInfo fi(QDir::cleanPath(qtDir));
  return QObject::tr("Qt %1 (%2)").arg(fi.dir().dirName(), fi.fileName());
}

void CompilerConfig::setProject(const QString &pvprojectFile)
{
  projFile = pvprojectFile;
  projMingw.clear();
  projQt.clear();
#ifndef _WIN32
  QFile q(projFile);
  if(!q.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  for(const QString &l : QString::fromUtf8(q.readAll()).split('\n'))
    if(l.startsWith("qt=")) projQt = l.mid(3).trimmed();
  q.close();
  if(!projQt.isEmpty() && projQt != "system" && !fileExists(projQt + "/bin/qmake"))
  {
    projNotice = QObject::tr("No se encuentra el Qt de este proyecto (%1): se usa el predeterminado").arg(projQt);
    projQt.clear();
  }
#endif
#ifdef _WIN32
  QFile f(projFile);
  if(!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  for(const QString &l : QString::fromUtf8(f.readAll()).split('\n'))
  {
    if(l.startsWith("mingw="))        projMingw = l.mid(6).trimmed();
    else if(l.startsWith("qtmingw=")) projQt    = l.mid(8).trimmed();
  }
  f.close();
  if(projMingw.isEmpty()) { projQt.clear(); return; }
  if(!isMinGW(projMingw))
  {
    // se actualizo MinGW (p. ej. mingw1310_64 -> mingw1420_64): la mas nueva de la misma arquitectura
    const QString old = projMingw;
    const QStringList all = findMinGWs();
    QString found;
    for(const QString &d : all)
      if(minGWVersion(d).bits == minGWVersion(old).bits) { found = d; break; }
    if(found.isEmpty())
    {
      projNotice = QObject::tr("No se encuentra el MinGW de este proyecto (%1): se usa el predeterminado").arg(old);
      projMingw.clear();
      projQt.clear();
      return;
    }
    QString qt = qtKitForMinGW(found);
    if(qt.isEmpty() && fileExists(projQt + "/bin/qmake.exe")) qt = projQt;
    setProjectMinGW(found, qt);
    projNotice = QObject::tr("No se encuentra el MinGW de este proyecto (%1): se usa %2 y se guarda en el proyecto")
                 .arg(old, minGWLabel(found));
  }
  else if(!projQt.isEmpty() && !fileExists(projQt + "/bin/qmake.exe"))
  {
    setProjectMinGW(projMingw, qtKitForMinGW(projMingw));
  }
#endif
}

CompilerConfig CompilerConfig::load(bool withProject)
{
  CompilerConfig c;
  QSettings s("pvbrowser", "pvdevelop");
  s.beginGroup("build");
  if(s.contains("compiler"))
  {
    c.compiler     = s.value("compiler").toString();
    c.vsDir        = s.value("vsDir").toString();
    c.qtMsvc       = s.value("qtMsvc").toString();
    c.mingwDir     = s.value("mingwDir").toString();
    c.qtMingw      = s.value("qtMingw").toString();
    c.jom          = s.value("jom").toString();
    c.jobs         = s.value("jobs", maxJobs()).toInt();
    c.autocomplete = s.value("autocomplete", true).toBool();
    c.indent       = qBound(0, s.value("indent", 2).toInt(), 8);
  }
  else
  {
    // Primera vez: lo que dejo start_pvdevelop.bat en el entorno y, lo que falte, deteccion automatica
    c.compiler = env("PVB_COMPILER").toLower();
    if(c.compiler != "msvc") c.compiler = "mingw"; // MinGW por defecto
    c.vsDir    = env("VSDIR");
    c.mingwDir = env("MINGWDIR");
    c.jom      = env("JOM");
    const QString qtdir = env("QTDIR");
    if(qtdir.contains("msvc", Qt::CaseInsensitive))  c.qtMsvc  = qtdir;
    if(qtdir.contains("mingw", Qt::CaseInsensitive)) c.qtMingw = qtdir;
    c.jobs = maxJobs();
    c.autocomplete = true;
    c.autodetect(false);
  }
  s.endGroup();
  if(c.jobs < 1) c.jobs = 1;
  // compilador que esta instalacion no trae (p. ej. MSVC en la variante mingw-www): usar el otro
  bool changed = false;
  if(!compilerAvailable(c.compiler))
  {
    const QString other = (c.compiler == "mingw") ? "msvc" : "mingw";
    if(compilerAvailable(other)) { c.compiler = other; changed = true; }
  }
  if(changed || !s.contains("build/compiler")) c.save(); // la deteccion (vswhere) se hace una sola vez
  if(withProject && !projMingw.isEmpty())
  {
    c.mingwDir = projMingw;
    c.qtMingw = projQt;
    c.projectMinGW = true;
  }
#ifndef _WIN32
  if(withProject && !projQt.isEmpty())
  {
    c.systemQt = projQt == "system";
    c.qtMingw = c.systemQt ? QString() : projQt;
    c.projectMinGW = true; // save() no lo guarda como predeterminado
  }
#endif
  return c;
}

void CompilerConfig::save() const
{
  QSettings s("pvbrowser", "pvdevelop");
  s.beginGroup("build");
  s.setValue("compiler", compiler);
  s.setValue("vsDir", vsDir);
  s.setValue("qtMsvc", qtMsvc);
  if(!projectMinGW) // los del proyecto se guardan en el .pvproject (setProjectMinGW)
  {
    s.setValue("mingwDir", mingwDir);
    s.setValue("qtMingw", qtMingw);
  }
  s.setValue("jom", jom);
  s.setValue("jobs", jobs);
  s.setValue("autocomplete", autocomplete);
  s.setValue("indent", indent);
  s.endGroup();
}

QString CompilerConfig::qtDir() const
{
  return (compiler == "mingw") ? qtMingw : qtMsvc;
}

QString CompilerConfig::check(const QString &what) const
{
  if(what == "vs")
    return fileExists(vsDir + "/VC/Auxiliary/Build/vcvars64.bat") ? QString() : QString("no se encuentra VC\\Auxiliary\\Build\\vcvars64.bat");
  if(what == "mingw")
    return fileExists(mingwDir + "/bin/mingw32-make.exe") ? QString() : QString("no se encuentra bin\\mingw32-make.exe");
  if(what == "qtmsvc" || what == "qtmingw")
  {
    const QString dir = (what == "qtmsvc") ? qtMsvc : qtMingw;
    if(dir.isEmpty()) return QString(); // opcional
    return (fileExists(dir + "/bin/qmake.exe") || fileExists(dir + "/bin/qmake")) ? QString() : QString("no se encuentra bin\\qmake.exe");
  }
  if(what == "jom")
    return (jom.isEmpty() || fileExists(jom)) ? QString() : QString("no se encuentra el archivo");
  return QString();
}

QString CompilerConfig::compilerProblem(const QString &which) const
{
  if(!compilerAvailable(which))
    return QObject::tr("no instalado en esta PV Suite (faltan sus bibliotecas en win-mingw\\bin)");
  const QString problem = check(which == "mingw" ? "mingw" : "vs");
  if(problem.isEmpty()) return QString();
  if(which == "mingw")
    return QObject::tr("no se encontro MinGW en \"%1\" (%2). Configurar en Action > Compilador...").arg(mingwDir, problem);
  return QObject::tr("no se encontro Visual Studio en \"%1\" (%2). Configurar en Action > Compilador...").arg(vsDir, problem);
}

void CompilerConfig::applyEnvironment() const
{
  qputenv("PVB_JOBS", QByteArray::number(jobs));
#ifdef _WIN32
  // PATH original (el de start_pvdevelop.bat): se recalcula cada vez para no acumular carpetas
  static const QByteArray originalPath = qgetenv("PATH");

  qputenv("PVB_COMPILER", compiler.toLocal8Bit());
  qputenv("VSDIR",        vsDir.toLocal8Bit());
  qputenv("MINGWDIR",     mingwDir.toLocal8Bit());
  if(jom.isEmpty()) qunsetenv("JOM");
  else              qputenv("JOM", jom.toLocal8Bit());

  QByteArray path = originalPath;
  if(compiler == "mingw" && !mingwDir.isEmpty()) path = QDir::toNativeSeparators(mingwDir + "/bin").toLocal8Bit() + ";" + path;
  const QString qt = qtDir();
  if(qt.isEmpty())
  {
    qunsetenv("QTDIR");  // fake_qmake genera el Makefile
  }
  else
  {
    qputenv("QTDIR", qt.toLocal8Bit()); // fake_qmake usa el qmake real de ese Qt
    path = QDir::toNativeSeparators(qt + "/bin").toLocal8Bit() + ";" + path;
  }
  qputenv("PATH", path);
#else
  // Linux / macOS: el Qt de la ventana "Compilador" (vacio: fake_qmake busca el qmake de Qt 6 de la distribucion).
  // Solo si se eligio uno: un QTDIR que ya traia el entorno se respeta.
  static const QByteArray originalQtdir = qgetenv("QTDIR");
  if(!qtMingw.isEmpty())       qputenv("QTDIR", QDir::cleanPath(qtMingw).toLocal8Bit());
  else if(systemQt || originalQtdir.isEmpty()) qunsetenv("QTDIR"); // Qt del sistema
  else                         qputenv("QTDIR", originalQtdir);
#endif
}
