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
#ifndef DESIGNER_H
#define DESIGNER_H
#include <QString>
#include <QStatusBar>
#include <QStack>
#include <QPointer>
#include <QList>
#include <QRubberBand>
#include <qwidget.h>
#include "dlginsertwidget.h"

typedef struct
{
  QWidget *OldParent;
  QWidget *OldCurrent;
  int     OldX, OldY, OldWidth, OldHeight;
}OldState;

class QScrollArea;

class MyRootWidget : public QWidget
{
  public:
    void copyWidget();    // Nestor: tambien desde los iconos Copiar y Pegar
    void pasteWidget();
    MyRootWidget(MyRootWidget *parent);
    ~MyRootWidget();
    void setScroll(QScrollArea *_scroll);
    QStatusBar *statusBar;
    int modified;
    void deleteLastChild();
    void showProperties();
    void GrabMouse();
    void ReleaseMouse();
    void EditLayout();
    void MoveKey(int key);
    // Nestor: alinear y espaciar la selección múltiple (barra Align de la ventana principal)
    enum AlignMode { AlignLeftEdges, AlignRightEdges, AlignTopEdges, AlignBottomEdges, SpaceHorizontally, SpaceVertically, AlignHCenters, AlignVCenters };
    void alignSelection(int mode);
    void stackClear();
    QWidget *pop(QWidget *current);
    QMainWindow  *mainWindow;
  private:
    void mouseDoubleClickEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);
    QWidget *getChild(int x, int y);
    void showWidgets(QObject *item);
    void restoreColors(QWidget *item);
    int  aboveDesignArea(int x, int y, int gx, int gy);
    void printStatusMessage(QWidget *child);
    void setCursor(Qt::CursorShape cursor);
    void push(QWidget *current);
    void perhapsDeselectOldWidget();
    void clearSelectionFrame(QWidget *w);
    void clearSelectionFrames();
    void setSelectionFrame(QWidget *w, bool primary);
    void refreshSelectionFrames();
    void clearSelection();
    void selectWidget(QWidget *child);
    void toggleSelection(QWidget *child);
    bool isSelected(QWidget *w);
    QList<QWidget *> selectedWidgets();
    void startRubberBand(QWidget *container, int x, int y);
    void finishRubberBand();
    void moveKeyWidget(QWidget *child, int key);
    Qt::CursorShape currentCursor;
    QScrollArea *scroll;
    int xOld,yOld,buttonDown,xChild0,yChild0,wChild0,hChild0,grabbed,tabbing,copying, inMiddle,parentLevel,reparentDone;
    QWidget *clickedChild, *lastChild, *lastTabChild, *parentLevel0, *parentLevel1, *parentLevel2;
    QPalette savedPalette;
    QString savedStatusTip;
    QString lastTab;
    dlgInsertWidget insert;
    QStack<OldState> stack;
    QList<QPointer<QWidget> > copiedParents; // padres de los widgets copiados (se pega en el mismo contenedor)
    QList<QPointer<QWidget> > selection;     // selección múltiple; selection[0] es la referencia para alinear
    QList<QPoint> groupStart;                // posiciones al empezar a arrastrar la selección
    bool groupPushed = false;                // ya se guardó el arrastre del grupo para deshacer
    bool dragStarted = false;                // el mouse se alejo del clic mas que startDragDistance: recien ahi se mueve o cambia el tamano
    QRubberBand *rubberBand = nullptr;       // rectángulo de selección
    QPoint rubberOrigin;
    QPointer<QWidget> rubberContainer;       // contenedor donde empezó el rectángulo
    int pasteCount = 0;              // pegados desde la última copia, para desplazar cada copia
};

class Designer
{
  public:
    Designer(const char *mask);
    ~Designer();
    MyRootWidget *root;
  private:
    QString filename;
    int xclick, yclick;
    int imask;
};

#endif
