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
#include <QFile>

#include "mainwindow.h"
#include "dlgopt.h"
#include "dlgnewprj.h"
#include "dlginsertfunction.h"
#include "dlgpastewidget.h"
#include "dlgtextbrowser.h"
#include "dlgdaemon.h"
#include "../pvbrowser/pvblanguage.h"
#include "../pvbrowser/pvbupdate.h"
#include "opt.h"
#include "cutil.h"
#include "widgetgenerator.h"
#include "pvbindgen.h"
#include "syntax.h"
#include <QPainter>
#include <QPixmap>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QInputDialog>
#include <QRegularExpression>
#include "compilerconfig.h"
#include "dlgcompiler.h"
#include "dlgtagmonitor.h"
#include "buildconsole.h"
#include <QDockWidget>
#include <QStandardPaths>
#include <QProcess>
#include <QTimer>
#include <QPainterPath>
#include <QCoreApplication>
#include <QToolBar>
#include <QSettings>
#include <QTextBlock>
#include <QRegularExpression>
#include "codecompleter.h"
#include <QMessageBox>
#include <QFileInfo>

// Nestor: íconos de la barra Align dibujados en código (guía gris, widgets azules)
static QIcon alignIcon(int mode)
{
  QPixmap pm(24, 24);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, false);
  const QColor guide(128, 128, 128);
  const QColor box(239, 108, 0);    // Nestor: naranja, para distinguirlos de la barra Proyecto (azul)
  p.setPen(Qt::NoPen);
  // un poco mas chicos que el lienzo de 24: del tamanio de los de la barra Proyecto
  auto R = [&p](int x, int y, int w, int h, const QColor &c) {
    const double k = 0.88;
    const int x0 = qRound(12 + (x - 12) * k), y0 = qRound(12 + (y - 12) * k);
    const int x1 = qRound(12 + (x + w - 12) * k), y1 = qRound(12 + (y + h - 12) * k);
    p.fillRect(x0, y0, qMax(1, x1 - x0), qMax(1, y1 - y0), c);
  };
  switch(mode)
  {
    case MyRootWidget::AlignLeftEdges:
      R(2, 2, 2, 20, guide);
      R(5, 4, 15, 6, box);  R(5, 14, 9, 6, box);
      break;
    case MyRootWidget::AlignRightEdges:
      R(20, 2, 2, 20, guide);
      R(4, 4, 15, 6, box);  R(10, 14, 9, 6, box);
      break;
    case MyRootWidget::AlignTopEdges:
      R(2, 2, 20, 2, guide);
      R(4, 5, 6, 15, box);  R(14, 5, 6, 9, box);
      break;
    case MyRootWidget::AlignBottomEdges:
      R(2, 20, 20, 2, guide);
      R(4, 4, 6, 15, box);  R(14, 10, 6, 9, box);
      break;
    case MyRootWidget::AlignHCenters:     // centros en una misma vertical
      R(4, 4, 16, 6, box);  R(7, 14, 10, 6, box);
      R(11, 1, 2, 22, guide);
      break;
    case MyRootWidget::AlignVCenters:     // centros en una misma horizontal
      R(4, 4, 6, 16, box);  R(14, 7, 6, 10, box);
      R(1, 11, 22, 2, guide);
      break;
    case MyRootWidget::SpaceHorizontally:
      R(1, 6, 4, 12, box);  R(10, 6, 4, 12, box);  R(19, 6, 4, 12, box);
      R(6, 11, 3, 2, guide); R(15, 11, 3, 2, guide);
      break;
    case MyRootWidget::SpaceVertically:
      R(6, 1, 12, 4, box);  R(6, 10, 12, 4, box);  R(6, 19, 12, 4, box);
      R(11, 6, 2, 3, guide); R(11, 15, 2, 3, guide);
      break;
  }
  p.end();
  return QIcon(pm);
}

extern OPT_DEVELOP opt_develop;

static const char exportWarning[] = "You are not allowed to define the layout management within Qt Designer.\n"
                                    "You will have to define a possible layout management within pvdevelop.\n"
                                    "This is done within the graphical designer of pvdevelop.\n"
                                    "There right click the mouse and choose \"Edit layout\" from the context menu.\n"
                                    "\n"
                                    "If you define a layout management within Qt Designer the geometry of your widgets will get lost after importing again to pvdevelop!!!\n"
                                    "\n"
                                    "Do you want to continue ?\n";

MainWindow::MainWindow()
{
  if(opt_develop.arg_debug) printf("MainWindow start\n");

  imask = 1;
  // Nestor: compilador / carpetas / procesadores elegidos en la ventana "Compilador"
  CompilerConfig::load().applyEnvironment();
  createActions();
  createMenus();
  createToolBars();
  createStatusBar();
  readSettings();
  buildConsole = NULL;
  createBuildConsole();

  editor   = NULL;
  designer = NULL;
  //viewDesigner();
  viewEditor();

  doChecked = true;
  currentMask = 1;
  setCurrentFile("");

  dlgtextbrowser = new dlgTextBrowser(opt_develop.manual);

  if(opt_develop.arg_project[0] != '\0')
  {
    QString test = opt_develop.arg_project;
    test += ".pro";
    FILE *fin = fopen(test.toUtf8(),"r");
    if(fin != NULL)
    {
      fclose(fin);
      name = opt_develop.arg_project;
      load(name + ".pro");
      editor->radioProject->setChecked(true);
      perhapsFixCONFIG();
    }
    else if(opt_develop.script == PV_LUA)
    {
      test = "main.lua";
      fin = fopen(test.toUtf8(),"r");
      if(fin != NULL)
      {
        fclose(fin);
        name = opt_develop.arg_project;
        load("main.lua");
        editor->radioMain->setChecked(true);
      }
    }
  }

  if(opt_develop.arg_debug) printf("MainWindow end\n");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  if(editor == NULL) viewEditor();
  if(maybeSave())
  {
    writeSettings();
    if(dlgtextbrowser != NULL) delete dlgtextbrowser;
    event->accept();
  }
  else
  {
    event->ignore();
  }
}

void MainWindow::newFile()
{
  QString project;
  int ret;
  if (maybeSave())
  {
    editor->edit->clear();
    setCurrentFile("");
  }

  dlgnewprj dlg;
  ret = dlg.exec();
  name = dlg.uidlg->lineEditName->text();
  if(ret == QDialog::Accepted && !name.isEmpty())
  {
    name = name.simplified();
    name = name.replace(" ","_");
    while(name.toUtf8().size() > (int) sizeof(opt_develop.arg_project) - 2) name.chop(1); // Nestor: bytes UTF-8, no caracteres
    strcpy(opt_develop.arg_project,(const char *) name.toUtf8());
    if(dlg.uidlg->lineEditDirectory->text() != "") QDir::setCurrent(dlg.uidlg->lineEditDirectory->text());
    int language = dlg.uidlg->comboBoxLanguage->currentIndex();
    if(opt_develop.arg_debug) printf("comboBoxLanguage->currentText=%s language=%d\n",
           (const char *) dlg.uidlg->comboBoxLanguage->currentText().toUtf8(),
           language);
    // language = 0 C/C++
    // language = 1 Lua
    // language = 2 Phyton (experimental)
    if     (language == 1) opt_develop.script = PV_LUA;
    else if(language == 2) opt_develop.script = PV_PYTHON;
    else                   opt_develop.script = 0; // C/C++
    if(opt_develop.script == PV_PYTHON)
    {
      name = "pvs";
      strcpy(opt_develop.arg_project, "pvs");
#ifdef PVWIN32
      system("pvb_copy_python_template.bat");
#else
      ret = system("cp /opt/pvb/language_bindings/python/template/* .");
      if(ret < 0) printf("ERROR system(cp /opt/pvb/language_bindings/python/template/* .)\n");
#endif
    }
    else
    {
      if(opt_develop.arg_debug) printf("calling generateInitialProject language=%d\n", language);
      generateInitialProject(name.toUtf8());
    }
    if(opt_develop.arg_debug) printf("script=%d\n", opt_develop.script);
    ret = readProject();
    if(ret == -1) { name.clear(); return; }
    syncBuildToolBar(); // Nestor: MinGW del proyecto
    // add additional language here
    if     (opt_develop.script == PV_PYTHON) editor->radioScript->show();
    else if(opt_develop.script == PV_PERL)   editor->radioScript->show();
    else if(opt_develop.script == PV_PHP)    editor->radioScript->show();
    else if(opt_develop.script == PV_TCL)    editor->radioScript->show();
    else                             editor->radioScript->hide();
    if(opt_develop.script == PV_LUA)         editor->radioHeader->hide();
    else                             editor->radioHeader->show();
    if(opt_develop.script == PV_LUA)         editor->radioProject->hide();
    else                             editor->radioProject->show();
    setCurrentFile(name + ".pro");
    project = "qmake=" + name;
    action(project.toUtf8());
    viewDesigner();
  }
}

int readProject()
{
  char line[1024],*cptr;
  QString project = QString::fromUtf8(opt_develop.arg_project);
  project += ".pvproject";
  QFile fin(project); // Nestor: QFile y no fopen: nombres con acentos en Windows
  if(!fin.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    return -1;
  }

  strcpy(opt_develop.target, "pvs");
  opt_develop.xmax = 1280;
  opt_develop.ymax = 1024;
  opt_develop.script = 0;
  while(fin.readLine(line,sizeof(line)-1) > 0)
  {
    if(strncmp(line,"target=",7) == 0)
    {
      cptr = strchr(line,'\n');
      if(cptr != NULL) *cptr = '\0';
      cptr = strchr(line,'=');
      if(cptr == NULL)
      {
        fin.close();
        return -1;
      }
      cptr++;
      strcpy(opt_develop.target,cptr);
    }
    else if(strncmp(line,"xmax=",5) == 0)
    {
      sscanf(line,"xmax=%d",&opt_develop.xmax);
    }
    else if(strncmp(line,"ymax=",5) == 0)
    {
      sscanf(line,"ymax=%d",&opt_develop.ymax);
    }
    else if(strncmp(line,"script=Python",13) == 0)
    {
      opt_develop.script = PV_PYTHON;
    }
    else if(strncmp(line,"script=Perl",11) == 0)
    {
      opt_develop.script = PV_PERL;
    }
    else if(strncmp(line,"script=PHP",10) == 0)
    {
      opt_develop.script = PV_PHP;
    }
    else if(strncmp(line,"script=Tcl",10) == 0)
    {
      opt_develop.script = PV_TCL;
    }
    else if(strncmp(line,"script=Lua",10) == 0)
    {
      opt_develop.script = PV_LUA;
    }
    // add additional language here
  }
  fin.close();
  if(opt_develop.arg_debug) printf("Project: target=%s xmax=%d ymax=%d script=%d\n", opt_develop.target, opt_develop.xmax, opt_develop.ymax, opt_develop.script);
  CompilerConfig::setProject(QDir::current().absoluteFilePath(project)); // Nestor: MinGW del proyecto (lineas mingw=, qtmingw=)
  return 0;
}

void MainWindow::open()
{
  int ret;
  if(maybeSave())
  {
    QString fileName = QFileDialog::getOpenFileName(this,"Choose project","","*.pvproject");
    if(!fileName.isEmpty())
    {
      QDir dir(fileName);
      dir.cdUp();
      QString path = dir.path();
      if(opt_develop.arg_debug) printf("mainwindow().open().setCurrent(%s)\n",(const char *) path.toUtf8());
      QDir::setCurrent(path);
      // Nestor: solo el nombre (la carpeta ya es la actual): una carpeta con punto (juan.perez) o con acentos
      // cortaba la ruta o fallaba fopen en Windows
      const QByteArray base = QFileInfo(fileName).completeBaseName().toUtf8();
      if(base.size() < (int) sizeof(opt_develop.arg_project)) strcpy(opt_develop.arg_project, base.constData());

      ret = readProject();
      if(ret == -1) { name.clear(); return; }
      syncBuildToolBar(); // Nestor: MinGW del proyecto
      // add additional language here
      if     (opt_develop.script == PV_PYTHON) editor->radioScript->show();
      else if(opt_develop.script == PV_PERL)   editor->radioScript->show();
      else if(opt_develop.script == PV_PHP)    editor->radioScript->show();
      else if(opt_develop.script == PV_TCL)    editor->radioScript->show();
      else                             editor->radioScript->hide();
      if(opt_develop.script == PV_LUA)         editor->radioHeader->hide();
      else                             editor->radioHeader->show();
      if(opt_develop.script == PV_LUA)         editor->radioProject->hide();
      else                             editor->radioProject->show();
      name = opt_develop.target;

#ifdef PVUNIX
      char cmd[1024];
      sprintf(cmd,"fake_qmake %s %s.pro -o Makefile", opt_develop.fake_qmake, (const char *) name.toUtf8());
      if(opt_develop.arg_debug) printf("cmd=%s\n",cmd);
      ret = system(cmd);
      if(ret < 0) printf("ERROR system(%s)\n", cmd);
#endif
      if(opt_develop.script == PV_LUA)
      {
        load("main.lua");
        if(editor != NULL) editor->radioMain->setChecked(true);
      }
      else
      {
        load(name + ".pro");
        if(editor != NULL) editor->radioProject->setChecked(true);
        perhapsFixCONFIG();
      }
    }
  }
}

