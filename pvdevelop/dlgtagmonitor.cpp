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
** Nestor: monitor de tags para depurar (ver dlgtagmonitor.h)
****************************************************************************/
#include "../pvbrowser/pvdefine.h"   // PVUNIX
#include "dlgtagmonitor.h"
#include "buildconsole.h"   // pvbStartInConsole
#include "../rllib/lib/rltagdef.h"
#include "../rllib/lib/rlsharedmemory.h"
#include "../rllib/lib/rlmodbusclient.h"
#include "../rllib/lib/rlsiemenstcpclient.h"
#include <QComboBox>
#include <QLineEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QTimer>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QMessageBox>
#include <QDateTime>
#include <cmath>
#include <string.h>

enum { T_NAME, T_ADDR, T_VALUE, T_QUALITY, T_FORCED, T_TYPE, T_DESC, T_COUNT };

// ordena numeros como numeros (la columna Valor)
class NumItem : public QTableWidgetItem
{
public:
  bool operator<(const QTableWidgetItem &o) const override
  {
    bool a, b;
    double x = text().toDouble(&a), y = o.text().toDouble(&b);
    if(a && b) return x < y;
    return QTableWidgetItem::operator<(o);
  }
};

DlgTagMonitor::DlgTagMonitor(QWidget *parent) : QDialog(parent), shm(nullptr), shmSize(0)
{
  setWindowTitle(tr("pvdevelop: monitor de tags"));
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
  QVBoxLayout *lay = new QVBoxLayout(this);

  QHBoxLayout *top = new QHBoxLayout();
  top->addWidget(new QLabel(tr("Daemon:"), this));
  comboSource = new QComboBox(this);
  comboSource->setToolTip(tr("Los .tagmap del proyecto (los genera pvdevelop al compilar cada daemon)"));
  top->addWidget(comboSource, 1);
  top->addWidget(new QLabel(tr("Filtro:"), this));
  editFilter = new QLineEdit(this);
  editFilter->setPlaceholderText(tr("parte del nombre o de la direccion (Motor1, Temp, %MW10, SYS...)"));
  editFilter->setClearButtonEnabled(true);
  top->addWidget(editFilter, 1);
  checkPause = new QCheckBox(tr("Pausa"), this);
  top->addWidget(checkPause);
  lay->addLayout(top);

  table = new QTableWidget(0, T_COUNT, this);
  table->setHorizontalHeaderLabels(QStringList() << tr("Tag") << tr("Direccion") << tr("Valor") << tr("Calidad") << tr("Forzado") << tr("Tipo") << tr("Descripcion"));
  table->horizontalHeader()->setStretchLastSection(true);
  table->horizontalHeader()->setToolTip(tr("Clic en el titulo de una columna: ordenar"));
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);
  table->verticalHeader()->setVisible(false);
  table->setSortingEnabled(true);
  table->sortByColumn(T_NAME, Qt::AscendingOrder);
  lay->addWidget(table, 1);

  // escribir y forzar
  QHBoxLayout *wr = new QHBoxLayout();
  wr->addWidget(new QLabel(tr("Valor:"), this));
  editValue = new QLineEdit(this);
  editValue->setPlaceholderText(tr("elegir un tag de la lista"));
  editValue->setMaximumWidth(160);
  wr->addWidget(editValue);
  buttonWrite = new QPushButton(tr("Escribir"), this);
  buttonWrite->setToolTip(tr("Escribe el valor una vez (el PLC lo puede cambiar despues)"));
  buttonForce = new QPushButton(tr("Forzar"), this);
  buttonForce->setToolTip(tr("Vuelve a escribir el valor cada vez que cambia, hasta Liberar o cerrar el monitor"));
  buttonRelease = new QPushButton(tr("Liberar"), this);
  buttonReleaseAll = new QPushButton(tr("Liberar todos"), this);
  wr->addWidget(buttonWrite);
  wr->addWidget(buttonForce);
  wr->addWidget(buttonRelease);
  wr->addWidget(buttonReleaseAll);
  wr->addStretch(1);
  lay->addLayout(wr);

  QHBoxLayout *bottom = new QHBoxLayout();
  labelStatus = new QLabel(this);
  labelStatus->setWordWrap(true);
  bottom->addWidget(labelStatus, 1);
  QPushButton *start = new QPushButton(tr("Iniciar daemon"), this);
  start->setToolTip(tr("Ejecuta el daemon de este .tagmap en su propia ventana"));
  QPushButton *startSim = new QPushButton(tr("Iniciar en simulacion"), this);
  startSim->setToolTip(tr("Ejecuta el daemon con -sim: se conecta a simulation_tcpadr= (mismos tags y memoria compartida)"));
  QPushButton *close = new QPushButton(tr("Cerrar"), this);
  bottom->addWidget(start);
  bottom->addWidget(startSim);
  bottom->addWidget(close);
  lay->addLayout(bottom);
  resize(900, 600);

  connect(comboSource, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DlgTagMonitor::slotSource);
  connect(editFilter, &QLineEdit::textChanged, this, &DlgTagMonitor::slotFilter);
  connect(start, &QPushButton::clicked, this, [this]() { slotStart(false); });
  connect(startSim, &QPushButton::clicked, this, [this]() { slotStart(true); });
  connect(close, &QPushButton::clicked, this, &DlgTagMonitor::close);
  connect(buttonWrite, &QPushButton::clicked, this, [this]() { slotWrite(false); });
  connect(buttonForce, &QPushButton::clicked, this, [this]() { slotWrite(true); });
  connect(editValue, &QLineEdit::returnPressed, this, [this]() { slotWrite(false); });
  connect(buttonRelease, &QPushButton::clicked, this, [this]() { slotRelease(false); });
  connect(buttonReleaseAll, &QPushButton::clicked, this, [this]() { slotRelease(true); });
  connect(table, &QTableWidget::itemSelectionChanged, this, &DlgTagMonitor::slotSelection);
  timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &DlgTagMonitor::slotRefresh);

  const QStringList maps = QDir::current().entryList(QStringList() << "*.tagmap", QDir::Files, QDir::Name | QDir::IgnoreCase);
  for(const QString &m : maps) comboSource->addItem(QFileInfo(m).completeBaseName(), m);
  if(maps.isEmpty()) labelStatus->setText(tr("No hay archivos .tagmap: compile un daemon (Daemon > Modbus / Siemens TCP)"));
  else slotSource(0);
  timer->start(500);
}

