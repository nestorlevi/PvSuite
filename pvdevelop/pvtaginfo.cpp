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
#include "pvtaginfo.h"
#include <QCoreApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>
#include <QMenu>
#include <QLineEdit>
#include <QKeyEvent>
#include <QFile>
#include "dlgnewtag.h"
#include "../rllib/lib/rltagdef.h"

static QElapsedTimer tagsChecked;

void pvbTagsInvalidate()
{
  tagsChecked.invalidate();
}

const QList<PvbTag> &pvbTagsCached()
{
  static QList<PvbTag> tags;
  static QString signature;
  QElapsedTimer &checked = tagsChecked;
  if(checked.isValid() && checked.elapsed() < 1000) return tags;   // el mouse pide muchas veces seguidas
  checked.start();
  QString sig = QDir::currentPath();
  for(const QFileInfo &fi : QDir::current().entryInfoList(QStringList() << "*.tagmap", QDir::Files, QDir::Name))
    sig += QString("|%1:%2:%3").arg(fi.fileName()).arg(fi.size()).arg(fi.lastModified().toMSecsSinceEpoch());
  if(sig != signature)
  {
    signature = sig;
    tags = pvbProjectTags();
  }
  return tags;
}

// "Temp[i].Valor" -> ^Temp\[\d+\]\.Valor$ (un indice que no es un numero vale cualquier elemento)
static QRegularExpression nameRx(const QString &want)
{
  QString rx;
  for(int i = 0; i < want.size(); )
  {
    int j = (want[i] == '[') ? want.indexOf(']', i) : -1;
    if(j < 0) { rx += QRegularExpression::escape(QString(want[i])); i++; continue; }
    bool num = false;
    QString in = want.mid(i + 1, j - i - 1).trimmed();
    in.toInt(&num);
    rx += num ? QRegularExpression::escape("[" + in + "]") : QString("\\[\\d+\\]");
    i = j + 1;
  }
  return QRegularExpression("^" + rx + "$");
}

static bool findName(const QList<PvbTag> &tags, const QString &source, const QString &name, PvbTag *t)
{
  if(name.isEmpty()) return false;
  for(const PvbTag &x : tags)
    if((source.isEmpty() || x.source == source) && x.name == name) { *t = x; return true; }
  if(!name.contains('[')) return false;
  const QRegularExpression rx = nameRx(name);
  for(const PvbTag &x : tags)
    if((source.isEmpty() || x.source == source) && rx.match(x.name).hasMatch()) { *t = x; return true; }
  return false;
}

static bool isIntType(const QString &type)
{
  return type == "byte" || type == "int" || type == "uint" || type == "dint" || type == "udint";
}

bool pvbLookupTag(const QString &text0, PvbTag *t)
{
  QString text = text0.trimmed();
  while(text.startsWith('!') || text.startsWith('&')) text = text.mid(1).trimmed();
  if(text.isEmpty() || text.startsWith('$') || text.startsWith('@')) return false;
  const QList<PvbTag> &tags = pvbTagsCached();
  QList<QPair<QString, QString>> tries;   // fuente, nombre
  int c = text.indexOf(':');
  if(c > 0) tries << qMakePair(text.left(c), text.mid(c + 1));
  else
  {
    QStringList sources;
    for(const PvbTag &x : tags) if(!sources.contains(x.source)) sources << x.source;
    for(const QString &s : sources) if(text.startsWith(s + "_")) tries << qMakePair(s, text.mid(s.size() + 1));
    tries << qMakePair(QString(), text);   // using namespace tags_PLC1
  }
  for(const QPair<QString, QString> &p : tries)
  {
    if(findName(tags, p.first, p.second, t)) return true;
    // "PLC1:Estado.3": el bit 3 de un entero
    int dot = p.second.lastIndexOf('.');
    bool num = false;
    int bit = dot > 0 ? p.second.mid(dot + 1).toInt(&num) : -1;
    if(num && bit >= 0 && findName(tags, p.first, p.second.left(dot), t) && isIntType(t->type)) { t->bit = bit; return true; }
  }
  // un elemento de estructura entero (PLC1:Motores[0], el de un faceplate): sus miembros
  for(const QPair<QString, QString> &p : tries)
  {
    if(p.second.isEmpty()) continue;
    const QRegularExpression rx = nameRx(p.second);
    QStringList members;
    PvbTag first;
    for(const PvbTag &x : tags)
    {
      if(!p.first.isEmpty() && x.source != p.first) continue;
      int dot = x.name.lastIndexOf('.');
      if(dot <= 0 || !rx.match(x.name.left(dot)).hasMatch()) continue;
      if(members.isEmpty()) first = x;
      if(x.source == first.source && x.name.left(dot) == first.name.left(first.name.lastIndexOf('.'))) members << x.name.mid(dot + 1);
    }
    if(members.isEmpty()) continue;
    *t = first;
    t->name = first.name.left(first.name.lastIndexOf('.'));
    t->type = QCoreApplication::translate("PvbTagInfo", "estructura: %1").arg(members.join(", "));
    t->desc.clear(); t->unit.clear(); t->alarm.clear(); t->alarmText.clear();
    return true;
  }
  return false;
}