void MainWindow::perhapsFixCONFIG()
{
  if(editor != NULL)
  {
    QTextCursor cursor = editor->edit->document()->find("CONFIG");
    if(cursor.isNull() == false)
    {
      cursor.clearSelection();
      cursor.movePosition(QTextCursor::NextWord);
      cursor.select(QTextCursor::WordUnderCursor);
      QString txt = cursor.selectedText();
      if(txt == "=")
      {
        cursor.insertText("+=");
      }
    }
  }
}

bool MainWindow::save()
{
  if(designer != NULL && !blockFile.isEmpty())   // Nestor: un bloque (y sus copias en las mascaras)
  {
    generateMask(blockFile.toUtf8(), designer->root);
    pvbSaveBindings(blockFile, designer->root);
    designer->root->modified = 0;
    return true;
  }
  if(designer != NULL)
  {
    char buf[80];
    if(opt_develop.script == PV_LUA)
    {
      sprintf(buf,"mask%d.lua",imask);
    }
    else
    {
      sprintf(buf,"mask%d.cpp",imask);
    }
    generateMask(buf, designer->root);
    if(opt_develop.script != PV_LUA) pvbSaveBindings(buf, designer->root);   // Nestor: maskN_pvb.h
    // add additional language here
    if(opt_develop.script == PV_PYTHON)
    {
      generatePython(imask, designer->root);
    }
    if(opt_develop.script == PV_PHP)
    {
      generatePHP(imask, designer->root);
    }
    if(opt_develop.script == PV_PERL)
    {
      generatePerl(imask, designer->root);
    }
    if(opt_develop.script == PV_TCL)
    {
      generateTcl(imask, designer->root);
    }
    designer->root->modified = 0;
    return true;
  }

  if(curFile.isEmpty()) return false; // saveAs();
  else                  return saveFile(curFile);
}

bool MainWindow::saveAs()
{
  QString fileName = QFileDialog::getSaveFileName(this);
  if(fileName.isEmpty()) return false;
  return saveFile(fileName);
}

void MainWindow::slotBackup()
{
  QString command;
  QString message = "Now running:\n";
  QString localname = name;
  localname.remove(".pro");
  //rlmurx-was-here command.sprintf("tar -zcf %s/%s.tar.gz .",opt_develop.backupLocation,(const char *) localname.toUtf8());
  command = QString::asprintf("tar -zcf %s/%s.tar.gz .",opt_develop.backupLocation,(const char *) localname.toUtf8());
  message.append(command);
  QMessageBox::information(this, tr("pvdevelop"),message);
  int ret = system(command.toUtf8());
  if(ret < 0) printf("ERROR system(%s)\n", (const char *) command.toUtf8());
}

void MainWindow::about()
{
  QMessageBox::about(this, tr("About pvdevelop"),
            tr("(C) 2000-2019 Lehrig Software Engineering\n"
               "develop pvserver's easily\n"
               "IDE for editing and designing pvserver and data acquisition\n"
               "Have a lot of fun: Your's pvbrowser community\n"
               "http://pvbrowser.org"));
}

void MainWindow::documentWasModified()
{
  setWindowModified(editor->edit->document()->isModified());
}

void MainWindow::createActions()
{
  if(opt_develop.arg_debug) printf("createActions\n");;

  optAct = new QAction(QIcon(":/images/option.png"), tr("O&ptions"), this);
  optAct->setStatusTip(tr("Edit options"));
  connect(optAct, SIGNAL(triggered()), this, SLOT(slotFileOpt()));

  newAct = new QAction(QIcon(":/images/new.png"), tr("&New pvserver"), this);
  newAct->setShortcut(tr("Ctrl+N"));
  newAct->setStatusTip(tr("Create a new visualization"));
  //connect(newAct, SIGNAL(triggered()), this, SLOT(newFile()));

  openAct = new QAction(QIcon(":/images/open.png"), tr("&Open..."), this);
  openAct->setShortcut(tr("Ctrl+O"));
  openAct->setStatusTip(tr("Open an existing file"));
  //connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

  saveAct = new QAction(QIcon(":/images/save.png"), tr("&Save"), this);
  saveAct->setShortcut(tr("Ctrl+S"));
  saveAct->setStatusTip(tr("Save the document to disk"));
  connect(saveAct, SIGNAL(triggered()), this, SLOT(save()));

  //saveAsAct = new QAction(tr("Save &As..."), this);
  //saveAsAct->setStatusTip(tr("Save the document under a new name"));
  //connect(saveAsAct, SIGNAL(triggered()), this, SLOT(saveAs()));

#ifdef PVUNIX
  backupAct = new QAction(QIcon(":/images/save.png"), tr("&Backup"), this);
  backupAct->setShortcut(tr("Ctrl+B"));
  backupAct->setStatusTip(tr("Create *.tar.gz file for backup purpose"));
  connect(backupAct, SIGNAL(triggered()), this, SLOT(slotBackup()));
#endif

  exitAct = new QAction(tr("E&xit"), this);
  exitAct->setShortcut(tr("Ctrl+Q"));
  exitAct->setStatusTip(tr("Exit the application"));
  //connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

  cutAct = new QAction(QIcon(":/images/cut.png"), tr("Cu&t"), this);
  cutAct->setStatusTip(tr("Cut the current selection's contents to the "
                            "clipboard"));

  copyAct = new QAction(QIcon(":/images/copy.png"), tr("&Copy"), this);
  copyAct->setShortcut(tr("Ctrl+C"));
  copyAct->setStatusTip(tr("Copy the current selection's contents to the "
                             "clipboard"));

  pasteAct = new QAction(QIcon(":/images/paste.png"), tr("&Paste"), this);
  pasteAct->setShortcut(tr("Ctrl+V"));
  pasteAct->setStatusTip(tr("Paste the clipboard's contents into the current "
                              "selection"));

  searchAct = new QAction(tr("&Find"), this);
  searchAct->setShortcut(tr("Ctrl+F"));
  searchAct->setStatusTip(tr("Find string in editor"));

  replaceAct = new QAction(tr("&Replace"), this);
  replaceAct->setShortcut(tr("Ctrl+R"));
  replaceAct->setStatusTip(tr("Replace strings in editor"));

  viewEditorAct = new QAction(QIcon(":/images/editor.png"), tr("&Editor"), this);
  //viewEditorAct->setShortcut(tr("Ctrl+E"));
  viewEditorAct->setStatusTip(tr("Show the Editor"));
  connect(viewEditorAct, SIGNAL(triggered()), this, SLOT(viewEditor()));

  viewDesignerAct = new QAction(QIcon(":/images/designer.png"), tr("&Designer"), this);
  //viewDesignerAct->setShortcut(tr("Ctrl+D"));
  viewDesignerAct->setStatusTip(tr("Show the Designer"));
  connect(viewDesignerAct, SIGNAL(triggered()), this, SLOT(viewDesigner()));

  // Nestor: alinear/espaciar la selección del diseñador (respecto del primer widget seleccionado)
  struct { QAction **act; int mode; const char *text; const char *tip; } aligns[] = {
    { &alignLeftAct,   MyRootWidget::AlignLeftEdges,    "Align &Left",        "Align left edges to the first selected widget" },
    { &alignRightAct,  MyRootWidget::AlignRightEdges,   "Align &Right",       "Align right edges to the first selected widget" },
    { &alignTopAct,    MyRootWidget::AlignTopEdges,     "Align &Top",         "Align top edges to the first selected widget" },
    { &alignBottomAct, MyRootWidget::AlignBottomEdges,  "Align &Bottom",      "Align bottom edges to the first selected widget" },
    { &alignHCenterAct, MyRootWidget::AlignHCenters,    "Centrar &horizontalmente", "Centrar horizontalmente respecto del primer widget seleccionado" },
    { &alignVCenterAct, MyRootWidget::AlignVCenters,    "Centrar &verticalmente",   "Centrar verticalmente respecto del primer widget seleccionado" },
    { &spaceHorAct,    MyRootWidget::SpaceHorizontally, "Equal &Horizontal Spacing", "Make horizontal spacing equal (at least 3 widgets)" },
    { &spaceVerAct,    MyRootWidget::SpaceVertically,   "Equal &Vertical Spacing",   "Make vertical spacing equal (at least 3 widgets)" },
  };
  for(const auto &a : aligns)
  {
    *a.act = new QAction(alignIcon(a.mode), tr(a.text), this);
    (*a.act)->setStatusTip(tr(a.tip));
    (*a.act)->setToolTip(tr(a.tip));
    (*a.act)->setEnabled(false); // solo en el diseñador
    const int mode = a.mode;
    connect(*a.act, &QAction::triggered, this, [this, mode]() {
      if(designer != NULL && designer->root != NULL) designer->root->alignSelection(mode);
    });
  }

  actionEditorAct = new QAction(QIcon(":/images/editor.png"), tr("&Editor"), this);
  actionEditorAct->setShortcut(tr("Ctrl+E"));
  actionEditorAct->setStatusTip(tr("Show the Editor"));
  connect(actionEditorAct, SIGNAL(triggered()), this, SLOT(viewEditor()));

  actionDesignerAct = new QAction(QIcon(":/images/designer.png"), tr("&Designer"), this);
  actionDesignerAct->setShortcut(tr("Ctrl+D"));
  actionDesignerAct->setStatusTip(tr("Show the Designer"));
  connect(actionDesignerAct, SIGNAL(triggered()), this, SLOT(viewDesigner()));

  actionQtDesignerAct = new QAction(QIcon(":/images/importui.png"), tr("Design UI-&File with Qt Designer"), this);
  actionQtDesignerAct->setStatusTip(tr("qtdesigner UI-File"));

  actionExportUIAct = new QAction(QIcon(":/images/importui.png"), tr("Export UI-&File"), this);
  //actionExportUIAct->setShortcut(tr("Ctrl+U"));
  actionExportUIAct->setStatusTip(tr("Export UI-File"));
  //connect(actionExportUIAct, SIGNAL(triggered()), this, SLOT(slotExportUI()));

  actionImportUIAct = new QAction(QIcon(":/images/importui.png"), tr("Import &UI-File"), this);
  actionImportUIAct->setShortcut(tr("Ctrl+U"));
  actionImportUIAct->setStatusTip(tr("Import UI-File"));
  //connect(actionImportUIAct, SIGNAL(triggered()), this, SLOT(slotImportUI()));

  actionInsertMaskAct = new QAction(QIcon(":/images/insertmask.png"), tr("&Insert Mask"), this);
  actionInsertMaskAct->setShortcut(tr("Ctrl+I"));
  actionInsertMaskAct->setStatusTip(tr("Insert a new mask"));
  //connect(actionInsertMaskAct, SIGNAL(triggered()), this, SLOT(slotInsertMask()));

  actionMakeAct = new QAction(QIcon(":/images/make.png"), tr("&Make"), this);
  actionMakeAct->setShortcut(tr("Ctrl+M"));
  actionMakeAct->setStatusTip(tr("Make project"));
  //connect(actionMakeAct, SIGNAL(triggered()), this, SLOT(slotMake()));

  actionStartServerAct = new QAction(QIcon(":/images/startserver.png"), tr("&StartServer"), this);
  actionStartServerAct->setShortcut(tr("Ctrl+T"));
  actionStartServerAct->setStatusTip(tr("Start the Server"));
  //connect(actionStartServerAct, SIGNAL(triggered()), this, SLOT(slotStartServer()));

  compilerAct = new QAction(QIcon(":/images/option.png"), tr("&Compilador..."), this); // Nestor
  compilerAct->setStatusTip(tr("Compilador (MSVC / MinGW), carpetas, procesadores y texto predictivo"));
  connect(compilerAct, &QAction::triggered, this, &MainWindow::slotCompilerDialog);

  actionStartPvbrowserAct = new QAction(QIcon(":/images/app.png"), tr("&pvbrowser"), this);
  actionStartPvbrowserAct->setShortcut(tr("Ctrl+P"));
  actionStartPvbrowserAct->setStatusTip(tr("Start pvbrowser"));
  //connect(actionStartPvbrowserAct, SIGNAL(triggered()), this, SLOT(slotStartPvbrowser()));

  rllibUncommentRllibAct = new QAction(tr("Uncomment &rllib"), this);
  rllibUncommentRllibAct->setStatusTip(tr("Uncomment rllib"));
  //connect(rllibUncommentRllibAct, SIGNAL(triggered()), this, SLOT(slotRllibUncommentRllib()));

  rllibUncommentModbusAct = new QAction(tr("Uncomment &modbus"), this);
  rllibUncommentModbusAct->setStatusTip(tr("Uncomment modbus"));
  //connect(rllibUncommentModbusAct, SIGNAL(triggered()), this, SLOT(slotRllibUncommentModbus()));

  rllibUncommentSiemenstcpAct = new QAction(tr("Uncomment &Siemens TCP"), this);
  rllibUncommentSiemenstcpAct->setStatusTip(tr("Uncomment Siemens TCP"));
  //connect(rllibUncommentSiemenstcpAct, SIGNAL(triggered()), this, SLOT(slotRllibUncommentSiemenstcp()));

  rllibUncommentPpiAct = new QAction(tr("Uncomment &ppi"), this);
  rllibUncommentPpiAct->setStatusTip(tr("Uncomment ppi"));
  //connect(rllibUncommentPpiAct, SIGNAL(triggered()), this, SLOT(slotRllibUncommentPpi()));

  daemonModbusAct = new QAction(tr("&Modbus"), this);
  daemonModbusAct->setStatusTip(tr("Modbus daemon"));
  //connect(daemonModbusAct, SIGNAL(triggered()), this, SLOT(slotDaemonModbus()));

  daemonSiemenstcpAct = new QAction(tr("&Siemens TCP"), this);
  daemonSiemenstcpAct->setStatusTip(tr("Siemens TCP daemon"));
  //connect(daemonSiemenstcpAct, SIGNAL(triggered()), this, SLOT(slotDaemonSiemenstcp()));

  tagMonitorAct = new QAction(tr("&Monitor de tags..."), this);
  tagMonitorAct->setStatusTip(tr("Valores de los tags de los daemons en vivo (para depurar): escribir y forzar valores; inicia el daemon normal o en simulacion"));
  connect(tagMonitorAct, &QAction::triggered, this, [this]() { (new DlgTagMonitor(this))->show(); });
  daemonPpiAct = new QAction(tr("&PPI"), this);
  daemonPpiAct->setStatusTip(tr("PPI daemon"));
  //connect(daemonPpiAct, SIGNAL(triggered()), this, SLOT(slotDaemonPpi()));

  linuxWriteStartscriptAct = new QAction(tr("&Write Startscript"), this);
  linuxWriteStartscriptAct->setStatusTip(tr("Write Startscript for daemon"));
  //connect(linuxWriteStartscriptAct, SIGNAL(triggered()), this, SLOT(slotLinuxWriteStartscript()));

  aboutManualAct = new QAction(tr("&Manual"), this);
  aboutManualAct->setShortcut(tr("Ctrl+H"));
  aboutManualAct->setStatusTip(tr("View Manual"));
  connect(aboutManualAct, SIGNAL(triggered()), this, SLOT(slotAboutManual()));

  aboutAct = new QAction(tr("&About"), this);
  aboutAct->setStatusTip(tr("Show the application's About box"));
  connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

  aboutQtAct = new QAction(tr("About &Qt"), this);
  aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));
  connect(aboutQtAct, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

  cutAct->setEnabled(false);
  copyAct->setEnabled(false);
}

