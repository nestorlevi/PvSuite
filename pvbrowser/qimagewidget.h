/***************************************************************************
                          QImageWidget.h  -  description
                             -------------------
    begin                : Fri Nov 17 2000
    copyright            : (C) 2000 by R. Lehrig
    email                : lehrig@t-online.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef _QIMAGE_WIDGET_H_
#define _QIMAGE_WIDGET_H_

#include <qwidget.h>
#include <qimage.h>
#include <qbitmap.h>
//Added by qt3to4:
#include <QPaintEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QList>
#include <QStringList>
class QTimer;
class QMovie;

class QImageWidget : public QWidget
{
    Q_OBJECT
public:
    QImageWidget(int *sock, int ident, QWidget *parent=0, const char *name=0, int wFlags=0 );
    ~QImageWidget();
    virtual void    setGeometry(int nx, int ny, int nw, int nh);
    void            setImage(const QImage *newimage);
    void            setImage(const char *filename, int rotate=0);
    void            setJpegImage(unsigned char *buffer, int buffersize, int rotate);
    void            setRGBA(unsigned char *buffer, int width, int height, int rotate);
    int             sendJpeg2clipboard();
    QImage          image; // the loaded image
    QImage          original_image; // the original loaded image
    void            scale(int w, int h);
    void            setAnimation(const QStringList &files, int periodMs);   // Nestor: cuadros cada periodMs (0 = detener)
    void            setZoom(double zx, double zy);  // Nestor: zoom de la mascara (1.0 = 100 %), lo usa scale()

protected:
    virtual void paintEvent(QPaintEvent *);
    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
            void enterEvent(QEnterEvent *event);
    virtual void leaveEvent(QEvent *event);
    virtual void resizeEvent(QResizeEvent *event);

private:
    void perhapsSetMask();
    int conversion_flags;
    int xx,yy,w,h,*s,id;
    double zoomX, zoomY;  // Nestor: zoom de la mascara
    QBitmap mask;
    void stopAnimation();
    void showFrame(const QImage &frame);
    QTimer *animTimer;
    QList<QImage> frames;
    int frameIndex;
    QMovie *movie;                        // gif animado
};


#endif
