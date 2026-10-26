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
** Nestor: Designer > Propiedades > Tags... (ver dlgbind.h; la ventana esta en dlgbind.ui)
****************************************************************************/
#include "dlgbind.h"
#include "ui_dlgbind.h"
#include <QCompleter>
#include <QMessageBox>
#include <QRegularExpression>
#include "pvimages.h"   // pvbAddImageButton

enum { K_NONE, K_WRITE, K_TOGGLE, K_ADD, K_SUB, K_GOTO, K_BACK, K_HOME, K_COUNT };
static const char *kindIds[] = { "", "write", "toggle", "add", "sub", "goto", "back", "home" };

static QString quoted(const QString &v)
{
  if(v.isEmpty() || v.contains(' ') || v.contains('"') || v.contains('\t') || v.contains('=') || v.contains(';'))
  {
    QString q = v;
    q.replace("\"", "\"\"");
    return "\"" + q + "\"";
  }
  return v;
}

DlgBind::DlgBind(QWidget *widget, QWidget *parent) : QDialog(parent)
{
  ui = new Ui_DlgBind;
  ui->setupUi(this);
  wtype = widget->statusTip().section(':', 0, 0).trimmed();
  tags = pvbProjectTags();
  setWindowTitle(tr("Accion y animacion de %1").arg(widget->objectName()));
  ui->labelNoTags->setVisible(tags.isEmpty());

  // ---- accion
  setupEventRow(&click, ui->kindClick, ui->tagClick, ui->valueClick, ui->maskClick);
  setupEventRow(&press, ui->kindPress, ui->tagPress, ui->valuePress, ui->maskPress);
  setupEventRow(&release, ui->kindRelease, ui->tagRelease, ui->valueRelease, ui->maskRelease);
  connect(ui->buttonMomentary, &QPushButton::clicked, this, [this]() {
    QString t = tagOf(press.tag).isEmpty() ? tagOf(click.tag) : tagOf(press.tag);
    if(t.isEmpty()) return;
    setEventRow(&press, "write:" + t + "=1");
    setEventRow(&release, "write:" + t + "=0");
  });
  enterTag = ui->enterTag;     editMin = ui->editMin;         editMax = ui->editMax;
  editConfirm = ui->editConfirm;
  condTag = ui->condTag;       condOp = ui->condOp;           condValue = ui->condValue;   condMsg = ui->condMsg;

  // ---- animacion
  textTag = ui->textTag;       editFmt = ui->editFmt;         valueTag = ui->valueTag;
  colorTag = ui->colorTag;     editColors = ui->editColors;   colorPart = ui->colorPart;
  blinkTag = ui->blinkTag;     visibleTag = ui->visibleTag;   checkInvert = ui->checkInvert;
  checkQuality = ui->checkQuality;
  for(QComboBox *c : { enterTag, condTag, textTag, valueTag, colorTag, blinkTag, visibleTag, ui->imageTag, ui->animTag }) fillTags(c);
  ui->groupImage->setVisible(wtype == "TQImage");
  pvbAddImageButton(ui->editImages, true);   // de cualquier carpeta (copia a Image/)
  pvbAddImageButton(ui->editFrames, true);
  // un boton empieza en Accion; un label o una barra, en Animacion
  if(wtype != "TQPushButton" && wtype != "TQLineEdit" && wtype != "TQSlider" && wtype != "TQDial" &&
     wtype != "TQSpinBox" && wtype != "TQCheck" && wtype != "TQRadio") ui->tabs->setCurrentIndex(1);

  // valores actuales
  QStringList p = pvbSplitPairs(pvbBinding(widget));
  QMap<QString, QString> kv;
  for(int k=0; k+1<p.size(); k+=2) kv[p[k]] = p[k+1];
  setEventRow(&click, kv.value("click"));
  setEventRow(&press, kv.value("press"));
  setEventRow(&release, kv.value("release"));
  setTag(enterTag, kv.value("enter"));
  editMin->setText(kv.value("min"));
  editMax->setText(kv.value("max"));
  editConfirm->setText(kv.value("confirm"));
  QRegularExpression rx("^\\s*(\\S+)\\s*(==|!=|>=|<=|>|<|=)\\s*(\\S+)\\s*$");
  QRegularExpressionMatch cm = rx.match(kv.value("cond"));
  if(cm.hasMatch())
  {
    setTag(condTag, cm.captured(1));
    condOp->setCurrentText(cm.captured(2) == "=" ? QString("==") : cm.captured(2));
    condValue->setText(cm.captured(3));
  }
  condMsg->setText(kv.value("condmsg"));
  setTag(textTag, kv.value("text"));
  editFmt->setText(kv.value("fmt"));
  setTag(valueTag, kv.value("value"));
  setTag(colorTag, kv.value("color"));
  editColors->setText(kv.value("colors"));
  colorPart->setCurrentIndex(kv.value("colorpart") == "fg" ? 1 : 0);
  setTag(blinkTag, kv.value("blink"));
  QString vis = kv.value("visible");
  checkInvert->setChecked(vis.startsWith('!'));
  setTag(visibleTag, vis.startsWith('!') ? vis.mid(1) : vis);
  checkQuality->setChecked(kv.value("quality", "1") != "0");
  setTag(ui->imageTag, kv.value("image"));
  ui->editImages->setText(kv.value("images"));
  setTag(ui->animTag, kv.value("anim"));
  ui->editFrames->setText(kv.value("frames"));
  if(kv.contains("period")) ui->spinPeriod->setValue(kv.value("period").toInt());
}

