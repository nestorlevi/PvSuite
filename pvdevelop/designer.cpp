/****************************************************************************
**
** Copyright (C) 2000-2006 Lehrig Software Engineering.
**
** This file is part of the pvbrowser project.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/
#include "../pvbrowser/pvdefine.h"
#include <QtGui>
#include <algorithm>
#include <QScopeGuard>
#include "opt.h"
#include "MyWidgets.h"
#include "designer.h"
#include "widgetgenerator.h"
#include "pvbindgen.h"
#include "dlgproperty.h"
#include "dlginsertwidget.h"
#include "dlgeditlayout.h"
#include "interpreter.h"

extern OPT_DEVELOP opt_develop;
extern QStringList tablist;
dlgeditlayout *editlayout = NULL;

static QString Properties()          { return QCoreApplication::translate("Designer", "Properties (Alt+Click)"); }
static QString Insert_Widget()       { return QCoreApplication::translate("Designer", "Insert Widget (Shift+Click)"); }
static QString Delete_Widget()       { return QCoreApplication::translate("Designer", "Delete Widget (Del | Backspace)"); }
static QString Add_Tab()             { return QCoreApplication::translate("Designer", "Add Tab"); }
static QString Add_Item()            { return QCoreApplication::translate("Designer", "Add Item"); }
static QString Define_new_TabOrder() { return QCoreApplication::translate("Designer", "define new TabOrder"); }
static QString Edit_layout()         { return QCoreApplication::translate("Designer", "edit layout (L)"); }
static QString End_define_TabOrder() { return QCoreApplication::translate("Designer", "end define TabOrder"); }
static QString DGrabMouse()          { return QCoreApplication::translate("Designer", "grabMouse (G)"); }
static QString DReleaseMouse()       { return QCoreApplication::translate("Designer", "releaseMouse (R)"); }

Designer::Designer(const char *mask)
{
  root = new MyRootWidget(NULL);
  editlayout = new dlgeditlayout;
  root->setObjectName("0");
  filename = mask;
  imask = 0;
  sscanf(mask,"mask%d", &imask);
  xclick = yclick = 0;
  if(opt_develop.arg_debug) printf("Designer::Designer: before getWidgetsFromMask(%s)\n",mask);
  if(opt_develop.arg_mask_to_generate == -1) 
  {
    if(opt_develop.script == PV_LUA) 
    {
      //rlmurx-was-here filename.sprintf("mask%d.lua", imask); 
      filename = QString::asprintf("mask%d.lua", imask); 
      getWidgetsFromMask(filename.toUtf8(), root);
    }
    else
    {
      getWidgetsFromMask(mask, root);
      pvbLoadBindings(mask, root);   // Nestor: acciones y animaciones de Propiedades
    }
  }  
  if(opt_develop.arg_debug) printf("Designer::Designer: after getWidgetsFromMask(%s)\n",mask);
}

Designer::~Designer()
{
  int ret;

  if(root->modified == 1 || root->modified == -1)
  {
    QString header  = "pvdevelop";
    ret = QMessageBox::No;
    if(root->modified == 1)
    {
      QString message = QCoreApplication::translate("Designer", "The design has been modified.\nDo you want to save your changes?");
      ret = QMessageBox::warning(root,header,message,QMessageBox::Yes,QMessageBox::No);
    }
    else
    {
      ret = QMessageBox::Yes;
    }
    if(ret == QMessageBox::Yes)
    { 
      generateMask(filename.toUtf8(), root); // generate C/C++
      if(opt_develop.script != PV_LUA) pvbSaveBindings(filename, root);   // Nestor: maskN_pvb.h
      // add additional language here
      if(opt_develop.script == PV_PYTHON)
      {
         if(opt_develop.arg_debug) printf("generate python imask=%d\n", imask);
         generatePython(imask, root);
      } 
      if(opt_develop.script == PV_PERL)
      {
         if(opt_develop.arg_debug) printf("generate perl imask=%d\n", imask);
         generatePerl(imask, root);
      } 
      if(opt_develop.script == PV_PHP)
      {
         if(opt_develop.arg_debug) printf("generate php imask=%d\n", imask);
         generatePHP(imask, root);
      } 
      if(opt_develop.script == PV_TCL)
      {
         if(opt_develop.arg_debug) printf("generate tcl imask=%d\n", imask);
         generateTcl(imask, root);
      } 
    }
  }
  delete editlayout;
  editlayout = NULL;
  QApplication::restoreOverrideCursor();
  QApplication::setOverrideCursor(QCursor(Qt::ArrowCursor));
}

MyRootWidget::MyRootWidget(MyRootWidget *parent)
             :QWidget(parent)
{
  scroll = NULL;
  xOld = yOld = xChild0 = yChild0 = wChild0 = hChild0 = buttonDown = tabbing = copying = inMiddle = parentLevel = 0;
  modified = 0;
  reparentDone = 0;
  parentLevel0 = parentLevel1 = parentLevel2 = NULL;
  clickedChild = lastChild = NULL;
  statusBar = NULL;
  lastChild = NULL;
  lastTabChild = NULL;
  setMouseTracking(true);
  //setFocusPolicy(Qt::NoFocus);
  //setFocusPolicy(Qt::StrongFocus);  // <--- necesario para que reciba eventos de teclado
  //setFocus();                        // <--- opcional, asegura que lo tenga al empezar
  qApp->installEventFilter(this);   // Nestor: Ctrl+C / Ctrl+V de widgets (ver eventFilter)
  
  grabbed = 2;
  opt_develop.altPressed = 0;
  opt_develop.ctrlPressed = 0;
  opt_develop.shiftPressed = 0;
  insert.myrootwidget = this;
  setCursor(Qt::ArrowCursor);
}

MyRootWidget::~MyRootWidget()
{
  releaseMouse();
  mainWindow->releaseKeyboard();
  setCursor(Qt::ArrowCursor);
}

void MyRootWidget::setScroll(QScrollArea *_scroll)
{
  scroll = _scroll;
}

QWidget *MyRootWidget::getChild(int x, int y)
{
  QWidget *child;
  parentLevel = 0;
  child = childAt(x,y);
  parentLevel0 = child;
  if(child != NULL)
  {
    QString st = child->statusTip();
    if(st.isEmpty())
    {
      parentLevel = 1;
      child = (QWidget *) child->parent();
      parentLevel1 = child;
      st = child->statusTip();
    }
    if(st.isEmpty())
    {
      parentLevel = 2;
      child = (QWidget *) child->parent();
      parentLevel2 = child;
      st = child->statusTip();
    }
  }
  return child;
}

void MyRootWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0) //Nestor aquí: Compatibilidad con QT6
    int x = (int) event->position().x();
    int y = (int) event->position().y();
#else
    int x = event->pos().x();
    int y = event->pos().y();
#endif

  if(opt_develop.arg_debug > 0) printf("DoubleClickEvent\n");
  // Nestor: doble clic sobre un widget: sus propiedades (Shift+clic y el lugar vacio: insertar, como antes)
  if(!opt_develop.shiftPressed)
  {
    QWidget *child = getChild(x,y);
    if(child != NULL && child != this && !child->statusTip().startsWith("TQWidget:"))
    {
      buttonDown = 0;
      if(lastChild != nullptr) perhapsDeselectOldWidget();
      clearSelectionFrames();
      dlgProperty dlg(child);
      releaseMouse();
      mainWindow->releaseKeyboard();
      setCursor(Qt::ArrowCursor);
      if(dlg.run() == 1) modified = 1;
      grabbed = 1;
      grabMouse();
      mainWindow->grabKeyboard();
      setCursor(Qt::CrossCursor);
      return;
    }
  }
  //#####################################
  grabbed = 0;
  releaseMouse();
  mainWindow->releaseKeyboard();
  setCursor(Qt::ArrowCursor);
  //#####################################
  QWidget *p = this;    //"this" es el widget que recibe el doble clic, es el root donde se colocará el nuevo widget. p inicia en this pero cambia en insert.newWidget con el widget seleccionado en el ui.
  if(parentLevel == 0)  //No hay padre
  {
    if(lastChild != NULL) perhapsDeselectOldWidget();
    insert.myrootwidget = this;
    insert.run();
    if(insert.ret == 1)
    {
      int xw,yw;
      QPoint parent0 = p->mapToGlobal(QPoint(0,0));
      QPoint thisxy  = mapToGlobal(QPoint(x,y));
      xw = thisxy.x() - parent0.x();
      yw = thisxy.y() - parent0.y();
      if(opt_develop.arg_debug) printf("Insert Widget: pos(%d,%d)\n",xw,yw);
      insert.newWidget(this,p,xw,yw);
      modified = 1;
      stackClear();
    }
  }
  grabbed = 1;
  grabMouse();
  mainWindow->grabKeyboard();
  setCursor(Qt::CrossCursor);
}

void MyRootWidget::MoveKey(int key)
{
  // Nestor: las flechas mueven (Ctrl+flechas redimensionan) todos los widgets seleccionados
  if(grabbed == 0) return;
  const QList<QWidget *> selected = selectedWidgets();
  beginUndoStep();
  for(int i=0; i<selected.size(); i++) moveKeyWidget(selected.at(i), key);
  endUndoStep();
}

void MyRootWidget::moveKeyWidget(QWidget *child, int key)
{
  int xNew, yNew;
  int wNew, hNew;
  if(child == NULL) return;

  push(child);
  if(opt_develop.ctrlPressed)
  {
    if     (key == Qt::Key_Left)
    {
      wNew = child->width() - opt_develop.xGrid;
      hNew = child->height();
    }
    else if(key == Qt::Key_Right)
    {
      wNew = child->width() + opt_develop.xGrid;
      hNew = child->height();
    }
    else if(key == Qt::Key_Up)
    {
      wNew = child->width();
      hNew = child->height() - opt_develop.yGrid;
    }
    else if(key == Qt::Key_Down)
    {
      wNew = child->width();
      hNew = child->height() + opt_develop.yGrid;
    }
    else
    {
      return;
    }
    wNew = (wNew/opt_develop.xGrid)*opt_develop.xGrid;
    hNew = (hNew/opt_develop.yGrid)*opt_develop.yGrid;
    if(wNew < opt_develop.xGrid) wNew = opt_develop.xGrid;
    if(hNew < opt_develop.yGrid) hNew = opt_develop.yGrid;
    if(wNew>=0 && hNew>=0)
    {
      myResize(child,wNew,hNew);
      modified = 1;
    }  
  }
  else
  {
    if     (key == Qt::Key_Left)
    {
      xNew = child->x() - opt_develop.xGrid;
      yNew = child->y();
    }
    else if(key == Qt::Key_Right)
    {
      xNew = child->x() + opt_develop.xGrid;
      yNew = child->y();
    }
    else if(key == Qt::Key_Up)
    {
      xNew = child->x();
      yNew = child->y() - opt_develop.yGrid;
    }
    else if(key == Qt::Key_Down)
    {
      xNew = child->x();
      yNew = child->y() + opt_develop.yGrid;
    }
    else
    {
      return;
    }
    xNew = (xNew/opt_develop.xGrid)*opt_develop.xGrid;
    yNew = (yNew/opt_develop.yGrid)*opt_develop.yGrid;
    if(xNew>=0 && yNew>=0 &&
       xNew<((QWidget *)child->parent())->width() && yNew<((QWidget *)child->parent())->height())
    {
      myMove(child,xNew,yNew);
      modified = 1;
    }
  }
  printStatusMessage(child);
}

void MyRootWidget::mouseMoveEvent(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0) //Nestor aquí: Compatibilidad con QT6
    const int x  = event->position().x();           // Coordenada X relativa al widget
    const int y  = event->position().y();           // Coordenada Y relativa al widget
    const int gx = event->globalPosition().x();     // Coordenada X absoluta (pantalla)
    const int gy = event->globalPosition().y();     // Coordenada Y absoluta (pantalla)
#else
    int x = event->pos().x();
    int y = event->pos().y();

    int gx = event->globalPos().x();
    int gy = event->globalPos().y();
#endif

    if(opt_develop.arg_debug > 0) printf("mouseMoveEvent x=%d y=%d\n", x, y);

    // --- ÁREA DE DISEÑO ---
    if (aboveDesignArea(x, y, gx, gy)) {
        // Area válida para diseño
        if (grabbed != 1) {                   // grabbed vale 2 al crear el widget: también hay que capturar
            grabbed = 1;
            grabMouse();                      // Captura del mouse al widget raíz
            mainWindow->grabKeyboard();       // Captura del teclado

            // Cambia el cursor si es diferente al actual
            if (currentCursor != Qt::CrossCursor) {
                setCursor(Qt::CrossCursor);   // Cursor cruz
                currentCursor = Qt::CrossCursor;
            }
        }
    } else {
        // Fuera del área de diseño
        if (grabbed) {
            grabbed = 0;
            releaseMouse();
            mainWindow->releaseKeyboard();

            if (currentCursor != Qt::ArrowCursor) {
                setCursor(Qt::ArrowCursor);   // Cursor normal
                currentCursor = Qt::ArrowCursor;
            }
        }
        return; // No se ejecuta más nada
    }

    // --- RECTÁNGULO DE SELECCIÓN ---
    if (buttonDown && rubberBand != nullptr && rubberBand->isVisible()) {
        rubberBand->setGeometry(QRect(rubberOrigin, QPoint(x, y)).normalized());
        return;
    }

    // --- MANEJO DE WIDGETS EXISTENTES ---
    QWidget *child = clickedChild;

    // Nestor: un clic o doble clic con un temblor de la mano no mueve ni cambia el tamano (antes un pixel bastaba y
    // ademas lo acomodaba a la grilla): el arrastre empieza cuando el mouse se aleja del clic mas que startDragDistance
    if (child && grabbed && buttonDown && !dragStarted) {
        if ((QPoint(x, y) - QPoint(xOld, yOld)).manhattanLength() < QApplication::startDragDistance()) return;
        dragStarted = true;
    }

    // --- ARRASTRE DE LA SELECCIÓN MÚLTIPLE ---
    if (child && grabbed && buttonDown && inMiddle == 1 && selection.size() > 1 &&
        isSelected(child) && groupStart.size() == selection.size()) {
        const int gridX = opt_develop.xGrid;
        const int gridY = opt_develop.yGrid;
        int xNew = ((xChild0 + x - xOld) / gridX) * gridX;
        int yNew = ((yChild0 + y - yOld) / gridY) * gridY;
        const int dx = xNew - xChild0;
        const int dy = yNew - yChild0;

        // solo se mueve si todos quedan dentro de su contenedor
        bool inside = true;
        for (int i = 0; i < selection.size(); i++) {
            QWidget *w = selection.at(i);
            QWidget *parent = w ? w->parentWidget() : nullptr;
            if (!parent) continue;
            const QPoint p = groupStart.at(i) + QPoint(dx, dy);
            if (p.x() < 0 || p.y() < 0 || p.x() >= parent->width() || p.y() >= parent->height()) inside = false;
        }
        if (inside) {
            for (int i = 0; i < selection.size(); i++) {
                QWidget *w = selection.at(i);
                if (!w) continue;
                if (!groupPushed) push(w);   // un solo paso de deshacer por widget y arrastre
                myMove(w, groupStart.at(i).x() + dx, groupStart.at(i).y() + dy);
            }
            groupPushed = true;
            modified = true;
            printStatusMessage(child);
        }
        return;
    }

    if (child && grabbed) {
        if(opt_develop.arg_debug > 0) printf("child->statusTip=%s\n", child->statusTip().toUtf8().constData());

        if (buttonDown) {
            const int gridX = opt_develop.xGrid;  // Paso de grilla horizontal
            const int gridY = opt_develop.yGrid;  // Paso de grilla vertical

            if (inMiddle == 1) {
                // --- MOVIMIENTO DE WIDGET ---
                int xNew = xChild0 + x - xOld; //xChild: posición original widget; xOld: posición previa del mouse; x: posicón actual del mouse
                int yNew = yChild0 + y - yOld;
                xNew = (xNew / gridX) * gridX; //Adapto a la posición de la grilla
                yNew = (yNew / gridY) * gridY;

                QWidget *parent = qobject_cast<QWidget*>(child->parent());
                if (parent && xNew >= 0 && yNew >= 0 && xNew < parent->width() && yNew < parent->height()) {
                    push(child); // Guardo en historial de deshacer
                    myMove(child, xNew, yNew); //Muevo el widget
                }
                else if (reparentDone == 0) {
                    // --- RE-PARENTING: se traslada a la raíz ---
                    QWidget *root = scroll->widget();
                    if (root) {
                        push(child);

                        const int dx = child->x();
                        const int dy = child->y();
                        const QPoint childPos0 = child->mapToGlobal(QPoint(0,0));
                        const QPoint parentPos0 = root->mapToGlobal(QPoint(0,0));

                        child->hide();
                        child->setParent(root);  // Nuevo parent

                        xNew = childPos0.x() - parentPos0.x();
                        yNew = childPos0.y() - parentPos0.y();
                        xNew = (xNew / gridX) * gridX + dx;
                        yNew = (yNew / gridY) * gridY + dy;

                        myMove(child, xNew, yNew);
                        child->show();

                        reparentDone = 1;
                        xChild0 = child->x() - child->width()/2;
                        yChild0 = child->y() - child->height()/2;
                        wChild0 = child->width();
                        hChild0 = child->height();
                        xOld = xNew;
                        yOld = yNew;
                    }
                }
                modified = true;
            }
            else if (inMiddle == 0) {
                // --- REDIMENSIONAMIENTO DEL WIDGET ---
                int wNew = wChild0 + x - xOld;
                int hNew = hChild0 + y - yOld;
                wNew = (wNew / gridX) * gridX;
                hNew = (hNew / gridY) * gridY;

                if (wNew < gridX) wNew = gridX;
                if (hNew < gridY) hNew = gridY;

                push(child);
                if (wNew >= 0 && hNew >= 0)
                    myResize(child, wNew, hNew);

                modified = true;
            }
        }
    }
    else if (grabbed && !child) {
        // --- ACTUALIZACIÓN DE CURSOR SEGÚN POSICIÓN ---
        child = getChild(x, y);  // Obtengo widget debajo del cursor
        int x0 = 0, y0 = 0, w = width(), h = height();

        if (child) {
            const QPoint childPos0 = child->mapToGlobal(QPoint(0,0));
            const QPoint thisPos0 = mapToGlobal(QPoint(0,0));
            x0 = childPos0.x() - thisPos0.x();
            y0 = childPos0.y() - thisPos0.y();
            w = child->width();
            h = child->height();
        }

        if(opt_develop.arg_debug > 0) printf("x0=%d y0=%d x=%d y=%d w=%d h=%d\n", x0, y0, x, y, w, h);

        // Selección del cursor adecuado
        Qt::CursorShape newCursor = Qt::CrossCursor;  // Por defecto cruz
        int modo = 1;  // 1 = mover, 0 = redimensionar

        if (child && (x - x0) > (w - 5) && (y - y0) > (h - 5)) {
            newCursor = Qt::SizeFDiagCursor;  // Esquina inferior derecha = resize
            modo = 0;
        }
        else if (child && child->statusTip().startsWith("TQ") && !child->statusTip().startsWith("TQWidget")) {
            newCursor = Qt::SizeAllCursor;  // Arrastrar widget
            modo = 1;
        }

        // Cambio de cursor solo si realmente cambia
        if (currentCursor != newCursor) {
            setCursor(newCursor);
            currentCursor = newCursor;
        }

        inMiddle = modo;
    }

    printStatusMessage(child);  // Muestra info del widget bajo el cursor
}

void MyRootWidget::printStatusMessage(QWidget *child)
{
  if(child != NULL && statusBar != NULL)
  {
    QString pos;
    //rlmurx-was-here pos.sprintf(" x=%d y=%d",child->x(),child->y());
    pos = QString::asprintf(" x=%d y=%d",child->x(),child->y());
    QString dim;
    //rlmurx-was-here dim.sprintf(" width=%d height=%d",child->width(),child->height());
    dim = QString::asprintf(" width=%d height=%d",child->width(),child->height());
    QString message = child->statusTip();
    message += " objectName=\"";
    message += child->objectName();
    message += "\" toolTip=\"";
    message += child->toolTip();
    message += "\" whatsThis=\"";
    message += child->whatsThis();
    message += "\"";
    message += pos;
    message += dim;
    statusBar->showMessage(message);
  }
  else if(statusBar != NULL)
  {
    statusBar->showMessage("over background");
  }
}

void MyRootWidget::setCursor(Qt::CursorShape cursor)
{

#ifdef PVWIN32
  // damn windows resets cursor otherwise
  QApplication::processEvents();
  QApplication::restoreOverrideCursor();
  QApplication::setOverrideCursor(QCursor(cursor));
#endif

#ifdef PVUNIX
  if(cursor != currentCursor)
  {
    QApplication::restoreOverrideCursor();
    QApplication::setOverrideCursor(QCursor(cursor));
  }
#endif
  currentCursor = cursor; // Nestor: también en Windows, mouseMoveEvent lo compara para no repetir setCursor()

}

int MyRootWidget::aboveDesignArea(int x, int y, int gx, int gy)
{
  if(x < 0) return 0;
  if(y < 0) return 0;
  // Nestor: en X11 (Openbox de Lubuntu) la ventana puede no figurar como activa despues de pasar del editor al
  // Designer y no se podia mover ningun widget hasta abrir y cerrar un dialogo. X manda el evento a la ventana
  // que esta bajo el mouse (o a la que lo capturo, y con la captura no se puede pasar a otra): si llego, es la nuestra
  static const bool xcb = (QGuiApplication::platformName() == "xcb");
  if(isActiveWindow() || (xcb && window()->isVisible()))
  {
    int dx = 0;
    int dy = 0;
    QWidget *vp = scroll->viewport();
    if(vp->width()  < scroll->width())  dx = scroll->verticalScrollBar()->width();
    if(vp->height() < scroll->height()) dy = scroll->horizontalScrollBar()->height();
    QPoint gm = mainWindow->mapToGlobal(QPoint(0,0));
    if(gx < gm.x()) return 0;
    if(gy < gm.y()) return 0;
    if(gx > gm.x() + mainWindow->width()                                      - dx) return 0;
    if(gy > gm.y() + mainWindow->height() - mainWindow->statusBar()->height() - dy) return 0;
    return 1;
  }
  return 0;
}

void MyRootWidget::clearSelectionFrame(QWidget *w)
{
    if (w == nullptr) return;
    // Eliminar el marco de selección del widget (no toca lastChild)
    QVariant focusFrameVariant = w->property("focusFrame");
    if (focusFrameVariant.isValid()) {
        QFocusFrame *focusFrame = qvariant_cast<QFocusFrame*>(focusFrameVariant);
        if (focusFrame) focusFrame->deleteLater();  // Eliminar el QFocusFrame de forma segura
        w->setProperty("focusFrame", QVariant());   // Limpiar la propiedad
    }
}

// Nestor: selección múltiple.
//   clic                 -> selecciona un widget
//   Ctrl+clic            -> agrega/quita un widget de la selección
//   arrastrar en vacío   -> rectángulo de selección (con Ctrl se suma a la selección)
// selection[0] es la referencia para alinear (marco azul); el resto, marco verde.
void MyRootWidget::clearSelectionFrames()
{
    for (int i = 0; i < selection.size(); i++) clearSelectionFrame(selection.at(i));
    clearSelectionFrame(lastChild);
}

void MyRootWidget::setSelectionFrame(QWidget *w, bool primary)
{
    if (w == nullptr) return;
    clearSelectionFrame(w);
    QFocusFrame *focusFrame = new QFocusFrame(this);
    focusFrame->setWidget(w);

    // Usar setStyleSheet para establecer el estilo de borde
    const char *color = primary ? "#1E88E5" : "#4CAF50";
    focusFrame->setStyleSheet(QString(R"(QFocusFrame {
        border-top: 1px solid %1;    /* Borde superior de 1px */
        border-left: 1px solid %1;   /* Borde izquierdo de 1px */
        border-right: 2px solid %1;  /* Borde derecho de 2px */
        border-bottom: 2px solid %1; /* Borde inferior de 2px */
        border-radius: 4px;          /* Bordes redondeados */
        padding: 5px;                /* Espaciado interno */
        })").arg(color));

    focusFrame->show();
    w->setProperty("focusFrame", QVariant::fromValue(focusFrame));  // Guardar el QFocusFrame
}

