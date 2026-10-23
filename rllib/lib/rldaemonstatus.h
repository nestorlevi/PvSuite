/***************************************************************************
                       rldaemonstatus.h  -  description
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
#ifndef _RL_DAEMON_STATUS_H_
#define _RL_DAEMON_STATUS_H_

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <chrono>
#include <mutex>
#include <vector>
#include "rlsharedmemory.h"
#include "rltag.h"

/*! \file rldaemonstatus.h
\brief Numero de ciclo, mensajes de consola y tags de sistema (TARGET_SYS) de un daemon.

<pre>
Lo usan los daemons que genera pvdevelop (modbus, siemensTCP) y rlopcuadaemon. Solo cabecera.

Mensajes en la consola (console= en el .mk, o -q / -n / -v al ejecutar el daemon):
  quiet    solo errores: cuando un bloque deja de leerse (y cuando vuelve), errores de escritura
  normal   lo mismo y un resumen cada console_interval segundos
  verbose  cada bloque:  [ciclo 1234] bloque 3/12 esclavo 1 FC3 40001..40010 ok 4 ms
           y el fin de cada ciclo con su duracion

Tags de sistema: en sysBase de la memoria compartida (ver rltagdef.h, "Tags de sistema"), en el
orden de rlTagSysOffset; despues un byte por esclavo / PLC (Esclavo<n> / Plc<n>).

  rlDaemonStatus status(&shm, PLC1_SYS_BASE, NUM_BLOCKS, DAEMON_CONSOLE, 10, "esclavo");
  status.setPlcs(plcs, 2);           // los esclavos en el orden de PLC1_SYS.Esclavo<n>
  status.start(ac, av);              // -q -n -v, hora de inicio
  while(1)
  {
    status.beginCycle();
    ret = leer bloque ...; status.block(i, esclavo, "FC3 40001..40010", ret, ms);  // ret > 0 = bien
    status.endCycle(conectado);
  }
  en el hilo que escribe: status.write(esclavo, "FC6 adr 10", ret);              // ret < 0 = error
</pre> */

class rlDaemonStatus
{
public:
  enum Level { QUIET = 0, NORMAL = 1, VERBOSE = 2 };

  //! sysBase < 0: no hay tags de sistema (solo mensajes). plcWord: "esclavo", "plc"...
  rlDaemonStatus(rlSharedMemory *shm, int sysBase, int numBlocks, int level = NORMAL, int interval = 10, const char *plcWord = "esclavo")
  {
    this->shm = shm; base = sysBase; this->level = level; this->interval = (interval > 0) ? interval : 10;
    word = plcWord; cycle = 0; notifications = 0; reportConnection = 1; blocksOk = blocksError = 0; lastError = 0;
    readErrors = writeErrors = 0; cycleMs = cycleMax = 0; lastOk = 0; started = 0; connected = -1;
    cycleStart = now(); summaryStart = cycleStart; summaryCycles = 0; summaryErrors = 0;
    state.assign(numBlocks > 0 ? numBlocks : 0, -1);
    this->numBlocks = numBlocks;
  }

  //! esclavos / PLC en el orden de sus tags TARGET_SYS.Esclavo<n> / Plc<n>
  void setPlcs(const int *list, int n)
  {
    plcs.assign(list, list + n);
    plcState.assign(n, -1);
  }

  //! -q (quiet), -n (normal), -v (verbose) en la linea de comandos; guarda la hora de inicio
  void start(int ac, char **av)
  {
    for(int i=1; i<ac; i++)
    {
      if     (strcmp(av[i], "-q") == 0) level = QUIET;
      else if(strcmp(av[i], "-n") == 0) level = NORMAL;
      else if(strcmp(av[i], "-v") == 0) level = VERBOSE;
    }
    started = (unsigned int) time(NULL);
    // la memoria compartida conserva los valores de la ejecucion anterior
    for(int off = RLSYS_COMUNICACION; off < RLSYS_PLC + (int) plcs.size(); off++) put8(off, 0);
    put32(RLSYS_INICIO, started);
    static const char *names[] = { "quiet", "normal", "verbose" };
    printf("consola: %s (-q solo errores, -n resumen cada %d s, -v cada bloque)\n", names[(level >= 0 && level <= 2) ? level : 1], interval);
    fflush(stdout);
  }

