/***************************************************************************
                          pvbind.h  -  description
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
#ifndef _PV_BIND_H_
#define _PV_BIND_H_

/*! \file pvbind.h
\brief Ayudantes para las acciones y animaciones que se definen en pvdevelop
       (Designer > Propiedades > Accion y animacion). Solo cabecera.

<pre>
pvdevelop genera maskN_pvb.h con dos funciones por mascara:
  pvbActions(p, id, evento, texto, valor)   escribir tags, ir a otra pantalla, volver...
  pvbAnimate(p)                             texto, color, visible, valor de los widgets segun los tags
y agrega una linea en cada slot (se puede mover: el codigo propio va antes o despues):
  if(int r = pvbActions(p, id, PVB_PRESS, NULL, 0)) return (r > 0) ? r : 0;
     r > 0: ir a la mascara r     r < 0: la accion no se hizo (condicion, falta confirmar)
  pvbAnimate(p);                            (en slotNullEvent)

Navegacion con historial (por cliente: cada pvbrowser conectado tiene el suyo):
  return pvbGoto(1, 3);    // desde la mascara 1 a la 3 (recuerda la 1)
  return pvbBack(3);       // a la anterior (si no hay: la 1)
  return pvbHome();        // a la 1 y borra el historial

</pre> */

#include "processviewserver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cmath>
#include <ctype.h>
#include <string>
#include <vector>
#include <map>

#include "pvbthreaddata.h"

enum PvbEvent { PVB_CLICK = 1, PVB_PRESS, PVB_RELEASE, PVB_TEXT, PVB_SLIDER, PVB_CHECK };
#define PVB_CONFIRM_BASE 30000   // id de los cuadros "Confirmar" (la respuesta llega en slotSliderEvent)

//------------------------------------------------------------------------------------------------
// navegacion con historial (un historial por hilo = por cliente conectado)

inline std::vector<int> &pvbHistory()
{
  return pvbThreadData<std::vector<int>, 1>();
}

inline int pvbGoto(int from, int to)
{
  if(from > 0 && from != to) pvbHistory().push_back(from);
  if(pvbHistory().size() > 50) pvbHistory().erase(pvbHistory().begin());
  return to;
}

inline int pvbBack(int current)
{
  std::vector<int> &h = pvbHistory();
  while(!h.empty() && h.back() == current) h.pop_back();
  if(h.empty()) return 1;
  int to = h.back();
  h.pop_back();
  return to;
}

inline int pvbHome()
{
  pvbHistory().clear();
  return 1;
}

//------------------------------------------------------------------------------------------------
// valores

//! numero escrito por el operador ("12,5" o "12.5"); ok = 0 si no es un numero
inline double pvbNumber(const char *text, int *ok)
{
  std::string s = text ? text : "";
  for(size_t i=0; i<s.size(); i++) if(s[i] == ',') s[i] = '.';
  if(s == "true" || s == "on" || s == "si" || s == "yes")  { *ok = 1; return 1; }
  if(s == "false" || s == "off" || s == "no")              { *ok = 1; return 0; }
  char *end = NULL;
  double v = strtod(s.c_str(), &end);
  *ok = (end != s.c_str() && end != NULL);
  while(*ok && end && *end == ' ') end++;
  if(end && *end != '\0') *ok = 0;
  if(*ok && !std::isfinite(v)) { *ok = 0; return 0; }   // Nestor: "nan", "inf", "1e999" no son valores para un PLC
  return v;
}

//! ultimo texto de un LineEdit (llega en cada tecla; el Enter llega como click sin el texto)
inline std::string &pvbEditText(int id)
{
  return pvbThreadData<std::map<int, std::string>, 2>()[id];
}

inline double pvbClamp(double v, double vmin, double vmax)
{
  if(!std::isnan(vmin) && v < vmin) v = vmin;
  if(!std::isnan(vmax) && v > vmax) v = vmax;
  return v;
}

//------------------------------------------------------------------------------------------------
// confirmacion (pvMessageBox no bloquea: la accion queda pendiente hasta la respuesta)

struct PvbPending { int id = 0; int event = 0; std::string text; int val = 0; };

