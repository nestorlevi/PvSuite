/****************************************************************************
** Nestor: texto predictivo para el editor de pvdevelop (ver codecompleter.h)
****************************************************************************/
#include "codecompleter.h"
#include <QTextEdit>
#include <QTextCursor>
#include <QTextBlock>
#include <QCompleter>
#include <QStandardItemModel>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QSet>
#include <QMap>
#include <QMetaObject>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <algorithm>

// palabra -> descripcion (firma de la funcion, "clase de rllib", ...)
static QMap<QString, QString> apiWords;
static QSet<QString> apiFunctions;

// Carpeta de la instalacion (o del codigo fuente) que contiene pvserver/processviewserver.h
static QString suiteDir()
{
  QStringList bases;
  bases << QString::fromLocal8Bit(qgetenv("SUITEDIR")) << QString::fromLocal8Bit(qgetenv("PVBDIR"))
        << "/opt/pvb"
        << QCoreApplication::applicationDirPath() + "/../.."         // instalado: <destino>/win-mingw/bin
        << QCoreApplication::applicationDirPath() + "/../../../..";  // compilado: Compilar/pvb6-*/pvdevelop/release
  for(const QString &b : bases)
  {
    if(!b.isEmpty() && QFile::exists(b + "/pvserver/processviewserver.h")) return QDir::cleanPath(b);
  }
  return QString();
}

static void loadApiWords()
{
  if(!apiWords.isEmpty()) return;

  static const char *keywords[] = {
    "alignas","alignof","auto","bool","break","case","catch","char","class","const","constexpr",
    "const_cast","continue","default","delete","double","dynamic_cast","else","enum","explicit",
    "extern","false","float","for","friend","goto","inline","int","long","mutable","namespace",
    "new","noexcept","nullptr","operator","private","protected","public","register",
    "reinterpret_cast","return","short","signed","sizeof","static","static_assert","static_cast",
    "struct","switch","template","this","throw","true","try","typedef","typename","union",
    "unsigned","using","virtual","void","volatile","while","#include","#define","#ifdef","#ifndef",
    "#endif","printf","sprintf","snprintf","strcpy","strncpy","strcat","strcmp","strncmp","strlen",
    "memset","memcpy","malloc","free","fopen","fclose","fprintf","fgets","atoi","atof",
    "PARAM","DATA","NULL" };
  for(const char *k : keywords) apiWords.insert(k, QString());

  const QString dir = suiteDir();
  if(dir.isEmpty()) return;

  // funciones de pvserver: "int pvSetText(PARAM *p, int id, const char *text);"
  QFile f(dir + "/pvserver/processviewserver.h");
  if(f.open(QIODevice::ReadOnly))
  {
    const QString text = QString::fromLatin1(f.readAll());
    static const QRegularExpression proto(
      "(?:^|\\n)[ \\t]*(?:[A-Za-z_][\\w \\t\\*&:<>]*?[\\s\\*&])((?:pv|qpw|qwt|gl|xml)\\w*)[ \\t]*\\(([^;{}]*?)\\)[ \\t]*;");
    QRegularExpressionMatchIterator it = proto.globalMatch(text);
    while(it.hasNext())
    {
      QRegularExpressionMatch m = it.next();
      const QString name = m.captured(1);
      if(!apiWords.value(name).isEmpty()) continue; // primera declaracion
      apiWords.insert(name, m.captured(0).simplified());
      apiFunctions.insert(name);
    }
  }

  // clases de rllib
  QDir rllib(dir + "/rllib/lib");
  static const QRegularExpression cls("\\bclass\\s+(?:\\w+_EXPORT\\s+)?(rl\\w+)");
  for(const QString &h : rllib.entryList(QStringList() << "*.h", QDir::Files))
  {
    QFile hf(rllib.filePath(h));
    if(!hf.open(QIODevice::ReadOnly)) continue;
    QRegularExpressionMatchIterator it = cls.globalMatch(QString::fromLatin1(hf.readAll()));
    while(it.hasNext())
    {
      const QString name = it.next().captured(1);
      if(!apiWords.contains(name)) apiWords.insert(name, QString("clase de rllib (%1)").arg(h));
    }
  }
}

