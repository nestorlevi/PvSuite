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
** Nestor: ayuda integrada de pvdevelop. Ver dlghelp.h.
****************************************************************************/
#include "dlghelp.h"
#include "ui_dlghelp.h"
#include "../pvbrowser/pvblanguage.h"
#include <QApplication>
#include <QBoxLayout>
#include <QDialogButtonBox>
#include <QAbstractButton>
#include <QLabel>
#include <QScreen>
#include <QScrollArea>
#include <QDesktopServices>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QListWidget>
#include <QRegularExpression>
#include <QShortcut>
#include <QStyle>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTreeWidget>
#include <QUrl>
#include <QWhatsThis>
#include <algorithm>
#include <cstring>

// minusculas y sin acentos: la busqueda no distingue ni una cosa ni la otra
static QString pvbFold(const QString &s)
{
  const QString d = s.normalized(QString::NormalizationForm_D).toLower();
  QString r;
  r.reserve(d.size());
  for(const QChar c : d)
    if(c.category() != QChar::Mark_NonSpacing) r += c;
  return r;
}

// carpetas doc posibles, en orden: la configurada, PVBDIR, SUITEDIR, junto al ejecutable y hacia arriba
// (instalacion: <destino>/win-mingw/bin -> <destino>/doc; desarrollo: la raiz del repositorio), el paquete de macOS y /opt/pvb
static QStringList pvbDocCandidates(const QString &configured)
{
  QStringList r;
  if(!configured.isEmpty()) r << QDir::cleanPath(configured);
  for(const char *v : {"PVBDIR", "SUITEDIR"})
  {
    const QString e = qEnvironmentVariable(v);
    if(!e.isEmpty()) r << QDir::cleanPath(e + "/doc");
  }
  QDir d(QCoreApplication::applicationDirPath());
  r << QDir::cleanPath(d.absoluteFilePath("../Resources/doc"));   // macOS: Contents/MacOS -> Contents/Resources
  for(int i=0; i<7; i++)
  {
    r << QDir::cleanPath(d.absoluteFilePath("doc"));
    if(!d.cdUp()) break;
  }
  r << "/opt/pvb/doc";
  r.removeDuplicates();
  return r;
}

QString DlgHelp::findDocDir(const QString &configured, const QString &language)
{
  for(const QString &c : pvbDocCandidates(configured))
    if(available(c, language)) return c;
  return QString();
}

QString DlgHelp::findManualDir(const QString &docDir)
{
  for(const QString &c : pvbDocCandidates(docDir))
  {
    const QDir m(c + "/manual");
    if(m.exists() && !m.entryList(QStringList{"Manual_PvSuite*.pdf", "Manual_PvSuite*.html"}, QDir::Files).isEmpty()) return m.absolutePath();
  }
  return QString();
}

DlgHelp::DlgHelp(const QString &docDirectory, QWidget *parent, const QString &language) : QDialog(parent), docDir(docDirectory), lang(language)
{
  ui = new Ui_DlgHelp;
  ui->setupUi(this);
  setWindowFlag(Qt::WindowMinMaxButtonsHint, true);
  setWindowTitle(tr("Ayuda de PV Suite"));
  ui->buttonBack->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
  ui->buttonForward->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
  ui->buttonHome->setIcon(style()->standardIcon(QStyle::SP_DirHomeIcon));
  ui->buttonBack->setEnabled(false);
  ui->buttonForward->setEnabled(false);
  ui->splitter->setStretchFactor(0, 0);
  ui->splitter->setStretchFactor(1, 1);
  ui->splitter->setSizes(QList<int>{280, 720});
  ui->stackLeft->setCurrentIndex(0);
  ui->verticalLayout->setStretch(1, 1);   // el arbol y el texto se llevan todo el alto
  applyStyle();
  load();
  buildTree();

  connect(ui->buttonBack, &QToolButton::clicked, ui->browser, &QTextBrowser::backward);
  connect(ui->buttonForward, &QToolButton::clicked, ui->browser, &QTextBrowser::forward);
  connect(ui->browser, &QTextBrowser::backwardAvailable, ui->buttonBack, &QToolButton::setEnabled);
  connect(ui->browser, &QTextBrowser::forwardAvailable, ui->buttonForward, &QToolButton::setEnabled);
  connect(ui->buttonHome, &QToolButton::clicked, this, &DlgHelp::slotHome);
  connect(ui->buttonPdf, &QToolButton::clicked, this, &DlgHelp::slotPdf);
  connect(ui->buttonFolder, &QToolButton::clicked, this, &DlgHelp::slotFolder);
  ui->buttonPdf->setEnabled(!pdfFile().isEmpty());
  connect(ui->lineSearch, &QLineEdit::textChanged, this, &DlgHelp::slotSearchChanged);
  connect(ui->listResults, &QListWidget::itemClicked, this, &DlgHelp::slotResult);
  connect(ui->listResults, &QListWidget::itemActivated, this, &DlgHelp::slotResult);
  connect(ui->treeIndex, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *cur, QTreeWidgetItem *) { slotTree(cur); });
  connect(ui->browser, &QTextBrowser::anchorClicked, this, &DlgHelp::slotAnchor);
  connect(ui->browser, &QTextBrowser::sourceChanged, this, &DlgHelp::slotSource);
  QShortcut *find = new QShortcut(QKeySequence::Find, this);
  connect(find, &QShortcut::activated, this, [this]() { ui->lineSearch->setFocus(); ui->lineSearch->selectAll(); });
  slotHome();
}