inline PvbPending &pvbPending()
{
  return pvbThreadData<PvbPending, 3>();   // id 0 = nada pendiente
}

//! pregunta "question"; la accion (id, event, text, val) se hace si el operador responde Si
inline void pvbAskConfirm(PARAM *p, int id, int event, const char *text, int val, const char *question)
{
  PvbPending &pd = pvbPending();
  pd.id = id; pd.event = event; pd.text = text ? text : ""; pd.val = val;
  pvMessageBox(p, PVB_CONFIRM_BASE + id, BoxWarning, question, MessageBoxYes, MessageBoxNo, 0);
}

inline void pvbMessage(PARAM *p, const char *text)
{
  pvMessageBox(p, PVB_CONFIRM_BASE - 1, BoxWarning, text, MessageBoxOk, 0, 0);
}

//------------------------------------------------------------------------------------------------
// animaciones: solo se manda al cliente lo que cambio

inline std::map<long, std::string> &pvbCache(int mask)
{
  std::map<long, std::string> &cache = pvbThreadData<std::map<long, std::string>, 4>();
  std::map<long, std::string> &faceplate = pvbThreadData<std::map<long, std::string>, 5>();
  static thread_local int lastMask = -1;
  static thread_local int lastDefinition = -1;
  if(mask < 0) return faceplate;                              // faceplate (se limpia al abrirlo)
  // otra mascara, o la misma definida de nuevo (show_maskN otra vez, una ventana modal):
  // los widgets volvieron a los valores del Designer, hay que mandar todo
  int def = pvDefinitionCount();
  if(mask != lastMask || def != lastDefinition) { cache.clear(); lastMask = mask; lastDefinition = def; }
  return cache;
}

#define PVB_CLOSE (-1000)   // accion "Cerrar ventana" (faceplate)

inline bool pvbChanged(int mask, int id, int what, const std::string &value)
{
  std::map<long, std::string> &c = pvbCache(mask);
  long key = (long) id * 16 + what;
  std::map<long, std::string>::iterator it = c.find(key);
  if(it != c.end() && it->second == value) return false;
  c[key] = value;
  return true;
}

inline int &pvbTick()
{
  static thread_local int tick = 0;
  return tick;
}

enum { PVB_A_TEXT = 1, PVB_A_BG, PVB_A_FG, PVB_A_VISIBLE, PVB_A_VALUE, PVB_A_CHECK };

inline void pvbSetText(PARAM *p, int mask, int id, const char *text)
{
  if(pvbChanged(mask, id, PVB_A_TEXT, text)) pvSetText(p, id, text);
}

//! 1 si fmt tiene exactamente una conversion de double (%[flags][ancho][.prec]eEfFgG; %% aparte)
inline int pvbDoubleFormat(const char *fmt)
{
  int n = 0;
  for(const char *c = fmt; c && *c; c++)
  {
    if(*c != '%') continue;
    c++;
    if(*c == '%') continue;
    while(*c && strchr("-+ #0", *c)) c++;
    while(isdigit((unsigned char) *c)) c++;
    if(*c == '.') { c++; while(isdigit((unsigned char) *c)) c++; }
    if(*c == 'l') c++;   // %lf tambien es de double
    if(*c == '\0' || !strchr("eEfFgG", *c)) return 0;
    n++;
  }
  return n == 1;
}

//! texto con formato printf; con mala calidad muestra "---" (un formato que no es de un double: "%g")
inline void pvbText(PARAM *p, int mask, int id, const char *fmt, double v, int quality)
{
  char buf[256];
  if(quality == 0) strcpy(buf, "---");
  else snprintf(buf, sizeof(buf), (fmt && pvbDoubleFormat(fmt)) ? fmt : "%g", v);
  pvbSetText(p, mask, id, buf);
}

//! texto segun el valor: "Parado|Marcha|Falla" con v = 1 -> "Marcha" (fuera de rango: el ultimo)
inline std::string pvbChoice(const char *options, double v)
{
  const char *a = options ? options : "";
  long i = (std::isnan(v) || v < 0) ? 0 : (v > 1e6 ? 1000000 : lround(v));
  for(; i > 0; i--)   // salta i opciones (si no hay tantas queda en la ultima)
  {
    const char *b = strchr(a, '|');
    if(b == NULL) break;
    a = b + 1;
  }
  const char *b = strchr(a, '|');
  return b ? std::string(a, b - a) : std::string(a);
}

