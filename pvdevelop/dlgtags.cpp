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
** Nestor: editor de tags de los daemons (ver dlgtags.h)
**
****************************************************************************/
#include "dlgtags.h"
#include "ui_dlgtags.h"
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QHeaderView>
#include <QPushButton>
#include <QInputDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QHBoxLayout>
#include <cmath>
#include <algorithm>
#include <QStyledItemDelegate>
#include <QApplication>
#include <QMouseEvent>
// Qt 5 y Qt 6
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
static QPoint mousePos(const QMouseEvent *e) { return e->position().toPoint(); }
#else
static QPoint mousePos(const QMouseEvent *e) { return e->pos(); }
#endif
#include <QLineEdit>
#include <QCheckBox>
#include "pvimages.h"   // pvbProjectName
#include "pvtablesort.h"   // Nestor: clic en el titulo: ordenar

// Nestor: texto centrado en las tablas del editor de tags (tambien al editar) y tildes centrados
void CenterDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
  QStyledItemDelegate::initStyleOption(option, index);
  option->displayAlignment = Qt::AlignCenter;
}

QWidget *CenterDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  QWidget *w = QStyledItemDelegate::createEditor(parent, option, index);
  if(QLineEdit *e = qobject_cast<QLineEdit *>(w)) e->setAlignment(Qt::AlignCenter);
  return w;
}

static bool isCheckOnly(const QModelIndex &index)
{
  return index.data(Qt::CheckStateRole).isValid() && index.data(Qt::DisplayRole).toString().isEmpty();
}

static QRect checkRect(const QStyleOptionViewItem &option)
{
  QStyleOptionButton b;
  QRect r = QApplication::style()->subElementRect(QStyle::SE_CheckBoxIndicator, &b, option.widget);
  r.moveCenter(option.rect.center());
  return r;
}

void CenterDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  if(!isCheckOnly(index)) { QStyledItemDelegate::paint(painter, option, index); return; }
  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);
  Qt::CheckState state = (Qt::CheckState) index.data(Qt::CheckStateRole).toInt();
  opt.features &= ~QStyleOptionViewItem::HasCheckIndicator; // fondo y seleccion, sin el tilde a la izquierda
  QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
  style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
  QStyleOptionViewItem ind = opt;
  ind.rect = checkRect(option);
  ind.state = ind.state & ~QStyle::State_HasFocus;
  ind.state |= (state == Qt::Checked) ? QStyle::State_On : (state == Qt::PartiallyChecked) ? QStyle::State_NoChange : QStyle::State_Off;
  if(!(index.flags() & Qt::ItemIsEnabled)) ind.state &= ~QStyle::State_Enabled;
  style->drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &ind, painter, opt.widget);
}

bool CenterDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
  if(!isCheckOnly(index)) return QStyledItemDelegate::editorEvent(event, model, option, index);
  if(!(index.flags() & Qt::ItemIsUserCheckable) || !(index.flags() & Qt::ItemIsEnabled)) return false;
  if(event->type() == QEvent::MouseButtonRelease)
  {
    QMouseEvent *me = static_cast<QMouseEvent *>(event);
    if(me->button() != Qt::LeftButton || !option.rect.contains(mousePos(me))) return false;
  }
  else if(event->type() == QEvent::MouseButtonDblClick || event->type() == QEvent::MouseButtonPress)
  {
    return option.rect.contains(mousePos(static_cast<QMouseEvent *>(event))); // el clic es del tilde
  }
  else if(event->type() == QEvent::KeyPress)
  {
    int key = static_cast<QKeyEvent *>(event)->key();
    if(key != Qt::Key_Space && key != Qt::Key_Select) return false;
  }
  else return false;
  // ciclo: sin marcar -> marcado (-> intermedio si es de tres estados) -> sin marcar
  Qt::CheckState state = (Qt::CheckState) index.data(Qt::CheckStateRole).toInt();
  bool tristate = index.flags() & Qt::ItemIsUserTristate;
  Qt::CheckState next = (state == Qt::Unchecked) ? Qt::Checked :
                        (state == Qt::Checked && tristate) ? Qt::PartiallyChecked : Qt::Unchecked;
  return model->setData(index, next, Qt::CheckStateRole);
}

