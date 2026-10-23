/***************************************************************************
                          rltagdef.cpp  -  description
                             -------------------
    begin                : 2026
    copyright            : (C) Nestor Palomeque, pvbrowser community
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU LESSER GENERAL PUBLIC LICENSE as        *
 *   published by the Free Software Foundation                             *
 *                                                                         *
 ***************************************************************************/
#include "rltagdef.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <locale>
#include <cmath>
#include <math.h>

// ORG de rlSiemensTCP (rlsiemenstcp.h)
#define S7_ORG_DB 1
#define S7_ORG_M  2
#define S7_ORG_E  3
#define S7_ORG_A  4

static std::string trim(const std::string &s)
{
  size_t a = 0, b = s.size();
  while(a < b && isspace((unsigned char) s[a])) a++;
  while(b > a && isspace((unsigned char) s[b-1])) b--;
  return s.substr(a, b - a);
}

static std::string upper(const std::string &s)
{
  std::string r = s;
  for(size_t i=0; i<r.size(); i++) r[i] = (char) toupper((unsigned char) r[i]);
  return r;
}

static std::string lower(const std::string &s)
{
  std::string r = s;
  for(size_t i=0; i<r.size(); i++) r[i] = (char) tolower((unsigned char) r[i]);
  return r;
}

static bool startsWith(const std::string &s, const char *prefix)
{
  return s.compare(0, strlen(prefix), prefix) == 0;
}

// numero entero sin signo completo; -1 si no lo es
static int toNumber(const std::string &s)
{
  if(s.empty() || s.size() > 9) return -1;
  for(size_t i=0; i<s.size(); i++) if(!isdigit((unsigned char) s[i])) return -1;
  return atoi(s.c_str());
}

static std::string itos(int i)
{
  char buf[32];
  sprintf(buf, "%d", i);
  return buf;
}

// Nestor: texto para un comentario "// ..." del .h generado: una barra invertida al final (o antes de
// espacios) continua el comentario en la linea siguiente y se "come" un inicializador sin dar error
static std::string commentText(const std::string &s)
{
  std::string r = s;
  for(size_t i=0; i<r.size(); i++)
  {
    if(r[i] == '\\') r[i] = '/';
    else if(r[i] == '\r' || r[i] == '\n') r[i] = ' ';
  }
  return r;
}

// numeros con punto decimal sin importar la configuracion regional (Qt la toma del sistema)
static std::string dtos(double d)
{
  std::ostringstream os;
  os.imbue(std::locale::classic());
  os << std::setprecision(15) << d;
  return os.str();
}

static double stod_c(const std::string &s)
{
  std::istringstream is(trim(s));
  is.imbue(std::locale::classic());
  double d = 0;
  is >> d;
  return d;
}

// mensajes de error traducibles: Qt Linguist los encuentra por QT_TRANSLATE_NOOP("rlTagList", ...)
// y pvdevelop conecta rlTagTranslate con QCoreApplication::translate (rllib no usa Qt)
#ifndef QT_TRANSLATE_NOOP
#define QT_TRANSLATE_NOOP(context, text) text
#endif
std::string (*rlTagTranslate)(const char *text) = NULL;

static std::string M(const char *fmt, const std::string &a1 = std::string(),
                     const std::string &a2 = std::string(), const std::string &a3 = std::string())
{
  std::string s = (rlTagTranslate != NULL) ? rlTagTranslate(fmt) : std::string(fmt);
  const std::string *args[3] = { &a1, &a2, &a3 };
  for(int i=0; i<3; i++)
  {
    std::string key = "%" + itos(i + 1);
    size_t from = 0, p;
    while((p = s.find(key, from)) != std::string::npos)
    {
      s.replace(p, key.size(), *args[i]);
      from = p + args[i]->size();
    }
  }
  return s;
}

//------------------------------------------------------------------------------------------------
// historial y alarmas

rlTagHist::rlTagHist()
{
  log = -1; deadband = NAN; log_max = -1; hyst = NAN; priority = -1; ack = -1; event = -1;
}

bool rlTagHist::used() const
{
  return log >= 0 || !std::isnan(deadband) || log_max >= 0 || !unit.empty() || !alarm.empty() ||
         !std::isnan(hyst) || !text.empty() || !group.empty() || priority >= 0 || ack >= 0 || event >= 0;
}

void rlTagHist::inherit(const rlTagHist &from)
{
  if(log < 0) log = from.log;
  if(std::isnan(deadband)) deadband = from.deadband;
  if(log_max < 0) log_max = from.log_max;
  if(unit.empty()) unit = from.unit;
  if(group.empty()) group = from.group;
  if(priority < 0) priority = from.priority;
  if(ack < 0) ack = from.ack;
  if(event < 0) event = from.event;
}

int rlTagHist::set(const std::string &key, const std::string &val)
{
  if     (key == "log")        log = atoi(val.c_str()) ? 1 : 0;
  else if(key == "deadband")   deadband = val.empty() ? NAN : stod_c(val);
  else if(key == "log_max")    log_max = val.empty() ? -1 : atoi(val.c_str());
  else if(key == "unit")       unit = val;
  else if(key == "alarm")      alarm = trim(val);
  else if(key == "hyst")       hyst = val.empty() ? NAN : stod_c(val);
  else if(key == "alarm_text") text = val;
  else if(key == "group")      group = val;
  else if(key == "priority")   priority = val.empty() ? -1 : atoi(val.c_str());
  else if(key == "ack")        ack = atoi(val.c_str()) ? 1 : 0;
  else if(key == "event")      event = atoi(val.c_str()) ? 1 : 0;
  else return 0;
  return 1;
}

static std::string quoteIfNeeded(const std::string &s);

std::string rlTagHist::pairs() const
{
  std::string s;
  if(log >= 0)              s += " log=" + itos(log);
  if(!std::isnan(deadband)) s += " deadband=" + dtos(deadband);
  if(log_max >= 0)          s += " log_max=" + itos(log_max);
  if(!unit.empty())         s += " unit=" + quoteIfNeeded(unit);
  if(!alarm.empty())        s += " alarm=" + quoteIfNeeded(alarm);
  if(!std::isnan(hyst))     s += " hyst=" + dtos(hyst);
  if(!text.empty())         s += " alarm_text=" + quoteIfNeeded(text);
  if(!group.empty())        s += " group=" + quoteIfNeeded(group);
  if(priority >= 0)         s += " priority=" + itos(priority);
  if(ack >= 0)              s += " ack=" + itos(ack);
  if(event >= 0)            s += " event=" + itos(event);
  return s;
}

bool rlTagAlarmCond::active(double v, bool wasActive, double h) const
{
  if(std::isnan(v)) return wasActive;
  if(std::isnan(h) || h < 0) h = 0;
  if(!wasActive) h = 0; // para activarse no hay histeresis
  switch(op)
  {
    case OP_GT:  return v >  v1 - h;
    case OP_GE:  return v >= v1 - h;
    case OP_LT:  return v <  v1 + h;
    case OP_LE:  return v <= v1 + h;
    case OP_EQ:  return v == v1;   // = y != no usan histeresis (bool, estados)
    case OP_NE:  return v != v1;
    case OP_IN:  return v >= v1 - h && v <= v2 + h;
    case OP_OUT: return v < v1 + h || v > v2 - h;
  }
  return false;
}

int rlTagList::parseAlarm(const std::string &cond0, rlTagAlarmCond &c)
{
  std::string s = lower(trim(cond0));
  c = rlTagAlarmCond();
  if(s.empty()) return 0;
  std::string num;
  if(startsWith(s, "in:") || startsWith(s, "out:"))
  {
    c.op = (s[0] == 'i') ? rlTagAlarmCond::OP_IN : rlTagAlarmCond::OP_OUT;
    std::string r = s.substr(s.find(':') + 1);
    size_t dots = r.find("..");
    if(dots == std::string::npos) return -1;
    std::string a = trim(r.substr(0, dots)), b = trim(r.substr(dots + 2));
    char *end;
    c.v1 = strtod(a.c_str(), &end); if(a.empty() || *end) return -1;
    c.v2 = strtod(b.c_str(), &end); if(b.empty() || *end) return -1;
    if(c.v2 < c.v1) std::swap(c.v1, c.v2);
    return 0;
  }
  if     (startsWith(s, ">=")) { c.op = rlTagAlarmCond::OP_GE; num = s.substr(2); }
  else if(startsWith(s, "<=")) { c.op = rlTagAlarmCond::OP_LE; num = s.substr(2); }
  else if(startsWith(s, "!=") || startsWith(s, "<>")) { c.op = rlTagAlarmCond::OP_NE; num = s.substr(2); }
  else if(startsWith(s, "==")) { c.op = rlTagAlarmCond::OP_EQ; num = s.substr(2); }
  else if(startsWith(s, ">"))  { c.op = rlTagAlarmCond::OP_GT; num = s.substr(1); }
  else if(startsWith(s, "<"))  { c.op = rlTagAlarmCond::OP_LT; num = s.substr(1); }
  else if(startsWith(s, "="))  { c.op = rlTagAlarmCond::OP_EQ; num = s.substr(1); }
  else                         { c.op = rlTagAlarmCond::OP_EQ; num = s; }  // "1" / "0": bool
  num = trim(num);
  std::istringstream is(num);
  is.imbue(std::locale::classic());
  double d;
  if(num.empty() || !(is >> d) || !is.eof()) { c.op = rlTagAlarmCond::OP_NONE; return -1; }
  c.v1 = d;
  return 0;
}

rlTagDef::rlTagDef()
{
  type = 0; count = 1; stride = 0; rw = 0; slave = 1; scan = 0; scale = 1.0; scale_offset = 0.0; order = -1;
  area = 0; dbnum = 0; addr = 0; bit = -1;
  group = -1;
  size = 0; offset = -1; status = -1; block = -1;
}

std::string rlTagDef::opt(const char *key, const char *def) const
{
  for(size_t i=0; i<opts.size(); i++) if(opts[i].first == key) return opts[i].second;
  return def;
}

void rlTagDef::setOpt(const std::string &key, const std::string &val)
{
  for(size_t i=0; i<opts.size(); i++) if(opts[i].first == key) { opts[i].second = val; return; }
  opts.push_back(std::make_pair(key, val));
}

const std::vector<std::string> &rlTagList::mqttOptionNames()
{
  static const std::vector<std::string> names{ "cmd_topic", "format", "json", "qos", "retain", "stale_s" };
  return names;
}

rlTagMember::rlTagMember()
{
  type = 0; count = 1; rw = -1; scale = 1.0; scale_offset = 0.0; order = -1;
  pos = 0; bit = -1; units = 0;
}

rlTagList::rlTagList(int _protocol)
{
  protocol = _protocol;
  notation = (protocol == SIEMENS) ? S7_GERMAN : MODICON;
  order = RLTAG_ABCD;
  max_gap = -1;
  max_block = -1;
  bit_write_fc22 = 0;
  console = CONSOLE_NORMAL;
  console_interval = 10;
  sys_base = -1;
  currentStruct = -1;
}

