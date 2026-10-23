/****************************************************************************
** Nestor: acciones y animaciones de los widgets (ver pvbindgen.h y pvserver/pvbind.h)
****************************************************************************/
#include "pvbindgen.h"
#include <QWidget>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QMap>
#include <QVariant>

static const char *PROP = "pvbBind";

// clave=valor separados por espacios; los valores pueden ir entre comillas ("" = una comilla)
QStringList pvbSplitPairs(const QString &line, QString *err)
{
  QStringList out;
  int i = 0, n = line.size();
  while(i < n)
  {
    while(i < n && line[i].isSpace()) i++;
    if(i >= n) break;
    int eq = line.indexOf('=', i);
    if(eq < 0) { if(err) *err = QString("falta '=' en: %1").arg(line.mid(i)); break; }
    QString key = line.mid(i, eq - i).trimmed();
    i = eq + 1;
    QString val;
    if(i < n && line[i] == '"')
    {
      i++;
      while(i < n)
      {
        if(line[i] == '"')
        {
          if(i + 1 < n && line[i+1] == '"') { val += '"'; i += 2; continue; }
          i++;
          break;
        }
        val += line[i++];
      }
    }
    else
    {
      while(i < n && !line[i].isSpace()) val += line[i++];
    }
    out << key << val;
  }
  return out;
}

QString pvbBinding(QWidget *w)
{
  return w ? w->property(PROP).toString() : QString();
}

void pvbSetBinding(QWidget *w, const QString &pairs)
{
  if(w) w->setProperty(PROP, pairs.trimmed());
}

QList<PvbTag> pvbProjectTags()
{
  QList<PvbTag> list;
  for(const QString &m : QDir::current().entryList(QStringList() << "*.tagmap", QDir::Files, QDir::Name))
  {
    QFile f(m);
    if(!f.open(QIODevice::ReadOnly)) continue;
    QString source = QFileInfo(m).completeBaseName(), protocol = "modbus";
    for(const QString &l : QString::fromUtf8(f.readAll()).split('\n'))
    {
      QString t = l.trimmed();
      if(t.startsWith("source=")) source = t.mid(7).remove('"');
      if(t.startsWith("protocol=")) protocol = t.mid(9).remove('"');
      if(!t.startsWith("tag ")) continue;
      QStringList kv = pvbSplitPairs(t.mid(4));
      PvbTag tg;
      tg.source = source;
      tg.protocol = protocol;
      for(int k=0; k+1<kv.size(); k+=2)
      {
        if(kv[k] == "name") tg.name = kv[k+1];
        if(kv[k] == "type") tg.type = kv[k+1];
        if(kv[k] == "plcaddr") tg.address = kv[k+1];
      }
      if(!tg.name.isEmpty()) list << tg;
    }
  }
  return list;
}

// Nestor: como pvbDoubleFormat de pvserver/pvbind.h (un "%s" o un "%d" con un double se caia o mostraba basura)
bool pvbValidTextFormat(const QString &fmt)
{
  if(fmt.isEmpty() || (fmt.contains('|') && !fmt.contains('%'))) return true;
  int n = 0;
  for(int i = 0; i < fmt.size(); i++)
  {
    if(fmt[i] != '%') continue;
    i++;
    if(i < fmt.size() && fmt[i] == '%') continue;
    while(i < fmt.size() && QString("-+ #0").contains(fmt[i])) i++;
    while(i < fmt.size() && fmt[i].isDigit()) i++;
    if(i < fmt.size() && fmt[i] == '.') { i++; while(i < fmt.size() && fmt[i].isDigit()) i++; }
    if(i < fmt.size() && fmt[i] == 'l') i++;
    if(i >= fmt.size() || !QString("eEfFgG").contains(fmt[i])) return false;
    n++;
  }
  return n == 1;
}

//------------------------------------------------------------------------------------------------
// generacion de maskN_pvb.h

namespace {

struct Gen
{
  QList<PvbTag> tags;
  QStringList errors;
  QStringList includes;            // encabezados que necesita la mascara
  int mask = 0;
  bool faceplate = false;          // codigo de un faceplate: $M se resuelve al abrirlo (FC, FM)
  QMap<QString, QString> params;   // M -> PLC1:Motores[0] (ejemplo, para el tipo de cada miembro)
  QStringList noSys;               // daemons sin SYS.Ciclo (ya avisados)

