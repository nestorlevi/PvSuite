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
** Nestor: pestanas de documentos de pvdevelop (ver doctabs.h)
****************************************************************************/
#include "doctabs.h"
#include <QTabBar>
#include <QToolButton>
#include <QHBoxLayout>
#include <QStyle>
#include <QFileInfo>
#include <QAbstractButton>
#include <QPainter>
#include <QStyleOption>
#include <QEvent>

namespace {
// boton de cerrar de cada pestana: con cambios sin guardar muestra un punto (como VS Code), que con el mouse
// encima vuelve a ser la X
class TabCloseButton : public QAbstractButton
{
public:
  explicit TabCloseButton(QWidget *parent) : QAbstractButton(parent)
  {
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
    setAttribute(Qt::WA_Hover);
    const int w = style()->pixelMetric(QStyle::PM_TabCloseIndicatorWidth, nullptr, this);
    const int h = style()->pixelMetric(QStyle::PM_TabCloseIndicatorHeight, nullptr, this);
    setFixedSize(qMax(w, 14), qMax(h, 14));
  }
  bool modified = false;
protected:
  bool event(QEvent *e) override
  {
    if(e->type() == QEvent::HoverEnter || e->type() == QEvent::HoverLeave) update();
    return QAbstractButton::event(e);
  }
  void paintEvent(QPaintEvent *) override
  {
    QPainter p(this);
    if(modified && !underMouse())
    {
      p.setRenderHint(QPainter::Antialiasing);
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(0xE6, 0x7E, 0x22));
      const qreal d = qMin(width(), height()) * 0.55;
      p.drawEllipse(QRectF((width() - d) / 2, (height() - d) / 2, d, d));
      return;
    }
    QStyleOption opt;
    opt.initFrom(this);
    opt.state |= QStyle::State_AutoRaise;
    if(isEnabled() && underMouse() && !isDown()) opt.state |= QStyle::State_Raised;
    if(isDown()) opt.state |= QStyle::State_Sunken;
    style()->drawPrimitive(QStyle::PE_IndicatorTabClose, &opt, &p, this);
  }
};
}

DocTabs::DocTabs(QWidget *parent) : QWidget(parent)
{
  QHBoxLayout *l = new QHBoxLayout(this);
  l->setContentsMargins(0, 0, 0, 0);
  l->setSpacing(4);
  bar = new QTabBar(this);
  bar->setTabsClosable(false);   // el boton de cerrar lo pone setTab (TabCloseButton)
  bar->setMovable(true);
  bar->setExpanding(false);
  bar->setDocumentMode(true);
  bar->setElideMode(Qt::ElideMiddle);
  bar->setUsesScrollButtons(true);
  l->addWidget(bar, 1);
  QToolButton *open = new QToolButton(this);
  open->setText("+");
  open->setToolTip(tr("Abrir otro archivo del proyecto en una pestaña nueva"));
  l->addWidget(open);
  saveAll = new QToolButton(this);
  saveAll->setText(tr("Guardar todo"));
  saveAll->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
  saveAll->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  l->addWidget(saveAll);
  updateSaveAll();
  connect(open, &QToolButton::clicked, this, &DocTabs::openFileRequested);
  connect(saveAll, &QToolButton::clicked, this, &DocTabs::saveAllRequested);
  connect(bar, &QTabBar::currentChanged, this, [this](int i) {
    if(!quiet && i >= 0) emit activate(bar->tabData(i).toString());
  });
}

int DocTabs::indexOf(const QString &key) const
{
  for(int i=0; i<bar->count(); i++) if(bar->tabData(i).toString() == key) return i;
  return -1;
}

QStringList DocTabs::keys() const
{
  QStringList r;
  for(int i=0; i<bar->count(); i++) r << bar->tabData(i).toString();
  return r;
}

void DocTabs::setTab(int index, const QString &key)
{
  bar->setTabData(index, key);
  const QString t = title ? title(key) : QFileInfo(fileOf(key)).fileName();
  const bool on = marked.contains(key);
  bar->setTabText(index, t);
  const QString tip = isDesigner(key) ? tr("Designer: %1").arg(fileOf(key)) : fileOf(key);
  bar->setTabToolTip(index, on ? tip + tr("\n(sin guardar)") : tip);
  bar->setTabIcon(index, style()->standardIcon(isDesigner(key) ? QStyle::SP_DesktopIcon : QStyle::SP_FileIcon));
  const QTabBar::ButtonPosition side = (QTabBar::ButtonPosition) style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, bar);
  TabCloseButton *b = dynamic_cast<TabCloseButton *>(bar->tabButton(index, side));
  if(b == nullptr)
  {
    b = new TabCloseButton(bar);
    connect(b, &QAbstractButton::clicked, this, [this, b, side]() {
      for(int i=0; i<bar->count(); i++) if(bar->tabButton(i, side) == b) { emit closeRequested(bar->tabData(i).toString()); return; }
    });
    bar->setTabButton(index, side, b);
  }
  b->modified = on;
  b->setToolTip(on ? tr("Cerrar (tiene cambios sin guardar)") : tr("Cerrar"));
  b->update();
}

void DocTabs::updateSaveAll()
{
  saveAll->setEnabled(!marked.isEmpty());
  saveAll->setToolTip(marked.isEmpty() ? tr("No hay cambios sin guardar")
                    : marked.size() == 1 ? tr("Guarda el documento sin guardar (%1)").arg(title ? title(marked.first()) : fileOf(marked.first()))
                    : tr("Guarda los %1 documentos sin guardar").arg(marked.size()));
}

void DocTabs::shown(const QString &key)
{
  quiet = true;
  int i = indexOf(key);
  if(i < 0)
  {
    i = bar->currentIndex();
    if(i < 0) i = bar->addTab(QString());
    marked.removeAll(bar->tabData(i).toString());
    setTab(i, key);
    updateSaveAll();
  }
  bar->setCurrentIndex(i);
  quiet = false;
}

void DocTabs::openNew(const QString &key)
{
  int i = indexOf(key);
  if(i < 0)
  {
    quiet = true;
    i = bar->insertTab(bar->currentIndex() + 1, QString());
    setTab(i, key);
    quiet = false;
  }
  if(i == bar->currentIndex()) return;
  bar->setCurrentIndex(i);   // emite activate
}

void DocTabs::remove(const QString &key)
{
  const int i = indexOf(key);
  if(i < 0) return;
  quiet = true;
  marked.removeAll(key);
  bar->removeTab(i);
  quiet = false;
  updateSaveAll();
}

void DocTabs::clear()
{
  quiet = true;
  while(bar->count() > 0) bar->removeTab(0);
  marked.clear();
  quiet = false;
  updateSaveAll();
}

QString DocTabs::neighbor(const QString &key) const
{
  const int i = indexOf(key);
  if(i < 0 || bar->count() < 2) return QString();
  return bar->tabData(i + 1 < bar->count() ? i + 1 : i - 1).toString();
}

void DocTabs::setModifiedMark(const QString &key, bool on)
{
  if(marked.contains(key) == on) return;
  marked.removeAll(key);
  if(on) marked << key;
  const int i = indexOf(key);
  if(i >= 0) setTab(i, key);
  updateSaveAll();
}

void DocTabs::refreshTitles()
{
  for(int i=0; i<bar->count(); i++) setTab(i, bar->tabData(i).toString());
}