void MainWindow::connectActions()
{
  if(opt_develop.arg_debug) printf("connectActions begin1\n");
  if(editor == NULL) return;
  if(opt_develop.arg_debug) printf("connectActions begin2\n");
  connect(newAct, SIGNAL(triggered()), this, SLOT(newFile()));
  connect(openAct, SIGNAL(triggered()), this, SLOT(open()));
  connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));
  connect(searchAct, SIGNAL(triggered()), this, SLOT(slotSearch()));
  connect(replaceAct, SIGNAL(triggered()), this, SLOT(slotReplace()));
  connect(actionQtDesignerAct, SIGNAL(triggered()), this, SLOT(slotQtDesigner()));
  connect(actionExportUIAct, SIGNAL(triggered()), this, SLOT(slotExportUI()));
  connect(actionImportUIAct, SIGNAL(triggered()), this, SLOT(slotImportUI()));
  connect(actionInsertMaskAct, SIGNAL(triggered()), this, SLOT(slotInsertMask()));
  connect(actionMakeAct, SIGNAL(triggered()), this, SLOT(slotMake()), Qt::UniqueConnection);   // Nestor: tambien en el Designer
  connect(actionStartServerAct, SIGNAL(triggered()), this, SLOT(slotStartServer()), Qt::UniqueConnection);
  connect(actionStartPvbrowserAct, SIGNAL(triggered()), this, SLOT(slotStartPvbrowser()), Qt::UniqueConnection);   // Nestor: Ctrl+P tambien en el Designer
  connect(rllibUncommentRllibAct, SIGNAL(triggered()), this, SLOT(slotRllibUncommentRllib()));
  connect(rllibUncommentModbusAct, SIGNAL(triggered()), this, SLOT(slotRllibUncommentModbus()));
  connect(rllibUncommentSiemenstcpAct, SIGNAL(triggered()), this, SLOT(slotRllibUncommentSiemenstcp()));
  connect(rllibUncommentPpiAct, SIGNAL(triggered()), this, SLOT(slotRllibUncommentPpi()));
  connect(daemonModbusAct, SIGNAL(triggered()), this, SLOT(slotDaemonModbus()), Qt::UniqueConnection);   // Nestor: tambien en el Designer
  connect(daemonSiemenstcpAct, SIGNAL(triggered()), this, SLOT(slotDaemonSiemenstcp()), Qt::UniqueConnection);   // Nestor: tambien en el Designer
  connect(daemonPpiAct, SIGNAL(triggered()), this, SLOT(slotDaemonPpi()), Qt::UniqueConnection);   // Nestor: tambien en el Designer
  connect(linuxWriteStartscriptAct, SIGNAL(triggered()), this, SLOT(slotLinuxWriteStartscript()));
  connect(cutAct, SIGNAL(triggered()), editor->edit, SLOT(cut()));
  connect(copyAct, SIGNAL(triggered()), editor->edit, SLOT(copy()));
  connect(pasteAct, SIGNAL(triggered()), editor->edit, SLOT(paste()));

  if(opt_develop.arg_debug) printf("connectActions 1\n");

  newAct->setEnabled(true);
  openAct->setEnabled(true);
  exitAct->setEnabled(true);
  searchAct->setEnabled(true);
  replaceAct->setEnabled(true);
  actionQtDesignerAct->setEnabled(true);
  actionExportUIAct->setEnabled(true);
  actionImportUIAct->setEnabled(true);
  actionInsertMaskAct->setEnabled(true);
  actionMakeAct->setEnabled(true);
  actionStartServerAct->setEnabled(true);
  actionStartPvbrowserAct->setEnabled(true);
  rllibUncommentRllibAct->setEnabled(true);
  rllibUncommentModbusAct->setEnabled(true);
  rllibUncommentSiemenstcpAct->setEnabled(true);
  rllibUncommentPpiAct->setEnabled(true);
  daemonModbusAct->setEnabled(true);
  daemonSiemenstcpAct->setEnabled(true);
  daemonPpiAct->setEnabled(true);
  linuxWriteStartscriptAct->setEnabled(true);
  cutAct->setEnabled(true);
  copyAct->setEnabled(true);
  pasteAct->setEnabled(true);

  if(opt_develop.script == PV_LUA)
  {
    actionMakeAct->setEnabled(false);
    rllibUncommentRllibAct->setEnabled(false);
  }
  //#################################################################

  if(opt_develop.arg_debug) printf("connectActions 2\n");

  connect(editor->edit, SIGNAL(copyAvailable(bool)), cutAct, SLOT(setEnabled(bool)));
  connect(editor->edit, SIGNAL(copyAvailable(bool)), copyAct, SLOT(setEnabled(bool)));

  connect(editor->edit->document(), SIGNAL(contentsChanged()), this, SLOT(documentWasModified()));

  connect(editor->radioProject, SIGNAL(toggled(bool)), this, SLOT(slotRadioProject(bool)));
  connect(editor->radioMain, SIGNAL(toggled(bool)), this, SLOT(slotRadioMain(bool)));
  connect(editor->radioHeader, SIGNAL(toggled(bool)), this, SLOT(slotRadioHeader(bool)));
  connect(editor->radioSlots, SIGNAL(toggled(bool)), this, SLOT(slotRadioSlots(bool)));
  if(opt_develop.arg_debug) printf("connectActions 3\n");
  connect(editor->radioScript, SIGNAL(toggled(bool)), this, SLOT(slotRadioScript(bool)));
  if(opt_develop.arg_debug) printf("connectActions 4\n");
  connect(editor->radioMask, SIGNAL(toggled(bool)), this, SLOT(slotRadioMask(bool)));

  connect(editor->spinBoxMask, SIGNAL(valueChanged(int)), this, SLOT(slotSpinBoxMask(int)));
  connect(editor->comboBoxEvent, SIGNAL(activated(int)), this, SLOT(slotComboEvents(int)));

  connect(editor->pushButtonInsertFunction, SIGNAL(clicked()), this, SLOT(slotInsertFunction()));

  connect(editor->widgetname, SIGNAL(itemClicked( QListWidgetItem*)), this, SLOT(slotWidgetname(QListWidgetItem *)));
  connect(editor->masklist, &QListWidget::itemClicked, this, &MainWindow::slotMaskList);
  if(opt_develop.arg_debug) printf("connectActions end\n");
}

void MainWindow::disconnectActions()
{
  if(opt_develop.arg_debug) printf("disconnectActions begin\n");;
  disconnect(newAct, SIGNAL(triggered()), this, SLOT(newFile()));
  disconnect(openAct, SIGNAL(triggered()), this, SLOT(open()));
  disconnect(exitAct, SIGNAL(triggered()), this, SLOT(close()));
  disconnect(searchAct, SIGNAL(triggered()), this, SLOT(slotSearch()));
  disconnect(replaceAct, SIGNAL(triggered()), this, SLOT(slotReplace()));
  disconnect(actionQtDesignerAct, SIGNAL(triggered()), this, SLOT(slotQtDesigner()));
  disconnect(actionExportUIAct, SIGNAL(triggered()), this, SLOT(slotExportUI()));
  disconnect(actionImportUIAct, SIGNAL(triggered()), this, SLOT(slotImportUI()));
  disconnect(actionInsertMaskAct, SIGNAL(triggered()), this, SLOT(slotInsertMask()));
  disconnect(rllibUncommentRllibAct, SIGNAL(triggered()), this, SLOT(slotRllibUncommentRllib()));
  disconnect(rllibUncommentModbusAct, SIGNAL(triggered()), this, SLOT(slotRllibUncommentModbus()));
  disconnect(rllibUncommentSiemenstcpAct, SIGNAL(triggered()), this, SLOT(slotRllibUncommentSiemenstcp()));
  disconnect(rllibUncommentPpiAct, SIGNAL(triggered()), this, SLOT(slotRllibUncommentPpi()));
  disconnect(linuxWriteStartscriptAct, SIGNAL(triggered()), this, SLOT(slotLinuxWriteStartscript()));
  disconnect(cutAct, SIGNAL(triggered()), editor->edit, SLOT(cut()));
  disconnect(copyAct, SIGNAL(triggered()), editor->edit, SLOT(copy()));
  disconnect(pasteAct, SIGNAL(triggered()), editor->edit, SLOT(paste()));

  newAct->setEnabled(false);
  openAct->setEnabled(false);
  exitAct->setEnabled(false);
  searchAct->setEnabled(false);
  replaceAct->setEnabled(false);
  actionQtDesignerAct->setEnabled(false);
  actionExportUIAct->setEnabled(false);
  actionImportUIAct->setEnabled(false);
  actionInsertMaskAct->setEnabled(false);
  actionMakeAct->setEnabled(opt_develop.script != PV_LUA);   // Nestor: Ctrl+M y Ctrl+T tambien en el Designer (guarda la mascara)
  actionStartServerAct->setEnabled(true);
  actionStartPvbrowserAct->setEnabled(true);
  rllibUncommentRllibAct->setEnabled(false);
  rllibUncommentModbusAct->setEnabled(false);
  rllibUncommentSiemenstcpAct->setEnabled(false);
  rllibUncommentPpiAct->setEnabled(false);
  linuxWriteStartscriptAct->setEnabled(false);
  cutAct->setEnabled(false);
  copyAct->setEnabled(false);
  pasteAct->setEnabled(false);

  //#####################################################################

  disconnect(editor->edit, SIGNAL(copyAvailable(bool)), cutAct, SLOT(setEnabled(bool)));
  disconnect(editor->edit, SIGNAL(copyAvailable(bool)), copyAct, SLOT(setEnabled(bool)));

  disconnect(editor->edit->document(), SIGNAL(contentsChanged()), this, SLOT(documentWasModified()));

  disconnect(editor->radioProject, SIGNAL(toggled(bool)), this, SLOT(slotRadioProject(bool)));
  disconnect(editor->radioMain, SIGNAL(toggled(bool)), this, SLOT(slotRadioMain(bool)));
  disconnect(editor->radioHeader, SIGNAL(toggled(bool)), this, SLOT(slotRadioHeader(bool)));
  disconnect(editor->radioSlots, SIGNAL(toggled(bool)), this, SLOT(slotRadioSlots(bool)));
  if(opt_develop.arg_debug) printf("disconnectActions 1\n");
  disconnect(editor->radioScript, SIGNAL(toggled(bool)), this, SLOT(slotRadioScript(bool)));
  if(opt_develop.arg_debug) printf("disconnectActions 2\n");
  disconnect(editor->radioMask, SIGNAL(toggled(bool)), this, SLOT(slotRadioMask(bool)));

  disconnect(editor->spinBoxMask, SIGNAL(valueChanged(int)), this, SLOT(slotSpinBoxMask(int)));
  disconnect(editor->comboBoxEvent, SIGNAL(activated(int)), this, SLOT(slotComboEvents(int)));

  disconnect(editor->pushButtonInsertFunction, SIGNAL(clicked()), this, SLOT(slotInsertFunction()));

  disconnect(editor->widgetname, SIGNAL(itemClicked( QListWidgetItem*)), this, SLOT(slotWidgetname(QListWidgetItem *)));
  disconnect(editor->masklist, &QListWidget::itemClicked, this, &MainWindow::slotMaskList);
  if(opt_develop.arg_debug) printf("disconnectActions end\n");;
}

