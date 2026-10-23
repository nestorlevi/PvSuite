/****************************************************************************
** Nestor: imagenes del proyecto (ver pvimages.h)
****************************************************************************/
#include "pvimages.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QCoreApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QAction>
#include <QStyle>

QString pvbProjectName()
{
  QStringList p = QDir::current().entryList(QStringList() << "*.pvproject", QDir::Files, QDir::Name);
  return p.isEmpty() ? QString() : QFileInfo(p.first()).completeBaseName();
}

//------------------------------------------------------------------------------------------------
// imagenes de cualquier carpeta (se copian a Image/ si se quiere)

static const char *IMAGE_DIR = "Image";

QString pvbImportImage(QWidget *parent, const QString &file0, int *askAll)
{
  QString file = QDir::fromNativeSeparators(file0.trimmed());
  if(file.isEmpty()) return file;
  QString rel = QDir::current().relativeFilePath(file);
  if(!rel.startsWith("..") && !QDir::isAbsolutePath(rel)) return rel;   // ya esta en el proyecto
  int answer = askAll ? *askAll : 0;
  if(answer == 0)
  {
    QMessageBox::StandardButtons b = QMessageBox::Yes | QMessageBox::No;
    if(askAll) b |= QMessageBox::YesToAll | QMessageBox::NoToAll;
    int r = QMessageBox::question(parent, QCoreApplication::translate("DlgResources", "Imagen fuera del proyecto"),
              QCoreApplication::translate("DlgResources", "%1\n\nesta fuera de la carpeta del proyecto. Copiarla a la carpeta %2 del proyecto?\n\n"
                                                          "Si no se copia, el pvserver la busca en esa ruta (solo sirve en esta PC).")
                .arg(QDir::toNativeSeparators(file), IMAGE_DIR), b, QMessageBox::Yes);
    if(r == QMessageBox::YesToAll) { answer = 1; if(askAll) *askAll = 1; }
    else if(r == QMessageBox::NoToAll) { answer = 2; if(askAll) *askAll = 2; }
    else answer = (r == QMessageBox::Yes) ? 1 : 2;
  }
  if(answer != 1) return file;
  QDir().mkpath(IMAGE_DIR);
  QFileInfo fi(file);
  QString target = QString("%1/%2").arg(IMAGE_DIR, fi.fileName());
  for(int n=2; QFile::exists(target); n++)   // mismo nombre: si es el mismo archivo se usa, si no motor_2.png
  {
    QFile a(file), b(target);
    if(a.open(QIODevice::ReadOnly) && b.open(QIODevice::ReadOnly) && a.readAll() == b.readAll()) return target;
    target = QString("%1/%2_%3.%4").arg(IMAGE_DIR, fi.completeBaseName()).arg(n).arg(fi.suffix());
  }
  if(!QFile::copy(file, target))
  {
    QMessageBox::warning(parent, "pvdevelop", QCoreApplication::translate("DlgResources", "No se pudo copiar %1").arg(file));
    return file;
  }
  return target;
}

QStringList pvbPickImages(QWidget *parent, const QString &current, bool multiple)
{
  QString first = current.section('|', 0, 0).trimmed();
  QString start = first.isEmpty() ? QDir::current().absoluteFilePath(QDir(IMAGE_DIR).exists() ? IMAGE_DIR : ".")
                                  : QFileInfo(QDir::current().absoluteFilePath(first)).absolutePath();
  QString filter = QCoreApplication::translate("DlgResources", "Imagenes") +
                   " (*.png *.jpg *.jpeg *.bmp *.gif *.svg *.ico *.xpm);;" + QCoreApplication::translate("DlgResources", "Todos") + " (*)";
  QStringList files;
  if(multiple) files = QFileDialog::getOpenFileNames(parent, QCoreApplication::translate("DlgResources", "Elegir imagenes"), start, filter);
  else
  {
    QString f = QFileDialog::getOpenFileName(parent, QCoreApplication::translate("DlgResources", "Elegir imagen"), start, filter);
    if(!f.isEmpty()) files << f;
  }
  QStringList out;
  int askAll = 0;
  for(const QString &f : files) out << pvbImportImage(parent, f, files.size() > 1 ? &askAll : nullptr);
  return out;
}

void pvbAddImageButton(QLineEdit *edit, bool multiple)
{
  QAction *a = edit->addAction(edit->style()->standardIcon(QStyle::SP_DirOpenIcon), QLineEdit::TrailingPosition);
  a->setToolTip(multiple ? QCoreApplication::translate("DlgResources", "Elegir imagenes (se agregan separadas por |)")
                         : QCoreApplication::translate("DlgResources", "Elegir imagen"));
  QObject::connect(a, &QAction::triggered, edit, [edit, multiple]() {
    QStringList files = pvbPickImages(edit->window(), edit->text(), multiple);
    if(files.isEmpty()) return;
    if(multiple && !edit->text().trimmed().isEmpty()) edit->setText(edit->text().trimmed() + "|" + files.join('|'));
    else edit->setText(files.join('|'));
  });
}
