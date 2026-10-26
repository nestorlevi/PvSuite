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
** Nestor: consola de compilacion de pvdevelop (ver buildconsole.h)
****************************************************************************/
#include "buildconsole.h"
#include "../pvbrowser/pvdefine.h"
#include <QPlainTextEdit>
#include <QProcess>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFontDatabase>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QScrollBar>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QDateTime>
#include <QMouseEvent>
#include <QStandardPaths>
// Qt 5 y Qt 6
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
static QPoint mousePos(const QMouseEvent *e) { return e->position().toPoint(); }
#else
static QPoint mousePos(const QMouseEvent *e) { return e->pos(); }
#endif

#ifdef PVWIN32
#include <windows.h>
#else
#include <QTemporaryFile>
#include <QThread>
#include <signal.h>
#endif

#ifndef PVWIN32
// 'texto' para sh
static QString shellQuote(const QString &s)
{
  return "'" + QString(s).replace("'", "'\\''") + "'";
}

// la primera terminal instalada y la opcion con la que ejecuta un programa
static bool terminalFor(QString *term, QString *opt)
{
  struct T { const char *name; const char *opt; };
#ifdef PVMAC
  static const T terms[] = { {"xterm", "-e"} };   // Terminal.app no recibe un programa: solo XQuartz
#else
  static const T terms[] = { {"x-terminal-emulator", "-e"}, {"qterminal", "-e"}, {"konsole", "-e"},
                             {"gnome-terminal", "--"}, {"xfce4-terminal", "-x"}, {"lxterminal", "-e"},
                             {"mate-terminal", "-x"}, {"xterm", "-e"} };
#endif
  for(const T &t : terms)
  {
    const QString path = QStandardPaths::findExecutable(t.name);
    if(path.isEmpty()) continue;
    if(term) *term = path;
    if(opt) *opt = t.opt;
    return true;
  }
  return false;
}
#endif


bool pvbStartInConsole(const QString &program, const QStringList &args, const QString &dir,
                       const QString &title, qint64 *pid)
{
  QProcess p;
  p.setWorkingDirectory(dir);
#ifdef PVWIN32
  p.setProgram(program);
  p.setArguments(args);
  p.setCreateProcessArgumentsModifier([title](QProcess::CreateProcessArguments *a) {
    a->flags |= CREATE_NEW_CONSOLE;
    a->flags &= ~DETACHED_PROCESS;
    // sin los manejadores del padre: escribe en su propia ventana
    a->startupInfo->dwFlags &= ~STARTF_USESTDHANDLES;
    a->inheritHandles = FALSE;
    static std::wstring t;
    t = title.toStdWString();
    a->startupInfo->lpTitle = (LPWSTR) t.c_str();
  });
#else
  // Nestor: la terminal que haya (Lubuntu no trae xterm). Cada una recibe el programa de otra forma:
  // se le pasa un script de un solo argumento sin espacios, que pone el titulo y ejecuta el programa
  QString term, opt;
  if(!terminalFor(&term, &opt))
  {
    p.setProgram(program);   // sin terminal: sin ventana propia
    p.setArguments(args);
    return p.startDetached(pid);
  }
  QTemporaryFile script(QDir::tempPath() + "/pvb_XXXXXX.sh");
  script.setAutoRemove(false);
  if(!script.open()) return false;
  QString cmd = shellQuote(program);
  for(const QString &a : args) cmd += " " + shellQuote(a);
  QString text = "#!/bin/sh\nrm -f \"$0\"\n";
  text += "printf '\\033]0;%s\\007' " + shellQuote(title) + "\n";
  text += "cd " + shellQuote(dir) + " || exit 1\n";
  text += "exec " + cmd + "\n";
  script.write(text.toUtf8());
  script.close();
  script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
  p.setProgram(term);
  p.setArguments(QStringList() << opt << script.fileName());
#endif
  return p.startDetached(pid);
}


bool pvbHasTerminal()
{
#ifdef PVWIN32
  return true;
#else
  return terminalFor(nullptr, nullptr);
#endif
}