//! "#RRGGBB" (vacio = el color original); what: PVB_A_BG o PVB_A_FG
inline void pvbColor(PARAM *p, int mask, int id, int what, const char *color)
{
  std::string c = color ? color : "";
  if(!pvbChanged(mask, id, what, c)) return;
  int r = -1, g = -1, b = -1;
  if(c.size() == 7 && c[0] == '#' && strspn(c.c_str() + 1, "0123456789abcdefABCDEF") == 6)
  {
    unsigned int v = (unsigned int) strtoul(c.c_str() + 1, NULL, 16);
    r = (v >> 16) & 0xff; g = (v >> 8) & 0xff; b = v & 0xff;
  }
  if(what == PVB_A_FG) pvSetPaletteForegroundColor(p, id, r, g, b);
  else                 pvSetPaletteBackgroundColor(p, id, r, g, b);
}

//! parpadeo: alterna el color con el original (cada llamada a pvbAnimate)
inline const char *pvbBlink(int on, const char *color)
{
  if(!on) return color;
  return (pvbTick() % 2) ? "" : ((color && color[0]) ? color : "#FF0000");
}

inline void pvbVisible(PARAM *p, int mask, int id, int visible)
{
  if(!pvbChanged(mask, id, PVB_A_VISIBLE, visible ? "1" : "0")) return;
  if(visible) pvShow(p, id);
  else        pvHide(p, id);
}

inline void pvbValue(PARAM *p, int mask, int id, int value)
{
  char buf[32];
  snprintf(buf, sizeof(buf), "%d", value);
  if(pvbChanged(mask, id, PVB_A_VALUE, buf)) pvSetValue(p, id, value);
}

inline void pvbFloat(PARAM *p, int mask, int id, double value)
{
  char buf[64];
  snprintf(buf, sizeof(buf), "%g", value);
  if(pvbChanged(mask, id, PVB_A_VALUE, buf)) pvDisplayFloat(p, id, (float) value);
}

inline void pvbChecked(PARAM *p, int mask, int id, int on)
{
  if(pvbChanged(mask, id, PVB_A_CHECK, on ? "1" : "0")) pvSetChecked(p, id, on ? 1 : 0);
}

enum { PVB_A_IMAGE = 8, PVB_A_ANIM, PVB_A_FILES };

//! imagen segun el valor (el archivo ya descargado; ver pvbImageAnimation)
inline void pvbImage(PARAM *p, int mask, int id, const char *file)
{
  if(file && file[0] && pvbChanged(mask, id, PVB_A_IMAGE, file)) pvSetImage(p, id, file);
}

//! animacion hecha por pvbrowser mientras "on" (frames = "a.png,b.png,c.png"); al terminar
//! se vuelve a mandar la imagen de pvbImage
inline void pvbImageAnimation(PARAM *p, int mask, int id, int on, const char *frames, int period_ms)
{
  if(!pvbChanged(mask, id, PVB_A_ANIM, on ? "1" : "0")) return;
  pvSetImageAnimation(p, id, frames, on ? period_ms : 0);
  if(!on) pvbCache(mask).erase((long) id * 16 + PVB_A_IMAGE);
}

//! descarga una vez por mascara las imagenes que usan las animaciones ("a.png|b.png")
inline void pvbDownloadImages(PARAM *p, int mask, const char *files)
{
  if(!pvbChanged(mask, 0, PVB_A_FILES, "1")) return;
  std::string s = files ? files : "";
  size_t a = 0;
  while(a <= s.size())
  {
    size_t b = s.find('|', a);
    if(b == std::string::npos) b = s.size();
    if(b > a) pvDownloadFile(p, s.substr(a, b - a).c_str());
    a = b + 1;
  }
}

//! color de mala calidad (dato no leido o daemon detenido)
#define PVB_BAD_COLOR "#C0C0C0"

#endif
