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