int pvbKillProgram(const QString &exe)
{
  int n = 0;
#if defined(PVWIN32)
  Q_UNUSED(exe);
#elif defined(__linux__)
  // el ejecutable de cada proceso (/proc/<pid>/exe); si se volvio a enlazar figura como "<ruta> (deleted)"
  QFileInfo fi(exe);
  const QString target = fi.exists() ? fi.canonicalFilePath() : fi.absoluteFilePath();
  QList<qint64> pids;
  for(const QString &d : QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot))
  {
    bool ok = false;
    const qint64 pid = d.toLongLong(&ok);
    if(!ok) continue;
    QString t = QFileInfo("/proc/" + d + "/exe").symLinkTarget();
    if(t.endsWith(" (deleted)")) t.chop(10);
    if(t == target && ::kill((pid_t) pid, SIGTERM) == 0) pids << pid;
  }
  // que suelten el puerto antes de iniciar el nuevo
  for(int i = 0; i < 20 && !pids.isEmpty(); i++)
  {
    QThread::msleep(100);
    for(int k = pids.size() - 1; k >= 0; k--)
      if(!QFileInfo::exists("/proc/" + QString::number(pids[k]))) { pids.removeAt(k); n++; }
  }
  for(qint64 pid : pids) if(::kill((pid_t) pid, SIGKILL) == 0) n++;
#else
  // macOS: sin /proc
  n = (QProcess::execute("pkill", QStringList() << "-f" << QRegularExpression::escape(exe)) == 0) ? 1 : 0;
#endif
  return n;
}

BuildConsole::BuildConsole(QWidget *parent) : QWidget(parent), proc(nullptr), startMs(0), serverMode(false)
{
  QVBoxLayout *lay = new QVBoxLayout(this);
  lay->setContentsMargins(2, 2, 2, 2);
  lay->setSpacing(2);
  QHBoxLayout *bar = new QHBoxLayout();
  label = new QLabel(tr("Sin compilar"), this);
  buttonStop = new QPushButton(tr("Detener"), this);
  buttonStop->setEnabled(false);
  QPushButton *buttonClear = new QPushButton(tr("Limpiar"), this);
  bar->addWidget(label, 1);
  bar->addWidget(buttonStop);
  bar->addWidget(buttonClear);
  // tamanio del panel (tambien se puede arrastrar su borde)
  QToolButton *grow = new QToolButton(this), *shrink = new QToolButton(this), *undock = new QToolButton(this);
  grow->setArrowType(Qt::UpArrow);     grow->setToolTip(tr("Agrandar el panel"));
  shrink->setArrowType(Qt::DownArrow); shrink->setToolTip(tr("Achicar el panel"));
  undock->setText(QString::fromUtf8("\xe2\xa7\x89")); // dos ventanas
  undock->setToolTip(tr("Separar en una ventana / volver a ponerlo abajo"));
  bar->addWidget(grow);
  bar->addWidget(shrink);
  bar->addWidget(undock);
  connect(grow, &QToolButton::clicked, this, [this]() { emit resizeRequested(+150); });
  connect(shrink, &QToolButton::clicked, this, [this]() { emit resizeRequested(-150); });
  connect(undock, &QToolButton::clicked, this, &BuildConsole::floatRequested);
  lay->addLayout(bar);
  out = new QPlainTextEdit(this);
  out->setReadOnly(true);
  out->setMaximumBlockCount(20000);
  out->setMinimumHeight(24);   // el panel se puede achicar hasta casi solo la barra
  out->setLineWrapMode(QPlainTextEdit::NoWrap);
  out->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  out->setToolTip(tr("Doble clic en un error o advertencia: abre el archivo en esa linea"));
  out->viewport()->installEventFilter(this);
  lay->addWidget(out, 1);
  connect(buttonStop, &QPushButton::clicked, this, &BuildConsole::stop);
  connect(buttonClear, &QPushButton::clicked, this, &BuildConsole::clear);
}

bool BuildConsole::running() const
{
  return proc != nullptr && proc->state() != QProcess::NotRunning;
}