DlgHelp::~DlgHelp()
{
  delete ui;
}

bool DlgHelp::available(const QString &docDir, const QString &language)
{
  const QString l = language.isEmpty() ? pvbLanguage("pvdevelop") : language;
  for(const QString &c : QStringList{l, "en", "es"})
    if(QFileInfo::exists(docDir + "/ayuda/" + c + "/indice.tsv")) return true;
  return false;
}

void DlgHelp::load()
{
  if(lang.isEmpty()) lang = pvbLanguage("pvdevelop");
  for(const QString &c : QStringList{lang, "en", "es"})
  {
    if(QFileInfo::exists(docDir + "/ayuda/" + c + "/indice.tsv")) { lang = c; break; }
  }
  helpDir = docDir + "/ayuda/" + lang;
  QFile f(helpDir + "/indice.tsv");
  if(!f.open(QIODevice::ReadOnly)) return;
  QString chapter;
  while(!f.atEnd())
  {
    const QString line = QString::fromUtf8(f.readLine()).trimmed();
    const QStringList c = line.split('\t');
    if(c.size() < 4) continue;
    Entry e;
    e.file = c[0]; e.anchor = c[1]; e.level = c[2].toInt(); e.title = c[3];
    if(e.level == 1) chapter = e.title;
    e.chapter = chapter;
    entries.append(e);
  }
}

void DlgHelp::buildTree()
{
  QTreeWidgetItem *top = nullptr;
  for(int i=0; i<entries.size(); i++)
  {
    const Entry &e = entries[i];
    QTreeWidgetItem *item = (e.level == 1 || top == nullptr) ? new QTreeWidgetItem(ui->treeIndex) : new QTreeWidgetItem(top);
    item->setText(0, e.title);
    item->setData(0, Qt::UserRole, i);
    if(e.level == 1) { top = item; QFont f = item->font(0); f.setBold(true); item->setFont(0, f); }
    treeItems.append(item);
  }
}

void DlgHelp::applyStyle()
{
  const bool dark = palette().color(QPalette::Window).lightness() < 128;
  const QString text = dark ? "#e6edf3" : "#1b2733";
  const QString head = dark ? "#4fd6e4" : "#0d1b2a";
  const QString link = dark ? "#4fd6e4" : "#006f7c";
  ui->browser->document()->setDefaultStyleSheet(QString(
    "body { font-family: 'Segoe UI', 'Noto Sans', sans-serif; font-size: 10pt; color: %1; }"
    "h1 { font-size: 22pt; color: %2; margin-top: 4px; margin-bottom: 10px; }"
    "h2 { font-size: 15pt; color: %2; margin-top: 18px; margin-bottom: 6px; }"
    "h3 { font-size: 11.5pt; color: %2; margin-top: 12px; margin-bottom: 3px; }"
    "a { color: %3; text-decoration: none; }"
    "li { margin-bottom: 2px; }").arg(text, head, link));
  ui->browser->setOpenLinks(false);
}

int DlgHelp::findEntry(const QString &file, const QString &anchor) const
{
  for(int i=0; i<entries.size(); i++)
    if(entries[i].file == file && entries[i].anchor == anchor) return i;
  if(anchor.isEmpty())   // el archivo sin ancla: su capitulo
    for(int i=0; i<entries.size(); i++)
      if(entries[i].file == file && entries[i].level == 1) return i;
  return -1;
}