DlgTags::DlgTags(int _protocol, const QString &_mkfile, const QString &text, QWidget *parent)
  : QDialog(parent)
{
  protocol = _protocol;
  mkfile = _mkfile;
  currentStruct = -1;
  tabHist = nullptr;
  tableHist = nullptr;
  histShown = false;
  form = new Ui_DlgTags;
  form->setupUi(this);
  // Nestor: cambiar de daemon (los del mismo protocolo en la carpeta del proyecto) sin cerrar el editor
  {
    QString ext = (protocol == rlTagList::SIEMENS) ? "*.mksiemens" : (protocol == rlTagList::OPCUA) ? "*.mkopcua" :
                  (protocol == rlTagList::MQTT) ? "*.mkmqtt" : "*.mkmodbus";
    QDir dir = QFileInfo(mkfile).absoluteDir();
    QHBoxLayout *top = new QHBoxLayout();
    top->addWidget(new QLabel(tr("Daemon:"), this));
    comboDaemon = new QComboBox(this);
    comboDaemon->setToolTip(tr("Editar los tags de otro daemon del proyecto (pregunta si se guardan los cambios de este)"));
    for(const QString &f : dir.entryList(QStringList() << ext, QDir::Files, QDir::Name))
      comboDaemon->addItem(QFileInfo(f).completeBaseName(), dir.absoluteFilePath(f));
    if(comboDaemon->findData(QFileInfo(mkfile).absoluteFilePath()) < 0) comboDaemon->addItem(QFileInfo(mkfile).completeBaseName(), QFileInfo(mkfile).absoluteFilePath());
    comboDaemon->setCurrentIndex(comboDaemon->findData(QFileInfo(mkfile).absoluteFilePath()));
    top->addWidget(comboDaemon);
    top->addStretch(1);
    if(QBoxLayout *bl = qobject_cast<QBoxLayout *>(layout())) bl->insertLayout(0, top);
    connect(comboDaemon, QOverload<int>::of(&QComboBox::activated), this, [this](int i) {
      QString f = comboDaemon->itemData(i).toString();
      auto back = [this]() { comboDaemon->setCurrentIndex(comboDaemon->findData(QFileInfo(mkfile).absoluteFilePath())); };
      if(f == QFileInfo(mkfile).absoluteFilePath()) return;
      int r = QMessageBox::question(this, "pvdevelop", tr("Guardar los cambios de %1 antes de abrir %2?").arg(QFileInfo(mkfile).completeBaseName(), QFileInfo(f).completeBaseName()),
                                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
      if(r == QMessageBox::Cancel) { back(); return; }
      switchTo = f;
      if(r == QMessageBox::No) { reject(); return; }
      slotOk();   // si hay errores y no se guarda, se queda aca
      if(QDialog::result() != QDialog::Accepted && isVisible()) { switchTo.clear(); back(); }
    });
  }
  // miembros: columna de historial y alarmas (texto clave=valor)
  form->tableMembers->insertColumn(M_HIST);
  form->tableMembers->setHorizontalHeaderItem(M_HIST, new QTableWidgetItem(tr("Historial / alarma")));
  form->tableMembers->horizontalHeaderItem(M_HIST)->setToolTip(tr(
    "Claves del historiador para este miembro (en todas las variables de esta estructura), por ejemplo:\n"
    "log=1 deadband=0.5 unit=C alarm=\">80\" hyst=2 alarm_text=\"Temperatura alta\" priority=1 ack=1\n"
    "event=1 registra los cambios de un bool. Ver la pestania Historial y alarmas."));
  form->tableMembers->setColumnHidden(M_HIST, true); // se edita en la pestania Historial y alarmas
  // columna "Historial" (tilde): solo lo marcado aparece en la pestania Historial y alarmas
  form->table->insertColumn(C_HIST);
  form->table->setHorizontalHeaderItem(C_HIST, new QTableWidgetItem(tr("Historial")));
  form->table->horizontalHeaderItem(C_HIST)->setToolTip(tr("Marcado: la variable aparece en la pestania Historial y alarmas "
                                                           "(tendencias, alarmas y eventos del historiador)"));
  form->table->horizontalHeader()->moveSection(C_HIST, C_TYPE + 1);
  form->tableMembers->insertColumn(M_HON);
  form->tableMembers->setHorizontalHeaderItem(M_HON, new QTableWidgetItem(tr("Historial")));
  form->tableMembers->horizontalHeaderItem(M_HON)->setToolTip(tr("Marcado: el miembro aparece en la pestania Historial y alarmas "
                                                                 "(vale para todas las variables de esta estructura)"));
  form->tableMembers->horizontalHeader()->moveSection(M_HON, M_TYPE + 1);
  // PV Suite no trae el historiador: las columnas quedan ocultas, pero las claves de historial y alarmas
  // de un .mk hecho con la version completa se conservan al guardar
  form->table->setColumnHidden(C_HIST, true);
  form->tableMembers->setColumnHidden(M_HON, true);
  if(protocol == rlTagList::MQTT)   // Nestor: opciones de cada tag MQTT (ver rltagdef.h)
  {
    const char *heads[] = { QT_TR_NOOP("Formato"), QT_TR_NOOP("Ruta JSON"), QT_TR_NOOP("Topico de comando"),
                            QT_TR_NOOP("QoS"), QT_TR_NOOP("Retain"), QT_TR_NOOP("Sin datos (s)") };
    const char *tips[] = {
      QT_TR_NOOP("Valor: el mensaje es el numero (\"25.3\", \"1\", \"true\", \"on\").\nJSON: el valor esta dentro de un objeto JSON (ver Ruta JSON)"),
      QT_TR_NOOP("Con formato JSON: ruta del valor, con puntos para objetos anidados (amb.temp). Vacio = value.\n"
                 "Varios tags pueden leer el mismo topico con rutas distintas"),
      QT_TR_NOOP("Topico donde se publican las escrituras (Escribir marcado). Vacio = <topico>/set"),
      QT_TR_NOOP("Calidad de servicio de la suscripcion y de las escrituras: 0, 1 o 2. Vacio = la del archivo (qos=)"),
      QT_TR_NOOP("Las escrituras se publican con retain (el broker las guarda para los que se conectan despues)"),
      QT_TR_NOOP("Segundos sin mensajes para marcar la calidad como mala (0 o vacio = nunca)") };
    for(int c=C_FORMAT; c<=C_STALE; c++)
    {
      form->table->insertColumn(c);
      form->table->setHorizontalHeaderItem(c, new QTableWidgetItem(tr(heads[c - C_FORMAT])));
      form->table->horizontalHeaderItem(c)->setToolTip(tr(tips[c - C_FORMAT]));
    }
    // la descripcion al final (se estira)
    QHeaderView *h = form->table->horizontalHeader();
    h->moveSection(h->visualIndex(C_DESC), form->table->columnCount() - 1);
  }
  buildHistTab();
  form->tabs->removeTab(form->tabs->indexOf(tabHist));   // sin historiador (ver arriba): la pestania no se muestra
  tabHist->setParent(this);
  tabHist->hide();
  // texto centrado en todas las columnas
  CenterDelegate *center = new CenterDelegate(this);
  form->table->setItemDelegate(center);
  form->tableMembers->setItemDelegate(center);
  tableHist->setItemDelegate(center);

  // mensajes del daemon en la consola (console= y console_interval=)
  QLabel *labelConsole = new QLabel(tr("Consola del daemon:"), form->groupSettings);
  QWidget *boxConsole = new QWidget(form->groupSettings);
  QHBoxLayout *layConsole = new QHBoxLayout(boxConsole);
  layConsole->setContentsMargins(0, 0, 0, 0);
  comboConsole = new QComboBox(boxConsole);
  comboConsole->addItem(tr("Silencioso (solo errores)"), rlTagList::CONSOLE_QUIET);
  comboConsole->addItem(tr("Normal (resumen)"), rlTagList::CONSOLE_NORMAL);
  comboConsole->addItem(tr("Detallado (cada bloque, con el numero de ciclo)"), rlTagList::CONSOLE_VERBOSE);
  comboConsole->setToolTip(tr(
    "Mensajes que el daemon escribe en su ventana:\n"
    "Silencioso: solo cuando un bloque deja de leerse (y cuando vuelve) y los errores de escritura.\n"
    "Normal: lo mismo y un resumen cada tantos segundos (ciclos, duracion, errores).\n"
    "Detallado: cada lectura con el numero de ciclo, el esclavo, la direccion, el resultado y lo que tardo.\n"
    "Al ejecutar el daemon se puede cambiar con -q, -n o -v."));
  spinConsoleInterval = new QSpinBox(boxConsole);
  spinConsoleInterval->setRange(1, 3600);
  spinConsoleInterval->setSuffix(tr(" s"));
  spinConsoleInterval->setToolTip(tr("Normal: segundos entre resumenes"));
  layConsole->addWidget(comboConsole, 1);
  layConsole->addWidget(spinConsoleInterval);
  QObject::connect(comboConsole, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
    spinConsoleInterval->setEnabled(comboConsole->currentData().toInt() == rlTagList::CONSOLE_NORMAL);
  });
  form->gridSettings->addWidget(labelConsole, 3, 0);
  form->gridSettings->addWidget(boxConsole, 3, 1, 1, 3);

  // ajustes segun el protocolo
  bool modbus = (protocol == rlTagList::MODBUS);
  bool opcua  = (protocol == rlTagList::OPCUA);
  QString unit;
  if(modbus)
  {
    setWindowTitle(tr("pvdevelop: tags Modbus - %1").arg(QFileInfo(mkfile).fileName()));
    form->comboNotation->addItem(tr("Modicon (40001, 30001, 00001, 10001)"), rlTagList::MODICON);
    form->comboNotation->addItem(tr("Offset base 0 (HR0, IR0, CO0, DI0)"),    rlTagList::OFFSET);
    form->comboNotation->addItem(tr("PLC Schneider (%MW0, %MD0, %MF0, %IW0, %M0, %I0)"), rlTagList::PLC);
    form->table->horizontalHeaderItem(C_SLAVE)->setText(tr("Esclavo"));
    unit = tr("registros");
  }
  else if(protocol == rlTagList::SIEMENS)
  {
    setWindowTitle(tr("pvdevelop: tags Siemens S7 - %1").arg(QFileInfo(mkfile).fileName()));
    form->comboNotation->addItem(tr("Alemana (E, A, M, DB10.DBX4.3)"), rlTagList::S7_GERMAN);
    form->comboNotation->addItem(tr("Inglesa (I, Q, M, DB10.DBX4.3)"), rlTagList::S7_ENGLISH);
    form->table->horizontalHeaderItem(C_SLAVE)->setText(tr("PLC (slave)"));
    unit = tr("bytes");
  }
  else if(protocol == rlTagList::MQTT)   // Nestor: fase 8
  {
    setWindowTitle(tr("pvdevelop: tags MQTT - %1").arg(QFileInfo(mkfile).fileName()));
    form->table->horizontalHeaderItem(C_ADDRESS)->setText(tr("Topico"));
    form->table->horizontalHeaderItem(C_ADDRESS)->setToolTip(tr("Topico MQTT del valor, sin + ni # (planta/sensor1/temp). En un arreglo, {i} se reemplaza por 0, 1, 2..."));
    form->labelNotation->hide();
    form->comboNotation->hide();
    form->table->setColumnHidden(C_SLAVE, true);
    form->table->setColumnHidden(C_STRIDE, true);
    form->table->setColumnHidden(C_SCAN, true);
    form->labelGap->hide();   form->spinGap->hide();
    form->labelBlock->hide(); form->spinBlock->hide();
    form->tableMembers->horizontalHeaderItem(M_AT)->setText(tr("Sufijo del topico"));
    form->tableMembers->setColumnHidden(M_COUNT, true);
  }
  else
  {
    setWindowTitle(tr("pvdevelop: tags OPC UA - %1").arg(QFileInfo(mkfile).fileName()));
    form->table->horizontalHeaderItem(C_ADDRESS)->setText(tr("NodeId (ns=2;s=...)"));
    form->labelNotation->hide();
    form->comboNotation->hide();
    form->table->setColumnHidden(C_SLAVE, true);
    form->table->setColumnHidden(C_STRIDE, true);
    form->labelGap->hide();   form->spinGap->hide();
    form->labelBlock->hide(); form->spinBlock->hide();
    form->table->horizontalHeaderItem(C_SCAN)->setText(tr("Muestreo (ms)"));
    form->tableMembers->horizontalHeaderItem(M_AT)->setText(tr("Sufijo del NodeId"));
    form->tableMembers->setColumnHidden(M_COUNT, true);
  }
  if(!modbus)
  {
    form->labelOrder->hide();    form->comboOrder->hide();
    form->labelBitWrite->hide(); form->comboBitWrite->hide();
    form->table->setColumnHidden(C_ORDER, true);
    form->tableMembers->setColumnHidden(M_ORDER, true);
  }
  form->table->horizontalHeaderItem(C_COUNT)->setToolTip(tr("Cantidad de elementos: mas de 1 es un arreglo (Nombre[0], Nombre[1], ...)"));
  form->table->horizontalHeaderItem(C_STRIDE)->setToolTip(tr("Distancia entre elementos del arreglo en %1 (bits en arreglos de bits); vacio = consecutivos").arg(unit));
  if(opcua)
  {
    form->labelStructHelp->setText(tr(
      "Cada miembro se lee del NodeId de la variable mas el sufijo (por defecto \".\" + nombre).\n"
      "Ejemplo: variable Zona con NodeId ns=2;s=Planta.Zona1 y miembro Temp -> ns=2;s=Planta.Zona1.Temp.\n"
      "Arreglos de variables: {i} en el NodeId (ns=2;s=Planta.Zona{i}) se reemplaza por 0, 1, 2, ...\n"
      "En el pvserver: OPC1.getReal(OPC1_Zona[2].Temp)"));
  }
  else if(protocol == rlTagList::MQTT)
  {
    form->labelStructHelp->setText(tr(
      "Cada miembro se lee del topico de la variable mas el sufijo (por defecto \"/\" + nombre).\n"
      "Ejemplo: variable Bomba1 con topico planta/bomba1 y miembro Marcha -> planta/bomba1/Marcha.\n"
      "Arreglos de variables: {i} en el topico (planta/zona{i}) se reemplaza por 0, 1, 2, ...\n"
      "En el pvserver: MQTT1.getReal(MQTT1_Zona[2].Temp)"));
  }
  else
  {
    form->labelStructHelp->setText(tr(
      "Posicion: lugar del miembro desde el comienzo de la estructura, en %1, con .bit para un bool "
      "(p. ej. 0.3). Vacia: a continuacion del anterior (los bool siguen en el bit siguiente).\n"
      "Una variable con Tipo = la estructura usa su Direccion como comienzo; con Cantidad > 1 es un arreglo de "
      "estructuras, una cada \"Paso\" %1 (vacio = consecutivas).\n"
      "En el pvserver: PLC1.getReal(PLC1_Bomba1.Velocidad), PLC1.setBool(PLC1_Bombas[2].Marcha, 1)").arg(unit));
  }

  // leer el texto del .mk: ajustes, estructuras, variables y el .csv
  rlTagList list(protocol);
  const QStringList lines = text.split('\n');
  int n = 0;
  for(const QString &l0 : lines)
  {
    n++;
    if(list.parseLine(l0.toUtf8().constData(), QString("linea %1").arg(n).toStdString()) == 0)
    {
      if(l0.trimmed() != "# --- tags (editados con pvdevelop) ---") otherLines << l0;
    }
  }
  while(!otherLines.isEmpty() && otherLines.last().trimmed().isEmpty()) otherLines.removeLast();
  // Nestor: estructuras compartidas del proyecto (<proyecto>.mkstruct): tambien si este .mk todavia no las usa
  if(list.structsFile.empty() && QFile::exists(sharedStructsPath())) list.structsFile = QFileInfo(sharedStructsPath()).fileName().toStdString();
  if(!list.structsFile.empty()) list.readSharedStructs(mkfile.toLocal8Bit().constData());
  for(const rlTagStruct &s : list.structs) if(s.shared) sharedLoaded << QString::fromStdString(s.name);
  if(!list.csvfile.empty())
  {
    form->radioCsv->setChecked(true);
    form->editCsv->setText(QString::fromStdString(list.csvfile));
    list.readCsv(csvPath().toLocal8Bit().constData());
  }
  else
  {
    form->editCsv->setText(QFileInfo(mkfile).completeBaseName() + "_tags.csv");
  }
  structs = list.structs;

  notation = list.notation;
  int ni = form->comboNotation->findData(notation);
  if(ni >= 0) form->comboNotation->setCurrentIndex(ni);
  form->comboOrder->setCurrentIndex(list.order & 3);
  form->comboBitWrite->setCurrentIndex(list.bit_write_fc22 ? 1 : 0);
  form->spinGap->setValue(list.max_gap < 0 ? -1 : list.max_gap);
  form->spinBlock->setValue(list.max_block < 0 ? 0 : list.max_block);
  int ci = comboConsole->findData(list.console);
  comboConsole->setCurrentIndex(ci < 0 ? 1 : ci);
  spinConsoleInterval->setValue(list.console_interval);
  spinConsoleInterval->setEnabled(list.console == rlTagList::CONSOLE_NORMAL);
  sysHist = list.sysHist;
  checkSys->blockSignals(true);
  for(size_t k=0; k<sysHist.size(); k++) if(sysHist[k].second.used()) checkSys->setChecked(true);
  checkSys->blockSignals(false);

  form->table->setRowCount(0);
  for(size_t i=0; i<list.tags.size(); i++) addRow(list.tags[i]);
  pvbTableResort(form->table);
  form->table->resizeColumnsToContents();
  // los combos de Tipo y Orden no cuentan para resizeColumnsToContents
  QComboBox *probe = typeCombo(this, true);
  fitComboColumn(form->table, C_TYPE, probe);
  delete probe;
  probe = orderCombo(this);
  fitComboColumn(form->table, C_ORDER, probe);
  delete probe;
  if(form->table->columnWidth(C_ADDRESS) < 170) form->table->setColumnWidth(C_ADDRESS, 170);
  if(form->table->columnWidth(C_NAME) < 140)    form->table->setColumnWidth(C_NAME, 140);
  form->table->horizontalHeader()->setStretchLastSection(true);

  for(size_t i=0; i<structs.size(); i++) form->listStructs->addItem(QString::fromStdString(structs[i].name));
  form->tableMembers->horizontalHeader()->setStretchLastSection(true);
  form->tableMembers->setColumnWidth(M_DESC, 180);
  pvbSortableTable(form->table);
  pvbSortableTable(form->tableMembers);
  probe = typeCombo(this, false);
  fitComboColumn(form->tableMembers, M_TYPE, probe);
  delete probe;
  probe = orderCombo(this);
  fitComboColumn(form->tableMembers, M_ORDER, probe);
  delete probe;
  form->tableMembers->setColumnWidth(M_NAME, 140);
  form->tableMembers->setColumnWidth(M_AT, textAddr() ? 180 : 80);
  if(protocol == rlTagList::MQTT)
  {
    form->table->setColumnWidth(C_FORMAT, 80);
    form->table->setColumnWidth(C_JSON, 100);
    if(form->table->columnWidth(C_CMD) < 170) form->table->setColumnWidth(C_CMD, 170);
    form->table->setColumnWidth(C_QOS, 50);
    form->table->setColumnWidth(C_RETAIN, 60);
    form->table->setColumnWidth(C_STALE, 100);
  }
  form->tableMembers->setColumnWidth(M_COUNT, 70);
  form->tableMembers->setColumnWidth(M_RW, 40);
  form->tableMembers->setColumnWidth(M_SCALE, 70);
  form->tableMembers->setColumnWidth(M_OFFSET, 70);
  if(!structs.empty()) form->listStructs->setCurrentRow(0); // (todavia sin conectar)
  showStruct(structs.empty() ? -1 : 0);

  if(!list.errors.empty())
  {
    QStringList e;
    for(size_t i=0; i<list.errors.size(); i++) e << QString::fromStdString(list.errors[i]);
    form->labelStatus->setText(e.join("\n"));
  }
  slotStoreChanged();

  connect(form->buttonAdd,       &QPushButton::clicked, this, &DlgTags::slotAdd);
  connect(form->buttonDuplicate, &QPushButton::clicked, this, &DlgTags::slotDuplicate);
  connect(form->buttonRemove,    &QPushButton::clicked, this, &DlgTags::slotRemove);
  connect(form->buttonImport,    &QPushButton::clicked, this, &DlgTags::slotImport);
  connect(form->buttonExport,    &QPushButton::clicked, this, &DlgTags::slotExport);
  connect(form->buttonCheck,     &QPushButton::clicked, this, &DlgTags::slotCheck);
  // Nestor: Modbus: una direccion de entrada discreta o input register no se puede escribir: desmarca RW
  connect(form->table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
    if(protocol != rlTagList::MODBUS || item->column() != C_ADDRESS) return;
    QTableWidgetItem *rw = form->table->item(item->row(), C_RW);
    if(rw == nullptr || rw->checkState() != Qt::Checked) return;
    rlTagList l(protocol);
    l.notation = currentNotation();
    rlTagDef t;
    std::string err;
    if(l.parseAddress(item->text().trimmed().toStdString(), t, err) == 0 && (t.area == RLTAG_DISCRETE || t.area == RLTAG_INPUT))
      rw->setCheckState(Qt::Unchecked);
  });
  connect(form->buttonCsv,       &QPushButton::clicked, this, &DlgTags::slotBrowseCsv);
  connect(form->radioMk,  &QRadioButton::toggled, this, &DlgTags::slotStoreChanged);
  connect(form->radioCsv, &QRadioButton::toggled, this, &DlgTags::slotStoreChanged);
  connect(form->comboNotation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DlgTags::slotNotation);
  connect(form->listStructs, &QListWidget::currentRowChanged, this, &DlgTags::slotStructSelected);
  connect(form->buttonStructNew,    &QPushButton::clicked, this, &DlgTags::slotStructNew);
  connect(form->buttonStructRename, &QPushButton::clicked, this, &DlgTags::slotStructRename);
  connect(form->buttonStructRemove, &QPushButton::clicked, this, &DlgTags::slotStructRemove);
  connect(form->buttonMemberAdd,    &QPushButton::clicked, this, &DlgTags::slotMemberAdd);
  connect(form->buttonMemberRemove, &QPushButton::clicked, this, &DlgTags::slotMemberRemove);
  connect(form->buttonMemberUp,     &QPushButton::clicked, this, &DlgTags::slotMemberUp);
  connect(form->buttonMemberDown,   &QPushButton::clicked, this, &DlgTags::slotMemberDown);
  connect(form->tabs, &QTabWidget::currentChanged, this, &DlgTags::slotTabChanged);
  connect(form->buttonBox, &QDialogButtonBox::accepted, this, &DlgTags::slotOk);
  connect(form->buttonBox, &QDialogButtonBox::rejected, this, &DlgTags::reject);
}

