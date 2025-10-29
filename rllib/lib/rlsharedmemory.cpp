/***************************************************************************
                          rlsharedmemory.cpp  -  description
                             -------------------
    begin                : Tue Jan 02 2001
    copyright            : (C) 2001 by R. Lehrig
    email                : lehrig@t-online.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU LESSER GENERAL PUBLIC LICENSE as        *
 *   published by the Free Software Foundation                             *
 *                                                                         *
 ***************************************************************************/
#include "rlsharedmemory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef RLWIN32
#include <unistd.h>
#endif

#ifdef RLUNIX
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/file.h>
#endif

#ifdef __VMS
#include <starlet.h>
#include <lib$routines.h>
#include <descrip.h>
#include <ssdef.h>
#include <syidef.h>
#include <secdef.h>
#include <rms.h>
#include <errno.h>
#include <psldef.h>
#include <dvidef.h>
typedef struct
{
  long start;
  long end;
}
ADD;
#endif

#ifdef RLWIN32
#include <windows.h>
//#include <sddl.h>
#endif

#ifndef RLWIN32
#ifndef RLUNIX
static void myinit(pthread_mutex_t *mutex)
{
  int *cnt = (int *) mutex;
  *cnt = 0;
}

static void mylock(pthread_mutex_t *mutex, int increment)
{
  volatile int *cnt = (int *) mutex;
  while(1)
  {
retry:
    if(*cnt == 0)
    { // try to lock the counter
      (*cnt) += increment;
      if(*cnt > 1)
      {
        (*cnt) -= increment;
        goto retry; // another process also wanted to lock the counter
      }
      return;       // now we can do it
    }
    rlwthread_sleep(1);
  }
}

static void myunlock(pthread_mutex_t *mutex)
{
  int *cnt = (int *) mutex;
  if(*cnt > 0) (*cnt)--;
}
#endif
#endif

rlSharedMemory::rlSharedMemory(const char *shmname, unsigned long Size, int rwmode)
{
#ifdef RLUNIX
    // Código original sin cambios para Unix
    struct shmid_ds buf;
    status  = OK;
    name = new char[strlen(shmname)+1];
    strcpy(name,shmname);
    _size    = Size + sizeof(*mutex);
    // ... resto del código original para Unix ...
#endif

#ifdef __VMS
    // Código original sin cambios para VMS
    int file_existed = 0;
    long ret,fd,page_size,pagelets,pagelet_size,file_block_size,flags,item,ident[2];
    // ... resto del código original para VMS ...
#endif

#ifdef RLWIN32
    HANDLE hShmem;
    status  = OK;

    // Convertir nombre a Unicode
    wchar_t *wname = new wchar_t[strlen(shmname)+1];
    MultiByteToWideChar(CP_ACP, 0, shmname, -1, wname, strlen(shmname)+1);

    name = new char[strlen(shmname)+1];
    strcpy(name, shmname);
    _size = Size + sizeof(*mutex);

    // Crear archivo con nombre Unicode
    hSharedFile = CreateFileW(wname,
                              GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);

    if(hSharedFile == INVALID_HANDLE_VALUE)
    {
        hSharedFile = CreateFileW(wname,
                                  GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL,
                                  CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL,
                                  NULL);
    }
    delete[] wname;

    if(hSharedFile == INVALID_HANDLE_VALUE) { status=ERROR_FILE; return; }

    // Crear mapeo de memoria
    hShmem = CreateFileMappingW(
        hSharedFile,
        NULL,                // no security attributes
        PAGE_READWRITE,      // read/write access
        0,                   // size: high 32-bits
        _size,               // size: low 32-bits
        NULL);               // name of map object

    if(hShmem == NULL) { status=ERROR_FILE; return; }

    base_adr = (char *) MapViewOfFile(
        hShmem,              // object to map view of
        FILE_MAP_WRITE,      // read/write access
        0,                   // high offset: map from
        0,                   // low offset: beginning
        0);                  // default: map entire file

    if(base_adr == NULL) { status=ERROR_FILE; return; }

    id     = hShmem;
    shmkey = hSharedFile;
    mutex     = (pthread_mutex_t *) base_adr;
    user_adr  = base_adr + sizeof(*mutex);
    memset(&overlapped, 0, sizeof(overlapped));
    UnlockFileEx(hSharedFile,0,_size,0,&overlapped);
#endif

    if(rwmode == 0) return; // no warning of unused parameter
}

rlSharedMemory::~rlSharedMemory()
{
  delete [] name;
  //printf("We are within the destructor\n");
#ifdef RLWIN32
  if(status != OK) return;
  CloseHandle((HANDLE) id);
  CloseHandle((HANDLE) shmkey);
#elif defined(RLUNIX)
  if(fdlock >= 0) close(fdlock);
#endif
}

