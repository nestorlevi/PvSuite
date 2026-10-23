/***************************************************************************
                          rltag.h  -  description
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
#ifndef _RL_TAG_H_
#define _RL_TAG_H_

#include "rldefine.h"

class rlSharedMemory;

/*! \file rltag.h
\brief Tags: descriptor rlTag y acceso a sus valores en la memoria compartida.

<pre>
Tags (variables con nombre) de los daemons modbus, siemensTCP y OPC UA.

pvdevelop genera, a partir de los "tag" del .mkmodbus / .mksiemens / .mkopcua (o de un .csv),
un descriptor rlTag por variable en TARGET.h. El daemon lee las variables en bloques y las
deja en la memoria compartida; el pvserver las usa por nombre con el cliente correspondiente:

  if(PLC1.getBool(PLC1_FlagTWR)) ...
  PLC1.setBool(PLC1_FlagTWR, 1);
  float t = PLC1.getReal(PLC1_Temperatura);    // con la escala del tag
  if(PLC1.quality(PLC1_Temperatura) == 0) ...  // el bloque no se pudo leer

Los valores se guardan en la memoria compartida en orden de bytes "big endian", como los
mandan Modbus y S7 (el daemon OPC UA tambien los guarda asi).
</pre> */

enum rlTagType
{
  RLTAG_BOOL  = 1,
  RLTAG_BYTE  = 2,  // 8 bits sin signo
  RLTAG_INT   = 3,  // 16 bits con signo
  RLTAG_UINT  = 4,  // 16 bits sin signo (WORD)
  RLTAG_DINT  = 5,  // 32 bits con signo
  RLTAG_UDINT = 6,  // 32 bits sin signo (DWORD)
  RLTAG_REAL  = 7,  // float de 32 bits
  RLTAG_LREAL = 8   // double de 64 bits
};

// orden de las palabras / bytes de los valores de 32 y 64 bits (Modbus; S7 y OPC UA usan ABCD)
enum rlTagOrder
{
  RLTAG_ABCD = 0,   // big endian (Modbus estandar, S7)
  RLTAG_CDAB = 1,   // palabras invertidas (muy comun en PLC con Modbus)
  RLTAG_BADC = 2,   // bytes invertidos en cada palabra
  RLTAG_DCBA = 3    // little endian
};

// area de un tag Modbus (es el codigo de funcion de lectura)
enum rlTagModbusArea
{
  RLTAG_COIL     = 1,
  RLTAG_DISCRETE = 2,
  RLTAG_HOLDING  = 3,
  RLTAG_INPUT    = 4
};

/*! Descriptor de una variable (lo genera pvdevelop en TARGET.h) */
// offsets de los tags de sistema dentro del area TARGET_SYS (ver rltagdef.h y rldaemonstatus.h);
// despues de RLSYS_PLC va un byte por esclavo Modbus / PLC S7 (Esclavo<n> / Plc<n>)
enum rlTagSysOffset
{
  RLSYS_COMUNICACION = 0, RLSYS_COMUNICACION_PARCIAL = 1, RLSYS_CONECTADO = 2,
  RLSYS_BLOQUES_OK = 4, RLSYS_BLOQUES_ERROR = 6, RLSYS_CICLO = 8, RLSYS_TIEMPO_CICLO = 12,
  RLSYS_TIEMPO_CICLO_MAX = 16, RLSYS_ERRORES_LECTURA = 20, RLSYS_ERRORES_ESCRITURA = 24,
  RLSYS_ULTIMO_ERROR = 28, RLSYS_ULTIMA_LECTURA_OK = 32, RLSYS_INICIO = 36, RLSYS_PLC = 40
};

