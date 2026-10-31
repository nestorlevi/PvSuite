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
#include "dlgnewtag.h"
#include "ui_dlgnewtag.h"
#include "pvtaginfo.h"
#include "../rllib/lib/rltagdef.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>

int gmodbus(const char *name);
int gsiemens(const char *name);
extern QString g_daemon_error;   // gmodbus.cpp

static int protocolOf(const QString &file)
{
  return file.endsWith(".mksiemens", Qt::CaseInsensitive) ? rlTagList::SIEMENS : rlTagList::MODBUS;
}

DlgNewTag::DlgNewTag(const QString &address, const QStringList &files, QWidget *parent) : QDialog(parent)
{
  ui = new Ui_DlgNewTag;
  ui->setupUi(this);
  ui->editAddress->setText(address.trimmed());
  for(const PvbDaemonFile &d : pvbDaemonFiles())
    if(files.contains(d.file)) ui->comboDaemon->addItem(d.source + "  (" + d.file + ")", d.file);
  ui->comboType->addItem(tr("(auto)"), 0);
  ui->checkRw->setChecked(true);
  connect(ui->comboDaemon, &QComboBox::currentIndexChanged, this, &DlgNewTag::slotDaemonChanged);
  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &DlgNewTag::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &DlgNewTag::reject);
  slotDaemonChanged();
  ui->editName->setFocus();
}

DlgNewTag::~DlgNewTag()
{
  delete ui;
}

QString DlgNewTag::name() const
{
  return ui->editName->text().trimmed();
}

// los tipos del protocolo (como el editor de tags: Modbus sin byte)
void DlgNewTag::slotDaemonChanged()
{
  const int protocol = protocolOf(ui->comboDaemon->currentData().toString());
  const int old = ui->comboType->currentData().toInt();
  while(ui->comboType->count() > 1) ui->comboType->removeItem(1);
  for(int ty = RLTAG_BOOL; ty <= RLTAG_LREAL; ty++)
  {
    if(protocol == rlTagList::MODBUS && ty == RLTAG_BYTE) continue;
    ui->comboType->addItem(rlTagList::typeName(ty), ty);
  }
  ui->comboType->setCurrentIndex(qMax(0, ui->comboType->findData(old)));
}

void DlgNewTag::accept()
{
  const QString file = ui->comboDaemon->currentData().toString();
  if(file.isEmpty()) return;
  QString error;
  if(!addTag(file, name(), ui->editAddress->text().trimmed(), ui->comboType->currentData().toInt(), ui->checkRw->isChecked(),
             ui->editDesc->text().trimmed(), &error))
  {
    QMessageBox::warning(this, "pvdevelop", error);
    return;
  }
  for(const PvbDaemonFile &d : pvbDaemonFiles()) if(d.file == file) src = d.source;
  QDialog::accept();
}

bool DlgNewTag::addTag(const QString &file, const QString &name, const QString &address, int type, bool rw,
                       const QString &desc, QString *error)
{
  const int protocol = protocolOf(file);
  rlTagList list(protocol);
  list.readMkFile(QFile::encodeName(file).constData());
  static const QRegularExpression valid("^[A-Za-z_][A-Za-z0-9_]*$");
  if(!valid.match(name).hasMatch())
  {
    *error = tr("Nombre invalido: \"%1\" (letras, numeros y _; no empieza con un numero)").arg(name);
    return false;
  }
  for(const rlTagDef &t : list.tags)
    if(QString::fromStdString(t.name) == name) { *error = tr("Ya hay un tag %1 en %2").arg(name, file); return false; }
  QString line = QString("tag name=%1 address=%2").arg(name, address);
  if(type > 0) line += QString(" type=%1").arg(rlTagList::typeName(type));
  if(rw) line += " rw=1";
  if(!desc.isEmpty()) line += " desc=\"" + QString(desc).replace("\"", "\"\"") + "\"";
  const size_t errors = list.errors.size();
  list.parseTagLine(line.toUtf8().constData(), name.toStdString());
  if(list.errors.size() > errors || list.tags.empty() || QString::fromStdString(list.tags.back().name) != name)
  {
    *error = list.errors.size() > errors ? QString::fromStdString(list.errors.back()) : tr("No se pudo leer: %1").arg(line);
    return false;
  }
  std::string err;
  if(list.check(list.tags.back(), err) < 0) { *error = QString::fromStdString(err); return false; }
  const QString tagLine = QString::fromStdString(list.tagLine(list.tags.back()));
  if(!list.csvfile.empty())   // tags=archivo.csv: va al .csv (relativo al .mk)
  {
    QString csv = QString::fromStdString(list.csvfile);
    if(QFileInfo(csv).isRelative()) csv = QFileInfo(file).absoluteDir().filePath(csv);
    if(list.writeCsv(QFile::encodeName(csv).constData()) < 0) { *error = tr("No se pudo escribir %1").arg(csv); return false; }
  }
  else
  {
    QFile f(file);
    if(!f.open(QIODevice::ReadOnly)) { *error = tr("No se pudo leer %1").arg(file); return false; }
    QByteArray text = f.readAll();
    f.close();
    const QByteArray nl = text.contains("\r\n") ? "\r\n" : "\n";
    if(!text.isEmpty() && !text.endsWith('\n')) text += nl;
    text += tagLine.toUtf8() + nl;
    if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(text) != text.size()) { *error = tr("No se pudo escribir %1").arg(file); return false; }
    f.close();
  }
  // el .tagmap y el .h, para usarlo ya (el daemon se compila con Compilar todo)
  const QByteArray arg = file.toUtf8();
  if((protocol == rlTagList::SIEMENS ? gsiemens(arg.constData()) : gmodbus(arg.constData())) < 0)
  {
    *error = tr("Se agrego el tag, pero no se pudo generar el daemon: %1").arg(g_daemon_error);
    return false;
  }
  pvbTagsInvalidate();
  return true;
}
