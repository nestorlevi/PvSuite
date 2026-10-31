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
**
** Nestor: editor de tags de los daemons (modbus, siemensTCP, OPC UA, MQTT).
** Lee las variables, estructuras y ajustes del texto del .mk (y el .csv de "tags=") y
** devuelve el texto del .mk actualizado; si se eligio CSV, escribe tambien el .csv.
**
****************************************************************************/
#ifndef DLG_TAGS_H
#define DLG_TAGS_H

#include <QDialog>
#include <QStringList>
#include <QSet>
#include <QStyledItemDelegate>
#include <set>
#include <vector>
#include "../rllib/lib/rltagdef.h"

class Ui_DlgTags;
class QTableWidget;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QLabel;

// Nestor: texto y tildes centrados en las tablas del editor de tags
class CenterDelegate : public QStyledItemDelegate
{
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
protected:
  void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
  bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;
};

class DlgTags : public QDialog
{
  Q_OBJECT
public:
  DlgTags(int protocol, const QString &mkfile, const QString &text, QWidget *parent = nullptr);
  ~DlgTags();
  QString resultText() const { return result; }
  //! Nestor: el usuario eligio otro daemon (del mismo protocolo) en el selector de arriba: abrirlo despues
  QString switchFile() const { return switchTo; }
  //! Nestor: selecciona la variable de un tag (Ctrl+clic en el editor): "Motores[0].Marcha" -> la fila Motores
  void selectTag(const QString &name);

private slots:
  void slotAdd();
  void slotDuplicate();
  void slotRemove();
  void slotImport();
  void slotExport();
  void slotCheck();
  void slotNotation(int index);
  void slotBrowseCsv();
  void slotStoreChanged();
  void slotOk();
  // estructuras
  void slotStructSelected(int row);
  void slotStructNew();
  void slotStructRename();
  void slotStructRemove();
  void slotMemberAdd();
  void slotMemberRemove();
  void slotMemberUp();
  void slotMemberDown();
  // historial y alarmas
  void slotTabChanged(int index);

private:
  enum Column { C_NAME, C_ADDRESS, C_TYPE, C_COUNT, C_STRIDE, C_RW, C_SLAVE, C_SCAN, C_SCALE, C_OFFSET, C_ORDER, C_DESC, C_HIST,
                C_FORMAT, C_JSON, C_CMD, C_QOS, C_RETAIN, C_STALE };   // Nestor: las C_FORMAT.. solo con MQTT
  bool textAddr() const { return protocol == rlTagList::OPCUA || protocol == rlTagList::MQTT; }   // NodeId / topico
  enum MemberColumn { M_NAME, M_AT, M_TYPE, M_COUNT, M_RW, M_SCALE, M_OFFSET, M_ORDER, M_DESC, M_HIST, M_HON };
  enum HistColumn { H_NAME, H_LOG, H_DEADBAND, H_LOGMAX, H_UNIT, H_ALARM, H_HYST, H_TEXT, H_GROUP, H_PRIORITY, H_ACK, H_EVENT, H_COUNT };
  // historial: cada fila de la tabla de variables lleva su rlTagHist (texto "clave=valor") en
  // Qt::UserRole del item del nombre; la pestania Historial lo muestra en columnas
  void    buildHistTab();
  void    showHist();                            // variables -> tabla de historial
  void    storeHist();                           // tabla de historial -> variables
  static rlTagHist histFromText(const QString &pairs);
  static QString   histText(const rlTagHist &h);
  QWidget      *tabHist;
  QTableWidget *tableHist;
  bool          histShown;
  std::vector< std::pair<std::string, rlTagHist> > sysHist; // tags de sistema (lineas "sys")
  std::vector<std::string> sysRows() const;               // sus filas en la pestania Historial
  struct HistRow { enum Kind { TAG, MEMBER, SYS }; int kind; int a, b; std::string sys; }; // fila de la pestania Historial
  std::vector<HistRow> histRows;                         // TAG: fila de variables; MEMBER: estructura y miembro
  std::set<std::string> memberHistOn;                    // "Estructura.miembro" con Historial marcado
  QCheckBox    *checkSys;                                 // mostrar los tags de sistema
  QLabel       *labelHistEmpty;
  QComboBox    *comboConsole;                             // console= (mensajes del daemon)
  QSpinBox     *spinConsoleInterval;                      // console_interval=
  void    addRow(const rlTagDef &t);
  rlTagDef rowTag(int row) const;
  void    fillList(rlTagList &list);             // ajustes, estructuras y filas de la tabla
  int     currentNotation() const;
  QString csvPath() const;
  bool    check(QString *report);                // false si hay errores
  QComboBox *typeCombo(QWidget *parent, bool structs) const;
  QComboBox *orderCombo(QWidget *parent) const;
  void    fitComboColumn(QTableWidget *table, int column, QComboBox *combo);
  void    refreshTypeCombos();                   // las estructuras cambiaron
  // estructuras
  void    showStruct(int index);
  void    storeStruct();                         // tabla de miembros -> structs[currentStruct]
  void    addMemberRow(const rlTagMember &m);
  rlTagMember memberRow(int row) const;
  void    swapMembers(int a, int b);

  Ui_DlgTags *form;
  int protocol;
  int notation;
  QString mkfile;
  QStringList otherLines;                        // lineas del .mk que no son de tags
  std::vector<rlTagStruct> structs;
  int currentStruct;
  QString result;
  // Nestor: estructuras compartidas del proyecto y cambio de daemon
  QString sharedStructsPath() const;             // <proyecto>.mkstruct (carpeta del .mk)
  QSet<QString> sharedLoaded;                    // compartidas que se cargaron al abrir
  QString switchTo;
  QComboBox *comboDaemon;
};

#endif
