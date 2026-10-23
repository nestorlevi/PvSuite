//###############################################################
//# mask1_pvb.h: acciones y animaciones (pvdevelop: Propiedades > Accion y animacion)
//# Generado por pvdevelop: no editar, se regenera al guardar la mascara.
//# El codigo propio va en mask1_slots.h, antes o despues de la linea pvbActions de cada slot.
//###############################################################
//pvb: btnBomba press=write:PLC1:Bomba=1 release=write:PLC1:Bomba=0 color=PLC1:Bomba colors="0=#808080;1=#00C000"
//pvb: btnMas click=add:PLC1:Motores[0].Velocidad=5 max=100
//pvb: btnMenos click=sub:PLC1:Motores[0].Velocidad=5 min=0
//pvb: lblEstado text=PLC1:Motores[0].Marcha fmt="Parado|Marcha" color=PLC1:Motores[0].Marcha colors="0=#C0C0C0;1=#00C000"
//pvb: lblCaudal text=PLC1:Caudal fmt="%.1f m3/h" color=PLC1:Caudal colors=">80=#FF0000;else=" colorpart=fg
//pvb: edtBrillo enter=PLC1:Brillo min=0 max=100
//pvb: sldVel enter=PLC1:Motores[1].Velocidad value=PLC1:Motores[1].Velocidad min=0 max=100
//pvb: barBrillo value=PLC1:Brillo
//pvb: chkBomba enter=PLC1:Bomba value=PLC1:Bomba
//pvb: lcdCaudal value=PLC1:Caudal
//pvb: lblCiclo text=PLC1:SYS.Ciclo fmt="ciclo %.0f"
//pvb: imgBomba image=PLC1:Bomba images="imagenes/off.png|imagenes/on.png" anim=PLC1:FlagTWR frames="imagenes/f1.png|imagenes/f2.png|imagenes/f3.png" period=300
#include "pvbind.h"
static void pvbAnimate(PARAM *p);
static inline int pvbAnimateBase(PARAM *p, void *) { pvbAnimate(p); return 0; }   // la mascara sigue al dia con una ventana modal abierta

static int pvbDo(PARAM *p, int id, int event, int confirmed, const char *text, int val)
{
  (void) p; (void) event; (void) confirmed; (void) text; (void) val;
  switch(id)
  {
    case btnBomba:
      if(event == PVB_PRESS) { { double pvbNew = (((1) != 0) ? 1.0 : 0.0); PLC1.setBool(PLC1_Bomba, ((pvbNew) != 0) ? 1 : 0); } return 0; }
      if(event == PVB_RELEASE) { { double pvbNew = (((0) != 0) ? 1.0 : 0.0); PLC1.setBool(PLC1_Bomba, ((pvbNew) != 0) ? 1 : 0); } return 0; }
      break;
    case btnMas:
      if(event == PVB_CLICK) { double v = pvbClamp((double) PLC1.getInt(PLC1_Motores[0].Velocidad) + (5), NAN, 100); { double pvbNew = (double) llround(v); PLC1.setInt(PLC1_Motores[0].Velocidad, (int) llround(pvbNew)); } return 0; }
      break;
    case btnMenos:
      if(event == PVB_CLICK) { double v = pvbClamp((double) PLC1.getInt(PLC1_Motores[0].Velocidad) - (5), 0, NAN); { double pvbNew = (double) llround(v); PLC1.setInt(PLC1_Motores[0].Velocidad, (int) llround(pvbNew)); } return 0; }
      break;
    case edtBrillo:
      if(event == PVB_TEXT) { pvbEditText(id) = text ? text : ""; return 0; }
      if(event == PVB_CLICK) { const char *t = confirmed ? text : pvbEditText(id).c_str(); int ok; double v = pvbNumber(t, &ok); if(!ok) { pvbMessage(p, "Valor no válido"); return -1; } v = pvbClamp(v, 0, 100); { double pvbNew = (double) llround(v); PLC1.setInt(PLC1_Brillo, (int) llround(pvbNew)); } return 0; }
      break;
    case sldVel:
      if(event == PVB_SLIDER) { { double pvbNew = (double) llround(pvbClamp(val, 0, 100)); PLC1.setInt(PLC1_Motores[1].Velocidad, (int) llround(pvbNew)); } return 0; }
      break;
    case chkBomba:
      if(event == PVB_CHECK) { { double pvbNew = ((((text && strchr(text, '1')) ? 1 : 0) != 0) ? 1.0 : 0.0); PLC1.setBool(PLC1_Bomba, ((pvbNew) != 0) ? 1 : 0); } return 0; }
      break;
    default:
      break;
  }
  return 0;
}

// r > 0: ir a la mascara r   r < 0: la accion no se hizo (condicion, confirmacion pendiente)   0: seguir
static int pvbActions(PARAM *p, int id, int event, const char *text, int val)
{
  if(event == PVB_SLIDER && id >= PVB_CONFIRM_BASE - 1)   // respuesta a "Confirmar" o a un aviso
  {
    if(id == PVB_CONFIRM_BASE - 1) return -1;
    PvbPending pd = pvbPending();
    pvbPending().id = 0;
    if(pd.id != id - PVB_CONFIRM_BASE || val != MessageBoxYes) return -1;
    return pvbDo(p, pd.id, pd.event, 1, pd.text.c_str(), pd.val);
  }
  return pvbDo(p, id, event, 0, text, val);
}