DlgTags::~DlgTags()
{
  delete form;
}

int DlgTags::currentNotation() const
{
  if(textAddr()) return rlTagList::MODICON;
  return form->comboNotation->currentData().toInt();
}

QString DlgTags::csvPath() const
{
  QString f = form->editCsv->text().trimmed();
  if(f.isEmpty() || QFileInfo(f).isAbsolute()) return f;
  return QFileInfo(mkfile).dir().filePath(f);
}

// Tipo: (auto) + tipos simples (+ estructuras). Data: numero de tipo, o -1 y el nombre en el texto
QComboBox *DlgTags::typeCombo(QWidget *parent, bool withStructs) const
{
  QComboBox *type = new QComboBox(parent);
  if(withStructs) type->addItem(tr("(auto)"), 0);
  for(int ty=RLTAG_BOOL; ty<=RLTAG_LREAL; ty++)
  {
    if(protocol == rlTagList::MODBUS && ty == RLTAG_BYTE) continue;
    type->addItem(rlTagList::typeName(ty), ty);
  }
  if(withStructs)
  {
    for(size_t i=0; i<structs.size(); i++) type->addItem(QString::fromStdString(structs[i].name), -1);
    type->setToolTip(tr("(auto): segun la direccion (bit -> bool, registro -> int, %MD -> dint, %MF -> real, MW -> int, MD -> dint).\n"
                        "Tambien puede ser una estructura (pestania Estructuras)."));
  }
  return type;
}

