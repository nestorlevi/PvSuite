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
** Nestor: consola de compilacion de pvdevelop (panel abajo, se puede ocultar).
** Ejecuta la compilacion (Ctrl+M, Ctrl+T, daemons) con QProcess, sin ventanas
** aparte, muestra la salida con los errores en rojo y las advertencias en
** naranja; con doble clic en un error se abre el archivo en esa linea.
****************************************************************************/
#ifndef BUILDCONSOLE_H
#define BUILDCONSOLE_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QProcessEnvironment>
#include <QMap>
#include <QList>
#include <functional>

class QPlainTextEdit;
class QProcess;
class QLabel;
class QPushButton;

// error o advertencia del compilador
struct BuildMessage
{
  QString file;   // relativo a la carpeta del proyecto si esta adentro
  int line;
  int column;
  bool error;     // false = advertencia
  QString text;
};

//! Nestor: ejecuta un programa de consola en su PROPIA ventana (como los daemons), con titulo.
//! (QProcess::startDetached usa la consola de pvdevelop si la tiene: la de start_pvdevelop.bat)
bool pvbStartInConsole(const QString &program, const QStringList &args, const QString &dir,
                       const QString &title, qint64 *pid = nullptr);
//! Nestor: Linux / macOS: hay una terminal para pvbStartInConsole (x-terminal-emulator, qterminal, xterm...)
bool pvbHasTerminal();
//! Nestor: Linux / macOS: cierra los procesos que ejecutan ese programa (ruta completa); devuelve cuantos
int pvbKillProgram(const QString &exe);
//! Nestor: lo compilado del proyecto en dir y sus subproyectos (subcarpetas con un .pro):
//! .o/.obj de otra arquitectura que bits (32/64; otherBits = la de ellos), rutas relativas a dir
QStringList pvbObjectsOfOtherBits(const QString &dir, int bits, int *otherBits = nullptr);
//! Nestor: lo que borra "Limpiar proyecto" (release/, debug/, Makefile*, .o, ...), relativo a dir
QStringList pvbCleanTargets(const QString &dir, const QString &project);
//! Nestor: borra esos archivos y carpetas; devuelve los que no se pudieron borrar
QStringList pvbRemoveTargets(const QString &dir, const QStringList &targets);

class BuildConsole : public QWidget
{
  Q_OBJECT
public:
  explicit BuildConsole(QWidget *parent = nullptr);

  //! ejecuta program args en la carpeta actual; done(ok) al terminar (ok = codigo de salida 0)
  bool run(const QString &title, const QString &program, const QStringList &args,
           std::function<void(bool)> done = nullptr, const QProcessEnvironment &env = QProcessEnvironment());
  bool running() const;
  //! Nestor: el pvserver dentro del panel (Linux sin terminal); Detener lo cierra
  bool runServer(const QString &title, const QString &program);
  bool serverRunning() const { return serverMode && running(); }
  void stopServer();
  void appendLine(const QString &text, const QString &color = QString());
  const QList<BuildMessage> &messages() const { return msgs; }
  //! lineas con error (true) o advertencia (false) de un archivo del proyecto
  QMap<int, bool> marksFor(const QString &file) const;

signals:
  void openLocation(const QString &file, int line, int column);
  void finished(bool ok);
  void started();
  void resizeRequested(int delta);   // Nestor: agrandar (+) o achicar (-) el panel
  void floatRequested();             // separar el panel en una ventana (o volver a ponerlo abajo)

public slots:
  void stop();
  void clear();

private slots:
  void slotOutput();
  void slotFinished(int exitCode);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  void parseLine(const QString &line);

  QPlainTextEdit *out;
  QLabel *label;
  QPushButton *buttonStop;
  QProcess *proc;
  QByteArray pending;
  QList<BuildMessage> msgs;
  QList<BuildMessage> places;   // Nestor: lineas de contexto con archivo:linea (incluido desde, llamado desde, notas)
  std::function<void(bool)> doneFn;
  qint64 startMs;
  QString currentTitle;
  bool serverMode;
};

#endif