void MainWindow::createMenus()
{
  if(opt_develop.arg_debug) printf("createMenus\n");;

  fileMenu = menuBar()->addMenu(tr("&File"));
  fileMenu->addAction(optAct);
  fileMenu->addSeparator();
  fileMenu->addAction(newAct);
  fileMenu->addAction(openAct);
  fileMenu->addAction(saveAct);
  //fileMenu->addAction(saveAsAct);
#ifdef PVUNIX
  fileMenu->addAction(backupAct);
#endif
  fileMenu->addSeparator();
  fileMenu->addAction(exitAct);

  editMenu = menuBar()->addMenu(tr("&Edit"));
  editMenu->addAction(cutAct);
  editMenu->addAction(copyAct);
  editMenu->addAction(pasteAct);
  editMenu->addSeparator();
  editMenu->addAction(searchAct);
  editMenu->addAction(replaceAct);

  actionMenu = menuBar()->addMenu(tr("&Action"));
  actionMenu->addAction(actionEditorAct);
  actionMenu->addAction(actionDesignerAct);
//#ifndef PVWIN32
  actionMenu->addAction(actionQtDesignerAct);
//#endif
  actionMenu->addAction(actionExportUIAct);
  actionMenu->addAction(actionImportUIAct);
  actionMenu->addAction(actionInsertMaskAct);
  actionMenu->addAction(actionMakeAct);
  actionMenu->addAction(actionStartServerAct);
  actionMenu->addAction(actionStartPvbrowserAct);
  actionMenu->addSeparator();
  actionMenu->addAction(compilerAct);

  rllibMenu = menuBar()->addMenu(tr("&RLlib"));
  rllibMenu->addAction(rllibUncommentRllibAct);
  rllibMenu->addAction(rllibUncommentModbusAct);
  rllibMenu->addAction(rllibUncommentSiemenstcpAct);
  rllibMenu->addAction(rllibUncommentPpiAct);

  daemonMenu = menuBar()->addMenu(tr("&Daemon"));
  daemonMenu->addAction(daemonModbusAct);
  daemonMenu->addAction(daemonSiemenstcpAct);
  daemonMenu->addAction(daemonPpiAct);
  daemonMenu->addSeparator();
  daemonMenu->addAction(tagMonitorAct);

#ifdef PVUNIX
  linuxMenu = menuBar()->addMenu(tr("&Linux"));
  linuxMenu->addAction(linuxWriteStartscriptAct);
#endif

  menuBar()->addSeparator();

  menuBar()->addMenu(pvbLanguageMenu(this, "pvdevelop")); // Nestor: Idioma
  helpMenu = menuBar()->addMenu(tr("&Help"));
  helpMenu->addAction(aboutManualAct);
  pvbAddUpdateActions(helpMenu, this, "pvdevelop", true); // Nestor: Buscar actualizaciones (y una vez por semana)
  helpMenu->addSeparator();
  helpMenu->addAction(aboutAct);
  helpMenu->addAction(aboutQtAct);
}

void MainWindow::createToolBars()
{
  if(opt_develop.arg_debug) printf("createToolBars\n");;

  fileToolBar = addToolBar(tr("File"));
  fileToolBar->addAction(newAct);
  fileToolBar->addAction(openAct);
  fileToolBar->addAction(saveAct);

  editToolBar = addToolBar(tr("Edit"));
  editToolBar->addAction(cutAct);
  editToolBar->addAction(copyAct);
  editToolBar->addAction(pasteAct);

  viewToolBar = addToolBar(tr("View"));
  viewToolBar->addAction(viewEditorAct);
  viewToolBar->addAction(viewDesignerAct);

  alignToolBar = addToolBar(tr("Align")); // Nestor
  alignToolBar->setObjectName("alignToolBar");
  alignToolBar->addAction(alignLeftAct);
  alignToolBar->addAction(alignHCenterAct);
  alignToolBar->addAction(alignRightAct);
  alignToolBar->addSeparator();
  alignToolBar->addAction(alignTopAct);
  alignToolBar->addAction(alignVCenterAct);
  alignToolBar->addAction(alignBottomAct);
  alignToolBar->addSeparator();
  alignToolBar->addAction(spaceHorAct);
  alignToolBar->addAction(spaceVerAct);

  // Nestor: cambio rapido de compilador (se guarda al momento). Los procesadores, las
  // carpetas y el texto predictivo se configuran en Action > Compilador...
  buildToolBar = addToolBar(tr("Compilacion"));
  comboCompiler = new QComboBox(buildToolBar);
#ifdef PVWIN32
  comboCompiler->addItem("MSVC", "msvc");
  comboCompiler->addItem("MinGW", "mingw");
  comboCompiler->setToolTip(tr("Compilador de los proyectos pvserver (Ctrl+M / Ctrl+T)"));
  buildToolBar->addWidget(comboCompiler);
#else
  comboCompiler->hide();
#endif
  // version de MinGW de este proyecto (se guarda en el .pvproject; "Predeterminado" = la de Action > Compilador...)
  comboMingw = new QComboBox(buildToolBar);
#ifdef PVWIN32
  comboMingw->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  comboMingw->setToolTip(tr("Version de MinGW de este proyecto (se guarda en el .pvproject)"));
  buildToolBar->addWidget(comboMingw);
#else
  comboMingw->hide();
#endif
  buildToolBar->addAction(compilerAct);
  syncBuildToolBar();
  connect(comboCompiler, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::slotQuickBuildSettings);
  connect(comboMingw, QOverload<int>::of(&QComboBox::activated), this, &MainWindow::slotProjectMinGW);
}

// Nestor: la barra refleja la configuracion. Un compilador sin bibliotecas o que no esta en la
// carpeta configurada queda deshabilitado; si era el elegido, se pasa al otro (si sirve).
void MainWindow::syncBuildToolBar()
{
  CompilerConfig c = CompilerConfig::load();
  const QSignalBlocker b1(comboCompiler);
#ifdef PVWIN32
  markUnavailableCompilers(comboCompiler, &c);
  const QString other = (c.compiler == "mingw") ? "msvc" : "mingw";
  const QString problem = c.compilerProblem(c.compiler);
  if(!problem.isEmpty() && c.compilerProblem(other).isEmpty())
  {
    statusBar()->showMessage(tr("%1 %2: se usa %3").arg(c.compiler == "mingw" ? "MinGW" : "MSVC")
                             .arg(problem).arg(other == "mingw" ? "MinGW" : "MSVC"), 15000);
    c.compiler = other;
    c.save();
  }
  c.applyEnvironment(); // tambien al abrir otro proyecto (puede tener otro MinGW)

  // versiones de MinGW: la predeterminada y las instaladas; la del proyecto queda elegida
  const QSignalBlocker b2(comboMingw);
  comboMingw->clear();
  const QString def = CompilerConfig::load(false).mingwDir;
  comboMingw->addItem(tr("MinGW predeterminado (%1)").arg(def.isEmpty() ? tr("ninguno") : CompilerConfig::minGWLabel(def)), QString());
  comboMingw->setItemData(0, tr("La de Action > Compilador...: %1").arg(def), Qt::ToolTipRole);
  QStringList dirs = CompilerConfig::findMinGWs();
  if(c.projectMinGW && !dirs.contains(c.mingwDir, Qt::CaseInsensitive)) dirs << c.mingwDir;
  for(const QString &d : dirs)
  {
    comboMingw->addItem(CompilerConfig::minGWLabel(d), d);
    comboMingw->setItemData(comboMingw->count() - 1, d, Qt::ToolTipRole);
    if(c.projectMinGW && d.compare(c.mingwDir, Qt::CaseInsensitive) == 0) comboMingw->setCurrentIndex(comboMingw->count() - 1);
  }
  comboMingw->setEnabled(c.compiler == "mingw" && !CompilerConfig::projectFile().isEmpty());
  const QString notice = CompilerConfig::takeNotice();
  if(!notice.isEmpty()) statusBar()->showMessage(notice, 20000);
#endif
  comboCompiler->setCurrentIndex(c.compiler == "mingw" ? 1 : 0);
}

// Nestor: version de MinGW del proyecto (barra "Compilacion"): se guarda en el .pvproject con el kit de Qt
// de la misma version de GCC (sin kit: fake_qmake genera el Makefile)
void MainWindow::slotProjectMinGW(int index)
{
#ifdef PVWIN32
  const QString dir = comboMingw->itemData(index).toString();
  const QString qt = dir.isEmpty() ? QString() : CompilerConfig::qtKitForMinGW(dir);
  if(!CompilerConfig::setProjectMinGW(dir, qt))
  {
    statusBar()->showMessage(tr("No se pudo guardar el MinGW en %1").arg(CompilerConfig::projectFile()), 10000);
  }
  else if(dir.isEmpty())
  {
    statusBar()->showMessage(tr("El proyecto usa el MinGW predeterminado. Conviene recompilar todo (los .o de otra version no sirven)."), 15000);
  }
  else
  {
    statusBar()->showMessage(tr("%1 guardado en el proyecto; Qt: %2. Conviene recompilar todo (los .o de otra version no sirven).")
                             .arg(CompilerConfig::minGWLabel(dir), qt.isEmpty() ? tr("ninguno (fake_qmake)") : qt), 15000);
  }
  syncBuildToolBar();
  if(!dir.isEmpty()) offerMinGWLibDir(name);
#else
  (void) index;
#endif
}

// Nestor: con MinGW 8.1 (GCC 8, Qt 5.15) el pvserver se enlaza con las bibliotecas de win-mingw/bin/mingw81_64
// (componente "Bibliotecas para MinGW 8.1"). Si el .pro enlaza siempre las de la suite (GCC 13) se ofrece,
// una sola vez por proyecto, agregarle PVB_MINGWLIB (elige la carpeta por QMAKE_GCC_MAJOR_VERSION).
void MainWindow::offerMinGWLibDir(const QString &project)
{
#ifdef PVWIN32
  const CompilerConfig c = CompilerConfig::load();
  if(project.isEmpty() || c.compiler != "mingw" || CompilerConfig::minGWMajor(c.mingwDir) != 8) return;
  const QByteArray pname = project.toUtf8();
  if(!proNeedsMinGWLibDir(pname.constData())) return;
  const QString pro = QFileInfo(project + ".pro").absoluteFilePath();
  QSettings s("pvbrowser", "pvdevelop");
  QStringList declined = s.value("build/mingw81Declined").toStringList();
  if(declined.contains(pro, Qt::CaseInsensitive)) return;
  const QString libs = QDir::cleanPath(QString::fromLocal8Bit(qgetenv("PVBDIR")) + "/win-mingw/bin/mingw81_64");
  QString text = tr("El proyecto usa %1, pero %2.pro enlaza las bibliotecas de pvserver de la suite (otro GCC).\n\n"
                    "¿Adaptarlo para que con GCC 8 use las de %3? Con el MinGW de la suite sigue igual.\n"
                    "(Se pregunta una sola vez.)").arg(CompilerConfig::minGWLabel(c.mingwDir), project, QDir::toNativeSeparators(libs));
  if(!QFileInfo::exists(libs + "/libserverlib.a"))
    text += "\n\n" + tr("Falta el componente \"Bibliotecas para MinGW 8.1\" de la instalacion de PV Suite.");
  if(QMessageBox::question(this, tr("MinGW 8.1"), text, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) != QMessageBox::Yes)
  {
    declined << pro;
    s.setValue("build/mingw81Declined", declined);
    return;
  }
  if(adaptProMinGWLibDir(pname.constData()) < 0)
    statusBar()->showMessage(tr("No se pudo escribir %1.pro").arg(project), 10000);
  else
    statusBar()->showMessage(tr("%1.pro adaptado: con GCC 8 enlaza las bibliotecas de %2").arg(project, QDir::toNativeSeparators(libs)), 15000);
#else
  (void) project;
#endif
}

// Nestor: selector de la barra "Compilacion"
void MainWindow::slotQuickBuildSettings()
{
  CompilerConfig c = CompilerConfig::load();
#ifdef PVWIN32
  c.compiler = comboCompiler->currentData().toString();
#endif
  c.save();
  c.applyEnvironment();
#ifdef PVWIN32
  comboMingw->setEnabled(c.compiler == "mingw" && !CompilerConfig::projectFile().isEmpty());
  statusBar()->showMessage(tr("Compilador: %1, %2 procesadores").arg(comboCompiler->currentText()).arg(c.jobs), 5000);
#endif
}