void MyRootWidget::refreshSelectionFrames()
{
    // quitar los widgets que ya no existen (borrados)
    for (int i = selection.size() - 1; i >= 0; i--) {
        if (selection.at(i).isNull()) selection.removeAt(i);
    }
    for (int i = 0; i < selection.size(); i++) {
        setSelectionFrame(selection.at(i), i == 0 && selection.size() > 1);
    }
    lastChild = selection.isEmpty() ? nullptr : selection.first().data();
    if (lastChild != nullptr) {
        // restoreColors() usa la paleta y el statusTip del widget origen
        savedPalette = lastChild->palette();
        savedStatusTip = lastChild->statusTip();
    }
}

void MyRootWidget::clearSelection()
{
    clearSelectionFrames();
    selection.clear();
    lastChild = nullptr;
}

bool MyRootWidget::isSelected(QWidget *w)
{
    if (w == nullptr) return false;
    for (int i = 0; i < selection.size(); i++) {
        if (selection.at(i).data() == w) return true;
    }
    return false;
}

QList<QWidget *> MyRootWidget::selectedWidgets()
{
    QList<QWidget *> list;
    for (int i = 0; i < selection.size(); i++) {
        if (!selection.at(i).isNull()) list.append(selection.at(i).data());
    }
    if (list.isEmpty() && lastChild != nullptr) list.append(lastChild);
    return list;
}