bool BuildConsole::run(const QString &title, const QString &program, const QStringList &args,
                       std::function<void(bool)> done, const QProcessEnvironment &env)
{
  if(running())
  {
    appendLine(tr("Ya hay una compilacion en curso: esperar o Detener"), "#b00000");
    return false;
  }
  clear();
  emit started();
  serverMode = false;
  doneFn = done;
  currentTitle = title;
  startMs = QDateTime::currentMSecsSinceEpoch();
  delete proc;
  proc = new QProcess(this);
  proc->setProcessChannelMode(QProcess::MergedChannels);
  proc->setWorkingDirectory(QDir::currentPath());
  if(!env.isEmpty()) proc->setProcessEnvironment(env);
  connect(proc, &QProcess::readyReadStandardOutput, this, &BuildConsole::slotOutput);
  connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int code, QProcess::ExitStatus st) {
    slotFinished(st == QProcess::NormalExit ? code : -1);
  });
  connect(proc, &QProcess::errorOccurred, this, [this, program](QProcess::ProcessError e) {
    if(e == QProcess::FailedToStart) { appendLine(tr("No se pudo ejecutar %1").arg(program), "#b00000"); slotFinished(-1); }
  });
  label->setText(tr("%1: compilando...").arg(title));
  buttonStop->setEnabled(true);
  appendLine(QString("> %1 %2").arg(program, args.join(' ')), "#606060");
  proc->start(program, args);
  return true;
}

bool BuildConsole::runServer(const QString &title, const QString &program)
{
  if(running()) return false;
  serverMode = true;
  doneFn = nullptr;
  currentTitle = title;
  startMs = QDateTime::currentMSecsSinceEpoch();
  if(proc) proc->deleteLater();   // se llama desde el final de la compilacion (senal de ese mismo proc)
  proc = new QProcess(this);
  proc->setProcessChannelMode(QProcess::MergedChannels);
  proc->setWorkingDirectory(QDir::currentPath());
  connect(proc, &QProcess::readyReadStandardOutput, this, &BuildConsole::slotOutput);
  connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int code, QProcess::ExitStatus st) {
    slotFinished(st == QProcess::NormalExit ? code : -1);
  });
  label->setText(tr("%1: en ejecucion (Detener lo cierra)").arg(title));
  buttonStop->setEnabled(true);
  appendLine(QString("> %1").arg(program), "#606060");
  proc->start(program, QStringList());
  if(!proc->waitForStarted(3000)) { serverMode = false; buttonStop->setEnabled(false); return false; }
  return true;
}

void BuildConsole::stopServer()
{
  if(!serverRunning()) return;
  proc->terminate();
  if(!proc->waitForFinished(2000)) { proc->kill(); proc->waitForFinished(1000); }
}

void BuildConsole::stop()
{
  if(!running()) return;
  if(serverMode) { stopServer(); return; }
#ifdef PVWIN32
  // cmd.exe y lo que lanzo (make, g++...): todo el arbol
  QProcess::execute("taskkill", QStringList() << "/F" << "/T" << "/PID" << QString::number(proc->processId()));
#else
  proc->kill();
#endif
}

void BuildConsole::clear()
{
  out->clear();
  msgs.clear();
  places.clear();
  pending.clear();
}

void BuildConsole::slotOutput()
{
  pending += proc->readAllStandardOutput();
  int nl;
  while((nl = pending.indexOf('\n')) >= 0)
  {
    QByteArray raw = pending.left(nl);
    pending.remove(0, nl + 1);
    if(raw.endsWith('\r')) raw.chop(1);
    // la salida de g++ es UTF-8; la de cmd, de la pagina de codigos local
    QString line = QString::fromUtf8(raw);
    if(line.contains(QChar::ReplacementCharacter)) line = QString::fromLocal8Bit(raw);
    if(serverMode) appendLine(line);   // la salida del pvserver no son errores de compilacion
    else           parseLine(line);
  }
}

