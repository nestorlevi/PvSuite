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
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "editor.h"
#include "designer.h"
#include "dlgsearchreplace.h"

class QAction;
class QMenu;
class QTextEdit;
class QScrollArea;
class QListWidgetItem;
class dlgTextBrowser;
class QDockWidget;
class BuildConsole;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow();

protected:
  void closeEvent(QCloseEvent *event);
  bool eventFilter(QObject *o, QEvent *e) override;   // Nestor: tamanio de la consola de compilacion

private slots:
  void slotFileOpt();
  void newFile();
  void open();
  void perhapsFixCONFIG();
  bool save();
  bool saveAs();
  void slotBackup();
  void viewEditor();
  void viewDesigner();
  void slotQtDesigner();
  void slotExportUI();
  void slotImportUI();
  void slotInsertMask();
  void slotMake();
  void slotStartServer();
  void slotStartPvbrowser();
  void slotCompilerDialog();      // Nestor: ventana "Compilador"
  void slotQuickBuildSettings();  // Nestor: barra "Compilacion"
  void slotProjectMinGW(int index); // Nestor: version de MinGW del proyecto
  void slotRllibUncommentRllib();
  void slotRllibUncommentModbus();
  void slotRllibUncommentSiemenstcp();
  void slotRllibUncommentPpi();
  void slotDaemonModbus();
  void slotDaemonSiemenstcp();
  void slotDaemonPpi();
  void slotLinuxWriteStartscript();
  void slotAboutManual();
  void about();
  void documentWasModified();
  void slotRadioProject(bool checked);
  void slotRadioMain(bool checked);
  void slotRadioHeader(bool checked);
  void slotRadioSlots(bool checked);
  void slotRadioScript(bool checked);
  void slotRadioMask(bool checked);
  void slotComboEvents(int i);
  void slotSpinBoxMask(int i);
  void slotInsertFunction();
  void slotWidgetname(QListWidgetItem *item);
  void slotMaskList(QListWidgetItem *item);   // Nestor: pantalla elegida en la barra del editor
  void slotSearch();
  void slotReplace();

private:
  // Nestor: consola de compilacion (abajo), marcas de error en el editor e historiador al ejecutar
  void createBuildConsole();
  void buildProject(bool startServer);
  void startServerProcess(const QString &project);
  static QString serverExePath(const QString &project);
  void applyErrorMarks();
  void openLocation(const QString &file, int line, int column);
  QDockWidget  *buildDock;
  BuildConsole *buildConsole;
  bool buildDockRestoring = false;   // Nestor: no guardar el tamanio mientras se restaura
  void restoreBuildDockSize();
  QToolBar *projectToolBar;          // Nestor: procesos, bloques, recursos, usuarios y simulacion
  static QIcon toolIcon(int kind);
  void createActions();
  void createMenus();
  void createToolBars();
  void createStatusBar();
  void connectActions();
  void disconnectActions();
  void readSettings();
  void writeSettings();
  bool maybeSave();
  void loadFile(const QString &fileName);
  bool saveFile(const QString &fileName);
  void setCurrentFile(const QString &fileName);
  QString strippedName(const QString &fullFileName);
  void load(const QString &fileName);
  void getWidgetNames(const QString &filename);
  void keyPressEvent(QKeyEvent * event);
  void keyReleaseEvent(QKeyEvent * event);

  //QTextEdit *textEdit;
  QString curFile;
  QString name;
  bool doChecked;
  int currentMask;

  QMenu *fileMenu;
  QMenu *editMenu;
  QMenu *actionMenu;
  QMenu *rllibMenu;
  QMenu *daemonMenu;
  QMenu *linuxMenu;
  QMenu *helpMenu;
  QToolBar *fileToolBar;
  QToolBar *editToolBar;
  QToolBar *viewToolBar;
  QToolBar *alignToolBar;   // Nestor: alinear/espaciar widgets en el diseñador
  QAction *alignLeftAct, *alignRightAct, *alignTopAct, *alignBottomAct, *spaceHorAct, *spaceVerAct;
  QAction *alignHCenterAct, *alignVCenterAct;
  void setDesignerMode(bool designerMode); // Nestor: barras segun la vista (Designer / editor)
  void syncMaskList();                      // Nestor: lista de pantallas y nombre en el editor
  QString maskTitle(int mask);              // "Alarmas (mask3)" o "mask3"
  QToolBar  *buildToolBar;   // Nestor: compilador y procesadores
  class QComboBox *comboCompiler;
  class QComboBox *comboMingw;   // Nestor: version de MinGW del proyecto (se guarda en el .pvproject)
  QAction   *compilerAct;
  void syncBuildToolBar();
  void offerMinGWLibDir(const QString &project); // Nestor: MinGW 8.1: adaptar el .pro (una sola vez)
  QAction *optAct;
  QAction *newAct;
  QAction *openAct;
  QAction *saveAct;
  QAction *saveAsAct;
  QAction *backupAct;
  QAction *exitAct;
  QAction *cutAct;
  QAction *copyAct;
  QAction *pasteAct;
  QMetaObject::Connection designerCopy, designerPaste;   // Nestor: copiar y pegar widgets en el Designer
  QAction *searchAct;
  QAction *replaceAct;
  QAction *viewEditorAct;
  QAction *viewDesignerAct;
  QAction *actionEditorAct;
  QAction *actionDesignerAct;
  QAction *actionQtDesignerAct;
  QAction *actionExportUIAct;
  QAction *actionImportUIAct;
  QAction *actionInsertMaskAct;
  QAction *actionMakeAct;
  QAction *actionStartServerAct;
  QAction *actionStartPvbrowserAct;
  QAction *rllibUncommentRllibAct;
  QAction *rllibUncommentModbusAct;
  QAction *rllibUncommentSiemenstcpAct;
  QAction *rllibUncommentPpiAct;
  QAction *daemonModbusAct;
  QAction *daemonSiemenstcpAct;
  QAction *daemonPpiAct;
  QAction *tagMonitorAct;     // Nestor: monitor de tags (depurar)
  QAction *linuxWriteStartscriptAct;
  QAction *aboutManualAct;
  QAction *aboutAct;
  QAction *aboutQtAct;

  int  beginMenu();
  void endMenu();
  Editor   *editor;
  Designer *designer;
  QScrollArea *scroll;
  dlgTextBrowser *dlgtextbrowser;
  dlgsearchreplace searchreplace;
  int imask;
  QString blockFile;                     // Nestor: el bloque que esta en el Designer (vacio: maskN)
  QMap<QString, int> filePos;
};

int readProject();

#endif