void MyRootWidget::perhapsDeselectOldWidget()
{
    // Mientras se define el TabOrder o se copian atributos, lastChild es el widget origen: no deseleccionar
    if (tabbing == 0 && copying == 0)
    {
        if (lastChild != nullptr && opt_develop.arg_debug > 0) printf("Deselect widget: %s\n", lastChild->objectName().toStdString().c_str());
        clearSelection();
    }
}

void MyRootWidget::selectWidget(QWidget *child)
{
    if (!child)
        return;

    // Selección simple: reemplaza la selección anterior
    clearSelectionFrames();
    selection.clear();
    selection.append(child);
    refreshSelectionFrames();

    // Guardar la posición y tamaño del widget seleccionado
    xChild0 = child->x();
    yChild0 = child->y();
    wChild0 = child->width();
    hChild0 = child->height();

    // Actualizar el widget seleccionado
    clickedChild = child;
    lastChild = child;

    if (opt_develop.arg_debug > 0) printf("Selected Widget: %s\n", child->objectName().toStdString().c_str());
}

void MyRootWidget::toggleSelection(QWidget *child)
{
    if (!child) return;
    for (int i = 0; i < selection.size(); i++) {
        if (selection.at(i).data() == child) {
            clearSelectionFrame(child);
            selection.removeAt(i);
            refreshSelectionFrames();
            clickedChild = nullptr;
            return;
        }
    }
    selection.append(child);
    refreshSelectionFrames();
    clickedChild = nullptr;
}

