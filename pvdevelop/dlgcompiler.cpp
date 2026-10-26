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
** Nestor: ventana "Compilador" (ver dlgcompiler.h)
****************************************************************************/
#include "dlgcompiler.h"
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QToolButton>
#include <QGroupBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QDir>

DlgCompiler::DlgCompiler(QWidget *parent) : QDialog(parent)
{
  setWindowTitle(tr("pvdevelop: compilador"));
  QVBoxLayout *main = new QVBoxLayout(this);

#ifdef _WIN32
  QGroupBox *groupCompiler = new QGroupBox(tr("Compilador de los proyectos pvserver"), this);
  QGridLayout *grid = new QGridLayout(groupCompiler);
  grid->addWidget(new QLabel(tr("Compilador:"), groupCompiler), 0, 0);
  comboCompiler = new QComboBox(groupCompiler);
  comboCompiler->addItem("Visual Studio (MSVC)", "msvc");
  comboCompiler->addItem("MinGW", "mingw");
  markUnavailableCompilers(comboCompiler); // p. ej. MSVC en la variante mingw-www
  grid->addWidget(comboCompiler, 0, 1, 1, 3);

  editVs      = addPath(grid, 1, tr("Visual Studio 2022:"),
                        tr("Carpeta de Visual Studio 2022 (Build Tools o Community) que contiene VC\\Auxiliary\\Build\\vcvars64.bat"),
                        true, &statusVs);
  editQtMsvc  = addPath(grid, 2, tr("Qt para MSVC (opcional):"),
                        tr("Kit de Qt para MSVC, p. ej. C:\\Qt\\6.9.2\\msvc2022_64. Hace falta si el proyecto usa Qt (QT += ...).\n"
                           "Vacio: fake_qmake genera el Makefile (alcanza para un pvserver sin Qt)."),
                        true, &statusQtMsvc);
  editJom     = addPath(grid, 3, tr("jom (opcional):"),
                        tr("jom.exe compila en paralelo con MSVC (nmake usa un solo procesador). Viene con Qt Creator."),
                        false, &statusJom);
  editMingw   = addPath(grid, 4, tr("MinGW predeterminado:"),
                        tr("Carpeta de MinGW que contiene bin\\mingw32-make.exe, p. ej. C:\\Qt\\Tools\\mingw1310_64.\n"
                           "La usan los proyectos que no eligieron otra version en la barra \"Compilacion\"."),
                        true, &statusMingw);
  editQtMingw = addPath(grid, 5, tr("Qt para MinGW (opcional):"),
                        tr("Kit de Qt para MinGW, p. ej. C:\\Qt\\6.9.2\\mingw_64. Hace falta si el proyecto usa Qt."),
                        true, &statusQtMingw);
  QPushButton *buttonDetect = new QPushButton(tr("Detectar automaticamente"), groupCompiler);
  grid->addWidget(buttonDetect, 6, 1, 1, 3, Qt::AlignLeft);
  // Nestor: el proyecto abierto puede tener su propia version de MinGW (se elige en la barra "Compilacion")
  if(!CompilerConfig::projectMinGWDir().isEmpty())
  {
    QLabel *note = new QLabel(tr("Este proyecto usa %1 (%2), elegido en la barra \"Compilacion\".")
                              .arg(CompilerConfig::minGWLabel(CompilerConfig::projectMinGWDir()), CompilerConfig::projectMinGWDir()), groupCompiler);
    note->setWordWrap(true);
    grid->addWidget(note, 7, 0, 1, 4);
  }
  connect(buttonDetect, &QPushButton::clicked, this, &DlgCompiler::slotAutodetect);
  connect(comboCompiler, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DlgCompiler::slotValidate);
  main->addWidget(groupCompiler);
#else
  // Linux / macOS: el compilador es el del sistema (gcc / clang); solo el Qt de los proyectos que usan Qt
  comboCompiler = nullptr;
  editVs = editQtMsvc = editMingw = editJom = nullptr;
  statusVs = statusQtMsvc = statusMingw = statusJom = nullptr;
  QGroupBox *groupCompiler = new QGroupBox(tr("Compilador de los proyectos pvserver"), this);
  QGridLayout *grid = new QGridLayout(groupCompiler);
  editQtMingw = addPath(grid, 0, tr("Qt (opcional):"),
                        tr("Kit de Qt, p. ej. ~/Qt/6.9.2/gcc_64. Hace falta si el proyecto usa Qt (QT += ...).\n"
                           "Vacio: el qmake de Qt 6 de la distribucion (Debian/Ubuntu: qt6-base-dev; Fedora: qt6-qtbase-devel);\n"
                           "si no esta, fake_qmake genera el Makefile (alcanza para un pvserver sin Qt)."),
                        true, &statusQtMingw);
  main->addWidget(groupCompiler);
#endif

  QGroupBox *groupBuild = new QGroupBox(tr("Compilacion y editor"), this);
  QGridLayout *gridBuild = new QGridLayout(groupBuild);
  gridBuild->addWidget(new QLabel(tr("Procesadores:"), groupBuild), 0, 0);
  spinJobs = new QSpinBox(groupBuild);
  spinJobs->setRange(1, maxJobs());
  spinJobs->setToolTip(tr("Archivos que se compilan a la vez (make -j). Esta maquina tiene %1.").arg(maxJobs()));
  gridBuild->addWidget(spinJobs, 0, 1, Qt::AlignLeft);
  checkAutocomplete = new QCheckBox(tr("Texto predictivo en el editor (Ctrl+Espacio)"), groupBuild);
  gridBuild->addWidget(checkAutocomplete, 1, 0, 1, 2);
  gridBuild->addWidget(new QLabel(tr("Sangría del editor:"), groupBuild), 2, 0);
  comboIndent = new QComboBox(groupBuild);
  for(int n : {2, 3, 4, 8}) comboIndent->addItem(tr("%1 espacios").arg(n), n);
  comboIndent->addItem(tr("Tabulación"), 0);
  comboIndent->setToolTip(tr("Lo que inserta Tab (Shift+Tab lo quita), Enter después de '{' y Formatear código.\n"
                             "Las tabulaciones que ya tenga un archivo se ven de ese ancho."));
  gridBuild->addWidget(comboIndent, 2, 1, Qt::AlignLeft);
  main->addWidget(groupBuild);

  QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  main->addWidget(buttons);

  base = CompilerConfig::load(false); // predeterminados (sin el MinGW del proyecto)
  setConfig(base);
  resize(720, sizeHint().height());
}

