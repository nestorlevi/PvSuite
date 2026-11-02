/***************************************************************************
                          rlmailbox.cpp  -  description
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
#include "rldefine.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifndef RLWIN32
#include <unistd.h>
#endif
#include "rlmailbox.h"
#include <vector>
#include "rlcutil.h"
#include "rlsharedmemory.h"   // rlMkdirParents
#include <string>

#ifdef RLUNIX
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <limits.h>

// Nestor: la clave de la cola sigue saliendo de ftok(name,'b'), que usa 16 bits del inodo y 8 del dispositivo:
// dos .mbx pueden caer en la misma cola. Por eso cada mensaje lleva como mtype un hash del camino completo del
// mailbox y el lector toma solo los suyos (antes mtype=1 y msgrcv tomaba cualquiera: las escrituras al PLC1
// las podia recibir el daemon del PLC2). Nunca vale 1: lo que quedo en la cola de una version anterior se descarta.
static long rlMbxType(const char *file)
{
  char abs[PATH_MAX];
  const char *path = (realpath(file, abs) != NULL) ? abs : file;
  unsigned int h = 2166136261u;
  for(; *path != 0; path++) { h ^= (unsigned char) *path; h *= 16777619u; }
  h &= 0x7fffffff;
  return (h < 2) ? (long) h + 2 : (long) h;
}
#endif

#ifdef __VMS
#include <lib$routines.h>
#include <libdef.h>
#include <dvidef.h>
#include <ssdef.h>
#include <iodef.h>
#include <descrip.h>
#include <starlet.h>
typedef struct
{
  short    iostat;
  unsigned short msg_len;
  int      reader_pid;
}IOSB;
#endif

#ifdef RLWIN32
#include <windows.h>
#include <sddl.h>

// Nestor (revision D4): el buzon con permisos explicitos (control total para el usuario del proceso, Administradores
// y SYSTEM; nadie mas), lo mismo que Windows pone hoy por omision pero sin depender de la configuracion de la PC.
// Devuelve NULL si no se pudo armar (se crea como antes, con la DACL por omision); liberar con LocalFree
static PSECURITY_DESCRIPTOR rlMailboxSecurity()
{
  HANDLE token = NULL;
  if(!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return NULL;
  DWORD size = 0;
  GetTokenInformation(token, TokenUser, NULL, 0, &size);
  std::vector<unsigned char> info(size ? size : 1);
  LPWSTR sid = NULL;
  if(size > 0 && GetTokenInformation(token, TokenUser, &info[0], size, &size))
    ConvertSidToStringSidW(((TOKEN_USER *) &info[0])->User.Sid, &sid);
  CloseHandle(token);
  if(sid == NULL) return NULL;
  std::wstring sddl = std::wstring(L"D:P(A;;FA;;;") + sid + L")(A;;FA;;;BA)(A;;FA;;;SY)";
  LocalFree(sid);
  PSECURITY_DESCRIPTOR sd = NULL;
  if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &sd, NULL)) return NULL;
  return sd;
}
#endif

rlMailbox::rlMailbox(const char *mbxname)
{
#ifdef RLUNIX
FILE  *fp;
key_t key;

  status = OK;
  buffer = NULL;
  buffer_size = 0;
  mtype = 2;
  name = new char[strlen(mbxname)+1];
  strcpy(name,mbxname);
  // create file
  rlMkdirParents(name); // Nestor: la carpeta del mailbox (p. ej. automation del proyecto)
  fp = fopen(name,"r");
  if(fp == NULL)
  {
    fp = fopen(name,"w");
    if(fp == NULL)
    {
      int ret;
      char buf[1024];
      sprintf(buf,"could not write mailbox=%s\n",mbxname);
      ret = ::write(1,buf,strlen(buf));
      if(ret < 0) exit(-1);
      sprintf(buf,"you have to run this program as root !!!\n");
      ret = ::write(1,buf,strlen(buf));
      if(ret < 0) exit(-1);
      exit(-1);
    }
    fchmod(fileno(fp),0644); // Nestor: antes 0x0fff (07777: setuid, setgid y escritura para todos); solo sirve para ftok
    if(fp == NULL) { status=COULD_NOT_CREATE_MAILBOX; return; }
  }
  fclose(fp);
  // getkey

  // old stuff, without suggestions from Stefan Lievens 
  // key = ftok(name, 'R');

  key = ftok(name, 'b');
  if(key == ((key_t) (-1))) { status=COULD_NOT_GET_KEY; return; }
  // get chanid

  // old stuff, without suggestions from Stefan Lievens 
  // chanid = msgget(key,IPC_CREAT);
  chanid = msgget(key, 0600 | IPC_CREAT);
  if(chanid == -1) { status=COULD_NOT_GET_CHAN_ID; return; }
  mtype = rlMbxType(name);
  {
    // mensajes viejos (mtype=1) que nadie va a leer: llenarian la cola
    std::vector<unsigned char> old(sizeof(long) + 256);
    while(msgrcv(chanid, (struct msgbuf *) old.data(), 256, 1, IPC_NOWAIT | MSG_NOERROR) >= 0);
  }
#endif

#ifdef __VMS
  long   ret;
  short  chan_id;
  struct dsc$descriptor_s dmbxname;

  chanid = -1;
  status = OK;
  name = new char[strlen(mbxname)+1];
  strcpy(name,mbxname);
  dmbxname.dsc$w_length  = strlen(name);
  dmbxname.dsc$a_pointer = name;
  dmbxname.dsc$b_dtype   = DSC$K_DTYPE_T;
  dmbxname.dsc$b_class   = DSC$K_CLASS_S;
  ret = sys$crembx((char)1,                     // 0 temp 1 permanence flag
                   &chan_id,                    // i/o channel
                   MAX_MAILBOX,                 // max msg length
                   MAX_MAILBOX*4,               // max buffer size
                   (long)0,
                   (long)0,
                   &dmbxname);                   // mailbox name
  if(ret == SS$_NORMAL) { chanid = chan_id; return; }
  else                  { status = COULD_NOT_CREATE_MAILBOX; return; }
#endif

#ifdef RLWIN32
  //rlmurxjan2025 chanid = -1;
  chanid = NULL;
  status = OK;
  buffer = NULL;      // Nestor: sin esto el destructor hacia delete [] de basura (heap corrupto en Windows)
  buffer_size = 0;
  name = new char[strlen(mbxname)+1];
  strcpy(name,mbxname);
  // Nestor: el nombre es el de un mailslot (\\.\mailslot\<name>): CreateMailslot no acepta '/' y fallaba
  // sin aviso con "automation/mbx/PLC1.mbx" (el daemon no recibia las escrituras). Quien abre si lo acepta,
  // asi que cambiarlo en los dos lados deja todo igual que antes para los nombres con '\'.
  for(char *c = name; *c; c++) if(*c == '/') *c = '\\';
#endif
}

rlMailbox::~rlMailbox()
{
  delete [] name;
  if(buffer != NULL) delete [] buffer;
#ifdef WIN32
  if(chanid == NULL) return;
#else
  if(chanid < 0) return;
#endif

#ifdef __VMS
  sys$dassgn((short)chanid);
#endif

#ifdef RLWIN32
  CloseHandle((HANDLE) chanid);
#endif
}

int rlMailbox::writeNoWait(const void *buf, int len)
{
#ifdef RLUNIX
  status = OK;
  unsigned char *message = new unsigned char [sizeof(long) + len];
  long *lptr = (long *) message;
  *lptr = mtype;
  memcpy(&message[sizeof(long)],buf,len);
  int retlen = msgsnd(chanid,(struct msgbuf *) message, len, IPC_NOWAIT);
  int err = errno;
  delete [] message;
  if(retlen == 0) return len;
  return (err == EAGAIN) ? MAILBOX_FULL : MAILBOX_ERROR;
#else
  return write(buf, len);   // Windows / VMS: la escritura no se queda esperando
#endif
}

int rlMailbox::write(const void *buf, int len)
{
  status = OK;

#ifdef RLUNIX
  int retlen;
  unsigned char *message = new unsigned char [sizeof(long) + len];
  long *lptr = (long *) message;
  *lptr = mtype;
  memcpy(&message[sizeof(long)],buf,len);
  retlen = msgsnd(chanid,(struct msgbuf *) message, len, 0);
  delete [] message;
  // Nestor: msgsnd devuelve 0 si escribio: se devuelven los bytes, como dice rlmailbox.h y como en Windows
  return (retlen == 0) ? len : MAILBOX_ERROR;
#endif

#ifdef __VMS
  int  ret;
  IOSB iosb;
  ret = sys$qiow (0,
                  (short) chanid,
                  IO$_WRITEVBLK | IO$M_NOW | IO$M_NORSWAIT,
                  &iosb,
                  0,0,
                  buf,
                  len,0,0,0,0);
  len = iosb.msg_len;
  if     (ret == SS$_MBFULL) return MAILBOX_FULL;
  else if(ret != SS$_NORMAL) return MAILBOX_ERROR;
  else                       return len; // Success
#endif

#ifdef RLWIN32
  BOOL bret;
  unsigned long numWritten;

  // Nestor: si el lector se reinicio (daemon levantado de nuevo) el handle guardado es del mailslot viejo
  // y la escritura falla: se reabre y se reintenta una vez (antes se perdia ese pedido)
  for(int attempt = 0; attempt < 2; attempt++)
  {
  bool cached = (chanid != NULL);
  int imurx = 0; // rlmurxjan2025
#ifdef WIN32
  if(chanid == NULL) imurx = -1;
#else
  imurx = chanid;
#endif

  //if(chanid == -1)
  if(imurx == -1)
  {
    HANDLE h;
    std::string mbxname = "\\\\.\\mailslot\\";   // Nestor: sin limite fijo (antes char[1024] con strcpy/strcat)
    mbxname += name;
    h = CreateFileA(
              mbxname.c_str(),                     // pointer to name of the file
              GENERIC_READ | GENERIC_WRITE,        // access (read-write) mode
              FILE_SHARE_READ | FILE_SHARE_WRITE,  // share mode
              NULL,                                // pointer to security attributes
              OPEN_EXISTING,                       // how to create
              FILE_ATTRIBUTE_NORMAL,               // file attributes
              NULL                                 // handle to file with attributes to copy
                  );
    if(h == INVALID_HANDLE_VALUE)
    {
      status = GetLastError();
      return MAILBOX_ERROR;
    }
#ifdef WIN32
    chanid = h;
#else
    chanid = (int) h;
#endif
  }

  bret = WriteFile(
            (HANDLE) chanid,      // handle to file to write to
            buf,                  // pointer to data to write to file
            len,                  // number of bytes to write
            &numWritten,          // pointer to number of bytes written
            NULL                  // pointer to structure for overlapped I/O
                  );
  if(bret==0)
  {
    status = GetLastError();
    CloseHandle((HANDLE) chanid);
#ifdef WIN32
    chanid = NULL;
#else
    chanid = -1;
#endif
    if(cached) continue;   // handle viejo: se abre de nuevo
    return MAILBOX_ERROR;
  }
  return numWritten;
  }
  return MAILBOX_ERROR;
#endif
}

int rlMailbox::read(void *buf, int maxlen, int wait)
{
    char *cbuf = (char *)buf;
    status = OK;

#ifdef RLUNIX
    // Implementación para Unix/Linux
    int len;
    unsigned char *message = new unsigned char[sizeof(long) + maxlen];
    for (;;)
    {
        if (wait == WAIT)
            len = msgrcv(chanid, (struct msgbuf *)message, maxlen, mtype, 0);
        else
            len = msgrcv(chanid, (struct msgbuf *)message, maxlen, mtype, IPC_NOWAIT);
        if (len >= 0) break;
        // Nestor: esperando, una senal (EINTR: un SIGSTOP/SIGCONT, gdb) o un mensaje mas largo que maxlen
        // (E2BIG: de otro programa o de un cliente viejo) no son el fin del mailbox. Antes el daemon salia del
        // bucle de lectura y las escrituras al PLC dejaban de llegar sin aviso. El mensaje largo se descarta.
        if (wait == WAIT && errno == EINTR) continue;
        if (errno == E2BIG)
        {
            msgrcv(chanid, (struct msgbuf *)message, maxlen, mtype, IPC_NOWAIT | MSG_NOERROR);
            fprintf(stderr, "rlMailbox: se descarto un mensaje de mas de %d bytes (%s)\n", maxlen, name ? name : "");
            if (wait == WAIT) continue;
        }
        break;
    }

    if (len >= 0)
    {
        // Nestor: antes (len < maxlen) un mensaje de exactamente maxlen bytes se perdia (devolvia len sin copiarlo)
        memcpy(buf, &message[sizeof(long)], len);
        if (len < maxlen) cbuf[len] = '\0';
    }
    else if (maxlen > 0)
    {
        cbuf[0] = '\0';
    }
    delete[] message;
    return len;
#endif

#ifdef __VMS
    // Implementación para VMS
    int ret, len;
    IOSB iosb;
    if (wait == NOWAIT)
    {
        ret = sys$qiow(0,
                       (short)chanid,
                       IO$_READVBLK | IO$M_NOWAIT,
                       &iosb,
                       0, 0,
                       buf,
                       maxlen, 0, 0, 0, 0);
    }
    else
    {
        ret = sys$qiow(0,
                       (short)chanid,
                       IO$_READVBLK,
                       &iosb,
                       0, 0,
                       buf,
                       maxlen, 0, 0, 0, 0);
    }
    len = (int)iosb.msg_len;
    if (len < maxlen && len >= 0) cbuf[len] = '\0';
    if (ret == SS$_NORMAL && iosb.iostat == SS$_NORMAL) return len;
    else if (iosb.iostat == SS$_NORMAL) { status = -1; return MAILBOX_ERROR; }
    else if (ret == SS$_NORMAL)
    {
        if (wait == NOWAIT && iosb.iostat == SS$_ENDOFFILE) { status = -2; return MAILBOX_ERROR; }
        else { status = -3; return MAILBOX_ERROR; }
    }
    status = -4;
    return MAILBOX_ERROR;
#endif

#ifdef RLWIN32
    // Implementación para Windows
    HANDLE h;
    wchar_t mbxname[1024];
    DWORD lenRead;
    BOOL bret;

    if (chanid == NULL)
    {
        // Nestor: el nombre convertido entero (antes wchar_t[256]: un nombre largo quedaba sin terminar)
        int nw = MultiByteToWideChar(CP_ACP, 0, name, -1, NULL, 0);
        std::wstring wname(nw > 0 ? nw : 1, L'\0');
        if(nw > 0) MultiByteToWideChar(CP_ACP, 0, name, -1, &wname[0], nw);
        std::wstring full = std::wstring(L"\\\\.\\mailslot\\") + wname.c_str();
        if(full.size() >= sizeof(mbxname) / sizeof(mbxname[0])) { status = ERROR_FILENAME_EXCED_RANGE; return MAILBOX_ERROR; }
        wcscpy(mbxname, full.c_str());

        PSECURITY_DESCRIPTOR sd = rlMailboxSecurity();
        if(sd == NULL) ::printf("rlMailbox: sin permisos propios para %s (error %lu): se usan los de Windows por omision\n", name, GetLastError());
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), sd, FALSE };
        h = CreateMailslotW(mbxname, MAX_MAILBOX, MAILSLOT_WAIT_FOREVER, sd ? &sa : NULL);
        if(sd) LocalFree(sd);
        if (h == INVALID_HANDLE_VALUE) {
            status = GetLastError();
            return MAILBOX_ERROR;
        }
        chanid = h;
        bret = SetMailslotInfo(h, MAILSLOT_WAIT_FOREVER);
        if (!bret) {
            status = GetLastError();
            return MAILBOX_ERROR;
        }
    }

    if (wait == NOWAIT)
    {
        bret = SetMailslotInfo(chanid, 0);
        if (!bret) { status = GetLastError(); return MAILBOX_ERROR; }
        lenRead = 0;
        BOOL rret = ReadFile(chanid, buf, maxlen, &lenRead, NULL);
        DWORD rerr = rret ? 0 : GetLastError();
        bret = SetMailslotInfo(chanid, MAILSLOT_WAIT_FOREVER);
        // Nestor: antes el resultado de ReadFile se pisaba con el de SetMailslotInfo: un error se
        // informaba como 0 bytes. Vacio = MAILBOX_ERROR, como msgrcv con IPC_NOWAIT en Linux.
        if (!rret)
        {
            if (maxlen > 0) cbuf[0] = '\0';
            if (rerr != ERROR_SEM_TIMEOUT) status = rerr;
            return MAILBOX_ERROR;
        }
        if (!bret) { status = GetLastError(); return MAILBOX_ERROR; }
        if ((int)lenRead < maxlen && (int)lenRead >= 0) cbuf[lenRead] = '\0';
        return lenRead;
    }

    for (;;)
    {
        lenRead = 0;
        bret = ReadFile(chanid, buf, maxlen, &lenRead, NULL);
        if (bret) break;
        status = GetLastError();
        // Nestor: un mensaje mas largo que maxlen se queda en el mailslot y cada ReadFile fallaba igual: se
        // descarta y se sigue esperando (antes el daemon salia del bucle y las escrituras dejaban de llegar)
        DWORD next = 0, count = 0;
        if (status == ERROR_INSUFFICIENT_BUFFER && GetMailslotInfo(chanid, NULL, &next, &count, NULL) &&
            next != MAILSLOT_NO_MESSAGE && next > 0)
        {
            std::vector<char> big(next);
            DWORD n = 0;
            ReadFile(chanid, big.data(), next, &n, NULL);
            fprintf(stderr, "rlMailbox: se descarto un mensaje de %lu bytes (mas de %d)\n", (unsigned long) next, maxlen);
            status = OK;
            continue;
        }
        return MAILBOX_ERROR;
    }
    if ((int)lenRead < maxlen && (int)lenRead >= 0) cbuf[lenRead] = '\0';
    return lenRead;
#endif
}

void rlMailbox::clear()
{
  char *buf = new char[MAX_MAILBOX];
  while(read(buf,MAX_MAILBOX,NOWAIT) > 0);
  delete [] buf;
}

int rlMailbox::printf(const char *format, ...)
{
  int ret;
  char message[rl_PRINTF_LENGTH]; // should be big enough

  va_list ap;
  va_start(ap,format);
  ret = rlvsnprintf(message, rl_PRINTF_LENGTH - 1, format, ap);
  va_end(ap);
  if(ret < 0) return ret;
  return write(message,strlen(message));
}

int rlMailbox::setReadBufferSize(int size)
{
  if(buffer != NULL) delete [] buffer;
  buffer_size = size;
  buffer = new char[size];
  return size;
}

const char *rlMailbox::read(int wait)
{
  if(buffer == NULL)
  {
    buffer_size = 4096;
    buffer = new char[buffer_size];
  }
  int ret = read(buffer,buffer_size,wait);
  if(ret < 0) buffer[0] = '\0';
  return buffer;
}

int rlMailbox::write(const char *message)
{
  return write(message,strlen(message)+1);
}


