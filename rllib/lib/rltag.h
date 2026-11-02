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
  RLTAG_LREAL = 8,  // double de 64 bits
  RLTAG_LINT  = 9,  // 64 bits con signo (getLInt / setLInt)
  RLTAG_ULINT = 10, // 64 bits sin signo
  RLTAG_STRING = 11,// texto de "size" caracteres (len= en el .mk): dos por registro en Modbus,
                    // con BADC / DCBA los dos bytes de cada registro van invertidos
  RLTAG_LAST_TYPE = 11,
  RLTAG_BLOCK = 100 // no es un tipo de variable: el elemento entero de una estructura (miembro _all
                    // que genera pvdevelop) para setBlock / getBlock / clear
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
  int    count; //!< RLTAG_BLOCK: registros / coils del elemento (los demas tipos: 0)
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

//! Nestor: 64 bits sin pasar por double (lint / ulint; los demas enteros tambien)
long long rlTagDecodeInt64(const rlTag &tag, const unsigned char *in);
int       rlTagEncodeInt64(const rlTag &tag, long long raw, unsigned char *out); //!< bytes o -1
long long rlTagGetInt64(rlSharedMemory *shm, const rlTag &tag);

//! Nestor: texto (RLTAG_STRING). Lee hasta el primer 0 (o tag.size caracteres) en buf (maxlen con el 0 final);
//! devuelve el largo o -1
int    rlTagGetString(rlSharedMemory *shm, const rlTag &tag, char *buf, int maxlen);
int    rlTagDecodeString(const rlTag &tag, const unsigned char *in, char *buf, int maxlen);
//! Codifica "text" como lo guarda la memoria / lo manda el protocolo: tag.size bytes (Modbus: redondeado a
//! registros) completados con 0. Devuelve los bytes, o -1 si el texto no entra en el tag.
int    rlTagEncodeString(const rlTag &tag, const char *text, unsigned char *out, int outlen);
//! Bytes que ocupa el valor en la memoria compartida (texto en Modbus: redondeado a registros enteros)
int    rlTagBytes(const rlTag &tag);

/*! <pre>
Nestor: pedidos al daemon con respuesta (Modbus): setIntWait, refresh...
El final de la memoria compartida (RLTAG_TRAILER_SIZE bytes, lo reserva rlTagList::layoutSys) tiene:
  contador de escrituras (4 bytes): el daemon lo incrementa antes y despues de escribir un bloque leido
    (impar = escribiendo); getBlock copia de nuevo si cambio durante la copia
  RLTAG_ACK_SLOTS respuestas de 8 bytes: [resultado 4][id 4] en el lugar id % RLTAG_ACK_SLOTS
  hora (4 bytes, s desde 1970): la renueva el daemon en cada vuelta (rlTagTrailerAlive)
  "RLTG" (4 bytes): lo escribe el daemon al arrancar
  Sin la marca o con la hora de hace mas de RLTAG_ALIVE_S segundos (daemon detenido o uno anterior que no
  entiende estos pedidos) el cliente no manda nada: RLTAG_ERROR enseguida.
Mensajes del mailbox (ademas de [esclavo][funcion][datos] de siempre):
  [esclavo][RLTAG_MBX_REQUEST][id 4][funcion][datos]   la escritura de siempre, con respuesta
  [esclavo][RLTAG_MBX_REFRESH][id 4][offset 4][bytes 4] leer ya los bloques que contienen ese tramo de la memoria
</pre> */
enum rlTagMailbox { RLTAG_MBX_REQUEST = 0xF0, RLTAG_MBX_REFRESH = 0xF1 };
enum rlTagResult  { RLTAG_OK = 0, RLTAG_ERROR = -1, RLTAG_TIMEOUT = -2 };
#define RLTAG_ACK_SLOTS   256
#define RLTAG_TRAILER_SIZE (4 + 8 * RLTAG_ACK_SLOTS + 4 + 4)
#define RLTAG_ALIVE_S 30
#define RLTAG_TRAILER_MAGIC 0x524C5447  /* "RLTG" */

//! id nuevo para un pedido (distinto de 0; distinto entre procesos y llamadas)
unsigned int rlTagNewRequestId();
//! Espera la respuesta "id" hasta timeout_ms; devuelve RLTAG_OK, RLTAG_ERROR (el PLC no la acepto) o RLTAG_TIMEOUT
int    rlTagWaitAck(rlSharedMemory *shm, int shmsize, unsigned int id, int timeout_ms);
//! 1 si el daemon dejo la marca "RLTG" y renovo la hora hace menos de RLTAG_ALIVE_S s (responde los pedidos con id)
int    rlTagHasTrailer(rlSharedMemory *shm, int shmsize);
//! Daemon: marca "RLTG", hora y respuestas en cero (al arrancar)
void   rlTagTrailerInit(rlSharedMemory *shm, int shmsize);
//! Daemon: renueva la hora (en cada vuelta)
void   rlTagTrailerAlive(rlSharedMemory *shm, int shmsize);
//! Daemon: anota la respuesta de un pedido
void   rlTagAck(rlSharedMemory *shm, int shmsize, unsigned int id, int result);
//! Daemon: escribe un bloque leido con el contador de escrituras (lo que lee rlTagReadCoherent)
int    rlTagWriteBlock(rlSharedMemory *shm, int shmsize, int offset, const void *data, int len);
//! Copia len bytes de la memoria sin que el daemon escriba un bloque en el medio (reintenta)
int    rlTagReadCoherent(rlSharedMemory *shm, int shmsize, int offset, void *buf, int len);

//! Nestor: valores de un elemento entero (RLTAG_BLOCK) o de un tag, en registros (0..65535) o coils (0/1),
//! coherente (rlTagReadCoherent). Devuelve la cantidad o -1 (max chico, elemento en varios bloques: offset -1).
int    rlTagGetBlock(rlSharedMemory *shm, int shmsize, const rlTag &tag, int *values, int max);
//! Registros (o coils) que ocupa un tag o un elemento en el PLC
int    rlTagUnits(const rlTag &tag);

//! Nestor: 1 si el valor (o la calidad) del tag cambio desde la ultima vez que este hilo lo miro (en el
//! pvserver, cada cliente tiene su hilo); la primera vez da 1. Se olvida solo cuando termina el hilo.
int    rlTagChanged(rlSharedMemory *shm, const rlTag &tag);

#endif
