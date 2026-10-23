/***************************************************************************
                          rltagdef.h  -  description
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
#ifndef _RL_TAGDEF_H_
#define _RL_TAGDEF_H_

#include <stdio.h>
#include <string>
#include <vector>
#include "rltag.h"

/*! \file rltagdef.h
\brief Definicion de tags (variables con nombre) de los daemons modbus, siemensTCP y OPC UA.

<pre>
Lista de tags de un daemon (modbus, siemensTCP u OPC UA): lectura del .mkmodbus /
.mksiemens / .mkopcua y de archivos .csv, interpretacion de las direcciones,
estructuras y arreglos, agrupacion en bloques de lectura y generacion de los
descriptores rlTag. La usan pvdevelop (para generar el daemon y target.h) y el
daemon OPC UA (al arrancar), asi los dos calculan la misma memoria compartida.

En el archivo .mk:
  address_notation=modicon     # modbus: modicon (40001) | offset (HR0) | plc (%MW0)
                               # siemens: german (E/A) | english (I/Q)
  word_order=CDAB              # modbus: orden por defecto de los valores de 32 bits
  bit_write=rmw                # modbus: rmw (leer-modificar-escribir) | fc22 (Mask Write Register)
  max_gap=8                    # registros (bytes en S7) que se leen de mas para unir bloques
  max_block=125                # tamanio maximo de un bloque (registros, coils o bytes)
  tags=PLC1_tags.csv           # tags en un .csv (relativo al .mk); se pueden combinar con "tag"
  console=normal               # mensajes del daemon: quiet (solo errores) | normal (resumen) |
                               # verbose (cada bloque, con el numero de ciclo); al ejecutarlo: -q / -n / -v
  console_interval=10          # normal: segundos entre resumenes
  tag name=Motor address=40300.15 type=bool rw=1 slave=1 scan=0 desc="Marcha motor"

Arreglos (direcciones consecutivas, o cada "stride" registros / bytes / bits):
  tag name=Temp   address=40100   type=real count=10            # Temp[0..9]
  tag name=Alarma address=40200.0 type=bool count=32            # bits consecutivos
  OPC UA: el NodeId lleva {i}:  address="ns=2;s=Zona{i}.Temp" count=4
  MQTT: el topico lleva {i}:    topic=planta/zona{i}/temp count=4

MQTT (Nestor, .mkmqtt, daemon rlmqtt): la direccion es el topico (address= o topic=) y cada
tag puede tener (ver rlTagList::mqttOptionNames):
  tag name=Temp type=real topic=planta/sensor1/temp format=value qos=1 stale_s=60
  tag name=Valvula type=bool rw=1 topic=planta/valvula/estado cmd_topic=planta/valvula/set format=json json=value
  format=value | json   el mensaje es el valor ("25.3", "1", "true") o un JSON
  json=value            ruta en el JSON (a.b.c); vacio = "value"
  cmd_topic=            topico donde se publican las escrituras (vacio = <topic>/set)
  qos=0|1|2  retain=0|1 (de las escrituras)  stale_s=60 (calidad mala si pasa ese tiempo sin mensaje)
En una estructura, "at" se agrega al topico (por defecto "/<miembro>").

Estructuras (como los UDT de los PLC): "at" es la posicion relativa del miembro
(registros o bytes, con .bit); sin "at" va a continuacion del anterior. En OPC UA
"at" es lo que se agrega al NodeId de la variable (p. ej. ".Velocidad").
  struct name=Motor desc="Motor de bomba"
  member name=Marcha    at=0.0 type=bool rw=1
  member name=Falla     at=0.1 type=bool
  member name=Velocidad at=2   type=real
  member name=Corriente        type=int
  end
  tag name=Bomba1 address=40500 type=Motor
  tag name=Bombas address=40600 type=Motor count=4 stride=10    # 4 motores cada 10 registros

En el pvserver:  PLC1.getReal(PLC1_Bomba1.Velocidad)   PLC1.getBool(PLC1_Bombas[2].Marcha)
                 PLC1.getReal(PLC1_Temp[i])            (PLC1_Temp_COUNT = 10)
Sin prefijo:     using namespace tags_PLC1;  PLC1.getReal(Bomba1.Velocidad)

Columnas del .csv (separador ; o ,):
  name;address;type;count;stride;rw;slave;scan;scale;offset;order;description
  y, para el historiador: log;deadband;log_max;unit;alarm;hyst;alarm_text;group;priority;ack;event
(las estructuras se definen en el .mk; en el .csv "type" puede ser el nombre de una)

Historial y alarmas (los usa el historiador rlhistorian, ver rlhistorian.h):
  tag name=Temp address=30011 type=int scale=0.1 log=1 deadband=0.2 unit=C
      alarm=">80" hyst=2 alarm_text="Temperatura alta" group=Horno priority=1 ack=1
  tag name=Marcha address=40300.0 event=1           # registra cada encendido / apagado
  log=1          registra el valor (tendencias)
  deadband=0.2   cambio minimo para registrar (0 = cualquier cambio)
  log_max=600    s: registra igual si paso ese tiempo (0 = nunca)
  alarm=         >80  >=80  <10  <=10  =1  !=0  1 (bool en 1)  0 (bool en 0)
                 in:10..20 (dentro del rango)  out:10..20 (fuera del rango)
  hyst=          histeresis para volver a la normalidad
  priority=      1 (alta) .. 4 (baja); 0 o sin poner: sin prioridad
  ack=1          la alarma queda en la lista hasta que un usuario la reconoce
  event=1        registra los cambios de un bool como eventos (con hora de encendido y apagado)
En un "member" valen las mismas claves. En una variable de tipo estructura, log, deadband,
log_max, group, priority, ack y event son el valor por defecto de sus miembros.

Tags de sistema: el daemon los actualiza al final de la memoria compartida (TARGET_SYS en TARGET.h):
  Comunicacion          bool   todos los bloques del ultimo ciclo se leyeron bien
  ComunicacionParcial   bool   algun bloque se leyo bien y alguno fallo
  Conectado             bool   la conexion (socket / puerto serie / sesion OPC UA) esta abierta
  BloquesOk, BloquesError   uint   resultado del ultimo ciclo
  Ciclo                 udint  numero de ciclo (sirve para saber si el daemon esta vivo: daemonAlive)
  TiempoCiclo, TiempoCicloMax   udint  ms
  ErroresLectura, ErroresEscritura  udint  contadores desde que arranco
  UltimoError           dint   codigo de retorno del ultimo error (0 = ninguno)
  UltimaLecturaOk, Inicio   udint  hora (segundos desde 1970, UTC)
  Esclavo<n> / Plc<n>   bool   el esclavo Modbus / PLC S7 n respondio en el ultimo ciclo
En el pvserver: PLC1.getBool(PLC1_SYS.Comunicacion), PLC1.daemonAlive(PLC1_SYS.Ciclo)
Historial y alarmas de un tag de sistema (las mismas claves que un tag):
  sys name=Comunicacion alarm=0 alarm_text="Sin comunicacion con PLC1" priority=1 ack=1
</pre> */

