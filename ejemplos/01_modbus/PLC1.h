// Modbus Signal assignment of PLC1
#define PLC1_SHARED_MEMORY "automation/shm/plc1.shm"
#define PLC1_MAILBOX "automation/mbx/plc1.mbx"

#define PLC1_CYCLE1_BASE 0
#define PLC1_CYCLE1_NUM 2
#define PLC1_CYCLE1_NUM_BYTES 4

#define PLC1_LIFE_COUNTER_BASE 4
#define PLC1_READ_ERROR_COUNT_BASE 6
#define PLC1_WRITE_ERROR_COUNT_BASE 8

#define PLC1_SHARED_MEMORY_SIZE 88

// Tags (generado por pvdevelop; no editar: se regenera)
// Uso: PLC1.getBool(PLC1_<tag>), getInt, getUInt, getReal, setBool, setInt, setReal, quality
#include "rltag.h"
// 4 bloques de lectura:
//   esclavo=1 funcion=3 desde=299 cantidad=2 scan=0ms -> offset=10 bytes=4 calidad=43
//   esclavo=1 funcion=3 desde=310 cantidad=2 scan=0ms -> offset=14 bytes=4 calidad=44
//   esclavo=1 funcion=3 desde=399 cantidad=12 scan=0ms -> offset=18 bytes=24 calidad=45
//   esclavo=1 funcion=1 desde=5 cantidad=1 scan=0ms -> offset=42 bytes=1 calidad=46
struct PLC1_Motor
{
  rlTag Marcha;
  rlTag Velocidad;
};
static const rlTag PLC1_FlagTWR = {"FlagTWR", RLTAG_BOOL, 10, 2, 15, 43, 1, 1, 3, 0, 299, RLTAG_CDAB, 1, 0}; // 40300.15  Control desde torre
static const rlTag PLC1_Brillo = {"Brillo", RLTAG_INT, 12, 2, -1, 43, 1, 1, 3, 0, 300, RLTAG_CDAB, 1, 0}; // 40301
static const rlTag PLC1_Caudal = {"Caudal", RLTAG_REAL, 14, 4, -1, 44, 1, 1, 3, 0, 310, RLTAG_CDAB, 1, 0}; // 40311
static const rlTag PLC1_Bomba = {"Bomba", RLTAG_BOOL, 42, 1, 0, 46, 1, 1, 1, 0, 5, RLTAG_CDAB, 1, 0}; // 00006  Bomba principal
static const PLC1_Motor PLC1_Motores[2] =
{
  {
    {"Motores[0].Marcha", RLTAG_BOOL, 18, 2, 0, 45, 1, 1, 3, 0, 399, RLTAG_CDAB, 1, 0}, // 40400.0
    {"Motores[0].Velocidad", RLTAG_INT, 20, 2, -1, 45, 1, 1, 3, 0, 400, RLTAG_CDAB, 1, 0} // 40401
  },
  {
    {"Motores[1].Marcha", RLTAG_BOOL, 38, 2, 0, 45, 1, 1, 3, 0, 409, RLTAG_CDAB, 1, 0}, // 40410.0
    {"Motores[1].Velocidad", RLTAG_INT, 40, 2, -1, 45, 1, 1, 3, 0, 410, RLTAG_CDAB, 1, 0} // 40411
  }
};
#define PLC1_Motores_COUNT 2
#define PLC1_TAG_COUNT 8
static const rlTag * const PLC1_tags[] = {
  &PLC1_FlagTWR,
  &PLC1_Brillo,
  &PLC1_Caudal,
  &PLC1_Bomba,
  &PLC1_Motores[0].Marcha,
  &PLC1_Motores[0].Velocidad,
  &PLC1_Motores[1].Marcha,
  &PLC1_Motores[1].Velocidad
};

// Sin prefijo: using namespace tags_PLC1;  ->  PLC1.getReal(<tag>)
namespace tags_PLC1
{
  typedef PLC1_Motor Motor;
#ifndef FlagTWR
  static const rlTag &FlagTWR = PLC1_FlagTWR;
#endif
#ifndef Brillo
  static const rlTag &Brillo = PLC1_Brillo;
#endif
#ifndef Caudal
  static const rlTag &Caudal = PLC1_Caudal;
#endif
#ifndef Bomba
  static const rlTag &Bomba = PLC1_Bomba;
#endif
#ifndef Motores
  static const PLC1_Motor (&Motores)[2] = PLC1_Motores;
  const int Motores_COUNT = 2;
#endif
}

