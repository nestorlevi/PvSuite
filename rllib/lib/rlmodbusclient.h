/***************************************************************************
                    rlmodbusclient.h  -  description
                             -------------------
    begin                : Wed Jan 07 2004
    copyright            : (C) 2004 by R. Lehrig
    email                : lehrig@t-online.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU LESSER GENERAL PUBLIC LICENSE as        *
 *   published by the Free Software Foundation                             *
 *                                                                         *
 ***************************************************************************/
#ifndef _RL_MODBUS_CLIENT_H_
#define _RL_MODBUS_CLIENT_H_

#include "rldefine.h"
#include "rlmodbus.h"
#include "rltag.h"
#include "rlmailbox.h"
#include "rlsharedmemory.h"
#include <string>

/*! <pre>
This class is for data aqusition over a modbus daemon as created by pvdevelop.
It communicates over a shared memory and a mailbox according to the pvbrowser principle.
</pre> */
class rlModbusClient : public rlMailbox, public rlSharedMemory
{
  public:
    rlModbusClient(const char *mbxname, const char *shmname, int shmsize);
    virtual ~rlModbusClient();
    int write(int slave, int function, const unsigned char *data, int len);
    int writeSingleCoil(int slave, int adr, int value);
    int writeMultipleCoils(int slave, int adr, const unsigned char *values, int num_coils);
    int writePresetSingleRegister(int slave, int adr, int value);
    int writePresetMultipleRegisters(int slave, int adr, const int *values, int num_values);
    int readBit(int offset, int number);
    int readByte(int offset, int number);
    int readShort(int offset, int number);

    // Tags por nombre (los "tag" del .mkmodbus; ver rltag.h): PLC1.getBool(PLC1_Motor)
    int          getBool(const rlTag &tag);
    int          getInt(const rlTag &tag);
    unsigned int getUInt(const rlTag &tag);
    double       getReal(const rlTag &tag);              // con la escala del tag
    int          quality(const rlTag &tag);              // 1 = leido bien, 0 = error
    int          daemonAlive(const rlTag &ciclo, int seconds = 5); // el daemon esta vivo: TARGET_SYS.Ciclo avanzo hace menos de "seconds" s
    int          setBool(const rlTag &tag, int value);   // bit de un registro: funcion 22 (el daemon
    int          setInt(const rlTag &tag, int value);    // la emula leyendo y escribiendo si bit_write=rmw)
    int          setReal(const rlTag &tag, double value);// con la escala del tag

    // Nestor: texto, 64 bits, bits y elementos enteros. Los set* devuelven 0 si el pedido se mando al daemon
    // (-1: no se puede escribir, el valor no entra, el daemon no esta). Ver rltag.h.
    std::string  getString(const rlTag &tag);                      // type=string len=N
    int          setString(const rlTag &tag, const char *text);    // un pedido (funcion 16); -1 si no entra
    long long    getLInt(const rlTag &tag);                        // lint (tambien los demas enteros)
    unsigned long long getULInt(const rlTag &tag);                 // ulint
    int          setLInt(const rlTag &tag, long long value);
    int          setULInt(const rlTag &tag, unsigned long long value);
    // bits de un entero sin pisar los demas (funcion 22; si el PLC no la tiene, el daemon lee, cambia y
    // escribe sin otro pedido en el medio): valor = (valor AND and_mask) OR (or_mask AND NOT and_mask)
    int          setBit(const rlTag &tag, int bit, int value);
    int          setMask(const rlTag &tag, long long and_mask, long long or_mask);
    // elemento entero en UN pedido (funcion 16; 15 para coils; mas de 123 registros: el daemon lo parte):
    //   PLC1.setBlock(PLC1_Tanques[n], datos, 16)  datos = registros (0..65535 o -32768..32767) o coils (0/1)
    //   PLC1.setBlock(PLC1_Temp[2], datos, 4)      tramo de un arreglo: 4 registros desde Temp[2]
    //   PLC1.clear(PLC1_Bombas[n])                 todo el elemento en cero
    //   (el elemento entero solo si todos sus registros son de miembros escribibles, sin huecos: si no, -1)
    //   PLC1.getBlock(PLC1_Tanques[n], buf, 16)    copia coherente (el daemon no escribe en el medio);
    //                                              devuelve la cantidad de valores o -1
    int          setBlock(const rlTag &tag, const int *values, int count);
    int          clear(const rlTag &tag);
    int          getBlock(const rlTag &tag, int *values, int max);
    template<class S> int setBlock(const S &elem, const int *values, int count) { return setBlock(elem._all, values, count); }
    template<class S> int clear(const S &elem)                                 { return clear(elem._all); }
    template<class S> int getBlock(const S &elem, int *values, int max)        { return getBlock(elem._all, values, max); }
    using rlMailbox::clear;

    // Nestor: leer ahora (los bloques que contienen el tag) y esperar el dato: RLTAG_OK, RLTAG_ERROR o RLTAG_TIMEOUT
    int          refresh(const rlTag &tag, int timeout_ms = 2000);
    template<class S> int refresh(const S &elem, int timeout_ms = 2000) { return refresh(elem._all, timeout_ms); }
    // Nestor: escritura con confirmacion: RLTAG_OK (el PLC la acepto), RLTAG_ERROR (no la acepto, no
    // respondio o el valor no sirve) o RLTAG_TIMEOUT (el daemon no contesto en timeout_ms)
    int          setBoolWait(const rlTag &tag, int value, int timeout_ms = 2000);
    int          setIntWait(const rlTag &tag, int value, int timeout_ms = 2000);
    int          setRealWait(const rlTag &tag, double value, int timeout_ms = 2000);
    int          setLIntWait(const rlTag &tag, long long value, int timeout_ms = 2000);
    int          setStringWait(const rlTag &tag, const char *text, int timeout_ms = 2000);
    int          setBitWait(const rlTag &tag, int bit, int value, int timeout_ms = 2000);
    int          setMaskWait(const rlTag &tag, long long and_mask, long long or_mask, int timeout_ms = 2000);
    int          setBlockWait(const rlTag &tag, const int *values, int count, int timeout_ms = 2000);
    template<class S> int setBlockWait(const S &elem, const int *values, int count, int timeout_ms = 2000) { return setBlockWait(elem._all, values, count, timeout_ms); }

    // Nestor: 1 si el tag cambio (valor o calidad) desde la ultima vez que este cliente (el hilo) lo miro;
    // la primera vez da 1
    int          changed(const rlTag &tag);
    template<class S> int changed(const S &elem) { return changed(elem._all); }
  private:
    int          writeTag(const rlTag &tag, double raw, int timeout_ms = -1);
    int          writeBytes(const rlTag &tag, const unsigned char *b, int nbytes, int timeout_ms);
    int          writeBlock(const rlTag &tag, const int *values, int count, int timeout_ms);
    int          writeMask(const rlTag &tag, long long and_mask, long long or_mask, int timeout_ms);
    int          post(int slave, int function, const unsigned char *data, int len, int timeout_ms);
    int          shmSize;     // el tamanio pedido (el final tiene las respuestas, rltag.h)
};

#endif