QComboBox *DlgTags::orderCombo(QWidget *parent) const
{
  QComboBox *order = new QComboBox(parent);
  order->addItem(tr("(general)"), -1);
  for(int o=RLTAG_ABCD; o<=RLTAG_DCBA; o++) order->addItem(rlTagList::orderName(o), o);
  return order;
}

void DlgTags::fitComboColumn(QTableWidget *table, int column, QComboBox *combo)
{
  int w = combo->sizeHint().width() + 8;
  if(table->columnWidth(column) < w) table->setColumnWidth(column, w);
}

void DlgTags::addRow(const rlTagDef &t)
{
  QTableWidget *tb = form->table;
  int row = tb->rowCount();
  tb->insertRow(row);
  QTableWidgetItem *nameItem = new QTableWidgetItem(QString::fromStdString(t.name));
  nameItem->setData(Qt::UserRole, histText(t.hist));
  tb->setItem(row, C_NAME, nameItem);
  tb->setItem(row, C_ADDRESS, new QTableWidgetItem(QString::fromStdString(t.address)));
  QComboBox *type = typeCombo(tb, true);
  int ti = t.structName.empty() ? type->findData(t.type) : type->findText(QString::fromStdString(t.structName));
  if(ti < 0 && !t.structName.empty())
  {
    type->addItem(QString::fromStdString(t.structName), -1); // estructura que no esta definida (error al verificar)
    ti = type->count() - 1;
  }
  type->setCurrentIndex(ti < 0 ? 0 : ti);
  tb->setCellWidget(row, C_TYPE, type);
  fitComboColumn(tb, C_TYPE, type);
  tb->setItem(row, C_COUNT,  new QTableWidgetItem((t.count > 1) ? QString::number(t.count) : QString()));
  tb->setItem(row, C_STRIDE, new QTableWidgetItem((t.stride > 0) ? QString::number(t.stride) : QString()));
  QTableWidgetItem *rw = new QTableWidgetItem();
  rw->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
  rw->setCheckState(t.rw ? Qt::Checked : Qt::Unchecked);
  tb->setItem(row, C_RW, rw);
  tb->setItem(row, C_SLAVE,  new QTableWidgetItem(QString::number(t.slave)));
  tb->setItem(row, C_SCAN,   new QTableWidgetItem(QString::number(t.scan)));
  tb->setItem(row, C_SCALE,  new QTableWidgetItem(QString::number(t.scale, 'g', 15)));
  tb->setItem(row, C_OFFSET, new QTableWidgetItem(QString::number(t.scale_offset, 'g', 15)));
  QComboBox *order = orderCombo(tb);
  int oi = order->findData(t.order);
  order->setCurrentIndex(oi < 0 ? 0 : oi);
  tb->setCellWidget(row, C_ORDER, order);
  fitComboColumn(tb, C_ORDER, order);
  tb->setItem(row, C_DESC, new QTableWidgetItem(QString::fromStdString(t.description)));
  QTableWidgetItem *hist = new QTableWidgetItem();
  hist->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
  hist->setCheckState(t.hist.used() ? Qt::Checked : Qt::Unchecked);
  tb->setItem(row, C_HIST, hist);
  if(protocol == rlTagList::MQTT)
  {
    QComboBox *format = new QComboBox(tb);
    format->addItem(tr("Valor"), "value");
    format->addItem("JSON", "json");
    format->setCurrentIndex(QString::fromStdString(t.opt("format", "value")).toLower() == "json" ? 1 : 0);
    tb->setCellWidget(row, C_FORMAT, format);
    tb->setItem(row, C_JSON, new QTableWidgetItem(QString::fromStdString(t.opt("json"))));
    tb->setItem(row, C_CMD,  new QTableWidgetItem(QString::fromStdString(t.opt("cmd_topic"))));
    tb->setItem(row, C_QOS,  new QTableWidgetItem(QString::fromStdString(t.opt("qos"))));
    QTableWidgetItem *retain = new QTableWidgetItem();
    retain->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    retain->setCheckState(t.opt("retain", "0") != "0" ? Qt::Checked : Qt::Unchecked);
    tb->setItem(row, C_RETAIN, retain);
    std::string stale = t.opt("stale_s");
    tb->setItem(row, C_STALE, new QTableWidgetItem(stale == "0" ? QString() : QString::fromStdString(stale)));
  }
}

rlTagDef DlgTags::rowTag(int row) const
{
  QTableWidget *tb = form->table;
  rlTagDef t;
  auto text = [tb, row](int c) { QTableWidgetItem *i = tb->item(row, c); return i ? i->text().trimmed() : QString(); };
  t.name        = text(C_NAME).toStdString();
  t.address     = text(C_ADDRESS).toStdString();
  t.description = text(C_DESC).toStdString();
  QComboBox *type = qobject_cast<QComboBox *>(tb->cellWidget(row, C_TYPE));
  if(type != nullptr)
  {
    int ty = type->currentData().toInt();
    if(ty < 0) t.structName = type->currentText().toStdString();
    else       t.type = ty;
  }
  bool ok;
  int count = text(C_COUNT).toInt(&ok);   if(ok && count > 0) t.count = count;
  int stride = text(C_STRIDE).toInt(&ok); if(ok && stride > 0) t.stride = stride;
  t.rw = (tb->item(row, C_RW) && tb->item(row, C_RW)->checkState() == Qt::Checked) ? 1 : 0;
  int slave = text(C_SLAVE).toInt(&ok);   if(ok) t.slave = slave; else if(protocol == rlTagList::SIEMENS) t.slave = 0;
  int scan = text(C_SCAN).toInt(&ok);     if(ok) t.scan = scan;
  double s = text(C_SCALE).toDouble(&ok); if(ok) t.scale = s;
  double o = text(C_OFFSET).toDouble(&ok);if(ok) t.scale_offset = o;
  QComboBox *order = qobject_cast<QComboBox *>(tb->cellWidget(row, C_ORDER));
  t.order = order ? order->currentData().toInt() : -1;
  if(tb->item(row, C_NAME)) t.hist = histFromText(tb->item(row, C_NAME)->data(Qt::UserRole).toString());
  if(tb->item(row, C_HIST) && tb->item(row, C_HIST)->checkState() != Qt::Checked) t.hist = rlTagHist(); // sin historial
  if(protocol == rlTagList::MQTT)   // solo lo que no es el valor por defecto
  {
    QComboBox *format = qobject_cast<QComboBox *>(tb->cellWidget(row, C_FORMAT));
    if(format && format->currentData().toString() == "json") t.setOpt("format", "json");
    if(!text(C_JSON).isEmpty()) t.setOpt("json", text(C_JSON).toStdString());
    if(!text(C_CMD).isEmpty())  t.setOpt("cmd_topic", text(C_CMD).toStdString());
    if(!text(C_QOS).isEmpty())  t.setOpt("qos", text(C_QOS).toStdString());
    if(tb->item(row, C_RETAIN) && tb->item(row, C_RETAIN)->checkState() == Qt::Checked) t.setOpt("retain", "1");
    if(!text(C_STALE).isEmpty() && text(C_STALE) != "0") t.setOpt("stale_s", text(C_STALE).toStdString());
  }
  return t;
}

void DlgTags::fillList(rlTagList &list)
{
  storeStruct();
  if(histShown) storeHist();
  list.notation = currentNotation();
  list.order = form->comboOrder->currentIndex();
  list.bit_write_fc22 = form->comboBitWrite->currentIndex();
  list.max_gap = form->spinGap->value();
  list.max_block = (form->spinBlock->value() > 0) ? form->spinBlock->value() : -1;
  list.console = comboConsole->currentData().toInt();
  list.console_interval = spinConsoleInterval->value();
  list.sysHist = sysHist;
  list.structs = structs;
  list.tags.clear();
  for(int r : pvbTableOrder(form->table)) list.tags.push_back(rowTag(r));   // el orden del archivo, no el que se ve
}