DlgTagMonitor::~DlgTagMonitor()
{
  closeShm();
}

void DlgTagMonitor::closeShm()
{
  delete shm;
  shm = nullptr;
  modbus.reset();
  s7.reset();
  forced.clear();
  lastForceWrite.clear();
}

// TARGET.tagmap: source=, shared_memory=, size=, mailbox= y una linea "tag name=... type=... offset=..." por variable
void DlgTagMonitor::loadTagMap(const QString &file)
{
  items.clear();
  names.clear();
  source = QFileInfo(file).completeBaseName();
  shmName.clear();
  mailbox.clear();
  protocol = "modbus";
  shmSize = 0;
  ciclo = -1;
  QFile f(file);
  if(!f.open(QIODevice::ReadOnly)) return;
  const QStringList lines = QString::fromUtf8(f.readAll()).split('\n');
  for(QString line : lines)
  {
    line = line.trimmed();
    if(line.isEmpty() || line.startsWith('#')) continue;
    std::string err;
    if(line.startsWith("tag "))
    {
      std::vector< std::pair<std::string, std::string> > p = rlTagList::splitPairs(line.mid(4).toStdString(), err);
      Item it;
      memset(&it.tag, 0, sizeof(it.tag));
      it.tag.status = -1; it.tag.bit = -1; it.tag.scale = 1.0;
      for(size_t i=0; i<p.size(); i++)
      {
        const std::string &k = p[i].first, &v = p[i].second;
        if     (k == "name")    it.name = QString::fromStdString(v);
        else if(k == "type")    { it.type = QString::fromStdString(v); it.tag.type = rlTagList::typeFromString(v); }
        else if(k == "offset")  it.tag.offset = atoi(v.c_str());
        else if(k == "size")    it.tag.size = atoi(v.c_str());
        else if(k == "bit")     it.tag.bit = atoi(v.c_str());
        else if(k == "status")  it.tag.status = atoi(v.c_str());
        else if(k == "order")   it.tag.order = rlTagList::orderFromString(v);
        else if(k == "scale")   it.tag.scale = QString::fromStdString(v).toDouble();
        else if(k == "soffset") it.tag.scale_offset = QString::fromStdString(v).toDouble();
        else if(k == "desc")    it.desc = QString::fromStdString(v);
        else if(k == "rw")      it.tag.rw = atoi(v.c_str());       // para escribir (tagmap nuevo)
        else if(k == "slave")   it.tag.slave = atoi(v.c_str());
        else if(k == "area")    it.tag.area = atoi(v.c_str());
        else if(k == "dbnum")   it.tag.dbnum = atoi(v.c_str());
        else if(k == "address") it.tag.address = atoi(v.c_str());
        else if(k == "plcaddr") it.addr = QString::fromStdString(v);   // %MW0.0, DB10.DBX0.1, nodo OPC UA
      }
      if(it.tag.order < 0) it.tag.order = 0;
      if(it.name == "SYS.Ciclo") ciclo = (int) items.size();
      items.push_back(it);
      continue;
    }
    int eq = line.indexOf('=');
    if(eq < 0) continue;
    QString key = line.left(eq).trimmed(), val = line.mid(eq + 1).trimmed();
    if(val.startsWith('"') && val.endsWith('"') && val.size() >= 2) val = val.mid(1, val.size() - 2);
    if     (key == "source")        source = val;
    else if(key == "shared_memory") shmName = val;
    else if(key == "size")          shmSize = val.toInt();
    else if(key == "mailbox")       mailbox = val;
    else if(key == "protocol")      protocol = val;
  }
  // ruta relativa (automation/shm/PLC1.shm): desde la carpeta del .tagmap (la del proyecto)
  for(QString *s : { &shmName, &mailbox })
  {
    while(s->contains("\\\\")) s->replace("\\\\", "\\");
#ifdef Q_OS_WIN
    // en Windows el mailbox es un mailslot (un nombre, no un archivo): tiene que quedar igual que en el daemon
    if(s == &mailbox) continue;
#endif
    if(!s->isEmpty() && QFileInfo(*s).isRelative()) *s = QFileInfo(file).absoluteDir().absoluteFilePath(*s);
  }
  names.reserve(items.size());
  for(size_t i=0; i<items.size(); i++) { names.push_back(items[i].name.toUtf8()); items[i].tag.name = names.back().constData(); }
}