int rlTagList::typeFromString(const std::string &s0)
{
  std::string s = lower(trim(s0));
  if(s == "bool" || s == "bit" || s == "boolean") return RLTAG_BOOL;
  if(s == "byte" || s == "usint")                 return RLTAG_BYTE;
  if(s == "int"  || s == "int16" || s == "short") return RLTAG_INT;
  if(s == "uint" || s == "word" || s == "uint16") return RLTAG_UINT;
  if(s == "dint" || s == "int32")                 return RLTAG_DINT;
  if(s == "udint"|| s == "dword"|| s == "uint32") return RLTAG_UDINT;
  if(s == "real" || s == "float")                 return RLTAG_REAL;
  if(s == "lreal"|| s == "double")                return RLTAG_LREAL;
  return 0;
}

const char *rlTagList::typeName(int type)
{
  switch(type)
  {
    case RLTAG_BOOL:  return "bool";
    case RLTAG_BYTE:  return "byte";
    case RLTAG_INT:   return "int";
    case RLTAG_UINT:  return "uint";
    case RLTAG_DINT:  return "dint";
    case RLTAG_UDINT: return "udint";
    case RLTAG_REAL:  return "real";
    case RLTAG_LREAL: return "lreal";
    default:          return "";
  }
}

int rlTagList::orderFromString(const std::string &s0)
{
  std::string s = upper(trim(s0));
  if(s == "CDAB") return RLTAG_CDAB;
  if(s == "BADC") return RLTAG_BADC;
  if(s == "DCBA") return RLTAG_DCBA;
  if(s == "ABCD") return RLTAG_ABCD;
  return -1;
}

const char *rlTagList::orderName(int order)
{
  switch(order)
  {
    case RLTAG_CDAB: return "CDAB";
    case RLTAG_BADC: return "BADC";
    case RLTAG_DCBA: return "DCBA";
    default:         return "ABCD";
  }
}

int rlTagList::notationFromString(const std::string &s0)
{
  std::string s = lower(trim(s0));
  if(s == "modicon") return MODICON;
  if(s == "offset")  return OFFSET;
  if(s == "plc")     return PLC;
  if(s == "german" || s == "aleman")  return S7_GERMAN;
  if(s == "english"|| s == "ingles")  return S7_ENGLISH;
  return -1;
}

const char *rlTagList::notationName(int notation)
{
  switch(notation)
  {
    case OFFSET:     return "offset";
    case PLC:        return "plc";
    case S7_GERMAN:  return "german";
    case S7_ENGLISH: return "english";
    default:         return "modicon";
  }
}

bool rlTagList::validName(const std::string &s)
{
  if(s.empty()) return false;
  if(!(isalpha((unsigned char) s[0]) || s[0] == '_')) return false;
  for(size_t i=0; i<s.size(); i++)
  {
    if(!(isalnum((unsigned char) s[i]) || s[i] == '_')) return false;
  }
  return true;
}

// Nestor: nombres que son macros en los encabezados que incluyen el daemon y el pvserver (rlserial.h:
// B0..B4000000; termios.h en Linux/macOS; windows.h): como nombre de tag el codigo generado no compila
bool rlTagList::reservedName(const std::string &s)
{
  static const char *names[] = {
    // velocidades (rlserial.h y termios.h)
    "B0", "B50", "B75", "B110", "B134", "B150", "B200", "B300", "B600", "B1200", "B1800", "B2400", "B4800",
    "B7200", "B9600", "B14400", "B19200", "B28800", "B38400", "B57600", "B76800", "B115200", "B230400",
    "B460800", "B500000", "B576000", "B921600", "B1000000", "B1152000", "B1500000", "B2000000", "B2500000",
    "B3000000", "B3500000", "B4000000",
    // termios.h
    "CS5", "CS6", "CS7", "CS8", "CSIZE", "CSTOPB", "CREAD", "PARENB", "PARODD", "HUPCL", "CLOCAL", "CRTSCTS",
    "ECHO", "ECHOE", "ECHOK", "ECHONL", "ECHOCTL", "ECHOPRT", "ECHOKE", "ICANON", "ISIG", "IEXTEN", "NOFLSH", "TOSTOP",
    "VMIN", "VTIME", "VINTR", "VQUIT", "VERASE", "VKILL", "VEOF", "VEOL", "VSTART", "VSTOP", "VSUSP",
    "IGNBRK", "BRKINT", "IGNPAR", "PARMRK", "INPCK", "ISTRIP", "INLCR", "IGNCR", "ICRNL", "IXON", "IXOFF", "IXANY",
    "OPOST", "ONLCR", "OCRNL", "ONOCR", "ONLRET", "NCCS", "TCSANOW", "TCSADRAIN", "TCSAFLUSH",
    // windows.h y C
    "ERROR", "IN", "OUT", "OPTIONAL", "DELETE", "TRUE", "FALSE", "NULL", "EOF", "near", "far", "NEAR", "FAR",
    "CONST", "VOID", "PASCAL", "CALLBACK", "WINAPI", "APIENTRY", "interface", "small", "hyper",
    "BUFSIZ", "FILENAME_MAX", "EXIT_SUCCESS", "EXIT_FAILURE", "RAND_MAX", "INFINITY", "NAN",
    "errno", "stdin", "stdout", "stderr", "assert", "unix", "linux", "WIN32", "_WIN32",
  };
  for(const char *n : names) if(s == n) return true;
  return false;
}

std::vector<std::string> rlTagList::splitCsvLine(const std::string &line, char sep)
{
  std::vector<std::string> out;
  std::string cur;
  bool quoted = false;
  for(size_t i=0; i<line.size(); i++)
  {
    char c = line[i];
    if(quoted)
    {
      if(c == '"')
      {
        if(i+1 < line.size() && line[i+1] == '"') { cur += '"'; i++; }
        else quoted = false;
      }
      else cur += c;
    }
    else if(c == '"') quoted = true;
    else if(c == sep) { out.push_back(trim(cur)); cur.clear(); }
    else if(c != '\r' && c != '\n') cur += c;
  }
  out.push_back(trim(cur));
  return out;
}

//------------------------------------------------------------------------------------------------
// direcciones

int rlTagList::parseModbusAddress(const std::string &text, rlTagDef &t, std::string &err) const
{
  std::string s = upper(trim(text));
  std::string num;
  int defaultType = 0;
  t.bit = -1;
  if(s.empty()) { err = M(QT_TRANSLATE_NOOP("rlTagList", "falta la direccion")); return -1; }

  // bit: 40300.15  HR299.15  %MW299.15  %MW299:X15
  size_t p = s.find(":X");
  if(p == std::string::npos) p = s.rfind('.');
  if(p != std::string::npos)
  {
    std::string b = s.substr(p + ((s[p] == ':') ? 2 : 1));
    t.bit = toNumber(b);
    if(t.bit < 0 || t.bit > 15) { err = M(QT_TRANSLATE_NOOP("rlTagList", "bit invalido en %1 (0..15)"), text); return -1; }
    s = s.substr(0, p);
  }

  if(s[0] == '%')
  {
    if     (startsWith(s, "%MW")) { t.area = RLTAG_HOLDING;  num = s.substr(3); }
    else if(startsWith(s, "%MD")) { t.area = RLTAG_HOLDING;  num = s.substr(3); defaultType = RLTAG_DINT; }
    else if(startsWith(s, "%MF")) { t.area = RLTAG_HOLDING;  num = s.substr(3); defaultType = RLTAG_REAL; }
    else if(startsWith(s, "%IW")) { t.area = RLTAG_INPUT;    num = s.substr(3); }
    else if(startsWith(s, "%M"))  { t.area = RLTAG_COIL;     num = s.substr(2); }
    else if(startsWith(s, "%I"))  { t.area = RLTAG_DISCRETE; num = s.substr(2); }
    else { err = M(QT_TRANSLATE_NOOP("rlTagList", "direccion PLC desconocida: %1 (%MW, %MD, %MF, %IW, %M, %I)"), text); return -1; }
    t.addr = toNumber(num);
  }
  else if(isalpha((unsigned char) s[0]))
  {
    if     (startsWith(s, "HR")) t.area = RLTAG_HOLDING;
    else if(startsWith(s, "IR")) t.area = RLTAG_INPUT;
    else if(startsWith(s, "CO")) t.area = RLTAG_COIL;
    else if(startsWith(s, "DI")) t.area = RLTAG_DISCRETE;
    else { err = M(QT_TRANSLATE_NOOP("rlTagList", "direccion desconocida: %1 (HR, IR, CO, DI)"), text); return -1; }
    t.addr = toNumber(s.substr(2));
  }
  else if(notation == MODICON)
  {
    // 40001 = primer holding register; 6 digitos para direcciones > 9999 (400001)
    if(s.size() != 5 && s.size() != 6) { err = M(QT_TRANSLATE_NOOP("rlTagList", "direccion Modicon invalida: %1 (5 o 6 digitos, p. ej. 40001)"), text); return -1; }
    switch(s[0])
    {
      case '0': t.area = RLTAG_COIL;     break;
      case '1': t.area = RLTAG_DISCRETE; break;
      case '3': t.area = RLTAG_INPUT;    break;
      case '4': t.area = RLTAG_HOLDING;  break;
      default: err = M(QT_TRANSLATE_NOOP("rlTagList", "direccion Modicon invalida: %1 (0xxxx, 1xxxx, 3xxxx o 4xxxx)"), text); return -1;
    }
    t.addr = toNumber(s.substr(1));
    if(t.addr >= 0) t.addr -= 1;
  }
  else
  {
    // offset / plc sin prefijo: holding register (base 0)
    t.area = RLTAG_HOLDING;
    t.addr = toNumber(s);
  }
  if(t.addr < 0 || t.addr > 65535) { err = M(QT_TRANSLATE_NOOP("rlTagList", "direccion invalida: %1"), text); return -1; }
  if(t.bit >= 0 && (t.area == RLTAG_COIL || t.area == RLTAG_DISCRETE))
  {
    err = M(QT_TRANSLATE_NOOP("rlTagList", "un coil o una entrada discreta no tiene bits: %1"), text);
    return -1;
  }
  if(t.type == 0)
  {
    if(t.area == RLTAG_COIL || t.area == RLTAG_DISCRETE || t.bit >= 0) t.type = RLTAG_BOOL;
    else if(defaultType != 0) t.type = defaultType;
    else t.type = RLTAG_INT;
  }
  return 0;
}

