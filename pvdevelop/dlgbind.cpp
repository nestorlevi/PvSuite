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
#include "pvtaginfo.h"
#include "ui_dlgbind.h"
#include <QCompleter>
#include <QMap>
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

  setHelp();

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

// Nestor: cada fila dice que hace y en que widgets sirve; si el widget que se edita no es de esos, lo avisa
void DlgBind::setHelp()
{
  static const QMap<QString, QString> names = {
    { "TQPushButton", "PushButton" }, { "TQLabel", "Label" }, { "TQLineEdit", "LineEdit" }, { "TQSlider", "Slider" },
    { "TQDial", "Dial" }, { "TQSpinBox", "SpinBox" }, { "TQCheck", "CheckBox" }, { "TQRadio", "RadioButton" },
    { "TQProgressBar", "ProgressBar" }, { "TQLCDNumber", "LCDNumber" }, { "TQImage", "Image" }, { "TQComboBox", "ComboBox" } };
  const QString self = names.value(wtype, wtype);
  // types vacio: todos los widgets
  auto help = [&](std::initializer_list<QWidget *> widgets, const QString &text, const QStringList &types) {
    QStringList shown;
    for(const QString &t : types) shown << names.value(t, t);
    QString tip = text + "<br><br><b>" + tr("Sirve para:") + "</b> " + (types.isEmpty() ? tr("todos los widgets") : shown.join(", "));
    if(!types.isEmpty() && !types.contains(wtype))
      tip += "<br><i>" + tr("Este widget (%1) no lo usa.").arg(self) + "</i>";
    for(QWidget *w : widgets)   // la ayuda propia del campo (del .ui) va primero
      w->setToolTip("<p>" + (w->toolTip().isEmpty() ? QString() : w->toolTip().toHtmlEscaped() + "<br><br>") + tip + "</p>");
  };
  // pvbrowser: el clic lo mandan PushButton e Image (Image al apretar); presionar/soltar, los de pressable (Image solo soltar)
  const QStringList buttons = { "TQPushButton", "TQImage" };
  const QStringList pressable = { "TQPushButton", "TQLabel", "TQLineEdit", "TQComboBox", "TQSlider", "TQCheck", "TQRadio" };
  const QStringList input = { "TQLineEdit", "TQSlider", "TQDial", "TQSpinBox", "TQCheck", "TQRadio" };
  const QString kinds = tr("Escribir valor (el de la columna Valor), alternar 0/1, sumar o restar (con los limites de Opciones), "
                           "ir a la pantalla indicada, volver a la anterior o a la de inicio.");
  help({ ui->labelClick, ui->kindClick, ui->tagClick, ui->valueClick, ui->maskClick },
       tr("Que hace el widget cuando se lo pulsa y suelta (un clic completo).") + "<br>" + kinds, buttons);
  help({ ui->labelPress, ui->kindPress, ui->tagPress, ui->valuePress, ui->maskPress },
       tr("Que hace en el momento en que se aprieta el boton del mouse sobre el widget.") + "<br>" + kinds
       + "<br>" + tr("Con un Label se arma un pulsador sin usar un PushButton."), pressable);
  help({ ui->labelRelease, ui->kindRelease, ui->tagRelease, ui->valueRelease, ui->maskRelease },
       tr("Que hace al soltar el boton del mouse. Junto con Al presionar arma un pulsador momentaneo."), QStringList(pressable) << "TQImage");
  help({ ui->buttonMomentary },
       tr("Completa Al presionar = 1 y Al soltar = 0 en el tag de Al presionar (o el de Al hacer clic): "
          "el tag vale 1 solo mientras se mantiene apretado."), pressable);
  help({ ui->labelEnter, ui->enterTag },
       tr("Tag donde se escribe lo que el operador ingresa: el numero de un LineEdit (al apretar Enter), "
          "la posicion de un Slider, Dial o SpinBox, o 1/0 de un CheckBox o RadioButton."), input);
  help({ ui->labelLimits, ui->editMin, ui->editMax },
       tr("Minimo y maximo para Sumar, Restar y lo ingresado: el valor escrito nunca sale de ese rango. Vacio = sin limite."), {});
  help({ ui->labelConfirm, ui->editConfirm },
       tr("Pregunta que se muestra antes de la accion de Al hacer clic (o de lo ingresado); si el operador cancela, no se escribe nada. "
          "Vacio = sin confirmar."), QStringList(buttons) << input);
  help({ ui->labelCond, ui->condTag, ui->condOp, ui->condValue, ui->condMsg },
       tr("La accion se hace solo si se cumple la condicion (por ejemplo PLC1:Remoto == 1). Si no se cumple, se muestra el mensaje."), {});
  help({ ui->labelText, ui->textTag, ui->editFmt },
       tr("Muestra el valor del tag como texto. Formato: vacio (el numero), %d, %.1f bar, o textos por valor como Parado|Marcha|Falla."),
       { "TQLabel", "TQPushButton", "TQLineEdit", "TQCheck", "TQRadio" });
  help({ ui->labelValueAnim, ui->valueTag },
       tr("Mueve el widget segun el valor del tag: el largo de una barra, la posicion de un Slider o Dial, el numero de un LCD o SpinBox, "
          "o marca un CheckBox / RadioButton si es distinto de 0."),
       { "TQProgressBar", "TQSlider", "TQDial", "TQSpinBox", "TQLCDNumber", "TQCheck", "TQRadio" });
  help({ ui->labelColor, ui->colorTag, ui->editColors, ui->colorPart },
       tr("Cambia el color de fondo o del texto segun reglas valor=color separadas por ; (por ejemplo 0=#808080;1=#00C000 o >80=#FF0000;else=). Los colores van como #RRGGBB; vacio = el original."), {});
  help({ ui->labelBlink, ui->blinkTag },
       tr("Parpadea en rojo (o con los colores de Color) mientras se cumple la condicion."), {});
  help({ ui->labelVisible, ui->visibleTag, ui->checkInvert },
       tr("Muestra el widget solo mientras se cumple la condicion; Invertir lo oculta en ese caso."), {});
  help({ ui->checkQuality },
       tr("Si el daemon se detiene o el dato es malo, el widget se pinta de gris y el texto muestra --- en lugar de un valor viejo."), {});
  help({ ui->labelImages, ui->imageTag, ui->editImages },
       tr("Una imagen por cada valor del tag (0, 1, 2...), separadas por |."), { "TQImage" });
  help({ ui->labelAnim, ui->animTag, ui->editFrames, ui->spinPeriod },
       tr("Mientras se cumple la condicion, pvbrowser pasa los cuadros uno tras otro cada tantos milisegundos."), { "TQImage" });
  for(QComboBox *c : findChildren<QComboBox *>()) if(c->property("pvbTagCombo").toBool()) pvbAttachTagInfo(c);   // Nestor: datos del tag
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
  c->setProperty("pvbTagCombo", true);   // al final del constructor: pvbAttachTagInfo
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