void MyRootWidget::startRubberBand(QWidget *container, int x, int y)
{
    if (rubberBand == nullptr) rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
    rubberContainer = container;
    rubberOrigin = QPoint(x, y);
    rubberBand->setGeometry(QRect(rubberOrigin, QSize()));
    rubberBand->show();
    clickedChild = nullptr;
}

void MyRootWidget::finishRubberBand()
{
    if (rubberBand == nullptr) return;
    const QRect band = rubberBand->geometry();
    rubberBand->hide();
    QWidget *container = rubberContainer.isNull() ? this : rubberContainer.data();

    // widgets del mismo contenedor que toca el rectángulo
    const QList<QObject *> children = container->children();
    for (int i = 0; i < children.size(); i++) {
        if (!children.at(i)->isWidgetType()) continue;
        QWidget *w = (QWidget *) children.at(i);
        if (!w->isVisible()) continue;
        if (!w->statusTip().startsWith("TQ") || w->statusTip().startsWith("TQWidget:")) continue;
        const QRect r(w->mapTo(this, QPoint(0, 0)), w->size());
        if (band.intersects(r) && !isSelected(w)) selection.append(w);
    }
    refreshSelectionFrames();
    printStatusMessage(lastChild);
}

void MyRootWidget::mousePressEvent(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0) //Nestor aquí: Compatibilidad con QT6
    int x = (int) event->position().x();
    int y = (int) event->position().y();

    int gx = (int) event->globalPosition().x();
    int gy = (int) event->globalPosition().y();
#else
    int x = event->pos().x();
    int y = event->pos().y();

    int gx = event->globalPos().x();
    int gy = event->globalPos().y();