/*! Traduccion de los mensajes de error de rlTagList (NULL = en espanol, como estan escritos).
    pvdevelop la conecta con QCoreApplication::translate("rlTagList", texto). */
extern std::string (*rlTagTranslate)(const char *text);

/*! Historial y alarmas de una variable (claves log, deadband, ... del .mk). Los enteros en -1
    y los reales NAN son "sin definir" (en una estructura, los toma de la variable). */
struct rlTagHist
{
  rlTagHist();
  int    log;          //!< 1 = registrar el valor (tendencias)
  double deadband;     //!< cambio minimo para registrar (0 = cualquier cambio)
  int    log_max;      //!< s: registra igual si paso este tiempo sin registrar (0 = nunca)
  std::string unit;
  std::string alarm;   //!< condicion (ver rltagdef.h); vacio = sin alarma
  double hyst;         //!< histeresis de la alarma
  std::string text;    //!< texto de la alarma o del evento (vacio = la descripcion o el nombre)
  std::string group;
  int    priority;     //!< 1 (alta) .. 4 (baja); 0 = sin prioridad
  int    ack;          //!< 1 = la alarma se reconoce
  int    event;        //!< 1 = registra los cambios de un bool como eventos

  bool used() const;                        //!< tiene algo definido
  void inherit(const rlTagHist &from);      //!< completa lo no definido (menos alarm, hyst y text)
  int  set(const std::string &key, const std::string &val); //!< 1 si key era de historial
  std::string pairs() const;                //!< " log=1 deadband=0.2 ..." (lo definido)
};

/*! Condicion de alarma interpretada (rlTagList::parseAlarm) */
struct rlTagAlarmCond
{
  enum Op { OP_NONE = 0, OP_GT, OP_GE, OP_LT, OP_LE, OP_EQ, OP_NE, OP_IN, OP_OUT };
  int    op;
  double v1, v2;
  rlTagAlarmCond() { op = OP_NONE; v1 = v2 = 0; }
  //! true si la alarma esta activa con "value" (wasActive y hyst: histeresis para normalizarse)
  bool active(double value, bool wasActive, double hyst) const;
};

/*! Una variable tal como se escribio (puede ser un arreglo o una estructura) o, en
    rlTagList::flat, cada variable simple que resulta de expandirlas. */
