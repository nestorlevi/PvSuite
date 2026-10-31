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
** Nestor: acciones y animaciones de los widgets (Designer > Propiedades > Accion y animacion).
**
** Cada widget guarda sus enlaces en la propiedad dinamica "pvbBind" (clave=valor):
**   click / press / release = write:PLC1:Motor1.Marcha=1 | toggle:TAG | add:TAG=5 | sub:TAG=5
**                             | goto:3 | back | home
**   enter=TAG          escribe lo ingresado (LineEdit: texto, Slider/Dial/SpinBox: valor, CheckBox)
**   min= max=          limites de add/sub/enter
**   confirm="texto"    pregunta antes (click y enter)
**   cond="TAG == 0" condmsg="texto"   la accion solo se hace si se cumple
**   text=TAG fmt="%.1f C"   (o fmt="Parado|Marcha|Falla" segun el valor)
**   value=TAG          ProgressBar, Slider, Dial, SpinBox, LCDNumber, CheckBox, RadioButton
**   color=TAG colors="0=#808080;1=#00C000;>80=#FF0000;else=" colorpart=bg|fg
**   blink=TAG          parpadea mientras el tag es 1     visible=TAG | !TAG
**   quality=0          no marcar en gris los datos malos (por defecto se marcan)
**   image=TAG images="parado.png|marcha.png|falla.png"    imagen segun el valor (QImage)
**   anim=TAG frames="a.png|b.png|c.png" period=300       pvbrowser anima mientras el tag != 0
** TAG = "<daemon>:<tag>" (PLC1:Motor1.Marcha); en el codigo: PLC1_Motor1.Marcha con el cliente PLC1.
**
** Al guardar la mascara se genera maskN_pvb.h (pvbActions y pvbAnimate, ver pvserver/pvbind.h),
** se agrega #include "maskN_pvb.h" en maskN.cpp y una linea en cada slot (una sola vez).
****************************************************************************/
#ifndef PVBINDGEN_H
#define PVBINDGEN_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
class QWidget;

// de los .tagmap del proyecto (address: plcaddr=, "%MW0.0"; protocol: modbus, siemens)
// bit: el bit de un entero que se pidio con "PLC1:Estado.3" (lo arma el generador; -1 = el tag entero)
struct PvbTag { QString source, name, type, address, protocol; int bit = -1; bool rw = true;   // rw: rw= del .tagmap (0 = solo lectura)
               QString desc, unit, alarm, alarmText; };   // Nestor: desc=, unit=, alarm=, alarm_text= (pvtaginfo.h)

QList<PvbTag> pvbProjectTags();                        // todos los tags (con los de sistema)
QString pvbBinding(QWidget *w);                        // "clave=valor ..."
void    pvbSetBinding(QWidget *w, const QString &pairs);
void    pvbLoadBindings(const QString &maskCpp, QWidget *root);   // al abrir la mascara
int     pvbSaveBindings(const QString &maskCpp, QWidget *root);   // al guardarla (-1 = error)
// fmt= de text=: vacio, una lista "Parado|Marcha" o un printf con una sola conversion de double (%.1f, %g, %lf;
// %% es el signo). Lo mismo que acepta pvbDoubleFormat (pvserver/pvbind.h); si no, el pvserver usa "%g"
bool    pvbValidTextFormat(const QString &fmt);
// clave=valor separados por espacios ("" = una comilla): devuelve clave, valor, clave, valor...
QStringList pvbSplitPairs(const QString &line, QString *err = nullptr);

// Nestor: los clientes de los daemons (rlModbusClient PLC1...) en pvapp.h y main.cpp, y rllib en el .pro: agrega
// lo que falta sin tocar lo que ya esta. Devuelve los avisos; added = lo que agrego (para la consola)
QStringList pvbEnsureDaemonClients(const QString &project = QString(), QStringList *added = nullptr);

#endif