void DlgTags::slotAdd()
{
  rlTagDef t;
  if(protocol == rlTagList::SIEMENS) t.slave = 0;
  t.rw = 1;   // Nestor: los nuevos se pueden escribir (en Modbus, una entrada discreta o un input register lo desmarca)
  t.name = QString("Tag%1").arg(form->table->rowCount() + 1).toStdString();
  addRow(t);
  form->table->setCurrentCell(form->table->rowCount() - 1, C_NAME);
  form->table->editItem(form->table->item(form->table->rowCount() - 1, C_NAME));
}

// copia la fila con la direccion siguiente (registro, bit o byte siguiente) y un nombre nuevo
void DlgTags::slotDuplicate()
{
  int row = form->table->currentRow();
  if(row < 0) return;
  rlTagDef t = rowTag(row);
  rlTagList list(protocol);
  fillList(list);
  std::string err;
  rlTagDef p = t;
  if(!textAddr() && t.structName.empty() && t.count <= 1 && list.check(p, err) == 0)
  {
    if(p.type == RLTAG_BOOL && p.bit >= 0 && !(protocol == rlTagList::MODBUS && p.size == 1))
    {
      int maxbit = (protocol == rlTagList::MODBUS) ? 15 : 7;
      if(p.bit < maxbit) p.bit++; else { p.bit = 0; p.addr++; }
    }
    else
    {
      p.addr += (protocol == rlTagList::MODBUS) ? ((p.size > 2) ? p.size / 2 : 1) : p.size;
    }
    t.address = list.formatAddress(p, currentNotation());
  }
  // nombre: incrementa el numero final (Motor1 -> Motor2)
  QString name = QString::fromStdString(t.name);
  int i = name.size();
  while(i > 0 && name[i-1].isDigit()) i--;
  int n = (i < name.size()) ? name.mid(i).toInt() + 1 : 2;
  t.name = (name.left(i) + QString::number(n)).toStdString();
  addRow(t);
  form->table->setCurrentCell(form->table->rowCount() - 1, C_NAME);
}

void DlgTags::slotRemove()
{
  QList<int> rows;
  for(const QModelIndex &i : form->table->selectionModel()->selectedRows()) rows << i.row();
  if(rows.isEmpty() && form->table->currentRow() >= 0) rows << form->table->currentRow();
  std::sort(rows.begin(), rows.end());
  for(int k=rows.size()-1; k>=0; k--) form->table->removeRow(rows[k]);
}

void DlgTags::slotImport()
{
  QString f = QFileDialog::getOpenFileName(this, tr("Importar tags"), QFileInfo(mkfile).absolutePath(), "CSV (*.csv);;*.*");
  if(f.isEmpty()) return;
  rlTagList list(protocol);
  list.readCsv(QDir::toNativeSeparators(f).toLocal8Bit().constData());
  for(size_t i=0; i<list.tags.size(); i++) addRow(list.tags[i]);
  QString msg = tr("%1 variables importadas de %2").arg(list.tags.size()).arg(QFileInfo(f).fileName());
  for(size_t i=0; i<list.errors.size(); i++) msg += "\n" + QString::fromStdString(list.errors[i]);
  form->labelStatus->setText(msg);
}

void DlgTags::slotExport()
{
  QString f = QFileDialog::getSaveFileName(this, tr("Exportar tags"), QFileInfo(mkfile).absolutePath(), "CSV (*.csv)");
  if(f.isEmpty()) return;
  if(!f.endsWith(".csv", Qt::CaseInsensitive)) f += ".csv";
  rlTagList list(protocol);
  fillList(list);
  if(list.writeCsv(QDir::toNativeSeparators(f).toLocal8Bit().constData()) < 0)
    QMessageBox::warning(this, "pvdevelop", tr("No se pudo escribir %1").arg(f));
  else
    form->labelStatus->setText(tr("Variables exportadas a %1 (las estructuras quedan en el archivo del daemon)").arg(f));
}

// convierte las direcciones de la tabla a la notacion nueva
void DlgTags::slotNotation(int)
{
  int newNotation = currentNotation();
  if(textAddr() || newNotation == notation) return;
  rlTagList oldList(protocol);
  oldList.notation = notation;
  int converted = 0, failed = 0;
  for(int r=0; r<form->table->rowCount(); r++)
  {
    rlTagDef t = rowTag(r);
    std::string err;
    if(t.address.empty()) continue;
    if(!t.structName.empty()) t.type = 0; // la direccion de una estructura es solo el comienzo
    if(oldList.parseAddress(t.address, t, err) == 0)
    {
      form->table->item(r, C_ADDRESS)->setText(QString::fromStdString(oldList.formatAddress(t, newNotation)));
      converted++;
    }
    else failed++;
  }
  notation = newNotation;
  form->labelStatus->setText(tr("%1 direcciones convertidas%2").arg(converted)
                             .arg(failed ? tr(", %1 no se pudieron convertir (revisarlas)").arg(failed) : QString()));
}

void DlgTags::slotBrowseCsv()
{
  QString f = QFileDialog::getSaveFileName(this, tr("Archivo de tags"), csvPath(), "CSV (*.csv)",
                                           nullptr, QFileDialog::DontConfirmOverwrite);
  if(f.isEmpty()) return;
  QDir dir = QFileInfo(mkfile).absoluteDir();
  QString rel = dir.relativeFilePath(f);
  form->editCsv->setText(rel.startsWith("..") ? QDir::toNativeSeparators(f) : rel);
  form->radioCsv->setChecked(true);
}

void DlgTags::slotStoreChanged()
{
  form->editCsv->setEnabled(form->radioCsv->isChecked());
  form->buttonCsv->setEnabled(form->radioCsv->isChecked());
}

bool DlgTags::check(QString *report)
{
  rlTagList list(protocol);
  fillList(list);
  int size = list.layout(0);
  QStringList out;
  if(size < 0)
  {
    for(size_t i=0; i<list.errors.size(); i++) out << QString::fromStdString(list.errors[i]);
    *report = out.join("\n");
    return false;
  }
  if(textAddr())
  {
    out << tr("%1 variables (%2 valores simples); %3 bytes en la memoria compartida")
           .arg(list.tags.size()).arg(list.flat.size()).arg(size);
  }
  else
  {
    out << tr("%1 variables (%2 valores simples) en %3 bloques de lectura (%4 bytes en la memoria compartida):")
           .arg(list.tags.size()).arg(list.flat.size()).arg(list.blocks.size()).arg(size);
    for(size_t i=0; i<list.blocks.size() && i<20; i++)
    {
      const rlTagBlock &b = list.blocks[i];
      if(protocol == rlTagList::MODBUS)
      {
        const char *area[] = { "", "coils", "entradas discretas", "holding registers", "input registers" };
        out << tr("  esclavo %1, %2 %3..%4 (%5)%6").arg(b.slave).arg(area[b.area & 7]).arg(b.start)
               .arg(b.start + b.count - 1).arg(b.count).arg(b.scan > 0 ? tr(", cada %1 ms").arg(b.scan) : QString());
      }
      else
      {
        const char *org[] = { "", "DB", "M", "E", "A" };
        QString where = (b.area == 1) ? QString("DB%1").arg(b.dbnum) : QString(org[b.area & 7]);
        out << tr("  PLC %1, %2 bytes %3..%4 (%5)%6").arg(b.slave).arg(where).arg(b.start)
               .arg(b.start + b.count - 1).arg(b.count).arg(b.scan > 0 ? tr(", cada %1 ms").arg(b.scan) : QString());
      }
    }
    if(list.blocks.size() > 20) out << tr("  ... (%1 bloques mas)").arg(list.blocks.size() - 20);
  }
  *report = out.join("\n");
  return true;
}

void DlgTags::slotCheck()
{
  QString report;
  check(&report);
  form->labelStatus->setText(report);
}

