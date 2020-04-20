/**********************************************************************
** $Id: qgroupbox.h 2051 2007-02-21 10:04:20Z chehrlic $
**
** Definition of QGroupBox widget class
**
** Created : 950203
**
** Copyright (C) 1992-2007 Trolltech ASA.  All rights reserved.
**
** This file is part of the widgets module of the Qt GUI Toolkit.
**
** This file may be distributed under the terms of the Q Public License
** as defined by Trolltech ASA of Norway and appearing in the file
** LICENSE.QPL included in the packaging of this file.
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.
**
** Licensees holding valid Qt Enterprise Edition or Qt Professional Edition
** licenses may use this file in accordance with the Qt Commercial License
** Agreement provided with the Software.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://www.trolltech.com/pricing.html or email sales@trolltech.com for
**   information about Qt Commercial License Agreements.
** See http://www.trolltech.com/qpl/ for QPL licensing information.
** See http://www.trolltech.com/gpl/ for GPL licensing information.
**
** Contact info@trolltech.com if any conditions of this licensing are
** not clear to you.
**
**********************************************************************/

#ifndef QGROUPBOX_H
#define QGROUPBOX_H

#ifndef QT_H
#include "qframe.h"
#endif // QT_H

#ifndef QT_NO_GROUPBOX


class QAccel;
class QGroupBoxPrivate;
class QVBoxLayout;
class QGridLayout;
class QSpacerItem;

class Q_EXPORT QGroupBox : public QFrame
{
    Q_OBJECT
    Q_PROPERTY( QString title READ title WRITE setTitle )
    Q_PROPERTY( Alignment alignment READ alignment WRITE setAlignment )
    Q_PROPERTY( Orientation orientation READ orientation WRITE setOrientation DESIGNABLE false )
    Q_PROPERTY( int columns READ columns WRITE setColumns DESIGNABLE false )
    Q_PROPERTY( bool flat READ isFlat WRITE setFlat )
#ifndef QT_NO_CHECKBOX
    Q_PROPERTY( bool checkable READ isCheckable WRITE setCheckable )
    Q_PROPERTY( bool checked READ isChecked WRITE setChecked )
#endif
public:
    QGroupBox( QWidget* parent=0, const char* name=0 );
    QGroupBox( const QString &title,
	       QWidget* parent=0, const char* name=0 );
    QGroupBox( int strips, Orientation o,
	       QWidget* parent=0, const char* name=0 );
    QGroupBox( int strips, Orientation o, const QString &title,
	       QWidget* parent=0, const char* name=0 );
    ~QGroupBox();

    virtual void setColumnLayout(int strips, Orientation o);

    QString title() const { return str; }
    virtual void setTitle( const QString &);

    int alignment() const { return align; }
    virtual void setAlignment( int );

    int columns() const;
    void setColumns( int );

    Orientation orientation() const { return dir; }
    void setOrientation( Orientation );

    int insideMargin() const;
    int insideSpacing() const;
    void setInsideMargin( int m );
    void setInsideSpacing( int s );

    void addSpace( int );
    QSize sizeHint() const;

    bool isFlat() const;
    void setFlat( bool b );
    bool isCheckable() const;
#ifndef QT_NO_CHECKBOX
    void setCheckable( bool b );
#endif
    bool isChecked() const;
    void setEnabled(bool on);

#ifndef QT_NO_CHECKBOX
public slots:
    void setChecked( bool b );

signals:
    void toggled( bool );
#endif
protected:
    bool event( QEvent * );
    void childEvent( QChildEvent * );
    void resizeEvent( QResizeEvent * );
    void paintEvent( QPaintEvent * );
    void focusInEvent( QFocusEvent * );
    void fontChange( const QFont & );

private slots:
    void fixFocus();
    void setChildrenEnabled( bool b );

private:
    void skip();
    void init();
    void calculateFrame();
    void insertWid( QWidget* );
    void setTextSpacer();
#ifndef QT_NO_CHECKBOX
    void updateCheckBoxGeometry();
#endif
    QString str;
    int align;
    int lenvisible;
#ifndef QT_NO_ACCEL
    QAccel * accel;
#endif
    QGroupBoxPrivate * d;

    QVBoxLayout *vbox;
    QGridLayout *grid;
    int row;
    int col : 30;
    uint bFlat : 1;
    int nRows, nCols;
    Orientation dir;
    int spac, marg;

private:	// Disabled copy constructor and operator=
#if defined(Q_DISABLE_COPY)
    QGroupBox( const QGroupBox & );
    QGroupBox &operator=( const QGroupBox & );
#endif
};


#endif // QT_NO_GROUPBOX

#endif // QGROUPBOX_H