QLineEdit *DlgCompiler::addPath(QGridLayout *grid, int row, const QString &label, const QString &tip,
                                bool directory, QLabel **status)
{
  QWidget *parent = grid->parentWidget();
  QLabel *l = new QLabel(label, parent);
  QLineEdit *edit = new QLineEdit(parent);
  QToolButton *browse = new QToolButton(parent);
  browse->setText("...");
  *status = new QLabel(parent);
  (*status)->setMinimumWidth(20);
  l->setToolTip(tip);
  edit->setToolTip(tip);
  grid->addWidget(l, row, 0);
  grid->addWidget(edit, row, 1);
  grid->addWidget(browse, row, 2);
  grid->addWidget(*status, row, 3);
  connect(edit, &QLineEdit::textChanged, this, &DlgCompiler::slotValidate);
  connect(browse, &QToolButton::clicked, this, [this, edit, directory, label]() {
    QString path = directory
      ? QFileDialog::getExistingDirectory(this, label, edit->text())
      : QFileDialog::getOpenFileName(this, label, edit->text(), "jom.exe (jom.exe);;*.exe (*.exe)");
    if(!path.isEmpty()) edit->setText(QDir::toNativeSeparators(path));
  });
  return edit;
}

void DlgCompiler::setConfig(const CompilerConfig &c)
{
#ifdef _WIN32
  comboCompiler->setCurrentIndex(c.compiler == "mingw" ? 1 : 0);
  editVs->setText(c.vsDir);
  editQtMsvc->setText(c.qtMsvc);
  editJom->setText(c.jom);
  editMingw->setText(c.mingwDir);
  editQtMingw->setText(c.qtMingw);
#else
  editQtMingw->setText(c.qtMingw);
#endif
  spinJobs->setValue(c.jobs);
  checkAutocomplete->setChecked(c.autocomplete);
  comboIndent->setCurrentIndex(qMax(0, comboIndent->findData(c.indent)));
  slotValidate();
}

CompilerConfig DlgCompiler::config() const
{
  CompilerConfig c = base; // lo que no esta en la ventana se conserva
#ifdef _WIN32
  c.compiler = comboCompiler->currentData().toString();
  c.vsDir    = editVs->text().trimmed();
  c.qtMsvc   = editQtMsvc->text().trimmed();
  c.jom      = editJom->text().trimmed();
  c.mingwDir = editMingw->text().trimmed();
  c.qtMingw  = editQtMingw->text().trimmed();
#else
  c.qtMingw  = editQtMingw->text().trimmed();
#endif
  c.jobs         = spinJobs->value();
  c.autocomplete = checkAutocomplete->isChecked();
  c.indent       = comboIndent->currentData().toInt();
  return c;
}

void DlgCompiler::slotAutodetect()
{
  CompilerConfig c = config();
  c.autodetect(true);
  setConfig(c);
}

void DlgCompiler::slotValidate()
{
#ifdef _WIN32
  const CompilerConfig c = config();
  const bool msvc = (c.compiler == "msvc");
  struct { QLabel *label; const char *what; bool used; } items[] = {
    { statusVs,      "vs",      msvc  },
    { statusQtMsvc,  "qtmsvc",  msvc  },
    { statusJom,     "jom",     msvc  },
    { statusMingw,   "mingw",   !msvc },
    { statusQtMingw, "qtmingw", !msvc },
  };
#else
  const CompilerConfig c = config();
  struct { QLabel *label; const char *what; bool used; } items[] = { { statusQtMingw, "qtmingw", true } };
#endif
  for(const auto &it : items)
  {
    const QString problem = c.check(it.what);
    if(problem.isEmpty())
    {
      it.label->setText(it.used ? QString::fromUtf8("\u2714") : QString());
      it.label->setStyleSheet("color: green;");
      it.label->setToolTip(QString());
    }
    else
    {
      it.label->setText(QString::fromUtf8("\u2716"));
      it.label->setStyleSheet(it.used ? "color: red;" : "color: gray;");
      it.label->setToolTip(problem);
    }
  }
}