  void beginCycle()
  {
    std::lock_guard<std::mutex> lock(mutex);
    cycle++;
    blocksOk = blocksError = 0;
    cycleStart = now();
    for(size_t i=0; i<plcState.size(); i++) plcState[i] = -1;
  }

  //! resultado de leer un bloque: ret > 0 = bien (bytes leidos); ms = lo que tardo
  void block(int index, int plc, const char *what, int ret, long long ms)
  {
    std::lock_guard<std::mutex> lock(mutex);
    bool ok = (ret > 0);
    if(ok) { blocksOk++; lastOk = (unsigned int) time(NULL); }
    else
    {
      blocksError++; readErrors++; summaryErrors++; lastError = (ret == 0) ? -1 : ret;
      put8(RLSYS_COMUNICACION, 0); // enseguida: sin PLC un ciclo puede tardar varios segundos
      put32(RLSYS_ERRORES_LECTURA, readErrors);
      put32(RLSYS_ULTIMO_ERROR, (unsigned int) lastError);
    }
    for(size_t i=0; i<plcs.size(); i++)
    {
      if(plcs[i] != plc) continue;
      if(ok) plcState[i] = 1;
      else if(plcState[i] < 0) plcState[i] = 0;
    }
    signed char before = (index >= 0 && index < numBlocks) ? state[index] : -1;
    if(index >= 0 && index < numBlocks) state[index] = ok ? 1 : 0;
    if(level >= VERBOSE)
    {
      if(ok) printf("[ciclo %u] bloque %d/%d %s %d %s ok %lld ms\n", cycle, index + 1, numBlocks, word, plc, what, ms);
      else   printf("[ciclo %u] bloque %d/%d %s %d %s ERROR ret=%d %lld ms\n", cycle, index + 1, numBlocks, word, plc, what, ret, ms);
    }
    else if(!ok && before != 0)
      printf("[ciclo %u] ERROR bloque %d/%d %s %d %s ret=%d\n", cycle, index + 1, numBlocks, word, plc, what, ret);
    else if(ok && before == 0)
      printf("[ciclo %u] bloque %d/%d %s %d %s: se lee de nuevo\n", cycle, index + 1, numBlocks, word, plc, what);
    fflush(stdout);
  }

  //! daemons con suscripciones (OPC UA): una notificacion de un valor (index = tag); se numeran.
  //! En el ciclo cuentan como bloques (BloquesOk / BloquesError = notificaciones buenas / malas).
  void notification(int index, const char *name, int ok, double value)
  {
    std::lock_guard<std::mutex> lock(mutex);
    notifications++;
    if(ok) { blocksOk++; lastOk = (unsigned int) time(NULL); }
    else   { blocksError++; readErrors++; summaryErrors++; lastError = -1; put8(RLSYS_COMUNICACION, 0); }
    signed char before = (index >= 0 && index < numBlocks) ? state[index] : -1;
    if(index >= 0 && index < numBlocks) state[index] = ok ? 1 : 0;
    if(level >= VERBOSE)
      printf("[ciclo %u] notificacion %u: %s = %g %s\n", cycle, notifications, name, value, ok ? "ok" : "ERROR");
    else if(!ok && before != 0)
      printf("[ciclo %u] notificacion %u: ERROR %s (valor malo)\n", cycle, notifications, name);
    else if(ok && before == 0)
      printf("[ciclo %u] notificacion %u: %s: valor bueno de nuevo\n", cycle, notifications, name);
    fflush(stdout);
  }

