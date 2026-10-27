# PV Suite

[![Invitame un café en cafecito.app](https://cdn.cafecito.app/imgs/buttons/button_1.svg)](https://cafecito.app/nestorlevi)

SCADA/HMI libre basado en [pvbrowser](https://pvbrowser.de), adaptado a **Qt 6** y extendido con
un editor de tags y animaciones sin código. Funciona en **Windows** y **Linux**.

*English: PV Suite is a free SCADA/HMI toolkit based on pvbrowser, ported to Qt 6. It adds a tag editor,
Modbus daemons generated from pvdevelop and no-code actions and animations for the widgets of each screen.*

## Componentes

| Programa | Qué hace |
|---|---|
| **pvbrowser** | Cliente: muestra las pantallas que manda un pvserver. Incluye navegador web (Qt WebEngine). |
| **pvdevelop** | IDE con diseñador de pantallas (Designer), editor de código y generador de daemons. |
| **pvserver** | Biblioteca con la que se programan los servidores de pantallas (pvservers). |
| **rllib** | Biblioteca de comunicación: Modbus, Siemens, memoria compartida, mailbox, tags. |

## Novedades de la versión 1.0.0

- **Tags**: editor de tags de los daemons (Daemon > Modbus > Tags...). El daemon Modbus se genera desde
  pvdevelop junto con su `.tagmap`, con estructuras y bits de enteros (`PLC1:Estado.3`).
- **Simulación**: el daemon generado se conecta a un simulador Modbus con `-sim` (`simulation_tcpadr=`).
- **Botón "Tags..." en Propiedades**, sin escribir código:
  - acciones: escribir un valor, alternar, sumar, restar, ir a otra pantalla, volver, inicio,
    ingresar un valor con límites, confirmación y condición;
  - animaciones: texto con formato o lista de textos, valor, color según reglas, parpadeo, visible,
    imagen según el valor, animación de imágenes y marca de dato malo (daemon detenido).

  pvdevelop genera `maskN_pvb.h` al guardar la pantalla; el código propio de cada slot sigue disponible.
- **Monitor de tags**: valores en vivo de los daemons; escribir y forzar valores para depurar.
- **Designer**: copiar, pegar y alinear (izquierda, centro, derecha, arriba, medio, abajo, espaciado
  parejo); doble clic en un widget abre Propiedades.
- **Compilación** en un panel de pvdevelop (Ctrl+M / Ctrl+T) con las líneas de error marcadas en el
  editor; los pvservers se compilan con MinGW o con Visual Studio (se elige en pvdevelop).
  Ctrl+T compila y ejecuta el pvserver; en Linux se abre en una ventana de la terminal del sistema
  (x-terminal-emulator, qterminal, konsole, gnome-terminal, xfce4-terminal, lxterminal, mate-terminal o
  xterm) que se cierra como cualquier ventana, y Ctrl+T cierra antes el pvserver anterior del proyecto.
  Sin terminal instalada, corre dentro del panel de compilación y el botón "Detener" lo cierra.
- **Editor** con texto predictivo (Ctrl+Espacio).
- Idiomas: castellano, inglés, portugués y alemán. Ayuda > Buscar actualizaciones.

Los proyectos de pvbrowser siguen funcionando: las funciones nuevas son opcionales.

Los proyectos hechos con la versión completa de PV Suite (que tiene usuarios, recetas, automatización y
bloques) se abren también aquí: las acciones `login`, `logout`, `users`, `ack`, `recipes`, `automation`,
`faceplate` y `close`, y los textos `@user` y `@automation`, se conservan en el proyecto pero no hacen
nada, y pvdevelop avisa al generar.

## Instalar

Los instaladores están en [Releases](https://github.com/nestorlevi/PvSuite/releases).

- **Windows (64 bits)**: `PvSuite-<versión>-Setup-win64.exe`. Se instala en `C:\PVSuite`.
  Se puede instalar solo pvbrowser (cliente) desmarcando la suite de desarrollo.
- **Linux (x86-64)**: `PvSuite-<versión>-Setup-linux-x64.run` (`chmod +x` y ejecutarlo). Se instala en
  `/opt/pvb`, con Qt incluido. Si faltan bibliotecas del escritorio (xcb, WebEngine), el instalador lo
  avisa; `sudo sh /opt/pvb/terceros/preparar_linux.sh` las instala (apt, dnf o zypper).

Para compilar los pvservers hace falta un compilador: en Windows, el MinGW de Qt 6.9 o Visual Studio 2022;
en Linux, g++ y make (`sudo sh /opt/pvb/terceros/preparar_linux.sh -compilar`). Los proyectos anteriores
se pueden seguir compilando con MinGW 8.1 y Qt 5.15.2, de 64 o de 32 bits: marcar el componente opcional
"Bibliotecas para MinGW 8.1" al instalar y elegir ese MinGW en pvdevelop. Sirve para pasar los proyectos
viejos a esta versión y después llevarlos al MinGW de Qt 6.

## Primeros pasos

1. pvdevelop: File > New, elegir la carpeta y el nombre del proyecto.
2. Daemon > Modbus: dirección del PLC, tags (botón Tags...) y Compile: se genera el daemon y su `.tagmap`.
3. Designer: insertar widgets; en Propiedades, botón **Tags...** para sus acciones y animaciones.
4. Ctrl+T compila y ejecuta el pvserver; Ctrl+P abre pvbrowser.
5. Daemon > Monitor de tags para ver y forzar los valores; "Iniciar en simulacion" usa el simulador.

El ejemplo [ejemplos/01_modbus](ejemplos/01_modbus) muestra todo esto armado, con un
[simulador Modbus](ejemplos/simulador) para probarlo sin PLC.

## Documentación

- **Ayuda integrada**: en pvdevelop, **Ayuda > Manual** (Ctrl+H) abre el manual en un visor con índice, búsqueda y
  adelante/atrás; **F1** en cualquier ventana abre el tema de esa ventana. **Ayuda > Manual en PDF** abre el PDF
  y **Ayuda > Referencia de las funciones del pvserver** abre `doc/pvslib`.
- **Manual de usuario** en `doc/manual`, en castellano, inglés, portugués y alemán (`Manual_PvSuite.pdf`,
  `Manual_PvSuite_en.pdf`, `Manual_PvSuite_pt.pdf`, `Manual_PvSuite_de.pdf` y sus versiones HTML). El texto
  está en los HTML; los PDF y la ayuda integrada se generan.
- `doc/index.html` es la portada de la documentación (manuales, referencia de funciones y atajos de teclado).

`doc/generar_documentacion.bat` (Windows) o `.sh` (Linux) regeneran todo: la referencia `doc/pvslib` (doxygen),
el estilo del manual, la ayuda integrada y la portada (Python 3, scripts en `doc/herramientas`) y los PDF
(Edge o Chrome 131 o más nuevo).

## Compilar desde el código

Hace falta **Qt 6.9** (probado con 6.9.2).

Se compila con qmake, en una carpeta aparte (`build/`, fuera de git). También se pueden abrir los
`.pro` en Qt Creator, cada uno con su kit.

**Linux y macOS** (`pvsuite.pro` compila todo, pvbrowser incluido):

```sh
mkdir build && cd build
~/Qt/6.9.2/gcc_64/bin/qmake ../pvsuite.pro CONFIG+=release    # macOS: ~/Qt/6.9.2/macos/bin/qmake
make -j8
```

**Windows**: la suite con MinGW (`pvsuite.pro`) y pvbrowser con MSVC 2022 (`msvc/pvbrowser-msvc.pro`),
porque Qt WebEngine no existe para MinGW. El segundo también deja `rllib.lib` y `serverlib.lib`, para
compilar los pvservers con Visual Studio.

```bat
rem consola con C:\Qt\Tools\mingw1310_64\bin y C:\Qt\6.9.2\mingw_64\bin en el PATH
mkdir build\mingw && cd build\mingw
qmake ..\..\pvsuite.pro CONFIG+=release
mingw32-make -j8

rem consola "x64 Native Tools" de Visual Studio 2022 con C:\Qt\6.9.2\msvc2022_64\bin en el PATH
mkdir build\msvc && cd build\msvc
qmake ..\..\msvc\pvbrowser-msvc.pro CONFIG+=release
nmake
```

Las bibliotecas quedan junto a sus fuentes (`rllib/lib`, `pvserver`, `qwt/lib`); en Windows, pvdevelop
necesita `qwt/lib/qwt.dll` y pvbrowser `msvc/qwt-msvc/lib/qwt.dll` junto al `.exe`.

## Apoyar el proyecto

Si PV Suite te resulta útil, podés invitarme un café:

[![Invitame un café en cafecito.app](https://cdn.cafecito.app/imgs/buttons/button_1.svg)](https://cafecito.app/nestorlevi)

## Licencia y créditos

PV Suite se basa en pvbrowser, de Rainer Lehrig y la comunidad de pvbrowser
([pvbrowser.de](https://pvbrowser.de)). pvbrowser y pvdevelop se distribuyen bajo la **GPL**
(`LICENSE.GPL`); pvserver y rllib, bajo la **LGPL** (`LICENSE.LGPL`), así los pvservers pueden tener
cualquier licencia. Adaptación a Qt 6 y extensiones: Néstor Palomeque.