// Nestor: ventana "Compilador" (carpetas, procesadores, texto predictivo)
void MainWindow::slotCompilerDialog()
{
  DlgCompiler dlg(this);
  if(dlg.exec() != QDialog::Accepted) return;
  const CompilerConfig c = dlg.config();
  c.save();
  syncBuildToolBar(); // aplica el entorno con el MinGW del proyecto, si tiene
  if(editor != NULL && editor->completer != NULL) editor->completer->setEnabled(c.autocomplete);
}

// Nestor: cada barra solo en su vista: "Align" en el Designer, "Compilacion" en el editor de codigo
void MainWindow::setDesignerMode(bool designerMode)
{
  alignLeftAct->setEnabled(designerMode);
  alignRightAct->setEnabled(designerMode);
  alignTopAct->setEnabled(designerMode);
  alignBottomAct->setEnabled(designerMode);
  alignHCenterAct->setEnabled(designerMode);
  alignVCenterAct->setEnabled(designerMode);
  spaceHorAct->setEnabled(designerMode);
  spaceVerAct->setEnabled(designerMode);
  alignToolBar->setVisible(designerMode);
  buildToolBar->setVisible(!designerMode);

}

QString MainWindow::maskTitle(int mask)
{
  return QString("mask%1").arg(mask);
}

// Nestor: las pantallas de la carpeta (maskN.cpp o maskN.lua), ordenadas
static QList<int> maskNumbers(const QString &dir, bool lua)
{
  static const QRegularExpression reCpp("^mask(\\d+)\\.cpp$"), reLua("^mask(\\d+)\\.lua$");
  const QRegularExpression &re = lua ? reLua : reCpp;
  QList<int> masks;
  for(const QString &f : QDir(dir).entryList(QDir::Files))
  {
    QRegularExpressionMatch m = re.match(f);
    if(m.hasMatch()) masks << m.captured(1).toInt();
  }
  std::sort(masks.begin(), masks.end());
  return masks;
}

void MainWindow::createStatusBar()
{
  if(opt_develop.arg_debug) printf("createStatusBar\n");;

  statusBar()->showMessage(tr("Ready"));
}

void MainWindow::readSettings()
{
  if(opt_develop.arg_debug) printf("readSettings\n");;
  QSettings settings("Trolltech", "Application Example");
  QPoint pos = settings.value("pos", QPoint(200, 200)).toPoint();
  QSize size = settings.value("size", QSize(400, 400)).toSize();
  resize(size);
  move(pos);
}

void MainWindow::writeSettings()
{
  if(opt_develop.arg_debug) printf("writeSettings\n");;
  QSettings settings("Trolltech", "Application Example");
  settings.setValue("pos", pos());
  settings.setValue("size", size());
}

bool MainWindow::maybeSave()
{
  if(editor != NULL && editor->edit->document()->isModified())   // Nestor: en el Designer no hay editor
  {
    // murx.rl.was.here.oct.2023 int ret = QMessageBox::warning(this, tr("pvdevelop"),
    //                 tr("The document has been modified.\n"
    //                    "Do you want to save your changes?"),
    //                 QMessageBox::Yes | QMessageBox::Default,
    //                 QMessageBox::No,
    //                 QMessageBox::Cancel | QMessageBox::Escape);
    int ret = QMessageBox::warning(this, tr("pvdevelop"),
                     tr("The document has been modified.\n"
                        "Do you want to save your changes?"),
                     QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                     QMessageBox::NoButton);
    if(ret == QMessageBox::Yes)          return save();
    else if (ret == QMessageBox::Cancel) return false;
  }
  return true;
}