int rlTagList::parseSiemensAddress(const std::string &text, rlTagDef &t, std::string &err) const
{
  std::string s = upper(trim(text));
  char width = 'X';
  t.bit = -1;
  t.dbnum = 0;
  if(s.empty()) { err = M(QT_TRANSLATE_NOOP("rlTagList", "falta la direccion")); return -1; }

  if(startsWith(s, "DB"))
  {
    // DB10.DBX4.3  DB10.DBB4  DB10.DBW4  DB10.DBD4
    size_t p = s.find('.');
    if(p == std::string::npos) { err = M(QT_TRANSLATE_NOOP("rlTagList", "direccion DB invalida: %1 (p. ej. DB10.DBW4)"), text); return -1; }
    t.dbnum = toNumber(s.substr(2, p - 2));
    s = s.substr(p + 1);
    if(!startsWith(s, "DB") || s.size() < 4) { err = M(QT_TRANSLATE_NOOP("rlTagList", "direccion DB invalida: %1 (p. ej. DB10.DBX4.3)"), text); return -1; }
    width = s[2];
    s = s.substr(3);
    t.area = S7_ORG_DB;
    if(t.dbnum < 0) { err = M(QT_TRANSLATE_NOOP("rlTagList", "numero de DB invalido: %1"), text); return -1; }
  }
  else
  {
    // M10.3 MB10 MW10 MD10 ; E/I (entradas) ; A/Q (salidas)
    switch(s[0])
    {
      case 'M':           t.area = S7_ORG_M; break;
      case 'E': case 'I': t.area = S7_ORG_E; break;
      case 'A': case 'Q': t.area = S7_ORG_A; break;
      default: err = M(QT_TRANSLATE_NOOP("rlTagList", "direccion S7 desconocida: %1 (DB, M, E/I, A/Q)"), text); return -1;
    }
    s = s.substr(1);
    if(!s.empty() && (s[0] == 'B' || s[0] == 'W' || s[0] == 'D' || s[0] == 'X'))
    {
      width = s[0];
      s = s.substr(1);
    }
  }

  size_t p = s.find('.');
  if(width == 'X')
  {
    if(p == std::string::npos) { err = M(QT_TRANSLATE_NOOP("rlTagList", "falta el bit en %1 (p. ej. M10.3)"), text); return -1; }
    t.bit = toNumber(s.substr(p + 1));
    s = s.substr(0, p);
    if(t.bit < 0 || t.bit > 7) { err = M(QT_TRANSLATE_NOOP("rlTagList", "bit invalido en %1 (0..7)"), text); return -1; }
  }
  else if(p != std::string::npos) { err = M(QT_TRANSLATE_NOOP("rlTagList", "un byte, palabra o doble palabra no lleva bit: %1"), text); return -1; }
  t.addr = toNumber(s);
  if(t.addr < 0 || t.addr > 65535) { err = M(QT_TRANSLATE_NOOP("rlTagList", "direccion invalida: %1"), text); return -1; }
  if(t.type == 0)
  {
    switch(width)
    {
      case 'B': t.type = RLTAG_BYTE; break;
      case 'W': t.type = RLTAG_INT;  break;
      case 'D': t.type = RLTAG_DINT; break;
      default:  t.type = RLTAG_BOOL; break;
    }
  }
  int need = (width == 'B') ? 1 : (width == 'W') ? 2 : (width == 'D') ? 4 : 0;
  int have = (t.type == RLTAG_BYTE) ? 1 : (t.type == RLTAG_INT || t.type == RLTAG_UINT) ? 2 :
             (t.type == RLTAG_LREAL) ? 8 : (t.type == RLTAG_BOOL) ? 0 : 4;
  if(width != 'X' && t.type != RLTAG_BOOL && need != have)
  {
    err = M(QT_TRANSLATE_NOOP("rlTagList", "el tipo %1 no coincide con el tamanio de %2"), typeName(t.type), text);
    return -1;
  }
  if(width == 'X' && t.type != RLTAG_BOOL) { err = M(QT_TRANSLATE_NOOP("rlTagList", "una direccion de bit es de tipo bool: %1"), text); return -1; }
  return 0;
}

int rlTagList::parseAddress(const std::string &text, rlTagDef &t, std::string &err) const
{
  if(protocol == MODBUS)  return parseModbusAddress(text, t, err);
  if(protocol == SIEMENS) return parseSiemensAddress(text, t, err);
  if(protocol == MQTT)
  {
    // Nestor: topico MQTT (sin comodines: un tag es un topico)
    std::string s = trim(text);
    if(s.empty()) { err = M(QT_TRANSLATE_NOOP("rlTagList", "falta el topico")); return -1; }
    if(s.find_first_of("+#") != std::string::npos || s.find_first_of(" \t") != std::string::npos)
    { err = M(QT_TRANSLATE_NOOP("rlTagList", "topico invalido: %1 (sin + ni # ni espacios)"), text); return -1; }
    t.area = 0; t.addr = 0; t.bit = -1;
    return 0;
  }
  // OPC UA: NodeId (ns=2;s=Motor.Velocidad | ns=3;i=1001 | i=2258)
  std::string s = trim(text);
  if(s.empty()) { err = M(QT_TRANSLATE_NOOP("rlTagList", "falta el NodeId")); return -1; }
  if(s.find("i=") == std::string::npos && s.find("s=") == std::string::npos &&
     s.find("g=") == std::string::npos && s.find("b=") == std::string::npos)
  {
    err = M(QT_TRANSLATE_NOOP("rlTagList", "NodeId invalido: %1 (p. ej. ns=2;s=Motor.Velocidad o ns=3;i=1001)"), text);
    return -1;
  }
  t.area = 0;
  t.addr = 0;
  t.bit  = -1;
  return 0;
}

std::string rlTagList::formatAddress(const rlTagDef &t, int not_) const
{
  std::string s;
  if(textAddress()) return t.address;
  if(protocol == SIEMENS)
  {
    const char *w = (t.type == RLTAG_BOOL && t.bit >= 0) ? "X" :
                    (t.type == RLTAG_BYTE) ? "B" :
                    (t.type == RLTAG_INT || t.type == RLTAG_UINT) ? "W" : "D";
    bool en = (not_ == S7_ENGLISH);
    if(t.area == S7_ORG_DB) s = "DB" + itos(t.dbnum) + ".DB" + w + itos(t.addr);
    else
    {
      s = (t.area == S7_ORG_M) ? "M" : (t.area == S7_ORG_E) ? (en ? "I" : "E") : (en ? "Q" : "A");
      if(w[0] != 'X') s += w;
      s += itos(t.addr);
    }
    if(w[0] == 'X') s += "." + itos(t.bit);
    return s;
  }
  // MODBUS
  bool reg = (t.area == RLTAG_HOLDING || t.area == RLTAG_INPUT);
  if(not_ == PLC)
  {
    switch(t.area)
    {
      case RLTAG_COIL:     s = "%M"; break;
      case RLTAG_DISCRETE: s = "%I"; break;
      case RLTAG_INPUT:    s = "%IW"; break;
      default:             s = (t.type == RLTAG_REAL && t.bit < 0) ? "%MF" :
                               ((t.type == RLTAG_DINT || t.type == RLTAG_UDINT) && t.bit < 0) ? "%MD" : "%MW"; break;
    }
    s += itos(t.addr);
  }
  else if(not_ == OFFSET)
  {
    s = (t.area == RLTAG_COIL) ? "CO" : (t.area == RLTAG_DISCRETE) ? "DI" : (t.area == RLTAG_INPUT) ? "IR" : "HR";
    s += itos(t.addr);
  }
  else
  {
    char buf[32];
    int d = (t.area == RLTAG_COIL) ? 0 : (t.area == RLTAG_DISCRETE) ? 1 : (t.area == RLTAG_INPUT) ? 3 : 4;
    if(t.addr + 1 <= 9999) sprintf(buf, "%d%04d", d, t.addr + 1);
    else                   sprintf(buf, "%d%05d", d, t.addr + 1);
    s = buf;
  }
  if(reg && t.bit >= 0) s += "." + itos(t.bit);
  return s;
}

std::string rlTagList::blockLabel(const rlTagBlock &b) const
{
  rlTagDef t;
  bool bits = (protocol == MODBUS && (b.area == RLTAG_COIL || b.area == RLTAG_DISCRETE));
  t.type = bits ? RLTAG_BOOL : (protocol == MODBUS) ? RLTAG_INT : RLTAG_BYTE;
  t.bit = -1; t.area = b.area; t.dbnum = b.dbnum;
  t.addr = b.start;
  std::string s = formatAddress(t, notation);
  if(b.count <= 1) return s;
  t.addr = b.start + b.count - 1;
  return s + ".." + formatAddress(t, notation);
}

int rlTagList::valueSize(const rlTagDef &t) const
{
  switch(t.type)
  {
    case RLTAG_BOOL:
      if(protocol == MODBUS && (t.area == RLTAG_HOLDING || t.area == RLTAG_INPUT)) return 2;
      return 1;
    case RLTAG_BYTE:  return 1;
    case RLTAG_INT:
    case RLTAG_UINT:  return 2;
    case RLTAG_DINT:
    case RLTAG_UDINT:
    case RLTAG_REAL:  return 4;
    case RLTAG_LREAL: return 8;
    default:          return 0;
  }
}

int rlTagList::check(rlTagDef &t, std::string &err) const
{
  // los de "flat" (Temp[3], Bomba.Marcha) se revisaron al expandir
  if(t.group < 0 && !validName(t.name)) { err = M(QT_TRANSLATE_NOOP("rlTagList", "nombre invalido: \"%1\" (letras, numeros y _; no empieza con numero)"), t.name); return -1; }
  if(parseAddress(t.address, t, err) < 0) { err = t.name + ": " + err; return -1; }
  if(t.type == 0) { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1: falta el tipo (bool, int, uint, dint, udint, real, lreal)"), t.name); return -1; }
  if(protocol == MODBUS)
  {
    if((t.area == RLTAG_COIL || t.area == RLTAG_DISCRETE) && t.type != RLTAG_BOOL)
    { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1: un coil o una entrada discreta es de tipo bool"), t.name); return -1; }
    if(t.type == RLTAG_BYTE) { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1: Modbus no tiene tipo byte (usar int o uint)"), t.name); return -1; }
    if(t.rw && (t.area == RLTAG_DISCRETE || t.area == RLTAG_INPUT))
    { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1: las entradas discretas y los input registers no se pueden escribir (rw=0)"), t.name); return -1; }
    if(t.slave < 0 || t.slave > 255) { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1: esclavo invalido (0..255)"), t.name); return -1; }
  }
  if(protocol == SIEMENS && (t.slave < 0 || t.slave > 255)) { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1: numero de PLC invalido (slave=0..255)"), t.name); return -1; }
  if(t.order < 0 || protocol != MODBUS) t.order = (protocol == MODBUS) ? order : RLTAG_ABCD;
  if(t.scan < 0) t.scan = 0;
  t.size = valueSize(t);
  rlTagAlarmCond c;
  if(parseAlarm(t.hist.alarm, c) < 0)
  { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1: condicion de alarma invalida \"%2\" (>80, <=10, =1, !=0, in:10..20, out:10..20)"), t.name, t.hist.alarm); return -1; }
  if(t.hist.priority > 4) { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1: prioridad invalida (1..4, 0 = sin prioridad)"), t.name); return -1; }
  if(t.hist.event > 0 && t.type != RLTAG_BOOL) { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1: event=1 es para variables bool"), t.name); return -1; }
  if(protocol == MQTT)
  {
    std::string f = lower(t.opt("format", "value"));
    if(f != "value" && f != "json") { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1: format invalido %2 (value o json)"), t.name, f); return -1; }
    std::string q = t.opt("qos", "1");
    if(q != "0" && q != "1" && q != "2") { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1: qos invalido %2 (0, 1 o 2)"), t.name, q); return -1; }
    std::string c = t.opt("cmd_topic");
    if(c.find_first_of("+# \t") != std::string::npos) { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1: cmd_topic invalido: %2"), t.name, c); return -1; }
    if(atoi(t.opt("stale_s", "0").c_str()) < 0) { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1: stale_s invalido"), t.name); return -1; }
  }
  return 0;
}

//------------------------------------------------------------------------------------------------
// lectura y escritura

int rlTagList::parseSetting(const char *cline)
{
  std::string line = trim(cline);
  size_t hash = line.find('#');
  if(hash != std::string::npos) line = trim(line.substr(0, hash));
  size_t eq = line.find('=');
  if(eq == std::string::npos) return 0;
  std::string key = lower(trim(line.substr(0, eq)));
  std::string val = trim(line.substr(eq + 1));
  if(key == "address_notation")
  {
    int n = notationFromString(val);
    if(n < 0) errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "address_notation desconocida: %1"), val));
    else      notation = n;
    return 1;
  }
  if(key == "word_order")
  {
    int o = orderFromString(val);
    if(o < 0) errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "word_order desconocido: %1 (ABCD, CDAB, BADC, DCBA)"), val));
    else      order = o;
    return 1;
  }
  if(key == "bit_write") { bit_write_fc22 = (lower(val) == "fc22") ? 1 : 0; return 1; }
  if(key == "max_gap")   { max_gap   = atoi(val.c_str()); return 1; }
  if(key == "max_block") { max_block = atoi(val.c_str()); return 1; }
  if(key == "tags")      { csvfile   = val; return 1; }
  if(key == "structs")   { structsFile = val; return 1; }   // Nestor: estructuras compartidas (las lee readMkFile)
  if(key == "console")
  {
    int c = consoleFromString(val);
    if(c < 0) errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "console desconocido: %1 (quiet, normal, verbose)"), val));
    else      console = c;
    return 1;
  }
  if(key == "console_interval") { console_interval = atoi(val.c_str()); if(console_interval < 1) console_interval = 1; return 1; }
  return 0;
}

// clave=valor separados por espacios; los valores pueden ir entre comillas ("" = una comilla)
std::vector< std::pair<std::string, std::string> > rlTagList::parsePairs(const std::string &line, const std::string &where)
{
  std::string err;
  std::vector< std::pair<std::string, std::string> > out = splitPairs(line, err);
  if(!err.empty()) errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "%1: se esperaba clave=valor"), where));
  return out;
}