DlgBind::~DlgBind()
{
  delete ui;
}

// Nestor: el formato va tal cual a snprintf en el pvserver: si no es de un double, el pvserver usa %g
void DlgBind::accept()
{
  const QString tag = tagOf(textTag), fmt = editFmt->text();
  if(!tag.isEmpty() && !pvbValidTextFormat(fmt))
  {
    if(QMessageBox::warning(this, windowTitle(),
         tr("El formato \"%1\" no sirve para mostrar un numero.\n\n"
            "Tiene que tener una sola conversion de printf para un numero con decimales "
            "(%f, %.1f, %g, %e; %% para el signo de por ciento), o ser una lista de textos como Parado|Marcha.\n\n"
            "Si se guarda asi, el pvserver muestra el valor con %g. Guardar igual?").arg(fmt),
         QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    {
      editFmt->setFocus();
      return;
    }
  }
  QDialog::accept();
}

// combo editable con los tags del proyecto (busca por cualquier parte del nombre)
void DlgBind::fillTags(QComboBox *c)
{
  c->setInsertPolicy(QComboBox::NoInsert);
  c->addItem(QString());
  for(const PvbTag &t : tags)
  {
    c->addItem(t.source + ":" + t.name);
    c->setItemData(c->count() - 1, t.type, Qt::ToolTipRole);
  }
  c->completer()->setFilterMode(Qt::MatchContains);
  c->completer()->setCompletionMode(QCompleter::PopupCompletion);
}

void DlgBind::setupEventRow(EventRow *r, QComboBox *kind, QComboBox *tag, QLineEdit *value, QSpinBox *mask)
{
  r->kind = kind; r->tag = tag; r->value = value; r->mask = mask;
  const char *names[] = { QT_TR_NOOP("Nada"), QT_TR_NOOP("Escribir valor"), QT_TR_NOOP("Alternar (bool)"),
                          QT_TR_NOOP("Sumar"), QT_TR_NOOP("Restar"), QT_TR_NOOP("Ir a pantalla"),
                          QT_TR_NOOP("Volver"), QT_TR_NOOP("Inicio") };
  for(int i=0; i<K_COUNT; i++) kind->addItem(tr(names[i]), kindIds[i]);
  fillTags(tag);
  auto update = [r]() {
    int k = r->kind->currentIndex();
    r->tag->setEnabled(k >= K_WRITE && k <= K_SUB);
    r->value->setVisible(k == K_WRITE || k == K_ADD || k == K_SUB);
    r->mask->setVisible(k == K_GOTO);
  };
  connect(kind, QOverload<int>::of(&QComboBox::currentIndexChanged), this, update);
  update();
}


void DlgBind::setEventRow(EventRow *r, const QString &spec)
{
  QString kind = spec.section(':', 0, 0).trimmed().toLower(), arg = spec.section(':', 1).trimmed();
  int k = 0;
  for(int i=1; i<K_COUNT; i++) if(kind == kindIds[i]) k = i;
  r->kind->setCurrentIndex(k);
  if(k == K_GOTO) r->mask->setValue(arg.toInt());
  else if(k == K_TOGGLE) setTag(r->tag, arg);
  else if(k == K_WRITE || k == K_ADD || k == K_SUB)
  {
    int eq = arg.lastIndexOf('=');
    setTag(r->tag, eq > 0 ? arg.left(eq) : arg);
    r->value->setText(eq > 0 ? arg.mid(eq + 1) : QString());
  }
}

QString DlgBind::eventSpec(const EventRow &r) const
{
  int k = r.kind->currentIndex();
  QString t = tagOf(r.tag);
  switch(k)
  {
    case K_WRITE: case K_ADD: case K_SUB:
      return t.isEmpty() ? QString() : QString("%1:%2=%3").arg(kindIds[k], t, r.value->text().trimmed().isEmpty() ? QString("0") : r.value->text().trimmed());
    case K_TOGGLE: return t.isEmpty() ? QString() : "toggle:" + t;
    case K_GOTO:   return QString("goto:%1").arg(r.mask->value());
    case K_BACK:   return "back";
    case K_HOME:   return "home";
    default:       return QString();
  }
}

void DlgBind::setTag(QComboBox *c, const QString &tag)
{
  c->setCurrentText(tag.trimmed());
}

QString DlgBind::tagOf(QComboBox *c)
{
  return c->currentText().trimmed();
}

QString DlgBind::binding() const
{
  QStringList out;
  auto add = [&out](const QString &key, const QString &val) { if(!val.isEmpty()) out << key + "=" + quoted(val); };
  add("click", eventSpec(click));
  add("press", eventSpec(press));
  add("release", eventSpec(release));
  add("enter", tagOf(enterTag));
  add("min", editMin->text().trimmed());
  add("max", editMax->text().trimmed());
  add("confirm", editConfirm->text().trimmed());
  if(!tagOf(condTag).isEmpty() && !condValue->text().trimmed().isEmpty())
  {
    add("cond", tagOf(condTag) + " " + condOp->currentText() + " " + condValue->text().trimmed());
    add("condmsg", condMsg->text().trimmed());
  }
  add("text", tagOf(textTag));
  if(!tagOf(textTag).isEmpty()) add("fmt", editFmt->text());
  add("value", tagOf(valueTag));
  add("color", tagOf(colorTag));
  if(!tagOf(colorTag).isEmpty())
  {
    add("colors", editColors->text().trimmed());
    if(colorPart->currentIndex() == 1) add("colorpart", "fg");
  }
  add("blink", tagOf(blinkTag));
  if(!tagOf(visibleTag).isEmpty()) add("visible", (checkInvert->isChecked() ? "!" : "") + tagOf(visibleTag));
  if(!checkQuality->isChecked()) add("quality", "0");
  if(!tagOf(ui->imageTag).isEmpty() && !ui->editImages->text().trimmed().isEmpty())
  {
    add("image", tagOf(ui->imageTag));
    add("images", ui->editImages->text().trimmed());
  }
  if(!tagOf(ui->animTag).isEmpty() && !ui->editFrames->text().trimmed().isEmpty())
  {
    add("anim", tagOf(ui->animTag));
    add("frames", ui->editFrames->text().trimmed());
    add("period", QString::number(ui->spinPeriod->value()));
  }
  return out.join(' ');
}
