# Ejemplo 01: tags Modbus, acciones y animaciones

Un daemon Modbus TCP (`PLC1`) lee y escribe un PLC y deja los valores en memoria compartida. La pantalla
`mask1` los muestra y los modifica con el botón **Tags...** de Propiedades: no hay código escrito a mano.
Sin PLC se prueba con el simulador de `../simulador`.

## Qué muestra

| Función | Dónde verla |
|---|---|
| Tags con nombre: bit de un registro, entero, real, bobina y una estructura `Motor` repetida | Daemon > Modbus > abrir `PLC1.mkmodbus` > **Tags...** |
| Daemon y `.tagmap` generados por pvdevelop, con simulación (`-sim`) | `PLC1.cpp`, `PLC1.h`, `PLC1.tagmap` |
| Acciones y animaciones sin código | Designer > doble clic en un widget > Propiedades > **Tags...** |
| Valores en vivo, escribir y forzar | Daemon > **Monitor de tags** |

## Los widgets de la pantalla

| Widget | Tags... | Qué hace |
|---|---|---|
| `btnBomba` | press escribe `Bomba=1`, release escribe `Bomba=0`; color según `Bomba` | Pulsador momentáneo: gris apagada, verde prendida. |
| `btnMas` / `btnMenos` | click suma / resta 5 a `Motores[0].Velocidad` (máx. 100, mín. 0) | Ajusta la velocidad del motor 1. |
| `lblEstado` | texto de `Motores[0].Marcha` con `Parado\|Marcha`; color | "Marcha" en verde cuando el motor anda. |
| `lblCaudal` | texto de `Caudal` con `%.1f m3/h`; texto rojo si pasa de 80 | Caudal con un decimal. |
| `edtBrillo` | ingresar `Brillo` (0 a 100) | Escribir un número y Enter. |
| `sldVel` | ingresar y mostrar `Motores[1].Velocidad` | Slider en los dos sentidos. |
| `barBrillo` | valor de `Brillo` | Barra de progreso. |
| `chkBomba` | ingresar y mostrar `Bomba` | Casilla en los dos sentidos. |
| `lcdCaudal` | valor de `Caudal` | Display LCD. |
| `lblCiclo` | texto de `SYS.Ciclo` | Avanza mientras el daemon se comunica. |
| `imgBomba` | imagen según `Bomba` (`off.png` / `on.png`); anima `f1..f3.png` cada 300 ms mientras `FlagTWR` está en 1 | Imagen y animación (la hace pvbrowser). |

Si el daemon se detiene, los valores se marcan en gris (dato malo).

Las acciones quedan en `mask1_pvb.h`, que pvdevelop regenera al guardar la pantalla. En cada slot de
`mask1_slots.h` hay una línea `pvbActions(...)`: el código propio puede ir antes o después.

## Probarlo sin PLC

1. Compilar y arrancar el simulador (ver `../simulador`); escucha en `127.0.0.1:502`.
2. Abrir el proyecto en pvdevelop (`pvs.pvproject`).
3. Daemon > Modbus > `PLC1.mkmodbus` > **Compile**: compila el daemon `PLC1`.
4. Daemon > Monitor de tags > **Iniciar en simulacion** (o `PLC1 -sim` en una consola, en esta carpeta).
5. Ctrl+T compila y arranca el pvserver; Ctrl+P abre pvbrowser.

El simulador mueve algunos valores: con la bomba prendida el caudal sube y baja (pasa de 80 y el texto se
pone rojo); `FlagTWR` se prende cada 6 s y anima la imagen; cada motor está en marcha mientras su velocidad
es mayor que 0.

## Con un PLC real

En `PLC1.mkmodbus` (o en el diálogo del daemon):
- `tcpadr=` y `tcpport=`: dirección y puerto del PLC.
- `address_notation`, `word_order` y la dirección de cada tag, según el manual del PLC.

Después **Compile** de nuevo y arrancar `PLC1` sin `-sim`.