void DlgHelp::select(int entry)
{
  if(entry < 0 || entry >= treeItems.size()) return;
  syncing = true;
  QTreeWidgetItem *item = treeItems[entry];
  ui->treeIndex->setCurrentItem(item);
  if(item->parent() != nullptr)
  {
    for(int i=0; i<ui->treeIndex->topLevelItemCount(); i++)
      if(ui->treeIndex->topLevelItem(i) != item->parent()) ui->treeIndex->topLevelItem(i)->setExpanded(false);
    item->parent()->setExpanded(true);
  }
  else
  {
    for(int i=0; i<ui->treeIndex->topLevelItemCount(); i++)
      if(ui->treeIndex->topLevelItem(i) != item) ui->treeIndex->topLevelItem(i)->setExpanded(false);
    item->setExpanded(true);
  }
  ui->treeIndex->scrollToItem(item);
  syncing = false;
}

void DlgHelp::showTopic(const QString &topic)
{
  if(entries.isEmpty()) return;
  int idx = -1;
  if(!topic.isEmpty())
  {
    for(int i=0; i<entries.size(); i++) if(entries[i].anchor == topic) { idx = i; break; }
    if(idx < 0)   // "s9-7" que ya no existe: el capitulo 9
    {
      const QRegularExpressionMatch m = QRegularExpression("^s(\\d+)-\\d+$").match(topic);
      if(m.hasMatch())
      {
        const QString prefix = m.captured(1) + ". ";
        for(int i=0; i<entries.size(); i++) if(entries[i].level == 1 && entries[i].title.startsWith(prefix)) { idx = i; break; }
      }
    }
  }
  if(idx < 0) idx = 0;   // inicio
  QUrl url = QUrl::fromLocalFile(helpDir + "/" + entries[idx].file);
  if(!entries[idx].anchor.isEmpty()) url.setFragment(entries[idx].anchor);
  ui->browser->setSource(url);
  select(idx);
}

void DlgHelp::slotHome()
{
  ui->lineSearch->clear();
  showTopic(QString());
}

void DlgHelp::slotTree(QTreeWidgetItem *item)
{
  if(syncing || item == nullptr) return;
  const int idx = item->data(0, Qt::UserRole).toInt();
  if(idx < 0 || idx >= entries.size()) return;
  QUrl url = QUrl::fromLocalFile(helpDir + "/" + entries[idx].file);
  if(!entries[idx].anchor.isEmpty()) url.setFragment(entries[idx].anchor);
  syncing = true;   // sourceChanged vuelve a elegir el mismo item
  ui->browser->setSource(url);
  if(item->parent() == nullptr) item->setExpanded(true);
  syncing = false;
}

void DlgHelp::slotAnchor(const QUrl &url)
{
  if(url.scheme() == "http" || url.scheme() == "https" || url.scheme() == "mailto")
  {
    QDesktopServices::openUrl(url);
    return;
  }
  QUrl target = url;
  if(url.isRelative()) target = ui->browser->source().resolved(url);
  if(target.isLocalFile() && !QFileInfo(target.toLocalFile()).fileName().endsWith(".html"))
  {
    QDesktopServices::openUrl(target);
    return;
  }
  ui->browser->setSource(target);
}

void DlgHelp::slotSource(const QUrl &url)
{
  if(syncing) return;
  select(findEntry(QFileInfo(url.path()).fileName(), url.fragment()));
}

QString DlgHelp::pdfFile() const
{
  const QString dir = findManualDir(docDir);
  if(dir.isEmpty()) return QString();
  const QString mine = dir + "/Manual_PvSuite" + (lang == "es" ? QString() : "_" + lang) + ".pdf";
  if(QFileInfo::exists(mine)) return mine;
  if(QFileInfo::exists(dir + "/Manual_PvSuite.pdf")) return dir + "/Manual_PvSuite.pdf";
  return QString();
}

// el PDF se abre con el programa del sistema (nunca dentro de la ayuda); si no esta, la carpeta de la documentacion
void DlgHelp::slotPdf()
{
  const QString f = pdfFile();
  if(!f.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(f));
  else slotFolder();
}

