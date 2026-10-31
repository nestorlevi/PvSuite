/****************************************************************************
**
** Copyright (C) 2000-2006 Lehrig Software Engineering.
**
** This file is part of the pvbrowser project.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/
#include "../pvbrowser/pvdefine.h"
#include "dlgdaemon.h"
#include "dlgtags.h"
#include <QFileInfo>
#include <QFileDialog>
#include <QDir>
#include <qpushbutton.h>
#include <qtextedit.h>
#include <qfile.h>
#include <qtextstream.h>
#include <qmessagebox.h>
#include <QProcess>
#include <QTimer>
#include <QCoreApplication>
#include "buildconsole.h"
extern BuildConsole *g_buildConsole; // mainwindow.cpp
#ifdef PVWIN32
#include <process.h>
#endif
QString target;

#ifdef PVUNIX
// Nestor: argumento de sh -c entre comillas simples (espacios, $, comillas en la ruta)
static QString shQuote(const QString &s)
{
  return "'" + QString(s).replace("'", "'\\''") + "'";
}
#endif

// Nestor: el primer archivo del daemon en la carpeta del proyecto (por nombre); si no hay, el de siempre
static QString firstFile(const QString &ext, const QString &fallback)
{
  QStringList files = QDir::current().entryList(QStringList() << "*" + ext, QDir::Files, QDir::Name | QDir::IgnoreCase);
  return files.isEmpty() ? fallback : files.first();
}

dlgDaemon::dlgDaemon(int _what)
{
  form = new Ui_DialogDaemon;
  form->setupUi(this);
  what = _what;
  QObject::connect(form->pushButtonCompile, &QPushButton::clicked, this, &dlgDaemon::slotCompile); // Nestor was here
  QObject::connect(form->pushButtonCancel,  &QPushButton::clicked, this, &dlgDaemon::reject);
  QObject::connect(form->pushButtonClose,   &QPushButton::clicked, this, &dlgDaemon::slotClose);
  QObject::connect(form->pushButtonOpen,    &QPushButton::clicked, this, &dlgDaemon::slotOpen);
  QObject::connect(form->pushButtonNew,     &QPushButton::clicked, this, &dlgDaemon::slotNew);
  QObject::connect(form->pushButtonTags,       &QPushButton::clicked, this, &dlgDaemon::slotTags);
  QObject::connect(form->pushButtonDaemonCode, &QPushButton::clicked, this, &dlgDaemon::slotDaemonCode);
  form->pushButtonTags->setVisible(what != PPI_DAEMON);
  form->pushButtonDaemonCode->setVisible(what == OPCUA_DAEMON);
  switch(what)
  {
    case OPCUA_DAEMON: // Nestor
      setWindowTitle(tr("pvdevelop Make an OPC UA Daemon"));
      form->pushButtonCompile->setText(tr("Generar .h"));
      form->pushButtonCompile->setToolTip(tr("Genera el .h con los tags para el pvserver. El daemon es rlopcuadaemon (viene con la suite)"));
      form->labelFile->show();
      form->pushButtonOpen->show();
      load(firstFile(".mkopcua", "opcuadaemon.mkopcua"));
      break;
    case PPI_DAEMON:
      setWindowTitle(tr("pvdevelop Make a PPI Daemon"));
      form->labelFile->hide();
      form->pushButtonOpen->hide();
      load(firstFile(".mkppi", "ppidaemon.mkppi"));
      break;
    case SIEMENSTCP_DAEMON:
      setWindowTitle(tr("pvdevelop Make a SiemensTCP Daemon"));
      form->labelFile->hide();
      form->pushButtonOpen->hide();
      load(firstFile(".mksiemens", "siemensdaemon.mksiemens"));
      break;
    case MODBUS_DAEMON:
      setWindowTitle(tr("pvdevelop Make a Modbus Daemon"));
      form->labelFile->show();
      form->pushButtonOpen->show();
      load(firstFile(".mkmodbus", "modbusdaemon.mkmodbus"));
      break;
    default:
      break;
  }
}

dlgDaemon::~dlgDaemon()
{
}

QString dlgDaemon::run(const QString &file, const QString &tag)
{
  load(file);
  selectTag = tag;
  return run();
}

QString dlgDaemon::run()
{
  perhapsEditTags();
  exec();
  return "";
}

// Nestor: si el archivo ya tiene tags ("tag ..." o "tags=archivo.csv"), se abre el editor de tags;
// al cerrarlo queda esta ventana para compilar
void dlgDaemon::perhapsEditTags()
{
  if(what == PPI_DAEMON) return;
  const QStringList lines = form->textEdit->toPlainText().split('\n');
  for(const QString &line : lines)
  {
    QString l = line.trimmed();
    if(l.startsWith("tag ") || l.startsWith("tag\t") || l.startsWith("tags="))
    {
      QTimer::singleShot(0, this, &dlgDaemon::slotTags);
      return;
    }
  }
}

// Nestor: el comando que compila un daemon generado (src: el .cpp sin la extension); false si no se puede
static bool daemonBuildCommand(int what, const QString &src, PvbDaemonJob *job, QString *error)
{
  (void) error;   // solo lo usa macOS (PPI)
  job->exe = QFileInfo(src).fileName();
  job->title = job->exe;
#ifdef PVUNIX
#ifdef PVMAC
  if(what == PPI_DAEMON) { *error = QCoreApplication::translate("dlgDaemon", "libnodave is not available for OS-X"); return false; }
  const QString cmd = "g++ "+shQuote(src+".cpp")+" -o "+shQuote(job->exe)+" -I/opt/pvb/rllib/lib -L/usr/lib/ -L/usr/local/lib/ /usr/local/lib/librllib.dylib -lpthread";
#else
  const QString nodave = (what == PPI_DAEMON) ? " /opt/pvb/rllib/lib/nodave.o /opt/pvb/rllib/lib/setport.o" : "";
  const QString cmd = "g++ -c "+shQuote(src+".cpp")+" -o "+shQuote(src+".o")+" -I/opt/pvb/rllib/lib && g++ "+shQuote(src+".o")+nodave+" /usr/lib/librllib.so -lpthread -o "+shQuote(job->exe);
#endif
  job->program = "sh";
  job->args = QStringList() << "-c" << cmd;
#else
  job->exe += ".exe";
  job->program = "cmd.exe";
  job->args = QStringList() << "/C" << "pvb_make_daemon.bat" << QDir::toNativeSeparators(src);
  if(what == PPI_DAEMON) job->args << "ppi";
  job->env = QProcessEnvironment::systemEnvironment();
  job->env.insert("PVB_NOPAUSE", "1");
#endif
  return true;
}

void dlgDaemon::slotCompile()
{
  int ret = 0;
  QFileInfo f_info(filename);
  save();
  if(generate() < 0) return; // Nestor: errores (p. ej. en los tags): no se compila
  switch(what)
  {
    case OPCUA_DAEMON: // Nestor: no se compila; rlopcuadaemon lee el .mkopcua
    {
      QMessageBox box(QMessageBox::Information, "pvdevelop",
        tr("Se genero %1.h con los tags para el pvserver.\n\n"
           "En el pvserver:\n"
           "  #include \"%1.h\"\n"
           "  rlOpcUaClient %1(%1_MAILBOX, %1_SHARED_MEMORY, %1_SHARED_MEMORY_SIZE);\n"
           "  float v = %1.getReal(%1_<tag>);\n\n"
           "El daemon se inicia con:\n"
           "  rlopcuadaemon %2").arg(target).arg(f_info.fileName()), QMessageBox::Ok, this);
      QPushButton *start = box.addButton(tr("Iniciar daemon"), QMessageBox::ActionRole);
      box.exec();
      if(box.clickedButton() == start)
      {
        // Nestor: en su propia ventana, que queda abierta al terminar; sin armar un comando de texto
        // (system() en Windows no sirve con rutas con acentos, y una comilla en la ruta rompia el comando)
#ifdef PVUNIX
        ret = pvbStartInConsole("sh", QStringList() << "-c" << "rlopcuadaemon \"$1\"; read" << "sh" << f_info.absoluteFilePath(),
                                QDir::currentPath(), "OPC UA " + target) ? 0 : -1;
#else
        ret = pvbStartInConsole("cmd.exe", QStringList() << "/k" << "rlopcuadaemon" << QDir::toNativeSeparators(f_info.absoluteFilePath()),
                                QDir::currentPath(), "OPC UA " + target) ? 0 : -1;
#endif
      }
      break;
    }
    case PPI_DAEMON:
    case SIEMENSTCP_DAEMON:
    case MODBUS_DAEMON:
    {
      // Nestor: se compila en la consola de compilacion de pvdevelop (abajo, sin ventanas aparte);
      // sin consola (pvdevelop -action=...), en su propia ventana
      QString src = buildName();
      QString exe = QFileInfo(src).fileName();
      PvbDaemonJob job;
      QString error;
      if(!daemonBuildCommand(what, src, &job, &error))
      {
        QMessageBox::information(this,"pvdevelop",error,QMessageBox::Ok);
        break;
      }
#ifdef PVUNIX
      if(g_buildConsole != NULL)
      {
        g_buildConsole->run(exe, job.program, job.args);
        break;
      }
      ret = pvbStartInConsole("sh", QStringList() << "-c" << job.args.last() + " && echo compiler_finished; read", QDir::currentPath(), exe) ? 0 : -1;
#else
      if(g_buildConsole != NULL)
      {
        // si el daemon esta corriendo su .exe no se puede reemplazar: preguntar si se lo cierra
        QProcessEnvironment env = job.env;
        QProcess t;
        t.start("tasklist", QStringList() << "/nh" << "/fi" << "IMAGENAME eq " + exe + ".exe");
        t.waitForFinished(3000);
        if(QString::fromLocal8Bit(t.readAll()).contains(exe + ".exe", Qt::CaseInsensitive))
        {
          if(QMessageBox::question(this, "pvdevelop", tr("%1.exe esta en ejecucion y no se puede reemplazar.\nCerrarlo y compilar?").arg(exe))
             != QMessageBox::Yes) break;
          env.insert("PVB_KILL", "1");
        }
        g_buildConsole->run(exe, job.program, job.args, nullptr, env);
        break;
      }
      ret = pvbStartInConsole(job.program, job.args, QDir::currentPath(), exe) ? 0 : -1;
#endif
      break;
    }
    default:
      break;
  }
  if(ret < 0) return;
}

// Nestor: Cerrar guarda y compila solo si el proyecto cambio desde la ultima compilacion
void dlgDaemon::slotClose()
{
  bool changed = save();
  if(changed || needsBuild()) slotCompile();
  accept();
}

// Nestor: nombre del .cpp que genera gmodbus/gsiemens/gppi (sin extension); el .exe queda en la carpeta actual
QString dlgDaemon::buildName() const
{
  if(what == MODBUS_DAEMON || what == OPCUA_DAEMON) return target;
  QString name = (what == PPI_DAEMON) ? "ppidaemon" : "siemensdaemon";
  const QStringList lines = form->textEdit->toPlainText().split('\n');
  for(const QString &line : lines)
  {
    if(line.startsWith("target=")) name = line.mid(7).trimmed().section(' ', 0, 0);
  }
  return name;
}

// Nestor: true si falta el .exe (OPC UA: el .h) o si el proyecto o su .csv de tags son mas nuevos
bool dlgDaemon::needsBuild() const
{
  QString out = buildName();
  if(what == OPCUA_DAEMON) out += ".h";
  else
  {
    out = QFileInfo(out).fileName();
#ifndef PVUNIX
    out += ".exe";
#endif
  }
  QFileInfo fout(out);
  if(!fout.exists()) return true;
  QFileInfo fmk(filename);
  if(fmk.lastModified() > fout.lastModified()) return true;
  const QStringList lines = form->textEdit->toPlainText().split('\n');
  for(const QString &line : lines)
  {
    if(!line.startsWith("tags=")) continue;
    QString csv = line.mid(5).trimmed();
    QFileInfo fcsv(QFileInfo(csv).isAbsolute() ? csv : fmk.absoluteDir().filePath(csv));
    if(fcsv.exists() && fcsv.lastModified() > fout.lastModified()) return true;
  }
  return false;
}

QString dlgDaemon::extension() const
{
  switch(what)
  {
    case PPI_DAEMON:        return ".mkppi";
    case SIEMENSTCP_DAEMON: return ".mksiemens";
    case OPCUA_DAEMON:      return ".mkopcua";
    default:                return ".mkmodbus";
  }
}

void dlgDaemon::slotOpen()
{
   QString ext = extension();
   QString f = QFileDialog::getOpenFileName(this,"Daemon - Open File","","daemon (*" + ext + ")");
   if(!f.isEmpty())
   {
       load(QDir::toNativeSeparators(f));
       perhapsEditTags();
   }
}
void dlgDaemon::slotNew()
{
   QString ext = extension();
   QString f = QFileDialog::getSaveFileName(this,"Daemon - New File","","*" + ext);
   if(!f.isEmpty())
   {
       if(!f.endsWith(ext))
       {
           f.append(ext);
       }
       load(QDir::toNativeSeparators(f));
   }
}

// Nestor: editor de tags sobre el texto actual
void dlgDaemon::slotTags()
{
  int protocol = (what == SIEMENSTCP_DAEMON) ? rlTagList::SIEMENS :
                 (what == OPCUA_DAEMON)      ? rlTagList::OPCUA   : rlTagList::MODBUS;
  while(true)   // Nestor: el editor puede pedir otro daemon del mismo protocolo
  {
    DlgTags dlg(protocol, QFileInfo(filename).absoluteFilePath(), form->textEdit->toPlainText(), this);
    if(!selectTag.isEmpty()) { dlg.selectTag(selectTag); selectTag.clear(); }
    if(dlg.exec() == QDialog::Accepted)
    {
      form->textEdit->document()->setPlainText(dlg.resultText());
      save();
    }
    if(dlg.switchFile().isEmpty()) return;
    load(QDir::toNativeSeparators(dlg.switchFile()));
  }
}

extern QString g_daemon_error;

// OPC UA no esta en esta version: el boton queda oculto
void dlgDaemon::slotDaemonCode()
{
}

void dlgDaemon::load(QString name)
{
  filename = name;
  target = QFileInfo(name).completeBaseName(); // Nestor: sin la extension (.mkmodbus, .mksiemens, .mkopcua)
  if(what == MODBUS_DAEMON) target = name.remove(".mkmodbus"); // como antes: con la carpeta
  form->textEdit->clear();
  form->labelFile->setText(filename);
  QFile f(filename);
  if(f.open(QIODevice::ReadOnly))
  {
    QTextStream ts( &f );
    form->textEdit->document()->setPlainText( ts.readAll() );
  }
  else
  {
    // Nestor: archivo nuevo: memoria compartida y mailbox en la carpeta "automation" del proyecto, con ruta
    // relativa (el proyecto se puede mover). Los programas se ejecutan desde la carpeta del proyecto
    // (pvdevelop y pvlauncher lo hacen asi). "/" sirve en Windows, Linux y macOS.
    QFileInfo fi(filename);
    QString autoShm = "automation/shm/" + fi.completeBaseName() + ".shm";
    QString autoMbx = "automation/mbx/" + fi.completeBaseName() + ".mbx";
    switch(what)
    {
      case OPCUA_DAEMON: // Nestor
        form->textEdit->append("############################################");
        form->textEdit->append("# OPC UA daemon (rlopcuadaemon nombre.mkopcua)");
        form->textEdit->append("# pvdevelop genera nombre.h con los tags para el pvserver");
        form->textEdit->append("############################################");
        form->textEdit->append("endpoint=opc.tcp://192.168.1.10:4840");
        form->textEdit->append("# sin user: acceso anonimo");
        form->textEdit->append("#user=operador");
        form->textEdit->append("#password=secreto");
        form->textEdit->append("shared_memory=" + autoShm);
        form->textEdit->append("mailbox=" + autoMbx);
        form->textEdit->append("# ms entre muestreos de los tags sin scan");
        form->textEdit->append("publishing_interval=500");
        form->textEdit->append("# tags: boton Tags... (o en un .csv con tags=archivo.csv)");
        form->textEdit->append("# type := bool | byte | int | uint | dint | udint | real | lreal");
        form->textEdit->append("tag name=Velocidad address=\"ns=2;s=Motor.Velocidad\" type=real rw=0");
        form->textEdit->append("tag name=Marcha address=\"ns=2;s=Motor.Marcha\" type=bool rw=1");
        break;
      case PPI_DAEMON:
        form->textEdit->append("############################################");
        form->textEdit->append("# Project file for generating ppi daemon   #");
        form->textEdit->append("# mkppi name.mkppi                         #");
        form->textEdit->append("############################################");
        form->textEdit->append("shared_memory=" + autoShm);
        form->textEdit->append("mailbox=" + autoMbx);
        form->textEdit->append("tty=/dev/ttyS0");
        form->textEdit->append("baudrate=9600");
        form->textEdit->append("# daveSpeed9k | daveSpeed19k | daveSpeed187k | daveSpeed500k daveSpeed1500k | daveSpeed45k | daveSpeed93k");
        form->textEdit->append("dave_speed=daveSpeed187k");
        form->textEdit->append("idletime=50 milliseconds");
        form->textEdit->append("#eventlog host=localhost port=6000");
        form->textEdit->append("# area :=");
        form->textEdit->append("# daveSD");
        form->textEdit->append("# daveInputs");
        form->textEdit->append("# daveOutputs");
        form->textEdit->append("# daveFlags");
        form->textEdit->append("# daveDB         //data blocks");
        form->textEdit->append("# daveDI         //not tested");
        form->textEdit->append("# daveLocal      //not tested");
        form->textEdit->append("# daveV          //don't know what it is");
        form->textEdit->append("# daveCounter    //not tested");
        form->textEdit->append("# daveTimer      //not tested");
        form->textEdit->append("# cycle := area, dbnum, start, len");
        form->textEdit->append("cycle1 slave=2 area=daveDB dbnum=1 start=0  len=64");
        form->textEdit->append("cycle2 slave=2 area=daveDB dbnum=1 start=64 len=64");
        break;
      case SIEMENSTCP_DAEMON:
        form->textEdit->append("################################################");
        form->textEdit->append("# Project file for generating siemens daemon   #");
        form->textEdit->append("# mksiemens name.mksiemens                     #");
        form->textEdit->append("################################################");
        form->textEdit->append("shared_memory=" + autoShm);
        form->textEdit->append("mailbox=" + autoMbx);
        form->textEdit->append("# type := S7_200 | S7_300 | S7_400 | S5 | S7_1200 | LOGO");
        form->textEdit->append("slave=0 adr=192.168.1.101 type=S7_200 fetch_write=1 function=-1 rack_slot=-1");
        form->textEdit->append("slave=1 adr=192.168.1.102 type=S7_200 fetch_write=0 function=-1 rack_slot=-1");
        form->textEdit->append("idletime=50 milliseconds");
        form->textEdit->append("#eventlog host=localhost port=6000");
        form->textEdit->append("# ORG := ORG_DB | ORG_M | ORG_E | ORG_A | ORG_PEPA | ORG_Z | ORG_T");
        form->textEdit->append("# cycle := slave + org + dbnum + start + len");
        form->textEdit->append("cycle1 slave=0 org=ORG_E  dbnum=0 start=0 len=8");
        form->textEdit->append("cycle2 slave=0 org=ORG_A  dbnum=0 start=0 len=8");
        form->textEdit->append("cycle3 slave=0 org=ORG_DB dbnum=0 start=0 len=32");
        break;
      case MODBUS_DAEMON:
        form->textEdit->append("############################################");
        form->textEdit->append("# Project file for generating modbusdaemon #");
        form->textEdit->append("# mkmodbus name.mkmodbus                   #");
        form->textEdit->append("############################################");
        form->textEdit->append("shared_memory=" + autoShm);
        form->textEdit->append("mailbox=" + autoMbx);
        form->textEdit->append("#communication=serial");
        form->textEdit->append("tty=/dev/ttyS0");
        form->textEdit->append("baudrate=9600");
        form->textEdit->append("rtscts=1");
        form->textEdit->append("parity=0   # 0=NONE 1=ODD 2=EVEN");
        form->textEdit->append("protocol=0 # 0=RTU  1=ASCII");
        form->textEdit->append("#idletime=4  # ms antes de cada pedido; sin esta linea: 3,5 caracteres en serie, 0 con TCP");
        form->textEdit->append("#cycletime=50 # duracion minima de una vuelta de lectura en ms (por defecto 50)");
        form->textEdit->append("communication=socket");
        form->textEdit->append("tcpadr=lehrig2");
        form->textEdit->append("tcpport=502");
        form->textEdit->append("# simulacion: el daemon ejecutado con -sim se conecta aca (mismos tags y memoria compartida)");
        form->textEdit->append("#simulation_tcpadr=127.0.0.1");
        form->textEdit->append("#simulation_tcpport=502");
        form->textEdit->append("#eventlog host=localhost port=6000");
        form->textEdit->append("# function := 1=ReadCoilStatus | 2=ReadInputStatus | 3=ReadHoldingRegisters | 4=ReadInputRegisters");
        form->textEdit->append("#             1 coil per bit   | 1 input per bit   | 2 byte per register    | 2byte per register");
        form->textEdit->append("cycle1 slave=1 function=1 start_adr=0 num_register=3");
        form->textEdit->append("cycle2 slave=2 function=2 start_adr=0 num_register=3");
        form->textEdit->append("cycle3 slave=3 function=3 start_adr=0 num_register=3");
        form->textEdit->append("cycle4 slave=4 function=4 start_adr=0 num_register=3");
        form->textEdit->append("cycle5 slave=5 function=2 start_adr=0 num_register=3");
        form->textEdit->append("cycle6 slave=6 function=2 start_adr=0 num_register=3");
        form->textEdit->append("cycle7 slave=7 function=2 start_adr=0 num_register=3");
        break;
      default:  
        break;
    }
  }
  form->textEdit->document()->setModified(false);
}

bool dlgDaemon::save()
{
  QString text = form->textEdit->document()->toPlainText();
  QFile f(filename);
  if(f.open(QIODevice::ReadOnly)) // Nestor: si no cambio no se reescribe (Cerrar compila solo si cambio)
  {
    QTextStream ts(&f);
    bool same = (ts.readAll() == text);
    f.close();
    if(same)
    {
      form->textEdit->document()->setModified(false);
      return false;
    }
  }
  if(!f.open(QIODevice::WriteOnly)) return false;
  QTextStream t( &f );
  t << text;
  f.close();
  form->textEdit->document()->setModified(false);
  return true;
}

int gppi(const char *name);
int gsiemens(const char *name);
int gmodbus(const char *name);

int dlgDaemon::generate()
{
  int ret = 0;
  g_daemon_error.clear();
  // Nestor: las carpetas de la memoria compartida y del mailbox (p. ej. automation del proyecto)
  const QStringList lines = form->textEdit->toPlainText().split('\n');
  for(const QString &line : lines)
  {
    QString l = line.trimmed();
    if(!l.startsWith("shared_memory=") && !l.startsWith("mailbox=")) continue;
    QString path = l.section('=', 1).section('#', 0, 0).trimmed().remove('"');
    if(!path.isEmpty()) QDir().mkpath(QFileInfo(path).absolutePath());
  }
  // Nestor: los generadores abren el archivo con fopen (ANSI en Windows): con la ruta relativa a la carpeta
  // del proyecto no importa que la carpeta tenga acentos (el nombre del archivo es el del daemon, PLC1)
  const QString rel = QDir::current().relativeFilePath(filename);
  const QByteArray arg = (QDir::isRelativePath(rel) ? rel : filename).toUtf8();
  switch(what)
  {
    case PPI_DAEMON:
      ret = gppi(arg.constData());
      break;
    case SIEMENSTCP_DAEMON:
      ret = gsiemens(arg.constData());
      break;
    case MODBUS_DAEMON:
      ret = gmodbus(arg.constData());
      break;
    default:
      break;
  }
  if(ret < 0) // Nestor: antes un error cerraba pvdevelop (exit)
  {
    QMessageBox::warning(this, "pvdevelop", g_daemon_error.isEmpty() ? tr("No se pudo generar el daemon") : g_daemon_error);
    return -1;
  }
  return 0;
}

// Nestor: Compilar todo. Lo generado esta desactualizado si falta, o si el archivo del daemon o su .csv
// de tags (tags=) son mas nuevos
static QString daemonSetting(const QString &file, const QString &key)
{
  QFile f(file);
  if(!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
  QString ret;
  for(const QString &line : QString::fromUtf8(f.readAll()).split('\n'))
    if(line.startsWith(key + "=")) ret = line.mid(key.length() + 1).trimmed().section(' ', 0, 0);
  return ret;
}

static bool daemonOutdated(const QString &mk, const QString &out)
{
  const QFileInfo fout(out), fmk(mk);
  if(!fout.exists() || fmk.lastModified() > fout.lastModified()) return true;
  const QString csv = daemonSetting(mk, "tags");
  if(csv.isEmpty()) return false;
  const QFileInfo fcsv(QFileInfo(csv).isAbsolute() ? csv : fmk.absoluteDir().filePath(csv));
  return fcsv.exists() && fcsv.lastModified() > fout.lastModified();
}

bool pvbDaemonJobs(bool force, QList<PvbDaemonJob> *jobs, QStringList *messages)
{
  bool ok = true;
  const QDir dir = QDir::current();
  struct Kind { const char *ext; int what; };
  for(const Kind &k : { Kind{ ".mkmodbus", MODBUS_DAEMON }, Kind{ ".mksiemens", SIEMENSTCP_DAEMON }, Kind{ ".mkppi", PPI_DAEMON } })
  {
    for(const QString &mk : dir.entryList(QStringList() << QString("*") + k.ext, QDir::Files, QDir::Name))
    {
      const QByteArray arg = mk.toUtf8();   // relativo a la carpeta del proyecto (los generadores usan fopen)
      QString src = QString(mk).remove(k.ext);
      if(k.what != MODBUS_DAEMON)
      {
        src = daemonSetting(mk, "target");
        if(src.isEmpty()) src = (k.what == PPI_DAEMON) ? "ppidaemon" : "siemensdaemon";
      }
      PvbDaemonJob job;
      QString error;
      if(!daemonBuildCommand(k.what, src, &job, &error)) { *messages << mk + ": " + error; continue; }
      if(!force && !daemonOutdated(mk, job.exe)) continue;
      g_daemon_error.clear();
      const int ret = (k.what == MODBUS_DAEMON) ? gmodbus(arg.constData())
                    : (k.what == PPI_DAEMON)    ? gppi(arg.constData()) : gsiemens(arg.constData());
      if(ret < 0)
      {
        ok = false;
        *messages << mk + ": " + (g_daemon_error.isEmpty() ? QCoreApplication::translate("dlgDaemon", "No se pudo generar el daemon") : g_daemon_error);
        continue;
      }
      jobs->append(job);
    }
  }
  return ok;
}