struct rlTag
{
  const char *name;     //!< nombre ("Temp", "Bomba1.Velocidad", "Temp[3]")
  int    type; //!< rlTagType
  int    offset; //!< byte de la memoria compartida donde empieza el valor
  int    size; //!< bytes del valor en la memoria compartida (1, 2, 4 u 8)
  int    bit; //!< RLTAG_BOOL: bit dentro de "size" bytes (0..7 o 0..15); -1 = no
  int    status; //!< byte de la memoria compartida con la calidad (1 = bien); -1 = no hay
  int    rw; //!< 1 = se puede escribir
  int    slave; //!< esclavo Modbus / numero de PLC S7 / indice del tag OPC UA
  int    area; //!< Modbus: rlTagModbusArea; S7: ORG (rlSiemensTCP::ORG_*); OPC UA: 0
  int    dbnum; //!< S7: numero de DB
  int    address; //!< Modbus: registro o coil (base 0); S7: byte
  int    order; //!< rlTagOrder
  double scale; //!< valor = crudo * scale + scale_offset (getReal / setReal)
  double scale_offset;
};

int    rlTagGetBool (rlSharedMemory *shm, const rlTag &tag); //!< 0 o 1
int    rlTagGetInt  (rlSharedMemory *shm, const rlTag &tag); //!< tipos enteros (sin escala)
unsigned int rlTagGetUInt(rlSharedMemory *shm, const rlTag &tag); //!< tipos enteros sin signo (sin escala)
double rlTagGetRaw  (rlSharedMemory *shm, const rlTag &tag); //!< cualquier tipo, sin escala
double rlTagGetValue(rlSharedMemory *shm, const rlTag &tag); //!< cualquier tipo, con escala
int    rlTagQuality (rlSharedMemory *shm, const rlTag &tag); //!< 1 = bien, 0 = error, -1 = sin dato

//! Decodifica tag.size bytes en el orden del tag (como en la memoria compartida); sin escala
double rlTagDecode(const rlTag &tag, const unsigned char *in);
//! Codifica "raw" (sin escala) como lo guarda la memoria compartida / lo manda el protocolo:
//! size bytes en el orden del tag. Devuelve la cantidad de bytes o -1.
int    rlTagEncode(const rlTag &tag, double raw, unsigned char *out);
//! Valor crudo a escribir para un valor con escala (setReal)
double rlTagUnscale(const rlTag &tag, double value);
//! 1 si "raw" (sin escala) cabe en el tipo del tag (INT -32768..32767, UINT 0..65535...), 0 si no
int    rlTagInRange(const rlTag &tag, double raw);

//! Nestor: el bit n (0 = el menos significativo) de un tag entero, como tag bool ("PLC1:Estado.3").
//! Se lee con getBool/rlTagGetBool para cualquier entero (byte, int, uint, dint, udint) y se escribe
//! con setBool en los de 8 y 16 bits (Modbus: el bit del registro; Siemens: el byte que lo contiene).
//! Devuelve 0, o -1 si el tag no es entero o n esta fuera de 0..bits-1 (*bit queda sin cambiar).
int    rlTagBit(const rlTag &word, int n, rlTag *bit);
//! Lo mismo sin comprobar (el codigo que genera pvdevelop ya lo comprobo)
inline rlTag rlTagBitOf(const rlTag &word, int n) { rlTag b = word; b.type = RLTAG_BOOL; b.bit = n; return b; }

//! Guarda "raw" (sin escala) en la memoria compartida (lo usan los daemons para los tags de
//! sistema). Solo para valores que ocupan sus bytes completos (no un bit dentro de un registro).
int    rlTagPut(rlSharedMemory *shm, const rlTag &tag, double raw);

//! true si el valor del tag cambio en los ultimos "seconds" segundos. Con el tag TARGET_SYS.Ciclo
//! indica si el daemon esta vivo (el contador avanza en cada ciclo). Mientras no se lo vio cambiar
//! da false (la memoria compartida conserva el ultimo valor de un daemon cerrado).
int    rlTagAlive(rlSharedMemory *shm, const rlTag &tag, int seconds = 5);
//! Lo mismo con tres estados: 1 = vivo, 0 = detenido (no cambio en "seconds" s), -1 = todavia no se sabe
//! (primeras lecturas: no cambio todavia y no paso el plazo)
int    rlTagAliveState(rlSharedMemory *shm, const rlTag &tag, int seconds = 5);

//! Busca un tag por nombre en la tabla TARGET_tags[] que genera pvdevelop
const rlTag *rlTagFind(const rlTag * const *table, int count, const char *name);

#endif