struct rlTagDef
{
  rlTagDef();
  std::string name;
  std::string address;      // como se escribio (modbus/siemens) o NodeId (OPC UA)
  std::string description;
  std::string structName;   // tipo estructura (si no, "type" es un rlTagType)
  int    type;              // rlTagType (0 = segun la direccion)
  int    count;             // > 1: arreglo
  int    stride;            // 0 = consecutivos; si no, registros / bytes (bits en arreglos de bits)
  int    rw;
  int    slave;
  int    scan;              // ms entre lecturas; 0 = en cada ciclo del daemon
  double scale;
  double scale_offset;
  int    order;             // rlTagOrder; -1 = el del archivo
  rlTagHist hist;           // historial y alarmas
  std::vector< std::pair<std::string, std::string> > opts; // Nestor: opciones del protocolo (MQTT: cmd_topic, format...)
  std::string opt(const char *key, const char *def = "") const; //!< valor de una opcion (o def)
  void setOpt(const std::string &key, const std::string &val);
  // de la direccion
  int    area;
  int    dbnum;
  int    addr;
  int    bit;
  // expansion (rlTagList::flat)
  std::string cname;        // "Bomba1.Velocidad", "Temp[3]", "Bombas[2].Marcha"
  int    group;             // indice en rlTagList::tags
  // calculado por layout()
  int    size;
  int    offset;
  int    status;
  int    block;
};

/*! Miembro de una estructura */
struct rlTagMember
{
  rlTagMember();
  std::string name;
  std::string at;           // posicion relativa ("2", "0.3"; vacio = a continuacion) o sufijo del NodeId
  std::string description;
  int    type;
  int    count;             // > 1: arreglo dentro de la estructura
  int    rw;                // -1 = el de la variable
  double scale;
  double scale_offset;
  int    order;
  rlTagHist hist;
  // calculado
  int    pos;              // registros / bytes desde el comienzo
  int    bit;
  int    units;             // registros / bytes / bits que ocupa (todo el arreglo)
};

/*! Estructura (tipo definido por el usuario, como un UDT) */
struct rlTagStruct
{
  std::string name;
  std::string description;
  std::vector<rlTagMember> members;
  int units;                // tamanio (registros / bytes); lo calcula rlTagList
  int shared = 0;           // Nestor: viene del archivo compartido del proyecto (structs=)
};

/*! Bloque de lectura (direcciones contiguas o cercanas que se leen en un pedido) */
struct rlTagBlock
{
  int slave;
  int area;
  int dbnum;
  int start;    // primer registro / coil / byte
  int count;    // cantidad de registros / coils / bytes
  int scan;
  int offset;   // en la memoria compartida
  int nbytes;
  int status;   // byte de calidad en la memoria compartida
};

/*! Lista de tags de un daemon (ver rltagdef.h) */
class rlTagList
{
public:
  enum Protocol { MODBUS = 1, SIEMENS = 2, OPCUA = 3, MQTT = 4 };
  enum Notation { MODICON = 0, OFFSET = 1, PLC = 2, S7_GERMAN = 10, S7_ENGLISH = 11 };
  enum Console  { CONSOLE_QUIET = 0, CONSOLE_NORMAL = 1, CONSOLE_VERBOSE = 2 };

  rlTagList(int protocol);

  //! Nestor: la direccion es un texto (NodeId de OPC UA, topico MQTT): sin bloques ni esclavos
  bool textAddress() const { return protocol == OPCUA || protocol == MQTT; }
  //! Nestor: claves propias de un tag MQTT (cmd_topic, format, json, qos, retain, stale_s)
  static const std::vector<std::string> &mqttOptionNames();

  int protocol;
  int notation;
  int order;          //!< rlTagOrder por defecto
  int max_gap;
  int max_block;
  int bit_write_fc22; //!< modbus: 1 = escribir bits con la funcion 22
  int console;          //!< mensajes del daemon en la consola: CONSOLE_QUIET, CONSOLE_NORMAL o CONSOLE_VERBOSE
  int console_interval; //!< CONSOLE_NORMAL: segundos entre resumenes
  std::string csvfile;
  std::string structsFile;   //!< Nestor: structs= (estructuras compartidas del proyecto)
  std::vector< std::pair<std::string, rlTagHist> > sysHist; //!< "sys name=Comunicacion alarm=0 ..." (historial de los tags de sistema)
  std::vector<rlTagDef>   sys;       //!< tags de sistema TARGET_SYS (los arma layoutSys())
  int sys_base;                      //!< offset del area de sistema en la memoria compartida (-1 = no hay)
  std::vector<rlTagStruct> structs;  //!< estructuras definidas
  std::vector<rlTagDef>   tags;      //!< variables como se escribieron
  std::vector<rlTagDef>   flat;      //!< variables simples (las arma layout())
  std::vector<rlTagBlock> blocks;
  std::vector<std::string> errors;