  // bits de un tipo entero (0 = no es entero: no tiene bits)
  static int bitsOf(const QString &type)
  {
    return (type == "byte") ? 8 : (type == "int" || type == "uint") ? 16 : (type == "dint" || type == "udint") ? 32 : 0;
  }

  // como tagBase y ademas "PLC1:Estado.3" = el bit 3 de un entero (un miembro de estructura nunca es un numero)
  bool tag(const QString &text0, PvbTag *t)
  {
    static const QRegularExpression rxBit("^(.+)\\.(\\d+)$");
    QString text = text0.trimmed();
    if(text.startsWith('!')) text = text.mid(1).trimmed();
    QRegularExpressionMatch m = rxBit.match(text);
    if(!m.hasMatch()) return tagBase(text0, t);
    if(!tagBase(m.captured(1), t)) return false;
    int n = m.captured(2).toInt(), bits = bitsOf(t->type);
    if(bits == 0)
    {
      errors << QString("%1: los bits solo se pueden usar en tags enteros (%2 es %3)").arg(text0.trimmed(), m.captured(1), t->type);
      return false;
    }
    if(n >= bits)
    {
      errors << QString("%1: %2 es de %3 bits (.0 a .%4)").arg(text0.trimmed(), m.captured(1)).arg(bits).arg(bits - 1);
      return false;
    }
    t->bit = n;
    return true;
  }

