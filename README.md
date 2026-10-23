# PV Suite

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
- **Editor** con texto predictivo (Ctrl+Espacio).
- Idiomas: castellano, inglés, portugués y alemán. Ayuda > Buscar actualizaciones.

Los proyectos de pvbrowser siguen funcionando: las funciones nuevas son opcionales.

## Instalar

Los instaladores están en [Releases](https://github.com/nestorlevi/PvSuite/releases).

- **Windows (64 bits)**: `PvSuite-<versión>-Setup-win64.exe`. Se instala en `C:\PVSuite`.
  Se puede instalar solo pvbrowser (cliente) desmarcando la suite de desarrollo.
- **Linux (x86-64)**: `PvSuite-<versión>-Setup-linux-x64.run` (`chmod +x` y ejecutarlo). Se instala en
  `/opt/pvb`, con Qt incluido. Si faltan bibliotecas del escritorio (xcb, WebEngine), el instalador lo
  avisa; `sudo sh /opt/pvb/terceros/preparar_linux.sh` las instala (apt, dnf o zypper).

Para compilar los pvservers hace falta un compilador: en Windows, el MinGW de Qt 6.9 o Visual Studio 2022;
en Linux, g++ y make (`sudo sh /opt/pvb/terceros/preparar_linux.sh -compilar`).

## Primeros pasos

1. pvdevelop: File > New, elegir la carpeta y el nombre del proyecto.
2. Daemon > Modbus: dirección del PLC, tags (botón Tags...) y Compile: se genera el daemon y su `.tagmap`.
3. Designer: insertar widgets; en Propiedades, botón **Tags...** para sus acciones y animaciones.
4. Ctrl+T compila y ejecuta el pvserver; Ctrl+P abre pvbrowser.
5. Daemon > Monitor de tags para ver y forzar los valores; "Iniciar en simulacion" usa el simulador.

El ejemplo [ejemplos/01_modbus](ejemplos/01_modbus) muestra todo esto armado, con un
[simulador Modbus](ejemplos/simulador) para probarlo sin PLC.

## Compilar desde el código

Hace falta **Qt 6.9** (probado con 6.9.2).

**Windows** (MinGW para la suite; pvbrowser con MSVC porque Qt WebEngine no existe para MinGW):

```bat
Compilar\pvsuite-win64\build.bat
```

Usa `C:\Qt\6.9.2\mingw_64`, `C:\Qt\Tools\mingw1310_64`, `C:\Qt\6.9.2\msvc2022_64` y Visual Studio 2022
Build Tools; las rutas se cambian con `QTDIR_MINGW`, `MINGWDIR`, `QTDIR_MSVC` y `VSDIR`.
También se pueden abrir `pvsuite-mingw.pro` y `pvbrowser-msvc.pro` en Qt Creator, cada uno con su kit.

**Linux**:

```sh
QTDIR=~/Qt/6.9.2/gcc_64 Compilar/pvsuite-linux64/build.sh
```

**Instaladores** (Qt Installer Framework 4.x): `Compilar/installer/collect_generate_installer.bat` en
Windows; `collect_files_linux.sh` y `generate_installer_linux.sh` en Linux (necesita `patchelf`).

## Licencia y créditos

PV Suite se basa en pvbrowser, de Rainer Lehrig y la comunidad de pvbrowser
([pvbrowser.de](https://pvbrowser.de)). pvbrowser y pvdevelop se distribuyen bajo la **GPL**
(`LICENSE.GPL`); pvserver y rllib, bajo la **LGPL** (`LICENSE.LGPL`), así los pvservers pueden tener
cualquier licencia. Adaptación a Qt 6 y extensiones: Néstor Palomeque.
