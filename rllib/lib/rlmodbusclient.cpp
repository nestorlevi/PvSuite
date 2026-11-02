/***************************************************************************
                 rlmodbusclient.cpp  -  description
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
#include "rlmodbusclient.h"
#include <cmath>
#include <string.h>
#include <vector>

rlModbusClient::rlModbusClient(const char *mbxname, const char *shmname, int shmsize)
               :rlMailbox(mbxname), rlSharedMemory(shmname, shmsize)
{
  shmSize = shmsize;   // Nestor: rlSharedMemory::size() suma el mutex
}

rlModbusClient::~rlModbusClient()
{
}

int rlModbusClient::write(int slave, int function, const unsigned char *data, int datalen)
{
  unsigned char buf[512];
  int i;

  if(datalen < 0)   return -1;
  if(datalen > 256) return -1;
  if(slave < 0)     return -1;
  if(slave > 255)   return -1;
  if(function < 0)  return -1;
  buf[0] = slave;
  buf[1] = function;
  for(i=0; i<datalen; i++)
  {
    buf[i+2] = data[i];
  }
  if(rlMailbox::writeNoWait((void *) buf,datalen+2) < 0) return -1; // Nestor: antes devolvia 0 aunque fallara
  return 0;
}

int rlModbusClient::writeSingleCoil(int slave, int adr, int value)
{
  unsigned char data[8];

  data[0] = adr/256; data[1] = adr & 0x0ff;
  data[2] = 0; data[3] = 0;
  if(value != 0) data[2] = 0x0ff;
  return write(slave, rlModbus::ForceSingleCoil, data, 4);
}

int rlModbusClient::writeMultipleCoils(int slave, int adr, const unsigned char *values, int num_coils)
{
  unsigned char data[512];
  int i,byte_count;

  if(num_coils <= 0) return -1;
  byte_count = ((num_coils-1)/8) + 1;
  data[0] = adr/256;       data[1] = adr & 0x0ff;
  data[2] = num_coils/256; data[3] = num_coils & 0x0ff;
  data[4] = byte_count;
  for(i=0; i<byte_count; i++)
  {
    if((5+i) > (int) sizeof(data)) return -1;
    data[5+i] = values[i];
  }
  return write(slave, rlModbus::ForceMultipleCoils, data, 5+byte_count);
}

int rlModbusClient::writePresetSingleRegister(int slave, int adr, int value)
{
  unsigned char data[8];

  data[0] = adr/256;   data[1] = adr & 0x0ff;
  data[2] = value/256; data[3] = value & 0x0ff;
  return write(slave, rlModbus::PresetSingleRegister, data, 4);
}

int rlModbusClient::writePresetMultipleRegisters(int slave, int adr, const int *values, int num_values)
{
  unsigned char data[512];
  int i;

  data[0] = adr/256;        data[1] = adr & 0x0ff;
  data[2] = num_values/256; data[3] = num_values & 0x0ff;
  data[4] = num_values*2; // byte count
  for(i=0; i<num_values; i++)
  {
    if((5+i*2) > (int) sizeof(data)) return -1;
    data[5+i*2] = values[i]/256; data[6+i*2] = values[i] & 0x0ff;
  }
  return write(slave, rlModbus::PresetMultipleRegs, data, 5+2*num_values);
}

int rlModbusClient::readBit(int offset, int number)
{
  int ret;
  unsigned char buf[128+1],b;

  if(number < 0)   return -1;
  if(number > 128) return -1;
  rlSharedMemory::read(offset,buf,(number/8)+1);
  b = buf[number/8];
  switch(number%8)
  {
    case 0:
      ret = b & BIT0;
      break;
    case 1:
      ret = b & BIT1;
      break;
    case 2:
      ret = b & BIT2;
      break;
    case 3:
      ret = b & BIT3;
      break;
    case 4:
      ret = b & BIT4;
      break;
    case 5:
      ret = b & BIT5;
      break;
    case 6:
      ret = b & BIT6;
      break;
    case 7:
      ret = b & BIT7;
      break;
    default:
      return -1;
  }
  if(ret > 0) return 1;
  return 0;
}

int rlModbusClient::readByte(int offset, int number)
{
  unsigned char buf[4],b;

  if(number < 0)   return -1;
  if(number > 128) return -1;
  rlSharedMemory::read(offset+number,buf,1);
  b = buf[0];
  return b;
}

int rlModbusClient::readShort(int offset, int number)
{
  unsigned char buf[4];
  int s;

  if(number < 0)  return -1;
  if(number > 64) return -1;
  rlSharedMemory::read(offset+(number*2),buf,2);
  s = buf[0]*256 + buf[1];
  return s;
}

// Nestor: tags por nombre (rltag.h)
int rlModbusClient::getBool(const rlTag &tag)
{
  return rlTagGetBool(this, tag);
}

int rlModbusClient::getInt(const rlTag &tag)
{
  return rlTagGetInt(this, tag);
}

unsigned int rlModbusClient::getUInt(const rlTag &tag)
{
  return rlTagGetUInt(this, tag);
}

double rlModbusClient::getReal(const rlTag &tag)
{
  return rlTagGetValue(this, tag);
}

int rlModbusClient::quality(const rlTag &tag)
{
  return rlTagQuality(this, tag);
}

int rlModbusClient::daemonAlive(const rlTag &ciclo, int seconds)
{
  return rlTagAlive(this, ciclo, seconds);
}

int rlModbusClient::setBool(const rlTag &tag, int value)
{
  return writeTag(tag, value ? 1 : 0);
}

int rlModbusClient::setInt(const rlTag &tag, int value)
{
  return writeTag(tag, value);
}

int rlModbusClient::setReal(const rlTag &tag, double value)
{
  return writeTag(tag, rlTagUnscale(tag, value));
}

int rlModbusClient::writeTag(const rlTag &tag, double raw, int timeout_ms)
{
  unsigned char b[8];
  if(tag.rw == 0) return -1;
  // Nestor: fuera del rango del tipo se desbordaba en silencio (40000 en un INT llegaba como -25536)
  if(!std::isfinite(raw) || (tag.type != RLTAG_BOOL && !rlTagInRange(tag, raw))) return -1;
  if(tag.type == RLTAG_STRING || tag.type == RLTAG_BLOCK) return -1;
  if(tag.area == RLTAG_COIL)
  {
    unsigned char data[4];
    data[0] = tag.address / 256; data[1] = tag.address & 0x0ff;
    data[2] = (raw != 0) ? 0x0ff : 0; data[3] = 0;
    return post(tag.slave, rlModbus::ForceSingleCoil, data, 4, timeout_ms);
  }
  if(tag.area != RLTAG_HOLDING) return -1;
  if(tag.type == RLTAG_BOOL && tag.bit >= 0)
  {
    // Nestor: un bit de un entero de 32 bits (rlTagBit) caeria en uno de dos registros segun el orden: no
    if(tag.size > 2 || tag.bit > 15) return -1;
    // Nestor: con BADC / DCBA la lectura invierte los bytes del registro (rlTagDecode): el bit n del valor
    // es el bit n^8 del registro
    int rbit = (tag.size == 2 && (tag.order == RLTAG_BADC || tag.order == RLTAG_DCBA)) ? (tag.bit ^ 8) : tag.bit;
    // Mask Write Register: registro = (registro AND and_mask) OR (or_mask AND NOT and_mask)
    int and_mask = (~(1 << rbit)) & 0x0ffff;
    int or_mask  = (raw != 0) ? (1 << rbit) : 0;
    unsigned char data[6];
    data[0] = tag.address / 256; data[1] = tag.address & 0x0ff;
    data[2] = and_mask / 256;    data[3] = and_mask & 0x0ff;
    data[4] = or_mask / 256;     data[5] = or_mask & 0x0ff;
    return post(tag.slave, rlModbus::MaskWrite4XRegisters, data, 6, timeout_ms);
  }
  if(rlTagEncode(tag, raw, b) < 0) return -1;
  return writeBytes(tag, b, tag.size, timeout_ms);
}

// Nestor: el mensaje del mailbox: [esclavo][funcion][datos] o, esperando la respuesta,
// [esclavo][RLTAG_MBX_REQUEST][id 4][funcion][datos] (ver rltag.h). timeout_ms < 0: sin esperar
int rlModbusClient::post(int slave, int function, const unsigned char *data, int len, int timeout_ms)
{
  if(slave < 0 || slave > 255 || function < 0 || len < 0 || len > 4096) return -1;
  std::string m;
  unsigned int id = 0;
  m += (char) slave;
  if(timeout_ms >= 0)
  {
    if(!rlTagHasTrailer(this, shmSize)) return RLTAG_ERROR;   // daemon viejo o detenido
    id = rlTagNewRequestId();
    m += (char) RLTAG_MBX_REQUEST;
    m += (char) (id >> 24); m += (char) (id >> 16); m += (char) (id >> 8); m += (char) id;
  }
  m += (char) function;
  m.append((const char *) data, len);
  if(timeout_ms < 0) return (rlMailbox::writeNoWait(m.data(), (int) m.size()) < 0) ? -1 : 0;   // Nestor: sin bloquearse con la cola llena
  // Nestor: esperando la respuesta no se queda bloqueado con la cola llena (Unix: daemon detenido)
  if(rlMailbox::writeNoWait(m.data(), (int) m.size()) < 0) return RLTAG_ERROR;
  return rlTagWaitAck(this, shmSize, id, timeout_ms);
}

// valor ya codificado (bytes en el orden del PLC): funcion 6 para un registro, 16 para varios
int rlModbusClient::writeBytes(const rlTag &tag, const unsigned char *b, int nbytes, int timeout_ms)
{
  if(tag.rw == 0 || tag.area != RLTAG_HOLDING || nbytes < 2 || (nbytes % 2) != 0) return -1;
  int n = nbytes / 2;
  if(tag.address < 0 || tag.address + n > 65536) return -1;   // no dar la vuelta al registro 0
  if(n == 1)
  {
    unsigned char data[4];
    data[0] = tag.address / 256; data[1] = tag.address & 0x0ff; data[2] = b[0]; data[3] = b[1];
    return post(tag.slave, rlModbus::PresetSingleRegister, data, 4, timeout_ms);
  }
  std::vector<unsigned char> data(5 + nbytes);
  data[0] = tag.address / 256; data[1] = tag.address & 0x0ff;
  data[2] = n / 256;           data[3] = n & 0x0ff;
  data[4] = (n <= 123) ? 2*n : 0;   // mas de 123 registros: el daemon los parte
  memcpy(&data[5], b, nbytes);
  return post(tag.slave, rlModbus::PresetMultipleRegs, data.data(), (int) data.size(), timeout_ms);
}

std::string rlModbusClient::getString(const rlTag &tag)
{
  if(tag.type != RLTAG_STRING || tag.size < 1) return std::string();
  std::vector<char> s(tag.size + 1);
  int n = rlTagGetString(this, tag, s.data(), tag.size + 1);
  return (n > 0) ? std::string(s.data(), n) : std::string();
}

int rlModbusClient::setString(const rlTag &tag, const char *text)
{
  return setStringWait(tag, text, -1);
}

int rlModbusClient::setStringWait(const rlTag &tag, const char *text, int timeout_ms)
{
  if(tag.type != RLTAG_STRING || tag.rw == 0 || (tag.size % 2) != 0) return -1;
  std::vector<unsigned char> b(tag.size);
  if(rlTagEncodeString(tag, text, b.data(), tag.size) < 0) return -1;
  return writeBytes(tag, b.data(), tag.size, timeout_ms);
}

long long rlModbusClient::getLInt(const rlTag &tag)
{
  return rlTagGetInt64(this, tag);
}

unsigned long long rlModbusClient::getULInt(const rlTag &tag)
{
  return (unsigned long long) rlTagGetInt64(this, tag);
}

int rlModbusClient::setLInt(const rlTag &tag, long long value)
{
  return setLIntWait(tag, value, -1);
}

int rlModbusClient::setULInt(const rlTag &tag, unsigned long long value)
{
  unsigned char b[8];
  if(tag.type == RLTAG_LINT && value > 9223372036854775807ULL) return -1;
  if(tag.type != RLTAG_ULINT) return setLIntWait(tag, (long long) value, -1);
  // los 64 bits tal cual (2^63 y mas no pasan por long long con signo)
  if(tag.rw == 0 || rlTagEncodeInt64(tag, (long long) value, b) != 8) return -1;
  return writeBytes(tag, b, 8, -1);
}

int rlModbusClient::setLIntWait(const rlTag &tag, long long value, int timeout_ms)
{
  unsigned char b[8];
  if(tag.type != RLTAG_LINT && tag.type != RLTAG_ULINT) return writeTag(tag, (double) value, timeout_ms);
  if(tag.type == RLTAG_ULINT && value < 0) return -1;   // negativo en un ulint: setULInt para 2^63 y mas
  if(tag.rw == 0 || rlTagEncodeInt64(tag, value, b) != 8) return -1;
  return writeBytes(tag, b, 8, timeout_ms);
}

// Nestor: mascaras sobre el valor del tag -> una funcion 22 por cada registro que cambia. La posicion de
// cada bit en los registros sale de codificar la mascara con el orden del tag.
int rlModbusClient::writeMask(const rlTag &tag, long long and_mask, long long or_mask, int timeout_ms)
{
  if(tag.rw == 0 || tag.area != RLTAG_HOLDING || tag.size < 2 || tag.size > 8 || (tag.size % 2) != 0) return -1;
  if(tag.type != RLTAG_INT && tag.type != RLTAG_UINT && tag.type != RLTAG_DINT && tag.type != RLTAG_UDINT &&
     tag.type != RLTAG_LINT && tag.type != RLTAG_ULINT) return -1;
  rlTag raw = tag;   // los mismos bytes como entero sin signo de su tamanio
  raw.type = (tag.size == 2) ? RLTAG_UINT : (tag.size == 4) ? RLTAG_UDINT : RLTAG_ULINT;
  unsigned long long bits = (tag.size == 8) ? ~0ULL : ((1ULL << (8 * tag.size)) - 1);
  unsigned char a[8], o[8];
  if(rlTagEncodeInt64(raw, (long long) ((unsigned long long) and_mask & bits), a) != tag.size) return -1;
  if(rlTagEncodeInt64(raw, (long long) ((unsigned long long) or_mask & bits), o) != tag.size) return -1;
  int ret = 0;
  for(int r=0; r<tag.size/2; r++)
  {
    int am = a[2*r]*256 + a[2*r+1], om = o[2*r]*256 + o[2*r+1];
    if(am == 0x0ffff) continue;   // este registro no cambia
    unsigned char data[6];
    int adr = tag.address + r;
    if(adr < 0 || adr > 65535) return -1;
    data[0] = adr / 256; data[1] = adr & 0x0ff;
    data[2] = am / 256;  data[3] = am & 0x0ff;
    data[4] = om / 256;  data[5] = om & 0x0ff;
    ret = post(tag.slave, rlModbus::MaskWrite4XRegisters, data, 6, timeout_ms);
    if(ret != 0) return ret;
  }
  return ret;
}

int rlModbusClient::setMask(const rlTag &tag, long long and_mask, long long or_mask)
{
  return writeMask(tag, and_mask, or_mask, -1);
}

int rlModbusClient::setMaskWait(const rlTag &tag, long long and_mask, long long or_mask, int timeout_ms)
{
  return writeMask(tag, and_mask, or_mask, timeout_ms);
}

int rlModbusClient::setBit(const rlTag &tag, int bit, int value)
{
  return setBitWait(tag, bit, value, -1);
}

int rlModbusClient::setBitWait(const rlTag &tag, int bit, int value, int timeout_ms)
{
  if(bit < 0 || bit >= 8 * tag.size || bit > 63) return -1;
  unsigned long long m = 1ULL << bit;
  return writeMask(tag, (long long) ~m, value ? (long long) m : 0, timeout_ms);
}

// Nestor: registros (funcion 16) o coils (funcion 15) desde la direccion del tag, en un pedido. Un mensaje
// del mailbox lleva hasta 960 registros o 7680 coils (macOS: msgmax = 2048 bytes); mas que eso va en varios
// (el daemon parte cada uno en telegramas de 123 registros / 1968 coils)
int rlModbusClient::writeBlock(const rlTag &tag, const int *values, int count, int timeout_ms)
{
  if(tag.rw == 0 || values == NULL || count < 1) return -1;
  if(tag.type == RLTAG_BLOCK && count > tag.count) return -1;
  if(tag.address < 0 || tag.address + count > 65536) return -1;   // no dar la vuelta al registro 0
  if(tag.area == RLTAG_HOLDING)
  {
    if(count > 2000) return -1;
    for(int i=0; i<count; i++) if(values[i] < -32768 || values[i] > 65535) return -1;
    int ret = -1;
    for(int done=0; done<count; done += 960)
    {
      int n = (count - done < 960) ? count - done : 960, adr = tag.address + done;
      std::vector<unsigned char> data(5 + 2 * n);
      data[0] = adr / 256; data[1] = adr & 0x0ff;
      data[2] = n / 256;   data[3] = n & 0x0ff;
      data[4] = (n <= 123) ? 2*n : 0;   // mas de 123 registros: el daemon los parte
      for(int i=0; i<n; i++) { data[5+2*i] = (values[done+i] >> 8) & 0x0ff; data[6+2*i] = values[done+i] & 0x0ff; }
      ret = post(tag.slave, rlModbus::PresetMultipleRegs, data.data(), (int) data.size(), timeout_ms);
      if(ret != 0) return ret;
    }
    return ret;
  }
  if(tag.area == RLTAG_COIL)
  {
    if(count > 16000) return -1;
    int ret = -1;
    for(int done=0; done<count; done += 7680)
    {
      int n = (count - done < 7680) ? count - done : 7680, adr = tag.address + done, bytes = (n + 7) / 8;
      std::vector<unsigned char> data(5 + bytes, 0);
      data[0] = adr / 256; data[1] = adr & 0x0ff;
      data[2] = n / 256;   data[3] = n & 0x0ff;
      data[4] = (bytes <= 246) ? bytes : 0;     // mas de 1968 coils: el daemon los parte
      for(int i=0; i<n; i++) if(values[done+i]) data[5 + i/8] |= (unsigned char) (1 << (i % 8));
      ret = post(tag.slave, rlModbus::ForceMultipleCoils, data.data(), (int) data.size(), timeout_ms);
      if(ret != 0) return ret;
    }
    return ret;
  }
  return -1;
}

int rlModbusClient::setBlock(const rlTag &tag, const int *values, int count)
{
  return writeBlock(tag, values, count, -1);
}

int rlModbusClient::setBlockWait(const rlTag &tag, const int *values, int count, int timeout_ms)
{
  return writeBlock(tag, values, count, timeout_ms);
}

int rlModbusClient::clear(const rlTag &tag)
{
  int n = rlTagUnits(tag);
  if(n < 1) return -1;
  std::vector<int> zero(n, 0);
  return writeBlock(tag, zero.data(), n, -1);
}

int rlModbusClient::getBlock(const rlTag &tag, int *values, int max)
{
  return rlTagGetBlock(this, shmSize, tag, values, max);
}

// bytes de la memoria compartida que ocupa un tag o un elemento
static int shmBytes(const rlTag &tag)
{
  if(tag.type == RLTAG_BLOCK && (tag.area == RLTAG_COIL || tag.area == RLTAG_DISCRETE))
    return ((tag.bit > 0 ? tag.bit : 0) + tag.count + 7) / 8;
  return (tag.size > 0) ? tag.size : 1;
}

int rlModbusClient::refresh(const rlTag &tag, int timeout_ms)
{
  if(tag.offset < 0 || timeout_ms < 0) return RLTAG_ERROR;
  if(!rlTagHasTrailer(this, shmSize)) return RLTAG_ERROR;
  unsigned int id = rlTagNewRequestId();
  unsigned int off = (unsigned int) tag.offset, bytes = (unsigned int) shmBytes(tag);
  unsigned char m[14];
  m[0] = (unsigned char) tag.slave; m[1] = RLTAG_MBX_REFRESH;
  m[2] = id >> 24;    m[3] = id >> 16;    m[4] = id >> 8;     m[5] = id;
  m[6] = off >> 24;   m[7] = off >> 16;   m[8] = off >> 8;    m[9] = off;
  m[10] = bytes >> 24; m[11] = bytes >> 16; m[12] = bytes >> 8; m[13] = bytes;
  if(rlMailbox::writeNoWait(m, 14) < 0) return RLTAG_ERROR;
  return rlTagWaitAck(this, shmSize, id, timeout_ms);
}

int rlModbusClient::setBoolWait(const rlTag &tag, int value, int timeout_ms)
{
  if(timeout_ms < 0) return RLTAG_ERROR;
  return writeTag(tag, value ? 1 : 0, timeout_ms);
}

int rlModbusClient::setIntWait(const rlTag &tag, int value, int timeout_ms)
{
  if(timeout_ms < 0) return RLTAG_ERROR;
  return writeTag(tag, value, timeout_ms);
}

int rlModbusClient::setRealWait(const rlTag &tag, double value, int timeout_ms)
{
  if(timeout_ms < 0) return RLTAG_ERROR;
  return writeTag(tag, rlTagUnscale(tag, value), timeout_ms);
}

int rlModbusClient::changed(const rlTag &tag)
{
  if(tag.type == RLTAG_BLOCK)
  {
    rlTag t = tag;   // el elemento entero: sus bytes en la memoria
    t.size = shmBytes(tag);
    t.bit = -1;
    return rlTagChanged(this, t);
  }
  return rlTagChanged(this, tag);
}
