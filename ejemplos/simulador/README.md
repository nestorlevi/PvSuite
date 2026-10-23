# Simulador Modbus TCP

Un esclavo Modbus TCP mínimo para probar los ejemplos sin PLC (no es para producción).
Atiende las funciones 1, 2, 3, 4, 5, 6, 15 y 16 en `127.0.0.1:502` y mueve algunos valores del
ejemplo `01_modbus` para ver las animaciones.

## Compilar

Con qmake (por ejemplo, la consola de Qt):

```sh
qmake simulador_modbus.pro
make            # Windows con MinGW: mingw32-make
```

O directamente con g++:

```sh
g++ simulador_modbus.cpp -o simulador_modbus -lws2_32     # Windows (MinGW)
g++ simulador_modbus.cpp -o simulador_modbus -lpthread    # Linux / macOS
```

## Usar

```sh
simulador_modbus          # puerto 502
simulador_modbus 5502     # otro puerto (cambiar simulation_tcpport= en el .mkmodbus)
```

En Linux, el puerto 502 necesita permisos de administrador: usar otro puerto (por ejemplo 5502) y
cambiar `simulation_tcpport=` en `PLC1.mkmodbus`.

El daemon del ejemplo se conecta al simulador cuando arranca con `-sim`
(`simulation_tcpadr=` / `simulation_tcpport=` del `.mkmodbus`).
