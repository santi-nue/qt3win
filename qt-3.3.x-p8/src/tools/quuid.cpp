/****************************************************************************
** $Id: quuid.cpp 2051 2007-02-21 10:04:20Z chehrlic $
**
** Implementation of QUuid class
**
** Copyright (C) 2000-2007 Trolltech ASA.  All rights reserved.
**
** This file is part of the tools module of the Qt GUI Toolkit.
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

#include "quuid.h"

#include "qdatastream.h"

/*!
    \class QUuid quuid.h
    \brief The QUuid class defines a Universally Unique Identifier (UUID).

    \reentrant

    For objects or declarations that must be uniquely identified,
    UUIDs (also known as GUIDs) are widely used in order to assign a
    fixed and easy to compare value to the object or declaration. The
    128-bit value of a UUID is generated by an algorithm that
    guarantees that the value is unique.

    In Qt, UUIDs are wrapped by the QUuid struct which provides
    convenience functions for handling UUIDs. Most platforms provide a
    tool to generate new UUIDs, for example, uuidgen and guidgen.

    UUIDs generated by QUuid, are based on the \c Random version of the
    \c DCE (Distributed Computing Environment) standard.

    UUIDs can be constructed from numeric values or from strings, or
    using the static createUuid() function. They can be converted to a
    string with toString(). UUIDs have a variant() and a version(),
    and null UUIDs return TRUE from isNull().
*/

/*!
    \fn QUuid::QUuid()

    Creates the null UUID {00000000-0000-0000-0000-000000000000}.
*/

/*!
    \fn QUuid::QUuid( uint l, ushort w1, ushort w2, uchar b1, uchar b2, uchar b3, uchar b4, uchar b5, uchar b6, uchar b7, uchar b8 )

    Creates a UUID with the value specified by the parameters, \a l,
    \a w1, \a w2, \a b1, \a b2, \a b3, \a b4, \a b5, \a b6, \a b7, \a
    b8.

    Example:
    \code
    // {67C8770B-44F1-410A-AB9A-F9B5446F13EE}
    QUuid IID_MyInterface( 0x67c8770b, 0x44f1, 0x410a, 0xab, 0x9a, 0xf9, 0xb5, 0x44, 0x6f, 0x13, 0xee )
    \endcode
*/

/*!
    \fn QUuid::QUuid( const QUuid &orig )

    Creates a copy of the QUuid \a orig.
*/
#ifndef QT_NO_QUUID_STRING
/*!
    Creates a QUuid object from the string \a text. The function can
    only convert a string in the format
    {HHHHHHHH-HHHH-HHHH-HHHH-HHHHHHHHHHHH} (where 'H' stands for a hex
    digit). If the conversion fails a null UUID is created.
*/
QUuid::QUuid( const QString &text )
{
    bool ok;
    if ( text.isEmpty() ) {
	*this = QUuid();
	return;
    }
    QString temp = text.upper();
    if ( temp[0] != '{' )
	temp = "{" + text;
    if ( text[(int)text.length()-1] != '}' )
	temp += "}";

    data1 = temp.mid( 1, 8 ).toULong( &ok, 16 );
    if ( !ok ) {
	*this = QUuid();
	return;
    }

    data2 = temp.mid( 10, 4 ).toUInt( &ok, 16 );
    if ( !ok ) {
	*this = QUuid();
	return;
    }
    data3 = temp.mid( 15, 4 ).toUInt( &ok, 16 );
    if ( !ok ) {
	*this = QUuid();
	return;
    }
    data4[0] = temp.mid( 20, 2 ).toUInt( &ok, 16 );
    if ( !ok ) {
	*this = QUuid();
	return;
    }
    data4[1] = temp.mid( 22, 2 ).toUInt( &ok, 16 );
    if ( !ok ) {
	*this = QUuid();
	return;
    }
    for ( int i = 2; i<8; i++ ) {
	data4[i] = temp.mid( 25 + (i-2)*2, 2 ).toUShort( &ok, 16 );
	if ( !ok ) {
	    *this = QUuid();
	    return;
	}
    }
}

