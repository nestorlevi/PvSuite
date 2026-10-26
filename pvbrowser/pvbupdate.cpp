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
** Nestor: Ayuda > Buscar actualizaciones (ver pvbupdate.h)
**
****************************************************************************/
#include "pvbupdate.h"
#include <QAction>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QUrl>


// "v1.0.2f" -> partes {1, 0, 2, "f"}: los numeros se comparan como numeros y lo que sigue como texto
int pvbCompareVersions(const char *a0, const char *b0)
{
  QString a = QString::fromUtf8(a0).trimmed(), b = QString::fromUtf8(b0).trimmed();
  a.remove(QRegularExpression("^[vV]"));
  b.remove(QRegularExpression("^[vV]"));
  QStringList pa = a.split(QRegularExpression("[.\\-_ ]")), pb = b.split(QRegularExpression("[.\\-_ ]"));
  QRegularExpression part("^(\\d*)(.*)$");
  for(int i=0; i<qMax(pa.size(), pb.size()); i++)
  {
    QRegularExpressionMatch ma = part.match(i < pa.size() ? pa[i] : QString());
    QRegularExpressionMatch mb = part.match(i < pb.size() ? pb[i] : QString());
    long long na = ma.captured(1).isEmpty() ? -1 : ma.captured(1).toLongLong();
    long long nb = mb.captured(1).isEmpty() ? -1 : mb.captured(1).toLongLong();
    if(na != nb) return (na < nb) ? -1 : 1;
    int c = QString::compare(ma.captured(2), mb.captured(2), Qt::CaseInsensitive);
    if(c != 0) return (c < 0) ? -1 : 1;
  }
  return 0;
}

static QString settingsUrl(const char *app)
{
  QSettings s("pvbrowser", app);
  QString url = s.value("updates/url").toString().trimmed();
  return url.isEmpty() ? QString(PVSUITE_UPDATE_URL) : url;
}

// instalador de la plataforma entre los archivos de la version publicada
static QString installerAsset(const QJsonArray &assets)
{
  QString best;
  for(const QJsonValue &v : assets)
  {
    QString name = v.toObject().value("name").toString();
    QString url  = v.toObject().value("browser_download_url").toString();
#if defined(Q_OS_WIN)
    if(name.endsWith("-Setup-win64.exe", Qt::CaseInsensitive)) return url;   // el online (liviano)
    if(name.endsWith(".exe", Qt::CaseInsensitive) && name.contains("Setup", Qt::CaseInsensitive)) best = url;
#elif defined(Q_OS_MACOS)
    if(name.endsWith(".dmg", Qt::CaseInsensitive)) return url;
#else
    if(name.endsWith(".run", Qt::CaseInsensitive)) return url;
#endif
  }
  return best;
}

