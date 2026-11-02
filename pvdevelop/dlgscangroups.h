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
** Nestor: Tags... > Grupos de lectura...
** Los scan_group del .mk (nombre y ciclo en ms) y que variables se leen con cada uno (scan=<grupo>).
** Renombrar un grupo cambia el scan= de sus variables; quitarlo (o desmarcar una variable) deja sus ms.
****************************************************************************/
#ifndef DLGSCANGROUPS_H
#define DLGSCANGROUPS_H

#include <QDialog>
#include <QStringList>
#include <string>
#include <utility>
#include <vector>

class Ui_DlgScanGroups;

class DlgScanGroups : public QDialog
{
  Q_OBJECT
public:
  typedef std::vector< std::pair<std::string, int> > Groups;
  //! names: las variables; scans: el scan de cada una (ms o nombre de grupo), en el mismo orden
  DlgScanGroups(const Groups &groups, const QStringList &names, const QStringList &scans, QWidget *parent = nullptr);
  ~DlgScanGroups();
  Groups groups() const;
  QStringList scans() const { return scanOf; }

public slots:
  void accept() override;

private slots:
  void slotAdd();
  void slotRemove();
  void slotGroupSelected();
  void slotGroupChanged(int row, int column);
  void slotTagChanged();

private:
  void addGroupRow(const QString &name, int ms);
  void showTags();
  QString groupAt(int row) const;
  int msAt(int row) const;
  Ui_DlgScanGroups *ui;
  QStringList names, scanOf;
  bool filling = false;
};

#endif