std::vector< std::pair<std::string, std::string> > rlTagList::splitPairs(const std::string &line, std::string &err)
{
  std::vector< std::pair<std::string, std::string> > out;
  err.clear();
  size_t i = 0;
  while(i < line.size())
  {
    while(i < line.size() && isspace((unsigned char) line[i])) i++;
    if(i >= line.size() || line[i] == '#') break;
    size_t eq = line.find('=', i);
    if(eq == std::string::npos) { err = line.substr(i); break; }
    std::string key = lower(trim(line.substr(i, eq - i)));
    std::string val;
    i = eq + 1;
    if(i < line.size() && line[i] == '"')
    {
      i++;
      while(i < line.size())
      {
        if(line[i] == '"')
        {
          if(i+1 < line.size() && line[i+1] == '"') { val += '"'; i += 2; continue; }
          i++;
          break;
        }
        val += line[i++];
      }
    }
    else
    {
      while(i < line.size() && !isspace((unsigned char) line[i])) val += line[i++];
    }
    out.push_back(std::make_pair(key, val));
  }
  return out;
}

// tipo simple o nombre de estructura
static void setType(rlTagDef &t, const std::string &val)
{
  t.type = rlTagList::typeFromString(val);
  t.structName = (t.type == 0) ? trim(val) : std::string();
}

int rlTagList::parseTagLine(const char *cline, const std::string &where)
{
  std::string line = trim(cline);
  if(startsWith(line, "tag")) line = line.substr(3);
  rlTagDef t;
  if(protocol == SIEMENS) t.slave = 0; // en S7 el primer PLC suele ser slave=0
  size_t nerr = errors.size();
  std::vector< std::pair<std::string, std::string> > p = parsePairs(line, where);
  if(errors.size() != nerr) return -1;
  for(size_t i=0; i<p.size(); i++)
  {
    const std::string &key = p[i].first, &val = p[i].second;
    if     (key == "name")    t.name = val;
    else if(key == "address" || key == "adr" || (key == "topic" && protocol == MQTT)) t.address = val;
    else if(protocol == MQTT && std::find(mqttOptionNames().begin(), mqttOptionNames().end(), key) != mqttOptionNames().end()) t.setOpt(key, val);
    else if(key == "type")    setType(t, val);
    else if(key == "count")   t.count  = atoi(val.c_str());
    else if(key == "stride")  t.stride = atoi(val.c_str());
    else if(key == "rw")      t.rw    = atoi(val.c_str()) ? 1 : 0;
    else if(key == "slave" || key == "plc") t.slave = atoi(val.c_str());
    else if(key == "scan")    t.scan  = atoi(val.c_str());
    else if(key == "scale")   t.scale = stod_c(val);
    else if(key == "offset")  t.scale_offset = stod_c(val);
    else if(key == "order")
    {
      t.order = orderFromString(val);   // Nestor: antes un orden invalido se ignoraba sin aviso
      if(t.order < 0) errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "%1: order desconocido %2 (ABCD, CDAB, BADC, DCBA)"), where, val));
    }
    else if(key == "desc" || key == "description") t.description = val;
    else if(t.hist.set(key, val)) ;
    else { errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "%1: clave desconocida %2"), where, key)); return -1; }
  }
  tags.push_back(t);
  return 0;
}

int rlTagList::parseLine(const char *cline, const std::string &where)
{
  std::string line = trim(cline);
  if(line.empty() || line[0] == '#') return 0;
  if(startsWith(line, "sys ") || startsWith(line, "sys\t"))
  {
    // historial y alarmas de un tag de sistema: sys name=Comunicacion alarm=0 ...
    std::string name;
    rlTagHist h;
    std::vector< std::pair<std::string, std::string> > p = parsePairs(line.substr(3), where);
    for(size_t i=0; i<p.size(); i++)
    {
      if(p[i].first == "name") name = p[i].second;
      else if(!h.set(p[i].first, p[i].second)) errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "%1: clave desconocida %2"), where, p[i].first));
    }
    if(name.empty()) errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "%1: falta name"), where));
    else
    {
      size_t k = 0;
      while(k < sysHist.size() && sysHist[k].first != name) k++;
      if(k < sysHist.size()) sysHist[k].second = h;
      else sysHist.push_back(std::make_pair(name, h));
    }
    return 1;
  }
  if(startsWith(line, "tag ") || startsWith(line, "tag\t"))
  {
    parseTagLine(line.c_str(), where);
    return 1;
  }
  // Nestor: MQTT: "publish ..." y "lwt ..." los lee el daemon (rlmqtt), no son tags
  if(protocol == MQTT && (startsWith(line, "publish ") || startsWith(line, "publish\t") ||
                          startsWith(line, "lwt ") || startsWith(line, "lwt\t"))) return 0;
  if(startsWith(line, "struct ") || startsWith(line, "struct\t"))
  {
    rlTagStruct s;
    s.units = 0;
    std::vector< std::pair<std::string, std::string> > p = parsePairs(line.substr(6), where);
    for(size_t i=0; i<p.size(); i++)
    {
      if     (p[i].first == "name") s.name = p[i].second;
      else if(p[i].first == "desc" || p[i].first == "description") s.description = p[i].second;
      else errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "%1: clave desconocida %2"), where, p[i].first));
    }
    structs.push_back(s);
    currentStruct = (int) structs.size() - 1;
    return 1;
  }
  if(startsWith(line, "member ") || startsWith(line, "member\t"))
  {
    if(currentStruct < 0) { errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "%1: member fuera de un struct"), where)); return 1; }
    rlTagMember m;
    std::vector< std::pair<std::string, std::string> > p = parsePairs(line.substr(6), where);
    for(size_t i=0; i<p.size(); i++)
    {
      const std::string &key = p[i].first, &val = p[i].second;
      if     (key == "name")   m.name = val;
      else if(key == "at")     m.at = val;
      else if(key == "type")
      {
        m.type = typeFromString(val);
        if(m.type == 0) errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "%1: tipo de miembro desconocido %2 (un miembro es de un tipo simple)"), where, val));
      }
      else if(key == "count")  m.count = atoi(val.c_str());
      else if(key == "rw")     m.rw = atoi(val.c_str()) ? 1 : 0;
      else if(key == "scale")  m.scale = stod_c(val);
      else if(key == "offset") m.scale_offset = stod_c(val);
      else if(key == "order")
      {
        m.order = orderFromString(val);
        if(m.order < 0) errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "%1: order desconocido %2 (ABCD, CDAB, BADC, DCBA)"), where, val));
      }
      else if(key == "desc" || key == "description") m.description = val;
      else if(m.hist.set(key, val)) ;
      else errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "%1: clave desconocida %2"), where, key));
    }
    structs[currentStruct].members.push_back(m);
    return 1;
  }
  if(line == "end" || startsWith(line, "end ") || startsWith(line, "end\t") || startsWith(line, "end#"))
  {
    currentStruct = -1;
    return 1;
  }
  return parseSetting(line.c_str());
}

int rlTagList::readCsv(const char *filename)
{
  FILE *fp = fopen(filename, "r");
  if(fp == NULL) { errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "no se puede abrir %1"), filename)); return -1; }
  char buf[4096];
  enum { NAME, ADDRESS, TYPE, COUNT, STRIDE, RW, SLAVE, SCAN, SCALE, OFFSET, ORDER, DESC, NCOL };
  std::vector<int> col(NCOL, -1);
  std::vector< std::pair<int, std::string> > histcol; // columnas del historiador: indice, clave
  std::vector< std::pair<int, std::string> > optcol;  // Nestor: columnas de opciones MQTT (cmd_topic, format...)
  char sep = ';';
  int lineno = 0;
  bool header = true;
  while(fgets(buf, sizeof(buf) - 1, fp) != NULL)
  {
    lineno++;
    std::string line = buf;
    if(lineno == 1 && line.size() >= 3 && (unsigned char) line[0] == 0xEF && (unsigned char) line[1] == 0xBB && (unsigned char) line[2] == 0xBF)
      line = line.substr(3); // BOM de UTF-8 (Excel)
    if(trim(line).empty() || trim(line)[0] == '#') continue;
    if(header)
    {
      sep = (std::count(line.begin(), line.end(), ';') >= std::count(line.begin(), line.end(), ',')) ? ';' : ',';
      std::vector<std::string> h = splitCsvLine(line, sep);
      for(size_t i=0; i<h.size(); i++)
      {
        std::string k = lower(h[i]);
        int c = -1;
        if(k == "name" || k == "nombre")                                c = NAME;
        else if(k == "address" || k == "direccion" || k == "nodeid" ||
                (k == "topic" && protocol == MQTT))                     c = ADDRESS;
        else if(k == "type" || k == "tipo")                             c = TYPE;
        else if(k == "count" || k == "cantidad")                        c = COUNT;
        else if(k == "stride" || k == "paso")                           c = STRIDE;
        else if(k == "rw")                                              c = RW;
        else if(k == "slave" || k == "esclavo" || k == "plc")           c = SLAVE;
        else if(k == "scan")                                            c = SCAN;
        else if(k == "scale" || k == "escala")                          c = SCALE;
        else if(k == "offset")                                          c = OFFSET;
        else if(k == "order" || k == "orden")                           c = ORDER;
        else if(k == "description" || k == "descripcion")               c = DESC;
        if(c >= 0) col[c] = (int) i;
        else if(protocol == MQTT && std::find(mqttOptionNames().begin(), mqttOptionNames().end(), k) != mqttOptionNames().end())
          optcol.push_back(std::make_pair((int) i, k));
        else
        {
          rlTagHist probe;
          if(probe.set(k, "1")) histcol.push_back(std::make_pair((int) i, k));
        }
      }
      if(col[NAME] < 0 || col[ADDRESS] < 0)
      {
        errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "%1: faltan las columnas name y address en la primera linea"), filename));
        fclose(fp);
        return -1;
      }
      header = false;
      continue;
    }
    std::vector<std::string> f = splitCsvLine(line, sep);
    rlTagDef t;
    if(protocol == SIEMENS) t.slave = 0;
    for(int c=0; c<NCOL; c++)
    {
      if(col[c] < 0 || col[c] >= (int) f.size()) continue;
      const std::string &v = f[col[c]];
      if(v.empty()) continue;
      switch(c)
      {
        case NAME:    t.name = v; break;
        case ADDRESS: t.address = v; break;
        case TYPE:    setType(t, v); break;
        case COUNT:   t.count = atoi(v.c_str()); break;
        case STRIDE:  t.stride = atoi(v.c_str()); break;
        case RW:      t.rw = atoi(v.c_str()) ? 1 : 0; break;
        case SLAVE:   t.slave = atoi(v.c_str()); break;
        case SCAN:    t.scan = atoi(v.c_str()); break;
        case SCALE:   t.scale = stod_c(v); break;
        case OFFSET:  t.scale_offset = stod_c(v); break;
        case ORDER:   t.order = orderFromString(v); break;
        case DESC:    t.description = v; break;
      }
    }
    for(size_t h=0; h<histcol.size(); h++)
    {
      if(histcol[h].first < (int) f.size() && !f[histcol[h].first].empty())
        t.hist.set(histcol[h].second, f[histcol[h].first]);
    }
    for(size_t o=0; o<optcol.size(); o++)
    {
      if(optcol[o].first < (int) f.size() && !f[optcol[o].first].empty())
        t.setOpt(optcol[o].second, f[optcol[o].first]);
    }
    if(t.name.empty() && t.address.empty()) continue;
    tags.push_back(t);
  }
  fclose(fp);
  return 0;
}