void BuildConsole::slotFinished(int exitCode)
{
  if(!pending.isEmpty()) { parseLine(QString::fromLocal8Bit(pending)); pending.clear(); }
  if(serverMode)
  {
    serverMode = false;
    const QString summary = tr("%1: terminado (codigo %2)").arg(currentTitle).arg(exitCode);
    label->setText(summary);
    appendLine(summary, "#606060");
    buttonStop->setEnabled(false);
    return;
  }
  bool ok = (exitCode == 0);
  int errors = 0, warnings = 0;
  for(const BuildMessage &m : msgs) (m.error ? errors : warnings)++;
  double s = (QDateTime::currentMSecsSinceEpoch() - startMs) / 1000.0;
  QString summary = ok ? tr("%1: listo en %2 s").arg(currentTitle).arg(s, 0, 'f', 1)
                       : tr("%1: ERROR (codigo %2) en %3 s").arg(currentTitle).arg(exitCode).arg(s, 0, 'f', 1);
  if(errors || warnings) summary += tr(", %1 errores, %2 advertencias").arg(errors).arg(warnings);
  label->setText(summary);
  appendLine(summary, ok ? "#006000" : "#b00000");
  buttonStop->setEnabled(false);
  std::function<void(bool)> fn = doneFn;
  doneFn = nullptr;
  emit finished(ok);
  if(fn) fn(ok);
}

void BuildConsole::appendLine(const QString &text, const QString &color)
{
  QScrollBar *sb = out->verticalScrollBar();
  bool atEnd = sb->value() >= sb->maximum() - 2;
  QString html = text.toHtmlEscaped();
  html.replace(' ', "&nbsp;");
  if(color.isEmpty()) out->appendHtml(QString("<span>%1</span>").arg(html));
  else                out->appendHtml(QString("<span style=\"color:%1\">%2</span>").arg(color, html));
  if(atEnd) sb->setValue(sb->maximum());
}