void pvbCheckForUpdates(QWidget *parent, const char *app, bool silent)
{
  QNetworkAccessManager nam;
  QNetworkRequest req(QUrl(settingsUrl(app)));
  req.setRawHeader("Accept", "application/vnd.github+json");
  req.setRawHeader("User-Agent", "PVSuite/" PVSUITE_VERSION);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = nam.get(req);
  QEventLoop loop;
  QTimer timer;
  timer.setSingleShot(true);
  QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  timer.start(15000);
  loop.exec();
  const QString title = QCoreApplication::translate("pvbUpdate", "PV Suite: actualizaciones");

  if(!reply->isFinished())
  {
    reply->abort();
    reply->deleteLater();
    if(!silent) QMessageBox::warning(parent, title, QCoreApplication::translate("pvbUpdate", "No hubo respuesta del servidor de actualizaciones."));
    return;
  }
  int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  QByteArray data = reply->readAll();
  QString error = reply->errorString();
  bool failed = (reply->error() != QNetworkReply::NoError);
  reply->deleteLater();
  if(status == 404)
  {
    if(!silent) QMessageBox::information(parent, title, QCoreApplication::translate("pvbUpdate", "Todavia no hay versiones publicadas."));
    return;
  }
  if(failed)
  {
    if(!silent) QMessageBox::warning(parent, title, QCoreApplication::translate("pvbUpdate", "No se pudo consultar si hay actualizaciones:\n%1").arg(error));
    return;
  }

  QJsonObject o = QJsonDocument::fromJson(data).object();
  QString tag  = o.value("tag_name").toString();
  QString page = o.value("html_url").toString();
  QString notes = o.value("body").toString();
  QString download = installerAsset(o.value("assets").toArray());
  if(tag.isEmpty())
  {
    if(!silent) QMessageBox::warning(parent, title, QCoreApplication::translate("pvbUpdate", "La respuesta del servidor de actualizaciones no es valida."));
    return;
  }
  QSettings s("pvbrowser", app);
  if(pvbCompareVersions(PVSUITE_VERSION, tag.toUtf8().constData()) >= 0)
  {
    if(!silent) QMessageBox::information(parent, title, QCoreApplication::translate("pvbUpdate", "PV Suite esta al dia (version %1).").arg(PVSUITE_VERSION));
    return;
  }
  if(silent && s.value("updates/skip").toString() == tag) return;   // el usuario la omitio

  QMessageBox box(QMessageBox::Information, title,
                  QCoreApplication::translate("pvbUpdate", "Hay una version nueva de PV Suite: %1 (instalada: %2).").arg(tag, PVSUITE_VERSION), QMessageBox::NoButton, parent);
  if(!notes.isEmpty()) box.setDetailedText(notes);
  QPushButton *bDownload = download.isEmpty() ? nullptr : box.addButton(QCoreApplication::translate("pvbUpdate", "Descargar el instalador"), QMessageBox::AcceptRole);
  QPushButton *bPage = box.addButton(QCoreApplication::translate("pvbUpdate", "Ver la pagina"), QMessageBox::ActionRole);
  QPushButton *bSkip = silent ? box.addButton(QCoreApplication::translate("pvbUpdate", "Omitir esta version"), QMessageBox::DestructiveRole) : nullptr;
  box.addButton(QCoreApplication::translate("pvbUpdate", "Mas tarde"), QMessageBox::RejectRole);
  box.exec();
  if(bDownload && box.clickedButton() == bDownload) QDesktopServices::openUrl(QUrl(download));
  else if(box.clickedButton() == bPage)             QDesktopServices::openUrl(QUrl(page));
  else if(bSkip && box.clickedButton() == bSkip)    s.setValue("updates/skip", tag);
}

void pvbAddUpdateActions(QMenu *menu, QWidget *parent, const char *app, bool autoDefault)
{
  QByteArray appName(app);
  QAction *check = menu->addAction(QCoreApplication::translate("pvbUpdate", "Buscar &actualizaciones..."));
  QObject::connect(check, &QAction::triggered, parent, [parent, appName]() { pvbCheckForUpdates(parent, appName.constData(), false); });
  QAction *autoCheck = menu->addAction(QCoreApplication::translate("pvbUpdate", "Buscar actualizaciones al iniciar"));
  autoCheck->setCheckable(true);
  QSettings s("pvbrowser", app);
  autoCheck->setChecked(s.value("updates/auto", autoDefault).toBool());
  QObject::connect(autoCheck, &QAction::toggled, parent, [appName](bool on) { QSettings("pvbrowser", appName.constData()).setValue("updates/auto", on); });

  // una vez por semana, en segundo plano, unos segundos despues de abrir
  if(!autoCheck->isChecked()) return;
  qint64 last = s.value("updates/last", 0).toLongLong(), now = QDateTime::currentSecsSinceEpoch();
  if(now - last < 7 * 24 * 3600) return;
  s.setValue("updates/last", now);
  QPointer<QWidget> p(parent);
  QTimer::singleShot(5000, parent, [p, appName]() { if(p) pvbCheckForUpdates(p, appName.constData(), true); });
}
