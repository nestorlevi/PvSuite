//
// Modbus daemon template                                 (C) R. Lehrig 2003
//
//
// Attention: this program must be run as super user
//

#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include "rlmodbus.h"
#include "rlsharedmemory.h"
#include "rlmailbox.h"
#include "rlthread.h"
#include "rlcutil.h"
#include "rldaemonstatus.h"
#include "PLC1.h"

#define MODBUS_IDLETIME 0 // ms antes de cada pedido (idletime= en el .mkmodbus)
#define MODBUS_CYCLE_TIME 100 // duracion minima de una vuelta en ms (cycletime= en el .mkmodbus)
#define SERIAL_DEVICE   ""
#define BIT_WRITE_FC22  0
rlModbus       modbus(256+1,rlModbus::MODBUS_RTU);
rlSocket       sock("192.168.1.10",502,1);
#define SIM_TCPADR "127.0.0.1" // simulation_tcpadr= (vacio = sin simulacion)
#define SIM_TCPPORT 502
static int     useSocket = 1; // 1 = Modbus TCP (o simulacion)
rlSharedMemory shm("automation/shm/plc1.shm",88);
rlMailbox      mbx("automation/mbx/plc1.mbx");
rlThread       thread;
rlThread       watchdog;
short          lifeCounter     = 0;
short          readErrorCount  = 0;
short          writeErrorCount = 0;

// mensajes en la consola: 0 = solo errores, 1 = resumen cada DAEMON_CONSOLE_INTERVAL s, 2 = cada bloque
// (console= y console_interval= en el archivo del daemon; al ejecutarlo: -q / -n / -v)
#define DAEMON_CONSOLE 1
#define DAEMON_CONSOLE_INTERVAL 10
#define NUM_CYCLES 1 // ciclos "cycle" del archivo
#define NUM_BLOCKS 5 // ciclos + bloques de los tags
static const char *tagBlockLabel[] = {
  "FC3 40300..40301",
  "FC3 40311..40312",
  "FC3 40400..40411",
  "FC1 00006"
};
static const int sysPlcs[] = { 1 }; // PLC1_SYS.Esclavo<n>
rlDaemonStatus status(&shm, 47, NUM_BLOCKS, DAEMON_CONSOLE, DAEMON_CONSOLE_INTERVAL, "esclavo"); // 47 = PLC1_SYS_BASE

// watchdog
static const char *av0 = "";
static int watchcnt1 = 0;
void *watchdogthread(void *arg)
{
  int cnt1 = -1;

  while(1)
  {
    rlsleep(60000);
    if(cnt1 == watchcnt1) break;
    cnt1 = watchcnt1;
  }
  rlsleep(100);
#ifdef unix
  rlexec(av0);
#endif
  exit(0); // pcontrol may start me again if rlexec fails
  return arg;
}

// read mailbox and write to modbus
void *reader(void *arg)
{
  int ret,slave,function,buflen;
  unsigned char buf[256+1];

  mbx.clear(); // clear old messages
  while((buflen = mbx.read(buf,sizeof(buf))) > 0)
  {
    slave        = buf[0];
    function     = buf[1];
    thread.lock();
    rlsleep(MODBUS_IDLETIME);
    if(function == rlModbus::MaskWrite4XRegisters && BIT_WRITE_FC22 == 0 && buflen >= 8)
    { // Nestor: funcion 22 emulada: leer el registro, cambiar los bits y escribirlo
      int adr = buf[2]*256 + buf[3], and_mask = buf[4]*256 + buf[5], or_mask = buf[6]*256 + buf[7];
      int rslave = slave, rfunction = 0, val;
      unsigned char data[256+1];
      ret = modbus.request(slave, rlModbus::ReadHoldingRegisters, adr, 1);
      if(ret >= 0) ret = modbus.response(&rslave, &rfunction, data);
      if(ret == 2)
      {
        val = data[0]*256 + data[1];
        val = (val & and_mask) | (or_mask & ~and_mask & 0x0ffff);
        data[0] = adr/256; data[1] = adr & 0x0ff; data[2] = val/256; data[3] = val & 0x0ff;
        rlsleep(MODBUS_IDLETIME);
        ret = modbus.write(slave, rlModbus::PresetSingleRegister, data, 4);
        ret = modbus.response(&rslave, &rfunction, data);
      }
      else if(ret >= 0) ret = -1;
    }
    else
    {
      ret = modbus.write( slave, function, &buf[2], buflen-2);
      ret = modbus.response( &slave, &function, buf);
    }
    if(useSocket && ret != rlModbus::MODBUS_SUCCESS && ret < 0) sock.disconnect();
    rlsleep(MODBUS_IDLETIME);
    thread.unlock();
    if(ret < 0)
    {
      writeErrorCount++;
      if(writeErrorCount >= 256*128) writeErrorCount = 0;
      shm.write(PLC1_WRITE_ERROR_COUNT_BASE,&writeErrorCount,2);
    }
    { char what[32]; sprintf(what, "FC%d", function); status.write(slave, what, ret); }
  }
  return arg;
}

