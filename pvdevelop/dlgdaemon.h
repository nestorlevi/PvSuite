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
#ifndef DLG_DAEMON_H
#define DLG_DAEMON_H

#include "ui_dlgdaemon.h"
#include <QProcessEnvironment>
#define PPI_DAEMON        1
#define SIEMENSTCP_DAEMON 2
#define MODBUS_DAEMON     3
#define OPCUA_DAEMON      4   // Nestor

class dlgDaemon: public QDialog
{
    Q_OBJECT

public:
    dlgDaemon(int what);
    ~dlgDaemon();
    QString run();
    QString run(const QString &file, const QString &tag);   // Nestor: ese archivo, con el tag seleccionado en el editor de tags
public slots:
    void slotCompile();
    void slotClose();
    void slotOpen();
    void slotNew();
    void slotTags();        // Nestor: editor de tags
    void slotDaemonCode();  // Nestor: OPC UA, copiar el codigo del daemon al proyecto
private:
    void load(QString name);
    bool save();                // true si el archivo cambio
    QString buildName() const;  // Nestor: nombre del .cpp generado (sin extension)
    bool needsBuild() const;    // el .exe (o el .h de OPC UA) es mas viejo que el proyecto
    int  generate();        // -1 si hubo errores (se muestran)
    QString extension() const;
    void perhapsEditTags();  // Nestor: abre el editor de tags si el archivo tiene tags
    int what;
    QString filename;
    QString selectTag;          // Nestor: run(file, tag)
    Ui_DialogDaemon *form;
};

// Nestor: Compilar todo: los daemons generados (Modbus, Siemens, PPI) desactualizados se generan de nuevo
// y devuelven el comando que los compila (en la consola de compilacion)
struct PvbDaemonJob
{
  QString title;            // nombre del daemon (PLC1)
  QString exe;              // el ejecutable que se reemplaza (Windows: PLC1.exe)
  QString program;
  QStringList args;
  QProcessEnvironment env;
};
//! force: aunque no haya cambiado nada. false si un generador dio error (en messages)
bool pvbDaemonJobs(bool force, QList<PvbDaemonJob> *jobs, QStringList *messages);

#endif