static std::string csvField(const std::string &s)
{
  if(s.find_first_of(";,\"") == std::string::npos) return s;
  std::string r = "\"";
  for(size_t i=0; i<s.size(); i++) { if(s[i] == '"') r += '"'; r += s[i]; }
  return r + "\"";
}

static std::string typeText(const rlTagDef &t)
{
  return t.structName.empty() ? std::string(rlTagList::typeName(t.type)) : t.structName;
}

int rlTagList::writeCsv(const char *filename) const
{
  FILE *fp = fopen(filename, "w");
  if(fp == NULL) return -1;
  // las columnas del historiador solo si alguna variable las usa
  bool hist = false;
  for(size_t i=0; i<tags.size(); i++) if(tags[i].hist.used()) hist = true;
  fprintf(fp, "name;address;type;count;stride;rw;slave;scan;scale;offset;order;description%s",
          hist ? ";log;deadband;log_max;unit;alarm;hyst;alarm_text;group;priority;ack;event" : "");
  const std::vector<std::string> &on = mqttOptionNames();   // Nestor: opciones MQTT
  if(protocol == MQTT) for(size_t o=0; o<on.size(); o++) fprintf(fp, ";%s", on[o].c_str());
  fprintf(fp, "\n");
  for(size_t i=0; i<tags.size(); i++)
  {
    const rlTagDef &t = tags[i];
    fprintf(fp, "%s;%s;%s;%s;%s;%d;%d;%d;%s;%s;%s;%s",
            csvField(t.name).c_str(), csvField(t.address).c_str(), typeText(t).c_str(),
            (t.count > 1) ? itos(t.count).c_str() : "", (t.stride > 0) ? itos(t.stride).c_str() : "",
            t.rw, t.slave, t.scan, dtos(t.scale).c_str(), dtos(t.scale_offset).c_str(),
            (t.order < 0) ? "" : orderName(t.order), csvField(t.description).c_str());
    if(hist)
    {
      const rlTagHist &h = t.hist;
      fprintf(fp, ";%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s",
              (h.log >= 0) ? itos(h.log).c_str() : "", std::isnan(h.deadband) ? "" : dtos(h.deadband).c_str(),
              (h.log_max >= 0) ? itos(h.log_max).c_str() : "", csvField(h.unit).c_str(), csvField(h.alarm).c_str(),
              std::isnan(h.hyst) ? "" : dtos(h.hyst).c_str(), csvField(h.text).c_str(), csvField(h.group).c_str(),
              (h.priority >= 0) ? itos(h.priority).c_str() : "", (h.ack >= 0) ? itos(h.ack).c_str() : "",
              (h.event >= 0) ? itos(h.event).c_str() : "");
    }
    if(protocol == MQTT) for(size_t o=0; o<on.size(); o++) fprintf(fp, ";%s", csvField(t.opt(on[o].c_str())).c_str());
    fprintf(fp, "\n");
  }
  fclose(fp);
  return 0;
}

static std::string quoteIfNeeded(const std::string &s)
{
  if(!s.empty() && s.find_first_of(" \t\"#") == std::string::npos) return s;
  std::string r = "\"";
  for(size_t i=0; i<s.size(); i++) { if(s[i] == '"') r += '"'; r += s[i]; }
  return r + "\"";
}

std::string rlTagList::tagLine(const rlTagDef &t) const
{
  std::string s = "tag name=" + t.name + " address=" + quoteIfNeeded(t.address);
  if(t.type != 0 || !t.structName.empty()) s += " type=" + typeText(t);
  if(t.count > 1)  s += " count=" + itos(t.count);
  if(t.stride > 0) s += " stride=" + itos(t.stride);
  s += " rw=" + itos(t.rw);
  if(!textAddress()) s += " slave=" + itos(t.slave);
  for(size_t i=0; i<t.opts.size(); i++) if(!t.opts[i].second.empty()) s += " " + t.opts[i].first + "=" + quoteIfNeeded(t.opts[i].second);
  if(t.scan > 0) s += " scan=" + itos(t.scan);
  if(t.scale != 1.0) s += " scale=" + dtos(t.scale);
  if(t.scale_offset != 0.0) s += " offset=" + dtos(t.scale_offset);
  if(t.order >= 0) s += std::string(" order=") + orderName(t.order);
  if(!t.description.empty()) s += " desc=" + quoteIfNeeded(t.description);
  s += t.hist.pairs();
  return s;
}

// Nestor: las propias (shared = 0) van al .mk; las compartidas al .mkstruct del proyecto
static std::vector<std::string> structsText(const std::vector<rlTagStruct> &structs, int shared);

std::vector<std::string> rlTagList::structLines() const
{
  return structsText(structs, 0);
}

std::vector<std::string> rlTagList::sharedStructLines() const
{
  return structsText(structs, 1);
}

int rlTagList::readSharedStructs(const char *mkfile)
{
  if(structsFile.empty()) return 0;
  std::string path = structsFile;
  bool absolute = (path[0] == '/' || path[0] == '\\' || (path.size() > 1 && path[1] == ':'));
  if(!absolute)
  {
    std::string dir = mkfile ? mkfile : "";
    size_t p = dir.find_last_of("/\\");
    dir = (p == std::string::npos) ? "" : dir.substr(0, p + 1);
    path = dir + path;
  }
  FILE *fp = fopen(path.c_str(), "r");
  if(fp == NULL) { errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "no se puede abrir %1"), path)); return (int) errors.size(); }
  size_t first = structs.size();
  char buf[4096];
  int lineno = 0, saved = currentStruct;
  currentStruct = -1;
  while(fgets(buf, sizeof(buf) - 1, fp) != NULL)
  {
    lineno++;
    std::string l = trim(buf);
    if(startsWith(l, "struct") || startsWith(l, "member") || startsWith(l, "end")) parseLine(buf, path + ":" + itos(lineno));
  }
  fclose(fp);
  currentStruct = saved;
  // las del .mk tienen prioridad (misma estructura definida en los dos lados: queda la propia)
  for(size_t i=first; i<structs.size(); )
  {
    bool local = false;
    for(size_t k=0; k<first; k++) if(structs[k].name == structs[i].name) local = true;
    if(local) structs.erase(structs.begin() + i);
    else { structs[i].shared = 1; i++; }
  }
  return (int) errors.size();
}

int rlTagList::writeSharedStructs(const char *filename, const std::vector<std::string> &lines)
{
  FILE *fp = fopen(filename, "w");
  if(fp == NULL) return -1;
  fprintf(fp, "# Estructuras compartidas por los daemons del proyecto (pvdevelop: editor de tags, pestania Estructuras).\n");
  fprintf(fp, "# Cada daemon las usa con la linea \"structs=<este archivo>\" en su .mk. Con la posicion (at) vacia\n");
  fprintf(fp, "# la misma estructura sirve para Modbus, Siemens y OPC UA.\n");
  for(size_t i=0; i<lines.size(); i++) fprintf(fp, "%s\n", lines[i].c_str());
  fclose(fp);
  return 0;
}

static std::vector<std::string> structsText(const std::vector<rlTagStruct> &structs, int shared)
{
  std::vector<std::string> out;
  for(size_t i=0; i<structs.size(); i++)
  {
    if(structs[i].shared != shared) continue;
    const rlTagStruct &s = structs[i];
    std::string l = "struct name=" + s.name;
    if(!s.description.empty()) l += " desc=" + quoteIfNeeded(s.description);
    out.push_back(l);
    for(size_t j=0; j<s.members.size(); j++)
    {
      const rlTagMember &m = s.members[j];
      std::string ml = "member name=" + m.name;
      if(!m.at.empty()) ml += " at=" + quoteIfNeeded(m.at);
      ml += std::string(" type=") + rlTagList::typeName(m.type);
      if(m.count > 1) ml += " count=" + itos(m.count);
      if(m.rw >= 0) ml += " rw=" + itos(m.rw);
      if(m.scale != 1.0) ml += " scale=" + dtos(m.scale);
      if(m.scale_offset != 0.0) ml += " offset=" + dtos(m.scale_offset);
      if(m.order >= 0) ml += std::string(" order=") + rlTagList::orderName(m.order);
      if(!m.description.empty()) ml += " desc=" + quoteIfNeeded(m.description);
      ml += m.hist.pairs();
      out.push_back(ml);
    }
    out.push_back("end");
  }
  return out;
}

const rlTagStruct *rlTagList::findStruct(const std::string &name) const
{
  for(size_t i=0; i<structs.size(); i++) if(structs[i].name == name) return &structs[i];
  return NULL;
}

int rlTagList::readMkFile(const char *filename)
{
  FILE *fp = fopen(filename, "r");
  if(fp == NULL) { errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "no se puede abrir %1"), filename)); return (int) errors.size(); }
  char buf[4096];
  int lineno = 0;
  while(fgets(buf, sizeof(buf) - 1, fp) != NULL)
  {
    lineno++;
    parseLine(buf, std::string(filename) + ":" + itos(lineno));
  }
  fclose(fp);
  if(currentStruct >= 0) errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "falta \"end\" al final del struct %1"), structs[currentStruct].name));
  currentStruct = -1;
  readSharedStructs(filename);   // Nestor: structs= (estructuras del proyecto)
  if(!csvfile.empty())
  {
    std::string path = csvfile;
    bool absolute = (path[0] == '/' || path[0] == '\\' || (path.size() > 1 && path[1] == ':'));
    if(!absolute)
    {
      std::string dir = filename;
      size_t p = dir.find_last_of("/\\");
      dir = (p == std::string::npos) ? "" : dir.substr(0, p + 1);
      path = dir + path;
    }
    readCsv(path.c_str());
  }
  return (int) errors.size();
}