  // "PLC1:Motor1.Marcha" (o PLC1_Motor1.Marcha, o $M.Marcha en un faceplate) -> el tag con su tipo
  bool tagBase(const QString &text0, PvbTag *t)
  {
    QString text = text0.trimmed();
    if(text.startsWith('!')) text = text.mid(1).trimmed();
    if(text.startsWith('$'))
    {
      QString pn = text.mid(1).section('.', 0, 0), member = text.section('.', 1);
      if(!faceplate || !params.contains(pn) || member.isEmpty())
      {
        errors << QString("%1: tag desconocido ($ no se usa en una pantalla)").arg(text0);
        return false;
      }
      QString ex = params.value(pn);
      t->source = "$" + pn;
      t->name = member;
      t->type = "real";
      for(const PvbTag &x : tags)
        if(x.source == ex.section(':', 0, 0) && x.name == ex.section(':', 1) + "." + member) { t->type = x.type; t->protocol = x.protocol; }
      return true;
    }
    for(const PvbTag &x : tags)
    {
      if(text == x.source + ":" + x.name || text == x.source + "_" + x.name) { *t = x; return true; }
    }
    int c = text.indexOf(':');
    if(c > 0)   // Nestor: no esta en los .tagmap: se genera igual (real), con aviso (un error de tipeo no compilaria)
    {
      t->source = text.left(c); t->name = text.mid(c + 1); t->type = "real";
      errors << QString("%1: el tag no esta en los .tagmap del proyecto (se toma como real)").arg(text);
      return true;
    }
    errors << QString("tag desconocido: %1").arg(text0);
    return false;
  }
  static bool isParam(const PvbTag &t) { return t.source.startsWith('$'); }
  QString src(const PvbTag &t) const { return isParam(t) ? QString("FC") : t.source; }
  QString cid(const PvbTag &t) const { return isParam(t) ? "FM->" + t.name : t.source + "_" + t.name; }
  bool isBool(const PvbTag &t) const { return t.type == "bool"; }
  bool isInt(const PvbTag &t) const { return t.type == "int" || t.type == "byte" || t.type == "dint"; }
  bool isUInt(const PvbTag &t) const { return t.type == "uint" || t.type == "udint"; }
  static bool isBit(const PvbTag &t) { return t.bit >= 0; }
  // el tag como rlTag: el bit de un entero es un rlTag bool armado al vuelo (rllib/lib/rltag.h)
  QString rtag(const PvbTag &t) const { return isBit(t) ? QString("rlTagBitOf(%1, %2)").arg(cid(t)).arg(t.bit) : cid(t); }
  QString get(const PvbTag &t) const
  {
    QString f = (isBool(t) || isBit(t)) ? "getBool" : isInt(t) ? "getInt" : isUInt(t) ? "getUInt" : "getReal";
    return QString("(double) %1.%2(%3)").arg(src(t), f, rtag(t));
  }
  QString set(const PvbTag &t, const QString &v) const
  {
    if(isBool(t) || isBit(t)) return QString("%1.setBool(%2, ((%3) != 0) ? 1 : 0);").arg(src(t), rtag(t), v);
    // Nestor: llround (long es de 32 bits en Windows) y uint/udint por unsigned: 3000000000 no desborda
    if(isUInt(t)) return QString("%1.setInt(%2, (int) (unsigned int) llround(%3));").arg(src(t), cid(t), v);
    if(isInt(t))  return QString("%1.setInt(%2, (int) llround(%3));").arg(src(t), cid(t), v);
    return QString("%1.setReal(%2, %3);").arg(src(t), cid(t), v);
  }
  // escritura de un tag (los enteros se redondean, los bool y los bits valen 0 o 1)
  QString write(const PvbTag &t, const QString &v)
  {
    if(isBit(t) && bitsOf(t.type) > 16)   // ver rlTagBit (rllib/lib/rltag.h)
      errors << QString("%1:%2.%3: escribir un bit solo se puede en enteros de 8 o 16 bits (leerlo, si)")
                .arg(t.source, t.name).arg(t.bit);
    QString nv = (isBool(t) || isBit(t)) ? QString("(((%1) != 0) ? 1.0 : 0.0)").arg(v)
               : (isInt(t) || isUInt(t)) ? QString("(double) llround(%1)").arg(v) : QString("(double) (%1)").arg(v);
    return QString("{ double pvbNew = %1; %2 }").arg(nv, set(t, "pvbNew"));
  }
  bool hasSys(const QString &source) const
  {
    for(const PvbTag &x : tags) if(x.source == source && x.name == "SYS.Ciclo") return true;
    return false;
  }
  QString quality(const PvbTag &t)
  {
    if(isParam(t)) return QString("(FC.quality(%1) != 0 && (FCICLO == NULL || FC.daemonAlive(*FCICLO)))").arg(cid(t));
    QString q = QString("(%1.quality(%2) != 0").arg(t.source, cid(t));
    if(hasSys(t.source)) q += QString(" && %1.daemonAlive(%1_SYS.Ciclo)").arg(t.source);
    else if(!noSys.contains(t.source))   // sin SYS.Ciclo no se sabe si el daemon esta corriendo
    {
      noSys << t.source;
      errors << QString("%1: el .tagmap no tiene SYS.Ciclo: con el daemon cerrado los datos se ven buenos (volver a compilar el daemon)").arg(t.source);
    }
    return q + ")";
  }
  static QString cstr(const QString &s)
  {
    QString o = s;
    o.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n");
    return "\"" + o + "\"";
  }
  QString num(const QString &v)
  {
    QString s = v.trimmed();
    if(s.isEmpty()) return "NAN";
    if(s == "true" || s == "on") return "1";
    if(s == "false" || s == "off") return "0";
    s.replace(',', '.');
    bool ok;
    s.toDouble(&ok);
    // texto del usuario: un valor que no es numero daria C++ que no compila
    if(!ok) { errors << QString("valor no numerico: %1").arg(v.trimmed()); return "NAN"; }
    return s;
  }