void DlgTags::slotOk()
{
  QString report;
  if(!check(&report))
  {
    form->labelStatus->setText(report);
    form->tabs->setCurrentIndex(0);
    if(QMessageBox::question(this, "pvdevelop", tr("Hay errores en los tags:\n\n%1\n\nGuardar igual?").arg(report)) != QMessageBox::Yes)
      return;
  }
  rlTagList list(protocol);
  fillList(list);

  QStringList out = otherLines;
  out << "" << "# --- tags (editados con pvdevelop) ---";
  if(!textAddr()) out << QString("address_notation=%1").arg(rlTagList::notationName(list.notation));
  if(protocol == rlTagList::MODBUS)
  {
    out << QString("word_order=%1").arg(rlTagList::orderName(list.order));
    out << QString("bit_write=%1").arg(list.bit_write_fc22 ? "fc22" : "rmw");
  }
  if(!textAddr())
  {
    if(list.max_gap >= 0)   out << QString("max_gap=%1").arg(list.max_gap);
    if(list.max_block > 0)  out << QString("max_block=%1").arg(list.max_block);
  }
  out << QString("console=%1").arg(rlTagList::consoleName(list.console));
  if(list.console_interval != 10) out << QString("console_interval=%1").arg(list.console_interval);
  for(size_t i=0; i<list.sysHist.size(); i++) // historial y alarmas de los tags de sistema
  {
    if(list.sysHist[i].second.used()) out << QString::fromStdString(list.sysLine(list.sysHist[i].first, list.sysHist[i].second));
  }
  std::vector<std::string> sl = list.structLines(); // las estructuras propias en el .mk
  for(size_t i=0; i<sl.size(); i++) out << QString::fromStdString(sl[i]);
  // Nestor: las compartidas en <proyecto>.mkstruct (las de otros daemons que no se tocaron aca, quedan)
  bool anyShared = false;
  for(const rlTagStruct &s : list.structs) anyShared |= (s.shared != 0);
  if(anyShared || !sharedLoaded.isEmpty())
  {
    rlTagList file(protocol), merged(protocol);
    file.structsFile = QFileInfo(sharedStructsPath()).fileName().toStdString();
    if(QFile::exists(sharedStructsPath())) file.readSharedStructs(mkfile.toLocal8Bit().constData());
    for(const rlTagStruct &s : file.structs)   // de otros daemons: no se cargaron aca o se dejaron igual
      if(!sharedLoaded.contains(QString::fromStdString(s.name)) && list.findStruct(s.name) == NULL) merged.structs.push_back(s);
    for(const rlTagStruct &s : list.structs) if(s.shared) merged.structs.push_back(s);
    for(rlTagStruct &s : merged.structs) s.shared = 1;
    if(rlTagList::writeSharedStructs(sharedStructsPath().toLocal8Bit().constData(), merged.sharedStructLines()) < 0)
    {
      QMessageBox::warning(this, "pvdevelop", tr("No se pudo escribir %1").arg(sharedStructsPath()));
      return;
    }
    if(anyShared) out << "structs=" + QFileInfo(sharedStructsPath()).fileName();
  }
  if(form->radioCsv->isChecked())
  {
    QString csv = form->editCsv->text().trimmed();
    if(csv.isEmpty())
    {
      QMessageBox::warning(this, "pvdevelop", tr("Falta el nombre del archivo CSV"));
      return;
    }
    if(list.writeCsv(csvPath().toLocal8Bit().constData()) < 0)
    {
      QMessageBox::warning(this, "pvdevelop", tr("No se pudo escribir %1").arg(csvPath()));
      return;
    }
    out << "tags=" + QDir::fromNativeSeparators(csv);
  }
  else
  {
    for(size_t i=0; i<list.tags.size(); i++) out << QString::fromStdString(list.tagLine(list.tags[i]));
  }
  result = out.join("\n") + "\n";
  accept();
}

//------------------------------------------------------------------------------------------------
// estructuras

void DlgTags::refreshTypeCombos()
{
  for(int r=0; r<form->table->rowCount(); r++)
  {
    QComboBox *old = qobject_cast<QComboBox *>(form->table->cellWidget(r, C_TYPE));
    QString current = old ? old->currentText() : QString();
    int data = old ? old->currentData().toInt() : 0;
    QComboBox *type = typeCombo(form->table, true);
    int i = (data < 0) ? type->findText(current) : type->findData(data);
    type->setCurrentIndex(i < 0 ? 0 : i);
    form->table->setCellWidget(r, C_TYPE, type);
    fitComboColumn(form->table, C_TYPE, type);
  }
}

void DlgTags::addMemberRow(const rlTagMember &m)
{
  QTableWidget *tb = form->tableMembers;
  int row = tb->rowCount();
  tb->insertRow(row);
  tb->setItem(row, M_NAME, new QTableWidgetItem(QString::fromStdString(m.name)));
  tb->setItem(row, M_AT,   new QTableWidgetItem(QString::fromStdString(m.at)));
  QComboBox *type = typeCombo(tb, false);
  int ti = type->findData(m.type);
  type->setCurrentIndex(ti < 0 ? 0 : ti);
  tb->setCellWidget(row, M_TYPE, type);
  fitComboColumn(tb, M_TYPE, type);
  tb->setItem(row, M_COUNT, new QTableWidgetItem((m.count > 1) ? QString::number(m.count) : QString()));
  QTableWidgetItem *rw = new QTableWidgetItem();
  rw->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserTristate);
  rw->setCheckState(m.rw < 0 ? Qt::PartiallyChecked : (m.rw ? Qt::Checked : Qt::Unchecked));
  rw->setToolTip(tr("Marcado: se puede escribir; vacio: solo lectura; intermedio: como la variable"));
  tb->setItem(row, M_RW, rw);
  tb->setItem(row, M_SCALE,  new QTableWidgetItem(QString::number(m.scale, 'g', 15)));
  tb->setItem(row, M_OFFSET, new QTableWidgetItem(QString::number(m.scale_offset, 'g', 15)));
  QComboBox *order = orderCombo(tb);
  int oi = order->findData(m.order);
  order->setCurrentIndex(oi < 0 ? 0 : oi);
  tb->setCellWidget(row, M_ORDER, order);
  fitComboColumn(tb, M_ORDER, order);
  tb->setItem(row, M_DESC, new QTableWidgetItem(QString::fromStdString(m.description)));
  tb->setItem(row, M_HIST, new QTableWidgetItem(histText(m.hist)));
  QTableWidgetItem *hist = new QTableWidgetItem();
  hist->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
  bool on = m.hist.used() || (currentStruct >= 0 && currentStruct < (int) structs.size() &&
                              memberHistOn.count(structs[currentStruct].name + "." + m.name));
  hist->setCheckState(on ? Qt::Checked : Qt::Unchecked);
  tb->setItem(row, M_HON, hist);
}

rlTagMember DlgTags::memberRow(int row) const
{
  QTableWidget *tb = form->tableMembers;
  rlTagMember m;
  auto text = [tb, row](int c) { QTableWidgetItem *i = tb->item(row, c); return i ? i->text().trimmed() : QString(); };
  m.name = text(M_NAME).toStdString();
  m.at   = text(M_AT).toStdString();
  m.description = text(M_DESC).toStdString();
  QComboBox *type = qobject_cast<QComboBox *>(tb->cellWidget(row, M_TYPE));
  m.type = type ? type->currentData().toInt() : 0;
  bool ok;
  int count = text(M_COUNT).toInt(&ok);   if(ok && count > 0) m.count = count;
  QTableWidgetItem *rw = tb->item(row, M_RW);
  m.rw = (rw == nullptr || rw->checkState() == Qt::PartiallyChecked) ? -1 : (rw->checkState() == Qt::Checked ? 1 : 0);
  double s = text(M_SCALE).toDouble(&ok);  if(ok) m.scale = s;
  double o = text(M_OFFSET).toDouble(&ok); if(ok) m.scale_offset = o;
  QComboBox *order = qobject_cast<QComboBox *>(tb->cellWidget(row, M_ORDER));
  m.order = order ? order->currentData().toInt() : -1;
  m.hist = histFromText(text(M_HIST));
  if(tb->item(row, M_HON) && tb->item(row, M_HON)->checkState() != Qt::Checked) m.hist = rlTagHist(); // sin historial
  return m;
}

void DlgTags::storeStruct()
{
  if(currentStruct < 0 || currentStruct >= (int) structs.size()) return;
  rlTagStruct &s = structs[currentStruct];
  s.description = form->editStructDesc->text().trimmed().toStdString();
  s.shared = form->checkStructShared->isChecked() ? 1 : 0;   // Nestor: en el .mkstruct del proyecto
  s.members.clear();
  for(int r : pvbTableOrder(form->tableMembers))   // el orden del archivo, no el que se ve
  {
    s.members.push_back(memberRow(r));
    // miembros con "Historial" marcado (aunque todavia no tengan claves): van a la pestania Historial
    QTableWidgetItem *on = form->tableMembers->item(r, M_HON);
    std::string key = s.name + "." + s.members.back().name;
    if(on && on->checkState() == Qt::Checked) memberHistOn.insert(key);
    else                                      memberHistOn.erase(key);
  }
}

void DlgTags::showStruct(int index)
{
  currentStruct = index;
  form->tableMembers->setRowCount(0);
  bool ok = (index >= 0 && index < (int) structs.size());
  form->editStructDesc->setEnabled(ok);
  form->tableMembers->setEnabled(ok);
  form->buttonMemberAdd->setEnabled(ok);
  form->buttonMemberRemove->setEnabled(ok);
  form->buttonMemberUp->setEnabled(ok);
  form->buttonMemberDown->setEnabled(ok);
  form->buttonStructRename->setEnabled(ok);
  form->buttonStructRemove->setEnabled(ok);
  form->checkStructShared->setEnabled(ok);
  if(!ok) { form->editStructDesc->clear(); form->checkStructShared->setChecked(false); return; }
  form->checkStructShared->setChecked(structs[index].shared != 0);
  form->editStructDesc->setText(QString::fromStdString(structs[index].description));
  for(size_t i=0; i<structs[index].members.size(); i++) addMemberRow(structs[index].members[i]);
  pvbTableResort(form->tableMembers);
}

void DlgTags::slotStructSelected(int row)
{
  storeStruct();
  showStruct(row);
}

