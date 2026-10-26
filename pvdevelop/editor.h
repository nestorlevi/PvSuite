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
#ifndef EDITOR_H
#define EDITOR_H

#include <QObject>
#include "syntax.h"

class QTextEdit;
class QSyntaxHighlighter;
class QTextDocument;
class QHBoxLayout;
class QVBoxLayout;
class QToolBox;
class QLabel;
class QSpinBox;
class QButtonGroup;
class QGroupBox;
class QPushButton;
class QCheckBox;
class QWidget;
class QListWidget;
class QRadioButton;
class QComboBox;
class CodeCompleter;

class Editor : public QObject
{
  Q_OBJECT

  public:
    Editor();
    ~Editor();
    int setSyntax(int language);
    void setDocument(QTextDocument *doc);   // Nestor: el de otra pestana
    QWidget *root;
    QTextEdit *edit;
    CodeCompleter *completer;   // Nestor: texto predictivo
    QCheckBox* checkBoxSuServer;
    QRadioButton* radioHeader;
    QRadioButton* radioMain;
    QRadioButton* radioProject;
    QRadioButton* radioSlots;
    QRadioButton* radioScript;
    QRadioButton* radioMask;
    QSpinBox* spinBoxMask;
    QComboBox* comboBoxEvent;
    QPushButton* pushButtonInsertFunction;
    QListWidget* widgetname;
    QListWidget* masklist;   // Nestor: pantallas (dato: el numero)
    QLabel* maskName;        // Nestor: nombre de la pantalla elegida
    QToolBox* t;             // Pantallas / Widget Names
  private:
    //QSyntaxHighlighter *highlighter;
    syntax *highlighter;
    QHBoxLayout* viewLayout;
    QVBoxLayout* widgetLayout;
    QWidget* left;
    QWidget* page1;
    QLabel* textLabel1;
    QGroupBox* buttonGroup1;
    QGroupBox* buttonGroup2;
    QLabel* textLabelLine;
    QWidget* page2;

  private slots:
    void slotCursorPositionChanged();
};

#endif
