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
** Nestor: monitor de tags para depurar (Daemon > Monitor de tags..., barra Proyecto).
** Lee los TARGET.tagmap del proyecto y muestra los valores de la memoria compartida de cada
** daemon, con un filtro por nombre y columnas ordenables. Si SYS.Ciclo no avanza, el daemon esta
** cerrado: todos los datos se marcan malos (la memoria guarda la ultima calidad).
** Escribe y fuerza valores por el mailbox del daemon (el .tagmap tiene que traer mailbox= y la
** direccion de cada tag: volver a compilar el daemon con esta version). Forzar = volver a escribir
** el valor cada vez que el PLC lo cambia, hasta Liberar o cerrar el monitor.
** Tambien inicia el daemon, normal o en simulacion (-sim).
****************************************************************************/
#ifndef DLGTAGMONITOR_H
#define DLGTAGMONITOR_H

#include <QDialog>
#include <QString>
#include <vector>
#include <map>
#include <memory>
#include "../rllib/lib/rltag.h"

class QComboBox;
class QLineEdit;
class QTableWidget;
class QLabel;
class QTimer;
class QCheckBox;
class QPushButton;
class rlSharedMemory;
class rlModbusClient;
class rlSiemensTCPClient;

class DlgTagMonitor : public QDialog
{
  Q_OBJECT
public:
  explicit DlgTagMonitor(QWidget *parent = nullptr);
  ~DlgTagMonitor();

private slots:
  void slotSource(int index);
  void slotFilter();
  void slotRefresh();
  void slotStart(bool simulation);
  void slotWrite(bool force);
  void slotRelease(bool all);
  void slotSelection();

private:
  struct Item { QString name, type, desc, addr; rlTag tag; };   // addr: direccion en el PLC (del .mk del daemon)
  void loadTagMap(const QString &file);
  void closeShm();
  void refresh();
  bool writeItem(int index, double value);   // por el mailbox; false si no se pudo
  int  selectedItem() const;                 // indice en items de la fila elegida (-1)
  bool confirmWrite();

  QComboBox    *comboSource;
  QLineEdit    *editFilter, *editValue;
  QCheckBox    *checkPause;
  QTableWidget *table;
  QLabel       *labelStatus;
  QPushButton  *buttonWrite, *buttonForce, *buttonRelease, *buttonReleaseAll;
  QTimer       *timer;
  std::vector<Item> items;
  std::vector<int>  shown;     // indices de items que pasan el filtro
  std::vector<QByteArray> names;
  rlSharedMemory *shm;
  std::unique_ptr<rlModbusClient> modbus;      // escrituras (tienen su propia memoria compartida)
  std::unique_ptr<rlSiemensTCPClient> s7;
  QString source, shmName, mailbox, protocol;
  int shmSize;
  int ciclo = -1;                       // indice de SYS.Ciclo en items (-1 = no hay)
  std::map<int, double> forced;         // indice en items -> valor forzado
  std::map<int, qint64> lastForceWrite;
  bool writeConfirmed = false;
};

#endif