void DlgTags::slotStructNew()
{
  bool ok;
  QString name = QInputDialog::getText(this, tr("Nueva estructura"), tr("Nombre (letras, numeros y _):"),
                                       QLineEdit::Normal, QString("Struct%1").arg(structs.size() + 1), &ok).trimmed();
  if(!ok || name.isEmpty()) return;
  if(!rlTagList::validName(name.toStdString()) || rlTagList::typeFromString(name.toStdString()) != 0 || rlTagList::reservedName(name.toStdString()))
  { QMessageBox::warning(this, "pvdevelop", tr("Nombre invalido: %1").arg(name)); return; }
  for(size_t i=0; i<structs.size(); i++)
    if(structs[i].name == name.toStdString()) { QMessageBox::warning(this, "pvdevelop", tr("Ya existe %1").arg(name)); return; }
  storeStruct();
  rlTagStruct s;
  s.name = name.toStdString();
  s.units = 0;
  rlTagMember m;
  m.name = "Valor";
  m.type = RLTAG_INT;
  s.members.push_back(m);
  structs.push_back(s);
  form->listStructs->addItem(name);
  refreshTypeCombos();
  form->listStructs->setCurrentRow(form->listStructs->count() - 1);
}

void DlgTags::slotStructRename()
{
  int i = form->listStructs->currentRow();
  if(i < 0) return;
  QString oldName = QString::fromStdString(structs[i].name);
  bool ok;
  QString name = QInputDialog::getText(this, tr("Renombrar estructura"), tr("Nombre:"), QLineEdit::Normal, oldName, &ok).trimmed();
  if(!ok || name.isEmpty() || name == oldName) return;
  if(!rlTagList::validName(name.toStdString()) || rlTagList::typeFromString(name.toStdString()) != 0 || rlTagList::reservedName(name.toStdString()))
  { QMessageBox::warning(this, "pvdevelop", tr("Nombre invalido: %1").arg(name)); return; }
  structs[i].name = name.toStdString();
  form->listStructs->item(i)->setText(name);
  // las variables de ese tipo pasan al nombre nuevo
  for(int r=0; r<form->table->rowCount(); r++)
  {
    QComboBox *type = qobject_cast<QComboBox *>(form->table->cellWidget(r, C_TYPE));
    if(type && type->currentData().toInt() < 0 && type->currentText() == oldName) type->setItemText(type->currentIndex(), name);
  }
  refreshTypeCombos();
}

void DlgTags::slotStructRemove()
{
  int i = form->listStructs->currentRow();
  if(i < 0) return;
  QString name = QString::fromStdString(structs[i].name);
  if(QMessageBox::question(this, "pvdevelop", tr("Eliminar la estructura %1?\nLas variables de ese tipo quedan con error hasta cambiarles el tipo.").arg(name)) != QMessageBox::Yes)
    return;
  currentStruct = -1;
  structs.erase(structs.begin() + i);
  delete form->listStructs->takeItem(i);
  refreshTypeCombos();
  showStruct(form->listStructs->currentRow());
}

void DlgTags::slotMemberAdd()
{
  if(currentStruct < 0) return;
  rlTagMember m;
  m.name = QString("Miembro%1").arg(form->tableMembers->rowCount() + 1).toStdString();
  m.type = RLTAG_INT;
  addMemberRow(m);
  form->tableMembers->setCurrentCell(form->tableMembers->rowCount() - 1, M_NAME);
  form->tableMembers->editItem(form->tableMembers->item(form->tableMembers->rowCount() - 1, M_NAME));
}

void DlgTags::slotMemberRemove()
{
  int r = form->tableMembers->currentRow();
  if(r >= 0) form->tableMembers->removeRow(r);
}

void DlgTags::swapMembers(int a, int b)
{
  if(a < 0 || b < 0 || a >= form->tableMembers->rowCount() || b >= form->tableMembers->rowCount()) return;
  pvbTableAcceptOrder(form->tableMembers);   // ordenada con un clic: lo que se ve pasa a ser el orden
  storeStruct();
  std::swap(structs[currentStruct].members[a], structs[currentStruct].members[b]);
  showStruct(currentStruct);
  form->tableMembers->setCurrentCell(b, M_NAME);
}

void DlgTags::slotMemberUp()
{
  int r = form->tableMembers->currentRow();
  swapMembers(r, r - 1);
}

void DlgTags::slotMemberDown()
{
  int r = form->tableMembers->currentRow();
  swapMembers(r, r + 1);
}

//------------------------------------------------------------------------------------------------
// historial y alarmas (los usa el historiador: Daemon > Historiador y alarmas)

rlTagHist DlgTags::histFromText(const QString &pairs)
{
  rlTagHist h;
  std::string err;
  std::vector< std::pair<std::string, std::string> > p = rlTagList::splitPairs(pairs.toUtf8().constData(), err);
  for(size_t i=0; i<p.size(); i++) h.set(p[i].first, p[i].second);
  return h;
}

QString DlgTags::histText(const rlTagHist &h)
{
  return QString::fromUtf8(h.pairs().c_str()).trimmed();
}

void DlgTags::buildHistTab()
{
  tabHist = new QWidget();
  QVBoxLayout *lay = new QVBoxLayout(tabHist);
  QLabel *help = new QLabel(tr(
    "Lo que guarda el historiador (rlhistorian) de cada variable: su valor para las tendencias, sus alarmas y eventos.\n"
    "Registrar: guarda el valor cuando cambia mas que la banda muerta (0 = cualquier cambio), y cada \"Max. s\" aunque no cambie.\n"
    "Alarma: >80  >=80  <10  <=10  =1  !=0  1 (bool en 1)  0 (bool en 0)  in:10..20  out:10..20.  "
    "Reconocer: la alarma queda en la lista hasta que un usuario la reconoce.\n"
    "Evento: registra cada encendido y apagado de un bool.\n"
    "Aparecen las variables con la columna Historial marcada (pestania Variables) y los miembros de estructura con "
    "Historial marcado (pestania Estructuras): las alarmas de una estructura van en cada miembro."), tabHist);
  help->setWordWrap(true);
  lay->addWidget(help);
  checkSys = new QCheckBox(tr("Mostrar los tags de sistema (estado de la comunicacion: SYS.Comunicacion, SYS.Ciclo...)"), tabHist);
  QObject::connect(checkSys, &QCheckBox::toggled, this, [this](bool) { storeHist(); showHist(); });
  lay->addWidget(checkSys);
  labelHistEmpty = new QLabel(tr("No hay variables con historial: marcar la columna Historial en las variables o en los miembros de las estructuras."), tabHist);
  labelHistEmpty->setStyleSheet("color: gray");
  lay->addWidget(labelHistEmpty);
  tableHist = new QTableWidget(0, H_COUNT, tabHist);
  QStringList head;
  head << tr("Variable") << tr("Registrar") << tr("Banda muerta") << tr("Max. s") << tr("Unidad") << tr("Alarma")
       << tr("Histeresis") << tr("Texto de la alarma") << tr("Grupo") << tr("Prioridad") << tr("Reconocer") << tr("Evento");
  tableHist->setHorizontalHeaderLabels(head);
  tableHist->horizontalHeaderItem(H_LOGMAX)->setToolTip(tr("Segundos: registra aunque no cambie (vacio = el del historiador, 600; 0 = nunca)"));
  tableHist->horizontalHeaderItem(H_ALARM)->setToolTip(tr(">80  >=80  <10  <=10  =1  !=0  1 (bool en 1)  0 (bool en 0)  in:10..20  out:10..20"));
  tableHist->horizontalHeaderItem(H_HYST)->setToolTip(tr("Histeresis: cuanto tiene que volver el valor para que la alarma termine (evita alarmas que van y vienen)"));
  tableHist->horizontalHeaderItem(H_TEXT)->setToolTip(tr("Vacio: la descripcion de la variable (o su nombre)"));
  tableHist->horizontalHeaderItem(H_PRIORITY)->setToolTip(tr("1 = alta ... 4 = baja; vacio = sin prioridad"));
  tableHist->horizontalHeader()->setStretchLastSection(true);
  pvbSortableTable(tableHist);
  lay->addWidget(tableHist);
  form->tabs->addTab(tabHist, tr("Historial y alarmas"));
}

static QTableWidgetItem *checkItem(int value, bool tristate = false)
{
  QTableWidgetItem *i = new QTableWidgetItem();
  i->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable | (tristate ? Qt::ItemIsUserTristate : Qt::NoItemFlags));
  i->setCheckState(value < 0 && tristate ? Qt::PartiallyChecked : (value > 0 ? Qt::Checked : Qt::Unchecked));
  return i;
}

// los tags de sistema que se muestran en la pestania Historial: los fijos y los de las lineas "sys"
std::vector<std::string> DlgTags::sysRows() const
{
  std::vector<std::string> names = rlTagList::sysNames();
  for(size_t i=0; i<sysHist.size(); i++)
  {
    if(std::find(names.begin(), names.end(), sysHist[i].first) == names.end()) names.push_back(sysHist[i].first);
  }
  return names;
}

