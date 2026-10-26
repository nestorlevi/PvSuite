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
** Nestor: idioma de pvbrowser y pvdevelop (ver pvblanguage.h)
**
****************************************************************************/
#include "pvblanguage.h"
#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QSettings>
#include <QLibraryInfo>
#include <QDir>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QMessageBox>

QStringList pvbLanguages()
{
  return QStringList() << "es" << "en" << "pt" << "de";
}

QString pvbLanguageName(const QString &code)
{
  if(code == "es") return QString::fromUtf8("Español");
  if(code == "en") return "English";
  if(code == "pt") return QString::fromUtf8("Português");
  if(code == "de") return "Deutsch";
  return code;
}

QString pvbSavedLanguage(const char *app)
{
  QSettings s("pvbrowser", app);
  return s.value("ui/language").toString();
}

QString pvbLanguage(const char *app)
{
  QString code = pvbSavedLanguage(app);
  if(code.isEmpty()) code = QLocale::system().name().left(2).toLower(); // "es_AR" -> "es"
  if(!pvbLanguages().contains(code)) code = "en";
  return code;
}

// traduccion de Qt (dialogos estandar): en la instalacion de Qt o junto al ejecutable
static void loadQtTranslation(QApplication &qapp, const QString &name, const QString &code)
{
  QStringList dirs;
  dirs << QCoreApplication::applicationDirPath() + "/translations"
       << QCoreApplication::applicationDirPath() + "/../translations"
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
       << QLibraryInfo::path(QLibraryInfo::TranslationsPath)
#else
       << QLibraryInfo::location(QLibraryInfo::TranslationsPath)
#endif
       << "/opt/pvb/translations";
  QStringList files;
  files << name + "_" + code;
  if(code == "pt") files << name + "_pt_BR" << name + "_pt_PT";
  for(const QString &f : files)
  {
    for(const QString &d : dirs)
    {
      QTranslator *t = new QTranslator(&qapp);
      if(t->load(f, d)) { qapp.installTranslator(t); return; }
      delete t;
    }
  }
}

void pvbInstallTranslators(QApplication &qapp, const char *app)
{
  QString code = pvbLanguage(app);
  QLocale::setDefault(QLocale(code));
  QTranslator *t = new QTranslator(&qapp);
  if(t->load(QString(":/i18n/%1_%2").arg(app).arg(code))) qapp.installTranslator(t);
  else delete t;
  if(code != "en")
  {
    loadQtTranslation(qapp, "qtbase", code);
    loadQtTranslation(qapp, "qt", code);
    loadQtTranslation(qapp, "qtwebengine", code);
  }
}

QMenu *pvbLanguageMenu(QWidget *parent, const char *app)
{
  QMenu *menu = new QMenu(QCoreApplication::translate("pvbLanguage", "&Language"), parent);
  QActionGroup *group = new QActionGroup(menu);
  QString saved = pvbSavedLanguage(app);
  QStringList codes;
  codes << "" << pvbLanguages();
  for(const QString &code : codes)
  {
    QString text = code.isEmpty() ? QCoreApplication::translate("pvbLanguage", "System language") : pvbLanguageName(code);
    QAction *a = menu->addAction(text);
    a->setCheckable(true);
    a->setChecked(code == saved);
    a->setData(code);
    group->addAction(a);
    if(code.isEmpty()) menu->addSeparator();
  }
  QString appName = app;
  QObject::connect(group, &QActionGroup::triggered, parent, [parent, appName](QAction *a) {
    QString code = a->data().toString();
    QSettings s("pvbrowser", appName);
    if(s.value("ui/language").toString() == code) return;
    s.setValue("ui/language", code);
    QMessageBox::information(parent, appName,
      QCoreApplication::translate("pvbLanguage", "The language will change the next time %1 is started.").arg(appName));
  });
  return menu;
}
