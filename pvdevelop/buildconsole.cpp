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
  if(QStandardPaths::findExecutable("xterm").isEmpty())   // Nestor: macOS (sin XQuartz) o Linux sin xterm: sin ventana propia
  {
    p.setProgram(program);
    p.setArguments(args);
  }
  else
  {
    p.setProgram("xterm");
    p.setArguments(QStringList() << "-T" << title << "-e" << program << args);
  }
#endif
  return p.startDetached(pid);
}

BuildConsole::BuildConsole(QWidget *parent) : QWidget(parent), proc(nullptr), startMs(0)
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

void BuildConsole::stop()
{
  if(!running()) return;
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
    parseLine(line);
  }
}

void BuildConsole::slotFinished(int exitCode)
{
  if(!pending.isEmpty()) { parseLine(QString::fromLocal8Bit(pending)); pending.clear(); }
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
  }
  return QWidget::eventFilter(obj, event);
}