  //! Lee los ajustes, estructuras y "tag" del .mk y, si hay "tags=", el .csv. Devuelve la cantidad de errores.
  int  readMkFile(const char *filename);
  //! Una linea del .mk: 1 si era de tags (tag, struct, member, end o un ajuste de tags)
  int  parseLine(const char *line, const std::string &where);
  int  parseSetting(const char *line);             //!< 1 si era un ajuste de tags
  int  parseTagLine(const char *line, const std::string &where);
  int  readCsv(const char *filename);
  int  writeCsv(const char *filename) const;
  std::string tagLine(const rlTagDef &t) const;    //!< "tag name=... " para el .mk
  std::vector<std::string> structLines() const;     //!< las estructuras propias para el .mk (sin las compartidas)
  //! Nestor: estructuras compartidas por los daemons del proyecto ("structs=proyecto.mkstruct" en el .mk).
  //! Las lee readMkFile; las propias del .mk tienen prioridad. mkfile: para la ruta relativa. Devuelve los errores
  int  readSharedStructs(const char *mkfile);
  std::vector<std::string> sharedStructLines() const; //!< las compartidas, para el .mkstruct
  static int writeSharedStructs(const char *filename, const std::vector<std::string> &lines);
  const rlTagStruct *findStruct(const std::string &name) const;

  int  parseAddress(const std::string &text, rlTagDef &t, std::string &err) const;
  std::string formatAddress(const rlTagDef &t, int notation) const;
  std::string blockLabel(const rlTagBlock &b) const;  //!< "40001..40010", "DB10.DBB0..DB10.DBB31" (para los mensajes del daemon)
  int  check(rlTagDef &t, std::string &err) const; //!< direccion, tipo, nombre; completa area/addr/size

  //! Expande estructuras y arreglos (flat), agrupa en bloques y asigna su lugar en la memoria
  //! compartida a partir de "base". Devuelve el tamanio total (o -1 si hay errores).
  int  layout(int base);
  //! Agrega el area de los tags de sistema (TARGET_SYS) en "base"; plcs = esclavos Modbus / PLC S7
  //! (un tag Esclavo<n> / Plc<n> por cada uno). Devuelve la nueva base (tamanio de la memoria).
  int  layoutSys(int base, const std::vector<int> &plcs);
  const rlTagDef *findSys(const std::string &name) const;  //!< "Comunicacion", "Ciclo", "Esclavo1"...
  static std::vector<std::string> sysNames();              //!< los tags de sistema fijos (sin Esclavo<n>)
  static std::string sysDescription(const std::string &name); //!< traducida (rlTagTranslate)
  std::string sysLine(const std::string &name, const rlTagHist &h) const; //!< "sys name=... log=1 ..."
  static int         consoleFromString(const std::string &s);  //!< -1 si no se reconoce
  static const char *consoleName(int level);
  //! Descriptores rlTag de target.h (despues de layout)
  void writeHeader(FILE *fout, const char *target) const;
  //! Mapa de la memoria compartida para el historiador (TARGET.tagmap, despues de layout)
  //! mailbox: el del daemon (para escribir desde otros procesos, p. ej. rlautomation); NULL = no se anota
  int  writeTagMap(const char *filename, const char *target, const char *shared_memory, int size, const char *mailbox = NULL) const;

  //! Interpreta una condicion de alarma; -1 si no es valida
  static int parseAlarm(const std::string &cond, rlTagAlarmCond &c);
  //! clave=valor separados por espacios (valores entre comillas; "" = una comilla). err: el error
  static std::vector< std::pair<std::string, std::string> > splitPairs(const std::string &line, std::string &err);

  static int         typeFromString(const std::string &s);
  static const char *typeName(int type);
  static int         orderFromString(const std::string &s);
  static const char *orderName(int order);
  static int         notationFromString(const std::string &s);
  static const char *notationName(int notation);
  static bool        validName(const std::string &s);
  //! Nestor: el nombre es una macro de los encabezados del sistema o de rllib (B9600, CS8, ERROR...):
  //! como tag, miembro o estructura el codigo generado no compilaria
  static bool        reservedName(const std::string &s);
  static std::vector<std::string> splitCsvLine(const std::string &line, char sep);

private:
  void writeSysHeader(FILE *fout, const char *target, bool include) const;
  int  parseModbusAddress(const std::string &text, rlTagDef &t, std::string &err) const;
  int  parseSiemensAddress(const std::string &text, rlTagDef &t, std::string &err) const;
  int  valueSize(const rlTagDef &t) const;
  int  units(const rlTagDef &t) const;             // registros / bytes que ocupa un valor simple
  int  resolveStruct(rlTagStruct &s, int area, std::string &err) const;
  int  expand();
  std::vector< std::pair<std::string, std::string> > parsePairs(const std::string &line, const std::string &where);
  int  currentStruct;                               // estructura que se esta leyendo (-1 = ninguna)
};

#endif