void DlgTagMonitor::slotSource(int index)
{
  closeShm();
  loadTagMap(comboSource->itemData(index).toString());
  if(!shmName.isEmpty() && shmSize > 0)
  {
    // abrir no borra nada: si el daemon no esta corriendo se ven ceros (o los ultimos valores que dejo)
    shm = new rlSharedMemory(shmName.toLocal8Bit().constData(), shmSize);
    if(shm->status != rlSharedMemory::OK) { delete shm; shm = nullptr; }
  }
  bool canWrite = shm != nullptr && !mailbox.isEmpty();
  if(canWrite)
  {
    QByteArray m = mailbox.toLocal8Bit(), s = shmName.toLocal8Bit();
    if(protocol == "siemens")    s7.reset(new rlSiemensTCPClient(m.constData(), s.constData(), shmSize));
    else                         modbus.reset(new rlModbusClient(m.constData(), s.constData(), shmSize));
  }
  for(QWidget *w : QList<QWidget *>{ editValue, buttonWrite, buttonForce, buttonRelease, buttonReleaseAll })   // con llaves (con "<<" la lista temporal se destruia antes del for)
  {
    w->setEnabled(canWrite);
    if(!w->property("tipOriginal").isValid()) w->setProperty("tipOriginal", w->toolTip());
    w->setToolTip(canWrite ? w->property("tipOriginal").toString() : tr("Para escribir hay que volver a compilar el daemon con esta version de pvdevelop (el .tagmap tiene que traer mailbox= y la direccion de cada tag)"));
  }
  slotFilter();
}

