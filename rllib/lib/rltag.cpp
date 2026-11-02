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
#include <utility>
#include "rlsharedmemory.h"
#include <string.h>
#include <time.h>
#include <math.h>
#include <cmath>
#include <map>
#include <mutex>
#include <chrono>
#include <thread>
#include <string>
#ifdef __MINGW32__
#include <pthread.h>
#endif
#ifdef _WIN32
#include <process.h>
#define RL_GETPID() _getpid()
#else
#include <unistd.h>
#define RL_GETPID() getpid()
#endif

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
    case RLTAG_LINT:  return (double) (long long) u;
    case RLTAG_ULINT: return (double) u;
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
                 word.type == RLTAG_DINT || word.type == RLTAG_UDINT || word.type == RLTAG_LINT || word.type == RLTAG_ULINT;
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
    case RLTAG_LINT:  return r >= -9223372036854775808.0 && r < 9223372036854775808.0;
    case RLTAG_ULINT: return r >= 0 && r < 18446744073709551616.0;
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
    case RLTAG_LINT:
      if(!rlTagInRange(tag, raw)) return -1;   // fuera de rango la conversion es indefinida
      u = (unsigned long long) (long long) floor(raw + 0.5);
      break;
    case RLTAG_ULINT:
      if(!rlTagInRange(tag, raw)) return -1;
      u = (unsigned long long) floor(raw + 0.5);
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

//------------------------------------------------------------------------------------------------
// Nestor: 64 bits, texto, pedidos con respuesta, bloques y cambios

long long rlTagDecodeInt64(const rlTag &tag, const unsigned char *in)
{
  unsigned char b[8];
  if(tag.size < 1 || tag.size > 8) return 0;
  if(tag.type == RLTAG_REAL || tag.type == RLTAG_LREAL || tag.type == RLTAG_BOOL) return (long long) rlTagDecode(tag, in);
  memcpy(b, in, tag.size);
  reorder(tag, b);
  unsigned long long u = bigEndian(b, tag.size);
  switch(tag.type)
  {
    case RLTAG_BYTE:  return (unsigned char) u;
    case RLTAG_INT:   return (short) (unsigned short) u;
    case RLTAG_UINT:  return (unsigned short) u;
    case RLTAG_DINT:  return (int) (unsigned int) u;
    case RLTAG_UDINT: return (unsigned int) u;
    case RLTAG_LINT:
    case RLTAG_ULINT: return (long long) u;   // ulint: el que llama lo toma como unsigned long long
  }
  return 0;
}

int rlTagEncodeInt64(const rlTag &tag, long long raw, unsigned char *out)
{
  if(tag.type != RLTAG_LINT && tag.type != RLTAG_ULINT) return rlTagEncode(tag, (double) raw, out);
  if(tag.size != 8) return -1;
  unsigned long long u = (unsigned long long) raw;
  for(int i=7; i>=0; i--) { out[i] = (unsigned char) (u & 0x0ff); u >>= 8; }
  reorder(tag, out);
  return 8;
}

long long rlTagGetInt64(rlSharedMemory *shm, const rlTag &tag)
{
  unsigned char b[8];
  if(shm == NULL || tag.size < 1 || tag.size > 8) return 0;
  if(shm->read(tag.offset, b, tag.size) < 0) return 0;
  return rlTagDecodeInt64(tag, b);
}

int rlTagBytes(const rlTag &tag)
{
  return tag.size;
}

// texto: con BADC / DCBA los dos bytes de cada registro van invertidos (CDAB no cambia nada)
static void swapStringBytes(const rlTag &tag, unsigned char *b, int n)
{
  if(tag.order != RLTAG_BADC && tag.order != RLTAG_DCBA) return;
  for(int i=0; i+1<n; i+=2) { unsigned char t = b[i]; b[i] = b[i+1]; b[i+1] = t; }
}

int rlTagDecodeString(const rlTag &tag, const unsigned char *in, char *buf, int maxlen)
{
  if(buf == NULL || maxlen < 1) return -1;
  buf[0] = '\0';
  if(tag.type != RLTAG_STRING || tag.size < 1) return -1;
  std::string b((const char *) in, tag.size);
  swapStringBytes(tag, (unsigned char *) &b[0], tag.size);
  int n = 0;
  while(n < tag.size && b[n] != '\0' && n < maxlen - 1) { buf[n] = b[n]; n++; }
  buf[n] = '\0';
  return n;
}

int rlTagGetString(rlSharedMemory *shm, const rlTag &tag, char *buf, int maxlen)
{
  if(buf != NULL && maxlen > 0) buf[0] = '\0';
  if(shm == NULL || tag.type != RLTAG_STRING || tag.size < 1 || tag.size > 65536) return -1;
  std::string b(tag.size, '\0');
  if(shm->read(tag.offset, &b[0], tag.size) < 0) return -1;
  return rlTagDecodeString(tag, (const unsigned char *) b.data(), buf, maxlen);
}

