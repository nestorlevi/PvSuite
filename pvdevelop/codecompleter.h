/****************************************************************************
** Nestor: texto predictivo para el editor de pvdevelop.
** Sugiere funciones de pvserver (con su firma), clases de rllib, palabras
** clave de C/C++ y las palabras del archivo abierto.
** Aparece al escribir 3 letras de una palabra o con Ctrl+Espacio.
****************************************************************************/
#ifndef CODECOMPLETER_H
#define CODECOMPLETER_H

#include <QObject>
#include <QString>
#include <QTextEdit>

class QCompleter;
class QStandardItemModel;
class CodeCompleter;

// Editor de pvdevelop: con la lista de sugerencias abierta, Enter/Tab/Esc son para la lista.
// (QCompleter primero le pasa la tecla al editor con event(); si el editor la usa, la lista no la recibe)
class CodeTextEdit : public QTextEdit
{
  Q_OBJECT

public:
  explicit CodeTextEdit(QWidget *parent = nullptr) : QTextEdit(parent) {}
  CodeCompleter *completer = nullptr;

protected:
  void keyPressEvent(QKeyEvent *event) override;
};

class CodeCompleter : public QObject
{
  Q_OBJECT

public:
  CodeCompleter(QTextEdit *edit);
  void setEnabled(bool enabled);
  bool popupVisible() const;

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void insertCompletion(const QString &completion);
  void perhapsShow(bool force = false);
  void slotDocumentChanged();
  void slotContentsChange(int position, int removed, int added);

private:
  QString wordUnderCursor() const;
  void updateModel();

  QTextEdit *edit;
  QCompleter *completer;
  QStandardItemModel *model;
  bool enabled;
  bool documentChanged;
};

#endif