  // codigo de una accion de click / press / release
  QString action(const QString &spec, const QString &vmin, const QString &vmax)
  {
    QString kind = spec.section(':', 0, 0).trimmed().toLower(), arg = spec.section(':', 1).trimmed();
    if(kind == "goto")
    {
      bool ok;
      int to = arg.toInt(&ok);
      if(!ok || to < 1) { errors << QString("%1: goto:N con N = numero de mascara").arg(spec); return QString(); }
      // Nestor: a una pantalla que no existe el pvs cerraria la conexion (main.cpp: default: return 0)
      if(!QFile::exists(QString("mask%1.cpp").arg(to)) && !QFile::exists(QString("mask%1.lua").arg(to)))
      {
        errors << QString("%1: no existe la pantalla mask%2 (el boton no hace nada)").arg(spec).arg(to);
        return "return -1;";
      }
      return QString("return pvbGoto(%1, %2);").arg(mask).arg(to);
    }
    if(kind == "back") return QString("return pvbBack(%1);").arg(mask);
    if(kind == "home") return "return pvbHome();";
    PvbTag t;
    if((kind == "toggle" || kind == "write" || kind == "add" || kind == "sub") && arg.startsWith('!'))
      errors << QString("%1: el ! no se usa en una accion (se escribe el tag)").arg(spec);
    if(kind == "toggle")
    {
      if(!tag(arg, &t)) return QString();
      return write(t, QString("(%1 != 0) ? 0 : 1").arg(get(t))) + " return 0;";
    }
    int eq = arg.lastIndexOf('=');
    if(eq < 0) { errors << QString("accion sin valor: %1").arg(spec); return QString(); }
    if(!tag(arg.left(eq), &t)) return QString();
    QString v = num(arg.mid(eq + 1));
    if(kind == "write") return write(t, v) + " return 0;";
    if(kind == "add" || kind == "sub")
      return QString("double v = pvbClamp(%1 %2 (%3), %4, %5); %6 return 0;")
             .arg(get(t), kind == "add" ? "+" : "-", v, num(vmin), num(vmax), write(t, "v"));
    errors << QString("accion desconocida: %1").arg(spec);
    return QString();
  }

  // condicion -> expresion de C: "PLC1:Marcha" (distinto de 0), "!PLC1:Marcha", "PLC1:Estado == 2",
  // "PLC1:Temp > 80", "PLC1:Estado.3" (un bit); operadores == != >= <= > < (= es ==).
  // Si no es valida: el error y "fail" (las acciones siguen permitidas con "1", una animacion no anima con "0")
  QString cond(const QString &c, const char *fail = "1")
  {
    static const QRegularExpression rx("^\\s*(!?)\\s*([^\\s<>=!]+)\\s*(?:(==|!=|>=|<=|>|<|=)\\s*(\\S+))?\\s*$");
    QRegularExpressionMatch m = rx.match(c);
    PvbTag t;
    if(!m.hasMatch()) { errors << QString("condicion no valida: %1 (TAG, !TAG, TAG == 2, TAG > 80, TAG.3)").arg(c.trimmed()); return fail; }
    if(!tag(m.captured(2), &t)) return fail;
    QString op = (m.captured(3) == "=") ? "==" : m.captured(3);
    QString e = op.isEmpty() ? QString("(%1 != 0)").arg(get(t)) : QString("(%1 %2 %3)").arg(get(t), op, num(m.captured(4)));
    return m.captured(1).isEmpty() ? e : "!" + e;
  }