void MainWindow::loadFile(const QString &fileName)
{
  QFile file(fileName);
  if(!file.open(QFile::ReadOnly | QFile::Text))
  {
    QMessageBox::warning(this, tr("pvdevelop"),
                             tr("Cannot read file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
    return;
  }

  QTextStream in(&file);
  QApplication::restoreOverrideCursor();
  QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

  editor->setSyntax(CPP_SYNTAX);
  // add additional language here
  if(fileName.contains(".py"))  editor->setSyntax(PYTHON_SYNTAX);
  if(fileName.contains(".pm"))  editor->setSyntax(PERL_SYNTAX);
  if(fileName.contains(".php")) editor->setSyntax(PHP_SYNTAX);
  if(fileName.contains(".tcl")) editor->setSyntax(TCL_SYNTAX);
  if(fileName.contains(".lua")) editor->setSyntax(LUA_SYNTAX);

  editor->edit->setPlainText(in.readAll());
  QApplication::restoreOverrideCursor();

  setCurrentFile(fileName);
  statusBar()->showMessage(tr("File loaded"), 2000);
}

bool MainWindow::saveFile(const QString &fileName)
{
  FILE *fout = fopen(fileName.toUtf8(),"w");
  if(fout == NULL)
  {
    QMessageBox::warning(this, tr("pvdevelop"),
                             tr("Cannot write file %1")
                             .arg(fileName)
                             );
    return false;
  }

  QApplication::restoreOverrideCursor();
  QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
  fprintf(fout,"%s",editor->edit->toPlainText().toUtf8().data());
  QApplication::restoreOverrideCursor();
  fclose(fout);

  setCurrentFile(fileName);
  statusBar()->showMessage(tr("File saved"), 2000);
  return true;
}

void MainWindow::setCurrentFile(const QString &fileName)
{
  if(editor == NULL) return;
  if(opt_develop.arg_debug) printf("setCurrentFile=%s\n", (const char *) fileName.toUtf8());

  curFile = fileName;
  editor->edit->document()->setModified(false);
  setWindowModified(false);

  QString shownName;
  if (curFile.isEmpty()) shownName = "no project loaded";
  else                   shownName = strippedName(curFile);

  setWindowTitle(tr("%1[*] - %2").arg(shownName).arg(tr("pvdevelop")));
  applyErrorMarks(); // Nestor: errores de la ultima compilacion en este archivo
}

QString MainWindow::strippedName(const QString &fullFileName)
{
  return QFileInfo(fullFileName).fileName();
}

void MainWindow::viewEditor()
{
  if(editor != NULL) return;
  if(opt_develop.arg_debug) printf("viewEditor begin\n");;
  setDesignerMode(false); // Nestor: barra Compilacion, sin barra Align

  if(editor != NULL || designer != NULL) centralWidget()->hide();
  if(designer != NULL)
  {
    designer->root->releaseMouse();
    designer->root->releaseKeyboard();
    delete designer;
  }
  designer = NULL;
  blockFile.clear();
  editor = new Editor();
  hide();
  //setCentralWidget(editor->edit);
  setCentralWidget(editor->root);
  show();
  centralWidget()->show();
  disconnect(designerCopy);    // Nestor: Copiar y Pegar vuelven a ser del editor de texto
  disconnect(designerPaste);
  copyAct->setShortcut(tr("Ctrl+C"));
  pasteAct->setShortcut(tr("Ctrl+V"));
  connectActions();

  if(!name.isEmpty())
  {
    if(opt_develop.script == PV_LUA)
    {
      load("main.lua");
      editor->radioMain->setChecked(true);
    }
    else
    {
      load(name + ".pro");
      editor->radioProject->setChecked(true);
      perhapsFixCONFIG();
    }
    editor->spinBoxMask->setValue(currentMask);
  }

  if(opt_develop.script == 0) editor->radioScript->hide();
  if(opt_develop.arg_debug) printf("viewEditor end\n");;
}

void MainWindow::viewDesigner()
{
  char buf[80];
  QString shownName;

  if(designer != NULL) return;
  if(beginMenu() == 0)
  {
    viewEditor();
    return;
  }
  if(opt_develop.arg_debug) printf("viewDesigner begin\n");;

  imask = 1;
  if(editor != NULL) imask = editor->spinBoxMask->value();
  if(editor != NULL || designer != NULL) centralWidget()->hide();
  disconnectActions();
  if(editor != NULL) delete editor;
  editor = NULL;
  scroll = new QScrollArea;
  sprintf(buf,"mask%d.cpp",imask);
  if(!blockFile.isEmpty()) snprintf(buf, sizeof(buf), "%s", blockFile.toUtf8().constData());   // Nestor: un bloque
  if(opt_develop.arg_debug) printf("viewDesigner before new Designer buf=%s\n", buf);
  designer = new Designer(buf);
  if(opt_develop.arg_debug) printf("viewDesigner after new Designer\n");
  designer->root->mainWindow = this;
  designer->root->resize(opt_develop.xmax,opt_develop.ymax);
  scroll->setMaximumSize(opt_develop.xmax,opt_develop.ymax);
  scroll->setWidget(designer->root);
  designer->root->setScroll(scroll);
  designer->root->statusBar = statusBar();
  hide();
  setCentralWidget(scroll);
  if(opt_develop.arg_debug) printf("viewDesigner before show\n");;
  show();
  if(opt_develop.arg_debug) printf("viewDesigner before centralWidget->show\n");;
  centralWidget()->show();
  if(opt_develop.arg_debug) printf("viewDesigner after centralWidget->show\n");;
  if (curFile.isEmpty()) shownName = "no project loaded";
  //rlmurx-was-here else                   shownName.sprintf("design mask%d   ClickRightMouse->PopupDialog   Alt+Click->PropertyDialog   Shift+Click->InsertDialog   Ctrl+Click->InsertLastSelectedWidget   Ctrl-Z->UndoLastMove   R->ReleaseMouse",imask);
  else if(!blockFile.isEmpty()) shownName = tr("bloque %1 ($M = la estructura de cada copia; al guardar se actualizan las copias)   ClickRightMouse->PopupDialog   Alt+Click->PropertyDialog").arg(QFileInfo(blockFile).completeBaseName());
  else                   shownName = tr("design %1").arg(maskTitle(imask)) + QString::asprintf("   ClickRightMouse->PopupDialog   Alt+Click->PropertyDialog   Shift+Click->InsertDialog   Ctrl+Click/Drag->Select   Ctrl+Shift+Click->InsertLastSelectedWidget   Ctrl-Z->UndoLastMove   R->ReleaseMouse");
  setDesignerMode(true); // Nestor: barra Align, sin barra Compilacion
  // Nestor: los iconos Copiar y Pegar copian y pegan widgets (Cortar queda deshabilitado). Sin atajo:
  // Ctrl+C / Ctrl+V los atiende el Designer (si no, se pegaria dos veces)
  copyAct->setShortcut(QKeySequence());
  pasteAct->setShortcut(QKeySequence());
  disconnect(designerCopy);
  disconnect(designerPaste);
  designerCopy = connect(copyAct, &QAction::triggered, this, [this]() { if(designer) designer->root->copyWidget(); });
  designerPaste = connect(pasteAct, &QAction::triggered, this, [this]() { if(designer) designer->root->pasteWidget(); });
  copyAct->setEnabled(true);
  pasteAct->setEnabled(true);
  cutAct->setEnabled(false);
  //setWindowTitle(tr("%1[*] - %2").arg(shownName).arg(tr("pvdevelop")));
  setWindowTitle(tr("%1[*]").arg(shownName));

  drawDrawWidgets(designer->root);
  if(opt_develop.arg_debug) printf("viewDesigner end\n");;
}

void MainWindow::slotSearch()
{
  searchreplace.runFind(editor);
}

void MainWindow::slotReplace()
{
  searchreplace.runReplace(editor);
}

void MainWindow::slotMake()
{
  char act[1024];

  sprintf(act,"make=%s", (const char *) name.toUtf8());
  if(designer != NULL) { if(!save()) return; }   // Nestor: desde el Designer: guarda la mascara (o el bloque)
  else if(!maybeSave()) return;
  if(opt_develop.script == PV_LUA || name.isEmpty()) action(act);
  else buildProject(false); // Nestor: en la consola de compilacion (abajo)
}

void MainWindow::slotQtDesigner()
{
  int ret;
  char cmd[1024];
  if(opt_develop.arg_debug) printf("qtdesigner ui %s imask=%d\n", (const char *) name.toUtf8(), imask);

  if(name.isEmpty())
  {
    QMessageBox::information(this,"pvdevelop",tr("No project loaded"),QMessageBox::Ok);
    return;
  }

  // murx.rl.was.here ret = QMessageBox::warning(this, tr("pvdevelop"),
  //                   tr(exportWarning),
  //                   QMessageBox::Yes ,
  //                   QMessageBox::No  | QMessageBox::Default);
  ret = QMessageBox::warning(this, tr("pvdevelop"),
                     tr(exportWarning),
                     QMessageBox::Yes | QMessageBox::No);
  if(ret == QMessageBox::No) return;

  if(opt_develop.script == PV_LUA)
  {
    load("main.lua");
    editor->radioMain->setChecked(true);
  }
  else
  {
    load(name + ".pro");
    editor->radioProject->setChecked(true);
    perhapsFixCONFIG();
  }

  int mymask = editor->spinBoxMask->value();
  hide();
#ifdef PVWIN32
  sprintf(cmd,"wait pvdevelop -action=designerUi:%d %s", mymask, (const char *) name.toUtf8());
  if(opt_develop.arg_debug) printf("cmd=%s\n",cmd);
  ret = mysystem(cmd);
  if(ret < 0) printf("ERROR system(%s)\n", cmd);
#else
  sprintf(cmd,"pvdevelop -action=designerUi:%d %s", mymask,  (const char *) name.toUtf8());
  if(opt_develop.arg_debug) printf("cmd=%s\n",cmd);
  system(cmd);
#endif
  show();
}

void MainWindow::slotExportUI()
{
  char file[80];
  FILE *fin;

  imask = 1;
  if(editor != NULL) imask = editor->spinBoxMask->value();
  if(opt_develop.arg_debug) printf("export ui %s imask=%d\n", (const char *) name.toUtf8(), imask);

  if(name.isEmpty())
  {
    QMessageBox::information(this,"pvdevelop",tr("No project loaded"),QMessageBox::Ok);
    return;
  }

  sprintf(file, "mask%d.ui", imask);
  fin = fopen(file, "r");
  if(fin != NULL)
  {
    fclose(fin);
    int ret = QMessageBox::warning(this, tr("pvdevelop"),
                     tr("ui-File already exists.\n"
                        "Do you want to overwrite it ?"),
                     QMessageBox::Yes | QMessageBox::Default,
                     QMessageBox::No);
    if(ret == QMessageBox::No) return;
  }
  // murx.rl.was.here int ret = QMessageBox::warning(this, tr("pvdevelop"),
  //                   tr(exportWarning),
  //                   QMessageBox::Yes ,
  //                   QMessageBox::No  | QMessageBox::Default);
  int ret = QMessageBox::warning(this, tr("pvdevelop"),
                     tr(exportWarning),
                     QMessageBox::Yes | QMessageBox::No);
  if(ret == QMessageBox::No) return;
  export_ui(imask);
}

void MainWindow::slotImportUI()
{
  char cmd[1024];
  if(opt_develop.arg_debug) printf("import ui %s imask=%d\n", (const char *) name.toUtf8(), imask);

  if(name.isEmpty())
  {
    QMessageBox::information(this,"pvdevelop",tr("No project loaded"),QMessageBox::Ok);
    return;
  }

  if(opt_develop.script == PV_LUA)
  {
    load("main.lua");
    editor->radioMain->setChecked(true);
  }
  else
  {
    load(name + ".pro");
    editor->radioProject->setChecked(true);
    perhapsFixCONFIG();
  }

  int mymask = editor->spinBoxMask->value();
#ifdef PVUNIX
  sprintf(cmd,"xterm -e \"pvdevelop -action=importUi:%d %s && echo hit return && read\"", mymask,  (const char *) name.toUtf8());
  if(opt_develop.arg_debug) printf("cmd=%s\n",cmd);
  mysystem(cmd);
#else
  sprintf(cmd,"start pvb_import_ui.bat %s %d", (const char *) name.toUtf8(), mymask);
  if(opt_develop.arg_debug) printf("cmd=%s\n",cmd);
  int ret = system(cmd);
  if(ret < 0) printf("ERROR system(%s)\n", cmd);
#endif
}

void MainWindow::slotInsertMask()
{
  char act[1024];

  if(strlen(name.toUtf8()) > 1024-80) return;
  if(designer != NULL) viewEditor();
  sprintf(act,"insertMask=%s", (const char *) name.toUtf8());
  imask = action(act);
  slotSpinBoxMask(imask);
  editor->spinBoxMask->setValue(imask);
  viewDesigner();
}

void MainWindow::slotStartServer()
{
  int checked = 0;
  char command[1024];

  // Nestor: desde el Designer se queda en el Designer (guarda la mascara o el bloque)
  if(editor != NULL) { if(editor->checkBoxSuServer->checkState() == Qt::Checked) checked = 1; }
  else if(opt_develop.su) checked = 1;
  sprintf(command,"startserver=%d:%s",checked, (const char *) name.toUtf8());
  if(designer != NULL) { if(!save()) return; }
  else if(!maybeSave()) return;
  // Nestor: compila en la consola (abajo) y, si compilo, ejecuta el pvserver en su ventana
  // (Lua y "su" en Linux: como antes)
  if(opt_develop.script == PV_LUA || checked == 1 || name.isEmpty()) action(command);
  else buildProject(true);
}

void MainWindow::slotStartPvbrowser()
{
  action("pvbrowser");
}

void MainWindow::load( const QString &fileName )
{
  int ret;

  if(opt_develop.script == PV_LUA) editor->setSyntax(LUA_SYNTAX);
  if(name.isEmpty())
  {
    QMessageBox::information(this,"pvdevelop",tr("No project loaded"),QMessageBox::Ok);
    return;
  }
  if(maybeSave() == false)
  {
    doChecked = false;
    if(curFile.startsWith(name + ".pro"))   editor->radioProject->setChecked(true);
    else if(curFile.startsWith("main.cpp")) editor->radioMain->setChecked(true);
    else if(curFile.startsWith("pvapp.h"))  editor->radioHeader->setChecked(true);
    else if(curFile.contains("_slots.h"))   editor->radioSlots->setChecked(true);
    else if(curFile.contains(".py"))        editor->radioScript->setChecked(true);
    else if(curFile.contains("main.lua"))   editor->radioMain->setChecked(true);
    else if(curFile.contains("_slots.lua")) editor->radioSlots->setChecked(true);
    else if(curFile.startsWith("mask"))     editor->radioMask->setChecked(true);
    doChecked = true;
    return;
  }
  if(opt_develop.arg_debug) printf("load(%s)\n",(const char *) fileName.toUtf8());
  if(curFile != fileName) filePos[curFile] = editor->edit->textCursor().position(); // remember file pos
  if(opt_develop.arg_debug) printf("curFile=%s pos=%d\n", (const char *) curFile.toUtf8(), filePos[curFile]);
  QFile f(fileName);
  if(!f.open(QIODevice::ReadOnly))
  {
    if(fileName.contains("_slots") && opt_develop.script != PV_LUA)
    {
      editor->spinBoxMask->setValue(1);
      f.setFileName("mask1_slots.h");
      setCurrentFile("mask1_slots.h");
      if(!f.open(QIODevice::ReadOnly)) return;
    }
    else if(fileName.contains("_slots") && opt_develop.script == PV_LUA)
    {
      editor->spinBoxMask->setValue(1);
      f.setFileName("mask1_slots.lua");
      setCurrentFile("mask1_slots.lua");
      if(!f.open(QIODevice::ReadOnly)) return;
    }
    else if(fileName.contains(".py"))
    {
      editor->spinBoxMask->setValue(1);
      f.setFileName("mask1.py");
      setCurrentFile("mask1.py");
      if(!f.open(QIODevice::ReadOnly)) return;
    }
    else if(fileName.startsWith("mask") && opt_develop.script != PV_LUA)
    {
      editor->spinBoxMask->setValue(1);
      f.setFileName("mask1.cpp");
      setCurrentFile("mask1.cpp");
      if(!f.open(QIODevice::ReadOnly)) return;
    }
    else if(fileName.startsWith("mask") && opt_develop.script == PV_LUA)
    {
      editor->spinBoxMask->setValue(1);
      f.setFileName("mask1.lua");
      setCurrentFile("mask1.lua");
      if(!f.open(QIODevice::ReadOnly)) return;
    }
    else
    {
      return;
    }
  }
  QTextStream ts( &f );
  // murx.rl.was.here ts.setCodec("UTF-8");

  editor->setSyntax(CPP_SYNTAX);
  if(fileName.contains(".py"))  editor->setSyntax(PYTHON_SYNTAX);
  if(fileName.contains(".lua")) editor->setSyntax(LUA_SYNTAX);

  editor->edit->setPlainText( ts.readAll() );
  editor->edit->document()->setModified( false );
  setCurrentFile(fileName);
  f.close();
  getWidgetNames(fileName);
  if(fileName.startsWith("mask") && fileName.contains("_slots"))
  {
    slotComboEvents(editor->comboBoxEvent->currentIndex());
  }

  ret = readProject();
  if(ret == -1) { name.clear(); return; }
  syncBuildToolBar(); // Nestor: MinGW del proyecto
  syncMaskList();
  // add additional language here
  if     (opt_develop.script == PV_PYTHON) editor->radioScript->show();
  else if(opt_develop.script == PV_PERL)   editor->radioScript->show();
  else if(opt_develop.script == PV_PHP)    editor->radioScript->show();
  else if(opt_develop.script == PV_TCL)    editor->radioScript->show();
  else                             editor->radioScript->hide();
  if(opt_develop.script == PV_LUA)         editor->radioHeader->hide();
  else                             editor->radioHeader->show();
  if(opt_develop.script == PV_LUA)         editor->radioProject->hide();
  else                             editor->radioProject->show();
  statusBar()->showMessage( tr("Loaded document %1").arg(fileName), 2000 );

  int file_pos = filePos[fileName]; // position cursor
  if(opt_develop.arg_debug) printf("set to filePos=%d\n", file_pos);
  QTextCursor cursor(editor->edit->textCursor());
  cursor.setPosition(file_pos);
  editor->edit->setTextCursor(cursor);
  editor->edit->ensureCursorVisible();
}

void MainWindow::slotRadioProject(bool checked)
{
  if(checked & doChecked) load(name + ".pro");
}

void MainWindow::slotRadioMain(bool checked)
{
  if     (checked & doChecked && opt_develop.script==PV_LUA) load("main.lua");
  else if(checked & doChecked                      ) load("main.cpp");
}

void MainWindow::slotRadioHeader(bool checked)
{
  if(checked & doChecked) load("pvapp.h");
}

void MainWindow::slotRadioSlots(bool checked)
{
  if(checked & doChecked)
  {
    QString fname;
    if(opt_develop.script == PV_LUA)
    {
      //rlmurx-was-here fname.sprintf("mask%d_slots.lua",editor->spinBoxMask->value());
      fname = QString::asprintf("mask%d_slots.lua",editor->spinBoxMask->value());
    }
    else
    {
      //rlmurx-was-here fname.sprintf("mask%d_slots.h",editor->spinBoxMask->value());
      fname = QString::asprintf("mask%d_slots.h",editor->spinBoxMask->value());
    }
    load(fname.toUtf8());
  }
}

void MainWindow::slotRadioScript(bool checked)
{
  if(checked & doChecked)
  {
    QString fname;
    //rlmurx-was-here fname.sprintf("mask%d.py",editor->spinBoxMask->value());
    fname = QString::asprintf("mask%d.py",editor->spinBoxMask->value());
    load(fname.toUtf8());
  }
}

void MainWindow::slotRadioMask(bool checked)
{
  if(checked & doChecked)
  {
    QString fname;
    if(opt_develop.script == PV_LUA)
    {
      //rlmurx-was-here fname.sprintf("mask%d.lua",editor->spinBoxMask->value());
      fname = QString::asprintf("mask%d.lua",editor->spinBoxMask->value());
    }
    else
    {
      //rlmurx-was-here fname.sprintf("mask%d.cpp",editor->spinBoxMask->value());
      fname = QString::asprintf("mask%d.cpp",editor->spinBoxMask->value());
    }
    load(fname.toUtf8());
  }
}

void MainWindow::slotComboEvents(int i)
{
  if((curFile.contains("_slots.h") || curFile.contains("_slots.lua") || curFile.contains(".py")) && editor != NULL)
  {
    QString what = editor->comboBoxEvent->itemText(i);
    if(what.contains("//###"))
    {
    }
    else if(what.contains("typedef"))
    {
    }
    else
    {
      what = "slot" + what;
    }

    QPoint pos; // scroll to top of document
    QTextCursor cursor = editor->edit->cursorForPosition(pos);
    editor->edit->setTextCursor(cursor);
    editor->edit->ensureCursorVisible();

    if(editor->edit->find(what))
    {
      editor->edit->ensureCursorVisible();
    }
    else if(editor->edit->find(what,QTextDocument::FindBackward))
    {
      editor->edit->ensureCursorVisible();
    }
  }
}

void MainWindow::slotSpinBoxMask(int i)
{
  char myname[1024];
  FILE *fin;

  if(name.isEmpty())
  {
    editor->spinBoxMask->setValue(1);
    QMessageBox::information(this,"pvdevelop",tr("No project loaded"),QMessageBox::Ok);
    return;
  }

  if(opt_develop.script == PV_LUA) sprintf(myname,"mask%d.lua",i); // test if mask exists
  else                     sprintf(myname,"mask%d.cpp",i); // test if mask exists
  fin = fopen(myname,"r");
  if(fin == NULL)
  {
    editor->spinBoxMask->setValue(1);
    currentMask = 1;
    return;
  }
  fclose(fin);

  if(curFile.contains(".py")) // load file
  {
    sprintf(myname,"mask%d.py",i);
    load(myname);
  }
  else if(curFile.contains("_slots.lua"))
  {
    sprintf(myname,"mask%d_slots.lua",i);
    load(myname);
  }
  else if(curFile.contains("_slots.h"))
  {
    sprintf(myname,"mask%d_slots.h",i);
    load(myname);
  }
  else if(curFile.contains("mask") && opt_develop.script == PV_LUA)
  {
    sprintf(myname,"mask%d.lua",i);
    load(myname);
  }
  else if(curFile.contains("mask"))
  {
    sprintf(myname,"mask%d.cpp",i);
    load(myname);
  }
  currentMask = i;
  editor->spinBoxMask->setToolTip(maskTitle(i));   // Nestor: nombre de la pantalla
  statusBar()->showMessage(maskTitle(i), 3000);
  syncMaskList();
}

// Nestor: las pantallas en la barra del editor; la elegida, marcada y con su nombre bajo Masknr.
void MainWindow::syncMaskList()
{
  if(editor == NULL || name.isEmpty()) return;
  const int current = editor->spinBoxMask->value();
  const QSignalBlocker b(editor->masklist);
  editor->masklist->clear();
  for(int m : maskNumbers(QDir::currentPath(), opt_develop.script == PV_LUA))
  {
    QListWidgetItem *item = new QListWidgetItem(QString("mask%1").arg(m));
    item->setData(Qt::UserRole, m);
    item->setToolTip(item->text());
    editor->masklist->addItem(item);
    if(m == current) editor->masklist->setCurrentItem(item);
  }
  const QString label = QString("mask%1").arg(current);
  editor->maskName->setText(editor->maskName->fontMetrics().elidedText(label, Qt::ElideRight, editor->maskName->width()));
  editor->maskName->setToolTip(maskTitle(current));
}

void MainWindow::slotMaskList(QListWidgetItem *item)
{
  if(editor == NULL || item == NULL) return;
  const int m = item->data(Qt::UserRole).toInt();
  const bool maskFile = curFile.startsWith("mask");   // maskN.cpp, _slots, .py: sigue en el mismo tipo de archivo
  if(editor->spinBoxMask->value() != m) editor->spinBoxMask->setValue(m);
  if(!maskFile) editor->radioMask->setChecked(true);
  syncMaskList();   // si no se pudo cambiar (cambios sin guardar), vuelve a marcar la que esta abierta
}

void MainWindow::slotRllibUncommentRllib()
{
  if(opt_develop.arg_debug) printf("slotRllibUncommentRllib()\n");
  if(beginMenu())
  {
    QString project = name + ".pro";
    uncommentRllib(project.toUtf8());
    endMenu();
  }
}

void MainWindow::slotRllibUncommentModbus()
{
  if(opt_develop.arg_debug) printf("slotRllibUncommentModbus()\n");
  if(beginMenu())
  {
    uncommentModbus();
    endMenu();
  }
}

void MainWindow::slotRllibUncommentSiemenstcp()
{
  if(opt_develop.arg_debug) printf("slotRllibUncommentSiemenstcp()\n");
  if(beginMenu())
  {
    uncommentSiemenstcp();
    endMenu();
  }
}

void MainWindow::slotRllibUncommentPpi()
{
  if(opt_develop.arg_debug) printf("slotRllibUncommentPpi()\n");
  if(beginMenu())
  {
    uncommentPpi();
    endMenu();
  }
}

void MainWindow::slotDaemonModbus()
{
  if(opt_develop.arg_debug) printf("slotDaemonModbus()\n");
  if(beginMenu())
  {
    dlgDaemon dlg(MODBUS_DAEMON);
    dlg.run();
  }
}

void MainWindow::slotDaemonSiemenstcp()
{
  if(opt_develop.arg_debug) printf("slotDaemonSiemenstcp()\n");
  if(beginMenu())
  {
    dlgDaemon dlg(SIEMENSTCP_DAEMON);
    dlg.run();
  }
}

void MainWindow::slotDaemonPpi()
{
  if(opt_develop.arg_debug) printf("slotDaemonPpi()\n");
  if(beginMenu())
  {
    dlgDaemon dlg(PPI_DAEMON);
    dlg.run();
  }
}

void MainWindow::slotLinuxWriteStartscript()
{
  if(opt_develop.arg_debug) printf("slotLinuxWriteStartscript()\n");
  if(beginMenu())
  {
    int ret;
    ret = writeStartscript(QDir::current().path().toUtf8(), name.toUtf8());
    if(ret != 0)
    {
      QMessageBox::information(this,"pvdevelop",tr("Could not write startscript"),QMessageBox::Ok);
    }
    else
    {
      QMessageBox::information(this,"pvdevelop",
                                    tr("traditional startscript written to project directory\n"
                                    "please review it, copy it to /etc/init.d/ and activate it through runlevel editor\n"
                                    "\n"
                                    "systemd <name>.service startscript also written to project directory"),
                                    QMessageBox::Ok);
    }
  }
}

void MainWindow::slotAboutManual()
{
  if(opt_develop.arg_debug) printf("slotAboutManual()\n");
  if(dlgtextbrowser != NULL) dlgtextbrowser->show();
}

void MainWindow::slotInsertFunction()
{
  if(opt_develop.arg_debug) printf("slotInsertFunction()\n");
  if(name.isEmpty())
  {
    QMessageBox::information(this,"pvdevelop",tr("No project loaded"),QMessageBox::Ok);
    return;
  }
  dlgInsertFunction dlg;
  QString ret = dlg.run();
  if(opt_develop.arg_debug) printf("ret=%s\n",(const char *) ret.toUtf8());
  if(ret.isEmpty()) return;
  if((ret.contains(");") || ret.contains("=") || ret.contains("BIT")) && !ret.contains("(...)"))
  {
    if(curFile.contains(".py")) ret.remove(";");
    if(opt_develop.script == PV_LUA)
    {
      ret.remove(";");
      QString tmp;
      if(ret.startsWith("pv")) tmp = "pv.";
      if(ret.startsWith("rl")) tmp = "rllib.";
      tmp += ret;
      ret = tmp;
    }
    editor->edit->textCursor().insertText(ret);
    return;
  }
  QMessageBox::information(this,"pvdevelop",tr("Please choose function from tree"),QMessageBox::Ok);
}

int MainWindow::beginMenu()
{
  if(name.isEmpty())
  {
    QMessageBox::information(this,"pvdevelop",tr("No project loaded"),QMessageBox::Ok);
    return 0;
  }
  maybeSave();
  return 1;
}

void MainWindow::endMenu()
{
  if(editor != NULL)
  {
    if(opt_develop.script == PV_LUA)
    {
      load(name + "main.lua");
      editor->radioMain->setChecked(true);
    }
    else
    {
      load(name + ".pro");
      editor->radioProject->setChecked(true);
      perhapsFixCONFIG();
    }
  }
}

void MainWindow::getWidgetNames(const QString &filename)
{
  if(editor == NULL) return;
  if(opt_develop.arg_debug) printf("getWidgetNames(%s)\n",(const char *) filename.toUtf8());

  char buf[1024];
  int i;
  QString line;
  editor->widgetname->clear();
  if(!filename.startsWith("mask")) return;
  QTextStream in;
  QString name;
  if(opt_develop.script == PV_LUA) name = "mask" + editor->spinBoxMask->text() + ".lua";
  else                     name = "mask" + editor->spinBoxMask->text() + ".cpp";
  QFile mask(name);
  if(!mask.open(QFile::ReadOnly)) return;

  in.setDevice(&mask);
  while(1)
  {
    line = in.readLine();
    if(line.isNull()) break;
    if(line.contains("ID_MAIN_WIDGET")) break;
  }
  while(1)
  {
    line = in.readLine();
    if(line.isNull()) break;
    if(line.contains("ID_END_OF_WIDGETS")) break;
    else
    {
      line.remove(" ");
      if(line.length() < (int) (sizeof(buf) - 1))
      {
        strcpy(buf,line.toUtf8());
        for(i=0; buf[i] != '\0'; i++)
        {
          if(buf[i]==',' || buf[i]=='=' || buf[i]==';')
          {
            buf[i] = '\0';
            break;
          }
        }
        editor->widgetname->addItem(buf);
      }
    }
  }
  mask.close();
}

void MainWindow::slotWidgetname(QListWidgetItem *item)
{
  QString text = item->text();
  if(opt_develop.arg_debug) printf("slotWidgetname %s\n", (const char *) text.toUtf8());

  dlgPasteWidget dlg;
  QString ret = dlg.run(text.toUtf8(), &curFile);
  if(!ret.isEmpty())
  {
    editor->edit->textCursor().insertText(ret);
    return;
  }
}

void MainWindow::slotFileOpt()
{
int ret;

  if(opt_develop.arg_debug) printf("slotFileOpt\n");
  DlgOpt *dlg = new DlgOpt(this);
  dlg->setWindowTitle("pvdevelop options");
  dlg->setFilename(inifile());
  ret = dlg->exec();
  if(ret == QDialog::Accepted)
  {
    readIniFile();
    delete dlgtextbrowser;
    dlgtextbrowser = new dlgTextBrowser(opt_develop.manual);
  }
  delete dlg;
}

void MainWindow::keyPressEvent(QKeyEvent * event)
{
  if(event->modifiers() & Qt::AltModifier)     opt_develop.altPressed = 1;
  else                                         opt_develop.altPressed = 0;
  if(event->modifiers() & Qt::ControlModifier) opt_develop.ctrlPressed = 1;
  else                                         opt_develop.ctrlPressed = 0;
  if(event->modifiers() & Qt::ShiftModifier)   opt_develop.shiftPressed = 1;
  else                                         opt_develop.shiftPressed = 0;

  if(designer != NULL && designer->root != NULL)
  {
    if(event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
      designer->root->deleteLastChild();
      designer->root->stackClear();
    }
    else if(event->key() == Qt::Key_P)
    {
      designer->root->showProperties();
    }
    else if(event->key() == Qt::Key_G)
    {
      designer->root->GrabMouse();
    }
    else if(event->key() == Qt::Key_R)
    {
      designer->root->ReleaseMouse();
    }
    else if(event->key() == Qt::Key_L)
    {
      designer->root->EditLayout();
    }
    else if(event->key() == Qt::Key_Left  ||
            event->key() == Qt::Key_Right ||
            event->key() == Qt::Key_Up    ||
            event->key() == Qt::Key_Down  )
    {
      designer->root->MoveKey(event->key());
    }
    else if(event->key() == Qt::Key_Z && opt_develop.ctrlPressed)
    {
      designer->root->pop(NULL);
    }
  }
  //printf("ctrlPressed=%d\n",opt_develop.ctrlPressed);
  QWidget::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent * event)
{
  opt_develop.altPressed = 0;
  opt_develop.ctrlPressed = 0;
  opt_develop.shiftPressed = 0;
  //printf("ctrlPressed=%d\n",opt_develop.ctrlPressed);
  QWidget::keyPressEvent(event);
}

//------------------------------------------------------------------------------------------------
// Nestor: consola de compilacion abajo (Accion > Consola de compilacion, Ctrl+J), errores marcados
// en el editor y el historiador que se inicia con el pvserver (Ctrl+T)

BuildConsole *g_buildConsole = NULL; // la usa tambien la ventana de los daemons (dlgdaemon.cpp)

void MainWindow::createBuildConsole()
{
  buildConsole = new BuildConsole(this);
  g_buildConsole = buildConsole;
  buildDock = new QDockWidget(tr("Compilacion"), this);
  buildDock->setObjectName("buildDock");
  buildDock->setWidget(buildConsole);
  buildDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
  addDockWidget(Qt::BottomDockWidgetArea, buildDock);
  buildDock->hide();
  QAction *toggle = buildDock->toggleViewAction();
  toggle->setText(tr("Consola de &compilacion"));
  toggle->setShortcut(tr("Ctrl+J"));
  toggle->setStatusTip(tr("Muestra u oculta la salida de la compilacion (Ctrl+M, Ctrl+T y daemons)"));
  actionMenu->addSeparator();
  actionMenu->addAction(toggle);
  connect(buildConsole, &BuildConsole::started, buildDock, &QDockWidget::show);
  connect(buildConsole, &BuildConsole::openLocation, this, &MainWindow::openLocation);
  connect(buildConsole, &BuildConsole::finished, this, [this](bool) { applyErrorMarks(); });
  // agrandar / achicar (tambien se arrastra su borde) y separar en una ventana; la altura se recuerda
  connect(buildConsole, &BuildConsole::resizeRequested, this, [this](int delta) {
    if(buildDock->isFloating()) { buildDock->resize(buildDock->width(), qMax(80, buildDock->height() + delta)); return; }
    resizeDocks(QList<QDockWidget *>() << buildDock, QList<int>() << qMax(40, buildDock->height() + delta), Qt::Vertical);
  });
  connect(buildConsole, &BuildConsole::floatRequested, this, [this]() { buildDock->setFloating(!buildDock->isFloating()); });
  buildDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
  // el tamanio se guarda cada vez que cambia (arrastrar el borde, agrandar/achicar) y se vuelve a poner
  // al mostrarla, despues de que Qt la ubica (si no, al compilar aparecia con la mitad de la ventana)
  buildDock->installEventFilter(this);
  connect(buildDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
    if(!visible) return;
    buildDockRestoring = true;
    QTimer::singleShot(0, this, [this]() { restoreBuildDockSize(); });
    QTimer::singleShot(300, this, [this]() { buildDockRestoring = false; });
  });
  connect(buildDock, &QDockWidget::topLevelChanged, this, [this](bool) {
    buildDockRestoring = true;
    QTimer::singleShot(0, this, [this]() { restoreBuildDockSize(); });
    QTimer::singleShot(300, this, [this]() { buildDockRestoring = false; });
  });

  // Nestor: barra del proyecto (entre la vista y alinear): el monitor de tags
  tagMonitorAct->setIcon(toolIcon(8));
  projectToolBar = new QToolBar(tr("Proyecto"), this);
  projectToolBar->setObjectName("projectToolBar");
  insertToolBar(alignToolBar, projectToolBar);
  projectToolBar->addAction(tagMonitorAct);
}

// Nestor: la consola de compilacion recuerda su tamanio (abajo: alto; a un costado: ancho; separada: tamanio)
bool MainWindow::eventFilter(QObject *o, QEvent *e)
{
  if(o == buildDock && e->type() == QEvent::Resize && buildDock->isVisible() && !buildDockRestoring)
  {
    QSettings st("pvbrowser", "pvdevelop");
    if(buildDock->isFloating()) st.setValue("build/floatsize", buildDock->size());
    else if(dockWidgetArea(buildDock) == Qt::BottomDockWidgetArea) { if(buildDock->height() > 40) st.setValue("build/dockheight", buildDock->height()); }
    else if(buildDock->width() > 60) st.setValue("build/dockwidth", buildDock->width());
  }
  return QMainWindow::eventFilter(o, e);
}

void MainWindow::restoreBuildDockSize()
{
  QSettings st("pvbrowser", "pvdevelop");
  if(buildDock->isFloating())
  {
    buildDock->resize(st.value("build/floatsize", QSize(700, 260)).toSize());
    return;
  }
  // a lo sumo el 60% de la ventana (un valor viejo muy grande no la tapa)
  if(dockWidgetArea(buildDock) == Qt::BottomDockWidgetArea)
    resizeDocks(QList<QDockWidget *>() << buildDock, QList<int>() << qBound(60, st.value("build/dockheight", 220).toInt(), qMax(60, height() * 6 / 10)), Qt::Vertical);
  else
    resizeDocks(QList<QDockWidget *>() << buildDock, QList<int>() << qBound(150, st.value("build/dockwidth", 450).toInt(), qMax(150, width() * 6 / 10)), Qt::Horizontal);
}

// Nestor: iconos de la barra Proyecto, dibujados (no hacen falta archivos)
QIcon MainWindow::toolIcon(int kind)
{
  // mismo estilo que los iconos de alinear (alignIcon): 24x24, plano, guias grises y objetos azules
  QPixmap pm(24, 24);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, false);
  const QColor guide(128, 128, 128);
  const QColor box(30, 136, 229);
  p.setPen(Qt::NoPen);
  switch(kind)
  {
    case 0:   // procesos: lista de procesos con su estado
      for(int i=0; i<3; i++)
      {
        p.fillRect(2, 3 + i * 7, 5, 5, box);
        p.fillRect(9, 4 + i * 7, 13, 3, guide);
      }
      break;
    case 1:   // bloques: un grupo de widgets dentro de un marco
      p.fillRect(1, 1, 22, 1, guide);  p.fillRect(1, 22, 22, 1, guide);
      p.fillRect(1, 1, 1, 22, guide);  p.fillRect(22, 1, 1, 22, guide);
      p.fillRect(4, 4, 16, 6, box);
      p.fillRect(4, 13, 7, 7, box);    p.fillRect(13, 13, 7, 7, box);
      break;
    case 2:   // recursos: imagen
    {
      p.fillRect(1, 3, 22, 1, guide);  p.fillRect(1, 20, 22, 1, guide);
      p.fillRect(1, 3, 1, 18, guide);  p.fillRect(22, 3, 1, 18, guide);
      p.fillRect(16, 6, 4, 4, guide);
      QPolygon m;
      m << QPoint(3, 19) << QPoint(9, 9) << QPoint(14, 16) << QPoint(16, 13) << QPoint(21, 19);
      p.setBrush(box);
      p.drawPolygon(m);
      break;
    }
    case 3:   // usuarios, alarmas y auditoria: usuario con candado
    {
      p.setRenderHint(QPainter::Antialiasing, true);    // cabeza y hombros redondeados
      p.setBrush(box);
      p.drawEllipse(QRectF(4.5, 1.5, 8, 8));             // cabeza
      QPainterPath body;                                  // hombros
      body.moveTo(1.5, 22.5);
      body.lineTo(1.5, 18);
      body.cubicTo(1.5, 13, 5, 11, 8.5, 11);
      body.cubicTo(12, 11, 15.5, 13, 15.5, 18);
      body.lineTo(15.5, 22.5);
      body.closeSubpath();
      p.drawPath(body);
      p.setBrush(Qt::NoBrush);                            // candado
      p.setPen(QPen(guide, 2));
      p.drawArc(QRectF(15.5, 7.5, 6, 8), 0, 180 * 16);
      p.drawLine(QPointF(15.5, 11.5), QPointF(15.5, 13));
      p.drawLine(QPointF(21.5, 11.5), QPointF(21.5, 13));
      p.setPen(Qt::NoPen);
      p.setBrush(guide);
      p.drawRoundedRect(QRectF(13.5, 12.5, 10, 10), 1.5, 1.5);
      p.setBrush(Qt::white);
      p.drawEllipse(QRectF(17.5, 15.5, 2, 2));
      p.fillRect(QRectF(18, 17, 1, 3), Qt::white);
      break;
    }
    case 6:   // recetas: planilla con valores
      p.fillRect(3, 4, 18, 19, box);
      p.fillRect(8, 2, 8, 4, guide);
      for(int i=0; i<3; i++)
      {
        p.fillRect(6, 9 + i * 4, 8, 2, Qt::white);
        p.fillRect(16, 9 + i * 4, 2, 2, Qt::white);
      }
      break;
    case 7:   // automatizacion: aviso (sobre)
    {
      p.fillRect(2, 6, 20, 14, box);
      p.setRenderHint(QPainter::Antialiasing, true);
      p.setPen(QPen(Qt::white, 1.6));
      p.drawLine(QPointF(3, 7), QPointF(12, 14));
      p.drawLine(QPointF(21, 7), QPointF(12, 14));
      p.setPen(Qt::NoPen);
      p.setBrush(guide);
      p.drawEllipse(QRectF(16, 1, 7, 7));   // aviso pendiente
      break;
    }
    case 8:   // monitor de tags: tabla con un valor resaltado
      p.fillRect(2, 2, 20, 1, guide);  p.fillRect(2, 21, 20, 1, guide);
      p.fillRect(2, 2, 1, 20, guide);  p.fillRect(21, 2, 1, 20, guide);
      for(int i=0; i<4; i++)
      {
        p.fillRect(5, 5 + i * 4, 7, 2, guide);
        p.fillRect(14, 5 + i * 4, 5, 2, box);
      }
      p.fillRect(13, 12, 7, 4, box);   // el valor forzado
      break;
    case 9:   // referencias cruzadas: lista con una lupa
    {
      for(int i=0; i<4; i++) p.fillRect(2, 3 + i * 5, 12, 2, guide);
      p.setRenderHint(QPainter::Antialiasing, true);
      p.setPen(QPen(box, 2.2));
      p.setBrush(Qt::NoBrush);
      p.drawEllipse(QRectF(10.5, 9.5, 8, 8));
      p.drawLine(QPointF(17.5, 16.5), QPointF(22, 21));
      p.setPen(Qt::NoPen);
      break;
    }
    case 10:  // instalador del proyecto: caja con una flecha hacia abajo
    {
      p.fillRect(3, 11, 18, 11, box);
      p.fillRect(3, 11, 18, 2, guide);
      p.fillRect(10, 1, 4, 8, guide);
      QPolygon arrow;
      arrow << QPoint(6, 8) << QPoint(18, 8) << QPoint(12, 14);
      p.setBrush(guide);
      p.drawPolygon(arrow);
      break;
    }
    case 4:   // simulacion: iniciar
    {
      QPolygon t;
      t << QPoint(6, 3) << QPoint(20, 12) << QPoint(6, 21);
      p.setBrush(QColor(46, 160, 67));
      p.drawPolygon(t);
      break;
    }
    default:  // detener
      p.fillRect(5, 5, 14, 14, QColor(211, 47, 47));
      break;
  }
  p.end();
  return QIcon(pm);
}