QString pvbTagSummary(const PvbTag &t, bool html)
{
  static const QMap<QString, QString> protocols = { { "modbus", "Modbus" }, { "siemens", "S7" } };
  QStringList a, b;
  a << QString("%1 (%2)").arg(t.source, protocols.value(t.protocol, t.protocol));
  QString address = t.name.startsWith("SYS.") ? QCoreApplication::translate("PvbTagInfo", "tag de sistema") : t.address;
  if(!address.isEmpty()) a << address;
  a << (t.bit >= 0 ? QCoreApplication::translate("PvbTagInfo", "bit %1 de %2").arg(t.bit).arg(t.type) : t.type);
  a << (t.rw ? QCoreApplication::translate("PvbTagInfo", "lectura/escritura") : QCoreApplication::translate("PvbTagInfo", "solo lectura"));
  if(!t.desc.isEmpty()) b << "\"" + t.desc + "\"";
  if(!t.unit.isEmpty()) b << t.unit;
  if(!t.alarm.isEmpty()) b << QCoreApplication::translate("PvbTagInfo", "alarma %1").arg(t.alarm) + (t.alarmText.isEmpty() ? QString() : " (" + t.alarmText + ")");
  const QString sep = QString::fromUtf8(" \xC2\xB7 ");
  QString name = t.source + ":" + t.name + (t.bit >= 0 ? QString(".%1").arg(t.bit) : QString());
  if(!html) return name + " - " + (a + b).join(sep);
  QString s = "<b>" + name.toHtmlEscaped() + "</b><br>" + a.join(sep).toHtmlEscaped();
  if(!b.isEmpty()) s += "<br>" + b.join(sep).toHtmlEscaped();
  return s;
}

static bool identChar(QChar c) { return c.isLetterOrNumber() || c == '_'; }

QString pvbTagWordAt(const QString &line, int pos)
{
  if(pos >= line.size() || (pos < line.size() && !identChar(line[pos]) && line[pos] != '.')) pos--;
  if(pos < 0 || pos >= line.size() || (!identChar(line[pos]) && line[pos] != '.')) return QString();
  int start = pos, end = pos;
  while(start > 0)
  {
    QChar c = line[start - 1];
    if(identChar(c) || c == '.') start--;
    else if(c == ']') { int j = line.lastIndexOf('[', start - 2); if(j < 0) break; start = j; }
    else break;
  }
  while(end < line.size())
  {
    QChar c = line[end];
    if(identChar(c) || c == '.') end++;
    else if(c == '[') { int j = line.indexOf(']', end); if(j < 0) break; end = j + 1; }
    else break;
  }
  QString w = line.mid(start, end - start);
  while(w.startsWith('.')) w = w.mid(1);
  while(w.endsWith('.')) w.chop(1);
  if(w.isEmpty() || w[0].isDigit()) return QString();
  return w;
}

bool pvbTagAt(const QString &line, int pos, PvbTag *t)
{
  QString w = pvbTagWordAt(line, pos);
  if(w.isEmpty()) return false;
  if(pvbLookupTag(w, t)) return true;
  // hasta el final del tramo donde esta el mouse (PLC1_Motores[0] de PLC1_Motores[0].Marcha no es un tag, pero
  // Bomba1.Velocidad de Bomba1.Velocidad.x si)
  int start = line.indexOf(w, qMax(0, pos - w.size()));
  if(start < 0) return false;
  int cut = pos - start;
  while(cut < w.size() && identChar(w[cut])) cut++;
  return cut < w.size() && pvbLookupTag(w.left(cut), t);
}