  // "0=#808080;1=#00C000;>80=#FF0000;else=" -> if/else con el color
  QString colorRules(const QString &rules, const QString &var)
  {
    QString code, elseColor = "\"\"";
    bool first = true;
    for(const QString &r0 : rules.split(';', Qt::SkipEmptyParts))
    {
      QString r = r0.trimmed();
      int eq = r.lastIndexOf('=');
      if(eq <= 0) continue;
      QString c = r.left(eq).trimmed(), color = cstr(r.mid(eq + 1).trimmed());
      if(c == "else" || c == "*") { elseColor = color; continue; }
      QString test;
      if(c.startsWith(">=") || c.startsWith("<=") || c.startsWith("!=")) test = var + " " + c.left(2) + " " + num(c.mid(2));
      else if(c.startsWith('>') || c.startsWith('<'))                   test = var + " " + c.left(1) + " " + num(c.mid(1));
      else                                                                test = var + " == " + num(c);
      code += QString("%1if(%2) c = %3;").arg(first ? "" : " else ", test, color);
      first = false;
    }
    code += first ? QString("c = %1;").arg(elseColor) : QString(" else c = %1;").arg(elseColor);
    return code;
  }
};

QString typeOf(QWidget *w)
{
  return w->statusTip().section(':', 0, 0).trimmed();
}

// widgets con acciones o animaciones (en el orden de la mascara)
QList<QWidget *> boundWidgets(QWidget *root)
{
  QList<QWidget *> bound;
  if(root)
    for(QWidget *w : root->findChildren<QWidget *>())
      if(!pvbBinding(w).isEmpty() && !w->objectName().isEmpty()) bound << w;
  return bound;
}

// cuerpo del switch de pvbDo (actions) y de pvbAnimate (anim)
void genBindings(Gen &g, const QList<QWidget *> &bound, QString &actions, QString &anim, QStringList &downloads)
{
  QTextStream a(&actions), an(&anim);
  for(QWidget *w : bound)
  {
    QMap<QString, QString> kv;
    QStringList p = pvbSplitPairs(pvbBinding(w));
    for(int k=0; k+1<p.size(); k+=2) kv[p[k]] = p[k+1];
    QString id = w->objectName(), type = typeOf(w);
    QString condCode;
    if(!kv.value("cond").isEmpty())
      condCode = QString("if(!(%1)) { pvbMessage(p, %2); return -1; } ")
                 .arg(g.cond(kv.value("cond")), Gen::cstr(kv.value("condmsg", QString::fromUtf8("No se cumple la condici\xc3\xb3n"))));
    QString confirm = kv.value("confirm");
    QStringList cases;
    struct { const char *key; const char *ev; } evs[] = { { "click", "PVB_CLICK" }, { "press", "PVB_PRESS" }, { "release", "PVB_RELEASE" } };
    for(auto &e : evs)
    {
      QString spec = kv.value(e.key);
      if(spec.isEmpty()) continue;
      QString act = g.action(spec, kv.value("min"), kv.value("max"));
      if(act.isEmpty()) continue;
      QString conf = (!confirm.isEmpty() && QString(e.key) == "click")
        ? QString("if(!confirmed) { pvbAskConfirm(p, id, event, text, val, %1); return -1; } ").arg(Gen::cstr(confirm)) : QString();
      cases << QString("      if(event == %1) { %2%3%4 }").arg(e.ev, condCode, conf, act);
    }
    if(!kv.value("enter").isEmpty())
    {
      PvbTag t;
      if(g.tag(kv.value("enter"), &t))
      {
        QString ev, value;
        if(type == "TQCheck" || type == "TQRadio") { ev = "PVB_CHECK"; value = "(text && strchr(text, '1')) ? 1 : 0"; }
        else if(type == "TQSlider" || type == "TQDial" || type == "TQSpinBox") { ev = "PVB_SLIDER"; value = QString("pvbClamp(val, %1, %2)").arg(g.num(kv.value("min")), g.num(kv.value("max"))); }
        else ev = "PVB_TEXT";
        QString conf = !confirm.isEmpty()
          ? QString("if(!confirmed) { pvbAskConfirm(p, id, event, text, val, %1); return -1; } ").arg(Gen::cstr(confirm)) : QString();
        if(ev == "PVB_TEXT")   // LineEdit: el texto llega en cada tecla, se escribe con Enter (llega como click)
        {
          QString confT = !confirm.isEmpty()
            ? QString("if(!confirmed) { pvbAskConfirm(p, id, event, t, val, %1); return -1; } ").arg(Gen::cstr(confirm)) : QString();
          cases << "      if(event == PVB_TEXT) { pvbEditText(id) = text ? text : \"\"; return 0; }";
          cases << QString("      if(event == PVB_CLICK) { const char *t = confirmed ? text : pvbEditText(id).c_str(); %1%2int ok; double v = pvbNumber(t, &ok); "
                           "if(!ok) { pvbMessage(p, %3); return -1; } v = pvbClamp(v, %4, %5); %6 return 0; }")
                   .arg(condCode, confT, Gen::cstr(QString::fromUtf8("Valor no v\xc3\xa1lido")),
                        g.num(kv.value("min")), g.num(kv.value("max")), g.write(t, "v"));
        }
        else
          cases << QString("      if(event == %1) { %2%3%4 return 0; }").arg(ev, condCode, conf, g.write(t, value));
      }
    }
    if(!cases.isEmpty()) a << "    case " << id << ":\n" << cases.join("\n") << "\n      break;\n";

    // animaciones
    bool useQ = kv.value("quality", "1") != "0";
    QString qExpr;
    PvbTag t;
    if(!kv.value("text").isEmpty() && g.tag(kv.value("text"), &t))
    {
      QString fmt = kv.value("fmt");
      if(!pvbValidTextFormat(fmt))
        g.errors << QString("%1: fmt=%2 no es una lista (A|B) ni un printf con una sola conversion de double (%.1f, %g): se usa %g").arg(id, fmt);
      qExpr = useQ ? g.quality(t) : QString("1");
      if(fmt.contains('|') && !fmt.contains('%'))
        an << QString("  { int q = %1; pvbSetText(p, mask, %2, q ? pvbChoice(%3, %4).c_str() : \"---\"); }\n")
              .arg(qExpr, id, Gen::cstr(fmt), g.get(t));
      else
        an << QString("  pvbText(p, mask, %1, %2, %3, %4);\n").arg(id, Gen::cstr(fmt), g.get(t), qExpr);
    }
    if(!kv.value("value").isEmpty() && g.tag(kv.value("value"), &t))
    {
      if(qExpr.isEmpty() && useQ) qExpr = g.quality(t);
      if(type == "TQLCDNumber")                    an << QString("  pvbFloat(p, mask, %1, %2);\n").arg(id, g.get(t));
      else if(type == "TQCheck" || type == "TQRadio") an << QString("  pvbChecked(p, mask, %1, %2 != 0);\n").arg(id, g.get(t));
      else                                         an << QString("  pvbValue(p, mask, %1, (int) lround(%2));\n").arg(id, g.get(t));
    }
    QString part = (kv.value("colorpart") == "fg") ? "PVB_A_FG" : "PVB_A_BG";
    QString blinkExpr = "0";
    if(!kv.value("blink").isEmpty()) blinkExpr = g.cond(kv.value("blink"), "0");   // TAG, !TAG, TAG > 80, TAG.3
    if(!kv.value("color").isEmpty() && g.tag(kv.value("color"), &t))
    {
      QString q = useQ ? g.quality(t) : QString("1");
      an << QString("  { double v = %1; const char *c = \"\"; %2 if(!%3) c = PVB_BAD_COLOR; pvbColor(p, mask, %4, %5, pvbBlink(%6, c)); }\n")
            .arg(g.get(t), g.colorRules(kv.value("colors"), "v"), q, id, part, blinkExpr);
    }
    else if(blinkExpr != "0")
      an << QString("  pvbColor(p, mask, %1, %2, %3 ? pvbBlink(1, \"#FF0000\") : \"\");\n").arg(id, part, blinkExpr);
    else if(!qExpr.isEmpty() && useQ)
      an << QString("  pvbColor(p, mask, %1, PVB_A_BG, %2 ? \"\" : PVB_BAD_COLOR);  // gris si el dato es malo\n").arg(id, qExpr);
    if(!kv.value("visible").isEmpty())   // TAG, !TAG, TAG == 2, TAG.3
      an << QString("  pvbVisible(p, mask, %1, %2);\n").arg(id, g.cond(kv.value("visible"), "1"));
    // imagen segun el valor y animacion (la hace pvbrowser)
    QString imageExpr;
    if(!kv.value("image").isEmpty() && !kv.value("images").isEmpty() && g.tag(kv.value("image"), &t))
    {
      imageExpr = QString("pvbChoice(%1, %2).c_str()").arg(Gen::cstr(kv.value("images")), g.get(t));
      for(const QString &f : kv.value("images").split('|', Qt::SkipEmptyParts)) if(!downloads.contains(f.trimmed())) downloads << f.trimmed();
    }
    if(!kv.value("anim").isEmpty() && !kv.value("frames").isEmpty())   // animar mientras: TAG, !TAG, TAG == 2, TAG > 80, TAG.3
    {
      QString animCond = g.cond(kv.value("anim"), "0");
      QStringList frames;
      for(const QString &f : kv.value("frames").split('|', Qt::SkipEmptyParts))
      {
        frames << f.trimmed();
        if(!downloads.contains(f.trimmed())) downloads << f.trimmed();
      }
      if(imageExpr.isEmpty()) imageExpr = Gen::cstr(w->whatsThis().trimmed());   // al detenerse: la imagen del widget
      an << QString("  { int a = %1 ? 1 : 0; pvbImageAnimation(p, mask, %2, a, %3, %4); if(!a) pvbImage(p, mask, %2, %5); }\n")
            .arg(animCond, id, Gen::cstr(frames.join(',')), QString::number(qMax(20, kv.value("period", "300").toInt())), imageExpr);
    }
    else if(!imageExpr.isEmpty())
      an << QString("  pvbImage(p, mask, %1, %2);\n").arg(id, imageExpr);
  }
}

QString errorComment(const Gen &g)
{
  if(g.errors.isEmpty()) return QString();
  QString o = "\n// ATENCION (pvdevelop):\n";
  for(const QString &e : g.errors) o += "//   " + e + "\n";
  return o;
}

} // namespace