// Tags de sistema (los actualiza el daemon): PLC1.getBool(PLC1_SYS.Comunicacion), PLC1.daemonAlive(PLC1_SYS.Ciclo)
#define PLC1_SYS_BASE 47
struct PLC1_SysTags
{
  rlTag Comunicacion;
  rlTag ComunicacionParcial;
  rlTag Conectado;
  rlTag BloquesOk;
  rlTag BloquesError;
  rlTag Ciclo;
  rlTag TiempoCiclo;
  rlTag TiempoCicloMax;
  rlTag ErroresLectura;
  rlTag ErroresEscritura;
  rlTag UltimoError;
  rlTag UltimaLecturaOk;
  rlTag Inicio;
  rlTag Esclavo1;
};
static const PLC1_SysTags PLC1_SYS =
{
  {"SYS.Comunicacion", RLTAG_BOOL, 47, 1, 0, -1, 0, 0, 0, 0, 0, RLTAG_ABCD, 1, 0}, // Todos los bloques del ultimo ciclo se leyeron bien
  {"SYS.ComunicacionParcial", RLTAG_BOOL, 48, 1, 0, -1, 0, 0, 0, 0, 0, RLTAG_ABCD, 1, 0}, // Algun bloque se leyo bien y alguno fallo
  {"SYS.Conectado", RLTAG_BOOL, 49, 1, 0, -1, 0, 0, 0, 0, 0, RLTAG_ABCD, 1, 0}, // Conexion abierta
  {"SYS.BloquesOk", RLTAG_UINT, 51, 2, -1, -1, 0, 0, 0, 0, 0, RLTAG_ABCD, 1, 0}, // Bloques leidos bien en el ultimo ciclo
  {"SYS.BloquesError", RLTAG_UINT, 53, 2, -1, -1, 0, 0, 0, 0, 0, RLTAG_ABCD, 1, 0}, // Bloques con error en el ultimo ciclo
  {"SYS.Ciclo", RLTAG_UDINT, 55, 4, -1, -1, 0, 0, 0, 0, 0, RLTAG_ABCD, 1, 0}, // Numero de ciclo
  {"SYS.TiempoCiclo", RLTAG_UDINT, 59, 4, -1, -1, 0, 0, 0, 0, 0, RLTAG_ABCD, 1, 0}, // Duracion del ultimo ciclo (ms)
  {"SYS.TiempoCicloMax", RLTAG_UDINT, 63, 4, -1, -1, 0, 0, 0, 0, 0, RLTAG_ABCD, 1, 0}, // Duracion maxima de un ciclo (ms)
  {"SYS.ErroresLectura", RLTAG_UDINT, 67, 4, -1, -1, 0, 0, 0, 0, 0, RLTAG_ABCD, 1, 0}, // Errores de lectura desde el inicio
  {"SYS.ErroresEscritura", RLTAG_UDINT, 71, 4, -1, -1, 0, 0, 0, 0, 0, RLTAG_ABCD, 1, 0}, // Errores de escritura desde el inicio
  {"SYS.UltimoError", RLTAG_DINT, 75, 4, -1, -1, 0, 0, 0, 0, 0, RLTAG_ABCD, 1, 0}, // Codigo del ultimo error (0 = ninguno)
  {"SYS.UltimaLecturaOk", RLTAG_UDINT, 79, 4, -1, -1, 0, 0, 0, 0, 0, RLTAG_ABCD, 1, 0}, // Hora de la ultima lectura correcta (s desde 1970, UTC)
  {"SYS.Inicio", RLTAG_UDINT, 83, 4, -1, -1, 0, 0, 0, 0, 0, RLTAG_ABCD, 1, 0}, // Hora de inicio del daemon (s desde 1970, UTC)
  {"SYS.Esclavo1", RLTAG_BOOL, 87, 1, 0, -1, 0, 1, 0, 0, 0, RLTAG_ABCD, 1, 0} // El esclavo 1 respondio en el ultimo ciclo
};
namespace tags_PLC1 { static const PLC1_SysTags &SYS = PLC1_SYS; }