#endif
  // int gx = event->globalX();
  // int gy = event->globalY();

  if(opt_develop.arg_debug > 0) printf("mousePressEvent x=%d y=%d\n",x,y);
  reparentDone = 0;
  //#####################################
  if(aboveDesignArea(x,y,gx,gy))
  {
    grabbed = 1;
    grabMouse();
    mainWindow->grabKeyboard();
  }
  else
  {
    grabbed = 0;
    releaseMouse();
    mainWindow->releaseKeyboard();
    return;
  }
  //#####################################
  QWidget *child = getChild(x,y);
  if(opt_develop.altPressed)
  {
    if(child != NULL)
    {
	/*	  
      if(lastChild != NULL)
      {
        lastChild->setAutoFillBackground(false);
        if( lastChild->statusTip().contains("TQLabel:")) lastChild->setAutoFillBackground(true);
        if( lastChild->statusTip().contains("TQFrame:")) lastChild->setAutoFillBackground(true);
        if( lastChild->statusTip().contains("TQRadio:")) lastChild->setAutoFillBackground(true);
        if( lastChild->statusTip().contains("TQCheck:")) lastChild->setAutoFillBackground(true);
        lastChild->setPalette(savedPalette);
      }
	*/
	// Si estaba seleccionado, deseleccionarlo. Evita que la ventana propiedades tenga datos incorrectos dados por selectWidget()
    if(lastChild != nullptr) perhapsDeselectOldWidget(); //Hace lastChild = nullptr
      dlgProperty dlg(child);
      releaseMouse();
      mainWindow->releaseKeyboard();
      setCursor(Qt::ArrowCursor);
      dlg.run();
      opt_develop.altPressed = 0;
      grabbed = 1;
      grabMouse();
      mainWindow->grabKeyboard();
      modified = 1;
    }
    opt_develop.altPressed = 0;
    return;
  }
  if(opt_develop.shiftPressed && !opt_develop.ctrlPressed) // Ctrl+Shift+clic: insertar el último widget (abajo)
  {
    mouseDoubleClickEvent(event); // show insert dialog
    opt_develop.shiftPressed = 0;
    return;
  }

  if(event->button() == Qt::LeftButton)
  {
    if(opt_develop.arg_debug > 0) printf("mousePressEvent LeftButton parentLevel=%d ctrlPressed=%d\n",parentLevel,opt_develop.ctrlPressed);
    const bool normalMode = (tabbing == 0 && copying == 0);
    const bool ctrl  = opt_develop.ctrlPressed == 1;
    const bool shift = opt_develop.shiftPressed == 1;
    // Nestor: un widget vacío de pestaña/ToolBox (TQWidget:) no se selecciona: ahí se arrastra el rectángulo
    const bool selectable = (child != NULL && !child->statusTip().startsWith("TQWidget:"));
    xOld = x;
    yOld = y;
    buttonDown = 1;
    groupPushed = false;
    dragStarted = false;
    beginUndoStep();   // Nestor: todo el arrastre es un solo Ctrl+Z

    if(normalMode && ctrl && !shift) // Ctrl+clic: agregar/quitar de la selección
    {
      if(selectable) toggleSelection(child);
      else           startRubberBand(child != NULL ? child : this, x, y); // Ctrl+arrastre: suma a la selección
      return;
    }
    if(normalMode && !ctrl && selectable && isSelected(child) && selection.size() > 1)
    {
      // clic sobre un widget de la selección múltiple: se arrastra todo el grupo
      clickedChild = child;
      xChild0 = child->x();
      yChild0 = child->y();
      wChild0 = child->width();
      hChild0 = child->height();
      groupStart.clear();
      for(int i=0; i<selection.size(); i++)
        groupStart.append(selection.at(i).isNull() ? QPoint() : selection.at(i)->pos());
    }
    else
    {
      perhapsDeselectOldWidget();
      if(normalMode && !ctrl) // set highlight
      {
        if(opt_develop.arg_debug > 0) printf("mousePressEvent LeftButton set background\n");
        if(selectable) selectWidget(child);
        else           startRubberBand(child != NULL ? child : this, x, y); // arrastre en zona vacía
      }
    }
    if(opt_develop.arg_debug > 0) printf("mousePressEvent LeftButton murx2\n");
    if(ctrl && shift && normalMode) // Ctrl+Shift+clic: insert last selected widget
    {
      QWidget *p = child;
      if(p == NULL && x>0 && y>0) p = this;
      if(p != NULL && parentLevel == 0)
      {
        if(opt_develop.arg_debug > 0) printf("mousePressEvent LeftButton newWidget\n");
        int xw,yw;
        QPoint parent0 = p->mapToGlobal(QPoint(0,0));
        QPoint thisxy  = mapToGlobal(QPoint(x,y));
        xw = thisxy.x() - parent0.x();
        yw = thisxy.y() - parent0.y();
        if(opt_develop.arg_debug) printf("Insert Widget: pos(%d,%d)\n",xw,yw);
        grabbed = 0;
        releaseMouse();
        mainWindow->releaseKeyboard();
        setCursor(Qt::ArrowCursor);
        insert.myrootwidget = this;
        QWidget *wnew = insert.newWidget(this,p,xw,yw);
        setCursor(Qt::CrossCursor);
        modified = 1;
        grabbed = 1;
        grabMouse();
        mainWindow->grabKeyboard();
        stackClear();
        selectWidget(wnew);
      }
    }
    if(child != NULL && grabbed == 1 && tabbing == 1 && copying == 0
       && !child->statusTip().startsWith("TQWidget:"))
    {
      if(lastTab.isEmpty())
      {
        lastTab = child->objectName();
        lastTabChild = child;
      }
      else
      {
        QString newTab = child->objectName();
        QString tabCommand;
        if(opt_develop.script == PV_LUA) tabCommand = "  pv.pvTabOrder(p,";
        else                     tabCommand = "  pvTabOrder(p,";
        tabCommand.append(lastTab);
        tabCommand.append(",");
        tabCommand.append(newTab);
        if(opt_develop.script == PV_LUA) tabCommand.append(")\n");
        else                     tabCommand.append(");\n");
        tablist.append(tabCommand);
        if(lastTabChild != NULL && child != NULL) QWidget::setTabOrder(lastTabChild,child);
        lastTab = newTab;
        lastTabChild = child;
      }
      if(child->statusTip().startsWith("TQTabWidget:"))
      {
        releaseMouse();
        releaseKeyboard();
        mainWindow->releaseKeyboard();
        QMessageBox::information(0,QCoreApplication::translate("Designer", "pvdevelop: define new TabOrder."),
                     QCoreApplication::translate("Designer", "TabWidget added to TabOrder\n"),
                     QMessageBox::Ok);
        grabbed = 1;
        grabMouse();
        mainWindow->grabKeyboard();
      }
      else if(child->statusTip().startsWith("TQToolBox:"))
      {
        releaseMouse();
        releaseKeyboard();
        mainWindow->releaseKeyboard();
        QMessageBox::information(0,QCoreApplication::translate("Designer", "pvdevelop: define new TabOrder."),
                     QCoreApplication::translate("Designer", "ToolBox added to TabOrder\n"),
                     QMessageBox::Ok);
        grabbed = 1;
        grabMouse();
        mainWindow->grabKeyboard();
      }
      else
      {
        child->hide();
      }
    }
    if(opt_develop.arg_debug > 0) printf("mousePressEvent LeftButton done\n");
  }
  else if(event->button() == Qt::RightButton)
  {
    QMenu popupMenu;
    QAction *ret;
    if(grabbed == 1 && tabbing == 0 && copying == 0) popupMenu.addAction(Properties());
    if(grabbed == 1 && tabbing == 0 && copying == 0) popupMenu.addAction(Insert_Widget());
    if(grabbed == 1 && tabbing == 0 && copying == 0) popupMenu.addAction(Delete_Widget());
    if(child != NULL && child->statusTip().startsWith("TQTabWidget:") &&
       tabbing == 0 && copying == 0)
                                                     popupMenu.addAction(Add_Tab());
    if(child != NULL && child->statusTip().startsWith("TQToolBox:") &&
       tabbing == 0 && copying == 0)
                                                     popupMenu.addAction(Add_Item());
    if(grabbed == 1 && tabbing == 0 && copying == 0) popupMenu.addSeparator();
    if(grabbed == 1 && tabbing == 0 && copying == 0) popupMenu.addAction(Define_new_TabOrder());
    if(grabbed == 1 && tabbing == 0 && copying == 0) popupMenu.addAction(Edit_layout());
    if(grabbed == 1 && tabbing == 0 && copying == 0) popupMenu.addSeparator();
    if(grabbed == 1 && tabbing == 1)                 popupMenu.addAction(End_define_TabOrder());
    if((grabbed == 1 && tabbing == 1) || copying == 1) popupMenu.addSeparator();
    if(grabbed == 0) popupMenu.addAction(DGrabMouse());
    if(grabbed == 1) popupMenu.addAction(DReleaseMouse());
    setCursor(Qt::ArrowCursor);
    ret = popupMenu.exec(QCursor::pos());
    stackClear();
    if(ret == NULL)
    {
      grabbed = 1;
      grabMouse();
      mainWindow->grabKeyboard();
      setCursor(Qt::CrossCursor);
      return;
    }
    else
    {
      if(ret->text() == Properties() && child != NULL &&
         !child->statusTip().startsWith("TQWidget:"))
      {
        if(child != NULL)
        {
          clearSelectionFrames(); // Nestor: la selección ya no cambia la paleta
          dlgProperty dlg(child);
          releaseMouse();
          mainWindow->releaseKeyboard();
          dlg.run();
          opt_develop.ctrlPressed = 0;
        }
        modified = 1;
      }
      else if(ret->text() == Properties())
      {
      }
      else if(ret->text() == Insert_Widget())
      {
        QWidget *p = child;
        if(p == NULL && x>0 && y>0) p = this;
        if(p != NULL && parentLevel == 0)
        {
          clearSelectionFrames();
          releaseMouse();
          mainWindow->releaseKeyboard();
          setCursor(Qt::ArrowCursor);
          insert.myrootwidget = this;
          insert.run();
          if(insert.ret == 1)
          {
            int xw,yw;
            QPoint parent0 = p->mapToGlobal(QPoint(0,0));
            QPoint thisxy  = mapToGlobal(QPoint(x,y));
            xw = thisxy.x() - parent0.x();
            yw = thisxy.y() - parent0.y();
            if(opt_develop.arg_debug) printf("Insert Widget: pos(%d,%d)\n",xw,yw);
            QWidget *wnew = insert.newWidget(this,p,xw,yw);
            modified = 1;
            stackClear();
            perhapsDeselectOldWidget();
            selectWidget(wnew);
          }
          setCursor(Qt::CrossCursor);
        }
      }
      else if(ret->text() == Delete_Widget())
      {
        if(child != NULL && isSelected(child) && selection.size() > 1)
        {
          deleteLastChild(); // Nestor: borra toda la selección múltiple
        }
        else if(child != NULL)
        {
          deleteWidgets(QList<QWidget *>{child});
        }
      }
      else if(ret->text() == Add_Tab())
      {
        if(child != NULL)
        {
          bool ok;
          releaseMouse();
          mainWindow->releaseKeyboard();
          setCursor(Qt::ArrowCursor);
          QString text = QInputDialog::getText(this, "pvdevelop: Add Tab",
                                                     "Title of tab", QLineEdit::Normal,
                                                     "", &ok);
          if(ok && !text.isEmpty())
          {
            QWidget *tab = new QWidget();
            tab->setStatusTip("TQWidget:");
            insert.myrootwidget = this;
            insert.setDefaultObjectName(this,tab);
            ((MyQTabWidget *)child)->addTab(tab, text);
            stackClear();
          }
          setCursor(Qt::CrossCursor);
        }
      }
      else if(ret->text() == Add_Item())
      {
        if(child != NULL)
        {
          QString text;
          bool ok;
          releaseMouse();
          mainWindow->releaseKeyboard();
          setCursor(Qt::ArrowCursor);
          text = QInputDialog::getText(this, "pvdevelop: Add Item",
                                                     "Title of Item", QLineEdit::Normal,
                                                     "", &ok);
          if(ok && !text.isEmpty())
          {
            QWidget *item = new QWidget();
            item->setStatusTip("TQWidget:");
            insert.myrootwidget = this;
            insert.setDefaultObjectName(this,item);
            ((MyQToolBox *)child)->addItem(item, text);
            stackClear();
          }
          setCursor(Qt::CrossCursor);
        }
      }
      else if(ret->text() == DGrabMouse())
      {
        grabMouse();
        mainWindow->grabKeyboard();
        setCursor(Qt::SizeAllCursor);
      }
      else if(ret->text() == DReleaseMouse())
      {
        clearSelectionFrames();
        releaseMouse();
        mainWindow->releaseKeyboard();
        grabbed = 0;
        setCursor(Qt::ArrowCursor);
        return;
      }
      else if(ret->text() == Define_new_TabOrder())
      {
        tabbing = modified = 1;
        tablist.clear();
        lastTab = "";
        lastTabChild = NULL;
        releaseMouse();
        mainWindow->releaseKeyboard();
        QMessageBox::information(0,QCoreApplication::translate("Designer", "pvdevelop: define new TabOrder."),
                     QCoreApplication::translate("Designer", "Click on the widgets in the order you want to define\n"
                     "The widgets will behidden.\n"
                     "When you are finished with definition click right mouse button and end definition.\n"
                     "In order to switch TabWidget: release mouse and grab it again.\n"
                     "In order to switch ToolBox: release mouse and grab it again."),
                     QMessageBox::Ok);
        stackClear();
      }
      else if(ret->text() == End_define_TabOrder())
      {
        tabbing = 0;
        showWidgets(this);
        stackClear();
      }
      else if(ret->text() == Edit_layout())
      {
        releaseMouse();
        mainWindow->releaseKeyboard();
        setCursor(Qt::ArrowCursor);
        editlayout->exec();
        modified = 1;
        stackClear();
      }
      else
      {
        printf("unknown popup text=%s\n",(const char *) ret->text().toUtf8());
      }
      grabbed = 1;
      grabMouse();
      mainWindow->grabKeyboard();
      setCursor(Qt::ArrowCursor);
    }
  }
}

