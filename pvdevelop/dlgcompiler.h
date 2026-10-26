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
** Nestor: ventana "Compilador": compilador (MSVC / MinGW), carpetas, cantidad
** de procesadores y texto predictivo del editor.
****************************************************************************/
#ifndef DLGCOMPILER_H
#define DLGCOMPILER_H

#include <QDialog>
#include "compilerconfig.h"

class QComboBox;
class QLineEdit;
class QLabel;
class QSpinBox;
class QCheckBox;

class DlgCompiler : public QDialog
{
  Q_OBJECT

public:
  explicit DlgCompiler(QWidget *parent = nullptr);
  CompilerConfig config() const;

private slots:
  void slotAutodetect();
  void slotValidate();

private:
  void setConfig(const CompilerConfig &c);
  QLineEdit *addPath(class QGridLayout *grid, int row, const QString &label, const QString &tip,
                     bool directory, QLabel **status);

  QComboBox *comboCompiler;
  QLineEdit *editVs, *editQtMsvc, *editMingw, *editQtMingw, *editJom;
  QLabel    *statusVs, *statusQtMsvc, *statusMingw, *statusQtMingw, *statusJom;
  QSpinBox  *spinJobs;
  QCheckBox *checkAutocomplete;
  QComboBox *comboIndent;   // Nestor: sangria del editor
  CompilerConfig base; // configuracion leida al abrir la ventana
};

#endif
