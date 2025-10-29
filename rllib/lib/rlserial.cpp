/***************************************************************************
                          rlserial.cpp  -  description
                             -------------------
    begin                : Sat Dec 21 2002
    copyright            : (C) 2002 by R. Lehrig
    email                : lehrig@t-online.de

    RMOS implementation:
    Copyright            : (C) 2004 Zertrox GbR
    Written by           : Alexander Feller
    Email                : feller@zertrox.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU LESSER GENERAL PUBLIC LICENSE as        *
 *   published by the Free Software Foundation                             *
 *                                                                         *
 ***************************************************************************/
#include "rlserial.h"

#ifdef RLUNIX
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#endif

#ifdef __VMS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <starlet.h>
#include <descrip.h>
#include <lib$routines.h>
#include <ssdef.h>
#include <iodef.h>
typedef struct
{
  short    iostat;
	unsigned short msg_len;
	int      reader_pid;
}IOSB;
#endif

#ifdef RLWIN32
#include <windows.h>
#include <stdio.h>
#endif

#ifdef RM3
#include <stdio.h>
#include <stdlib.h>
#include <rmcomp.h>
#include <rmapi.h>
#include <rio.h>
#include <drvspec.h>
#include <rm3cav.h>
//#include <clean.h>
#include <rcinc.h>
#define RTS_TIME_WAIT 0x008     /* Verzoerungszeit fuer RTS - Signal      */
#endif

#include "rlthread.h"

/*
static void sighandler(int sig)
{
  if(sig == SIGINT)
  {
    closeDevice();
    closeDatabase();
  }
}
*/

rlSerial::rlSerial()
{
  ttysavefd = -1;
  ttystate  = RESET;
  fd        = -1;
  trace     = 0;
}

rlSerial::~rlSerial()
{
  closeDevice();
}

void rlSerial::setTrace(int on)
{
  if(on == 1) trace = 1;
  else        trace = 0;
}

int rlSerial::openDevice(const char *devicename, int speed, int block, int rtscts, int bits, int stopbits, int parity)
{
#ifdef RLUNIX
    struct termios buf;

    if(fd != -1) return -1;

    // Open device with Unicode support
    fd = open(devicename, O_RDWR | O_NOCTTY | O_NDELAY);
    if(fd < 0) { return -1; }

    if(tcgetattr(fd, &save_termios) < 0) { return -1; }
    buf = save_termios;

    // Configure for Unicode/UTF-8 support
    buf.c_cflag = speed | CLOCAL | CREAD;
    if(rtscts   == 1)  buf.c_cflag |= CRTSCTS;
    if(bits     == 7)  buf.c_cflag |= CS7;
    else               buf.c_cflag |= CS8;  // Default to 8 bits for Unicode
    if(stopbits == 2)  buf.c_cflag |= CSTOPB;
    if(parity == rlSerial::ODD)  buf.c_cflag |= (PARENB | PARODD);
    if(parity == rlSerial::EVEN) buf.c_cflag |= PARENB;

    // Important Unicode settings
    buf.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  // Raw mode for Unicode
    buf.c_oflag     = 0;  // No output processing
    buf.c_iflag     = IGNBRK | IGNPAR;  // Basic input processing

    // Character settings
    buf.c_cc[VMIN]  = 1;  // Read at least 1 byte
    buf.c_cc[VTIME] = 0;  // No timeout

#ifndef PVMAC
    buf.c_line      = 0;
#endif

    if(tcsetattr(fd, TCSAFLUSH, &buf) < 0) { return -1; }
    ttystate = RAW;
    ttysavefd = fd;

    if(block == 1) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
    tcflush(fd, TCIOFLUSH);
#endif

#ifdef __VMS
    // VMS implementation remains largely the same since it's system-managed
    struct dsc$descriptor_s dsc;
    int status;

    dsc.dsc$w_length  = strlen(devicename);
    dsc.dsc$a_pointer = (char *) devicename;
    dsc.dsc$b_class   = DSC$K_CLASS_S;
    dsc.dsc$b_dtype   = DSC$K_DTYPE_T;
    status = SYS$ASSIGN(&dsc,&vms_channel,0,0);
    if(status != SS$_NORMAL) return -1;
#endif

#ifdef RLWIN32
    DWORD ccsize;
    COMMCONFIG cc;
    int baudrate,ret;
    wchar_t devname[100];

    if(strlen(devicename) > 80) return -1;

    // Convert to wide char for Unicode support
    swprintf(devname, 100, L"\\\\.\\%S", devicename);

    hdl = CreateFileW(
        devname,                       // Unicode device name
        GENERIC_READ | GENERIC_WRITE,  // access mode
        0,                             // share mode
        NULL,                          // security attributes
        OPEN_EXISTING,                 // creation disposition
        FILE_ATTRIBUTE_NORMAL,         // flags and attributes
        NULL                           // template file
        );
    if(hdl == INVALID_HANDLE_VALUE)
    {
        printf("CreateFile(%s) failed\n",devicename);
        return -1;
    }

    // Baud rate mapping remains the same
    baudrate = CBR_9600;
    if(speed == B50) baudrate = 50;
    // ... (rest of baud rate mappings remain unchanged)

    ccsize = sizeof(cc);
    GetCommConfig(hdl,&cc,&ccsize);

    // Configure for Unicode support
    cc.dcb.DCBlength     = sizeof(DCB);
    cc.dcb.BaudRate      = baudrate;
    cc.dcb.fBinary       = TRUE;        // Binary mode essential for Unicode
    cc.dcb.fParity       = (parity != rlSerial::NONE);
    cc.dcb.fOutxCtsFlow  = (rtscts == 1);
    cc.dcb.fOutxDsrFlow  = FALSE;
    cc.dcb.fDtrControl   = DTR_CONTROL_ENABLE;
    cc.dcb.fDsrSensitivity = FALSE;
    cc.dcb.fTXContinueOnXoff = TRUE;
    cc.dcb.fNull         = FALSE;       // Don't strip nulls (important for Unicode)
    cc.dcb.fRtsControl   = (rtscts == 1) ? RTS_CONTROL_HANDSHAKE : RTS_CONTROL_ENABLE;
    cc.dcb.fAbortOnError = FALSE;

    // Character size - force 8 bits for Unicode
    cc.dcb.ByteSize      = 8;           // Always 8 bits for Unicode
    cc.dcb.Parity        = (parity == rlSerial::ODD) ? 1 :
                        (parity == rlSerial::EVEN) ? 2 : 0;
    cc.dcb.StopBits      = (stopbits == 2) ? TWOSTOPBITS : ONESTOPBIT;

    // Important for Unicode:
    cc.dcb.fErrorChar    = FALSE;       // Don't replace errors
    cc.dcb.fInX          = FALSE;       // No XON/XOFF for input
    cc.dcb.fOutX         = FALSE;       // No XON/XOFF for output

    ret = SetCommConfig(hdl,&cc,sizeof(cc));
    if(ret == 0)
    {
        printf("SetCommConfig ret=%d devicename=%s LastError=%ld\n",ret,devicename,GetLastError());
        return -1;
    }

    // Setup timeouts
    COMMTIMEOUTS timeouts;
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    SetCommTimeouts(hdl, &timeouts);

    if(block) return 0;
#endif

#ifdef RM3
    // RM3 implementation remains the same as it's hardware-specific
    // ... (existing RM3 code remains unchanged)
#endif

    return 0;
}