int rlTagEncodeString(const rlTag &tag, const char *text, unsigned char *out, int outlen)
{
  if(tag.type != RLTAG_STRING || text == NULL || tag.size < 1 || outlen < tag.size) return -1;
  int n = (int) strlen(text);
  if(n > tag.size) return -1;
  memset(out, 0, tag.size);
  memcpy(out, text, n);
  swapStringBytes(tag, out, tag.size);
  return tag.size;
}

// final de la memoria compartida: [contador 4][respuestas 8 * RLTAG_ACK_SLOTS][hora 4][RLTG 4]
static int trailerBase(int shmsize) { return shmsize - RLTAG_TRAILER_SIZE; }
static int aliveOffset(int shmsize) { return trailerBase(shmsize) + 4 + 8 * RLTAG_ACK_SLOTS; }
static int magicOffset(int shmsize) { return aliveOffset(shmsize) + 4; }
static unsigned int unixNow() { return (unsigned int) time(NULL); }

static long long msSteady()
{
  return (long long) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

unsigned int rlTagNewRequestId()
{
  static std::mutex mutex;
  static unsigned int next = 0;
  std::lock_guard<std::mutex> lock(mutex);
  if(next == 0)
  {
    // distinto en cada proceso: reloj fino y el pid
    unsigned long long t = (unsigned long long) std::chrono::high_resolution_clock::now().time_since_epoch().count();
    next = (unsigned int) (t ^ (t >> 32)) ^ ((unsigned int) RL_GETPID() * 2654435761u);
  }
  next++;
  if(next == 0) next = 1;
  return next;
}

int rlTagHasTrailer(rlSharedMemory *shm, int shmsize)
{
  unsigned int magic = 0, alive = 0;
  int base = trailerBase(shmsize);
  if(shm == NULL || shm->status != rlSharedMemory::OK || base < 0) return 0;
  if(shm->read(magicOffset(shmsize), &magic, 4) < 0 || magic != RLTAG_TRAILER_MAGIC) return 0;
  if(shm->read(aliveOffset(shmsize), &alive, 4) < 0) return 0;
  // Nestor: el daemon renueva 'alive' en cada vuelta. Se mira que cambie, medido con un reloj monotonico:
  // antes se comparaba con la hora de pared de este proceso, y un salto del reloj de mas de 30 s (NTP, cambio
  // manual) hacia fallar los *Wait/refresh hasta la vuelta siguiente del daemon. La hora de pared se usa solo
  // la primera vez que se ve esta memoria.
  static std::mutex mutex;
  static std::map<std::pair<const void *, int>, std::pair<unsigned int, long long> > seen;   // valor, desde (ms)
  long long now = msSteady();
  std::lock_guard<std::mutex> lock(mutex);
  std::pair<const void *, int> key(shm, shmsize);
  std::map<std::pair<const void *, int>, std::pair<unsigned int, long long> >::iterator it = seen.find(key);
  if(it == seen.end())
  {
    if(seen.size() > 1000) seen.clear();
    long long age = (long long) unixNow() - (long long) alive;
    bool ok = age >= -RLTAG_ALIVE_S && age <= RLTAG_ALIVE_S;
    seen[key] = std::make_pair(alive, ok ? now : now - 1000LL * (RLTAG_ALIVE_S + 1));
    return ok ? 1 : 0;
  }
  if(it->second.first != alive) { it->second = std::make_pair(alive, now); return 1; }
  return (now - it->second.second <= 1000LL * RLTAG_ALIVE_S) ? 1 : 0;
}

void rlTagTrailerInit(rlSharedMemory *shm, int shmsize)
{
  int base = trailerBase(shmsize);
  if(shm == NULL || base < 0) return;
  std::string zero(4 + 8 * RLTAG_ACK_SLOTS, '\0');
  unsigned int seq = 0;
  shm->read(base, &seq, 4);
  seq &= ~1u;
  memcpy(&zero[0], &seq, 4);   // el contador sigue (un lector puede estar copiando)
  shm->write(base, zero.data(), (int) zero.size());
  rlTagTrailerAlive(shm, shmsize);
  unsigned int magic = RLTAG_TRAILER_MAGIC;
  shm->write(magicOffset(shmsize), &magic, 4);
}

void rlTagTrailerAlive(rlSharedMemory *shm, int shmsize)
{
  if(shm == NULL || trailerBase(shmsize) < 0) return;
  unsigned int now = unixNow();
  shm->write(aliveOffset(shmsize), &now, 4);
}

void rlTagAck(rlSharedMemory *shm, int shmsize, unsigned int id, int result)
{
  int base = trailerBase(shmsize);
  if(shm == NULL || base < 0 || id == 0) return;
  unsigned char slot[8];
  memcpy(slot, &result, 4);
  memcpy(slot + 4, &id, 4);
  shm->write(base + 4 + 8 * (id % RLTAG_ACK_SLOTS), slot, 8);   // una escritura: el id y el resultado juntos
}

int rlTagWaitAck(rlSharedMemory *shm, int shmsize, unsigned int id, int timeout_ms)
{
  int base = trailerBase(shmsize);
  if(shm == NULL || base < 0 || id == 0) return RLTAG_ERROR;
  long long t0 = msSteady();
  while(1)
  {
    unsigned char slot[8];
    unsigned int got = 0;
    int result = RLTAG_ERROR;
    if(shm->read(base + 4 + 8 * (id % RLTAG_ACK_SLOTS), slot, 8) < 0) return RLTAG_ERROR;
    memcpy(&result, slot, 4);
    memcpy(&got, slot + 4, 4);
    if(got == id) return (result == RLTAG_OK) ? RLTAG_OK : RLTAG_ERROR;
    if(msSteady() - t0 >= timeout_ms) return RLTAG_TIMEOUT;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
}

int rlTagWriteBlock(rlSharedMemory *shm, int shmsize, int offset, const void *data, int len)
{
  int base = trailerBase(shmsize);
  if(shm == NULL) return -1;
  if(base < 0) return shm->write(offset, data, len);
  unsigned int seq = 0;
  shm->read(base, &seq, 4);
  seq &= ~1u;            // par = nadie escribe
  unsigned int odd = seq + 1;
  shm->write(base, &odd, 4);   // impar: escribiendo
  int ret = shm->write(offset, data, len);
  seq += 2;
  shm->write(base, &seq, 4);
  return ret;
}

int rlTagReadCoherent(rlSharedMemory *shm, int shmsize, int offset, void *buf, int len)
{
  int base = trailerBase(shmsize);
  if(shm == NULL) return -1;
  if(base < 0) return shm->read(offset, buf, len);
  for(int i=0; i<2000; i++)
  {
    unsigned int s1 = 0, s2 = 0;
    shm->read(base, &s1, 4);
    if(s1 & 1) { std::this_thread::yield(); continue; }
    int ret = shm->read(offset, buf, len);
    shm->read(base, &s2, 4);
    if(s1 == s2) return ret;
  }
  return shm->read(offset, buf, len);   // el daemon no para de escribir: la ultima copia
}

int rlTagUnits(const rlTag &tag)
{
  if(tag.type == RLTAG_BLOCK) return tag.count;
  if(tag.area == RLTAG_COIL || tag.area == RLTAG_DISCRETE) return 1;
  return (tag.size + 1) / 2;
}

int rlTagGetBlock(rlSharedMemory *shm, int shmsize, const rlTag &tag, int *values, int max)
{
  int units = rlTagUnits(tag);
  if(shm == NULL || values == NULL || tag.offset < 0 || units < 1 || units > max) return -1;
  bool bits = (tag.area == RLTAG_COIL || tag.area == RLTAG_DISCRETE);
  int first = (bits && tag.bit > 0) ? tag.bit : 0;
  int nbytes = bits ? (first + units + 7) / 8 : units * 2;
  std::string b(nbytes, '\0');
  if(rlTagReadCoherent(shm, shmsize, tag.offset, &b[0], nbytes) < 0) return -1;
  const unsigned char *u = (const unsigned char *) b.data();
  for(int i=0; i<units; i++)
  {
    if(bits) values[i] = (u[(first + i) / 8] >> ((first + i) % 8)) & 1;
    else     values[i] = u[2*i] * 256 + u[2*i+1];
  }
  return units;
}

// cambios: por hilo (en el pvserver cada cliente tiene el suyo); se borran cuando termina el hilo
struct ChangedKey
{
  const void *shm;
  int offset, size, bit, status;
  bool operator<(const ChangedKey &o) const
  {
    if(shm != o.shm) return shm < o.shm;
    if(offset != o.offset) return offset < o.offset;
    if(size != o.size) return size < o.size;
    if(bit != o.bit) return bit < o.bit;
    return status < o.status;
  }
};
typedef std::map<ChangedKey, std::string> ChangedMap;

static ChangedMap &changedMap()
{
#ifdef __MINGW32__
  // como pvbThreadData: con MinGW, thread_local con destructor falla al terminar el hilo
  struct Key
  {
    pthread_key_t key;
    Key() { pthread_key_create(&key, destroy); }
    static void destroy(void *v) { delete (ChangedMap *) v; }
  };
  static Key k;
  ChangedMap *m = (ChangedMap *) pthread_getspecific(k.key);
  if(m == NULL) { m = new ChangedMap(); pthread_setspecific(k.key, m); }
  return *m;
#else
  static thread_local ChangedMap m;
  return m;
#endif
}

int rlTagChanged(rlSharedMemory *shm, const rlTag &tag)
{
  if(shm == NULL || tag.size < 1 || tag.size > 65536) return 0;
  std::string now(tag.size, '\0');
  if(shm->read(tag.offset, &now[0], tag.size) < 0) return 0;
  if(tag.type == RLTAG_BOOL && tag.bit >= 0) now = rlTagGetBool(shm, tag) ? "1" : "0";  // solo su bit
  unsigned char q = 0;
  if(tag.status >= 0) shm->read(tag.status, &q, 1);
  now += (char) q;
  ChangedKey k = { shm, tag.offset, tag.size, tag.bit, tag.status };
  ChangedMap &m = changedMap();
  ChangedMap::iterator it = m.find(k);
  if(it != m.end() && it->second == now) return 0;
  m[k] = now;
  return 1;
}