CodeCompleter::CodeCompleter(QTextEdit *e) : QObject(e), edit(e), enabled(true), documentChanged(true)
{
  loadApiWords();
  model = new QStandardItemModel(this);
  completer = new QCompleter(model, this);
  completer->setWidget(edit);
  completer->setCompletionMode(QCompleter::PopupCompletion);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
  completer->setWrapAround(false);
  completer->setMaxVisibleItems(12);
  connect(completer, QOverload<const QString &>::of(&QCompleter::activated), this, &CodeCompleter::insertCompletion);
  connect(edit->document(), &QTextDocument::contentsChanged, this, &CodeCompleter::slotDocumentChanged);
  connect(edit->document(), &QTextDocument::contentsChange, this, &CodeCompleter::slotContentsChange);
  edit->installEventFilter(this);
}

void CodeTextEdit::keyPressEvent(QKeyEvent *event)
{
  if(completer != nullptr && completer->popupVisible())
  {
    switch(event->key())
    {
      case Qt::Key_Enter:
      case Qt::Key_Return:
      case Qt::Key_Escape:
      case Qt::Key_Tab:
      case Qt::Key_Backtab:
        event->ignore(); // QCompleter acepta / cierra la lista
        return;
      default:
        break;
    }
  }
  QTextEdit::keyPressEvent(event);
}

bool CodeCompleter::popupVisible() const
{
  return completer->popup()->isVisible();
}

void CodeCompleter::setEnabled(bool e)
{
  enabled = e;
  if(!enabled) completer->popup()->hide();
}

void CodeCompleter::slotDocumentChanged()
{
  documentChanged = true;
}

// Nestor: se escribio o borro un caracter en el editor (no al cargar un archivo ni al pegar)
void CodeCompleter::slotContentsChange(int position, int removed, int added)
{
  (void) position;
  if(!enabled || (!edit->hasFocus() && !completer->popup()->isVisible())) return;
  if(added > 1 || removed > 1) { completer->popup()->hide(); return; }
  // despues de que el editor termine de procesar la tecla
  QMetaObject::invokeMethod(this, "perhapsShow", Qt::QueuedConnection, Q_ARG(bool, false));
}

// Nestor: tipo de cada palabra para el icono de la lista: W widget, T tag, D daemon, f funcion, C clase
static QMap<QString, QChar> wordKind;