static void pvbAnimate(PARAM *p)
{
  const int mask = 1;
  (void) p; (void) mask;
  pvbTick()++;
  pvbDownloadImages(p, mask, "imagenes/off.png|imagenes/on.png|imagenes/f1.png|imagenes/f2.png|imagenes/f3.png");
  { double v = (double) PLC1.getBool(PLC1_Bomba); const char *c = ""; if(v == 0) c = "#808080"; else if(v == 1) c = "#00C000"; else c = ""; if(!(PLC1.quality(PLC1_Bomba) != 0 && PLC1.daemonAlive(PLC1_SYS.Ciclo))) c = PVB_BAD_COLOR; pvbColor(p, mask, btnBomba, PVB_A_BG, pvbBlink(0, c)); }
  { int q = (PLC1.quality(PLC1_Motores[0].Marcha) != 0 && PLC1.daemonAlive(PLC1_SYS.Ciclo)); pvbSetText(p, mask, lblEstado, q ? pvbChoice("Parado|Marcha", (double) PLC1.getBool(PLC1_Motores[0].Marcha)).c_str() : "---"); }
  { double v = (double) PLC1.getBool(PLC1_Motores[0].Marcha); const char *c = ""; if(v == 0) c = "#C0C0C0"; else if(v == 1) c = "#00C000"; else c = ""; if(!(PLC1.quality(PLC1_Motores[0].Marcha) != 0 && PLC1.daemonAlive(PLC1_SYS.Ciclo))) c = PVB_BAD_COLOR; pvbColor(p, mask, lblEstado, PVB_A_BG, pvbBlink(0, c)); }
  pvbText(p, mask, lblCaudal, "%.1f m3/h", (double) PLC1.getReal(PLC1_Caudal), (PLC1.quality(PLC1_Caudal) != 0 && PLC1.daemonAlive(PLC1_SYS.Ciclo)));
  { double v = (double) PLC1.getReal(PLC1_Caudal); const char *c = ""; if(v > 80) c = "#FF0000"; else c = ""; if(!(PLC1.quality(PLC1_Caudal) != 0 && PLC1.daemonAlive(PLC1_SYS.Ciclo))) c = PVB_BAD_COLOR; pvbColor(p, mask, lblCaudal, PVB_A_FG, pvbBlink(0, c)); }
  pvbValue(p, mask, sldVel, (int) lround((double) PLC1.getInt(PLC1_Motores[1].Velocidad)));
  pvbColor(p, mask, sldVel, PVB_A_BG, (PLC1.quality(PLC1_Motores[1].Velocidad) != 0 && PLC1.daemonAlive(PLC1_SYS.Ciclo)) ? "" : PVB_BAD_COLOR);  // gris si el dato es malo
  pvbValue(p, mask, barBrillo, (int) lround((double) PLC1.getInt(PLC1_Brillo)));
  pvbColor(p, mask, barBrillo, PVB_A_BG, (PLC1.quality(PLC1_Brillo) != 0 && PLC1.daemonAlive(PLC1_SYS.Ciclo)) ? "" : PVB_BAD_COLOR);  // gris si el dato es malo
  pvbChecked(p, mask, chkBomba, (double) PLC1.getBool(PLC1_Bomba) != 0);
  pvbColor(p, mask, chkBomba, PVB_A_BG, (PLC1.quality(PLC1_Bomba) != 0 && PLC1.daemonAlive(PLC1_SYS.Ciclo)) ? "" : PVB_BAD_COLOR);  // gris si el dato es malo
  pvbFloat(p, mask, lcdCaudal, (double) PLC1.getReal(PLC1_Caudal));
  pvbColor(p, mask, lcdCaudal, PVB_A_BG, (PLC1.quality(PLC1_Caudal) != 0 && PLC1.daemonAlive(PLC1_SYS.Ciclo)) ? "" : PVB_BAD_COLOR);  // gris si el dato es malo
  pvbText(p, mask, lblCiclo, "ciclo %.0f", (double) PLC1.getUInt(PLC1_SYS.Ciclo), (PLC1.quality(PLC1_SYS.Ciclo) != 0 && PLC1.daemonAlive(PLC1_SYS.Ciclo)));
  pvbColor(p, mask, lblCiclo, PVB_A_BG, (PLC1.quality(PLC1_SYS.Ciclo) != 0 && PLC1.daemonAlive(PLC1_SYS.Ciclo)) ? "" : PVB_BAD_COLOR);  // gris si el dato es malo
  { int a = ((double) PLC1.getBool(PLC1_FlagTWR) != 0) ? 1 : 0; pvbImageAnimation(p, mask, imgBomba, a, "imagenes/f1.png,imagenes/f2.png,imagenes/f3.png", 300); if(!a) pvbImage(p, mask, imgBomba, pvbChoice("imagenes/off.png|imagenes/on.png", (double) PLC1.getBool(PLC1_Bomba)).c_str()); }
}