  //! resultado de una escritura (pedido del mailbox): ret < 0 = error
  void write(int plc, const char *what, int ret)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if(ret < 0) { writeErrors++; lastError = ret; }
    if(ret < 0)                 printf("[ciclo %u] ERROR escritura %s %d %s ret=%d\n", cycle, word, plc, what, ret);
    else if(level >= VERBOSE)   printf("[ciclo %u] escritura %s %d %s ok\n", cycle, word, plc, what);
    fflush(stdout);
  }

  //! fin del ciclo: actualiza los tags de sistema; conectado = socket / puerto / sesion abierta (-1 = no se sabe)
  void endCycle(int isConnected = -1)
  {
    std::lock_guard<std::mutex> lock(mutex);
    long long t = now();
    cycleMs = (unsigned int) (t - cycleStart);
    if(cycleMs > cycleMax) cycleMax = cycleMs;
    summaryCycles++;
    if(reportConnection && isConnected >= 0 && connected >= 0 && isConnected != connected)
      printf("[ciclo %u] %s\n", cycle, isConnected ? "conexion restablecida" : "ERROR conexion perdida");
    if(isConnected >= 0) connected = isConnected;
    if(connected == 0) { put8(RLSYS_COMUNICACION, 0); put8(RLSYS_COMUNICACION_PARCIAL, 0); }
    if(blocksOk + blocksError > 0)
    {
      put8(RLSYS_COMUNICACION, (blocksError == 0) ? 1 : 0);
      put8(RLSYS_COMUNICACION_PARCIAL, (blocksOk > 0 && blocksError > 0) ? 1 : 0);
    }
    put8 (RLSYS_CONECTADO, (connected >= 0) ? connected : ((blocksOk > 0) ? 1 : 0));
    put16(RLSYS_BLOQUES_OK, (unsigned int) blocksOk);
    put16(RLSYS_BLOQUES_ERROR, (unsigned int) blocksError);
    put32(RLSYS_CICLO, cycle);
    put32(RLSYS_TIEMPO_CICLO, cycleMs);
    put32(RLSYS_TIEMPO_CICLO_MAX, cycleMax);
    put32(RLSYS_ERRORES_LECTURA, readErrors);
    put32(RLSYS_ERRORES_ESCRITURA, writeErrors);
    put32(RLSYS_ULTIMO_ERROR, (unsigned int) lastError);
    put32(RLSYS_ULTIMA_LECTURA_OK, lastOk);
    for(size_t i=0; i<plcState.size(); i++)
    {
      if(plcState[i] >= 0) put8(RLSYS_PLC + (int) i, plcState[i]);
    }
    if(level >= VERBOSE)
      printf("[ciclo %u] fin: bloques ok=%d error=%d, %u ms\n", cycle, blocksOk, blocksError, cycleMs);
    else if(level == NORMAL && t - summaryStart >= (long long) interval * 1000)
    {
      printf("[ciclo %u] %u ciclos en %d s, ultimo %u ms (max %u), bloques ok=%d error=%d, errores lectura=%u (%u nuevos) escritura=%u\n",
             cycle, summaryCycles, (int) ((t - summaryStart + 500) / 1000), cycleMs, cycleMax,
             blocksOk, blocksError, readErrors, summaryErrors, writeErrors);
      summaryStart = t; summaryCycles = 0; summaryErrors = 0;
    }
    fflush(stdout);
  }

  //! milisegundos (reloj monotono) para medir lo que tarda cada bloque
  static long long now()
  {
    return (long long) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  unsigned int cycle;   //!< numero de ciclo
  int level;            //!< QUIET, NORMAL o VERBOSE
  int reportConnection; //!< 1 = endCycle avisa cuando se pierde o vuelve la conexion

private:
  void put8(int off, unsigned int v)
  {
    if(base < 0 || shm == NULL) return;
    unsigned char b = (unsigned char) v;
    shm->write(base + off, &b, 1);
  }
  void put16(int off, unsigned int v)
  {
    if(base < 0 || shm == NULL) return;
    unsigned char b[2] = { (unsigned char) (v >> 8), (unsigned char) v };
    shm->write(base + off, b, 2);
  }
  void put32(int off, unsigned int v)
  {
    if(base < 0 || shm == NULL) return;
    unsigned char b[4] = { (unsigned char) (v >> 24), (unsigned char) (v >> 16), (unsigned char) (v >> 8), (unsigned char) v };
    shm->write(base + off, b, 4);
  }

  rlSharedMemory *shm;
  std::mutex mutex;
  int base, interval, numBlocks, connected;
  const char *word;
  int blocksOk, blocksError, lastError;
  unsigned int notifications;
  unsigned int readErrors, writeErrors, cycleMs, cycleMax, lastOk, started, summaryCycles, summaryErrors;
  long long cycleStart, summaryStart;
  std::vector<signed char> state;
  std::vector<int> plcs;
  std::vector<int> plcState;
};

#endif