int rlSharedMemory::deleteSharedMemory()
{
#ifdef RLUNIX
  struct shmid_ds buf;
  if(status != OK) return -1;
  //rlwthread_mutex_destroy(mutex);
  flock(fdlock,LOCK_UN);
  shmctl(id, IPC_RMID, &buf);
  _size = 0;
  return 0;
#endif

#ifdef __VMS
  int ret;
  ADD add_in,add_ret;
  struct dsc$descriptor_s section_name;

  if(status != OK) return -1;
  rlwthread_mutex_destroy(mutex);
  // Fill descriptor for section name
  section_name.dsc$w_length  = strlen(name);
  section_name.dsc$a_pointer = name;
  section_name.dsc$b_dtype   = DSC$K_DTYPE_T;
  section_name.dsc$b_class   = DSC$K_CLASS_S;
  // Delete the section
  ret = sys$dgblsc(0,&section_name,0);
  if(ret != SS$_NORMAL) return -1;
  // Fill the input address
  add_in.start = (long) base_adr;
  add_in.end   = (long) base_adr + _size;
  // Free the memory
  ret = sys$deltva(&add_in,&add_ret,0);
  if(ret != SS$_NORMAL) return -1;
  // Test the section addresses
  if(add_in.start != add_ret.start || add_in.end != add_ret.end) return -1;
  return 0;
#endif

#ifdef RLWIN32
  if(status != OK) return -1;
  //rlwthread_mutex_destroy(mutex);
  UnmapViewOfFile(base_adr);
  CloseHandle((HANDLE) id);
  CloseHandle((HANDLE) shmkey);
  UnlockFile(hSharedFile,0,0,_size,0); // Changed by FMakkinga 18-03-2013
  CloseHandle(hSharedFile);           // Changed by FMakkinga 18-03-2013
  status = ~OK;
  return 0;
#endif
}

int rlSharedMemory::write(unsigned long offset, const void *buf, int len)
{
  void *ptr;
  if(status != OK)       return -1;
  if(len <= 0)           return -1;
  if(offset+len > _size) return -1;
  ptr = user_adr + offset;
#ifdef RLWIN32
  LockFileEx(hSharedFile,LOCKFILE_EXCLUSIVE_LOCK,0,_size,0,&overlapped); // Changed by FMakkinga 18-03-2013
#elif defined(RLUNIX)
  flock(fdlock,LOCK_EX);
#else
  mylock(mutex,1);
#endif
  memcpy(ptr,buf,len);
#ifdef RLWIN32
  UnlockFileEx(hSharedFile,0,_size,0,&overlapped);                       // Changed by FMakkinga 18-03-2013
#elif defined(RLUNIX)
  flock(fdlock,LOCK_UN);
#else
  myunlock(mutex);
#endif
  return len;
}

int rlSharedMemory::read(unsigned long offset, void *buf, int len)
{
  void *ptr;
  if(status != OK)       return -1;
  if(len <= 0)           return -1;
  if(offset+len > _size) return -1;
  ptr = user_adr + offset;
#ifdef RLWIN32
  LockFileEx(hSharedFile,LOCKFILE_EXCLUSIVE_LOCK,0,_size,0,&overlapped); // Changed by FMakkinga 18-03-2013
#elif defined(RLUNIX)
  flock(fdlock,LOCK_EX);
#else
  mylock(mutex,1);
#endif
  memcpy(buf,ptr,len);
#ifdef RLWIN32
  UnlockFileEx(hSharedFile,0,_size,0,&overlapped);                       // Changed by FMakkinga 18-03-2013
#elif defined(RLUNIX)
  flock(fdlock,LOCK_UN);
#else
  myunlock(mutex);
#endif
  return len;
}

int rlSharedMemory::readInt(unsigned long offset, int index)
{
  int val;
  if(index < 0) return -1;
  read(offset+index*sizeof(val),&val,sizeof(val));
  return val;
}

int rlSharedMemory::readShort(unsigned long offset, int index)
{
  short int val;
  if(index < 0) return -1;
  read(offset+index*sizeof(val),&val,sizeof(val));
  return val;
}

int rlSharedMemory::readByte(unsigned long offset, int index)
{
  char val;
  if(index < 0) return -1;
  read(offset+index*sizeof(val),&val,sizeof(val));
  return val;
}

float rlSharedMemory::readFloat(unsigned long offset, int index)
{
  float val;
  if(index < 0) return -1;
  read(offset+index*sizeof(val),&val,sizeof(val));
  return val;
}

int rlSharedMemory::writeInt(unsigned long offset, int index, int val)
{
  int ret;
  if(index < 0) return -1;
  ret = write(offset+index*sizeof(val),&val,sizeof(val));
  return ret;
}

int rlSharedMemory::writeShort(unsigned long offset, int index, int val)
{
  int ret;
  short int val2;

  if(index < 0) return -1;
  val2 = (short int) val;
  ret = write(offset+index*sizeof(val2),&val2,sizeof(val2));
  return ret;
}

int rlSharedMemory::writeByte(unsigned long offset, int index, unsigned char val)
{
  int ret;
  if(index < 0) return -1;
  ret = write(offset+index*sizeof(val),&val,sizeof(val));
  return ret;
}

int rlSharedMemory::writeFloat(unsigned long offset, int index, float val)
{
  int ret;
  if(index < 0) return -1;
  ret = write(offset+index*sizeof(val),&val,sizeof(val));
  return ret;
}

void *rlSharedMemory::getUserAdr()
{
  return (void *) user_adr;
}

int rlSharedMemory::shmKey()
{
  int imurx = 0; // rlmurxjan2025
#ifdef WIN32
  if(shmkey == NULL) imurx = -1;
#else
  if(shmkey == -1) imurx = -1;
#endif

  //return shmkey;
  return imurx;
}

int rlSharedMemory::shmId()
{
  int imurx = 0; // rlmurxjan2025
#ifdef WIN32
  if(id == NULL) imurx = -1;
#else
  if(id == -1) imurx = -1;
#endif

  //return id;
  return imurx;
}

unsigned long rlSharedMemory::size()
{
  return _size;
}

