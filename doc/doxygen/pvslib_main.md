pvslib - programación del pvserver {#mainpage}
==================================

Funciones para programar un **pvserver**: el programa que define las pantallas (máscaras) que muestra
pvbrowser y responde a sus eventos. Están declaradas en processviewserver.h.

Estructura de un pvserver
-------------------------

pvdevelop genera el proyecto: `main.cpp` (arranque), `maskN.cpp` (la pantalla, generada por el Designer)
y `maskN_slots.h` (la lógica). En los slots se reciben los eventos y se llama a las funciones pv...:

    static int slotInit(PARAM *p, DATA *d)            // al abrir la pantalla
    {
      pvDownloadFile(p, "Imagenes/fondo.png");
      pvSetImage(p, Fondo, "fondo.png");
      return 0;
    }

    static int slotNullEvent(PARAM *p, DATA *d)       // periódicamente
    {
      pvPrintf(p, LabelTemp, "%.1f °C", temperatura);
      return 0;
    }

    static int slotButtonEvent(PARAM *p, int id, DATA *d)
    {
      if(id == BotonAlarmas) return 2;               // mostrar la máscara 2
      return 0;
    }

Funciones más usadas
--------------------

| Función | Para qué |
|---------|----------|
| pvPrintf(), pvSetText() | Texto de etiquetas, campos, botones |
| pvSetValue(), pvSetChecked() | Valores de barras, sliders, casillas |
| pvSetPaletteBackgroundColor(), pvSetFontColor(), pvSetStyleSheet() | Colores y estilos |
| pvHide(), pvShow(), pvSetEnabled(), pvSetGeometry() | Visibilidad y posición |
| pvDownloadFile(), pvSetImage(), pvSetMovie() | Imágenes y animaciones |
| pvSetSource() | Página web, archivo HTML o panel de Grafana en un TextBrowser (Qt WebEngine) |
| pvSetTableText(), pvSetNumRows(), pvInsertItem() | Tablas y listas |
| pvMessageBox(), pvInputDialog(), pvFileDialog() | Diálogos (la respuesta llega como evento) |
| pvRunModalDialog() | Ventana emergente con otra máscara |

Los datos del proceso se leen con los clientes de rllib (rlModbusClient, rlSiemensTCPClient)
por medio de tags; ver el manual de usuario.

Versión
-------

`pvserver_version` (por ejemplo `6.0.1f`) es la versión del **protocolo** entre el pvserver y pvbrowser, heredada de pvbrowser, y
se manda al conectar. No es la versión de PV Suite (esa figura en el manual y en Ayuda > Acerca de).
