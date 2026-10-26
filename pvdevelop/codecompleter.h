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
** Nestor: texto predictivo para el editor de pvdevelop.
** Sugiere funciones de pvserver (con su firma), clases de rllib, palabras
** clave de C/C++ y las palabras del archivo abierto.
** Aparece al escribir 3 letras de una palabra o con Ctrl+Espacio.
****************************************************************************/
#ifndef CODECOMPLETER_H
#define CODECOMPLETER_H

#include <QObject>
#include <QString>
#include <QTextEdit>
#include <QPointer>

class QCompleter;
class QStandardItemModel;
class CodeCompleter;

// Editor de pvdevelop: con la lista de sugerencias abierta, Enter/Tab/Esc son para la lista.
// (QCompleter primero le pasa la tecla al editor con event(); si el editor la usa, la lista no la recibe)
class CodeTextEdit : public QTextEdit
{
  Q_OBJECT

public:
  explicit CodeTextEdit(QWidget *parent = nullptr) : QTextEdit(parent) {}
  CodeCompleter *completer = nullptr;
  // Nestor: sangria (Opciones > Compilador): espacios por nivel, 0 = tabulacion. Las tabulaciones que ya tenga el
  // archivo se ven del mismo ancho (con tabulacion: 4)
  void setIndent(int spaces);
  QString indentUnit() const { return unit; }
  void showDocument(QTextDocument *doc);   // otro documento (con su deshacer), con la misma tabulacion y letra

protected:
  void keyPressEvent(QKeyEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;   // Nestor: + Formatear codigo

private:
  void shiftLines(bool right);   // Tab / Shift+Tab con varias lineas seleccionadas (o Shift+Tab en una)
  QString unit = "  ";
};

// Nestor: Formatear codigo: vuelve a sangrar las lineas segun las llaves { } de C/C++, a partir de la sangria de
// la primera, con 'unit' por nivel (cada tabulacion de esa primera sangria cuenta como un nivel). Despues de
// if/for/while/else sin llave, la linea siguiente lleva un nivel mas. Quedan como estan las de preprocesador (#),
// las de dentro de un comentario /* */ y las continuaciones (con un parentesis abierto de la linea anterior)
QStringList pvbFormatCode(const QStringList &lines, const QString &unit = "  ");

class CodeCompleter : public QObject
{
  Q_OBJECT

public:
  CodeCompleter(QTextEdit *edit);
  void setEnabled(bool enabled);
  void documentReplaced();   // Nestor: el editor muestra otro QTextDocument (pestanas con su deshacer)
  bool popupVisible() const;

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void insertCompletion(const QString &completion);
  void perhapsShow(bool force = false);
  void slotDocumentChanged();
  void slotContentsChange(int position, int removed, int added);

private:
  QString wordUnderCursor() const;
  void updateModel();

  QTextEdit *edit;
  QCompleter *completer;
  QStandardItemModel *model;
  bool enabled;
  bool documentChanged;
  QPointer<QTextDocument> doc;
};

#endif