//------------------------------------------------------------------------------------------------
// estructuras y arreglos

// registros (Modbus) / bytes (S7) que ocupa un valor simple; coils: 1
int rlTagList::units(const rlTagDef &t) const
{
  if(protocol == MODBUS && (t.area == RLTAG_COIL || t.area == RLTAG_DISCRETE)) return 1;
  int s = valueSize(t);
  if(protocol == MODBUS) return (s + 1) / 2;
  return (s > 0) ? s : 1;
}

// posicion de cada miembro (pos, bit) y tamanio de la estructura, en el area de la variable
int rlTagList::resolveStruct(rlTagStruct &s, int area, std::string &err) const
{
  bool coils = (protocol == MODBUS && (area == RLTAG_COIL || area == RLTAG_DISCRETE));
  int bpu = (protocol == MODBUS) ? 16 : 8;  // bits por registro / byte
  int cursor = 0, lastBoolPos = -1, lastBit = -1;
  s.units = 0;
  if(s.members.empty()) { err = M(QT_TRANSLATE_NOOP("rlTagList", "la estructura %1 no tiene miembros"), s.name); return -1; }
  if(reservedName(s.name)) { err = M(QT_TRANSLATE_NOOP("rlTagList", "la estructura \"%1\" tiene un nombre reservado del lenguaje C (macro de los encabezados del sistema); usar otro"), s.name); return -1; }
  for(size_t i=0; i<s.members.size(); i++)
  {
    rlTagMember &m = s.members[i];
    if(!validName(m.name)) { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1: nombre de miembro invalido \"%2\""), s.name, m.name); return -1; }
    if(reservedName(m.name)) { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1: el miembro \"%2\" es un nombre reservado del lenguaje C (macro de los encabezados del sistema); usar otro"), s.name, m.name); return -1; }
    for(size_t j=0; j<i; j++) if(s.members[j].name == m.name) { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1: miembro repetido %2"), s.name, m.name); return -1; }
    if(m.type == 0) { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1.%2: falta el tipo"), s.name, m.name); return -1; }
    if(m.count < 1) m.count = 1;
    if(textAddress())
    {
      if(m.count > 1) { err = M((protocol == MQTT) ? QT_TRANSLATE_NOOP("rlTagList", "%1.%2: en MQTT un miembro no puede ser un arreglo")
                                                   : QT_TRANSLATE_NOOP("rlTagList", "%1.%2: en OPC UA un miembro no puede ser un arreglo"), s.name, m.name); return -1; }
      if(m.at.empty()) m.at = ((protocol == MQTT) ? "/" : ".") + m.name;  // sufijo del NodeId / topico por defecto
      m.pos = 0; m.bit = -1; m.units = 0;
      continue;
    }
    if(coils && m.type != RLTAG_BOOL) { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1.%2: en coils o entradas discretas los miembros son bool"), s.name, m.name); return -1; }
    rlTagDef tmp;
    tmp.type = m.type; tmp.area = area;
    int eu = units(tmp);
    bool bitMember = (m.type == RLTAG_BOOL && !coils);
    if(!m.at.empty())
    {
      std::string a = trim(m.at);
      size_t dot = a.find('.');
      m.pos = toNumber(a.substr(0, dot));
      m.bit = (dot == std::string::npos) ? (bitMember ? 0 : -1) : toNumber(a.substr(dot + 1));
      if(m.pos < 0) { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1.%2: posicion invalida \"%3\""), s.name, m.name, m.at); return -1; }
      if(dot != std::string::npos && (!bitMember || m.bit < 0 || m.bit >= bpu))
      { err = M(QT_TRANSLATE_NOOP("rlTagList", "%1.%2: bit invalido en \"%3\""), s.name, m.name, m.at); return -1; }
    }
    else if(bitMember)
    {
      // bits a continuacion del bool anterior, en el mismo registro / byte si entran
      if(lastBoolPos >= 0 && lastBit + 1 < bpu) { m.pos = lastBoolPos; m.bit = lastBit + 1; }
      else { m.pos = cursor; m.bit = 0; }
    }
    else
    {
      m.pos = cursor; m.bit = -1;
    }
    if(bitMember)
    {
      int lastbit = m.bit + m.count - 1;
      m.units = lastbit / bpu + 1;
      lastBoolPos = m.pos + lastbit / bpu;
      lastBit = lastbit % bpu;
      cursor = (std::max)(cursor, m.pos + m.units);
    }
    else
    {
      m.units = eu * m.count;
      lastBoolPos = -1;
      cursor = (std::max)(cursor, m.pos + m.units);
    }
    s.units = (std::max)(s.units, m.pos + m.units);
  }
  return 0;
}

static std::string replaceIndex(const std::string &s, const char *key, int i)
{
  std::string r = s;
  size_t p;
  while((p = r.find(key)) != std::string::npos) r.replace(p, strlen(key), itos(i));
  return r;
}

int rlTagList::expand()
{
  std::string err;
  flat.clear();
  for(size_t g=0; g<tags.size(); g++)
  {
    const rlTagDef &d = tags[g];
    std::string who = d.name.empty() ? std::string("(sin nombre)") : d.name;
    if(!validName(d.name)) { errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "nombre invalido: \"%1\" (letras, numeros y _; no empieza con numero)"), d.name)); continue; }
    if(reservedName(d.name)) { errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "\"%1\" es un nombre reservado del lenguaje C (macro de los encabezados del sistema, p. ej. B9600, CS8, ERROR); usar otro nombre"), d.name)); continue; }
    bool dup = false;
    for(size_t j=0; j<g; j++) if(tags[j].name == d.name) dup = true;
    if(dup) { errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "nombre repetido: %1"), d.name)); continue; }
    if(findStruct(d.name) != NULL)
    { errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "%1: se llama igual que una estructura (en tags_TARGET serian el mismo nombre)"), d.name)); continue; }
    int n = (d.count > 1) ? d.count : 1;
    if(d.count < 0 || d.count > 65535) { errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "%1: cantidad invalida"), who)); continue; }
    if(protocol == OPCUA && n > 1 && d.address.find("{i}") == std::string::npos)
    { errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "%1: un arreglo OPC UA lleva {i} en el NodeId (p. ej. ns=2;s=Zona{i}.Temp)"), who)); continue; }
    if(protocol == MQTT && n > 1 && d.address.find("{i}") == std::string::npos)
    { errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "%1: un arreglo MQTT lleva {i} en el topico (p. ej. planta/zona{i}/temp)"), who)); continue; }

    // direccion base (Modbus / S7)
    rlTagDef a = d;
    a.type = d.structName.empty() ? d.type : 0;
    if(!textAddress())
    {
      if(parseAddress(d.address, a, err) < 0) { errors.push_back(who + ": " + err); continue; }
    }
    int bpu = (protocol == MODBUS) ? 16 : 8;

    if(d.structName.empty())
    {
      int type = (d.type != 0) ? d.type : a.type;
      rlTagDef tmp = a;
      tmp.type = type;
      int u = units(tmp);
      bool bitArray = (!textAddress() && type == RLTAG_BOOL && a.bit >= 0);
      for(int e=0; e<n; e++)
      {
        rlTagDef f = d;
        f.group = (int) g;
        f.count = 1; f.stride = 0;
        f.cname = (n > 1) ? d.name + "[" + itos(e) + "]" : d.name;
        f.name = f.cname;
        if(textAddress())
        {
          f.address = replaceIndex(d.address, "{i}", e);
          if(!d.opt("cmd_topic").empty()) f.setOpt("cmd_topic", replaceIndex(d.opt("cmd_topic"), "{i}", e));
        }
        else
        {
          rlTagDef p = a;
          p.type = type;
          if(bitArray)
          {
            int bitpos = a.bit + e * ((d.stride > 0) ? d.stride : 1);
            p.addr = a.addr + bitpos / bpu;
            p.bit  = bitpos % bpu;
          }
          else
          {
            p.addr = a.addr + e * ((d.stride > 0) ? d.stride : u);
          }
          f.address = formatAddress(p, notation);
        }
        flat.push_back(f);
      }
      continue;
    }

    // estructura
    const rlTagStruct *ps = findStruct(d.structName);
    if(ps == NULL) { errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "%1: tipo desconocido %2"), who, d.structName)); continue; }
    rlTagStruct s = *ps;
    if(resolveStruct(s, a.area, err) < 0) { errors.push_back(who + ": " + err); continue; }
    int stride = (d.stride > 0) ? d.stride : s.units;
    for(int e=0; e<n; e++)
    {
      std::string prefix = (n > 1) ? d.name + "[" + itos(e) + "]" : d.name;
      for(size_t mi=0; mi<s.members.size(); mi++)
      {
        const rlTagMember &m = s.members[mi];
        for(int k=0; k<m.count; k++)
        {
          rlTagDef f = d;
          f.group = (int) g;
          f.count = 1; f.stride = 0; f.structName.clear();
          f.type = m.type;
          f.cname = prefix + "." + m.name + ((m.count > 1) ? "[" + itos(k) + "]" : std::string());
          f.name = f.cname;
          if(m.rw >= 0) f.rw = m.rw;
          f.scale = m.scale; f.scale_offset = m.scale_offset;
          if(m.order >= 0) f.order = m.order;
          f.description = m.description;
          f.hist = m.hist;
          f.hist.inherit(d.hist);  // log, group, priority, ... de la variable
          if(textAddress())
          {
            f.address = replaceIndex(d.address, "{i}", e) + m.at;
            if(!d.opt("cmd_topic").empty()) f.setOpt("cmd_topic", replaceIndex(d.opt("cmd_topic"), "{i}", e) + m.at);
          }
          else
          {
            rlTagDef p = a;
            p.type = m.type;
            int base = a.addr + e * stride + m.pos;
            if(protocol == MODBUS && (a.area == RLTAG_COIL || a.area == RLTAG_DISCRETE))
            {
              p.addr = base + k; p.bit = -1;
            }
            else if(m.type == RLTAG_BOOL)
            {
              int bitpos = m.bit + k;
              p.addr = base + bitpos / bpu; p.bit = bitpos % bpu;
            }
            else
            {
              p.addr = base + k * (m.units / m.count); p.bit = -1;
            }
            f.address = formatAddress(p, notation);
          }
          flat.push_back(f);
        }
      }
    }
  }
  return errors.empty() ? 0 : -1;
}

//------------------------------------------------------------------------------------------------
// bloques y memoria compartida

struct GroupKey
{
  int slave, area, dbnum, scan;
  bool operator==(const GroupKey &o) const { return slave == o.slave && area == o.area && dbnum == o.dbnum && scan == o.scan; }
};

