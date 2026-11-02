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

#ifdef RLUNIX
#include <mutex>
// Nestor: flock es por descripcion de archivo abierta: los hilos que usan el mismo objeto no se excluyen
// entre si (entre procesos si). Un mutex por objeto (repartidos en una tabla, sin cambiar la clase).
static std::mutex &threadLock(const void *who)
{
  static std::mutex table[64];
  return table[((unsigned long long) (size_t) who >> 4) % 64];
}
#endif
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
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>

// Nestor: la memoria compartida es POSIX (shm_open/mmap) con un nombre que sale del camino completo del .shm.
// Antes era System V con ftok(name,'b'): ftok usa 16 bits del inodo y 8 del dispositivo, y dos .shm podian dar
// la misma clave (dos daemons escribiendo el mismo segmento sin aviso). Ademas en macOS System V tiene limites
// chicos (8 segmentos por proceso, 4 MB). En Linux se abre /dev/shm directamente (es lo que hace shm_open, y
// asi no hace falta -lrt con glibc viejas); en macOS shm_open (nombres de hasta 31 caracteres).
static unsigned long long rlShmFnv64(const char *s)
{
  unsigned long long h = 1469598103934665603ULL;
  for(; *s != '\0'; s++) { h ^= (unsigned char) *s; h *= 1099511628211ULL; }
  return h;
}

static void rlShmPosixName(const char *file, char *out, size_t n)
{
  char abs[PATH_MAX];
  const char *path = (realpath(file, abs) != NULL) ? abs : file;
  snprintf(out, n, "/pvb_%016llx", rlShmFnv64(path));
}

#ifndef __APPLE__
static const char *rlShmDir()
{
  static const char *dir = (access("/dev/shm", W_OK) == 0) ? "/dev/shm" : "/tmp";
  return dir;
}
#endif

static int rlShmOpen(const char *pname, int flags, int mode)
{
#ifdef __APPLE__
  return shm_open(pname, flags, mode);
#else
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s%s", rlShmDir(), pname);
  return open(path, flags | O_CLOEXEC, mode);
#endif
}

static int rlShmUnlink(const char *pname)
{
#ifdef __APPLE__
  return shm_unlink(pname);
#else
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s%s", rlShmDir(), pname);
  return unlink(path);
#endif
}

// el segmento System V que creaban las versiones anteriores para este archivo: si nadie lo usa se borra
// (si no, quedaba ocupando RAM hasta reiniciar la PC)
static void rlShmRemoveOldSysV(const char *file)
{
  key_t key = ftok(file, 'b');
  if(key == (key_t) -1) return;
  int oldid = shmget(key, 0, 0);
  if(oldid < 0) return;
  struct shmid_ds ds;
  if(shmctl(oldid, IPC_STAT, &ds) != 0) return;
  if(ds.shm_nattch != 0) return;
  if(ds.shm_perm.cuid != geteuid() && geteuid() != 0) return;
  shmctl(oldid, IPC_RMID, NULL);
}
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

#ifndef RLWIN32
#include <sys/stat.h>
#include <sys/types.h>
#endif

int rlMkdirParents(const char *filepath)
{
  char buf[4096];
  if(filepath == NULL || strlen(filepath) >= sizeof(buf)) return -1;
  strcpy(buf, filepath);
  for(char *c = buf + 1; *c != '\0'; c++)
  {
    if(*c != '/' && *c != '\\') continue;
    if(c == buf + 2 && buf[1] == ':') continue; // "C:\" (unidad)
    char save = *c;
    *c = '\0';
#ifdef RLWIN32
    CreateDirectoryA(buf, NULL);   // si ya existe, falla sin hacer nada
#else
    mkdir(buf, 0755);
#endif
    *c = save;
  }
  return 0;
}

// Nestor: espacio reservado al principio, antes de los datos. En Windows es fijo de 8 bytes:
// sizeof(pthread_mutex_t) da 4 en 32 bits y 8 en 64, y un daemon de 32 bits dejaba los datos
// corridos 4 bytes para las herramientas de 64 (el candado real es LockFileEx, no este espacio)
#ifdef RLWIN32
#define RLSHM_HEADER ((unsigned long) 8)
#else
#define RLSHM_HEADER ((unsigned long) sizeof(pthread_mutex_t))
#endif

