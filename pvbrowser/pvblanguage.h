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
** Nestor: idioma de pvbrowser y pvdevelop (espanol, ingles, portugues, aleman).
**
** El idioma se elige en el menu Idioma (se guarda con QSettings) o, si no se eligio,
** es el del sistema; si no hay traduccion para el, ingles. Las traducciones (.qm) van
** dentro del ejecutable (qmake: CONFIG += lrelease embed_translations, en :/i18n/).
** Tambien se cargan las traducciones de Qt (dialogos estandar) si estan instaladas.
** Los textos se editan con Qt Linguist en pvbrowser/translations y pvdevelop/translations.
**
****************************************************************************/
#ifndef PVB_LANGUAGE_H
#define PVB_LANGUAGE_H

#include <QString>
#include <QStringList>

class QApplication;
class QMenu;
class QWidget;

// codigos disponibles: "es", "en", "pt", "de"
QStringList pvbLanguages();
// nombre del idioma en ese idioma ("Español", "English", ...)
QString     pvbLanguageName(const QString &code);
// idioma guardado para la aplicacion ("" = el del sistema)
QString     pvbSavedLanguage(const char *app);
// idioma que se usa: el guardado o el del sistema (o "en")
QString     pvbLanguage(const char *app);
// carga las traducciones de la aplicacion (":/i18n/<app>_<codigo>.qm") y las de Qt
void        pvbInstallTranslators(QApplication &qapp, const char *app);
// menu "Idioma" (Sistema, Español, English, Português, Deutsch); al elegir se guarda y
// se avisa que hay que reiniciar la aplicacion
QMenu      *pvbLanguageMenu(QWidget *parent, const char *app);

#endif
