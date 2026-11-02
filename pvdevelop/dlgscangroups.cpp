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
#include "dlgscangroups.h"
#include "ui_dlgscangroups.h"
#include "../rllib/lib/rltagdef.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>

DlgScanGroups::DlgScanGroups(const Groups &groups, const QStringList &_names, const QStringList &scans, QWidget *parent)
  : QDialog(parent), ui(new Ui_DlgScanGroups), names(_names), scanOf(scans)
{
  ui->setupUi(this);
  while(scanOf.size() < names.size()) scanOf << QString();
  filling = true;
  for(const auto &g : groups) addGroupRow(QString::fromStdString(g.first), g.second);
  filling = false;
  ui->tableGroups->resizeColumnToContents(0);
  ui->tableGroups->setColumnWidth(0, qBound(120, ui->tableGroups->columnWidth(0) + 20, 260));
  connect(ui->buttonAdd, &QPushButton::clicked, this, &DlgScanGroups::slotAdd);
  connect(ui->buttonRemove, &QPushButton::clicked, this, &DlgScanGroups::slotRemove);
  connect(ui->tableGroups, &QTableWidget::itemSelectionChanged, this, &DlgScanGroups::slotGroupSelected);
  connect(ui->tableGroups, &QTableWidget::cellChanged, this, &DlgScanGroups::slotGroupChanged);
  connect(ui->listTags, &QListWidget::itemChanged, this, &DlgScanGroups::slotTagChanged);
  if(ui->tableGroups->rowCount() > 0) ui->tableGroups->selectRow(0);
  else showTags();
}

DlgScanGroups::~DlgScanGroups()
{
  delete ui;
}

void DlgScanGroups::addGroupRow(const QString &name, int ms)
{
  QTableWidget *t = ui->tableGroups;
  int row = t->rowCount();
  t->insertRow(row);
  QTableWidgetItem *n = new QTableWidgetItem(name);
  n->setData(Qt::UserRole, name);   // el nombre anterior (para renombrar sus variables)
  t->setItem(row, 0, n);
  QTableWidgetItem *m = new QTableWidgetItem(QString::number(ms));
  m->setTextAlignment(Qt::AlignCenter);
  t->setItem(row, 1, m);
}

QString DlgScanGroups::groupAt(int row) const
{
  QTableWidgetItem *it = ui->tableGroups->item(row, 0);
  return it ? it->data(Qt::UserRole).toString() : QString();
}

int DlgScanGroups::msAt(int row) const
{
  QTableWidgetItem *it = ui->tableGroups->item(row, 1);
  bool ok = false;
  int ms = it ? it->text().trimmed().toInt(&ok) : -1;
  return (ok && ms >= 0) ? ms : -1;
}

DlgScanGroups::Groups DlgScanGroups::groups() const
{
  Groups g;
  for(int r=0; r<ui->tableGroups->rowCount(); r++) g.push_back(std::make_pair(groupAt(r).toStdString(), qMax(0, msAt(r))));
  return g;
}

void DlgScanGroups::showTags()
{
  int row = ui->tableGroups->currentRow();
  QString g = (row >= 0) ? groupAt(row) : QString();
  filling = true;
  ui->listTags->clear();
  for(int i=0; i<names.size(); i++)
  {
    const QString &s = scanOf[i];
    bool isGroup = !s.isEmpty() && (s[0].isLetter() || s[0] == '_');
    QString how = isGroup ? tr("grupo %1").arg(s) : tr("%1 ms").arg(s.isEmpty() ? QString("0") : s);
    QListWidgetItem *it = new QListWidgetItem(QString("%1   (%2)").arg(names[i], how), ui->listTags);
    if(g.isEmpty()) it->setFlags(it->flags() & ~Qt::ItemIsUserCheckable);
    else
    {
      it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
      it->setCheckState((s == g) ? Qt::Checked : Qt::Unchecked);
    }
    it->setData(Qt::UserRole, i);
  }
  filling = false;
  ui->buttonRemove->setEnabled(!g.isEmpty());
}

void DlgScanGroups::slotGroupSelected()
{
  showTags();
}

void DlgScanGroups::slotTagChanged()
{
  if(filling) return;
  int row = ui->tableGroups->currentRow();
  if(row < 0) return;
  QString g = groupAt(row);
  for(int k=0; k<ui->listTags->count(); k++)
  {
    QListWidgetItem *it = ui->listTags->item(k);
    int i = it->data(Qt::UserRole).toInt();
    if(it->checkState() == Qt::Checked) scanOf[i] = g;
    else if(scanOf[i] == g) scanOf[i] = QString::number(qMax(0, msAt(row)));   // sale del grupo: sigue con sus ms
  }
  QMetaObject::invokeMethod(this, [this]() { showTags(); }, Qt::QueuedConnection);   // el texto entre parentesis
}

// renombrar (columna 0) o cambiar el ciclo (columna 1)
void DlgScanGroups::slotGroupChanged(int row, int column)
{
  if(filling) return;
  QTableWidgetItem *it = ui->tableGroups->item(row, column);
  if(it == nullptr) return;
  if(column == 1)
  {
    if(msAt(row) < 0)
    {
      QMessageBox::warning(this, "pvdevelop", tr("El ciclo es un número de milisegundos (0 = en cada vuelta)"));
      filling = true; it->setText("1000"); filling = false;
    }
    return;
  }
  QString old = it->data(Qt::UserRole).toString(), name = it->text().trimmed();
  if(name == old) return;
  bool taken = false;
  for(int r=0; r<ui->tableGroups->rowCount(); r++) if(r != row && groupAt(r) == name) taken = true;
  if(!rlTagList::validName(name.toStdString()) || taken)
  {
    QMessageBox::warning(this, "pvdevelop", taken ? tr("Ya hay un grupo %1").arg(name)
                                                 : tr("Nombre no válido: letras, números y _, sin empezar con un número"));
    filling = true; it->setText(old); filling = false;
    return;
  }
  for(QString &s : scanOf) if(s == old) s = name;
  filling = true; it->setData(Qt::UserRole, name); filling = false;
  showTags();
}

void DlgScanGroups::slotAdd()
{
  QString name;
  for(int n=1; name.isEmpty(); n++)
  {
    QString c = QString("grupo%1").arg(n);
    bool taken = false;
    for(int r=0; r<ui->tableGroups->rowCount(); r++) if(groupAt(r) == c) taken = true;
    if(!taken) name = c;
  }
  filling = true;
  addGroupRow(name, 1000);
  filling = false;
  int row = ui->tableGroups->rowCount() - 1;
  ui->tableGroups->selectRow(row);
  ui->tableGroups->editItem(ui->tableGroups->item(row, 0));
}

void DlgScanGroups::slotRemove()
{
  int row = ui->tableGroups->currentRow();
  if(row < 0) return;
  QString g = groupAt(row), ms = QString::number(qMax(0, msAt(row)));
  for(QString &s : scanOf) if(s == g) s = ms;
  ui->tableGroups->removeRow(row);
  showTags();
}

void DlgScanGroups::accept()
{
  for(int r=0; r<ui->tableGroups->rowCount(); r++)
    if(msAt(r) < 0)
    {
      QMessageBox::warning(this, "pvdevelop", tr("Grupo %1: el ciclo es un número de milisegundos").arg(groupAt(r)));
      return;
    }
  QDialog::accept();
}