// filas de la pestania Historial: solo las variables y los miembros de estructura con la columna
// "Historial" marcada, y los tags de sistema si esta marcado "Mostrar tags de sistema"
void DlgTags::showHist()
{
  QTableWidget *tv = form->table, *th = tableHist;
  th->setRowCount(0);
  histRows.clear();
  auto num = [](double d) { return std::isnan(d) ? QString() : QString::number(d, 'g', 15); };
  for(int r=0; r<tv->rowCount(); r++)
  {
    QTableWidgetItem *on = tv->item(r, C_HIST);
    if(on && on->checkState() == Qt::Checked) histRows.push_back(HistRow{ HistRow::TAG, r, -1, std::string() });
  }
  for(int s=0; s<(int) structs.size(); s++)
  {
    for(int m=0; m<(int) structs[s].members.size(); m++)
    {
      if(structs[s].members[m].hist.used() || memberHistOn.count(structs[s].name + "." + structs[s].members[m].name))
        histRows.push_back(HistRow{ HistRow::MEMBER, s, m, std::string() });
    }
  }
  if(checkSys->isChecked())
  {
    std::vector<std::string> sysNames = sysRows();
    int nsys = (textAddr()) ? (int) rlTagList::sysNames().size() : (int) sysNames.size();
    for(int k=0; k<nsys; k++) histRows.push_back(HistRow{ HistRow::SYS, -1, -1, sysNames[k] });
  }
  labelHistEmpty->setVisible(histRows.empty());

  for(int r=0; r<(int) histRows.size(); r++)
  {
    const HistRow &hr = histRows[r];
    rlTagHist h;
    QString label;
    bool isStruct = false;
    if(hr.kind == HistRow::TAG)
    {
      rlTagDef t = rowTag(hr.a);
      h = t.hist;
      isStruct = !t.structName.empty();
      label = QString::fromStdString(t.name) + (isStruct ? QString(" (%1)").arg(QString::fromStdString(t.structName)) : QString());
    }
    else if(hr.kind == HistRow::MEMBER)
    {
      h = structs[hr.a].members[hr.b].hist;
      label = QString::fromStdString(structs[hr.a].name + "." + structs[hr.a].members[hr.b].name);
    }
    else
    {
      for(size_t k=0; k<sysHist.size(); k++) if(sysHist[k].first == hr.sys) h = sysHist[k].second;
      label = QString::fromStdString("SYS." + hr.sys);
    }
    th->insertRow(r);
    QTableWidgetItem *name = new QTableWidgetItem(label);
    name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    if(hr.kind == HistRow::SYS)
    {
      name->setForeground(QBrush(Qt::darkBlue));
      std::string desc = rlTagList::sysDescription(hr.sys);
      name->setToolTip(tr("Tag de sistema (lo actualiza el daemon): %1").arg(QString::fromStdString(desc)) +
                       ((hr.sys.rfind("Esclavo", 0) == 0 || hr.sys.rfind("Plc", 0) == 0) ? QString() :
                        tr("\nPara un esclavo o PLC en particular: linea \"sys name=Esclavo1 ...\" (o Plc0) en el archivo")));
    }
    else if(hr.kind == HistRow::MEMBER)
    {
      name->setForeground(QBrush(Qt::darkGreen));
      name->setToolTip(tr("Miembro de la estructura %1: vale para todas las variables de ese tipo").arg(QString::fromStdString(structs[hr.a].name)));
    }
    th->setItem(r, H_NAME, name);
    th->setItem(r, H_LOG, checkItem(h.log, isStruct));
    th->setItem(r, H_DEADBAND, new QTableWidgetItem(num(h.deadband)));
    th->setItem(r, H_LOGMAX, new QTableWidgetItem(h.log_max >= 0 ? QString::number(h.log_max) : QString()));
    th->setItem(r, H_UNIT, new QTableWidgetItem(QString::fromStdString(h.unit)));
    th->setItem(r, H_ALARM, new QTableWidgetItem(QString::fromStdString(h.alarm)));
    th->setItem(r, H_HYST, new QTableWidgetItem(num(h.hyst)));
    th->setItem(r, H_TEXT, new QTableWidgetItem(QString::fromStdString(h.text)));
    th->setItem(r, H_GROUP, new QTableWidgetItem(QString::fromStdString(h.group)));
    QComboBox *prio = new QComboBox(th);
    prio->addItem("", -1);
    prio->addItem(tr("1 (alta)"), 1); prio->addItem("2", 2); prio->addItem("3", 3); prio->addItem(tr("4 (baja)"), 4);
    int pi = prio->findData(h.priority > 0 ? h.priority : -1);
    prio->setCurrentIndex(pi < 0 ? 0 : pi);
    th->setCellWidget(r, H_PRIORITY, prio);
    th->setItem(r, H_ACK, checkItem(h.ack, isStruct));
    th->setItem(r, H_EVENT, checkItem(h.event, isStruct));
    if(isStruct)
    {
      // variable de tipo estructura: valores por defecto de los miembros (las alarmas van en cada miembro)
      for(int c : { H_ALARM, H_HYST, H_TEXT })
      {
        th->item(r, c)->setFlags(Qt::ItemIsSelectable);
        th->item(r, c)->setToolTip(tr("En una estructura la alarma se define en cada miembro (marcar Historial en la pestania Estructuras)"));
      }
      name->setToolTip(tr("Registrar, banda muerta, grupo, prioridad, reconocer y evento: valores por defecto de los miembros "
                          "(intermedio = lo que diga cada miembro)"));
    }
  }
  th->resizeColumnsToContents();
  if(th->columnWidth(H_NAME) < 140) th->setColumnWidth(H_NAME, 140);
  if(th->columnWidth(H_ALARM) < 90) th->setColumnWidth(H_ALARM, 90);
  if(th->columnWidth(H_TEXT) < 180) th->setColumnWidth(H_TEXT, 180);
  pvbTableResort(th);
  histShown = true;
}

void DlgTags::storeHist()
{
  QTableWidget *tv = form->table, *th = tableHist;
  if(!histShown || th == nullptr) return;
  const QList<int> order = pvbTableOrder(th), members = pvbTableOrder(form->tableMembers);   // filas en el orden de histRows
  for(int k=0; k<order.size() && k<(int) histRows.size(); k++)
  {
    const int r = order[k];
    const HistRow &hr = histRows[k];
    rlTagHist h;
    auto text = [th, r](int c) { QTableWidgetItem *i = th->item(r, c); return i ? i->text().trimmed() : QString(); };
    auto check = [th, r](int c) {
      QTableWidgetItem *i = th->item(r, c);
      if(i == nullptr || i->checkState() == Qt::PartiallyChecked) return -1;
      return (i->checkState() == Qt::Checked) ? 1 : -1;  // sin marcar = sin definir (no se escribe)
    };
    h.log = check(H_LOG);
    bool ok;
    double d = text(H_DEADBAND).toDouble(&ok); if(ok) h.deadband = d;
    int m = text(H_LOGMAX).toInt(&ok);         if(ok) h.log_max = m;
    h.unit = text(H_UNIT).toStdString();
    if(th->item(r, H_ALARM) && (th->item(r, H_ALARM)->flags() & Qt::ItemIsEnabled))
    {
      h.alarm = text(H_ALARM).toStdString();
      d = text(H_HYST).toDouble(&ok); if(ok) h.hyst = d;
      h.text = text(H_TEXT).toStdString();
    }
    h.group = text(H_GROUP).toStdString();
    QComboBox *prio = qobject_cast<QComboBox *>(th->cellWidget(r, H_PRIORITY));
    if(prio && prio->currentData().toInt() > 0) h.priority = prio->currentData().toInt();
    h.ack = check(H_ACK);
    h.event = check(H_EVENT);
    if(hr.kind == HistRow::TAG)
    {
      QTableWidgetItem *nameItem = tv->item(hr.a, C_NAME);
      if(nameItem) nameItem->setData(Qt::UserRole, histText(h));
    }
    else if(hr.kind == HistRow::MEMBER)
    {
      if(hr.a < (int) structs.size() && hr.b < (int) structs[hr.a].members.size())
      {
        structs[hr.a].members[hr.b].hist = h;
        // la estructura que muestra la pestania Estructuras: tambien en su tabla de miembros
        int mr = (hr.b < members.size()) ? members[hr.b] : -1;   // la fila del miembro (puede estar ordenada)
        if(hr.a == currentStruct && mr >= 0 && form->tableMembers->item(mr, M_HIST))
          form->tableMembers->item(mr, M_HIST)->setText(histText(h));
      }
    }
    else
    {
      size_t k = 0;
      while(k < sysHist.size() && sysHist[k].first != hr.sys) k++;
      if(k < sysHist.size()) sysHist[k].second = h;
      else if(h.used()) sysHist.push_back(std::make_pair(hr.sys, h));
    }
  }
}

void DlgTags::slotTabChanged(int index)
{
  if(histShown) storeHist();
  histShown = false;
  if(form->tabs->widget(index) == tabHist)
  {
    storeStruct();
    showHist();
  }
}

// Nestor: estructuras compartidas por los daemons del proyecto
QString DlgTags::sharedStructsPath() const
{
  QString name = pvbProjectName();
  if(name.isEmpty()) name = "proyecto";
  return QFileInfo(mkfile).absoluteDir().absoluteFilePath(name + ".mkstruct");
}