// gcc / clang:  archivo:linea:columna: error: texto     (tambien "fatal error", "warning", "nota" se ignora)
// MSVC:         archivo(linea,columna): error C2065: texto   /  archivo(linea): warning C4100: texto
void BuildConsole::parseLine(const QString &line)
{
  static const QRegularExpression gcc("^(.+?):(\\d+):(?:(\\d+):)?\\s*(fatal error|error|warning|advertencia|Fehler|Warnung)\\b[: ]\\s*(.*)$");
  static const QRegularExpression msvc("^\\s*(.+?)\\((\\d+)(?:,(\\d+))?\\)\\s*:\\s*(fatal error|error|warning)\\s*(\\w*\\d*)\\s*:\\s*(.*)$");
  BuildMessage m;
  bool found = false;
  QRegularExpressionMatch r = gcc.match(line);
  if(r.hasMatch() && !r.captured(1).startsWith("make"))
  {
    m.file = r.captured(1); m.line = r.captured(2).toInt(); m.column = r.captured(3).toInt();
    QString kind = r.captured(4);
    m.error = !(kind == "warning" || kind == "advertencia" || kind == "Warnung");
    m.text = r.captured(5);
    found = true;
  }
  else if((r = msvc.match(line)).hasMatch())
  {
    m.file = r.captured(1); m.line = r.captured(2).toInt(); m.column = r.captured(3).toInt();
    m.error = (r.captured(4) != "warning");
    m.text = (r.captured(5).isEmpty() ? QString() : r.captured(5) + ": ") + r.captured(6);
    found = true;
  }
  if(found)
  {
    // rutas del proyecto relativas (mask1_slots.h); las de afuera (Qt, rllib) quedan como estan
    QFileInfo fi(m.file);
    QString abs = fi.isAbsolute() ? fi.absoluteFilePath() : QDir::current().absoluteFilePath(m.file);
    QString rel = QDir::current().relativeFilePath(abs);
    if(!rel.startsWith("..")) m.file = QDir::cleanPath(rel);
    msgs.append(m);
    appendLine(line, m.error ? "#c00000" : "#b06000");
    // Nestor: el indice va en la linea misma: al pasar de 20000 lineas Qt borra las primeras y los numeros cambian
    out->document()->lastBlock().setUserState(msgs.size() - 1);
    return;
  }
  // Nestor: el contexto del error tambien lleva al codigo con doble clic: "In file included from mask1.cpp:12",
  // "mask1_slots.h:40:5:   required from here", "x.h:8:6: note: declared here", MSVC "x.cpp(8): note: ...".
  // No cuenta como error ni marca el editor. Solo si el archivo existe (no se confunde con las lineas de make)
  static const QRegularExpression incl("^\\s*(?:In file included from|from)\\s+(.+?):(\\d+)(?::(\\d+))?[,:]?\\s*$");
  static const QRegularExpression ctx("^(.+?):(\\d+):(?:(\\d+):)?\\s+\\S");
  static const QRegularExpression msvcCtx("^\\s*(.+?)\\((\\d+)(?:,(\\d+))?\\)\\s*:\\s*(?:note|message)\\b");
  for(const QRegularExpression *re : { &incl, &ctx, &msvcCtx })
  {
    r = re->match(line);
    if(!r.hasMatch()) continue;
    QFileInfo fi(r.captured(1).trimmed());
    QString abs = fi.isAbsolute() ? fi.absoluteFilePath() : QDir::current().absoluteFilePath(fi.filePath());
    if(!QFileInfo(abs).isFile()) continue;
    BuildMessage p;
    QString rel = QDir::current().relativeFilePath(abs);
    p.file = rel.startsWith("..") ? abs : QDir::cleanPath(rel);
    p.line = r.captured(2).toInt();
    p.column = r.captured(3).toInt();
    p.error = false;
    places.append(p);
    appendLine(line, "#2a5d9f");
    out->document()->lastBlock().setUserState(-2 - (places.size() - 1));   // -1 = sin ubicacion
    return;
  }
  // "mask1_slots.h: In function 'int slotInit(PARAM*, DATA*)':" no trae la linea: la de la definicion
  static const QRegularExpression inFunc("^(.+?): In (?:static )?(?:member )?function '(?:[^']*?\\s)?([\\w:~]+)\\(");
  r = inFunc.match(line);
  if(r.hasMatch())
  {
    QFileInfo fi(r.captured(1).trimmed());
    QString abs = fi.isAbsolute() ? fi.absoluteFilePath() : QDir::current().absoluteFilePath(fi.filePath());
    QFile f(abs);
    if(f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
      const QRegularExpression def("(?:^|[\\s*&:])" + QRegularExpression::escape(r.captured(2).section("::", -1)) + "\\s*\\(");
      const QStringList lines = QString::fromUtf8(f.readAll()).split('\n');
      for(int i=0; i<lines.size(); i++)
      {
        const QString s = lines[i].trimmed();
        if(!def.match(lines[i]).hasMatch() || s.endsWith(';') || s.startsWith("//")) continue;   // no las llamadas ni las declaraciones
        BuildMessage p;
        QString rel = QDir::current().relativeFilePath(abs);
        p.file = rel.startsWith("..") ? abs : QDir::cleanPath(rel);
        p.line = i + 1;
        p.column = 0;
        p.error = false;
        places.append(p);
        appendLine(line, "#2a5d9f");
        out->document()->lastBlock().setUserState(-2 - (places.size() - 1));
        return;
      }
    }
  }
  appendLine(line);
}

QMap<int, bool> BuildConsole::marksFor(const QString &file) const
{
  QMap<int, bool> marks;
  for(const BuildMessage &m : msgs)
  {
    if(m.file.compare(file, Qt::CaseInsensitive) != 0 || m.line <= 0) continue;
    if(m.error || !marks.contains(m.line)) marks[m.line] = m.error || marks.value(m.line);
  }
  return marks;
}

bool BuildConsole::eventFilter(QObject *obj, QEvent *event)
{
  if(obj == out->viewport() && event->type() == QEvent::MouseButtonDblClick)
  {
    QMouseEvent *me = static_cast<QMouseEvent *>(event);
    const int index = out->cursorForPosition(mousePos(me)).block().userState();
    if(index >= 0 && index < msgs.size())
    {
      const BuildMessage &m = msgs[index];
      emit openLocation(m.file, m.line, m.column);
      return true;
    }
    const int k = -2 - index;   // contexto (ver parseLine)
    if(index <= -2 && k < places.size())
    {
      const BuildMessage &m = places[k];
      emit openLocation(m.file, m.line, m.column);
      return true;
    }
  }
  return QWidget::eventFilter(obj, event);
}

