/***************************************************************************
                          rltag.cpp  -  description
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
#include "rltag.h"
#include "rlsharedmemory.h"
#include <string.h>
#include <math.h>
#include <cmath>
#include <map>
#include <mutex>
#include <chrono>

// convierte entre el orden del tag y "big endian" (ABCD); la operacion es su propia inversa
static void reorder(const rlTag &tag, unsigned char *b)
{
  unsigned char t;
  int i;
  if(tag.size < 2) return;
  if(tag.order == RLTAG_BADC || tag.order == RLTAG_DCBA)
  {
    for(i=0; i+1<tag.size; i+=2) { t = b[i]; b[i] = b[i+1]; b[i+1] = t; }
  }
  if((tag.order == RLTAG_CDAB || tag.order == RLTAG_DCBA) && tag.size >= 4)
  {
    int words = tag.size / 2;
    for(i=0; i<words/2; i++)
    {
      int j = words - 1 - i;
      t = b[2*i];   b[2*i]   = b[2*j];   b[2*j]   = t;
      t = b[2*i+1]; b[2*i+1] = b[2*j+1]; b[2*j+1] = t;
    }
  }
}

static unsigned long long bigEndian(const unsigned char *b, int size)
{
  unsigned long long v = 0;
  for(int i=0; i<size; i++) v = (v << 8) | b[i];
  return v;
}

double rlTagGetRaw(rlSharedMemory *shm, const rlTag &tag)
{
  unsigned char b[8];
  if(shm == NULL || tag.size < 1 || tag.size > 8) return 0;
  if(shm->read(tag.offset, b, tag.size) < 0) return 0;
  return rlTagDecode(tag, b);
}

double rlTagDecode(const rlTag &tag, const unsigned char *in)
{
  unsigned char b[8];
  if(tag.size < 1 || tag.size > 8) return 0;
  memcpy(b, in, tag.size);
  reorder(tag, b);
  unsigned long long u = bigEndian(b, tag.size);
  switch(tag.type)
  {
    case RLTAG_BOOL:
      if(tag.bit >= 0) return (double) ((u >> tag.bit) & 1);
      return (u != 0) ? 1 : 0;
    case RLTAG_BYTE:  return (double) (unsigned char) u;
    case RLTAG_INT:   return (double) (short) (unsigned short) u;
    case RLTAG_UINT:  return (double) (unsigned short) u;
    case RLTAG_DINT:  return (double) (int) (unsigned int) u;
    case RLTAG_UDINT: return (double) (unsigned int) u;
    case RLTAG_REAL:
    {
      unsigned int ui = (unsigned int) u;
      float f;
      memcpy(&f, &ui, 4);
      return f;
    }
    case RLTAG_LREAL:
    {
      double d;
      memcpy(&d, &u, 8);
      return d;
    }
    default:
      return 0;
  }
}

int rlTagGetBool(rlSharedMemory *shm, const rlTag &tag)
{
  return (rlTagGetRaw(shm, tag) != 0) ? 1 : 0;
}

int rlTagGetInt(rlSharedMemory *shm, const rlTag &tag)
{
  return (int) (long long) rlTagGetRaw(shm, tag);
}

unsigned int rlTagGetUInt(rlSharedMemory *shm, const rlTag &tag)
{
  return (unsigned int) (long long) rlTagGetRaw(shm, tag);
}

double rlTagGetValue(rlSharedMemory *shm, const rlTag &tag)
{
  double raw = rlTagGetRaw(shm, tag);
  if(tag.type == RLTAG_BOOL) return raw;
  return raw * tag.scale + tag.scale_offset;
}

int rlTagQuality(rlSharedMemory *shm, const rlTag &tag)
{
  unsigned char q = 0;
  if(shm == NULL || tag.status < 0) return -1;
  if(shm->read(tag.status, &q, 1) < 0) return -1;
  return (q == 1) ? 1 : 0;
}

double rlTagUnscale(const rlTag &tag, double value)
{
  if(tag.type == RLTAG_BOOL || tag.scale == 0) return value;
  return (value - tag.scale_offset) / tag.scale;
}

int rlTagBit(const rlTag &word, int n, rlTag *bit)
{
  bool integer = word.type == RLTAG_BYTE || word.type == RLTAG_INT || word.type == RLTAG_UINT ||
                 word.type == RLTAG_DINT || word.type == RLTAG_UDINT;
  if(!integer || n < 0 || n >= word.size * 8) return -1;
  *bit = rlTagBitOf(word, n);
  return 0;
}

int rlTagInRange(const rlTag &tag, double raw)
{
  if(!std::isfinite(raw)) return 0;
  double r = floor(raw + 0.5);   // como lo redondea rlTagEncode
  switch(tag.type)
  {
    case RLTAG_BOOL:  return 1;
    case RLTAG_BYTE:  return r >= 0 && r <= 255;
    case RLTAG_INT:   return r >= -32768 && r <= 32767;
    case RLTAG_UINT:  return r >= 0 && r <= 65535;
    case RLTAG_DINT:  return r >= -2147483648.0 && r <= 2147483647.0;
    case RLTAG_UDINT: return r >= 0 && r <= 4294967295.0;
    case RLTAG_REAL:  return fabs(raw) <= 3.402823466e38;
    case RLTAG_LREAL: return 1;
  }
  return 0;
}

int rlTagEncode(const rlTag &tag, double raw, unsigned char *out)
{
  unsigned long long u = 0;
  if(tag.size < 1 || tag.size > 8) return -1;
  switch(tag.type)
  {
    case RLTAG_BOOL:  u = (raw != 0) ? 1 : 0; break;
    case RLTAG_BYTE:  u = (unsigned char)  (long long) floor(raw + 0.5); break;
    case RLTAG_INT:   u = (unsigned short) (short) (long long) floor(raw + 0.5); break;
    case RLTAG_UINT:  u = (unsigned short) (long long) floor(raw + 0.5); break;
    case RLTAG_DINT:  u = (unsigned int)   (int) (long long) floor(raw + 0.5); break;
    case RLTAG_UDINT: u = (unsigned int)   (long long) floor(raw + 0.5); break;
    case RLTAG_REAL:
    {
      float f = (float) raw;
      unsigned int ui;
      memcpy(&ui, &f, 4);
      u = ui;
      break;
    }
    case RLTAG_LREAL:
      memcpy(&u, &raw, 8);
      break;
    default:
      return -1;
  }
  for(int i=tag.size-1; i>=0; i--) { out[i] = (unsigned char) (u & 0x0ff); u >>= 8; }
  reorder(tag, out);
  return tag.size;
}

const rlTag *rlTagFind(const rlTag * const *table, int count, const char *name)
{
  if(table == NULL || name == NULL) return NULL;
  for(int i=0; i<count; i++)
  {
    if(table[i] != NULL && table[i]->name != NULL && strcmp(table[i]->name, name) == 0) return table[i];
  }
  return NULL;
}

int rlTagPut(rlSharedMemory *shm, const rlTag &tag, double raw)
{
  unsigned char b[8];
  if(shm == NULL) return -1;
  int n = rlTagEncode(tag, raw, b);
  if(n <= 0) return -1;
  return shm->write(tag.offset, b, n);
}

// ultimo valor y cuando cambio, por memoria compartida y offset (los pvservers usan varios hilos)
struct AliveKey
{
  const void *shm;
  int offset;
  bool operator<(const AliveKey &o) const { return (shm != o.shm) ? (shm < o.shm) : (offset < o.offset); }
};
struct AliveValue { double value; long long changed; long long first; bool sawChange; };

int rlTagAliveState(rlSharedMemory *shm, const rlTag &tag, int seconds)
{
  static std::mutex mutex;
  static std::map<AliveKey, AliveValue> last;
  if(shm == NULL || shm->status != rlSharedMemory::OK) return 0;
  double v = rlTagGetRaw(shm, tag);
  long long now = (long long) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
  std::lock_guard<std::mutex> lock(mutex);
  AliveKey k = { shm, tag.offset };
  std::map<AliveKey, AliveValue>::iterator it = last.find(k);
  if(it == last.end())
  {
    // Nestor: la primera lectura no dice nada (la memoria conserva el ultimo valor de un daemon cerrado)
    AliveValue a = { v, now, now, false };
    last[k] = a;
    return -1;
  }
  AliveValue &a = it->second;
  if(a.value != v) { a.value = v; a.changed = now; a.sawChange = true; return 1; }
  if(!a.sawChange) return (now - a.first) >= (long long) seconds * 1000 ? 0 : -1;
  return (now - a.changed) < (long long) seconds * 1000 ? 1 : 0;
}

int rlTagAlive(rlSharedMemory *shm, const rlTag &tag, int seconds)
{
  return (rlTagAliveState(shm, tag, seconds) == 1) ? 1 : 0;
}