void DlgTagMonitor::slotFilter()
{
  const QString f = editFilter->text().trimmed();
  shown.clear();
  for(size_t i=0; i<items.size(); i++)
    if(f.isEmpty() || items[i].name.contains(f, Qt::CaseInsensitive) || items[i].addr.contains(f, Qt::CaseInsensitive)) shown.push_back((int) i);
  table->setSortingEnabled(false);
  table->setRowCount((int) shown.size());
  for(int r=0; r<(int) shown.size(); r++)
  {
    const Item &it = items[shown[r]];
    QTableWidgetItem *n = new QTableWidgetItem(it.name);
    n->setData(Qt::UserRole, shown[r]);   // la fila sigue al tag aunque se ordene
    if(it.name.startsWith("SYS.")) n->setForeground(QBrush(Qt::darkBlue));
    table->setItem(r, T_NAME, n);
    QTableWidgetItem *a = new QTableWidgetItem(it.addr);
    a->setTextAlignment(Qt::AlignCenter);
    table->setItem(r, T_ADDR, a);
    QTableWidgetItem *v = new NumItem();
    v->setTextAlignment(Qt::AlignCenter);
    table->setItem(r, T_VALUE, v);
    for(int c : { T_QUALITY, T_FORCED }) { QTableWidgetItem *x = new QTableWidgetItem(); x->setTextAlignment(Qt::AlignCenter); table->setItem(r, c, x); }
    QTableWidgetItem *t = new QTableWidgetItem(it.type);
    t->setTextAlignment(Qt::AlignCenter);
    table->setItem(r, T_TYPE, t);
    table->setItem(r, T_DESC, new QTableWidgetItem(it.desc));
  }
  refresh(); // tambien en pausa: las filas nuevas
  table->setSortingEnabled(true);
  table->resizeColumnToContents(T_NAME);
  if(table->columnWidth(T_NAME) < 180) table->setColumnWidth(T_NAME, 180);
  table->setColumnWidth(T_VALUE, 120);
  table->resizeColumnToContents(T_ADDR);   // %MW0.0 corto, un nodo OPC UA largo: entre 100 y 260
  table->setColumnWidth(T_ADDR, qBound(100, table->columnWidth(T_ADDR) + 16, 260));
}

void DlgTagMonitor::slotRefresh()
{
  if(!checkPause->isChecked()) refresh();
}