void DlgHelp::slotFolder()
{
  QString dir = findManualDir(docDir);
  if(dir.isEmpty()) dir = docDir;
  QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

QString DlgHelp::plain(const QString &s) const
{
  QString t = s;
  t.replace(QRegularExpression("<[^>]*>"), " ");
  t.replace("&nbsp;", " ").replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", "\"").replace("&amp;", "&");
  return t.simplified();
}

void DlgHelp::loadTexts() const
{
  if(textsLoaded) return;
  textsLoaded = true;
  texts.clear();
  for(int i=0; i<entries.size(); i++) texts.append(QString());
  int i = 0;
  while(i < entries.size())
  {
    // las entradas de un mismo archivo van seguidas y en el orden del texto
    int j = i;
    while(j < entries.size() && entries[j].file == entries[i].file) j++;
    QFile f(helpDir + "/" + entries[i].file);
    QString html;
    if(f.open(QIODevice::ReadOnly)) html = QString::fromUtf8(f.readAll());
    QList<int> pos;
    int from = 0;
    for(int k=i; k<j; k++)
    {
      int p = entries[k].anchor.isEmpty() ? 0 : html.indexOf("<a name=\"" + entries[k].anchor + "\"", from);
      if(p < 0) p = from;
      pos.append(p);
      from = p;
    }
    for(int k=i; k<j; k++)
    {
      const int end = (k + 1 < j) ? pos[k - i + 1] : html.size();
      const int start = pos[k - i];
      texts[k] = pvbFold(entries[k].title + " " + plain(html.mid(start, qMax(0, end - start))));
    }
    i = j;
  }
}

QList<int> DlgHelp::searchIndices(const QString &text) const
{
  QList<int> result;
  const QStringList terms = pvbFold(text).split(' ', Qt::SkipEmptyParts);
  if(terms.isEmpty()) return result;
  loadTexts();
  QList<QPair<int, int>> scored;   // puntaje, indice
  for(int i=0; i<entries.size(); i++)
  {
    const QString &t = texts[i];
    int score = 0;
    bool all = true;
    const QString title = pvbFold(entries[i].title);
    for(const QString &term : terms)
    {
      const int n = t.count(term);
      if(n == 0) { all = false; break; }
      score += qMin(n, 10) + (title.contains(term) ? 20 : 0);
    }
    if(all) scored.append(qMakePair(score, i));
  }
  std::stable_sort(scored.begin(), scored.end(), [](const QPair<int, int> &a, const QPair<int, int> &b) { return a.first > b.first; });
  for(const auto &p : scored) result.append(p.second);
  return result;
}

QStringList DlgHelp::search(const QString &text) const
{
  QStringList r;
  for(int i : searchIndices(text)) r << entries[i].anchor;
  return r;
}

void DlgHelp::slotSearchChanged(const QString &text)
{
  lastTerm = text.trimmed();
  if(lastTerm.size() < 2)
  {
    ui->stackLeft->setCurrentIndex(0);
    ui->labelStatus->clear();
    return;
  }
  ui->listResults->clear();
  const QList<int> hits = searchIndices(lastTerm);
  for(int i : hits)
  {
    const Entry &e = entries[i];
    QListWidgetItem *item = new QListWidgetItem(e.level == 1 ? e.title : e.title + "  —  " + e.chapter);
    item->setData(Qt::UserRole, i);
    ui->listResults->addItem(item);
  }
  ui->stackLeft->setCurrentIndex(1);
  ui->labelStatus->setText(hits.isEmpty() ? tr("Sin resultados para «%1»").arg(lastTerm) : tr("%1 resultado(s)").arg(hits.size()));
}

void DlgHelp::slotResult(QListWidgetItem *item)
{
  if(item == nullptr) return;
  const int i = item->data(Qt::UserRole).toInt();
  if(i < 0 || i >= entries.size()) return;
  QUrl url = QUrl::fromLocalFile(helpDir + "/" + entries[i].file);
  if(!entries[i].anchor.isEmpty()) url.setFragment(entries[i].anchor);
  ui->browser->setSource(url);
  select(i);
  // marca la primera aparicion de la primera palabra en el texto que se ve
  const QString first = lastTerm.section(' ', 0, 0);
  if(!first.isEmpty())
  {
    ui->browser->setTextCursor(ui->browser->cursorForPosition(QPoint(0, 0)));   // desde lo que se ve (la seccion)
    ui->browser->find(first);
  }
}