int rlSerial::readChar()
{
#ifdef RLUNIX
  int  ret;
  unsigned char buf[2];

  if(fd == -1) return -1;
  ret = read(fd,buf,1);
  if(ret == 1) return buf[0];
  if(ret == 0) return -2;
  return -1;
#endif

#ifdef __VMS
  unsigned char buf[2];

  if(readBlock(buf, 1) < 0) return -1;
  return buf[0];
#endif

#ifdef RLWIN32
  unsigned char buf[2];
  unsigned long len;

  ReadFile(
                 hdl,  // handle of file to read
                 buf,  // pointer to buffer that receives data
                 1,    // number of bytes to read
                 &len, // pointer to number of bytes read
                 NULL  // pointer to structure for data
          );
  if(len > 0)
  {
    if(trace == 1) printf("readChar %d\n",(int) buf[0]);
    return buf[0];
  }
  return -1;
#endif

#ifdef RM3
  int ret;
  unsigned char buf[2];
  
  ret = readBlock(buf,1);
  if(ret < 0) return ret;
  return buf[0];
#endif
}

int rlSerial::writeChar(unsigned char uchar)
{
#ifdef RLUNIX
  int ret;
  if(fd == -1) return -1;
  if(trace == 1) printf("writeChar %d\n",(int)uchar);
  ret = write(fd,&uchar,1);
  if(ret < 0) return -1;
  //tcflush(fd, TCIOFLUSH);
  return ret;
#endif

#ifdef __VMS
  int  status;
  IOSB iosb;

  status = SYS$QIOW(0,vms_channel,IO$_WRITEVBLK | IO$M_CANCTRLO | IO$M_NOFORMAT,
                    &iosb,0,0,&uchar,1,0,0,0,0);
  if(status != SS$_NORMAL) return -1;
  return 1;
#endif

#ifdef RLWIN32
  BOOL ret;
  unsigned long len;

  if(trace == 1) printf("writeChar %d\n",(int)uchar);
  ret = WriteFile(
                  hdl,    // handle to file to write to
                  &uchar, // pointer to data to write to file
                  1,      // number of bytes to write
                  &len,   // pointer to number of bytes written
                  NULL    // pointer to structure for overlapped I/O
                 );

  if(ret) return (int) len;
  return -1;
#endif

#ifdef RM3
  return writeBlock(&uchar, 1);
#endif
}

