/****************************************************************************
** Nestor: imagenes del proyecto (Propiedades de un QImage y Tags... > Imagen).
**
** Se eligen de cualquier carpeta; las que estan fuera del proyecto se pueden copiar a la
** carpeta Image del proyecto (se pregunta).
****************************************************************************/
#ifndef PVIMAGES_H
#define PVIMAGES_H

#include <QString>
#include <QStringList>
class QWidget;

QString     pvbProjectName();                          // el .pvproject de la carpeta actual

// Elegir imagenes de cualquier carpeta. Devuelve las rutas como se usan en el proyecto
// (relativas: Image/motor.png; absolutas si no se copiaron). Vacio = se cancelo.
QStringList pvbPickImages(QWidget *parent, const QString &current, bool multiple);
// Una imagen ya elegida: si esta fuera del proyecto pregunta si se copia a Image/.
// askAll: para varias seguidas (0 = preguntar, 1 = copiar todas, 2 = ninguna)
QString     pvbImportImage(QWidget *parent, const QString &file, int *askAll = nullptr);
// Agrega un boton "..." al campo de texto para elegir imagenes (multiple: separadas por |)
void        pvbAddImageButton(class QLineEdit *edit, bool multiple);

#endif