// ---------------------------------------------------------------------------------------------------------
// F1: tema de ayuda segun la ventana activa
// ---------------------------------------------------------------------------------------------------------
QString pvbHelpTopicForClass(const char *cls)
{
  static const struct { const char *cls; const char *topic; } table[] = {
    {"DlgTags", "tags"},               {"DlgBind", "acciones"},           {"DlgTagMonitor", "s8-7"},
    {"DlgCompiler", "compilar"},       {"DlgOpt", "pvdevelop"},           {"dlgDaemon", "daemons"},
    {"dlgInsertWidget", "s4-1"},       {"dlgProperty", "s4-2"},           {"dlgnewprj", "s3-1"},
    {"dlgInsertFunction", "programar"}, {"dlgPasteWidget", "s4-4"},       {"dlgsearchreplace", "pvdevelop"},
    {"dlgeditlayout", "designer"},
  };
  for(const auto &t : table)
    if(strcmp(cls, t.cls) == 0) return QString::fromLatin1(t.topic);
  return QString();
}

QString pvbHelpTopicFor(const QObject *window)
{
  if(window == nullptr) return QString();
  for(const QMetaObject *m = window->metaObject(); m != nullptr; m = m->superClass())
  {
    const QString t = pvbHelpTopicForClass(m->className());
    if(!t.isNull()) return t;
  }
  return QString();
}

// Shift+F1 (o Ayuda > Que es esto?): el texto de un control es su "Que es esto?" o, si no tiene, su tooltip
QString pvbWhatsThisText(const QWidget *w)
{
  if(w == nullptr) return QString();
  return w->whatsThis().isEmpty() ? w->toolTip() : w->whatsThis();
}

// los popups (el cartel del "Que es esto?", menus, tooltips) no tienen ayuda: si el cartel la recibia como ventana,
// showText lo borraba mientras Qt le entregaba el clic y pvdevelop se cerraba
static bool pvbIsPopup(const QWidget *w)
{
  const Qt::WindowType t = w->window()->windowType();
  return t == Qt::Popup || t == Qt::ToolTip;
}

static bool pvbIsButtonRow(QLayoutItem *item)
{
  if(item->widget() != nullptr) return qobject_cast<QDialogButtonBox *>(item->widget()) != nullptr;
  QLayout *l = item->layout();
  if(l == nullptr || qobject_cast<QHBoxLayout *>(l) == nullptr) return false;
  for(int i = 0; i < l->count(); i++)
  {
    QWidget *c = l->itemAt(i)->widget();
    if(c != nullptr && !qobject_cast<QAbstractButton *>(c) && !qobject_cast<QLabel *>(c) && !qobject_cast<QDialogButtonBox *>(c)) return false;
  }
  return true;
}

void pvbFitWindowToScreen(QWidget *w)
{
  if(w == nullptr || !w->isWindow() || !w->inherits("QDialog") || w->property("pvbFitted").toBool()) return;
  const char *cls = w->metaObject()->className();
  if(cls[0] == 'Q') return;   // solo los dialogos de pvdevelop (no QMessageBox, QFileDialog...)
  w->setProperty("pvbFitted", true);
  QScreen *sc = w->screen();
  if(sc == nullptr) return;
  const QRect avail = sc->availableGeometry();
  const QSize frame(w->frameGeometry().width() - w->width(), w->frameGeometry().height() - w->height());
  const int maxW = avail.width() - qMax(frame.width(), 16);
  const int maxH = avail.height() - qMax(frame.height(), 40);
  QBoxLayout *box = qobject_cast<QBoxLayout *>(w->layout());
  const QSize need = w->minimumSizeHint();
  if(box != nullptr && box->count() > 0 && (need.height() > maxH || need.width() > maxW)
     && w->findChild<QScrollArea *>("pvbFitScroll") == nullptr)
  {
    QScrollArea *sa = new QScrollArea(w);
    sa->setObjectName("pvbFitScroll");
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    QWidget *inner = new QWidget;
    QVBoxLayout *il = new QVBoxLayout(inner);
    il->setContentsMargins(0, 0, 0, 0);
    const int n = box->count() - (pvbIsButtonRow(box->itemAt(box->count() - 1)) ? 1 : 0);
    for(int i = 0; i < n; i++)
    {
      const int stretch = box->stretch(0);
      QLayoutItem *item = box->takeAt(0);
      if(QWidget *c = item->widget()) { il->addWidget(c, stretch); delete item; }
      else if(QLayout *l = item->layout()) { l->setParent(nullptr); il->addLayout(l, stretch); }
      else il->addItem(item);
    }
    sa->setWidget(inner);
    box->insertWidget(0, sa, 1);
    w->setMinimumSize(0, 0);
  }
  QRect g = w->geometry();
  g.setSize(g.size().boundedTo(QSize(maxW, maxH)));
  // que quede entera dentro de la pantalla (con el marco)
  if(g.right() + frame.width() > avail.right()) g.moveRight(avail.right() - frame.width());
  if(g.bottom() + frame.height() > avail.bottom()) g.moveBottom(avail.bottom() - frame.height());
  if(g.left() < avail.left()) g.moveLeft(avail.left());
  if(g.top() - (frame.height() - frame.width() / 2) < avail.top()) g.moveTop(avail.top() + frame.height() - frame.width() / 2);
  if(g != w->geometry()) w->setGeometry(g);
}

