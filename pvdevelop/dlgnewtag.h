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
** Nestor: crear un tag en una direccion que no tiene ninguno (se escribio %MW1 en un campo de tag o en el
** editor). Lo agrega al .mk del daemon (o a su .csv, con tags=) y genera de nuevo su .tagmap y su .h.
****************************************************************************/
#ifndef DLGNEWTAG_H
#define DLGNEWTAG_H

#include <QDialog>
#include <QStringList>

class Ui_DlgNewTag;

class DlgNewTag : public QDialog
{
  Q_OBJECT
public:
  //! files: los .mkmodbus / .mksiemens que entienden la direccion
  DlgNewTag(const QString &address, const QStringList &files, QWidget *parent = nullptr);
  ~DlgNewTag();
  QString source() const { return src; }   //!< PLC1 (despues de aceptar)
  QString name() const;
  //! agrega el tag al daemon y lo genera; error: por que no se pudo. Lo usa accept() (y las pruebas)
  static bool addTag(const QString &file, const QString &name, const QString &address, int type, bool rw,
                     const QString &desc, QString *error);

public slots:
  void accept() override;

private slots:
  void slotDaemonChanged();

private:
  Ui_DlgNewTag *ui;
  QString src;
};

#endif