// Nestor: campo de tag con una direccion: Enter la cambia por el tag (o se elige / se crea); al salir, si hay uno solo
class AddressFilter : public QObject
{
public:
  AddressFilter(QComboBox *c) : QObject(c), combo(c) {}
  bool eventFilter(QObject *o, QEvent *e) override
  {
    if(e->type() == QEvent::FocusOut)   // despues (no durante: al destruir el dialogo tambien pierde el foco)
    {
      QComboBox *c = combo;
      QTimer::singleShot(0, combo, [c]() {
        const QString text = c->currentText().trimmed();
        if(!pvbLooksLikeAddress(text)) return;
        const QList<PvbTag> tags = pvbTagsAtAddress(text);
        if(tags.size() == 1) c->setCurrentText(tags.first().source + ":" + tags.first().name);
      });
      return false;
    }
    if(e->type() == QEvent::KeyPress)
    {
      const QString text = combo->currentText().trimmed();
      QKeyEvent *ke = static_cast<QKeyEvent *>(e);
      if((ke->key() != Qt::Key_Return && ke->key() != Qt::Key_Enter) || !pvbLooksLikeAddress(text)) return false;
      QStringList canCreate;
      if(pvbTagsAtAddress(text, &canCreate).isEmpty() && canCreate.isEmpty()) return false;
      const QString tag = pvbResolveAddress(combo, text, combo->mapToGlobal(QPoint(0, combo->height())), false);
      if(!tag.isEmpty()) combo->setCurrentText(tag);
      return true;   // no es el OK del dialogo
    }
    return QObject::eventFilter(o, e);
  }
private:
  QComboBox *combo;
};

void pvbAttachTagInfo(QComboBox *combo)
{
  if(combo == nullptr || combo->property("pvbTagInfo").isValid()) return;
  combo->setProperty("pvbTagInfo", true);
  for(int i = 0; i < combo->count(); i++)   // la lista y las sugerencias al escribir
  {
    PvbTag t;
    if(pvbLookupTag(combo->itemText(i), &t)) combo->setItemData(i, pvbTagSummary(t, true), Qt::ToolTipRole);
  }
  auto update = [combo]() {
    // la ayuda del campo (la puede poner el dialogo despues): se le agregan los datos del tag
    if(combo->toolTip() != combo->property("pvbTipSet").toString()) combo->setProperty("pvbBaseTip", combo->toolTip());
    const QString baseTip = combo->property("pvbBaseTip").toString();
    const QString text = combo->currentText().trimmed();
    PvbTag t;
    QString tip;
    QStringList canCreate;
    QList<PvbTag> atAddress;
    if(pvbLooksLikeAddress(text)) atAddress = pvbTagsAtAddress(text, &canCreate);
    if(text.isEmpty() || text.startsWith('$') || text.startsWith('@')) ;
    else if(atAddress.size() == 1)
      tip = QCoreApplication::translate("PvbTagInfo", "Enter: cambiar por este tag").toHtmlEscaped() + "<br>" + pvbTagSummary(atAddress.first(), true);
    else if(atAddress.size() > 1)
      tip = QCoreApplication::translate("PvbTagInfo", "Enter: elegir uno de los %1 tags en %2").arg(atAddress.size()).arg(text).toHtmlEscaped();
    else if(!canCreate.isEmpty())
      tip = QCoreApplication::translate("PvbTagInfo", "No hay ningun tag en %1. Enter: crear uno").arg(text).toHtmlEscaped();
    else if(pvbTagsCached().isEmpty()) ;
    else if(pvbLookupTag(text, &t)) tip = pvbTagSummary(t, true);
    else tip = "<span style=\"color:#d03030\">" +
               QCoreApplication::translate("PvbTagInfo", "No existe el tag \"%1\" en los .tagmap del proyecto").arg(text).toHtmlEscaped() + "</span>";
    const QString full = tip.isEmpty() ? baseTip : (baseTip.isEmpty() ? tip : baseTip.toHtmlEscaped().replace('\n', "<br>") + "<hr>" + tip);
    combo->setProperty("pvbTipSet", full);
    combo->setToolTip(full);
  };
  if(combo->lineEdit()) combo->lineEdit()->installEventFilter(new AddressFilter(combo));
  QObject::connect(combo, &QComboBox::editTextChanged, combo, update);
  QObject::connect(combo, &QComboBox::currentIndexChanged, combo, update);
  update();
  QTimer::singleShot(0, combo, update);   // con la ayuda que el dialogo pone despues
}

//------------------------------------------------------------------------------------------------
// tag por su direccion

