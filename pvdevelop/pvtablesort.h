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
** Nestor: ordenar una tabla con clic en el titulo de una columna (cualquier columna).
**
** Ordena solo lo que se ve: numeros como numeros, textos sin distinguir mayusculas y con los
** numeros de adentro en orden (Tag2 antes que Tag10); celdas con un widget (combo, casilla,
** spin) por su valor. Segundo clic en la misma columna: al reves.
** No usa setSortingEnabled: las filas no se mueven mientras se editan ni al agregarlas.
**
** El orden de los archivos no cambia al ordenar: pvbTableOrder() da las filas en el orden de
** antes del primer clic (las agregadas despues van al final). Subir/Bajar: pvbTableAcceptOrder()
** antes de mover, asi el orden que se ve pasa a ser el del archivo.
****************************************************************************/
#ifndef PVTABLESORT_H
#define PVTABLESORT_H

#include <QList>

class QTableWidget;

void pvbSortableTable(QTableWidget *t);
QList<int> pvbTableOrder(QTableWidget *t);    // filas (indices visibles) en el orden del archivo
void pvbTableAcceptOrder(QTableWidget *t);    // el orden que se ve pasa a ser el del archivo
void pvbTableResort(QTableWidget *t);         // tras volver a llenar la tabla: el ultimo orden elegido
void pvbTableSortBy(QTableWidget *t, int column, bool ascending);   // como un clic (pruebas)

#endif