rlSharedMemory::rlSharedMemory(const char *shmname, unsigned long Size, int rwmode)
{
  rlMkdirParents(shmname); // Nestor: la carpeta de la memoria compartida (p. ej. automation del proyecto)
#ifdef RLUNIX
  status  = OK;
  id = shmkey = -1;
  posixName[0] = 0;
  base_adr = user_adr = NULL;
  mutex = NULL;
  name = new char[strlen(shmname)+1];
  strcpy(name,shmname);
  _size    = Size + RLSHM_HEADER;

  // create file
  fdlock = open(name, O_RDWR | O_CREAT, rwmode );
  if(fdlock < 0)     
  {
    int ret; 
    char buf[1024];
    sprintf(buf,"could not write shm=%s\n",shmname);
    ret = ::write(1,buf,strlen(buf));
    if(ret < 0) exit(-1);
    sprintf(buf,"you have to run this program as root !!!\n");
    ret = ::write(1,buf,strlen(buf));
    if(ret < 0) exit(-1);
    status=ERROR_FILE;
    exit(-1);
  }

  // Nestor: POSIX con nombre por el camino completo (ver rlShmPosixName). El lock del archivo ordena la
  // creacion entre procesos (el primero le da el tamanio).
  flock(fdlock,LOCK_EX);
  rlShmRemoveOldSysV(name);
  rlShmPosixName(name, posixName, sizeof(posixName));
  const char *pname = posixName;
  int fd = rlShmOpen(pname, O_RDWR | O_CREAT, rwmode);
  struct stat st;
  if(fd >= 0 && fstat(fd, &st) == 0 && (unsigned long) st.st_size < _size)
  {
    // nueva, o mas chica (se agregaron tags y se regenero el daemon). Linux la agranda aunque otros la tengan
    // mapeada (siguen usando su parte); macOS solo deja darle tamanio una vez: se crea de nuevo y los procesos
    // que tenian la vieja (pvserver, historiador...) hay que reiniciarlos.
    if(ftruncate(fd, _size) != 0 && st.st_size != 0)
    {
      fprintf(stderr, "rlSharedMemory: %s: la memoria compartida existente tiene %lu bytes y se piden %lu: "
                      "se crea de nuevo; reiniciar los procesos que la usaban\n",
              shmname, (unsigned long) st.st_size, (unsigned long) _size);
      close(fd);
      rlShmUnlink(pname);
      fd = rlShmOpen(pname, O_RDWR | O_CREAT, rwmode);
      if(fd >= 0 && ftruncate(fd, _size) != 0) { close(fd); fd = -1; }
    }
  }
  if(fd >= 0) fchmod(fd, rwmode);  // como antes shmget: sin la umask
  // Nestor: ante un error se dice la causa (antes los tags se leian como 0 sin otro aviso) y se libera el
  // lock del archivo (antes quedaba tomado y frenaba a los demas procesos que abrian esta memoria).
  if(fd < 0)
  {
    int err = errno;
    fprintf(stderr, "rlSharedMemory: %s: shm_open %s: %s\n", shmname, pname, strerror(err));
    flock(fdlock,LOCK_UN);
    status=ERROR_SHMGET; return;
  }

  void *adr = mmap(NULL, _size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);
  if(adr == MAP_FAILED)
  {
    int err = errno;
    fprintf(stderr, "rlSharedMemory: %s: mmap: %s\n", shmname, strerror(err));
    flock(fdlock,LOCK_UN); status=ERROR_SHMAT; return;
  }
  base_adr = (char *) adr;
  id = shmkey = 0;

  mutex     = (pthread_mutex_t *) base_adr;
  user_adr  = base_adr + RLSHM_HEADER;
  flock(fdlock,LOCK_UN);
#endif

#ifdef __VMS
  int file_existed = 0;
  long ret,fd,page_size,pagelets,pagelet_size,file_block_size,flags,item,ident[2];
  FILE *fp;
  ADD  add_in,add_ret;
  struct dsc$descriptor_s section_name;
  struct FAB fab;

  status  = OK;
  name = new char[strlen(shmname)+1];
  strcpy(name,shmname);
  _size    = Size + RLSHM_HEADER;
  // The file block size is fixed
  file_block_size = 512; // Bytes
  // Get the page size
  item = SYI$_PAGE_SIZE;
  ret = lib$getsyi( &item       ,
                    &page_size  ,
                    0           ,
                    0           ,
                    0           ,
                    0
                   );
  if(ret != SS$_NORMAL) { status=ERROR_FILE; return; }
  // Fill descriptor for section name
  section_name.dsc$w_length  = strlen(name);
  section_name.dsc$a_pointer = name;
  section_name.dsc$b_dtype   = DSC$K_DTYPE_T;
  section_name.dsc$b_class   = DSC$K_CLASS_S;
  // The pagelet size is fixed
  pagelet_size = 512; // Bytes
  // Get memory
  if(_size % page_size == 0) pagelets = _size / pagelet_size;
  else                      pagelets = (_size / page_size + 1) * (page_size / pagelet_size);
  ret = sys$expreg(pagelets,&add_ret,0,0);
  if(ret != SS$_NORMAL) { status=ERROR_FILE; return; }
  // Set the addresses
  base_adr = (char *) add_ret.start;
  user_adr = base_adr + RLSHM_HEADER;
  mutex    = (pthread_mutex_t *) base_adr;
  if(base_adr == NULL) { status=ERROR_SHMAT;  return; }
  // Fill the fab
  fab = cc$rms_fab;                       // Initialize fab
  fab.fab$b_fac = fab.fab$b_fac | FAB$M_PUT | FAB$M_DEL | FAB$M_GET | FAB$M_UPD;
  fab.fab$l_fna = name;
  fab.fab$b_fns = strlen(name);
  fab.fab$l_fop = fab.fab$l_fop
                      | FAB$M_CIF            // create file if not existent
                      | FAB$M_CTG            // contiguous
                      | FAB$M_UFO;           // user open
  fab.fab$b_shr =   fab.fab$b_shr            // shareble access
                      | FAB$M_SHRPUT
                      | FAB$M_UPI;
  fab.fab$l_alq = pagelets * pagelet_size / file_block_size;
  // Open the section file
  ret = sys$create (&fab);
  if(ret != RMS$_NORMAL && ret != RMS$_CREATED)
  {
    sys$close (&fab);
    status=ERROR_FILE;
    return;
  }
  // Set the channel
  fd = fab.fab$l_stv;
  // Fill the input address
  add_in.start = add_ret.start;
  add_in.end   = add_ret.end;
  // Clear ident
  ident[0] = 0;
  ident[1] = 0;
  // Set flags
  flags = 0;
  flags = SEC$M_GBL | SEC$M_WRT | SEC$M_PERM;
  // Create and map the section
  ret = sys$crmpsc(&add_in ,&add_ret,
                     (long)0       ,  // acmode
                     flags         ,  // flags
                     &section_name ,  // gsdnam
                     &ident        ,  // ident
                     (long)0       ,  // relpag
                     (short)fd     ,  // chan
                     pagelets      ,  // pagcnt
                     (long)0       ,  // vbn
                     (long)0       ,  // prot
                     (long)0          // pfc
                   );
  if(ret != SS$_NORMAL && ret != SS$_CREATED)
  {
    sys$close(&fab);
    status=ERROR_FILE;
    return;
  }
  // Test the section addresses
  if(add_in.start != add_ret.start || add_in.end != add_ret.end)
  {
    sys$close(&fab);
    status=ERROR_FILE;
    return;
  }
  // Close the section file
  ret = sys$close(&fab);
  // rlwthread_mutex_init(mutex,NULL);
  if(file_existed == 0) myinit(mutex);
#endif

#ifdef RLWIN32
    HANDLE hShmem;
    status  = OK;
    id = shmkey = NULL; base_adr = user_adr = NULL;

    // Convertir nombre a Unicode
    wchar_t *wname = new wchar_t[strlen(shmname)+1];
    MultiByteToWideChar(CP_ACP, 0, shmname, -1, wname, strlen(shmname)+1);

    name = new char[strlen(shmname)+1];
    strcpy(name, shmname);
    _size = Size + RLSHM_HEADER;

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

    if(hShmem == NULL) { CloseHandle(hSharedFile); hSharedFile = INVALID_HANDLE_VALUE; status=ERROR_FILE; return; }

    base_adr = (char *) MapViewOfFile(
        hShmem,              // object to map view of
        FILE_MAP_WRITE,      // read/write access
        0,                   // high offset: map from
        0,                   // low offset: beginning
        0);                  // default: map entire file

    if(base_adr == NULL) { CloseHandle(hShmem); CloseHandle(hSharedFile); hSharedFile = INVALID_HANDLE_VALUE; status=ERROR_FILE; return; }

    id     = hShmem;
    shmkey = hSharedFile;
    mutex     = (pthread_mutex_t *) base_adr;
    user_adr  = base_adr + RLSHM_HEADER;
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
  UnmapViewOfFile(base_adr);        // Nestor: sin esto la vista quedaba mapeada
  CloseHandle((HANDLE) id);
  CloseHandle((HANDLE) shmkey);     // shmkey es hSharedFile
#elif defined(RLUNIX)
  if(base_adr != NULL) munmap(base_adr, _size);
  if(fdlock >= 0) close(fdlock);
#endif
}