// Ctrl+M (startServer = false) y Ctrl+T (true): compila en la consola; Ctrl+T ejecuta el pvserver si compilo
void MainWindow::buildProject(bool startServer)
{
  const QString project = name;
  // Nestor: con una compilacion en curso no se mata nada (antes se cerraba el servidor y run() la rechazaba igual)
  if(buildConsole->running()) { buildConsole->appendLine(tr("Ya hay una compilacion en curso: esperar o Detener"), "#b00000"); return; }
  std::function<void(bool)> done = [this, project, startServer](bool ok) {
    if(ok && startServer) startServerProcess(project);
  };
#ifdef PVWIN32
  offerMinGWLibDir(project);
  perhapsAddMsvcScope(project.toUtf8().constData());
  // si el servidor sigue abierto no se puede reemplazar el .exe al enlazar
  QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << project + ".exe");
  if(startServer) QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << "pvs.exe");
  buildConsole->run(project, "cmd.exe", QStringList() << "/C" << "pvb_build.bat" << project, done);
#else
  // Nestor: el nombre entre comillas simples (sh -c)
  QString cmd = QString("fake_qmake %1 '%2.pro' && make -j%3").arg(opt_develop.fake_qmake).arg(QString(project).replace("'", "'\\''")).arg(buildJobs());
  buildConsole->run(project, "sh", QStringList() << "-c" << cmd, done);
