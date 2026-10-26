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
** Nestor: pestanas de documentos de pvdevelop (pantallas en el Designer y archivos en el editor)
**
** Hay un solo editor y un solo Designer: cada pestana recuerda que documento mostrar y MainWindow lo abre al
** elegirla (openDoc). Lo que se abre sin pedir otra pestana (lista de pantallas, botones del editor, combo
** del Designer) reemplaza al de la pestana actual; "Abrir en nueva pestana" agrega una (si ya esta abierta,
** pasa a esa). Clave de cada documento: "designer:mask3.cpp" (o el archivo de un bloque) o "editor:pvapp.h".
****************************************************************************/
#ifndef DOCTABS_H
#define DOCTABS_H

#include <QWidget>
#include <QStringList>
#include <functional>

class QTabBar;
class QToolButton;

class DocTabs : public QWidget
{
  Q_OBJECT
public:
  explicit DocTabs(QWidget *parent = nullptr);
  static QString designerKey(const QString &file) { return "designer:" + file; }
  static QString editorKey(const QString &file)   { return "editor:" + file; }
  static bool    isDesigner(const QString &key)   { return key.startsWith("designer:"); }
  static QString fileOf(const QString &key)       { return key.section(':', 1); }

  void shown(const QString &key);      // se ve key: pasa a su pestana o la actual pasa a ser key
  void openNew(const QString &key);    // en otra pestana (o la que ya lo tiene) y emite activate
  void remove(const QString &key);
  void clear();
  bool contains(const QString &key) const { return indexOf(key) >= 0; }
  QStringList keys() const;
  QString neighbor(const QString &key) const;   // la que queda al cerrar key (vacio: es la unica)
  void setModifiedMark(const QString &key, bool on);   // cambios sin guardar: un punto en lugar de la X de cerrar
  void refreshTitles();
  std::function<QString(const QString &)> title;   // texto de la pestana (MainWindow)

signals:
  void activate(const QString &key);       // el usuario eligio otra pestana
  void closeRequested(const QString &key);
  void openFileRequested();                // "+": otro archivo del proyecto
  void saveAllRequested();

private:
  int indexOf(const QString &key) const;
  void setTab(int index, const QString &key);
  void updateSaveAll();
  QTabBar *bar;
  QToolButton *saveAll;
  QStringList marked;
  bool quiet = false;
};

#endif
