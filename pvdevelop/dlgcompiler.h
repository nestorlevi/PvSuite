/****************************************************************************
** Nestor: ventana "Compilador": compilador (MSVC / MinGW), carpetas, cantidad
** de procesadores y texto predictivo del editor.
****************************************************************************/
#ifndef DLGCOMPILER_H
#define DLGCOMPILER_H

#include <QDialog>
#include "compilerconfig.h"

class QComboBox;
class QLineEdit;
class QLabel;
class QSpinBox;
class QCheckBox;

class DlgCompiler : public QDialog
{
  Q_OBJECT

public:
  explicit DlgCompiler(QWidget *parent = nullptr);
  CompilerConfig config() const;

private slots:
  void slotAutodetect();
  void slotValidate();

private:
  void setConfig(const CompilerConfig &c);
  QLineEdit *addPath(class QGridLayout *grid, int row, const QString &label, const QString &tip,
                     bool directory, QLabel **status);

  QComboBox *comboCompiler;
  QLineEdit *editVs, *editQtMsvc, *editMingw, *editQtMingw, *editJom;
  QLabel    *statusVs, *statusQtMsvc, *statusMingw, *statusQtMingw, *statusJom;
  QSpinBox  *spinJobs;
  QCheckBox *checkAutocomplete;
  CompilerConfig base; // configuracion leida al abrir la ventana
};

#endif