void pvbLoadBindings(const QString &maskCpp, QWidget *root)
{
  if(root == nullptr) return;
  QFileInfo fi(maskCpp);
  QFile f(fi.dir().filePath(fi.completeBaseName() + "_pvb.h"));
  if(!f.open(QIODevice::ReadOnly)) return;
  for(const QString &l0 : QString::fromUtf8(f.readAll()).split('\n'))
  {
    QString l = l0.trimmed();
    if(l.startsWith("//pvb: "))
    {
      QString rest = l.mid(7).trimmed();
      QWidget *w = root->findChild<QWidget *>(rest.section(' ', 0, 0));
      if(w) pvbSetBinding(w, rest.section(' ', 1));
    }
  }
  f.close();
}

// Nestor: escribe el archivo solo si el contenido cambio, reemplazandolo de una vez (si falla queda el anterior)
static bool pvbWriteIfChanged(const QString &file, const QByteArray &data)
{
  QFile old(file);
  if(old.open(QIODevice::ReadOnly) && old.readAll() == data) return true;
  old.close();
  QSaveFile f(file);
  if(!f.open(QIODevice::WriteOnly)) return false;
  f.write(data);
  return f.commit();
}

// agrega "line" al comienzo de la funcion "func" de maskN_slots.h (despues del control de NULL) si no esta
static bool insertInSlot(QString &code, const QString &func, const QString &line, const QString &marker)
{
  int h = code.indexOf("static int " + func + "(");
  if(h < 0) return false;
  int bodyEnd = code.indexOf("\n}", h);
  if(bodyEnd < 0) return false;
  if(code.mid(h, bodyEnd - h).contains(marker)) return true;   // ya esta (quizas movida por el programador)
  int open = code.indexOf('{', h);
  int nullCheck = code.indexOf("return -1;", open);
  int pos = (nullCheck > 0 && nullCheck < bodyEnd) ? code.indexOf('\n', nullCheck) + 1 : code.indexOf('\n', open) + 1;
  code.insert(pos, "  " + line + (code.contains("\r\n") ? "\r\n" : "\n"));
  return true;
}