/*!
    \internal
*/
QUuid::QUuid( const char *text )
{
    *this = QUuid( QString(text) );
}
#endif
/*!
    \fn QUuid QUuid::operator=(const QUuid &uuid )

    Assigns the value of \a uuid to this QUuid object.
*/

/*!
    \fn bool QUuid::operator==(const QUuid &other) const

    Returns TRUE if this QUuid and the \a other QUuid are identical;
    otherwise returns FALSE.
*/

/*!
    \fn bool QUuid::operator!=(const QUuid &other) const

    Returns TRUE if this QUuid and the \a other QUuid are different;
    otherwise returns FALSE.
*/
#ifndef QT_NO_QUUID_STRING
/*!
    \fn QUuid::operator QString() const

    Returns the string representation of the uuid.

    \sa toString()
*/

/*!
    QString QUuid::toString() const

    Returns the string representation of the uuid.
*/
QString QUuid::toString() const
{
    QString result;

    result = "{" + QString::number( data1, 16 ).rightJustify( 8, '0' ) + "-";
    result += QString::number( (int)data2, 16 ).rightJustify( 4, '0' ) + "-";
    result += QString::number( (int)data3, 16 ).rightJustify( 4, '0' ) + "-";
    result += QString::number( (int)data4[0], 16 ).rightJustify( 2, '0' );
    result += QString::number( (int)data4[1], 16 ).rightJustify( 2, '0' ) + "-";
    for ( int i = 2; i < 8; i++ )
	result += QString::number( (int)data4[i], 16 ).rightJustify( 2, '0' );

    return result + "}";
}
#endif

#ifndef QT_NO_DATASTREAM
/*!
    \relates QUuid
    Writes the uuid \a id to the datastream \a s.
*/
QDataStream &operator<<( QDataStream &s, const QUuid &id )
{
    s << (Q_UINT32)id.data1;
    s << (Q_UINT16)id.data2;
    s << (Q_UINT16)id.data3;
    for (int i = 0; i < 8; i++ )
	s << (Q_UINT8)id.data4[i];
    return s;
}

/*!
    \relates QUuid
    Reads uuid from from the stream \a s into \a id.
*/
QDataStream &operator>>( QDataStream &s, QUuid &id )
{
    Q_UINT32 u32;
    Q_UINT16 u16;
    Q_UINT8 u8;
    s >> u32;
    id.data1 = u32;
    s >> u16;
    id.data2 = u16;
    s >> u16;
    id.data3 = u16;
    for (int i = 0; i < 8; i++ ) {
	s >> u8;
	id.data4[i] = u8;
    }
    return s;
}
#endif

/*!
    Returns TRUE if this is the null UUID
    {00000000-0000-0000-0000-000000000000}; otherwise returns FALSE.
*/
bool QUuid::isNull() const
{
    return data4[0] == 0 && data4[1] == 0 && data4[2] == 0 && data4[3] == 0 &&
	   data4[4] == 0 && data4[5] == 0 && data4[6] == 0 && data4[7] == 0 &&
	   data1 == 0 && data2 == 0 && data3 == 0;
}

/*!
    \enum QUuid::Variant

    This enum defines the variant of the UUID, which is the scheme
    which defines the layout of the 128-bits value.

    \value VarUnknown Variant is unknown
    \value NCS Reserved for NCS (Network Computing System) backward compatibility
    \value DCE Distributed Computing Environment, the scheme used by QUuid
    \value Microsoft Reserved for Microsoft backward compatibility (GUID)
    \value Reserved Reserved for future definition
*/

/*!
    \enum QUuid::Version

    This enum defines the version of the UUID.

    \value VerUnknown Version is unknown
    \value Time Time-based, by using timestamp, clock sequence, and
    MAC network card address (if available) for the node sections
    \value EmbeddedPOSIX DCE Security version, with embedded POSIX UUIDs
    \value Name Name-based, by using values from a name for all sections
    \value Random Random-based, by using random numbers for all sections
*/