#endif
}

// Nestor: el ejecutable del pvserver del proyecto (en la carpeta actual)
QString MainWindow::serverExePath(const QString &project)
{
#ifdef PVWIN32
  return QDir::current().absoluteFilePath("release/" + project + ".exe");
#elif defined(PVMAC)
  return QDir::current().absoluteFilePath(QString("%1.app/Contents/MacOS/%1").arg(project));
#else
  return QDir::current().absoluteFilePath(project);
#endif
}

// el pvserver en su propia ventana (ahi escribe sus mensajes)
void MainWindow::startServerProcess(const QString &project)
{
  // Nestor: pvbStartInConsole (buildconsole.cpp): sin cmd /C start ni xterm fijo (macOS no lo trae)
  const QString exe = serverExePath(project);
  bool ok = pvbStartInConsole(QDir::toNativeSeparators(exe), QStringList(), QDir::currentPath(), project);
  buildConsole->appendLine(ok ? tr("%1 iniciado").arg(project) : tr("No se pudo iniciar %1").arg(project), ok ? "#006000" : "#b00000");
}

// errores y advertencias de la ultima compilacion en el archivo que muestra el editor
void MainWindow::applyErrorMarks()
{
  if(editor == NULL || buildConsole == NULL) return;
  QList<QTextEdit::ExtraSelection> sel;
  const QMap<int, bool> marks = buildConsole->marksFor(curFile);
  QTextDocument *doc = editor->edit->document();
  for(QMap<int, bool>::const_iterator i = marks.constBegin(); i != marks.constEnd(); ++i)
  {
    QTextBlock b = doc->findBlockByNumber(i.key() - 1);
    if(!b.isValid()) continue;
    QTextEdit::ExtraSelection s;
    s.format.setBackground(i.value() ? QColor(255, 210, 210) : QColor(255, 236, 190));
    s.format.setProperty(QTextFormat::FullWidthSelection, true);
    s.cursor = QTextCursor(b);
    QStringList tips;
    for(const BuildMessage &m : buildConsole->messages())
      if(m.line == i.key() && m.file.compare(curFile, Qt::CaseInsensitive) == 0) tips << m.text;
    s.format.setToolTip(tips.join('\n'));
    sel << s;
  }
  editor->edit->setExtraSelections(sel);
}

// doble clic en un error de la consola: abre el archivo del proyecto en esa linea
void MainWindow::openLocation(const QString &file, int line, int column)
{
  if(editor == NULL) viewEditor();
  if(!QFile::exists(file) || QFileInfo(file).isAbsolute())
  {
    statusBar()->showMessage(tr("%1 no es un archivo del proyecto").arg(file), 4000);
    return;
  }
  if(curFile != file)
  {
    doChecked = false;
    if(file == name + ".pro")             editor->radioProject->setChecked(true);
    else if(file == "main.cpp")           editor->radioMain->setChecked(true);
    else if(file == "pvapp.h")            editor->radioHeader->setChecked(true);
    else if(file.contains("_slots"))      editor->radioSlots->setChecked(true);
    else if(file.startsWith("mask"))      editor->radioMask->setChecked(true);
    doChecked = true;
    static const QRegularExpression rx("^mask(\\d+)");
    QRegularExpressionMatch m = rx.match(file);
    if(m.hasMatch()) editor->spinBoxMask->setValue(m.captured(1).toInt());
    load(file);
    if(curFile != file) return; // no se pudo (cambios sin guardar)
  }
  QTextBlock b = editor->edit->document()->findBlockByNumber(line - 1);
  if(!b.isValid()) return;
  QTextCursor c(b);
  if(column > 1) c.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, qMin(column - 1, b.length() - 1));
  editor->edit->setTextCursor(c);
  editor->edit->ensureCursorVisible();
  editor->edit->setFocus();
}