int rlSharedMemory::deleteSharedMemory()
{
#ifdef RLUNIX
  if(status != OK) return -1;
  //rlwthread_mutex_destroy(mutex);
  flock(fdlock,LOCK_UN);
  // Nestor: como antes IPC_RMID: se borra el nombre y la memoria sigue mapeada hasta el destructor
  rlShmUnlink(posixName);
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
  UnlockFile(hSharedFile,0,0,_size,0); // Changed by FMakkinga 18-03-2013
  CloseHandle(hSharedFile);           // Nestor: una sola vez (shmkey es el mismo handle)
  status = ~OK;
  return 0;
#endif
}

int rlSharedMemory::write(unsigned long offset, const void *buf, int len)
{
  void *ptr;
  if(status != OK)       return -1;
  if(len <= 0)           return -1;
  if(offset+len > _size - RLSHM_HEADER) return -1; // Nestor: user_adr ya esta corrido RLSHM_HEADER
  ptr = user_adr + offset;
#ifdef RLWIN32
  LockFileEx(hSharedFile,LOCKFILE_EXCLUSIVE_LOCK,0,_size,0,&overlapped); // Changed by FMakkinga 18-03-2013
#elif defined(RLUNIX)
  threadLock(this).lock();   // Nestor: flock no excluye a los hilos del mismo proceso (mismo descriptor)
  flock(fdlock,LOCK_EX);
#else
  mylock(mutex,1);
#endif
  memcpy(ptr,buf,len);
#ifdef RLWIN32
  UnlockFileEx(hSharedFile,0,_size,0,&overlapped);                       // Changed by FMakkinga 18-03-2013
#elif defined(RLUNIX)
  flock(fdlock,LOCK_UN);
  threadLock(this).unlock();
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
  if(offset+len > _size - RLSHM_HEADER) return -1; // Nestor: user_adr ya esta corrido RLSHM_HEADER
  ptr = user_adr + offset;
#ifdef RLWIN32
  LockFileEx(hSharedFile,LOCKFILE_EXCLUSIVE_LOCK,0,_size,0,&overlapped); // Changed by FMakkinga 18-03-2013
#elif defined(RLUNIX)
  threadLock(this).lock();   // Nestor: flock no excluye a los hilos del mismo proceso (mismo descriptor)
  flock(fdlock,LOCK_EX);
#else
  mylock(mutex,1);
#endif
  memcpy(buf,ptr,len);
#ifdef RLWIN32
  UnlockFileEx(hSharedFile,0,_size,0,&overlapped);                       // Changed by FMakkinga 18-03-2013
#elif defined(RLUNIX)
  flock(fdlock,LOCK_UN);
  threadLock(this).unlock();
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

