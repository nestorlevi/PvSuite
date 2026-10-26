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
** Nestor: ayuda integrada de pvdevelop (Ayuda > Manual, Ctrl+H; F1 en cualquier ventana).
**
** Lee las paginas que genera doc/herramientas/ayuda_html.py a partir del manual:
**   <doc>/ayuda/<es|en|pt|de>/indice.tsv     archivo <TAB> ancla <TAB> nivel (1 capitulo, 2 seccion) <TAB> titulo
**   <doc>/ayuda/<idioma>/<archivo>.html      un archivo por capitulo
** Arbol de capitulos y secciones a la izquierda, texto a la derecha, busqueda en todo el texto (sin
** distinguir mayusculas ni acentos), adelante/atras y el manual completo en PDF.
** F1 abre el tema de la ventana activa (pvbHelpTopicFor).
****************************************************************************/
#ifndef DLGHELP_H
#define DLGHELP_H

#include <QDialog>
#include <QObject>
#include <QStringList>
#include <QList>
#include <functional>

class Ui_DlgHelp;
class QTreeWidgetItem;
class QListWidgetItem;
class QUrl;

class DlgHelp : public QDialog
{
  Q_OBJECT
public:
  //! docDir: la carpeta doc de la instalacion (la que tiene index.html, ayuda/ y manual/)
  explicit DlgHelp(const QString &docDir, QWidget *parent = nullptr, const QString &language = QString());   // language vacio: el de pvdevelop
  ~DlgHelp();
  //! la carpeta doc con la ayuda: la configurada (Ayuda/Opciones), y si no la tiene, las instaladas y de desarrollo
  //! (PVBDIR, SUITEDIR, junto al ejecutable y hacia arriba, /opt/pvb, paquete de macOS). Vacio si no hay ninguna
  static QString findDocDir(const QString &configured, const QString &language = QString());
  //! la carpeta con los manuales PDF (docDir/manual u otra de las anteriores); vacio si no hay
  static QString findManualDir(const QString &docDir);
  //! hay paginas de ayuda en docDir (si no, MainWindow usa el visor viejo)
  static bool available(const QString &docDir, const QString &language = QString());
  QString docDirectory() const { return docDir; }
  //! muestra un tema: ancla de capitulo ("designer") o de seccion ("s4-7"); vacio = inicio
  void showTopic(const QString &topic);
  //! texto de las secciones que contienen todas las palabras (para las pruebas); indices en entries
  QStringList search(const QString &text) const;
  int entryCount() const { return entries.size(); }
  QString languageCode() const { return lang; }
  //! el PDF del manual en el idioma de la ayuda (vacio si no esta)
  QString pdfFile() const;

private slots:
  void slotSearchChanged(const QString &text);
  void slotResult(QListWidgetItem *item);
  void slotTree(QTreeWidgetItem *item);
  void slotAnchor(const QUrl &url);
  void slotSource(const QUrl &url);
  void slotHome();
  void slotPdf();
  void slotFolder();

private:
  struct Entry { QString file, anchor, title, chapter; int level = 1; };
  void load();
  QList<int> searchIndices(const QString &text) const;
  void buildTree();
  void loadTexts() const;
  void applyStyle();
  QString plain(const QString &s) const;
  QString fold(const QString &s) const;       // minusculas y sin acentos
  int findEntry(const QString &file, const QString &anchor) const;
  void select(int entry);

  Ui_DlgHelp *ui;
  QString docDir, helpDir, lang;
  QList<Entry> entries;
  mutable QStringList texts;                  // texto plano (sin acentos ni mayusculas) de cada entrada
  mutable bool textsLoaded = false;
  QList<QTreeWidgetItem *> treeItems;         // uno por entrada
  bool syncing = false;
  QString lastTerm;
};

// F1 en cualquier ventana de pvdevelop: abre la ayuda en el tema de esa ventana.
class HelpKeyFilter : public QObject
{
  Q_OBJECT
public:
  //! open(topic, ventana desde la que se pidio); mainTopic: tema de la ventana principal (Designer o editor)
  HelpKeyFilter(QWidget *mainWindow, std::function<QString()> mainTopic,
                std::function<void(const QString &, QWidget *)> open, QObject *parent = nullptr);
protected:
  bool eventFilter(QObject *o, QEvent *e) override;
private:
  QWidget *mainWindow;
  std::function<QString()> mainTopic;
  std::function<void(const QString &, QWidget *)> open;
};

// tema de ayuda de una ventana de pvdevelop segun su clase ("" = el inicio); para las pruebas
QString pvbHelpTopicFor(const QObject *window);

#endif