int pvbSaveBindings(const QString &maskCpp, QWidget *root)
{
  QFileInfo fi(maskCpp);
  QString base = fi.completeBaseName();                       // mask3
  QString pvbFile = fi.dir().filePath(base + "_pvb.h");
  QList<QWidget *> bound = boundWidgets(root);
  if(bound.isEmpty() && !QFile::exists(pvbFile)) return 0;    // la mascara nunca uso acciones

  Gen g;
  g.tags = pvbProjectTags();
  g.mask = QRegularExpression("(\\d+)$").match(base).captured(1).toInt();
  QString head, actions, anim;
  QStringList downloads;          // imagenes de las animaciones (se descargan una vez por mascara)
  for(QWidget *w : bound) head += "//pvb: " + w->objectName() + " " + pvbBinding(w) + "\n";
  genBindings(g, bound, actions, anim, downloads);

  QString out;
  QTextStream o(&out);
  o << "//###############################################################\n";
  o << "//# " << base << "_pvb.h: acciones y animaciones (pvdevelop: Propiedades > Accion y animacion)\n";
  o << "//# Generado por pvdevelop: no editar, se regenera al guardar la mascara.\n";
  o << "//# El codigo propio va en " << base << "_slots.h, antes o despues de la linea pvbActions de cada slot.\n";
  o << "//###############################################################\n";
  o << head;
  o << "#include \"pvbind.h\"\n";
  o << "static void pvbAnimate(PARAM *p);\n";
  o << "static inline int pvbAnimateBase(PARAM *p, void *) { pvbAnimate(p); return 0; }   // la mascara sigue al dia con una ventana modal abierta\n";
  for(const QString &inc : g.includes) o << "#include \"" << inc << "\"\n";
  o << "\n";
  o << "static int pvbDo(PARAM *p, int id, int event, int confirmed, const char *text, int val)\n{\n";
  o << "  (void) p; (void) event; (void) confirmed; (void) text; (void) val;\n";
  o << "  switch(id)\n  {\n" << actions << "    default:\n      break;\n  }\n  return 0;\n}\n\n";
  o << "// r > 0: ir a la mascara r   r < 0: la accion no se hizo (condicion, confirmacion pendiente)   0: seguir\n";
  o << "static int pvbActions(PARAM *p, int id, int event, const char *text, int val)\n{\n";
  o << "  if(event == PVB_SLIDER && id >= PVB_CONFIRM_BASE - 1)   // respuesta a \"Confirmar\" o a un aviso\n  {\n";
  o << "    if(id == PVB_CONFIRM_BASE - 1) return -1;\n";
  o << "    PvbPending pd = pvbPending();\n    pvbPending().id = 0;\n";
  o << "    if(pd.id != id - PVB_CONFIRM_BASE || val != MessageBoxYes) return -1;\n";
  o << "    return pvbDo(p, pd.id, pd.event, 1, pd.text.c_str(), pd.val);\n  }\n";
  o << "  return pvbDo(p, id, event, 0, text, val);\n}\n\n";
  o << "static void pvbAnimate(PARAM *p)\n{\n  const int mask = " << g.mask << ";\n  (void) p; (void) mask;\n  pvbTick()++;\n";
  if(!downloads.isEmpty()) o << "  pvbDownloadImages(p, mask, " << Gen::cstr(downloads.join('|')) << ");\n";
  o << anim << "}\n";
  o << errorComment(g);
  // Nestor: solo si cambio (no recompilar la mascara por nada) y de una vez (QSaveFile)
  if(!pvbWriteIfChanged(pvbFile, out.toUtf8())) return -1;

  // #include en maskN.cpp (antes de maskN_slots.h)
  QFile cf(maskCpp);
  if(cf.open(QIODevice::ReadOnly))
  {
    QString cpp = QString::fromUtf8(cf.readAll());
    cf.close();
    QString inc = "#include \"" + base + "_pvb.h\"";
    QString slotsInc = "#include \"" + base + "_slots.h\"";
    if(!cpp.contains(inc) && cpp.contains(slotsInc))
    {
      cpp.replace(slotsInc, inc + "   // pvdevelop: acciones y animaciones de Propiedades" + (cpp.contains("\r\n") ? "\r\n" : "\n") + slotsInc);
      pvbWriteIfChanged(maskCpp, cpp.toUtf8());
    }
  }
  // una linea en cada slot (una sola vez)
  QFile sf(fi.dir().filePath(base + "_slots.h"));
  if(sf.open(QIODevice::ReadOnly))
  {
    QString code = QString::fromUtf8(sf.readAll());
    sf.close();
    QString before = code;
    const QString c = "  // pvdevelop: accion de Propiedades (codigo propio: antes o despues)";
    insertInSlot(code, "slotNullEvent", "pvbAnimate(p);   // pvdevelop: animaciones de Propiedades", "pvbAnimate(");
    insertInSlot(code, "slotButtonEvent", "if(int r = pvbActions(p, id, PVB_CLICK, NULL, 0)) return (r > 0) ? r : 0;" + c, "PVB_CLICK");
    insertInSlot(code, "slotButtonPressedEvent", "if(int r = pvbActions(p, id, PVB_PRESS, NULL, 0)) return (r > 0) ? r : 0;" + c, "PVB_PRESS");
    insertInSlot(code, "slotButtonReleasedEvent", "if(int r = pvbActions(p, id, PVB_RELEASE, NULL, 0)) return (r > 0) ? r : 0;" + c, "PVB_RELEASE");
    insertInSlot(code, "slotTextEvent", "if(int r = pvbActions(p, id, PVB_TEXT, text, 0)) return (r > 0) ? r : 0;" + c, "PVB_TEXT");
    insertInSlot(code, "slotSliderEvent", "if(int r = pvbActions(p, id, PVB_SLIDER, NULL, val)) return (r > 0) ? r : 0;" + c, "PVB_SLIDER");
    insertInSlot(code, "slotCheckboxEvent", "if(int r = pvbActions(p, id, PVB_CHECK, text, 0)) return (r > 0) ? r : 0;" + c, "PVB_CHECK");
    if(code != before) pvbWriteIfChanged(sf.fileName(), code.toUtf8());
  }
  return g.errors.isEmpty() ? 0 : 1;
}
