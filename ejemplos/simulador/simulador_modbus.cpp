/****************************************************************************
** Simulador Modbus TCP para probar los ejemplos sin PLC (no es para produccion).
**
** Escucha en 127.0.0.1:502 (u otro puerto: simulador_modbus 5502) y atiende las funciones
** 1, 2, 3, 4, 5, 6, 15 y 16. Ademas mueve algunos valores del ejemplo 01_modbus para ver
** las animaciones (direcciones Modicon, 40001 = registro 0):
**   Caudal  (40311, real CDAB)  sube y baja entre 0 y 90 m3/h mientras la Bomba (00006) esta en 1
**   FlagTWR (40300.15)          se prende y apaga cada 6 s (anima la imagen de la bomba)
**   Motores[i].Marcha (40400.0, 40410.0) en 1 mientras Motores[i].Velocidad (40401, 40411) > 0
** Lo que escribe el daemon (botones, campos, slider) queda escrito.
**
** Compilar:  Windows (MinGW): g++ simulador_modbus.cpp -o simulador_modbus -lws2_32
**            Linux / macOS:   g++ simulador_modbus.cpp -o simulador_modbus -lpthread
**            o con qmake:     qmake simulador_modbus.pro && make
****************************************************************************/
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
typedef int SOCKET;
#define closesocket close
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static unsigned short hr[1000], ir[1000];
static unsigned char  co[1000], di[1000];

#ifdef _WIN32
static CRITICAL_SECTION mtx;
static void lock()   { EnterCriticalSection(&mtx); }
static void unlock() { LeaveCriticalSection(&mtx); }
static void msleep(int ms) { Sleep(ms); }
#else
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static void lock()   { pthread_mutex_lock(&mtx); }
static void unlock() { pthread_mutex_unlock(&mtx); }
static void msleep(int ms) { usleep(ms * 1000); }
#endif

// real en dos registros, palabra baja primero (word_order=CDAB del ejemplo)
static void setReal(int reg, float v)
{
  unsigned int u;
  memcpy(&u, &v, 4);
  hr[reg]     = (unsigned short) (u & 0xffff);
  hr[reg + 1] = (unsigned short) (u >> 16);
}

static void setBit(int reg, int bit, int on)
{
  if(on) hr[reg] |=  (unsigned short) (1 << bit);
  else   hr[reg] &= (unsigned short) ~(1 << bit);
}

// los valores que se mueven solos (cada 200 ms)
static void simulate(double t)
{
  static double caudal = 0;
  lock();
  double objetivo = co[5] ? 45 + 45 * sin(t / 4) : 0;       // Bomba (00006)
  caudal += (objetivo - caudal) * 0.15;
  setReal(310, (float) caudal);                              // Caudal (40311)
  setBit(299, 15, ((int) (t / 6)) % 2);                      // FlagTWR (40300.15)
  for(int m = 0; m < 2; m++)                                 // Motores[m] (40400 + 10*m)
    setBit(399 + 10 * m, 0, hr[400 + 10 * m] > 0);
  unlock();
}

#ifdef _WIN32
static DWORD WINAPI simThread(LPVOID)
#else
static void *simThread(void *)
#endif
{
  for(double t = 0; ; t += 0.2)
  {
    simulate(t);
    msleep(200);
  }
  return 0;
}

static int handle(unsigned char *q, unsigned char *r)
{
  int fc = q[7], a = q[8]*256 + q[9], n = q[10]*256 + q[11], i;
  memcpy(r, q, 8);
  int pdu = 0;
  if((fc <= 4 || fc == 15 || fc == 16) && (a + n > 1000 || n > 120)) fc = 0x7f;   // fuera de rango
  if((fc == 5 || fc == 6) && a >= 1000) fc = 0x7f;
  lock();
  switch(fc)
  {
    case 1: case 2:
    {
      unsigned char *bits = (fc == 1) ? co : di;
      int bytes = (n + 7) / 8;
      r[8] = bytes;
      memset(&r[9], 0, bytes);
      for(i=0; i<n; i++) if(bits[a+i]) r[9 + i/8] |= (1 << (i%8));
      pdu = 2 + bytes;
      break;
    }
    case 3: case 4:
    {
      unsigned short *regs = (fc == 3) ? hr : ir;
      r[8] = n*2;
      for(i=0; i<n; i++) { r[9+2*i] = regs[a+i] >> 8; r[10+2*i] = regs[a+i] & 0xff; }
      pdu = 2 + n*2;
      break;
    }
    case 5:  co[a] = (q[10] == 0xff) ? 1 : 0; memcpy(&r[8], &q[8], 4); pdu = 5; break;
    case 6:  hr[a] = q[10]*256 + q[11];       memcpy(&r[8], &q[8], 4); pdu = 5; break;
    case 15: for(i=0; i<n; i++) co[a+i] = (q[13 + i/8] >> (i%8)) & 1; memcpy(&r[8], &q[8], 4); pdu = 5; break;
    case 16: for(i=0; i<n; i++) hr[a+i] = q[13+2*i]*256 + q[14+2*i];  memcpy(&r[8], &q[8], 4); pdu = 5; break;
    default: r[7] = q[7] | 0x80; r[8] = (fc == 0x7f) ? 2 : 1; pdu = 2; break;   // 2 = direccion ilegal, 1 = funcion ilegal
  }
  unlock();
  r[4] = 0; r[5] = pdu + 1;
  return 6 + 1 + pdu;
}

int main(int argc, char **argv)
{
#ifdef _WIN32
  WSADATA w; WSAStartup(MAKEWORD(2,2), &w);
  InitializeCriticalSection(&mtx);
  CreateThread(NULL, 0, simThread, NULL, 0, NULL);
#else
  pthread_t th;
  pthread_create(&th, NULL, simThread, NULL);
#endif
  hr[300] = 40;    // Brillo (40301)
  hr[400] = 30;    // Motores[0].Velocidad (40401)
  int port = argc > 1 ? atoi(argv[1]) : 502;
  SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in sa; memset(&sa, 0, sizeof(sa)); sa.sin_family = AF_INET; sa.sin_port = htons(port); sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  int one = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(one));
  if(bind(s, (sockaddr *) &sa, sizeof(sa)) != 0) { printf("No se pudo usar el puerto %d (otro programa lo usa?)\n", port); return 1; }
  listen(s, 1);
  printf("Simulador Modbus en 127.0.0.1:%d (Ctrl+C para terminar)\n", port); fflush(stdout);
  while(1)
  {
    SOCKET c = accept(s, NULL, NULL);
    printf("daemon conectado\n"); fflush(stdout);
    unsigned char q[300], r[300];
    while(1)
    {
      int n = recv(c, (char *) q, 6, MSG_WAITALL);
      if(n != 6) break;
      int rest = q[4]*256 + q[5];
      if(rest < 2 || rest > 260 || recv(c, (char *) &q[6], rest, MSG_WAITALL) != rest) break;
      int rl = handle(q, r);
      send(c, (char *) r, rl, 0);
    }
    closesocket(c);
    printf("daemon desconectado\n"); fflush(stdout);
  }
}