void MyRootWidget::mouseReleaseEvent(QMouseEvent *event)
{
  const auto endStep = qScopeGuard([this]() { endUndoStep(); });
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0) //Nestor aquí: Compatibilidad con QT6
    int x = (int) event->position().x();
    int y = (int) event->position().y();

    int gx = (int) event->globalPosition().x();
    int gy = (int) event->globalPosition().y();
#else
    int x = event->pos().x();
    int y = event->pos().y();

    int gx = event->globalPos().x();
    int gy = event->globalPos().y();
#endif

  if(opt_develop.arg_debug > 0) printf("mouseReleaseEvent x=%d y=%d\n",x,y);
  if(rubberBand != NULL && rubberBand->isVisible()) // Nestor: fin del rectángulo de selección
  {
    finishRubberBand();
    buttonDown = 0;
    clickedChild = NULL;
    return;
  }
  //#####################################
  if(aboveDesignArea(x,y,gx,gy))
  {
    grabbed = 1;
    grabMouse();
    mainWindow->grabKeyboard();
  }
  else
  {
    grabbed = 0;
    releaseMouse();
    mainWindow->releaseKeyboard();
    return;
  }
  //#####################################
  if(clickedChild != NULL && selection.size() > 1 && isSelected(clickedChild))
  {
    // Nestor: la selección múltiple se mueve dentro de su contenedor, sin cambiar de padre al soltar
    buttonDown = 0;
    clickedChild = NULL;
    return;
  }
  if(clickedChild != NULL && dragStarted)   // Nestor: un clic sin arrastre no cambia de padre ni lo acomoda a la grilla
  {
    clickedChild->hide();
    QWidget *item = childAt(x,y);
    if(item != NULL)
    {
      int xNew, yNew;
      push(clickedChild);
      QPoint parentPos0 = item->mapToGlobal(QPoint(0,0));
      QPoint childPos0  = clickedChild->mapToGlobal(QPoint(0,0));
      item->hide();
      clickedChild->setParent(item);
      xNew = childPos0.x() - parentPos0.x();
      yNew = childPos0.y() - parentPos0.y();
      xNew = (xNew/opt_develop.xGrid)*opt_develop.xGrid;
      yNew = (yNew/opt_develop.yGrid)*opt_develop.yGrid;
      if(xNew>=0 && yNew>=0) myMove(clickedChild,xNew,yNew);
      item->show();
      modified = 1;
    }
    clickedChild->show();
    modified = 1;
  }
  buttonDown = 0;
  clickedChild = NULL;
  if(opt_develop.arg_debug > 0) printf("mouseReleaseEvent end\n");
}

