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
**
** Nestor: Ayuda > Buscar actualizaciones (pvdevelop y pvbrowser).
** Consulta la ultima version publicada en GitHub Releases del repositorio de PV Suite
** y, si es mas nueva que la instalada, ofrece bajar el instalador.
** La direccion se puede cambiar sin recompilar: QSettings("pvbrowser", <programa>) "updates/url".
**
****************************************************************************/
#ifndef PVB_UPDATE_H
#define PVB_UPDATE_H

class QWidget;
class QMenu;

// version de PV Suite (la misma que <Version> de Compilar/installer/config/config.xml)
#define PVSUITE_VERSION "1.0.0-beta.3"
// ultima version publicada (GitHub Releases)
#define PVSUITE_UPDATE_URL "https://api.github.com/repos/nestorlevi/PvSuite/releases/latest"

//! Consulta si hay una version nueva. silent: solo avisa si la hay (sin errores ni "esta al dia")
void pvbCheckForUpdates(QWidget *parent, const char *app, bool silent = false);
//! Agrega "Buscar actualizaciones..." y "Buscar al iniciar" al menu; si corresponde (una vez por
//! semana, si esta activado) busca en segundo plano unos segundos despues de iniciar
void pvbAddUpdateActions(QMenu *menu, QWidget *parent, const char *app, bool autoDefault);
//! -1 si a < b, 0 si son iguales, 1 si a > b ("1.0.0-beta.2" < "1.0.1" < "v1.1.0")
int  pvbCompareVersions(const char *a, const char *b);

#endif