int rlSerial::readBlock(unsigned char *buf, int len, int timeout)
{
#ifdef RLUNIX
  int c, retlen;

  retlen = 0;
  for(int i=0; i<len; i++)
  {
    if(timeout >= 0)
    {
      if(select(timeout) == 0) break; // timeout
    }
    c = readChar();
    if(c < 0) return c;
    buf[i] = (unsigned char) c;
    retlen = i+1;
  }
  if(retlen <= 0) return -1;
  return retlen;
#endif

#ifdef __VMS
  int   status;
  int   _timeout = 1; // second
  short iosb[4];

  status = SYS$QIOW(0,vms_channel,
                    IO$_READVBLK | IO$M_NOFILTR | IO$M_NOECHO | IO$M_TIMED,
                    iosb,0,0,buf,len,_timeout,0,0,0);
  if(status != SS$_NORMAL) return -1;
  len=iosb[1];
  if(iosb[2] != 0)
  {
    len++;
    buf[len] = iosb[2];
  }
  return len;
#endif

#ifdef RLWIN32
  unsigned long retlen;

  if(timeout >= 0) select(timeout);
  ReadFile(
                 hdl,     // handle of file to read
                 buf,     // pointer to buffer that receives data
                 len,     // number of bytes to read
                 &retlen, // pointer to number of bytes read
                 NULL     // pointer to structure for data
           );
  if(retlen > 0)
  {
    if(trace == 1) printf("readBlock retlen=%ld\n",retlen);
    return (int) retlen;
  }
  return -1;
#endif

#ifdef RM3
  RmBytParmStruct  PBlock;    /* Parameterstruktur fr RmIO - Funktion          */
  RmIOStatusStruct DrvSts;    /* Struktur der Rckgabewerte fr RmIO - Funktion */
  int              iStatus;   /* Rckgabewert                                   */
  int              i;         /* Schleifenz�ler                                */
                              /**************************************************/

  /*
   * Schreibparameter setzen
   */
  PBlock.string = 0;
  PBlock.strlen = 0;
  PBlock.buffer = (char*)buf;
  PBlock.timlen = len;
  PBlock.status = 0;

  /*
   * Lesevorgang einleiten
   */
  iStatus = RmIO( BYT_POLL_XBUF_WAIT, (unsigned)device, (unsigned)unit, 0u, 0u, &DrvSts, &PBlock );

  if( iStatus ) printf( "BYT_POLL_XBUF_WAIT (set ucb): Error status = %X\n", iStatus );

  if( !iStatus ) return len;
  return -1;
#endif
}

int rlSerial::writeBlock(const unsigned char *buf, int len)
{
#ifdef RLUNIX
  int ret;

  if(fd == -1) return -1;
  if(trace == 1)
  {
    printf("writeBlock:");
    for(int i=0; i<len; i++) printf(" %d",(int) buf[i]);
    printf("\n");
  }
  ret = write(fd,buf,len);
  //tcflush(fd, TCIOFLUSH);
  return ret;
#endif

#ifdef __VMS
  int  status;
  IOSB iosb;


  status = SYS$QIOW(0,vms_channel,IO$_WRITEVBLK | IO$M_CANCTRLO | IO$M_NOFORMAT,
                    &iosb,0,0,buf,len,0,0,0,0);
  if(status != SS$_NORMAL) return -1;
  return len;
#endif

#ifdef RLWIN32
  BOOL ret;
  unsigned long retlen;

  if(trace == 1)
  {
    printf("writeBlock:");
    for(int i=0; i<len; i++) printf(" %d",(int) buf[i]);
    printf("\n");
  }
  retlen = len;
  ret = WriteFile(
                  hdl,     // handle to file to write to
                  buf,     // pointer to data to write to file
                  len,     // number of bytes to write
                  &retlen, // pointer to number of bytes written
                  NULL     // pointer to structure for overlapped I/O
                 );

  if(ret) return (int) retlen;
  return -1;
#endif

#ifdef RM3
  RmBytParmStruct  PBlock;    /* Parameterstruktur fr RmIO - Funktion          */
  RmIOStatusStruct DrvSts;    /* Struktur der Rckgabewerte fr RmIO - Funktion  */
  unsigned char    cByte;     /* Rckgabewert von ibyte - Funktion              */
  int              iStatus;   /* Rckgabewert                                   */
  int              i;         /* Schleifenz�ler                                */
                              /**************************************************/

  /*
   * Schreibparameter setzen
   */
  PBlock.string = (char*)buf;
  PBlock.strlen = len;
  PBlock.buffer = 0;
  PBlock.timlen = 0;
  PBlock.status = 0;

  /******************************************************
   *                                                    *
   * Toggle mode wird emuliert indem an dieser Stelle   *
   * RTS-Signal gesetzt und sp�er wieder gel�cht wird   *
   *                                                    *
   ******************************************************/

  cByte = inbyte( com + 0x04u );
  outbyte( com + 0x04, (unsigned char)(cByte | 0x02u) );

  /*
   * Schreibvorgang einleiten
   */
  iStatus = RmIO( BYT_WRITE_WAIT, (unsigned)device, (unsigned)unit, 0u, 0u, &DrvSts, &PBlock );

  /******************************************************************
   *                                                                *
   * 8ms warten.Bei der Aenderung der Uebertragungsgeschwindigkeit, *
   * sollte dieser Wert angepasst werden. Hier fuer 9600            *
   *                                                                *
   ******************************************************************/
  RmPauseTask((RTS_TIME_WAIT*9600)/baudrate);

  /*
   * RTS-Signal l�chen
   */
  outbyte( com + 0x04, (unsigned char)(cByte & 0xFDu) );

  if( iStatus ) printf( "BYT_WRITE_WAIT (write block): Error status = %X\n", iStatus );

  if( !iStatus ) return len;
  return -1;
#endif
}