/***************************************************************************
** Nestor: lo compilado del proyecto (Limpiar proyecto y cambio de arquitectura)
***************************************************************************/
// el proyecto y sus subproyectos (subcarpetas con un .pro, p.ej. una biblioteca propia)
static QStringList buildRoots(const QString &dir)
{
  QStringList roots{ QDir(dir).absolutePath() };
  for(const QFileInfo &sub : QDir(dir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
    if(!QDir(sub.filePath()).entryList(QStringList{ "*.pro" }, QDir::Files).isEmpty()) roots << sub.filePath();
  return roots;
}

// 32 o 64 segun el encabezado del objeto (COFF de MinGW/MSVC o ELF); 0 si no se sabe
static int objectBits(const QString &file)
{
  QFile f(file);
  if(!f.open(QIODevice::ReadOnly)) return 0;
  const QByteArray h = f.read(8);
  if(h.size() < 5) return 0;
  if(h.startsWith("\x7f" "ELF")) return h[4] == 1 ? 32 : (h[4] == 2 ? 64 : 0);
  const unsigned machine = (unsigned char) h[0] | ((unsigned char) h[1] << 8);
  if(machine == 0x14c) return 32;                       // i386
  if(machine == 0x8664 || machine == 0xaa64) return 64; // x86-64, ARM64
  return 0;
}

QStringList pvbObjectsOfOtherBits(const QString &dir, int bits, int *otherBits)
{
  QStringList ret;
  if(bits != 32 && bits != 64) return ret;
  for(const QString &root : buildRoots(dir))
  {
    for(const QString &sub : { QString(), QString("/release"), QString("/debug") })
    {
      QDir d(root + sub);
      if(!d.exists()) continue;
      for(const QString &o : d.entryList(QStringList{ "*.o", "*.obj" }, QDir::Files))
      {
        const int b = objectBits(d.filePath(o));
        if(b == 0 || b == bits) continue;
        if(otherBits) *otherBits = b;
        ret << QDir(dir).relativeFilePath(d.filePath(o));
      }
    }
  }
  return ret;
}

QStringList pvbCleanTargets(const QString &dir, const QString &project)
{
  // los Makefile de qmake y de pvb_build.bat (Makefile.win), no los de otros nombres (pueden ser del usuario)
  static const QStringList files{ "Makefile", "Makefile.Debug", "Makefile.Release", "Makefile.win", "Makefile.win.Debug",
                                  "Makefile.win.Release", ".qmake.stash", ".pvb_compiler", "object_script.*",
                                  "*.o", "*.obj", "moc_*.cpp", "moc_predefs.h", "qrc_*.cpp" };
  QStringList ret;
  for(const QString &root : buildRoots(dir))
  {
    QDir d(root);
    for(const char *b : { "release", "debug" })
      if(QFileInfo(d.filePath(b)).isDir()) ret << d.filePath(b);
    for(const QString &f : d.entryList(files, QDir::Files | QDir::Hidden)) ret << d.filePath(f);
  }
#ifdef PVWIN32
  (void) project;
#else
  // Linux/macOS: el pvserver queda en la carpeta del proyecto
  if(!project.isEmpty())
  {
    QFileInfo exe(QDir(dir).filePath(project));
    if(exe.isFile() && exe.isExecutable()) ret << exe.filePath();
    if(QFileInfo(QDir(dir).filePath(project + ".app")).isDir()) ret << QDir(dir).filePath(project + ".app");
  }
#endif
  for(QString &t : ret) t = QDir(dir).relativeFilePath(t);
  return ret;
}

QStringList pvbRemoveTargets(const QString &dir, const QStringList &targets)
{
  QStringList failed;
  for(const QString &t : targets)
  {
    const QString path = QDir(dir).filePath(t);
    const bool ok = QFileInfo(path).isDir() ? QDir(path).removeRecursively() : QFile::remove(path);
    if(!ok && QFileInfo::exists(path)) failed << t;
  }
  return failed;
}