void MyRootWidget::showWidgets(QObject *item)
{
  QObject *subitem;
  QList<QObject *> itemlist;
  itemlist.clear();
  itemlist = item->children();
  for(int i=0; i<itemlist.size(); i++)
  {
    subitem = itemlist.at(i);
    if(subitem->isWidgetType() &&
      ((QWidget *) subitem)->statusTip().startsWith("TQ") &&
      !((QWidget *) subitem)->statusTip().startsWith("TQWidget:"))
    {
      ((QWidget *)subitem)->show();
    }
    showWidgets(subitem);
  }
}

void MyRootWidget::restoreColors(QWidget *item)
{
  QColor color;
  QString st = item->statusTip();
  st.remove("background:");
  st.remove("foreground:");
  int r,g,b;
  //item->unsetPalette();
  QPalette palette;
  item->setPalette(palette);
  if(savedStatusTip.contains("TQPushButton:") ||
     savedStatusTip.contains("TQRadio:")      ||
     savedStatusTip.contains("TQCheck:")      )
  {
    color = savedPalette.color(QPalette::ButtonText);
    r = color.red();
    g = color.green();
    b = color.blue();
    if(savedStatusTip.contains("foreground:"))
    {
      mySetForegroundColor(item,TQPushButton,r,g,b);
      st.append("foreground:");
    }
    color = savedPalette.color(QPalette::Button);
    r = color.red();
    g = color.green();
    b = color.blue();
    if(savedStatusTip.contains("background:"))
    {
      if(savedStatusTip.contains("TQPushButton:")) mySetBackgroundColor(item,TQPushButton,r,g,b);
      if(savedStatusTip.contains("TQRadio:"))      mySetBackgroundColor(item,TQRadio,r,g,b);
      if(savedStatusTip.contains("TQCheck:"))      mySetBackgroundColor(item,TQCheck,r,g,b);
      st.append("background:");
    }
  }
  else
  {
    color = savedPalette.color(QPalette::WindowText);
    r = color.red();
    g = color.green();
    b = color.blue();
    if(savedStatusTip.contains("foreground:"))
    {
      mySetForegroundColor(item,-1,r,g,b);
      st.append("foreground:");
    }
    color = savedPalette.color(QPalette::Window);
    r = color.red();
    g = color.green();
    b = color.blue();
    if(savedStatusTip.contains("background:"))
    {
      mySetBackgroundColor(item,-1,r,g,b);
      st.append("background:");
    }
  }
  item->setStatusTip(st);
}

void MyRootWidget::deleteLastChild()
{
  // Nestor: borra todos los widgets seleccionados
  deleteWidgets(selectedWidgets());
}

// Nestor: borra los widgets y los guarda en el formato de copiar/pegar: Ctrl+Z los vuelve a poner
// (QPointer: si uno estaba dentro de otro ya borrado, se salta)
void MyRootWidget::deleteWidgets(const QList<QWidget *> &widgets)
{
  if(widgets.isEmpty()) return;
  const QString text = copyWidgetsToText(this, widgets);
  QList<QPointer<QWidget> > victims, parents;
  for(int i=0; i<widgets.size(); i++) { victims.append(widgets.at(i)); parents.append(widgets.at(i)->parentWidget()); }
  clearSelectionFrames();
  selection.clear();
  lastChild = clickedChild = NULL;
  for(int i=0; i<victims.size(); i++)
  {
    if(!victims.at(i).isNull()) { delete victims.at(i).data(); modified = 1; }
  }
  stackClear();   // las posiciones guardadas pueden ser de los borrados
  deletedText = text;
  deletedParents = parents;
}

void MyRootWidget::showProperties()
{
  if(lastChild != NULL)
  {
    clearSelectionFrames();
    dlgProperty dlg(lastChild);
    setCursor(Qt::ArrowCursor);
    releaseMouse();
    mainWindow->releaseKeyboard();
    int ret = dlg.run();
    if(ret == 1) modified = 1;
  }
}

void MyRootWidget::GrabMouse()
{
  grabMouse();
  mainWindow->grabKeyboard();
  grabbed = 1;
  setCursor(Qt::CrossCursor);
}

void MyRootWidget::ReleaseMouse()
{
  clearSelectionFrames();
  releaseMouse();
  mainWindow->releaseKeyboard();
  grabbed = 0;
  setCursor(Qt::ArrowCursor);
}

void MyRootWidget::EditLayout()
{
  releaseMouse();
  mainWindow->releaseKeyboard();
  setCursor(Qt::ArrowCursor);
  editlayout->exec();
  modified = 1;
  GrabMouse();
}

void MyRootWidget::push(QWidget *current)
{
  OldState state;
  if(current == NULL) return;
  if(undoGrouping)
  {
    // Nestor: en un arrastre solo vale la posicion de antes de empezar
    for(int i=stack.size()-1; i>=0 && stack.at(i).step == undoStep; i--)
      if(stack.at(i).OldCurrent == current) return;
  }
  else undoStep++;

  state.step       = undoStep;
  state.OldX       = current->x();
  state.OldY       = current->y();
  state.OldWidth   = current->width();
  state.OldHeight  = current->height();
  state.OldParent  = (QWidget *) current->parent();
  state.OldCurrent = (QWidget *) current;
  stack.push(state);
}

QWidget *MyRootWidget::pop(QWidget *current)
{
  // current may be NULL
  OldState state;
  if(stack.isEmpty() && !deletedText.isEmpty())
  {
    // Nestor: deshacer el ultimo borrado: vuelven con los mismos nombres, en el mismo contenedor y lugar
    QList<QWidget *> parents;
    for(int i=0; i<deletedParents.size(); i++) if(!deletedParents.at(i).isNull()) parents.append(deletedParents.at(i).data());
    const QList<QWidget *> back = pasteWidgetsFromText(this, deletedText, parents, 1, QString(), QString(), true);
    deletedText.clear();
    deletedParents.clear();
    bool hasDraw = false;
    for(QWidget *w : back)
    {
      w->show();
      if(w->statusTip().startsWith("TQDraw:")) hasDraw = true;
      for(QWidget *c : w->findChildren<QWidget *>()) if(c->statusTip().startsWith("TQDraw:")) hasDraw = true;
    }
    if(hasDraw) drawDrawWidgets(this);
    if(back.isEmpty()) { qApp->beep(); return current; }
    clearSelection();
    for(QWidget *w : back) selection.append(w);
    refreshSelectionFrames();
    modified = 1;
    printStatusMessage(back.first());
    return back.first();
  }
  if(stack.isEmpty())
  {
    qApp->beep();
    return current;
  }
  // Nestor: vuelve todo el paso (un arrastre, una alineacion, una flecha con varios seleccionados)
  const int step = stack.top().step;
  while(!stack.isEmpty() && stack.top().step == step)
  {
    state = stack.pop();
    current = state.OldCurrent;
    if(current->parent() != state.OldParent)
    {
      state.OldParent->hide();
      current->setParent(state.OldParent);
      state.OldParent->show();
    }
    current->setGeometry(state.OldX, state.OldY, state.OldWidth, state.OldHeight);
  }
  printStatusMessage(current);
  return current;
}

void MyRootWidget::stackClear()
{
  stack.clear();
  deletedText.clear();
  deletedParents.clear();
}

// Nestor: copiar/pegar widgets con Ctrl+C / Ctrl+V en el diseñador