/*!
    \fn QUuid::Variant QUuid::variant() const

    Returns the variant of the UUID.
    The null UUID is considered to be of an unknown variant.

    \sa version()
*/
QUuid::Variant QUuid::variant() const
{
    if ( isNull() )
	return VarUnknown;
    // Check the 3 MSB of data4[0]
    if ( (data4[0] & 0x80) == 0x00 ) return NCS;
    else if ( (data4[0] & 0xC0) == 0x80 ) return DCE;
    else if ( (data4[0] & 0xE0) == 0xC0 ) return Microsoft;
    else if ( (data4[0] & 0xE0) == 0xE0 ) return Reserved;
    return VarUnknown;
}

/*!
    \fn QUuid::Version QUuid::version() const

    Returns the version of the UUID, if the UUID is of the DCE
    variant; otherwise returns VerUnknown.

    \sa variant()
*/
QUuid::Version QUuid::version() const
{
    // Check the 4 MSB of data3
    Version ver = (Version)(data3>>12);
    if ( isNull() 
	 || (variant() != DCE)
	 || ver < Time 
	 || ver > Random )
	return VerUnknown;
    return ver;
}

/*!
    \fn bool QUuid::operator<(const QUuid &other) const

    Returns TRUE if this QUuid is of the same variant,
    and lexicographically before the \a other QUuid;
    otherwise returns FALSE.

    \sa variant()
*/
#define ISLESS(f1, f2) if (f1!=f2) return (f1<f2);
bool QUuid::operator<(const QUuid &other ) const
{
    if ( variant() != other.variant() )
	return FALSE;

    ISLESS( data1, other.data1 );
    ISLESS( data2, other.data2 );
    ISLESS( data3, other.data3 );
    for ( int n = 0; n < 8; n++ ) {
	ISLESS( data4[n], other.data4[n] );
    }
    return FALSE;
}

/*!
    \fn bool QUuid::operator>(const QUuid &other) const

    Returns TRUE if this QUuid is of the same variant,
    and lexicographically after the \a other QUuid;
    otherwise returns FALSE.

    \sa variant()
*/
#define ISMORE(f1, f2) if (f1!=f2) return (f1>f2);
bool QUuid::operator>(const QUuid &other ) const
{
    if ( variant() != other.variant() )
	return FALSE;

    ISMORE( data1, other.data1 );
    ISMORE( data2, other.data2 );
    ISMORE( data3, other.data3 );
    for ( int n = 0; n < 8; n++ ) {
	ISMORE( data4[n], other.data4[n] );
    }
    return FALSE;
}

/*!
    \fn QUuid QUuid::createUuid()

    Returns a new UUID of \c DCE variant, and \c Random type. The
    UUIDs generated are based on the platform specific pseudo-random
    generator, which is usually not a cryptographic-quality random
    number generator. Therefore, a UUID is not guaranteed to be unique
    cross application instances.

    On Windows, the new UUID is extremely likely to be unique on the
    same or any other system, networked or not.

    \sa variant(), version()
*/
#if defined(Q_OS_WIN32)
#include <objbase.h> // For CoCreateGuid
QUuid QUuid::createUuid()
{
    GUID guid;
    CoCreateGuid( &guid );
    QUuid result = guid;
    return result;
}
#else // !Q_OS_WIN32
#include "qdatetime.h"
#include "stdlib.h" // For srand/rand
QUuid QUuid::createUuid()
{
    static const int intbits = sizeof(int)*8;
    static int randbits = 0;
    if ( !randbits ) {
	int max = RAND_MAX;
	do { ++randbits; } while ( (max=max>>1) );
	srand( (uint)QDateTime::currentDateTime().toTime_t() );
	rand(); // Skip first
    }

    QUuid result;
    uint *data = &(result.data1);
    int chunks = 16 / sizeof(uint);
    while ( chunks-- ) {
	uint randNumber = 0;
	for ( int filled = 0; filled < intbits; filled += randbits )
	    randNumber |= rand()<<filled;
	 *(data+chunks) = randNumber;
    }

    result.data4[0] = (result.data4[0] & 0x3F) | 0x80;	// UV_DCE
    result.data3 = (result.data3 & 0x0FFF) | 0x4000;	// UV_Random

    return result;
}
#endif // !Q_OS_WIN32

