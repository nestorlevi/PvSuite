/***************************************************************************
 *   Copyright (C) 2005 by R. Lehrig                                       *
 *   lehrig@t-online.de                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
//
// mkmodbus name.mkmodbus  (C) R. Lehrig 2003-2004
//
// Nestor: ademas de los "cycle" de siempre, acepta "tag" (variables con nombre) en el
// .mkmodbus y/o en un .csv (tags=archivo.csv). Los tags se agrupan en bloques de lectura
// (direcciones contiguas o cercanas) que se agregan despues de los ciclos y contadores de
// siempre, asi un proyecto viejo genera exactamente la misma memoria compartida.
//

#include "../pvbrowser/pvdefine.h"
#include "../rllib/lib/rltagdef.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <QFile>

// Nestor: nombre de archivo (UTF-8) para fopen: en Windows fopen usa la pagina de codigos ANSI
static const char *nativeName(const char *utf8)
{
  static QByteArray buf;
  buf = QFile::encodeName(QString::fromUtf8(utf8));
  return buf.constData();
}
QFileInfo f_info;
QString g_daemon_error; // ultimo error de gmodbus / gsiemens (lo muestra la ventana del daemon)
static char line[256*256];
static char target[1024];
static char shared_memory[4096];
static char mailbox[4096];
#define SERIAL 1
#define SOCKET 2
static int  communication;
static char tty[4096];
static int  baudrate;
static int  rtscts;
static int  parity;
static int  protocol;
static char tcpadr[4096];
static int  tcpport;
static char simadr[4096]; // Nestor: simulation_tcpadr= (el daemon la usa si se ejecuta con -sim)
static int  simport;
static int  base;
static char eventhost[4096];
static int  eventport;
static int  idletime; // Nestor: ms de espera antes de cada pedido (-1 = automatico)
static int  cycletime; // Nestor: duracion minima de una vuelta en ms (-1 = 50)
static rlTagList *tags = NULL;
static std::vector<int> plcs;   // Nestor: esclavos leidos (tags de sistema Esclavo<n>)
static std::vector<std::string> cycleLabels; // Nestor: "FC3 0..9 (cycle1)" para los mensajes del daemon
std::string gdaemonCString(const char *s);
void gdaemonNormPath(char *path);

static int fail(const QString &text)
{
  g_daemon_error = text;
  printf("%s\n", text.toUtf8().constData());
  return -1;
}

static int init(const char *name)
{
  int i,slave,function,start_adr,num_register,cyclecnt;
  char *cptr;
  FILE *fp,*fout;
  fp = fopen(nativeName(name),"r");
  if(fp == NULL)
  {
    return fail(QCoreApplication::translate("gdaemon", "could not open %1").arg(name));
  }

  eventhost[0] = '\0';
  eventport = -1;
  idletime = -1;
  cycletime = -1;
  rtscts = 1;
  parity = 0;
  protocol = 0; // RTU
  communication = 0;
  tty[0] = tcpadr[0] = shared_memory[0] = mailbox[0] = '\0';
  baudrate = tcpport = 0;
  simadr[0] = '\0';
  simport = 0;
  while(fgets(line,sizeof(line)-1,fp) != NULL)
  {
    if(strncmp("target=",line,7) == 0)
    {
      sscanf(line,"target=%1023s",target);
    }
    else if(strncmp("tcpport=",line,8) == 0)
    {
      sscanf(line,"tcpport=%d",&tcpport);
    }
    else if(strncmp("simulation_tcpadr=",line,18) == 0)
    {
      sscanf(line,"simulation_tcpadr=%1023s",simadr);
    }
    else if(strncmp("simulation_tcpport=",line,19) == 0)
    {
      sscanf(line,"simulation_tcpport=%d",&simport);
    }
    else if(strncmp("tcpadr=",line,7) == 0)
    {
      sscanf(line,"tcpadr=%1023s",tcpadr);
    }
    else if(strncmp("tty=",line,4) == 0)
    {
      sscanf(line,"tty=%1023s",tty);
    }
    else if(strncmp("baudrate=",line,9) == 0)
    {
      sscanf(line,"baudrate=%d",&baudrate);
    }
    else if(strncmp("rtscts=",line,7) == 0)
    {
      sscanf(line,"rtscts=%d",&rtscts);
    }
    else if(strncmp("parity=",line,7) == 0)
    {
      sscanf(line,"parity=%d",&parity);
    }
    else if(strncmp("idletime=",line,9) == 0)
    {
      sscanf(line,"idletime=%d",&idletime);
    }
    else if(strncmp("cycletime=",line,10) == 0)
    {
      sscanf(line,"cycletime=%d",&cycletime);
    }
    else if(strncmp("protocol=",line,9) == 0)
    {
      sscanf(line,"protocol=%d",&protocol);
    }
    else if(strncmp("communication=",line,14) == 0)
    {
      communication = SERIAL;
      if(strstr(line,"socket") != NULL) communication = SOCKET;
    }
    else if(strncmp("mailbox=",line,8) == 0)
    {
      sscanf(line,"mailbox=%1023s",mailbox);
      gdaemonNormPath(mailbox);
    }
    else if(strncmp("shared_memory=",line,14) == 0)
    {
      sscanf(line,"shared_memory=%1023s",shared_memory);
      gdaemonNormPath(shared_memory);
    }
    else if(strncmp("eventlog",line,8) == 0)
    {
      cptr = strstr(line,"port=");
      if(cptr != NULL) sscanf(cptr,"port=%d",&eventport);
      cptr = strstr(line,"host=");
      if(cptr != NULL)
      {
        snprintf(eventhost,sizeof(eventhost),"%s",&cptr[5]);
        cptr = strstr(eventhost," ");
        if(cptr != NULL) *cptr = '\0';
      }
    }
  }

  fclose(fp);

  printf("target=%s\n",target);
  printf("shared_memory=%s\n",shared_memory);
  printf("mailbox=%s\n",mailbox);
  if     (communication == SERIAL) printf("communication=serial\n");
  else if(communication == SOCKET) printf("communication=socket\n");
  else                           { return fail(QCoreApplication::translate("gdaemon", "undefined communication (communication=serial or communication=socket)")); }
  printf("tty=%s\n",tty);
  printf("baudrate=%d\n",baudrate);
  printf("rtscts=%d\n",rtscts);
  printf("parity=%d\n",parity);
  printf("protocol=%d\n",protocol);
  printf("tcpadr=%s\n",tcpadr);
  printf("tcpport=%d\n",tcpport);
  if(eventport != -1) printf("eventlog host=%s port=%d\n",eventhost,eventport);

  // Nestor: tags (en el .mkmodbus y/o en el .csv de "tags=")
  delete tags;
  tags = new rlTagList(rlTagList::MODBUS);
  tags->readMkFile(nativeName(name));

  fp = fopen(nativeName(name),"r");
  if(fp == NULL)
  {
    return fail(QCoreApplication::translate("gdaemon", "could not open %1").arg(name));
  }

  sprintf(line,"%s.h",QFile::encodeName(f_info.filePath().remove(".mkmodbus")).constData());
  fout = fopen(line,"w");
  if(fout == NULL)
  {
    fclose(fp);
    return fail(QCoreApplication::translate("gdaemon", "could not write %1").arg(line));
  }
  fprintf(fout,"// Modbus Signal assignment of %s\n",target);
  fprintf(fout,"#define %s_SHARED_MEMORY \"%s\"\n",target,gdaemonCString(shared_memory).c_str());
  fprintf(fout,"#define %s_MAILBOX \"%s\"\n",target,gdaemonCString(mailbox).c_str());

  cyclecnt = 1;
  base = 0;
  plcs.clear();
  cycleLabels.clear();
  while(fgets(line,sizeof(line)-1,fp) != NULL)
  {
    if(strncmp("cycle",line,5) == 0 && strncmp("cycletime",line,9) != 0)
    {
      slave = function = start_adr = num_register = -1;
      cptr = strstr(line,"slave=");
      if(cptr!=NULL) sscanf(cptr,"slave=%d",&slave);
      cptr = strstr(line,"function=");
      if(cptr!=NULL) sscanf(cptr,"function=%d",&function);
      cptr = strstr(line,"start_adr=");
      if(cptr!=NULL) sscanf(cptr,"start_adr=%d",&start_adr);
      cptr = strstr(line,"num_register=");
      if(cptr!=NULL) sscanf(cptr,"num_register=%d",&num_register);
      if(slave==-1 || function==-1 || start_adr==-1 || num_register==-1)
      {
        fclose(fp);
        fclose(fout);
        return fail(QCoreApplication::translate("gdaemon", "parameter missing in: %1").arg(line));
      }
      fprintf(fout,"\n#define %s_CYCLE%d_BASE %d\n",target,cyclecnt,base);
      fprintf(fout,"#define %s_CYCLE%d_NUM %d\n",target,cyclecnt,num_register);
      i = num_register;
      if(function==1 || function==2)
      {
        i = num_register / 8;
        if((num_register % 8) > 0) i++;
      }
      else
      {
        i = num_register*2;
      }
      fprintf(fout,"#define %s_CYCLE%d_NUM_BYTES %d\n",target,cyclecnt,i);
      plcs.push_back(slave);
      cycleLabels.push_back("FC" + std::to_string(function) + " " + std::to_string(start_adr) + ".." + std::to_string(start_adr + num_register - 1) + " (cycle" + std::to_string(cyclecnt) + ")");
      base += i;
      cyclecnt++;
    }
  }
  fprintf(fout,"\n");
  fprintf(fout,"#define %s_LIFE_COUNTER_BASE %d\n",target,base); base += 2;
  fprintf(fout,"#define %s_READ_ERROR_COUNT_BASE %d\n",target,base); base += 2;
  fprintf(fout,"#define %s_WRITE_ERROR_COUNT_BASE %d\n",target,base); base += 2;
  // Nestor: los bloques de los tags van despues (no cambia nada para los proyectos sin tags)
  int newbase = tags->layout(base);
  if(newbase < 0)
  {
    QString err = QCoreApplication::translate("gdaemon", "Errores en los tags:");
    for(size_t e=0; e<tags->errors.size(); e++) err += "\n" + QString::fromStdString(tags->errors[e]);
    fclose(fp);
    fclose(fout);
    return fail(err);
  }
  base = newbase;
  // Nestor: tags de sistema (TARGET_SYS) al final de la memoria compartida
  for(size_t b=0; b<tags->blocks.size(); b++) plcs.push_back(tags->blocks[b].slave);
  base = tags->layoutSys(base, plcs);
  if(!tags->errors.empty())
  {
    QString err = QCoreApplication::translate("gdaemon", "Errores en los tags:");
    for(size_t e=0; e<tags->errors.size(); e++) err += "\n" + QString::fromStdString(tags->errors[e]);
    fclose(fp);
    fclose(fout);
    return fail(err);
  }
  fprintf(fout,"\n#define %s_SHARED_MEMORY_SIZE %d\n",target,base);
  tags->writeHeader(fout, target);
  // Nestor: mapa de la memoria compartida para el historiador (TARGET.tagmap)
  tags->writeTagMap(QFile::encodeName(f_info.filePath().remove(".mkmodbus") + ".tagmap").constData(), target, shared_memory, base, mailbox);

  fclose(fp);
  fclose(fout);
  return 0;
}

// Nestor: ruta de shared_memory= / mailbox= tal como la usan el daemon, el pvserver y el historiador:
// "\\" doble (como se escribia antes para el .h) queda simple. Relativa = desde la carpeta del proyecto.
void gdaemonNormPath(char *path)
{
  char *out = path;
  for(char *in = path; *in; in++)
  {
    if(*in == '\\' && in[1] == '\\') continue; // de "\\" queda una
    *out++ = *in;
  }
  *out = '\0';
}

// Nestor: texto para un literal de C en el codigo generado ("c:\automation\shm" -> "c:\\automation\\shm")
std::string gdaemonCString(const char *s)
{
  std::string out;
  for(; s != NULL && *s; s++)
  {
    if(*s == '\\' || *s == '"') out += '\\';
    out += *s;
  }
  return out;
}

// Nestor: numero de ciclo, mensajes de consola y tags de sistema del daemon (rldaemonstatus.h).
// Lo usan gmodbus y gsiemens: escribe NUM_CYCLES, NUM_BLOCKS, tagBlockLabel[], sysPlcs[] y "status".
void gdaemonWriteStatus(FILE *fout, const rlTagList *list, const char *target, const std::vector<std::string> &cycleLabels, const char *plcWord)
{
  int ncycles = (int) cycleLabels.size(), nblocks = (int) list->blocks.size();
  fprintf(fout,"\n");
  fprintf(fout,"// mensajes en la consola: 0 = solo errores, 1 = resumen cada DAEMON_CONSOLE_INTERVAL s, 2 = cada bloque\n");
  fprintf(fout,"// (console= y console_interval= en el archivo del daemon; al ejecutarlo: -q / -n / -v)\n");
  fprintf(fout,"#define DAEMON_CONSOLE %d\n", list->console);
  fprintf(fout,"#define DAEMON_CONSOLE_INTERVAL %d\n", list->console_interval);
  fprintf(fout,"#define NUM_CYCLES %d // ciclos \"cycle\" del archivo\n", ncycles);
  fprintf(fout,"#define NUM_BLOCKS %d // ciclos + bloques de los tags\n", ncycles + nblocks);
  if(nblocks > 0)
  {
    fprintf(fout,"static const char *tagBlockLabel[] = {\n");
    for(int b=0; b<nblocks; b++)
    {
      const rlTagBlock &bl = list->blocks[b];
      std::string what = list->blockLabel(bl);
      if(list->protocol == rlTagList::MODBUS) what = "FC" + std::to_string(bl.area) + " " + what;
      fprintf(fout,"  \"%s\"%s\n", what.c_str(), (b + 1 < nblocks) ? "," : "");
    }
    fprintf(fout,"};\n");
  }
  std::vector<int> ids;
  for(size_t i=0; i<list->sys.size(); i++)
  {
    const rlTagDef &t = list->sys[i];
    if(t.name.rfind("Esclavo", 0) == 0 || t.name.rfind("Plc", 0) == 0) ids.push_back(t.slave);
  }
  if(!ids.empty())
  {
    fprintf(fout,"static const int sysPlcs[] = {");
    for(size_t i=0; i<ids.size(); i++) fprintf(fout,"%s%d", i ? ", " : " ", ids[i]);
    fprintf(fout," }; // %s_SYS.%s<n>\n", target, (list->protocol == rlTagList::SIEMENS) ? "Plc" : "Esclavo");
  }
  fprintf(fout,"rlDaemonStatus status(&shm, %d, NUM_BLOCKS, DAEMON_CONSOLE, DAEMON_CONSOLE_INTERVAL, \"%s\"); // %d = %s_SYS_BASE\n",
          list->sys_base, plcWord, list->sys_base, target);
}

static bool hasPlcTags()
{
  for(size_t i=0; i<tags->sys.size(); i++) if(tags->sys[i].name.rfind("Esclavo", 0) == 0) return true;
  return false;
}

static void writeStatus(FILE *fout)
{
  gdaemonWriteStatus(fout, tags, target, cycleLabels, "esclavo");
}

static int generate(const char *name)
{
  FILE *fout,*fp;
  char *cptr;
  int  slave,function,start_adr,num_register,cycles;

  sprintf(line,"%s.cpp",QFile::encodeName(f_info.filePath().remove(".mkmodbus")).constData());
  fout = fopen(line,"w");
  if(fout == NULL)
  {
    return fail(QCoreApplication::translate("gdaemon", "could not write %1").arg(line));
  }

  fprintf(fout,"//\n");
  fprintf(fout,"// Modbus daemon template                                 (C) R. Lehrig 2003\n");
  fprintf(fout,"//\n");
  fprintf(fout,"//\n");
  fprintf(fout,"// Attention: this program must be run as super user\n");
  fprintf(fout,"//\n");
  fprintf(fout,"\n");
  fprintf(fout,"#include <stdio.h>\n");
  fprintf(fout,"#include <stdlib.h>\n");
  fprintf(fout,"#include <chrono>\n");
  fprintf(fout,"#include \"rlmodbus.h\"\n");
  fprintf(fout,"#include \"rlsharedmemory.h\"\n");
  fprintf(fout,"#include \"rlmailbox.h\"\n");
  fprintf(fout,"#include \"rlthread.h\"\n");
  fprintf(fout,"#include \"rlcutil.h\"\n");
  fprintf(fout,"#include \"rldaemonstatus.h\"\n");
  if(eventport != -1)
  {
    fprintf(fout,"%s","#include \"rlevent.h\"\n");
  }
  // Nestor: el .h que se genero (lleva el nombre del .mkmodbus aunque haya "target=")
  fprintf(fout,"#include \"%s.h\"\n",f_info.completeBaseName().toUtf8().constData());
  fprintf(fout,"\n");
  if(baudrate <= 0) baudrate = 9600;
  // Nestor: espera antes de cada pedido. Antes era (4*1000)/(baudrate/100) ms (41 ms a 9600, tambien
  // con TCP). Ahora: serie = 3,5 caracteres de 11 bits como pide Modbus RTU (minimo 2 ms; 4 ms a 9600);
  // TCP = 0. "idletime=<ms>" en el .mkmodbus la fija (idletime=41 vuelve a la de antes).
  int idle = idletime;
  if(idle < 0)
  {
    if(communication == SOCKET) idle = 0;
    else
    {
      idle = (38500 + baudrate - 1) / baudrate; // 3,5 * 11 bits * 1000 ms / baudios, redondeado hacia arriba
      if(idle < 2) idle = 2;
    }
  }
  fprintf(fout,"#define MODBUS_IDLETIME %d // ms antes de cada pedido (idletime= en el .mkmodbus)\n",idle);
  fprintf(fout,"#define MODBUS_CYCLE_TIME %d // duracion minima de una vuelta en ms (cycletime= en el .mkmodbus)\n",
          (cycletime >= 0) ? cycletime : 50);
  fprintf(fout,"#define SERIAL_DEVICE   \"%s\"\n",gdaemonCString(tty).c_str());
  // Nestor: bits de registros (setBool): 1 = funcion 22 (Mask Write Register) en el PLC;
  // 0 = el daemon lee el registro, cambia el bit y lo escribe (sirve con cualquier PLC)
  fprintf(fout,"#define BIT_WRITE_FC22  %d\n", tags->bit_write_fc22);
  if(protocol == 1 && communication == SERIAL)
  {
    fprintf(fout,"rlModbus       modbus(1024+1,rlModbus::MODBUS_ASCII);\n");
  }
  else
  {
    fprintf(fout,"rlModbus       modbus(256+1,rlModbus::MODBUS_RTU);\n");
  }
  if(communication == SERIAL)
  {
    fprintf(fout,"rlSerial       serial;\n");
  }
  if(communication != SERIAL || simadr[0] != '\0')
  {
    // con serie y simulation_tcpadr: el socket solo se usa en simulacion
    if(communication == SERIAL) fprintf(fout,"rlSocket       sock(\"%s\",%d,1);\n", simadr, (simport > 0) ? simport : 502);
    else                        fprintf(fout,"rlSocket       sock(\"%s\",%d,1);\n",tcpadr,tcpport);
  }
  // Nestor: simulacion (simulation_tcpadr= en el .mkmodbus): con -sim (o PVB_SIMULATION=1) el daemon se
  // conecta a esa direccion; los tags, la memoria compartida y el .h son los mismos
  fprintf(fout,"#define SIM_TCPADR \"%s\" // simulation_tcpadr= (vacio = sin simulacion)\n", simadr);
  fprintf(fout,"#define SIM_TCPPORT %d\n", (simport > 0) ? simport : ((communication == SOCKET && tcpport > 0) ? tcpport : 502));
  fprintf(fout,"static int     useSocket = %d; // 1 = Modbus TCP (o simulacion)\n", (communication == SERIAL) ? 0 : 1);
  fprintf(fout,"rlSharedMemory shm(\"%s\",%d);\n",gdaemonCString(shared_memory).c_str(),base);
  fprintf(fout,"rlMailbox      mbx(\"%s\");\n",gdaemonCString(mailbox).c_str());
  fprintf(fout,"rlThread       thread;\n");
  fprintf(fout,"rlThread       watchdog;\n");
  fprintf(fout,"short          lifeCounter     = 0;\n");
  fprintf(fout,"short          readErrorCount  = 0;\n");
  fprintf(fout,"short          writeErrorCount = 0;\n");
  writeStatus(fout);
  fprintf(fout,"\n");
  fprintf(fout,"// watchdog\n");
  fprintf(fout,"static const char *av0 = \"\";\n");
  fprintf(fout,"static int watchcnt1 = 0;\n");
  fprintf(fout,"void *watchdogthread(void *arg)\n");
  fprintf(fout,"{\n");
  fprintf(fout,"  int cnt1 = -1;\n");
  fprintf(fout,"\n");
  fprintf(fout,"  while(1)\n");
  fprintf(fout,"  {\n");
  fprintf(fout,"    rlsleep(60000);\n");
  fprintf(fout,"    if(cnt1 == watchcnt1) break;\n");
  fprintf(fout,"    cnt1 = watchcnt1;\n");
  fprintf(fout,"  }\n");
  if(communication == SERIAL) fprintf(fout,"  serial.closeDevice();\n");
  fprintf(fout,"  rlsleep(100);\n");
  fprintf(fout,"#ifdef unix\n");
  fprintf(fout,"  rlexec(av0);\n");
  fprintf(fout,"#endif\n");
  fprintf(fout,"  exit(0); // pcontrol may start me again if rlexec fails\n");
  fprintf(fout,"  return arg;\n");
  fprintf(fout,"}\n");
  fprintf(fout,"\n");
  fprintf(fout,"// read mailbox and write to modbus\n");
  fprintf(fout,"void *reader(void *arg)\n");
  fprintf(fout,"{\n");
  fprintf(fout,"  int ret,slave,function,buflen;\n");
  fprintf(fout,"  unsigned char buf[256+1];\n");
  fprintf(fout,"\n");
  fprintf(fout,"  mbx.clear(); // clear old messages\n");
  fprintf(fout,"  while((buflen = mbx.read(buf,sizeof(buf))) > 0)\n");
  fprintf(fout,"  {\n");
  fprintf(fout,"    slave        = buf[0];\n");
  fprintf(fout,"    function     = buf[1];\n");
  fprintf(fout,"    thread.lock();\n");
  fprintf(fout,"    rlsleep(MODBUS_IDLETIME);\n");
  fprintf(fout,"    if(function == rlModbus::MaskWrite4XRegisters && BIT_WRITE_FC22 == 0 && buflen >= 8)\n");
  fprintf(fout,"    { // Nestor: funcion 22 emulada: leer el registro, cambiar los bits y escribirlo\n");
  fprintf(fout,"      int adr = buf[2]*256 + buf[3], and_mask = buf[4]*256 + buf[5], or_mask = buf[6]*256 + buf[7];\n");
  fprintf(fout,"      int rslave = slave, rfunction = 0, val;\n");
  fprintf(fout,"      unsigned char data[256+1];\n");
  fprintf(fout,"      ret = modbus.request(slave, rlModbus::ReadHoldingRegisters, adr, 1);\n");
  fprintf(fout,"      if(ret >= 0) ret = modbus.response(&rslave, &rfunction, data);\n");
  fprintf(fout,"      if(ret == 2)\n");
  fprintf(fout,"      {\n");
  fprintf(fout,"        val = data[0]*256 + data[1];\n");
  fprintf(fout,"        val = (val & and_mask) | (or_mask & ~and_mask & 0x0ffff);\n");
  fprintf(fout,"        data[0] = adr/256; data[1] = adr & 0x0ff; data[2] = val/256; data[3] = val & 0x0ff;\n");
  fprintf(fout,"        rlsleep(MODBUS_IDLETIME);\n");
  fprintf(fout,"        ret = modbus.write(slave, rlModbus::PresetSingleRegister, data, 4);\n");
  fprintf(fout,"        ret = modbus.response(&rslave, &rfunction, data);\n");
  fprintf(fout,"      }\n");
  fprintf(fout,"      else if(ret >= 0) ret = -1;\n");
  fprintf(fout,"    }\n");
  fprintf(fout,"    else\n");
  fprintf(fout,"    {\n");
  fprintf(fout,"      ret = modbus.write( slave, function, &buf[2], buflen-2);\n");
  fprintf(fout,"      ret = modbus.response( &slave, &function, buf);\n");
  fprintf(fout,"    }\n");
  if(communication != SERIAL || simadr[0] != '\0') fprintf(fout,"    if(useSocket && ret != rlModbus::MODBUS_SUCCESS && ret < 0) sock.disconnect();\n");
  if(eventport != -1)
  {
    fprintf(fout,"%s","    if(ret <= 0) { rlEvent(rlWarning,\"modbus.response mbx ret=%d slave=%d function=%d\",ret, slave, function); }\n");
  }
  fprintf(fout,"    rlsleep(MODBUS_IDLETIME);\n");
  fprintf(fout,"    thread.unlock();\n");
  fprintf(fout,"    if(ret < 0)\n");
  fprintf(fout,"    {\n");
  fprintf(fout,"      writeErrorCount++;\n");
  fprintf(fout,"      if(writeErrorCount >= 256*128) writeErrorCount = 0;\n");
  fprintf(fout,"      shm.write(%s_WRITE_ERROR_COUNT_BASE,&writeErrorCount,2);\n",target);
  fprintf(fout,"    }\n");
  fprintf(fout,"    { char what[32]; sprintf(what, \"FC%%d\", function); status.write(slave, what, ret); }\n");
  fprintf(fout,"  }\n");
  fprintf(fout,"  return arg;\n");
  fprintf(fout,"}\n");
  fprintf(fout,"\n");
  fprintf(fout,"// read cycle on modbus\n");
  fprintf(fout,"// index y what: para los mensajes y los tags de sistema (rldaemonstatus.h)\n");
  fprintf(fout,"int modbusCycle(int offset, int slave, int function, int start_adr, int num_register, int index, const char *what)\n");
  fprintf(fout,"{\n");
  fprintf(fout,"  unsigned char data[256+1];\n");
  fprintf(fout,"  int ret, plc = slave, rslave = slave, rfunction = function, expected = -1;\n");
  fprintf(fout,"  long long t0;\n");
  fprintf(fout,"\n");
  fprintf(fout,"  watchcnt1++;\n");
  fprintf(fout,"  if(watchcnt1 > 10000) watchcnt1 = 0;\n");
  fprintf(fout,"  rlsleep(MODBUS_IDLETIME);\n");
  fprintf(fout,"  thread.lock();\n");
  fprintf(fout,"  t0 = rlDaemonStatus::now();\n");
  fprintf(fout,"  ret = modbus.request(slave, function, start_adr, num_register);\n");
  fprintf(fout,"  if(ret >= 0) ret = modbus.response(&rslave, &rfunction, data);\n");
  // Nestor: una respuesta de otro pedido (atrasada, de otro esclavo o una excepcion) no se escribe en la memoria:
  // esclavo, funcion y cantidad de bytes tienen que ser los del pedido; si no, es un error (calidad mala)
  fprintf(fout,"  if(function == 1 || function == 2) expected = (num_register + 7) / 8;  // bobinas y entradas: bits\n");
  fprintf(fout,"  else if(function == 3 || function == 4) expected = 2 * num_register;   // registros\n");
  fprintf(fout,"  if(ret >= 0 && (rslave != slave || rfunction != function || (expected >= 0 && ret != expected)))\n");
  fprintf(fout,"  {\n");
  fprintf(fout,"    if(DAEMON_CONSOLE > 0) printf(\"respuesta que no corresponde (%%s): esclavo %%d funcion %%d bytes %%d (se esperaba %%d %%d %%d)\\n\", what ? what : \"\", rslave, rfunction, ret, slave, function, expected);\n");
  fprintf(fout,"    ret = -1;\n");
  fprintf(fout,"  }\n");
  if(communication == SOCKET || simadr[0] != '\0') fprintf(fout,"  if(useSocket && ret < 0) sock.disconnect();\n");
  fprintf(fout,"  thread.unlock();\n");
  fprintf(fout,"  if(ret > 0) shm.write(offset,data,ret);\n");
  fprintf(fout,"  else\n");
  fprintf(fout,"  {\n");
  fprintf(fout,"    readErrorCount++;\n");
  fprintf(fout,"    if(readErrorCount >= 256*128) readErrorCount = 0;\n");
  fprintf(fout,"    shm.write(%s_READ_ERROR_COUNT_BASE,&readErrorCount,2);\n",target);
  fprintf(fout,"  }\n");
  if(eventport != -1)
  {
    fprintf(fout,"%s","  if(ret <= 0) { rlEvent(rlWarning,\"modbus.response cycle ret=%d slave=%d function=%d start_adr=%d num_register=%d\",ret,slave,function,start_adr,num_register); rlsleep(5000); }\n");
  }
  fprintf(fout,"  status.block(index, plc, what, ret, rlDaemonStatus::now() - t0);\n");
  fprintf(fout,"  return ret;\n");
  fprintf(fout,"}\n");
  fprintf(fout,"\n");

  fprintf(fout,"static long long msNow()\n");
  fprintf(fout,"{\n");
  fprintf(fout,"  return (long long) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();\n");
  fprintf(fout,"}\n");
  fprintf(fout,"\n");
  if(!tags->blocks.empty())
  {
    // Nestor: bloques de los tags (direcciones contiguas o cercanas leidas de una vez)
    fprintf(fout,"// bloques de lectura de los tags (generados a partir de los \"tag\" del .mkmodbus)\n");
    fprintf(fout,"typedef struct { int slave, function, start, count, scan, offset, status; long long last; } TagBlock;\n");
    fprintf(fout,"static TagBlock tagBlock[] = {\n");
    fprintf(fout,"// slave function start count scan(ms) offset status\n");
    for(size_t b=0; b<tags->blocks.size(); b++)
    {
      const rlTagBlock &bl = tags->blocks[b];
      fprintf(fout,"  { %d, %d, %d, %d, %d, %d, %d, -1 }%s\n", bl.slave, bl.area, bl.start, bl.count,
              bl.scan, bl.offset, bl.status, (b + 1 < tags->blocks.size()) ? "," : "");
    }
    fprintf(fout,"};\n");
    fprintf(fout,"#define NUM_TAG_BLOCKS %d\n", (int) tags->blocks.size());
    fprintf(fout,"\n");
    fprintf(fout,"// lee los bloques que corresponden segun su scan; devuelve cuantos leyo\n");
    fprintf(fout,"int readTagBlocks()\n");
    fprintf(fout,"{\n");
    fprintf(fout,"  int i, ret, n = 0;\n");
    fprintf(fout,"  unsigned char ok;\n");
    fprintf(fout,"  for(i=0; i<NUM_TAG_BLOCKS; i++)\n");
    fprintf(fout,"  {\n");
    fprintf(fout,"    TagBlock *b = &tagBlock[i];\n");
    fprintf(fout,"    long long now = msNow();\n");
    fprintf(fout,"    if(b->scan > 0 && b->last >= 0 && (now - b->last) < b->scan) continue;\n");
    fprintf(fout,"    b->last = now;\n");
    fprintf(fout,"    ret = modbusCycle(b->offset, b->slave, b->function, b->start, b->count, NUM_CYCLES + i, tagBlockLabel[i]);\n");
    fprintf(fout,"    ok = (ret > 0) ? 1 : 0;\n");
    fprintf(fout,"    shm.write(b->status, &ok, 1);\n");
    fprintf(fout,"    n++;\n");
    fprintf(fout,"  }\n");
    fprintf(fout,"  return n;\n");
    fprintf(fout,"}\n");
    fprintf(fout,"\n");
  }

  fprintf(fout,"int main(int ac, char **av)\n");
  fprintf(fout,"{\n");
  fprintf(fout,"  int offset,first;\n");
  if(eventport != -1)
  {
       fprintf(fout,"  char *argv[] = {\"\",\"-eventhost=%s\",\"-eventport=%d\"};\n",eventhost,eventport);
       fprintf(fout,"\n");
       fprintf(fout,"  rlEventInit(3,argv,\"%s:\");\n",target);
  }
  fprintf(fout,"  if(ac > 0) av0 = av[0];\n");
  fprintf(fout,"  first = 1;\n");
  // Nestor: -sim o PVB_SIMULATION=1: conectarse al simulador (simulation_tcpadr=)
  fprintf(fout,"  int simulation = (getenv(\"PVB_SIMULATION\") != NULL && atoi(getenv(\"PVB_SIMULATION\")) == 1);\n");
  fprintf(fout,"  for(int i=1; i<ac; i++) if(strcmp(av[i], \"-sim\") == 0) simulation = 1;\n");
  fprintf(fout,"  if(simulation && SIM_TCPADR[0] == '\\0') { printf(\"-sim: falta simulation_tcpadr= en el archivo del daemon\\n\"); simulation = 0; }\n");
  if(simadr[0] != '\0')
  {
    fprintf(fout,"  if(simulation)\n");
    fprintf(fout,"  {\n");
    fprintf(fout,"    sock.setAdr(SIM_TCPADR);\n");
    fprintf(fout,"    sock.setPort(SIM_TCPPORT);\n");
    fprintf(fout,"    useSocket = 1;\n");
    fprintf(fout,"    printf(\"SIMULACION: %%s:%%d\\n\", SIM_TCPADR, SIM_TCPPORT);\n");
    fprintf(fout,"  }\n");
  }
  if(communication == SERIAL)
  {
    sprintf(line,"  while(!useSocket && serial.openDevice(SERIAL_DEVICE,B%d,1,%d,8,1,rlSerial::",baudrate,rtscts);
    if     (parity == 2) strcat(line,"EVEN) < 0)");
    else if(parity == 1) strcat(line,"ODD) < 0)");
    else                 strcat(line,"NONE) < 0)");
    fprintf(fout,"%s\n",line);
    //fprintf(fout,"  while(serial.openDevice(SERIAL_DEVICE,B%d) < 0)\n",baudrate);
    fprintf(fout,"  {\n");
    fprintf(fout,"%s","    if(first==1) printf(\"could not open serial device %s\\n\",SERIAL_DEVICE);\n");
    fprintf(fout,"    first = 0;\n");
    fprintf(fout,"    rlsleep(1000);\n");
    fprintf(fout,"  }\n");
  }
  fprintf(fout,"%s","  printf(\"\\n%s starting\\n\",av0);\n");
  if(eventport != -1)
  {
    fprintf(fout,"  rlEvent(rlInfo,\"%s starting\");\n",target);
  }
  if(communication == SERIAL && simadr[0] != '\0')
  {
    fprintf(fout,"  if(useSocket) modbus.registerSocket(&sock);\n");
    fprintf(fout,"  else          modbus.registerSerial(&serial);\n");
  }
  else if(communication == SERIAL)
  {
    fprintf(fout,"  modbus.registerSerial(&serial);\n");
  }
  else
  {
    fprintf(fout,"  modbus.registerSocket(&sock);\n");
  }
  fprintf(fout,"  thread.create(reader,NULL);\n");
  fprintf(fout,"  watchdog.create(watchdogthread,NULL);\n");
  if(hasPlcTags()) fprintf(fout,"  status.setPlcs(sysPlcs, (int) (sizeof(sysPlcs) / sizeof(sysPlcs[0])));\n");
  fprintf(fout,"  status.start(ac, av);\n");
  fprintf(fout,"\n");
  fprintf(fout,"  shm.write(%s_LIFE_COUNTER_BASE,&lifeCounter,2);\n",target);
  fprintf(fout,"  shm.write(%s_READ_ERROR_COUNT_BASE,&readErrorCount,2);\n",target);
  fprintf(fout,"  shm.write(%s_WRITE_ERROR_COUNT_BASE,&writeErrorCount,2);\n",target);
  if(!tags->blocks.empty())
  {
    fprintf(fout,"  for(int i=0; i<NUM_TAG_BLOCKS; i++) { unsigned char q = 0; shm.write(tagBlock[i].status, &q, 1); }\n");
  }
  fprintf(fout,"  while(1)\n");
  fprintf(fout,"  {\n");
  fprintf(fout,"    long long cycleStart = msNow();\n");
  fprintf(fout,"    status.beginCycle();\n");
  fprintf(fout,"    lifeCounter++;\n");
  fprintf(fout,"    if(lifeCounter >= 256*128) lifeCounter = 0;\n");
  fprintf(fout,"    shm.write(%s_LIFE_COUNTER_BASE,&lifeCounter,2);\n",target);
  fprintf(fout,"    offset = 0;\n");
  fprintf(fout,"    //    modbusCycle(offset, slave, function, start_adr, num_register);\n");

  // write cycles
  fp = fopen(nativeName(name),"r");
  if(fp == NULL)
  {
    fclose(fout);
    return fail(QCoreApplication::translate("gdaemon", "could not open %1").arg(name));
  }
  cycles = 0;
  while(fgets(line,sizeof(line)-1,fp) != NULL)
  {
    if(strncmp("cycle",line,5) == 0 && strncmp("cycletime",line,9) != 0)
    {
      slave = function = start_adr = num_register = -1;
      cptr = strstr(line,"slave=");
      if(cptr!=NULL) sscanf(cptr,"slave=%d",&slave);
      cptr = strstr(line,"function=");
      if(cptr!=NULL) sscanf(cptr,"function=%d",&function);
      cptr = strstr(line,"start_adr=");
      if(cptr!=NULL) sscanf(cptr,"start_adr=%d",&start_adr);
      cptr = strstr(line,"num_register=");
      if(cptr!=NULL) sscanf(cptr,"num_register=%d",&num_register);
      if(slave==-1 || function==-1 || start_adr==-1 || num_register==-1)
      {
        fclose(fp);
        fclose(fout);
        return fail(QCoreApplication::translate("gdaemon", "parameter missing in: %1").arg(line));
      }
      int nbytes = (function==1 || function==2) ? (num_register + 7) / 8 : num_register * 2;
      // Nestor: el offset avanza aunque falle la lectura (antes un error salteaba los ciclos siguientes)
      fprintf(fout,"    modbusCycle(offset,%d,%d,%d,%d,%d,\"%s\");\n",slave,function,start_adr,num_register,
              cycles,(cycles < (int) cycleLabels.size()) ? cycleLabels[cycles].c_str() : "");
      fprintf(fout,"    offset += %d;\n",nbytes);
      cycles++;
    }
  }
  fclose(fp);
  if(!tags->blocks.empty())
  {
    fprintf(fout,"    // tags\n");
    fprintf(fout,"    readTagBlocks();\n");
  }
  (void) cycles;
  // Nestor: una vuelta dura al menos MODBUS_CYCLE_TIME ms (no satura el PLC ni la CPU)
  if(communication == SOCKET || simadr[0] != '\0') fprintf(fout,"    status.endCycle(useSocket ? (sock.isConnected() ? 1 : 0) : -1);\n");
  else                                             fprintf(fout,"    status.endCycle(-1);\n");
  fprintf(fout,"    long long used = msNow() - cycleStart;\n");
  fprintf(fout,"    if(used < MODBUS_CYCLE_TIME) rlsleep((long) (MODBUS_CYCLE_TIME - used));\n");

  fprintf(fout,"  }\n");
  fprintf(fout,"\n");
  fprintf(fout,"  // we will never come here\n");
  if(communication == SERIAL) fprintf(fout,"  serial.closeDevice();\n");
  fprintf(fout,"  return 0;\n");
  fprintf(fout,"}\n");

  fclose(fout);
  return 0;
}

int gmodbus(const char *name)
{
    g_daemon_error.clear();
    f_info = QFileInfo(name);
    // Nestor: copia (antes apuntaba a un QByteArray temporal ya liberado)
    strncpy(target, f_info.fileName().remove(".mkmodbus").toUtf8().constData(), sizeof(target) - 1);
    target[sizeof(target) - 1] = '\0';
    if(init(name) < 0) return -1;
    if(generate(name) < 0) return -1;
    return 0;
}