int rlTagList::layout(int base)
{
  std::string err;
  blocks.clear();
  if(expand() < 0) return -1;
  for(size_t i=0; i<flat.size(); i++)
  {
    if(check(flat[i], err) < 0) errors.push_back(err);
  }
  if(!errors.empty()) return -1;

  if(textAddress())
  {
    for(size_t i=0; i<flat.size(); i++) { flat[i].offset = base; base += flat[i].size; }
    for(size_t i=0; i<flat.size(); i++) { flat[i].status = base++; }
    return base;
  }

  // agrupar por esclavo, area (funcion / ORG), DB y frecuencia de lectura
  std::vector<GroupKey> keys;
  std::vector< std::vector<int> > groups;
  for(size_t i=0; i<flat.size(); i++)
  {
    GroupKey k = { flat[i].slave, flat[i].area, flat[i].dbnum, flat[i].scan };
    size_t g = 0;
    while(g < keys.size() && !(keys[g] == k)) g++;
    if(g == keys.size()) { keys.push_back(k); groups.push_back(std::vector<int>()); }
    groups[g].push_back((int) i);
  }

  for(size_t g=0; g<groups.size(); g++)
  {
    std::vector<int> &idx = groups[g];
    std::stable_sort(idx.begin(), idx.end(), [this](int a, int b) { return flat[a].addr < flat[b].addr; });
    bool bits = (protocol == MODBUS && (keys[g].area == RLTAG_COIL || keys[g].area == RLTAG_DISCRETE));
    int maxb = max_block, gap = max_gap;
    if(protocol == MODBUS)
    {
      if(maxb <= 0) maxb = bits ? 2000 : 125;
      if(bits && maxb > 2000) maxb = 2000;
      if(!bits && maxb > 125) maxb = 125;
      if(gap < 0) gap = bits ? 64 : 8;
    }
    else
    {
      if(maxb <= 0) maxb = 200; // PDU de S7 de 240 bytes
      if(maxb > 222) maxb = 222; // Nestor: lo que entra en una PDU de 240 bytes (mas grande fallaba en cada ciclo)
      if(gap < 0) gap = 16;
    }
    rlTagBlock b;
    bool open = false;
    for(size_t n=0; n<idx.size(); n++)
    {
      rlTagDef &t = flat[idx[n]];
      int u = bits ? 1 : (protocol == MODBUS) ? (t.size + 1) / 2 : t.size;
      int first = t.addr, last = t.addr + u - 1;
      if(open && first <= b.start + b.count + gap && ((std::max)(last, b.start + b.count - 1) - b.start + 1) <= maxb)
      {
        b.count = (std::max)(last, b.start + b.count - 1) - b.start + 1;
      }
      else
      {
        if(open) blocks.push_back(b);
        b.slave = keys[g].slave; b.area = keys[g].area; b.dbnum = keys[g].dbnum; b.scan = keys[g].scan;
        b.start = first; b.count = u; b.offset = b.nbytes = b.status = -1;
        open = true;
      }
      t.block = (int) blocks.size();
    }
    if(open) blocks.push_back(b);
  }

  for(size_t i=0; i<blocks.size(); i++)
  {
    rlTagBlock &b = blocks[i];
    bool bits = (protocol == MODBUS && (b.area == RLTAG_COIL || b.area == RLTAG_DISCRETE));
    b.nbytes = bits ? (b.count + 7) / 8 : (protocol == MODBUS) ? b.count * 2 : b.count;
    b.offset = base;
    base += b.nbytes;
  }
  for(size_t i=0; i<blocks.size(); i++) blocks[i].status = base++;

  for(size_t i=0; i<flat.size(); i++)
  {
    rlTagDef &t = flat[i];
    const rlTagBlock &b = blocks[t.block];
    bool bits = (protocol == MODBUS && (b.area == RLTAG_COIL || b.area == RLTAG_DISCRETE));
    if(bits)
    {
      t.offset = b.offset + (t.addr - b.start) / 8;
      t.bit    = (t.addr - b.start) % 8;
      t.size   = 1;
    }
    else if(protocol == MODBUS) t.offset = b.offset + (t.addr - b.start) * 2;
    else                        t.offset = b.offset + (t.addr - b.start);
    t.status = b.status;
  }
  return base;
}

//------------------------------------------------------------------------------------------------
// tags de sistema (TARGET_SYS): estado de la comunicacion que actualiza el daemon

struct SysDef { const char *name; int type; int offset; int size; const char *desc; };
static const SysDef sysDefs[] = {
  { "Comunicacion",        RLTAG_BOOL,  RLSYS_COMUNICACION,         1, QT_TRANSLATE_NOOP("rlTagList", "Todos los bloques del ultimo ciclo se leyeron bien") },
  { "ComunicacionParcial", RLTAG_BOOL,  RLSYS_COMUNICACION_PARCIAL, 1, QT_TRANSLATE_NOOP("rlTagList", "Algun bloque se leyo bien y alguno fallo") },
  { "Conectado",           RLTAG_BOOL,  RLSYS_CONECTADO,            1, QT_TRANSLATE_NOOP("rlTagList", "Conexion abierta") },
  { "BloquesOk",           RLTAG_UINT,  RLSYS_BLOQUES_OK,           2, QT_TRANSLATE_NOOP("rlTagList", "Bloques leidos bien en el ultimo ciclo") },
  { "BloquesError",        RLTAG_UINT,  RLSYS_BLOQUES_ERROR,        2, QT_TRANSLATE_NOOP("rlTagList", "Bloques con error en el ultimo ciclo") },
  { "Ciclo",               RLTAG_UDINT, RLSYS_CICLO,                4, QT_TRANSLATE_NOOP("rlTagList", "Numero de ciclo") },
  { "TiempoCiclo",         RLTAG_UDINT, RLSYS_TIEMPO_CICLO,         4, QT_TRANSLATE_NOOP("rlTagList", "Duracion del ultimo ciclo (ms)") },
  { "TiempoCicloMax",      RLTAG_UDINT, RLSYS_TIEMPO_CICLO_MAX,     4, QT_TRANSLATE_NOOP("rlTagList", "Duracion maxima de un ciclo (ms)") },
  { "ErroresLectura",      RLTAG_UDINT, RLSYS_ERRORES_LECTURA,      4, QT_TRANSLATE_NOOP("rlTagList", "Errores de lectura desde el inicio") },
  { "ErroresEscritura",    RLTAG_UDINT, RLSYS_ERRORES_ESCRITURA,    4, QT_TRANSLATE_NOOP("rlTagList", "Errores de escritura desde el inicio") },
  { "UltimoError",         RLTAG_DINT,  RLSYS_ULTIMO_ERROR,         4, QT_TRANSLATE_NOOP("rlTagList", "Codigo del ultimo error (0 = ninguno)") },
  { "UltimaLecturaOk",     RLTAG_UDINT, RLSYS_ULTIMA_LECTURA_OK,    4, QT_TRANSLATE_NOOP("rlTagList", "Hora de la ultima lectura correcta (s desde 1970, UTC)") },
  { "Inicio",              RLTAG_UDINT, RLSYS_INICIO,               4, QT_TRANSLATE_NOOP("rlTagList", "Hora de inicio del daemon (s desde 1970, UTC)") }
};
#define NUM_SYS_DEFS ((int) (sizeof(sysDefs) / sizeof(sysDefs[0])))

std::vector<std::string> rlTagList::sysNames()
{
  std::vector<std::string> v;
  for(int i=0; i<NUM_SYS_DEFS; i++) v.push_back(sysDefs[i].name);
  return v;
}

std::string rlTagList::sysDescription(const std::string &name)
{
  for(int i=0; i<NUM_SYS_DEFS; i++) if(name == sysDefs[i].name) return M(sysDefs[i].desc);
  if(startsWith(name, "Esclavo")) return M(QT_TRANSLATE_NOOP("rlTagList", "El esclavo %1 respondio en el ultimo ciclo"), name.substr(7));
  if(startsWith(name, "Plc"))     return M(QT_TRANSLATE_NOOP("rlTagList", "El PLC %1 respondio en el ultimo ciclo"), name.substr(3));
  return std::string();
}

int rlTagList::layoutSys(int base, const std::vector<int> &plcs0)
{
  sys.clear();
  sys_base = base;
  for(int i=0; i<NUM_SYS_DEFS; i++)
  {
    rlTagDef t;
    t.name = t.cname = sysDefs[i].name;
    t.type = sysDefs[i].type; t.size = sysDefs[i].size; t.offset = base + sysDefs[i].offset;
    t.bit = (t.type == RLTAG_BOOL) ? 0 : -1;
    t.slave = 0;
    sys.push_back(t);
  }
  std::vector<int> plcs = plcs0;
  std::sort(plcs.begin(), plcs.end());
  plcs.erase(std::unique(plcs.begin(), plcs.end()), plcs.end());
  if(textAddress()) plcs.clear();
  int pos = RLSYS_PLC;
  for(size_t i=0; i<plcs.size(); i++, pos++)
  {
    rlTagDef t;
    t.name = t.cname = std::string((protocol == SIEMENS) ? "Plc" : "Esclavo") + itos(plcs[i]);
    t.type = RLTAG_BOOL; t.size = 1; t.bit = 0; t.offset = base + pos;
    t.slave = plcs[i];
    sys.push_back(t);
  }
  for(size_t i=0; i<sys.size(); i++)
  {
    rlTagDef &t = sys[i];
    t.description = sysDescription(t.name);
    t.status = -1; t.rw = 0; t.order = RLTAG_ABCD; t.scale = 1; t.scale_offset = 0;
    for(size_t k=0; k<sysHist.size(); k++) if(sysHist[k].first == t.name) t.hist = sysHist[k].second;
  }
  for(size_t k=0; k<sysHist.size(); k++)
  {
    if(findSys(sysHist[k].first) == NULL)
      errors.push_back(M(QT_TRANSLATE_NOOP("rlTagList", "sys name=%1: no hay un tag de sistema con ese nombre"), sysHist[k].first));
  }
  return base + pos;
}

const rlTagDef *rlTagList::findSys(const std::string &name) const
{
  for(size_t i=0; i<sys.size(); i++) if(sys[i].name == name) return &sys[i];
  return NULL;
}

std::string rlTagList::sysLine(const std::string &name, const rlTagHist &h) const
{
  return "sys name=" + quoteIfNeeded(name) + h.pairs();
}

int rlTagList::consoleFromString(const std::string &s0)
{
  std::string s = lower(trim(s0));
  if(s == "quiet"   || s == "silencioso" || s == "0") return CONSOLE_QUIET;
  if(s == "normal"  || s == "1")                      return CONSOLE_NORMAL;
  if(s == "verbose" || s == "detallado"  || s == "2") return CONSOLE_VERBOSE;
  return -1;
}

const char *rlTagList::consoleName(int level)
{
  if(level == CONSOLE_QUIET)   return "quiet";
  if(level == CONSOLE_VERBOSE) return "verbose";
  return "normal";
}

//------------------------------------------------------------------------------------------------
// target.h

static const char *typeConst[] = { "0", "RLTAG_BOOL", "RLTAG_BYTE", "RLTAG_INT", "RLTAG_UINT",
                                   "RLTAG_DINT", "RLTAG_UDINT", "RLTAG_REAL", "RLTAG_LREAL" };
static const char *orderConst[] = { "RLTAG_ABCD", "RLTAG_CDAB", "RLTAG_BADC", "RLTAG_DCBA" };