void DlgTagMonitor::refresh()
{
  if(shm == nullptr)
  {
    labelStatus->setText(items.empty() ? labelStatus->text() : tr("No se pudo abrir la memoria compartida %1").arg(shmName));
    return;
  }
  // el daemon cerrado deja los ultimos valores con calidad 1: SYS.Ciclo tiene que avanzar
  bool alive = (ciclo < 0) || rlTagAlive(shm, items[ciclo].tag, 5);
  qint64 now = QDateTime::currentMSecsSinceEpoch();
  bool sorting = table->isSortingEnabled();
  table->setSortingEnabled(false);   // si no, las filas se mueven mientras se actualizan
  int bad = 0;
  for(int r=0; r<table->rowCount(); r++)
  {
    QTableWidgetItem *nameItem = table->item(r, T_NAME);
    if(nameItem == nullptr) continue;
    int index = nameItem->data(Qt::UserRole).toInt();
    const Item &it = items[index];
    double v = rlTagGetValue(shm, it.tag);
    int q = rlTagQuality(shm, it.tag);
    if(!alive) q = 0;
    QString text;
    if(it.tag.type == RLTAG_BOOL) text = (v != 0) ? "1" : "0";
    else if(it.tag.type == RLTAG_REAL || it.tag.type == RLTAG_LREAL || it.tag.scale != 1.0 || it.tag.scale_offset != 0.0)
      text = QString::number(v, 'g', 7);
    else text = QString::number((qlonglong) llround(v));
    // horas de los tags de sistema (segundos desde 1970)
    if((it.name == "SYS.UltimaLecturaOk" || it.name == "SYS.Inicio") && v > 0)
      text = QDateTime::fromSecsSinceEpoch((qint64) v).toString("yyyy-MM-dd HH:mm:ss");
    if(table->item(r, T_VALUE)->text() != text) table->item(r, T_VALUE)->setText(text);
    QString qt = !alive ? tr("detenido") : (q < 0) ? "-" : (q == 1) ? tr("bien") : tr("error");
    if(table->item(r, T_QUALITY)->text() != qt)
    {
      table->item(r, T_QUALITY)->setText(qt);
      table->item(r, T_QUALITY)->setForeground(QBrush(q == 0 ? Qt::red : Qt::darkGreen));
    }
    // forzado: se vuelve a escribir si el valor cambio (a lo sumo una vez por segundo)
    auto f = forced.find(index);
    QString ft = (f == forced.end()) ? QString() : QString::number(f->second, 'g', 10);
    if(table->item(r, T_FORCED)->text() != ft)
    {
      table->item(r, T_FORCED)->setText(ft);
      for(int c=0; c<T_COUNT; c++) table->item(r, c)->setBackground(ft.isEmpty() ? QBrush() : QBrush(QColor(255, 236, 160)));
    }
    if(f != forced.end() && alive && fabs(v - f->second) > 1e-9 && now - lastForceWrite[index] >= 1000)
    {
      lastForceWrite[index] = now;
      writeItem(index, f->second);
    }
    if(q == 0) bad++;
  }
  table->setSortingEnabled(sorting);
  QString st = tr("%1: %2 tags (%3 mostrados), %4 con error de lectura").arg(source).arg(items.size()).arg(shown.size()).arg(bad);
  if(!forced.empty()) st += tr(", %1 forzados").arg(forced.size());
  if(!alive) st = tr("%1: DAEMON DETENIDO (SYS.Ciclo no avanza): los valores son los ultimos que dejo").arg(source);
  if(!buttonWrite->isEnabled() && shm != nullptr) st += tr(". Escribir y forzar: volver a compilar el daemon con esta version (el .tagmap no trae mailbox= ni las direcciones)");
  else if(ciclo < 0) st += tr(". Sin SYS.Ciclo: no se puede saber si el daemon esta corriendo (volver a compilarlo)");
  labelStatus->setStyleSheet(alive ? QString() : QString("color: #b00000"));
  labelStatus->setText(st + tr(". Memoria: %1").arg(shmName));
}

int DlgTagMonitor::selectedItem() const
{
  QList<QTableWidgetItem *> sel = table->selectedItems();
  if(sel.isEmpty()) return -1;
  QTableWidgetItem *n = table->item(sel.first()->row(), T_NAME);
  return n ? n->data(Qt::UserRole).toInt() : -1;
}

void DlgTagMonitor::slotSelection()
{
  int i = selectedItem();
  if(i < 0) return;
  auto f = forced.find(i);
  QList<QTableWidgetItem *> sel = table->selectedItems();
  QTableWidgetItem *cell = sel.isEmpty() ? nullptr : table->item(sel.first()->row(), T_VALUE);
  QString v = (f != forced.end()) ? QString::number(f->second, 'g', 10) : (cell ? cell->text() : QString());
  editValue->setText(v);
  bool rw = items[i].tag.rw != 0;
  editValue->setPlaceholderText(rw ? QString() : tr("solo lectura"));
}