bool MyRootWidget::eventFilter(QObject *obj, QEvent *event)
{
    // El filtro está instalado en qApp: solo actuar si el diseñador está visible, con el mouse capturado
    // y sin estar definiendo TabOrder ni copiando atributos
    if (event->type() == QEvent::KeyPress && isVisible() && grabbed == 1 && tabbing == 0 && copying == 0) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        QWidget *focused = QApplication::focusWidget();
        if (focused && focused->isVisible() &&
                       (qobject_cast<QLineEdit *>(focused) ||
                        qobject_cast<QTextEdit *>(focused) ||
                        qobject_cast<QPlainTextEdit *>(focused))) {
            // Si el foco está en un campo de texto, no interceptar
            return false;
        }

        if ((keyEvent->modifiers() & Qt::ControlModifier) && !keyEvent->isAutoRepeat()) {
            if (keyEvent->key() == Qt::Key_C) {
                copyWidget();
                return true;  // Evento manejado
            }
            if (keyEvent->key() == Qt::Key_V) {
                pasteWidget();
                return true;
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

void MyRootWidget::copyWidget() {
    const QList<QWidget *> selected = selectedWidgets();
    if (selected.isEmpty()) {
        qDebug() << "No se ha seleccionado ningún widget para copiar.";
        return;
    }

    // Mismo formato que la máscara: incluye los widgets que contienen (frames, groupbox, pestañas...)
    QString text = copyWidgetsToText(this, selected);
    if (text.isEmpty()) {
        qDebug() << "No se pudo copiar la selección.";
        return;
    }
    QApplication::clipboard()->setText(text);
    copiedParents.clear();
    for (int i = 0; i < selected.size(); i++) copiedParents.append(selected.at(i)->parentWidget());
    pasteCount = 0;

    if(opt_develop.arg_debug) qDebug() << "Widgets copiados:" << selected.size();
}

void MyRootWidget::pasteWidget() {
    QString text = QApplication::clipboard()->text();
    if (!isWidgetClipboardText(text)) {
        if(opt_develop.arg_debug) qDebug() << "El portapapeles no contiene un widget de pvdevelop.";
        return;
    }

    // Los widgets pegados con nombre por defecto reciben el siguiente objN libre
    int max = 0;
    insert.findMax(this, &max);

    QList<QWidget *> parents;
    for (int i = 0; i < copiedParents.size(); i++) {
        if (!copiedParents.at(i).isNull()) parents.append(copiedParents.at(i).data());
    }

    perhapsDeselectOldWidget();
    const QList<QWidget *> pasted = pasteWidgetsFromText(this, text, parents, max + 1);
    if (pasted.isEmpty()) {
        qDebug() << "No se pudo pegar el widget.";
        return;
    }

    // Desplazados respecto del original; cada pegado sucesivo un poco más
    pasteCount++;
    bool hasDraw = false;
    for (int i = 0; i < pasted.size(); i++) {
        QWidget *w = pasted.at(i);
        int x = ((w->x() + 20*pasteCount) / opt_develop.xGrid) * opt_develop.xGrid;
        int y = ((w->y() + 20*pasteCount) / opt_develop.yGrid) * opt_develop.yGrid;
        // Nestor: dentro de un contenedor (frame, groupbox, pestana) no puede quedar afuera: no se veria
        QWidget *pw = w->parentWidget();
        if (pw != NULL && pw != this) {
            if (x + w->width()  > pw->width())  x = qMax(0, pw->width()  - w->width());
            if (y + w->height() > pw->height()) y = qMax(0, pw->height() - w->height());
        }
        myMove(w, x, y);
        w->show();
        if (w->statusTip().startsWith("TQDraw:")) hasDraw = true;
        const QList<QWidget *> children = w->findChildren<QWidget *>();
        for (int k = 0; k < children.size(); k++) {
            if (children.at(k)->statusTip().startsWith("TQDraw:")) hasDraw = true;
        }
    }
    // los QDraw pegados se dibujan desde su archivo, igual que al cargar la máscara
    if (hasDraw) drawDrawWidgets(this);

    stackClear();   // igual que al insertar: no se puede deshacer hacia un estado sin estos widgets

    // queda seleccionado lo pegado (el primero es la referencia para alinear)
    clearSelection();
    for (int i = 0; i < pasted.size(); i++) selection.append(pasted.at(i));
    refreshSelectionFrames();
    modified = 1;
    printStatusMessage(lastChild);

    if(opt_develop.arg_debug) qDebug() << "Widgets pegados:" << pasted.size();
}

// Nestor: alinear / espaciar la selección respecto del primer widget seleccionado.
// Se trabaja en coordenadas de la máscara (this), así funciona aunque estén en contenedores distintos.
void MyRootWidget::alignSelection(int mode)
{
    const QList<QWidget *> ws = selectedWidgets();
    const bool spacing = (mode == SpaceHorizontally || mode == SpaceVertically);
    if (ws.size() < (spacing ? 3 : 2)) {
        if (statusBar != NULL) {
            statusBar->showMessage(spacing ? tr("Select at least 3 widgets (Ctrl+click or drag a rectangle)")
                                           : tr("Select at least 2 widgets (Ctrl+click or drag a rectangle)"), 4000);
        }
        qApp->beep();
        return;
    }

    beginUndoStep();   // Nestor: la alineacion entera es un solo Ctrl+Z
    const auto endStep = qScopeGuard([this]() { endUndoStep(); });
    struct Item { QWidget *w; QRect r; };
    QList<Item> items;
    for (int i = 0; i < ws.size(); i++) {
        Item it;
        it.w = ws.at(i);
        it.r = QRect(it.w->mapTo(this, QPoint(0, 0)), it.w->size());
        items.append(it);
    }

    // mueve w a la posición pos (coordenadas de la máscara)
    auto moveTo = [this](QWidget *w, const QPoint &pos) {
        QWidget *parent = w->parentWidget();
        const QPoint p = (parent != NULL) ? parent->mapFrom(this, pos) : pos;
        if (p == w->pos()) return;
        push(w);
        myMove(w, p.x(), p.y());
    };

    const QRect ref = items.first().r;
    if (!spacing) {
        for (int i = 1; i < items.size(); i++) {
            const QRect r = items.at(i).r;
            QPoint pos = r.topLeft();
            if      (mode == AlignLeftEdges)   pos.setX(ref.x());
            else if (mode == AlignRightEdges)  pos.setX(ref.x() + ref.width() - r.width());
            else if (mode == AlignTopEdges)    pos.setY(ref.y());
            else if (mode == AlignBottomEdges) pos.setY(ref.y() + ref.height() - r.height());
            else if (mode == AlignHCenters)    pos.setX(ref.x() + (ref.width() - r.width()) / 2);   // Nestor: mismo centro horizontal
            else if (mode == AlignVCenters)    pos.setY(ref.y() + (ref.height() - r.height()) / 2); // Nestor: mismo centro vertical
            moveTo(items.at(i).w, pos);
        }
    } else {
        // los de los extremos quedan fijos; los del medio se reparten con el mismo espacio entre ellos
        const bool horizontal = (mode == SpaceHorizontally);
        std::sort(items.begin(), items.end(), [horizontal](const Item &a, const Item &b) {
            return horizontal ? a.r.x() < b.r.x() : a.r.y() < b.r.y();
        });
        const Item &first = items.first();
        const Item &last  = items.last();
        int total = 0;
        for (int i = 0; i < items.size(); i++) total += horizontal ? items.at(i).r.width() : items.at(i).r.height();
        const int span = horizontal ? (last.r.x() + last.r.width()) - first.r.x()
                                    : (last.r.y() + last.r.height()) - first.r.y();
        const double gap = double(span - total) / double(items.size() - 1);
        double pos = horizontal ? first.r.x() + first.r.width() : first.r.y() + first.r.height();
        for (int i = 1; i < items.size() - 1; i++) {
            pos += gap;
            const QRect r = items.at(i).r;
            const int p = qRound(pos);
            moveTo(items.at(i).w, horizontal ? QPoint(p, r.y()) : QPoint(r.x(), p));
            pos += horizontal ? r.width() : r.height();
        }
    }
    modified = 1;
    printStatusMessage(lastChild);
}