void rlTagList::writeHeader(FILE *fout, const char *target) const
{
  if(flat.empty())
  {
    writeSysHeader(fout, target, true);
    return;
  }
  fprintf(fout, "\n// Tags (generado por pvdevelop; no editar: se regenera)\n");
  fprintf(fout, "// Uso: %s.getBool(%s_<tag>), getInt, getUInt, getReal, setBool, setInt, setReal, quality\n", target, target);
  fprintf(fout, "#include \"rltag.h\"\n");
  if(!textAddress())
  {
    fprintf(fout, "// %d bloques de lectura:\n", (int) blocks.size());
    for(size_t i=0; i<blocks.size(); i++)
    {
      const rlTagBlock &b = blocks[i];
      if(protocol == MODBUS)
        fprintf(fout, "//   esclavo=%d funcion=%d desde=%d cantidad=%d scan=%dms -> offset=%d bytes=%d calidad=%d\n",
                b.slave, b.area, b.start, b.count, b.scan, b.offset, b.nbytes, b.status);
      else
        fprintf(fout, "//   plc=%d org=%d db=%d desde=%d bytes=%d scan=%dms -> offset=%d calidad=%d\n",
                b.slave, b.area, b.dbnum, b.start, b.count, b.scan, b.offset, b.status);
    }
  }

  // tipos de las estructuras usadas
  for(size_t i=0; i<structs.size(); i++)
  {
    bool used = false;
    for(size_t g=0; g<tags.size(); g++) if(tags[g].structName == structs[i].name) used = true;
    if(!used) continue;
    const rlTagStruct &s = structs[i];
    fprintf(fout, "struct %s_%s%s%s\n{\n", target, s.name.c_str(), s.description.empty() ? "" : " // ", commentText(s.description).c_str());
    for(size_t j=0; j<s.members.size(); j++)
    {
      const rlTagMember &m = s.members[j];
      if(m.count > 1) fprintf(fout, "  rlTag %s[%d];\n", m.name.c_str(), m.count);
      else            fprintf(fout, "  rlTag %s;\n", m.name.c_str());
    }
    fprintf(fout, "};\n");
  }

  // variables: simples, arreglos y estructuras (en el orden de "flat")
  size_t i = 0;
  for(size_t g=0; g<tags.size(); g++)
  {
    const rlTagDef &d = tags[g];
    int n = (d.count > 1) ? d.count : 1;
    const rlTagStruct *s = d.structName.empty() ? NULL : findStruct(d.structName);
    std::string ctype = s ? std::string(target) + "_" + s->name : std::string("rlTag");
    std::string decl = "static const " + ctype + " " + target + "_" + d.name + ((n > 1) ? "[" + itos(n) + "]" : std::string());
    fprintf(fout, "%s =%s", decl.c_str(), (n > 1 || s) ? "\n{" : " ");
    if(!d.description.empty() && (n > 1 || s)) fprintf(fout, " // %s", commentText(d.description).c_str());
    if(n > 1 || s) fprintf(fout, "\n");
    for(int e=0; e<n; e++)
    {
      if(s && n > 1) fprintf(fout, "  {\n");
      int members = s ? (int) s->members.size() : 1;
      for(int mi=0; mi<members; mi++)
      {
        int mc = (s && s->members[mi].count > 1) ? s->members[mi].count : 1;
        if(mc > 1) fprintf(fout, "%s  {\n", (s && n > 1) ? "  " : "");
        for(int k=0; k<mc; k++, i++)
        {
          const rlTagDef &t = flat[i];
          std::string comment = formatAddress(t, notation);
          if(!t.description.empty()) comment += "  " + t.description;
          comment = commentText(comment);
          const char *indent = (n > 1 || s) ? ((s && n > 1) ? ((mc > 1) ? "      " : "    ") : ((mc > 1) ? "    " : "  ")) : "";
          bool lastItem = (k + 1 == mc) && (!s || mi + 1 == members) && (e + 1 == n);
          bool lastInList = (mc > 1) ? (k + 1 == mc) : (s ? (mi + 1 == members) : (e + 1 == n));
          (void) lastItem;
          fprintf(fout, "%s{\"%s\", %s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %s, %s, %s}%s // %s\n",
                  indent, t.cname.c_str(), typeConst[t.type], t.offset, t.size, t.bit, t.status, t.rw,
                  textAddress() ? (int) i : t.slave, t.area, t.dbnum, t.addr, orderConst[t.order & 3],
                  dtos(t.scale).c_str(), dtos(t.scale_offset).c_str(),
                  (n > 1 || s) ? (lastInList ? "" : ",") : ";", comment.c_str());
        }
        if(mc > 1) fprintf(fout, "%s  }%s\n", (s && n > 1) ? "  " : "", (mi + 1 == members) ? "" : ",");
      }
      if(s && n > 1) fprintf(fout, "  }%s\n", (e + 1 == n) ? "" : ",");
    }
    if(n > 1 || s) fprintf(fout, "};\n");
    if(n > 1) fprintf(fout, "#define %s_%s_COUNT %d\n", target, d.name.c_str(), n);
  }

  fprintf(fout, "#define %s_TAG_COUNT %d\n", target, (int) flat.size());
  fprintf(fout, "static const rlTag * const %s_tags[] = {\n", target);
  for(size_t k=0; k<flat.size(); k++)
    fprintf(fout, "  &%s_%s%s\n", target, flat[k].cname.c_str(), (k + 1 < flat.size()) ? "," : "");
  fprintf(fout, "};\n");

  // los mismos tags sin prefijo: "using namespace tags_TARGET;" en una mascara y PLC1.getReal(Temp)
  // (si un nombre choca con un widget de la mascara, usar TARGET_Temp o tags_TARGET::Temp)
  fprintf(fout, "\n// Sin prefijo: using namespace tags_%s;  ->  %s.getReal(<tag>)\n", target, target);
  fprintf(fout, "namespace tags_%s\n{\n", target);
  for(size_t k=0; k<structs.size(); k++)
  {
    bool used = false;
    for(size_t g=0; g<tags.size(); g++) if(tags[g].structName == structs[k].name) used = true;
    if(used) fprintf(fout, "  typedef %s_%s %s;\n", target, structs[k].name.c_str(), structs[k].name.c_str());
  }
  for(size_t g=0; g<tags.size(); g++)
  {
    const rlTagDef &d = tags[g];
    int n = (d.count > 1) ? d.count : 1;
    std::string ctype = d.structName.empty() ? std::string("rlTag") : std::string(target) + "_" + d.structName;
    // Nestor: si el nombre ya es una macro (de un encabezado incluido antes) el alias se omite: queda TARGET_nombre
    fprintf(fout, "#ifndef %s\n", d.name.c_str());
    if(n > 1)
    {
      fprintf(fout, "  static const %s (&%s)[%d] = %s_%s;\n", ctype.c_str(), d.name.c_str(), n, target, d.name.c_str());
      fprintf(fout, "  const int %s_COUNT = %d;\n", d.name.c_str(), n);
    }
    else
    {
      fprintf(fout, "  static const %s &%s = %s_%s;\n", ctype.c_str(), d.name.c_str(), target, d.name.c_str());
    }
    fprintf(fout, "#endif\n");
  }
  fprintf(fout, "}\n");
  writeSysHeader(fout, target, false);
}

// TARGET_SYS: tags de sistema (estado de la comunicacion; los actualiza el daemon)
void rlTagList::writeSysHeader(FILE *fout, const char *target, bool include) const
{
  if(sys.empty()) return;
  if(include)
  {
    fprintf(fout, "\n// Tags (generado por pvdevelop; no editar: se regenera)\n");
    fprintf(fout, "#include \"rltag.h\"\n");
  }
  fprintf(fout, "\n// Tags de sistema (los actualiza el daemon): %s.getBool(%s_SYS.Comunicacion), %s.daemonAlive(%s_SYS.Ciclo)\n",
          target, target, target, target);
  fprintf(fout, "#define %s_SYS_BASE %d\n", target, sys_base);
  fprintf(fout, "struct %s_SysTags\n{\n", target);
  for(size_t i=0; i<sys.size(); i++) fprintf(fout, "  rlTag %s;\n", sys[i].name.c_str());
  fprintf(fout, "};\n");
  fprintf(fout, "static const %s_SysTags %s_SYS =\n{\n", target, target);
  for(size_t i=0; i<sys.size(); i++)
  {
    const rlTagDef &t = sys[i];
    fprintf(fout, "  {\"SYS.%s\", %s, %d, %d, %d, -1, 0, %d, 0, 0, 0, RLTAG_ABCD, 1, 0}%s // %s\n",
            t.name.c_str(), typeConst[t.type], t.offset, t.size, t.bit, t.slave,
            (i + 1 < sys.size()) ? "," : "", t.description.c_str());
  }
  fprintf(fout, "};\n");
  fprintf(fout, "namespace tags_%s { static const %s_SysTags &SYS = %s_SYS; }\n", target, target, target);
}

//------------------------------------------------------------------------------------------------
// TARGET.tagmap: lo lee el historiador (rlhistorian) para saber donde esta cada variable

static const char *protocolName(int p)
{
  return (p == rlTagList::MODBUS) ? "modbus" : (p == rlTagList::SIEMENS) ? "siemens" : (p == rlTagList::MQTT) ? "mqtt" : "opcua";
}

int rlTagList::writeTagMap(const char *filename, const char *target, const char *shared_memory, int size, const char *mailbox) const
{
  FILE *fp = fopen(filename, "w");
  if(fp == NULL) return -1;
  fprintf(fp, "# Mapa de la memoria compartida de %s para el historiador (generado por pvdevelop; no editar)\n", target);
  fprintf(fp, "source=%s\n", quoteIfNeeded(target).c_str());
  fprintf(fp, "protocol=%s\n", protocolName(protocol));
  fprintf(fp, "shared_memory=%s\n", quoteIfNeeded(shared_memory).c_str());
  fprintf(fp, "size=%d\n", size);
  if(mailbox && mailbox[0]) fprintf(fp, "mailbox=%s\n", quoteIfNeeded(mailbox).c_str());   // Nestor: escrituras (rlautomation)
  for(size_t i=0; i<flat.size(); i++)
  {
    const rlTagDef &t = flat[i];
    std::string desc = t.description;
    if(desc.empty() && t.group >= 0 && t.group < (int) tags.size()) desc = tags[t.group].description;
    fprintf(fp, "tag name=%s type=%s offset=%d size=%d bit=%d status=%d order=%s scale=%s soffset=%s",
            quoteIfNeeded(t.cname).c_str(), typeName(t.type), t.offset, t.size, t.bit, t.status,
            orderName(t.order & 3), dtos(t.scale).c_str(), dtos(t.scale_offset).c_str());
    // Nestor: lo que necesita una escritura (los mismos campos que el rlTag de TARGET.h)
    fprintf(fp, " rw=%d slave=%d area=%d dbnum=%d address=%d", t.rw, textAddress() ? (int) i : t.slave, t.area, t.dbnum, t.addr);
    // Nestor: la direccion en la notacion del PLC (%MW0.0, DB10.DBX0.1, el nodo OPC UA), para mostrarla
    fprintf(fp, " plcaddr=%s", quoteIfNeeded(formatAddress(t, notation)).c_str());
    if(!desc.empty()) fprintf(fp, " desc=%s", quoteIfNeeded(desc).c_str());
    fprintf(fp, "%s\n", t.hist.pairs().c_str());
  }
  for(size_t i=0; i<sys.size(); i++) // tags de sistema (sin byte de calidad)
  {
    const rlTagDef &t = sys[i];
    fprintf(fp, "tag name=SYS.%s type=%s offset=%d size=%d bit=%d status=-1 order=ABCD scale=1 soffset=0 desc=%s%s\n",
            t.name.c_str(), typeName(t.type), t.offset, t.size, t.bit, quoteIfNeeded(t.description).c_str(), t.hist.pairs().c_str());
  }
  fclose(fp);
  return 0;
}