bool DlgTagMonitor::confirmWrite()
{
  if(writeConfirmed) return true;
  writeConfirmed = QMessageBox::question(this, windowTitle(),
                     tr("Escribir y forzar cambia valores en el PLC (si el daemon no esta en simulacion, es la maquina real).\n\nContinuar?"))
                   == QMessageBox::Yes;
  return writeConfirmed;
}

bool DlgTagMonitor::writeItem(int index, double value)
{
  const rlTag &t = items[index].tag;
  int r = -1;
  if(t.type == RLTAG_BOOL)
    r = modbus ? modbus->setBool(t, value != 0) : s7 ? s7->setBool(t, value != 0) : -1;
  else if(t.type == RLTAG_REAL || t.type == RLTAG_LREAL)
    r = modbus ? modbus->setReal(t, value) : s7 ? s7->setReal(t, value) : -1;
  else
    r = modbus ? modbus->setInt(t, (int) lround(value)) : s7 ? s7->setInt(t, (int) lround(value)) : -1;
  return r >= 0;
}

void DlgTagMonitor::slotWrite(bool force)
{
  int i = selectedItem();
  if(i < 0) { QMessageBox::information(this, windowTitle(), tr("Elegir un tag de la lista")); return; }
  const Item &it = items[i];
  if(it.tag.rw == 0) { QMessageBox::information(this, windowTitle(), tr("%1 es de solo lectura (rw=0 en el .tagmap). En Daemon > Modbus / Siemens TCP, Tags..., marcar la columna RW de ese tag y volver a compilar el daemon.").arg(it.name)); return; }
  QString text = editValue->text().trimmed();
  text.replace(',', '.');
  if(text == "true" || text == "on") text = "1";
  if(text == "false" || text == "off") text = "0";
  bool ok;
  double v = text.toDouble(&ok);
  if(!ok) { QMessageBox::warning(this, windowTitle(), tr("Valor no valido")); return; }
  if(!confirmWrite()) return;
  if(!writeItem(i, v)) { QMessageBox::warning(this, windowTitle(), tr("No se pudo escribir %1 (esta corriendo el daemon?)").arg(it.name)); return; }
  if(force)
  {
    forced[i] = v;
    lastForceWrite[i] = QDateTime::currentMSecsSinceEpoch();
  }
  else forced.erase(i);
  refresh();
}

void DlgTagMonitor::slotRelease(bool all)
{
  if(all) forced.clear();
  else
  {
    int i = selectedItem();
    if(i >= 0) forced.erase(i);
  }
  refresh();
}

// el daemon de este .tagmap: <source>.exe (Modbus, Siemens)
void DlgTagMonitor::slotStart(bool simulation)
{
  if(source.isEmpty()) return;
  QString file = QFileInfo(comboSource->currentData().toString()).completeBaseName();
  QStringList args;
  QString program;
  {
#ifdef PVUNIX
    program = "./" + source;
#else
    program = source + ".exe";
#endif
    if(!QFile::exists(program))
    {
      QMessageBox::warning(this, "pvdevelop", tr("No existe %1: compilar el daemon (Daemon > Modbus / Siemens TCP)").arg(program));
      return;
    }
    if(simulation) args << "-sim";
  }
  QString title = source + (simulation ? tr(" (simulacion)") : QString());
  // Nestor: en su propia consola (buildconsole.cpp), sin armar un comando de cmd ni depender de xterm
  if(QFile::exists(program)) program = QDir::current().absoluteFilePath(program);
  bool ok = pvbStartInConsole(QDir::toNativeSeparators(program), args, QDir::currentPath(), title);
  if(!ok) QMessageBox::warning(this, "pvdevelop", tr("No se pudo ejecutar %1").arg(program));
}