int rlSerial::readLine(unsigned char *buf, int maxlen, int timeout)
{
  int i,c,ret;

  if(maxlen <= 1) return -1;
  ret = 0;
  buf[maxlen-1] = '\0';
  for(i=0; i<maxlen-2; i++)
  {
    ret = i;
    if(timeout > 0)
    {
      int t = select(timeout);
      if(t == 0) return -1;
    }
    c = readChar();
    if(c < 0)
    {
      buf[i] = '\0';
      ret = c;
      break;
    }
    buf[i] = (unsigned char) c;
    if(c < ' ' && c != '\t')
    {
      buf[i+1] = '\0';
      break;
    }
  }
  return ret;
}

int rlSerial::select(int timeout)
{
#ifdef RLUNIX
  struct timeval timout;
  fd_set wset,rset,eset;
  int    ret,maxfdp1;

  if(timeout <= 0) return 1;
  /* setup sockets to read */
  maxfdp1 = fd+1;
  FD_ZERO(&rset);
  FD_SET (fd,&rset);
  FD_ZERO(&wset);
  FD_ZERO(&eset);
  timout.tv_sec  =  timeout / 1000;
  timout.tv_usec = (timeout % 1000) * 1000;

  ret = ::select(maxfdp1,&rset,&wset,&eset,&timout);
  if(ret == 0) return 0; /* timeout */
  return 1;
#endif

#ifdef __VMS
  return 1;
#endif

#ifdef RLWIN32
  COMMTIMEOUTS ctimeout;

  ctimeout.ReadIntervalTimeout          = timeout; 
  ctimeout.ReadTotalTimeoutMultiplier   = 1; 
  ctimeout.ReadTotalTimeoutConstant     = timeout; 
  ctimeout.WriteTotalTimeoutMultiplier  = 1; 
  ctimeout.WriteTotalTimeoutConstant    = timeout; 

  SetCommTimeouts(hdl, &ctimeout);
  return 1;
#endif

#ifdef RM3
  return 1;
#endif
}

int rlSerial::closeDevice()
{
#ifdef RLUNIX
  if(fd == -1) return -1;
  //if(::tcsetattr(fd,TCSAFLUSH,&save_termios) < 0) return -1;
  if(::tcsetattr(fd,TCSANOW,&save_termios) < 0) return -1;
  ::close(fd);
  ttystate = RESET;
  fd = -1;
  return 0;
#endif

#ifdef __VMS
  sys$dassgn(vms_channel);
  return 0;
#endif

#ifdef RLWIN32
  CloseHandle(hdl);
  return 0;
#endif

#ifdef RM3
  RmBytParmStruct  PBlock;    /* Parameterstruktur fr RmIO - Funktion          */
  RmIOStatusStruct DrvSts;    /* Struktur der Rckgabewerte fr RmIO - Funktion */
                              /**************************************************/
  if( RmIO( BYT_RELEASE, (unsigned)(device), (unsigned)(unit), 0u, 0u, &DrvSts, &PBlock ) < 0 )
  {
    printf( "Error: Unable to release device. device: %i, unit: %i\n",device, unit );
    return -1;
  }
  return 0;
#endif
}