static QString daemonTarget(const QString &file)
{
  QFile f(file);
  if(f.open(QIODevice::ReadOnly))
  {
    static const QRegularExpression rx(R"(^\s*target\s*=\s*"?([^"\s]+))", QRegularExpression::MultilineOption);
    QRegularExpressionMatch m = rx.match(QString::fromUtf8(f.readAll()));
    if(m.hasMatch() && !file.endsWith(".mkmodbus")) return m.captured(1);   // Modbus: el nombre del archivo (gmodbus)
  }
  return QFileInfo(file).completeBaseName();
}

QList<PvbDaemonFile> pvbDaemonFiles()
{
  QList<PvbDaemonFile> list;
  const QList<QPair<QString, int>> kinds = { { "*.mkmodbus", rlTagList::MODBUS }, { "*.mksiemens", rlTagList::SIEMENS } };
  for(const QPair<QString, int> &k : kinds)
    for(const QString &f : QDir::current().entryList(QStringList() << k.first, QDir::Files, QDir::Name))
    {
      PvbDaemonFile d;
      d.file = f;
      d.source = daemonTarget(f);
      d.protocol = k.second;
      list << d;
    }
  return list;
}

bool pvbLooksLikeAddress(const QString &text)
{
  // %MW1 %MD2 %MX0.1 %M5 %I0.0 %IW3 | 40002 00006.1 | HR1 IR2 CO3 DI4 | DB1.DBW0 DB1.DBX0.1 MW10 MB0 M0.1 E0.0 I0.0 A1.0 Q1.0
  static const QRegularExpression rx(R"(^(%[A-Z]{1,2}\d+([.:]X?\d+)?|\d+(\.\d+)?|(HR|IR|CO|DI)\d+(\.\d+)?|)"
                                     R"(DB\d+\.DB[XBWD]\d+(\.\d+)?|[MEAIQ][BWD]?\d+(\.\d+)?)$)", QRegularExpression::CaseInsensitiveOption);
  return rx.match(text.trimmed()).hasMatch();
}

QList<PvbTag> pvbTagsAtAddress(const QString &text0, QStringList *canCreate)
{
  QList<PvbTag> found;
  const QString text = text0.trimmed();
  if(text.isEmpty()) return found;
  for(const PvbDaemonFile &d : pvbDaemonFiles())
  {
    if(!pvbLooksLikeAddress(text)) continue;
    rlTagList list(d.protocol);
    list.readMkFile(QFile::encodeName(d.file).constData());
    rlTagDef want;
    std::string err;
    if(list.parseAddress(text.toStdString(), want, err) != 0) continue;
    if(canCreate) *canCreate << d.file;
    list.layout(0);
    for(const rlTagDef &f : list.flat)
    {
      if(f.area != want.area || f.addr != want.addr || f.bit != want.bit) continue;
      if(d.protocol == rlTagList::SIEMENS && f.dbnum != want.dbnum) continue;
      PvbTag t;
      if(!pvbLookupTag(d.source + ":" + QString::fromStdString(f.cname), &t))   // sin .tagmap todavia
      {
        t.source = d.source;
        t.name = QString::fromStdString(f.cname);
        t.type = rlTagList::typeName(f.type);
        t.address = text;
        t.rw = f.rw != 0;
        t.desc = QString::fromStdString(f.description);
      }
      found << t;
    }
  }
  return found;
}

static QString tagText(const PvbTag &t, bool code)
{
  return t.source + (code ? "_" : ":") + t.name;
}

QString pvbResolveAddress(QWidget *parent, const QString &text0, const QPoint &globalPos, bool code)
{
  const QString text = text0.trimmed();
  QStringList canCreate;
  const QList<PvbTag> tags = pvbTagsAtAddress(text, &canCreate);
  if(tags.isEmpty() && canCreate.isEmpty()) return QString();
  if(tags.size() == 1) return tagText(tags.first(), code);
  QMenu menu(parent);
  for(const PvbTag &t : tags)
  {
    QAction *a = menu.addAction(tagText(t, code) + "   " + pvbTagSummary(t, false).section(" - ", 1));
    a->setData(tagText(t, code));
  }
  QAction *create = nullptr;
  if(!canCreate.isEmpty())
  {
    if(!tags.isEmpty()) menu.addSeparator();
    create = menu.addAction(QCoreApplication::translate("PvbTagInfo", "Crear un tag en %1...").arg(text));
  }
  QAction *chosen = menu.exec(globalPos);
  if(chosen == nullptr) return QString();
  if(chosen != create) return chosen->data().toString();
  DlgNewTag dlg(text, canCreate, parent);
  if(dlg.exec() != QDialog::Accepted) return QString();
  pvbTagsInvalidate();
  return dlg.source() + (code ? "_" : ":") + dlg.name();
}
