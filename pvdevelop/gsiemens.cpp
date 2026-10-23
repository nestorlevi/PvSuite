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
// mksiemens name.mksiemens  (C) R. Lehrig 2004
/*
    S7_200 update        : Wed Mar 21 2007
    copyright            : (C) 2007 by Aljosa Merljak
    Email                : aljosa.merljak@datapan.si
*/

// Nestor: ademas de los "cycle" de siempre, acepta "tag" (variables con nombre, notacion S7:
// DB10.DBX4.3, MW10, E0.1/I0.1, ...) en el .mksiemens y/o en un .csv (tags=archivo.csv).
// Sus bloques de lectura van despues de los ciclos de siempre.

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

extern QString g_daemon_error; // gmodbus.cpp
std::string gdaemonCString(const char *s); // gmodbus.cpp
void gdaemonNormPath(char *path);           // gmodbus.cpp: "a\\b" -> "a\b"
void gdaemonWriteStatus(FILE *fout, const rlTagList *list, const char *target, const std::vector<std::string> &cycleLabels, const char *plcWord);
static rlTagList *tags = NULL;
static int fail(const QString &text)
{
  g_daemon_error = text;
  printf("%s\n", text.toUtf8().constData());
  return -1;
}

static char line[256*256];
static char target[4096];
static char shared_memory[4096];
static char mailbox[4096];
static int  idletime;
static int  shared_memory_size;
static char eventhost[4096];
static int  eventport;
static std::vector<int> plcs;                 // Nestor: PLC leidos (tags de sistema Plc<n>)
static std::vector<std::string> cycleLabels;  // Nestor: "ORG_DB DB1 0..31 (cycle1)" para los mensajes del daemon

