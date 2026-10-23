/****************************************************************************
** Nestor: Designer > Propiedades > Tags...: accion y animacion de un widget con
** los tags de los daemons (ver pvbindgen.h). Sin escribir codigo; si hace falta
** mas, el slot sigue disponible (antes o despues de la linea pvbActions).
****************************************************************************/
#ifndef DLGBIND_H
#define DLGBIND_H

#include <QDialog>
#include <QMap>
#include "pvbindgen.h"

class QComboBox;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class Ui_DlgBind;

class DlgBind : public QDialog
{
  Q_OBJECT
public:
  DlgBind(QWidget *widget, QWidget *parent = nullptr);
  ~DlgBind();
  QString binding() const;          // "clave=valor ..." para pvbSetBinding

public slots:
  void accept() override;           // avisa si el formato del texto no sirve (pvbValidTextFormat)

private:
  struct EventRow { QComboBox *kind; QComboBox *tag; QLineEdit *value; QSpinBox *mask; };
  void fillTags(QComboBox *c);
  void setupEventRow(EventRow *r, QComboBox *kind, QComboBox *tag, QLineEdit *value, QSpinBox *mask);
  void setEventRow(EventRow *r, const QString &spec);
  QString eventSpec(const EventRow &r) const;
  static void setTag(QComboBox *c, const QString &tag);
  static QString tagOf(QComboBox *c);

  Ui_DlgBind *ui;
  QList<PvbTag> tags;
  QString wtype;
  EventRow click, press, release;
  QComboBox *enterTag, *condTag, *condOp, *textTag, *valueTag, *colorTag, *colorPart, *blinkTag, *visibleTag;
  QLineEdit *editMin, *editMax, *editConfirm, *condValue, *condMsg, *editFmt, *editColors;
  QCheckBox *checkInvert, *checkQuality;
};

#endif
