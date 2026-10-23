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
#include "../pvbrowser/pvdefine.h"
#include <QtGui>
#include "syntax.h"
#include "codecompleter.h"
#include "compilerconfig.h"
#include "editor.h"
#include "opt.h"

extern OPT_DEVELOP opt_develop;

Editor::Editor()
{
  root = new QWidget();

  viewLayout = new QHBoxLayout(root); // this, 11, 6, "viewLayout");

  // Nestor: el Modulo siempre a la vista; abajo, Pantallas y Nombres de widgets (uno u otro)
  left = new QWidget(root);
  left->setMaximumSize( QSize(170,32767) );
  QVBoxLayout *leftLayout = new QVBoxLayout(left);
  leftLayout->setContentsMargins(0,0,0,0);
  leftLayout->setSpacing(2);
  QLabel *moduleTitle = new QLabel(QCoreApplication::translate("Editor", "Module"), left);
  moduleTitle->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
  moduleTitle->setStyleSheet("QLabel { font-weight: bold; padding: 3px; }");
  leftLayout->addWidget(moduleTitle);

  page1 = new QWidget(left);
  page1->setBackgroundRole(QPalette::Window);
  page1->setFixedSize(170,335);
  leftLayout->addWidget(page1);

  t = new QToolBox(left);
  leftLayout->addWidget(t, 1);

  buttonGroup1 = new QGroupBox(QCoreApplication::translate("Editor", "File"),page1);
  buttonGroup1->setGeometry( QRect(0,0,170,195) );

  radioProject = new QRadioButton(QCoreApplication::translate("Editor", "Project"),buttonGroup1);
  radioProject->setGeometry( QRect(10,20,130,25) );

  radioMain = new QRadioButton(QCoreApplication::translate("Editor", "Main"),buttonGroup1);
  radioMain->setGeometry( QRect(10,50,130,25) );

  radioHeader = new QRadioButton(QCoreApplication::translate("Editor", "Header"),buttonGroup1);
  radioHeader->setGeometry( QRect(10,80,130,25) );

  radioMask = new QRadioButton(QCoreApplication::translate("Editor", "Masks"),buttonGroup1);
  radioMask->setGeometry( QRect(10,110,130,25) );
  radioMask->setChecked( false );

  radioSlots = new QRadioButton(QCoreApplication::translate("Editor", "Slots"),buttonGroup1);
  radioSlots->setGeometry( QRect(10,140,70,25) ); //130

  radioScript = new QRadioButton(QCoreApplication::translate("Editor", "Script"),buttonGroup1);
  radioScript->setGeometry( QRect(65,140,50,25) );

  textLabelLine = new QLabel(QCoreApplication::translate("Editor", "Line=%1").arg(0),buttonGroup1);
  textLabelLine->setGeometry( QRect(100,20,80,25) );

  textLabel1 = new QLabel(QCoreApplication::translate("Editor", "Masknr."),buttonGroup1);
  textLabel1->setGeometry( QRect(120,110,80,25) );

  spinBoxMask = new QSpinBox(buttonGroup1);
  spinBoxMask->setGeometry( QRect(120,140,45,25) );
  spinBoxMask->setMaximum(1000);
  spinBoxMask->setMinimum(1);

  maskName = new QLabel(buttonGroup1);   // Nestor: el nombre de la pantalla
  maskName->setGeometry( QRect(10,168,155,22) );
  maskName->setStyleSheet("QLabel { font-weight: bold; }");

  buttonGroup1 = new QGroupBox(QCoreApplication::translate("Editor", "Slot Position"),page1);
  buttonGroup1->setGeometry( QRect(0,205,170,60) );

  comboBoxEvent = new QComboBox(buttonGroup1);
  comboBoxEvent->setGeometry( QRect(10,25,150,25) );
  comboBoxEvent->insertItem(0,"//###");
  comboBoxEvent->insertItem(1,"typedef");
  comboBoxEvent->insertItem(2,"Init");
  comboBoxEvent->insertItem(3,"NullEvent");
  comboBoxEvent->insertItem(4,"ButtonEvent");
  comboBoxEvent->insertItem(5,"ButtonPressedEvent");
  comboBoxEvent->insertItem(6,"ButtonReleasedEvent");
  comboBoxEvent->insertItem(7,"TextEvent");
  comboBoxEvent->insertItem(8,"SliderEvent");
  comboBoxEvent->insertItem(9,"CheckboxEvent");
  comboBoxEvent->insertItem(10,"RadioButtonEvent");
  comboBoxEvent->insertItem(11,"GlInitializeEvent");
  comboBoxEvent->insertItem(12,"GlPaintEvent");
  comboBoxEvent->insertItem(13,"GlResizeEvent");
  comboBoxEvent->insertItem(14,"GlIdleEvent");
  comboBoxEvent->insertItem(15,"TabEvent");
  comboBoxEvent->insertItem(16,"TableTextEvent");
  comboBoxEvent->insertItem(17,"TableClickedEvent");
  comboBoxEvent->insertItem(18,"SelectionEvent");
  comboBoxEvent->insertItem(19,"ClipboardEvent");
  comboBoxEvent->insertItem(20,"RightMouseEvent");
  comboBoxEvent->insertItem(21,"KeyboardEvent");
  comboBoxEvent->insertItem(22,"MouseMovedEvent");
  comboBoxEvent->insertItem(23,"MousePressedEvent");
  comboBoxEvent->insertItem(24,"MouseReleasedEvent");
  comboBoxEvent->insertItem(25,"MouseOverEvent");
  comboBoxEvent->insertItem(26,"UserEvent");

  checkBoxSuServer = new QCheckBox("su --command=\"./pvs\"",page1);
  checkBoxSuServer->setGeometry( QRect(0,275,170,25) );
  checkBoxSuServer->setChecked(opt_develop.su);

  pushButtonInsertFunction = new QPushButton(QCoreApplication::translate("Editor", "Insert Function"),page1);
  pushButtonInsertFunction->setGeometry( QRect(0,305,170,25) );

  masklist = new QListWidget(t);   // Nestor: las pantallas del proyecto (clic: abrir)
  masklist->setToolTip(QCoreApplication::translate("Editor", "Clic: abrir la pantalla"));
  t->addItem( masklist, QCoreApplication::translate("Editor", "Pantallas"));

  page2 = new QWidget(t);
  //page1->setBackgroundRole(QPalette::Window);
  t->addItem( page2, QCoreApplication::translate("Editor", "Widget Names"));
  t->setCurrentIndex(0);

  widgetname = new QListWidget(page2);
  widgetLayout = new QVBoxLayout(page2);
  widgetLayout->setSpacing(0);
  // removed murx.rl.was.here.oct.2023 widgetLayout->setMargin(0);
  widgetLayout->addWidget(widgetname);

  edit = new CodeTextEdit(root); // Nestor: QTextEdit con texto predictivo
  edit->setFont(QFont("Courier", 12));
  edit->setLineWrapMode( QTextEdit::NoWrap );
  highlighter = new syntax(edit);
  completer = new CodeCompleter(edit); // Nestor: texto predictivo (Ctrl+Espacio)
  static_cast<CodeTextEdit *>(edit)->completer = completer;
  completer->setEnabled(CompilerConfig::load().autocomplete);

  viewLayout->addWidget(left);
  viewLayout->addWidget(edit);

  connect(edit, &QTextEdit::cursorPositionChanged, this, &Editor::slotCursorPositionChanged); // Nestor was here
}

Editor::~Editor()
{
  if(checkBoxSuServer->isChecked()) opt_develop.su = 1;
  else                              opt_develop.su = 0;
  delete highlighter;
  delete root;
}

int Editor::setSyntax(int language)
{
  highlighter->setSyntax(language);
  return 0;
}

void Editor::slotCursorPositionChanged()
{
  int line;

  QTextCursor currentPosition = edit->textCursor();
  currentPosition.movePosition(QTextCursor::StartOfLine);

  QTextCursor cursor = QTextCursor(edit->document());
  cursor.movePosition(QTextCursor::StartOfLine);

  for(line=1; cursor < currentPosition; line++)
  {
    cursor.movePosition(QTextCursor::Down);
  }

  textLabelLine->setText(QCoreApplication::translate("Editor", "Line=%1").arg(line));
}