static int init(const char *name)
{
  int base,dbnum,start,len,cyclecnt;
  char *cptr;
  FILE *fp,*fout;
  fp = fopen(nativeName(name),"r");
  if(fp == NULL)
  {
    printf("could not open %s\n",name);
    return fail(QCoreApplication::translate("gdaemon", "Error en el .mksiemens: ver los mensajes de la consola"));
  }

  strcpy(target,"siemensdaemon");
  idletime = 100;
  eventhost[0] = '\0';
  eventport = -1;
  while(fgets(line,sizeof(line)-1,fp) != NULL)
  {
    if(strncmp("target=",line,7) == 0)
    {
      sscanf(line,"target=%1023s",target);
    }
    else if(strncmp("idletime=",line,9) == 0)
    {
      sscanf(line,"idletime=%d",&idletime);
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
    else if(strncmp("slave=",line,6) == 0)
    {    
      printf("%s",line);
    }  
  }

  fclose(fp);

  printf("target=%s\n",target);
  printf("shared_memory=%s\n",shared_memory);
  printf("mailbox=%s\n",mailbox);
  if(eventport != -1) printf("eventlog host=%s port=%d\n",eventhost,eventport);

  fp = fopen(nativeName(name),"r");
  if(fp == NULL)
  {
    printf("could not open %s\n",name);
    return fail(QCoreApplication::translate("gdaemon", "Error en el .mksiemens: ver los mensajes de la consola"));
  }

  sprintf(line,"%s.h",target);
  fout = fopen(line,"w");
  if(fout == NULL)
  {
    printf("could not write %s\n",line);
    fclose(fp);
    return fail(QCoreApplication::translate("gdaemon", "Error en el .mksiemens: ver los mensajes de la consola"));
  }
  fprintf(fout,"// Siemens Signal assignment of %s\n",target);
  fprintf(fout,"#define %s_SHARED_MEMORY \"%s\"\n",target,gdaemonCString(shared_memory).c_str());
  fprintf(fout,"#define %s_MAILBOX \"%s\"\n",target,gdaemonCString(mailbox).c_str());

  cyclecnt = 1;
  base = 0;
  plcs.clear();
  cycleLabels.clear();
  while(fgets(line,sizeof(line)-1,fp) != NULL)
  {
    if(strncmp("cycle",line,5) == 0)
    {
      int plc = 0;
      char org[128] = "";
      cptr = strstr(line,"slave=");
      if(cptr!=NULL) sscanf(cptr,"slave=%d",&plc);
      cptr = strstr(line,"org=");
      if(cptr!=NULL) sscanf(cptr,"org=%100s",org);
      dbnum = start = len = -1;
      cptr = strstr(line,"dbnum=");
      if(cptr!=NULL) sscanf(cptr,"dbnum=%d",&dbnum);
      cptr = strstr(line,"start=");
      if(cptr!=NULL) sscanf(cptr,"start=%d",&start);
      cptr = strstr(line,"len=");
      if(cptr!=NULL) sscanf(cptr,"len=%d",&len);
      if(dbnum==-1 || start==-1 || len==-1)
      {
        printf("parameter missing in: %s\n",line);
        fclose(fp);
        return fail(QCoreApplication::translate("gdaemon", "Error en el .mksiemens: ver los mensajes de la consola"));
      }
      fprintf(fout,"\n#define %s_CYCLE%d_BASE %d\n",target,cyclecnt,base);
      fprintf(fout,"#define %s_CYCLE%d_NUM_BYTES %d\n",target,cyclecnt,len);
      plcs.push_back(plc);
      cycleLabels.push_back(std::string(org) + (strcmp(org, "ORG_DB") == 0 ? " DB" + std::to_string(dbnum) : std::string()) +
                            " " + std::to_string(start) + ".." + std::to_string(start + len - 1) + " (cycle" + std::to_string(cyclecnt) + ")");
      base += len;
      cyclecnt++;
    }
  }
  // Nestor: los bloques de los tags van despues de los ciclos de siempre
  delete tags;
  tags = new rlTagList(rlTagList::SIEMENS);
  tags->readMkFile(nativeName(name));
  int newbase = tags->errors.empty() ? tags->layout(base) : -1;
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
  shared_memory_size = base;
  tags->writeHeader(fout, target);
  // Nestor: mapa de la memoria compartida para el historiador (TARGET.tagmap)
  sprintf(line,"%s.tagmap",target);
  tags->writeTagMap(line, target, shared_memory, base, mailbox);

  fclose(fp);
  fclose(fout);
  return 0;
}

static int generate(const char *name)
{
  FILE *fout,*fp;
  char *cptr,adr[256],plc_type[256],org[256];
  int  slave,dbnum,start,len,fetch_write,function,rack_slot;
  int  cycle = 0; // Nestor: indice del ciclo (mensajes del daemon)

  sprintf(line,"%s.cpp",target);
  fout = fopen(line,"w");
  if(fout == NULL)
  {
    printf("could not write %s\n",line);
    return fail(QCoreApplication::translate("gdaemon", "Error en el .mksiemens: ver los mensajes de la consola"));
  }

  fprintf(fout,"%s","//\n");
  fprintf(fout,"%s","// SiemensTCP daemon template                                 (C) R. Lehrig 2004\n");
  fprintf(fout,"%s","//\n");
  fprintf(fout,"%s","//\n");
  fprintf(fout,"%s","// Attention: this program must be run as super user\n");
  fprintf(fout,"%s","//\n");
  fprintf(fout,"%s","\n");
  fprintf(fout,"%s","#include <stdio.h>\n");
  fprintf(fout,"%s","#include <stdlib.h>\n");
  if(!tags->blocks.empty()) fprintf(fout,"%s","#include <chrono>\n");
  fprintf(fout,"%s","#include <string.h>\n");
  fprintf(fout,"%s","#include \"rlsiemenstcp.h\"\n");
  fprintf(fout,"%s","#include \"rlsharedmemory.h\"\n");
  fprintf(fout,"%s","#include \"rlmailbox.h\"\n");
  fprintf(fout,"%s","#include \"rlthread.h\"\n");
  fprintf(fout,"%s","#include \"rlcutil.h\"\n");
  fprintf(fout,"%s","#include \"rldaemonstatus.h\"\n");
  if(eventport != -1)
  {
    fprintf(fout,"%s","#include \"rlevent.h\"\n");
  }
  fprintf(fout,"%s","\n");
  fprintf(fout,"#define %s_CYCLE_TIME %d\n",target,idletime);
  fprintf(fout,"%s","\n");

  // write slaves
  fp = fopen(nativeName(name),"r");
  if(fp == NULL)
  {
    printf("could not open %s\n",name);
    fclose(fout);
    return fail(QCoreApplication::translate("gdaemon", "Error en el .mksiemens: ver los mensajes de la consola"));
  }
  while(fgets(line,sizeof(line)-1,fp) != NULL)
  {
    if(strncmp("slave=",line,6) == 0)
    {
      slave = -1;
      fetch_write = 1;
      function = rack_slot = -1;
      adr[0] = plc_type[0] = '\0';
      sscanf(line,"slave=%d",&slave);
      cptr = strstr(line,"adr=");
      if(cptr!=NULL) strncpy(adr,&cptr[4],100);
      cptr = strstr(adr," ");
      if(cptr!=NULL) *cptr = '\0';
      cptr = strstr(line,"type=");
      if(cptr!=NULL) strncpy(plc_type,&cptr[5],100);
      cptr = strstr(plc_type," ");
      if(cptr!=NULL) *cptr = '\0';
      cptr = strstr(plc_type,"\n");
      if(cptr!=NULL) *cptr = '\0';
      cptr = strstr(line,"fetch_write=");
      if(cptr!=NULL) sscanf(cptr,"fetch_write=%d",&fetch_write);
      cptr = strstr(line,"function=");
      if(cptr!=NULL) sscanf(cptr,"function=%d",&function);
      cptr = strstr(line,"rack_slot=");
      if(cptr!=NULL) sscanf(cptr,"rack_slot=%d",&rack_slot);
      
      if(adr[0] == '\0' || plc_type[0] == '\0' || slave < 0)
      {
        printf("parameter missing in: %s\n",line);
        fclose(fp);
        fclose(fout);
        return fail(QCoreApplication::translate("gdaemon", "Error en el .mksiemens: ver los mensajes de la consola"));
      }
      fprintf(fout,"rlSiemensTCP   slave%d(\"%s\",rlSiemensTCP::%s,%d,%d,%d);\n",slave,adr,plc_type,fetch_write,function,rack_slot);
    }
  }
  fclose(fp);    
  
  fprintf(fout,"rlSharedMemory shm(\"%s\",%d);\n",gdaemonCString(shared_memory).c_str(),shared_memory_size);
  fprintf(fout,"rlMailbox      mbx(\"%s\");\n",gdaemonCString(mailbox).c_str());
  fprintf(fout,"%s","rlThread       thread;\n");
  fprintf(fout,"%s","rlSiemensTCP   *slave_array[256];\n");
  gdaemonWriteStatus(fout, tags, target, cycleLabels, "plc");
  fprintf(fout,"%s","\n");
  fprintf(fout,"%s","// read mailbox and write to siemensTCP\n");
  fprintf(fout,"%s","void *reader(void *arg)\n");
  fprintf(fout,"%s","{\n");
  fprintf(fout,"%s","  int buflen,slave,org,dbnr,start_adr,len,function,ret;\n");
  fprintf(fout,"%s","  char what[64];\n");
  fprintf(fout,"%s","  unsigned char buf[2048+8];\n");
  fprintf(fout,"%s","\n");
  fprintf(fout,"%s","  mbx.clear(); // clear old messages\n");
  fprintf(fout,"%s","  while((buflen = mbx.read(buf,sizeof(buf))) > 0)\n");
  fprintf(fout,"%s","  {\n");
  fprintf(fout,"%s","    slave     = buf[0];\n");
  fprintf(fout,"%s","    org       = buf[1];\n");
  fprintf(fout,"%s","    dbnr      = buf[2]*256 + buf[3];\n");
  fprintf(fout,"%s","    start_adr = buf[4]*256 + buf[5];\n");
  fprintf(fout,"%s","    len       = buf[6]*256 + buf[7];\n");
  fprintf(fout,"%s","    function  = buf[8];\n");
  fprintf(fout,"%s","    if(slave >= 0 && slave < 256 && slave_array[slave] != NULL)\n");
  fprintf(fout,"%s","    {\n");
  fprintf(fout,"%s","      thread.lock();\n");
  fprintf(fout,"%s","      ret = slave_array[slave]->write(org,dbnr,start_adr,len,&buf[9],function);\n");
  fprintf(fout,"%s","      thread.unlock();\n");
  fprintf(fout,"%s","      sprintf(what, \"org=%d db=%d %d..%d\", org, dbnr, start_adr, start_adr + len - 1);\n");
  fprintf(fout,"%s","      status.write(slave, what, ret);\n");
  fprintf(fout,"%s","    }\n");
  if(eventport != -1)
  {
    fprintf(fout,"%s","    else { rlEvent(rlError,\"wrong param send to mailbox\"); }\n");
  }  
  fprintf(fout,"%s","  }\n");
  fprintf(fout,"%s","  return arg;\n");
  fprintf(fout,"%s","}\n");
  fprintf(fout,"%s","\n");
  fprintf(fout,"%s","// read cycle on SiemensTCP\n");
  fprintf(fout,"%s","// index y what: para los mensajes y los tags de sistema (rldaemonstatus.h)\n");
  fprintf(fout,"%s","int siemensTCPCycle(int slave, int offset, int org, int dbnr, int start_adr, int len, int index, const char *what)\n");
  fprintf(fout,"%s","{\n");
  fprintf(fout,"%s","  unsigned char data[4096];\n");
  fprintf(fout,"%s","  int ret;\n");
  fprintf(fout,"%s","  long long t0;\n");
  fprintf(fout,"%s","\n");
  fprintf(fout,"%s","  if(slave_array[slave] == NULL) return len;\n");
  fprintf(fout,"%s","  thread.lock();\n");
  fprintf(fout,"%s","  t0 = rlDaemonStatus::now();\n");
  fprintf(fout,"%s","  ret = slave_array[slave]->fetch(org,dbnr,start_adr,len,data);\n");
  fprintf(fout,"%s","  thread.unlock();\n");
  if(eventport != -1)
  {
    fprintf(fout,"%s","  if(ret < 0) { rlEvent(rlWarning,\"fetch ret=%d slave=%d\",ret,slave); rlsleep(5000); }\n");
  }  
  fprintf(fout,"%s","  if(ret > 0) shm.write(offset,data,ret);\n");
  fprintf(fout,"%s","  status.block(index, slave, what, ret, rlDaemonStatus::now() - t0);\n");
  fprintf(fout,"%s","  return len;\n");
  fprintf(fout,"%s","}\n");
  fprintf(fout,"%s","\n");
  if(!tags->blocks.empty())
  {
    // Nestor: bloques de los tags (bytes contiguos o cercanos leidos de una vez)
    fprintf(fout,"// bloques de lectura de los tags (generados a partir de los \"tag\" del .mksiemens)\n");
    fprintf(fout,"typedef struct { int slave, org, dbnum, start, len, scan, offset, status; long long last; } TagBlock;\n");
    fprintf(fout,"static TagBlock tagBlock[] = {\n");
    fprintf(fout,"// plc org dbnum start len scan(ms) offset status\n");
    for(size_t b=0; b<tags->blocks.size(); b++)
    {
      const rlTagBlock &bl = tags->blocks[b];
      fprintf(fout,"  { %d, %d, %d, %d, %d, %d, %d, %d, -1 }%s\n", bl.slave, bl.area, bl.dbnum, bl.start, bl.count,
              bl.scan, bl.offset, bl.status, (b + 1 < tags->blocks.size()) ? "," : "");
    }
    fprintf(fout,"};\n");
    fprintf(fout,"#define NUM_TAG_BLOCKS %d\n", (int) tags->blocks.size());
    fprintf(fout,"\n");
    fprintf(fout,"static long long msNow()\n");
    fprintf(fout,"{\n");
    fprintf(fout,"  return (long long) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();\n");
    fprintf(fout,"}\n");
    fprintf(fout,"\n");
    fprintf(fout,"void readTagBlocks()\n");
    fprintf(fout,"{\n");
    fprintf(fout,"  unsigned char data[4096], ok;\n");
    fprintf(fout,"  int i, ret;\n");
    fprintf(fout,"  for(i=0; i<NUM_TAG_BLOCKS; i++)\n");
    fprintf(fout,"  {\n");
    fprintf(fout,"    TagBlock *b = &tagBlock[i];\n");
    fprintf(fout,"    long long now = msNow();\n");
    fprintf(fout,"    if(b->scan > 0 && b->last >= 0 && (now - b->last) < b->scan) continue;\n");
    fprintf(fout,"    b->last = now;\n");
    fprintf(fout,"    ret = -1;\n");
    fprintf(fout,"    if(b->slave >= 0 && b->slave < 256 && slave_array[b->slave] != NULL)\n");
    fprintf(fout,"    {\n");
    fprintf(fout,"      thread.lock();\n");
    fprintf(fout,"      ret = slave_array[b->slave]->fetch(b->org, b->dbnum, b->start, b->len, data);\n");
    fprintf(fout,"      thread.unlock();\n");
    fprintf(fout,"    }\n");
    fprintf(fout,"    status.block(NUM_CYCLES + i, b->slave, tagBlockLabel[i], ret, msNow() - now);\n");
    fprintf(fout,"    if(ret > 0) shm.write(b->offset, data, ret);\n");
    fprintf(fout,"    ok = (ret > 0) ? 1 : 0;\n");
    fprintf(fout,"    shm.write(b->status, &ok, 1);\n");
    fprintf(fout,"  }\n");
    fprintf(fout,"}\n");
    fprintf(fout,"\n");
  }
  fprintf(fout,"%s","int main(int ac, char **av)\n");
  fprintf(fout,"%s","{\n");
  fprintf(fout,"%s","  int offset,ret,i;\n");
  if(eventport != -1)
  {
    fprintf(fout,"  char *argv[] = {\"\",\"-eventhost=%s\",\"-eventport=%d\"};\n",eventhost,eventport);
    fprintf(fout,"\n");
    fprintf(fout,"  rlEventInit(3,argv,\"%s:\");\n",target);
    fprintf(fout,"  rlEvent(rlInfo,\"%s starting\");\n",target); 
  }    
  
  // write slaves array
  fprintf(fout,"%s","  for(i=0; i<256; i++) slave_array[i] = NULL;\n");
  fp = fopen(nativeName(name),"r");
  if(fp == NULL)
  {
    printf("could not open %s\n",name);
    fclose(fout);
    return fail(QCoreApplication::translate("gdaemon", "Error en el .mksiemens: ver los mensajes de la consola"));
  }
  while(fgets(line,sizeof(line)-1,fp) != NULL)
  {
    if(strncmp("slave=",line,6) == 0)
    {
      slave = -1;
      adr[0] = plc_type[0] = '\0';
      sscanf(line,"slave=%d",&slave);      
      if(slave < 0)
      {
        printf("parameter missing in: %s\n",line);
        fclose(fp);
        fclose(fout);
        return fail(QCoreApplication::translate("gdaemon", "Error en el .mksiemens: ver los mensajes de la consola"));
      }
      fprintf(fout,"  slave_array[%d] = &slave%d;\n",slave,slave);
    }
  }
  fclose(fp);    
  
  fprintf(fout,"%s","\n");  
  fprintf(fout,"%s","  //rlSetDebugPrintf(1); // set debugging on\n");
  fprintf(fout,"  thread.create(reader,NULL);\n");
  bool hasPlcs = false;
  for(size_t i=0; i<tags->sys.size(); i++) if(tags->sys[i].name.rfind("Plc", 0) == 0) hasPlcs = true;
  if(hasPlcs) fprintf(fout,"  status.setPlcs(sysPlcs, (int) (sizeof(sysPlcs) / sizeof(sysPlcs[0])));\n");
  fprintf(fout,"  status.start(ac, av);\n");
  fprintf(fout,"  while(1)\n");
  fprintf(fout,"  {\n");
  fprintf(fout,"    rlsleep(%s_CYCLE_TIME);\n",target);
  fprintf(fout,"    status.beginCycle();\n");
  fprintf(fout,"    offset = 0;\n");
  fprintf(fout,"    // SiemensTCPCycle(int slave, int offset, int org, int dbnr, int start_adr, int len);\n");
  
  // write cycles
  fp = fopen(nativeName(name),"r");
  if(fp == NULL)
  {
    printf("could not open %s\n",name);
    fclose(fout);
    return fail(QCoreApplication::translate("gdaemon", "Error en el .mksiemens: ver los mensajes de la consola"));
  }
  while(fgets(line,sizeof(line)-1,fp) != NULL)
  {
    if(strncmp("cycle",line,5) == 0)
    {
      org[0] = '\0';
      slave = dbnum = start = len = -1;
      cptr = strstr(line,"slave=");
      if(cptr!=NULL) sscanf(cptr,"slave=%d",&slave);
      cptr = strstr(line,"dbnum=");
      if(cptr!=NULL) sscanf(cptr,"dbnum=%d",&dbnum);
      cptr = strstr(line,"start=");
      if(cptr!=NULL) sscanf(cptr,"start=%d",&start);
      cptr = strstr(line,"len=");
      if(cptr!=NULL) sscanf(cptr,"len=%d",&len);
      cptr = strstr(line,"org=");
      if(cptr!=NULL) strncpy(org,&cptr[4],100);
      cptr = strstr(org," ");
      if(cptr!=NULL) *cptr = '\0';
      
      if(slave==-1 || org[0]=='\0' || dbnum==-1 || start==-1 || len==-1)
      {
        printf("parameter missing in: %s\n",line);
        fclose(fp);
        fclose(fout);
        return fail(QCoreApplication::translate("gdaemon", "Error en el .mksiemens: ver los mensajes de la consola"));
      }
      fprintf(fout,"    ret = siemensTCPCycle(%d,offset, rlSiemensTCP::%s, %d, %d, %d, %d, \"%s\");\n",slave,org,dbnum,start,len,
              cycle, (cycle < (int) cycleLabels.size()) ? cycleLabels[cycle].c_str() : "");
      fprintf(fout,"    if(ret>0) offset += ret;\n");
      cycle++;
    }
  }
  fclose(fp);
  if(!tags->blocks.empty()) fprintf(fout,"    readTagBlocks(); // tags\n");
  fprintf(fout,"    status.endCycle();\n");

  fprintf(fout,"  }\n");
  fprintf(fout,"\n");
  fprintf(fout,"  return 0;\n");
  fprintf(fout,"}\n");

  fclose(fout);
  return 0;
}

int gsiemens(const char *name)
{
  g_daemon_error.clear();
  strcpy(target,"siemensdaemon");
  if(init(name) < 0) return -1;
  if(generate(name) < 0) return -1;
  return 0;
}
