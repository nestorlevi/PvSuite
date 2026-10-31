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
** Nestor: datos de un tag (direccion, tipo, comentario, unidad, alarma) donde se lo usa:
** cartelito en el editor (mouse sobre PLC1_Bomba1.Velocidad), barra de estado con el cursor de texto,
** Ctrl+clic = abrirlo en el editor de tags, y el cartelito de los campos de tag de los dialogos
** (PLC1:Bomba1.Velocidad, con aviso si no existe). Los datos salen de los .tagmap del proyecto.
****************************************************************************/
#ifndef PVTAGINFO_H
#define PVTAGINFO_H

#include <QString>
#include <QList>
#include <QPoint>
#include "pvbindgen.h"
class QComboBox;
class QWidget;

// los tags del proyecto (pvbProjectTags), leidos de nuevo solo si cambio algun .tagmap
const QList<PvbTag> &pvbTagsCached();
// "PLC1:Bomba1.Velocidad", "PLC1_Bomba1.Velocidad", "Bomba1.Velocidad" (using namespace tags_PLC1), "PLC1:Estado.3"
// (bit de un entero); un indice que no es un numero (PLC1_Temp[i]) toma el primer elemento. false = no es un tag
bool pvbLookupTag(const QString &text, PvbTag *t);
// "PLC1 (Modbus) · 40502 · real · lectura/escritura" y, si los tiene, "comentario · unidad · alarma >80 (texto)"
// html: para un cartelito (con el nombre en negrita, en dos lineas); si no, en una linea
QString pvbTagSummary(const PvbTag &t, bool html);
// el nombre (con puntos e indices) que hay en la posicion pos de la linea; vacio si no hay
QString pvbTagWordAt(const QString &line, int pos);
// el tag que hay en la posicion pos de la linea (el nombre entero o, si no es un tag, hasta donde esta pos)
bool pvbTagAt(const QString &line, int pos, PvbTag *t);
// campo de tag de un dialogo: el cartelito del campo suma los datos del tag escrito (o "no existe", en rojo) y cada
// tag de la lista lleva los suyos. Los $M (bloques) y @user... no se marcan como error
void pvbAttachTagInfo(QComboBox *combo);
// que vuelva a leer los .tagmap ya (se acaba de generar uno)
void pvbTagsInvalidate();

// Nestor: tag por su direccion (%MW1, 40002, HR1, DB1.DBW0...), como la entiende cada daemon (address_notation)
struct PvbDaemonFile { QString file, source; int protocol = 0; };   // protocol: rlTagList::MODBUS...
QList<PvbDaemonFile> pvbDaemonFiles();                 // .mkmodbus y .mksiemens del proyecto
bool pvbLooksLikeAddress(const QString &text);         // %MW1, 40002, HR1.3, DB1.DBW0, MW10 (no un nombre)
// los tags en esa direccion; canCreate: los daemons (.mkmodbus, .mksiemens) que entienden la direccion
QList<PvbTag> pvbTagsAtAddress(const QString &text, QStringList *canCreate = nullptr);
// campo de tag con una direccion: Enter la cambia por el tag (si hay varios, se elige; si no hay, se ofrece crearlo).
// Al salir del campo se cambia sola si hay uno solo. code: en el editor (PLC1_Brillo en vez de PLC1:Brillo)
// Devuelve el texto que va (vacio = se cancelo o no era una direccion)
QString pvbResolveAddress(QWidget *parent, const QString &text, const QPoint &globalPos, bool code);

#endif