// Nestor: palabras del proyecto (carpeta actual): los widgets de cada mascara (enum de maskN.cpp)
// y los tags de los daemons (TARGET.tagmap: PLC1_Motor1, PLC1_SYS, los miembros...).
// Se vuelven a leer solo si cambio algun archivo.
static void addProjectWords(QMap<QString, QString> &words)
{
  static QMap<QString, QString> cache;
  static QString stamp;
  QDir dir(QDir::currentPath());
  const QFileInfoList files = dir.entryInfoList(QStringList() << "mask*.cpp" << "*.tagmap", QDir::Files, QDir::Name);
  QString now = dir.absolutePath();
  for(const QFileInfo &fi : files) now += "|" + fi.fileName() + ":" + QString::number(fi.lastModified().toMSecsSinceEpoch());
  if(now != stamp)
  {
    stamp = now;
    cache.clear();
    wordKind.clear();
    static const QRegularExpression enumBlock("enum\\s*\\{([^}]*)\\}");
    static const QRegularExpression enumItem("\\b([A-Za-z_]\\w*)\\b\\s*(?:=[^,]*)?(?:,|$)");
    for(const QFileInfo &fi : files)
    {
      QFile f(fi.filePath());
      if(!f.open(QIODevice::ReadOnly)) continue;
      const QString text = QString::fromUtf8(f.readAll());
      if(fi.suffix() == "cpp")
      {
        // enum { ID_MAIN_WIDGET = 0, buttonOk, labelTemp, ..., ID_END_OF_WIDGETS };
        QRegularExpressionMatch m = enumBlock.match(text);
        if(!m.hasMatch()) continue;
        const QString mask = fi.completeBaseName();
        QRegularExpressionMatchIterator it = enumItem.globalMatch(m.captured(1));
        while(it.hasNext())
        {
          const QString w = it.next().captured(1);
          if(w.startsWith("ID_")) continue;
          if(!cache.contains(w)) cache.insert(w, QCoreApplication::translate("CodeCompleter", "widget de %1").arg(mask));
          wordKind.insert(w, QChar('W'));
        }
      }
      else
      {
        // source=PLC1 / tag name=Motor1.Estado type=int ...
        QString source = fi.completeBaseName();
        for(const QString &line : text.split('\n'))
        {
          if(line.startsWith("source=")) source = line.mid(7).trimmed().remove('"');
          if(!line.startsWith("tag name=")) continue;
          QString name = line.mid(9).section(' ', 0, 0).remove('"');
          const QString type = line.section(" type=", 1).section(' ', 0, 0);
          const QString tip = QCoreApplication::translate("CodeCompleter", "tag de %1 (%2)").arg(source, type);
          QString top = name.section('.', 0, 0).section('[', 0, 0);
          if(!cache.contains(source + "_" + top)) cache.insert(source + "_" + top, tip);
          wordKind.insert(source + "_" + top, QChar('T'));
          if(!cache.contains(source)) cache.insert(source, QCoreApplication::translate("CodeCompleter", "daemon (memoria compartida)"));
          wordKind.insert(source, QChar('D'));
          const QString member = name.section('.', 1).section('[', 0, 0);
          if(!member.isEmpty() && !cache.contains(member)) cache.insert(member, tip);
          if(!member.isEmpty() && !wordKind.contains(member)) wordKind.insert(member, QChar('T'));
        }
      }
    }
  }
  for(QMap<QString, QString>::const_iterator i = cache.constBegin(); i != cache.constEnd(); ++i)
  {
    if(words.value(i.key()).isEmpty()) words.insert(i.key(), i.value());
  }
}

// icono de la lista: cuadrado de color con la letra del tipo (W widget, T tag, D daemon, f funcion, C clase)
static QIcon kindIcon(QChar kind)
{
  static QMap<QChar, QIcon> icons;
  if(icons.contains(kind)) return icons.value(kind);
  QColor color;
  switch(kind.unicode())
  {
    case 'W': color = QColor(30, 136, 229); break;  // widget: azul
    case 'T': color = QColor(46, 160, 67);  break;  // tag: verde
    case 'D': color = QColor(0, 128, 128);  break;  // daemon: verde azulado
    case 'f': color = QColor(142, 68, 173); break;  // funcion de pvserver: violeta
    case 'C': color = QColor(230, 126, 34); break;  // clase de rllib: naranja
    default:  icons.insert(kind, QIcon()); return QIcon();
  }
  QPixmap pm(16, 16);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(Qt::NoPen);
  p.setBrush(color);
  p.drawRoundedRect(QRectF(0.5, 0.5, 15, 15), 3, 3);
  QFont f = p.font();
  f.setPixelSize(11);
  f.setBold(true);
  p.setFont(f);
  p.setPen(Qt::white);
  p.drawText(QRect(0, 0, 16, 16), Qt::AlignCenter, QString(kind));
  p.end();
  icons.insert(kind, QIcon(pm));
  return icons.value(kind);
}