// read cycle on modbus
// index y what: para los mensajes y los tags de sistema (rldaemonstatus.h)
int modbusCycle(int offset, int slave, int function, int start_adr, int num_register, int index, const char *what)
{
  unsigned char data[256+1];
  int ret, plc = slave, rslave = slave, rfunction = function, expected = -1;
  long long t0;

  watchcnt1++;
  if(watchcnt1 > 10000) watchcnt1 = 0;
  rlsleep(MODBUS_IDLETIME);
  thread.lock();
  t0 = rlDaemonStatus::now();
  ret = modbus.request(slave, function, start_adr, num_register);
  if(ret >= 0) ret = modbus.response(&rslave, &rfunction, data);
  if(function == 1 || function == 2) expected = (num_register + 7) / 8;  // bobinas y entradas: bits
  else if(function == 3 || function == 4) expected = 2 * num_register;   // registros
  if(ret >= 0 && (rslave != slave || rfunction != function || (expected >= 0 && ret != expected)))
  {
    if(DAEMON_CONSOLE > 0) printf("respuesta que no corresponde (%s): esclavo %d funcion %d bytes %d (se esperaba %d %d %d)\n", what ? what : "", rslave, rfunction, ret, slave, function, expected);
    ret = -1;
  }
  if(useSocket && ret < 0) sock.disconnect();
  thread.unlock();
  if(ret > 0) shm.write(offset,data,ret);
  else
  {
    readErrorCount++;
    if(readErrorCount >= 256*128) readErrorCount = 0;
    shm.write(PLC1_READ_ERROR_COUNT_BASE,&readErrorCount,2);
  }
  status.block(index, plc, what, ret, rlDaemonStatus::now() - t0);
  return ret;
}

static long long msNow()
{
  return (long long) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// bloques de lectura de los tags (generados a partir de los "tag" del .mkmodbus)
typedef struct { int slave, function, start, count, scan, offset, status; long long last; } TagBlock;
static TagBlock tagBlock[] = {
// slave function start count scan(ms) offset status
  { 1, 3, 299, 2, 0, 10, 43, -1 },
  { 1, 3, 310, 2, 0, 14, 44, -1 },
  { 1, 3, 399, 12, 0, 18, 45, -1 },
  { 1, 1, 5, 1, 0, 42, 46, -1 }
};
#define NUM_TAG_BLOCKS 4

// lee los bloques que corresponden segun su scan; devuelve cuantos leyo
int readTagBlocks()
{
  int i, ret, n = 0;
  unsigned char ok;
  for(i=0; i<NUM_TAG_BLOCKS; i++)
  {
    TagBlock *b = &tagBlock[i];
    long long now = msNow();
    if(b->scan > 0 && b->last >= 0 && (now - b->last) < b->scan) continue;
    b->last = now;
    ret = modbusCycle(b->offset, b->slave, b->function, b->start, b->count, NUM_CYCLES + i, tagBlockLabel[i]);
    ok = (ret > 0) ? 1 : 0;
    shm.write(b->status, &ok, 1);
    n++;
  }
  return n;
}

int main(int ac, char **av)
{
  int offset,first;
  if(ac > 0) av0 = av[0];
  first = 1;
  int simulation = (getenv("PVB_SIMULATION") != NULL && atoi(getenv("PVB_SIMULATION")) == 1);
  for(int i=1; i<ac; i++) if(strcmp(av[i], "-sim") == 0) simulation = 1;
  if(simulation && SIM_TCPADR[0] == '\0') { printf("-sim: falta simulation_tcpadr= en el archivo del daemon\n"); simulation = 0; }
  if(simulation)
  {
    sock.setAdr(SIM_TCPADR);
    sock.setPort(SIM_TCPPORT);
    useSocket = 1;
    printf("SIMULACION: %s:%d\n", SIM_TCPADR, SIM_TCPPORT);
  }
  printf("\n%s starting\n",av0);
  modbus.registerSocket(&sock);
  thread.create(reader,NULL);
  watchdog.create(watchdogthread,NULL);
  status.setPlcs(sysPlcs, (int) (sizeof(sysPlcs) / sizeof(sysPlcs[0])));
  status.start(ac, av);

  shm.write(PLC1_LIFE_COUNTER_BASE,&lifeCounter,2);
  shm.write(PLC1_READ_ERROR_COUNT_BASE,&readErrorCount,2);
  shm.write(PLC1_WRITE_ERROR_COUNT_BASE,&writeErrorCount,2);
  for(int i=0; i<NUM_TAG_BLOCKS; i++) { unsigned char q = 0; shm.write(tagBlock[i].status, &q, 1); }
  while(1)
  {
    long long cycleStart = msNow();
    status.beginCycle();
    lifeCounter++;
    if(lifeCounter >= 256*128) lifeCounter = 0;
    shm.write(PLC1_LIFE_COUNTER_BASE,&lifeCounter,2);
    offset = 0;
    //    modbusCycle(offset, slave, function, start_adr, num_register);
    modbusCycle(offset,1,3,0,2,0,"FC3 0..1 (cycle1)");
    offset += 4;
    // tags
    readTagBlocks();
    status.endCycle(useSocket ? (sock.isConnected() ? 1 : 0) : -1);
    long long used = msNow() - cycleStart;
    if(used < MODBUS_CYCLE_TIME) rlsleep((long) (MODBUS_CYCLE_TIME - used));
  }

  // we will never come here
  return 0;
}