HelpKeyFilter::HelpKeyFilter(QWidget *main, std::function<QString()> mainTopicFn,
                             std::function<void(const QString &, QWidget *)> openFn, QObject *parent)
  : QObject(parent), mainWindow(main), mainTopic(mainTopicFn), open(openFn)
{
}

QString HelpKeyFilter::topicOf(QWidget *window) const
{
  return (window == mainWindow) ? mainTopic() : pvbHelpTopicFor(window);
}

bool HelpKeyFilter::eventFilter(QObject *o, QEvent *e)
{
  switch(e->type())
  {
    case QEvent::Show:
      if(o->isWidgetType() && static_cast<QWidget *>(o)->isWindow()) pvbFitWindowToScreen(static_cast<QWidget *>(o));
      return false;
    case QEvent::KeyPress:
    {
      QKeyEvent *k = static_cast<QKeyEvent *>(e);
      const Qt::KeyboardModifiers mods = k->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier);
      if(k->key() != Qt::Key_F1 || (mods != Qt::NoModifier && mods != Qt::ShiftModifier)) return false;
      if(mods == Qt::ShiftModifier)   // el cursor pasa a "?": un clic en un control muestra su ayuda ahi mismo
      {
        if(!QWhatsThis::inWhatsThisMode()) QWhatsThis::enterWhatsThisMode();
        return true;
      }
      QWidget *w = QApplication::activeModalWidget();
      if(w == nullptr) w = QApplication::activeWindow();
      if(w == nullptr) return false;
      if(w->inherits("DlgHelp")) return false;
      open(topicOf(w), w);
      return true;
    }
    // "Que es esto?": el control con texto lo muestra; si no tiene, sigue al contenedor y por ultimo a la ventana
    // (que siempre tiene al menos el enlace a la ayuda)
    case QEvent::QueryWhatsThis:
    {
      QWidget *w = qobject_cast<QWidget *>(o);
      if(w == nullptr || pvbIsPopup(w) || (pvbWhatsThisText(w).isEmpty() && !w->isWindow())) return false;
      e->accept();
      return true;
    }
    case QEvent::WhatsThis:
    {
      QWidget *w = qobject_cast<QWidget *>(o);
      if(w == nullptr || pvbIsPopup(w)) return false;
      QString text = pvbWhatsThisText(w);
      if(text.isEmpty() && !w->isWindow()) return false;
      if(!Qt::mightBeRichText(text)) text = text.toHtmlEscaped().replace("\n", "<br>");
      if(text.isEmpty()) text = "<b>" + w->window()->windowTitle().toHtmlEscaped() + "</b>";
      text += "<p><a href=\"pvbhelp:" + topicOf(w->window()) + "\">" + tr("Ver en la ayuda (F1)") + "</a></p>";
      QWhatsThis::showText(static_cast<QHelpEvent *>(e)->globalPos(), text, w);
      return true;
    }
    case QEvent::WhatsThisClicked:
    {
      const QString href = static_cast<QWhatsThisClickedEvent *>(e)->href();
      QWidget *w = qobject_cast<QWidget *>(o);
      if(!href.startsWith("pvbhelp:") || w == nullptr) return false;
      QWhatsThis::hideText();
      open(href.mid(8), w->window());
      return true;
    }
    default:
      return false;
  }
}