void CodeCompleter::updateModel()
{
  // palabras fijas + del proyecto (widgets y tags) + del archivo abierto (identificadores de 4 letras o mas)
  QMap<QString, QString> words = apiWords;
  addProjectWords(words);
  static const QRegularExpression ident("\\b[A-Za-z_][A-Za-z0-9_]{3,}\\b");
  const QString typing = wordUnderCursor(); // la palabra que se esta escribiendo no se sugiere a si misma
  QRegularExpressionMatchIterator it = ident.globalMatch(edit->toPlainText());
  while(it.hasNext())
  {
    const QString w = it.next().captured(0);
    if(w != typing && !words.contains(w)) words.insert(w, QString());
  }

  QStringList names = words.keys();
  std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
    const int c = a.compare(b, Qt::CaseInsensitive);
    return (c != 0) ? (c < 0) : (a < b);
  });
  model->clear();
  for(const QString &n : names)
  {
    QStandardItem *item = new QStandardItem(n);
    item->setIcon(kindIcon(wordKind.contains(n) ? wordKind.value(n) : apiFunctions.contains(n) ? QChar('f') :
                           (!words.value(n).isEmpty() && n.startsWith("rl")) ? QChar('C') : QChar(' ')));
    const QString tip = words.value(n);
    if(!tip.isEmpty()) item->setToolTip(tip);
    item->setEditable(false);
    model->appendRow(item);
  }
  documentChanged = false;
}

QString CodeCompleter::wordUnderCursor() const
{
  const QTextCursor tc = edit->textCursor();
  const QString block = tc.block().text();
  int end = tc.positionInBlock();
  int start = end;
  while(start > 0)
  {
    const QChar c = block.at(start - 1);
    if(c.isLetterOrNumber() || c == '_' || c == '#') start--;
    else break;
  }
  return block.mid(start, end - start);
}

bool CodeCompleter::eventFilter(QObject *obj, QEvent *event)
{
  if(obj != edit || event->type() != QEvent::KeyPress || !enabled) return QObject::eventFilter(obj, event);

  QKeyEvent *ke = static_cast<QKeyEvent *>(event);
  if(completer->popup()->isVisible())
  {
    // el popup se ocupa de estas teclas (QCompleter las reenvia al editor)
    switch(ke->key())
    {
      case Qt::Key_Enter:
      case Qt::Key_Return:
      case Qt::Key_Escape:
      case Qt::Key_Tab:
      case Qt::Key_Backtab:
        ke->ignore();
        return true;
      default:
        break;
    }
  }

  if((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_Space)
  {
    perhapsShow(true);
    return true;
  }

  // lo que se escribe se detecta en slotContentsChange(): con el popup abierto QCompleter
  // pasa las teclas al editor con event() y no pasan por este filtro
  return QObject::eventFilter(obj, event);
}

void CodeCompleter::perhapsShow(bool force)
{
  QAbstractItemView *popup = completer->popup();
  if(!enabled) { popup->hide(); return; }

  const QString prefix = wordUnderCursor();
  if(!force && prefix.length() < 3) { popup->hide(); return; }
  if(documentChanged) updateModel();

  if(prefix != completer->completionPrefix()) completer->setCompletionPrefix(prefix);
  if(completer->completionCount() == 0) { popup->hide(); return; }
  // ya esta escrita la unica sugerencia
  if(!force && completer->completionCount() == 1 && completer->currentCompletion() == prefix) { popup->hide(); return; }

  popup->setCurrentIndex(completer->completionModel()->index(0, 0));
  QRect cr = edit->cursorRect();
  cr.setWidth(popup->sizeHintForColumn(0) + popup->verticalScrollBar()->sizeHint().width() + 8);
  completer->complete(cr);
}

void CodeCompleter::insertCompletion(const QString &completion)
{
  if(completer->widget() != edit) return;
  QTextCursor tc = edit->textCursor();
  const int len = wordUnderCursor().length();
  tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, len);
  tc.insertText(completion);
  // funciones de pvserver: agregar "(" si no esta
  if(apiFunctions.contains(completion))
  {
    const QString rest = tc.block().text().mid(tc.positionInBlock());
    if(!rest.startsWith("(")) tc.insertText("(");
  }
  edit->setTextCursor(tc);
}
