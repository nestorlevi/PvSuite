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
  private:
    int          writeTag(const rlTag &tag, double raw);
};

#endif

