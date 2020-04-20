/**********************************************************************
** Copyright (C) 2000-2007 Trolltech ASA.  All rights reserved.
**
** This file is part of Qt Designer.
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
** See http://www.trolltech.com/gpl/ for GPL licensing information.
** See http://www.trolltech.com/pricing.html or email sales@trolltech.com for
**   information about Qt Commercial License Agreements.
**
** Contact info@trolltech.com if any conditions of this licensing are
** not clear to you.
**
**********************************************************************/

#include "ui2uib.h"
#include "uib.h"

#include <domtool.h>

#include <qcolor.h>
#include <qcursor.h>
#include <qdatetime.h>
#include <qdom.h>
#include <qfile.h>
#include <qfont.h>
#include <qobject.h>
#include <qrect.h>
#include <qsizepolicy.h>

/*
    The .uib file format is the binary counterpart of the .ui file
    format. It is generated by the ui2uib converter and understood by
    QWidgetFactory; in a future version, it might also be understood
    by a uib2ui converter. Experiments show that .uib files are about
    2.5 times faster to load and 6 times smaller than .ui files.

    The .uib format, unlike the .ui format, is internal to Trolltech
    and is not officially documented; it is assumed that anybody who
    needs to understand the file format can read the ui2uib and
    QWidgetFactory source code, with some guidance. And here's some
    guidance.

    A .uib file starts with a 32-bit magic number that allows
    QWidgetFactory to determine the file type. The magic number is
    followed by '\n' (0x0a) and '\r' (0x0d), which ensure that the
    file wasn't corrupted during transfer between different
    platforms. For example, transferring a .ui file from Windows to
    Unix using FTP with type ASCII will produce a file with '\r\n\r'
    in place of '\n\r'. This is followed by the QDataStream format
    version number used.

    The rest of the file is made up of blocks, each of which starts
    with a block type (Block_XXX) and a block length. Block_Intro and
    Block_Widget are mandatory; the others are optional.
    QWidgetFactory makes certain assumptions about the order of the
    blocks; for example, it expects Block_String before any other
    block that refers to a string and Block_Images before
    Block_Widget. The order generated by ui2uib is one of the orders
    that make sense. Just after the last block, a Block_End marker
    indicates the end of the file.

    The division of .uib files into blocks corresponds grossly to the
    division of .ui files in top-level XML elements. Thus,
    Block_Widget corresponds to <widget> and Block_Toolbars to
    <toolbars>. The internal organization of each block also mimics
    the organization of the corresponding XML elements.

    These are the major differences, all of which contribute to
    making .uib files more compact:

    1.  The strings are gathered in Block_Strings, a string-table.
	When a string is needed later, it is referenced by a 32-bit
	index into that table. The UicStringTable class makes the
	whole process of inserting and looking up strings very
	simple. The advantage of this scheme is that if a string is
	used more than once, it is stored only once. Also, the
	string-table is preinitialized with very common strings, so
	that these need not be stored along with .uib files.

    2.  QObjects are referred to by index in a table rather than by
	name. The table itself is not stored in the .uib file; it is
	rather build dynamically by ui2uib and QWidgetFactory as new
	objects are specified. In ui2uib, the table is represented by
	a UibIndexMap object; in QWidgetFactory, a plain array of
	QObject pointers suffices.

    3.  The data is packed to take as little place as possible,
	without slowing down QLayoutFactory too much. For example, an
	index into the string-table is a 32-bit integer, but in
	practice it is rarely above 65534, so only 16 bits are used
	for them; when an index above 65534 is met, the index is
	saved as 65535 followed by the 32-bit index, for a total of
	48 bits.

    4.  The name of a signal or slot and its signature are saved
	separately. That way, if a signal 'foo(const QString&)' is
	connected to a slot 'bar(const QString&)', the string-table
	will only contain 'foo', 'bar', and '(const QString&)',
	instead of the longer 'foo(const QString&)' and 'bar(const
	QString&)'. The signatures are normalized beforehand to
	ensure that trivial spacing problems don't result in multiple
	string-table entries.

    5.  In a signal-to-slot connection, a sender, signal, receiver,
	or slot is not repeated if it's the same as for the previous
	connection. Bit flags indicate what is repeated and what is
	specified.

    6.  Some of the information stored in a .ui file is useful only
	by uic, not to QLayoutFactory. That information is, for now,
	not taken along in the .uib file. Likewise, needless
	QLayoutWidget objects are not taken along.

    The arbitrary constants related to the .uib file formats are
    defined in uib.h. Constants such as Block_Actions and
    Object_SubWidget are given values such as 'A' and 'W' to make
    .uib files easier to read in a hexadecimal editor.

    The file format isn't designed to be extensible. Any extension
    that prevents an older version of QLayoutWidget of reading the
    file correctly must have a different magic number. The plan is to
    use UibMagic + 1 for version 2, UibMagic + 2 for version 3, etc.
*/

static QCString layoutForTag( const QString& tag )
{
    if ( tag == "grid" ) {
	return "QGridLayout";
    } else if ( tag == "hbox" ) {
	return "QHBoxLayout";
    } else if ( tag == "vbox" ) {
	return "QVBoxLayout";
    } else {
	return "QLayout";
    }
}

class UibHack : public QObject
{
public:
    static QString normalize( const QString& member ) {
	return QString::fromUtf8( QObject::normalizeSignalSlot(member.utf8()) );
    }
};

class UibIndexMap
{
public:
    UibIndexMap() : next( 0 ) { }

    void insert( const QString& name ) { setName( insert(), name ); }
    int insert() { return next++; }
    void setName( int no, const QString& name );

    int find( const QString& name, int deflt = -1 ) const;
    int count() const { return next; }

private:
    QMap<QString, int> nameMap;
    QMap<QString, int> conflicts;
    int next;
};

void UibIndexMap::setName( int no, const QString& name )
{
    if ( !name.isEmpty() ) {
	if ( *nameMap.insert(name, no, FALSE) != no )
	    conflicts.insert( name, 0 );
    }
}

int UibIndexMap::find( const QString& name, int deflt ) const
{
    QMap<QString, int>::ConstIterator no = nameMap.find( name );
    if ( no == nameMap.end() || conflicts.contains(name) ) {
	return deflt;
    } else {
	return *no;
    }
}

static void packUInt16( QDataStream& out, Q_UINT16 n )
{
    if ( n < 255 ) {
	out << (Q_UINT8) n;
    } else {
	out << (Q_UINT8) 255;
	out << n;
    }
}

static void packUInt32( QDataStream& out, Q_UINT32 n )
{
    if ( n < 65535 ) {
	out << (Q_UINT16) n;
    } else {
	out << (Q_UINT16) 65535;
	out << n;
    }
}

static void packByteArray( QDataStream& out, const QByteArray& array )
{
    packUInt32( out, array.size() );
    out.writeRawBytes( array.data(), array.size() );
}

static void packCString( UibStrTable& strings, QDataStream& out,
			 const char *cstr )
{
    packUInt32( out, strings.insertCString(cstr) );
}

static void packString( UibStrTable& strings, QDataStream& out,
			const QString& str )
{
    packUInt32( out, strings.insertString(str) );
}

static void packStringSplit( UibStrTable& strings, QDataStream& out,
			     const QString& str, QChar sep )
{
    int pos = str.find( sep );
    if ( pos == -1 )
	pos = str.length();
    packString( strings, out, str.left(pos) );
    packString( strings, out, str.mid(pos) );
}

static void packVariant( UibStrTable& strings, QDataStream& out,
			 QVariant value, QString tag = "" )
{
    QStringList::ConstIterator s;

    Q_UINT8 type = value.type();
    if ( tag == "pixmap" ) {
	type = QVariant::Pixmap;
    } else if ( tag == "image" ) {
	type = QVariant::Image;
    } else if ( tag == "iconset" ) {
	type = QVariant::IconSet;
    }
    out << type;

    switch ( type ) {
    case QVariant::String:
    case QVariant::Pixmap:
    case QVariant::Image:
    case QVariant::IconSet:
	packString( strings, out, value.asString() );
	break;
    case QVariant::StringList:
	packUInt16( out, value.asStringList().count() );
	s = value.asStringList().begin();
	while ( s != value.asStringList().end() ) {
	    packString( strings, out, *s );
	    ++s;
	}
	break;
    case QVariant::Font:
	out << value.asFont();
	break;
    case QVariant::Rect:
	packUInt16( out, value.asRect().x() );
	packUInt16( out, value.asRect().y() );
	packUInt16( out, value.asRect().width() );
	packUInt16( out, value.asRect().height() );
	break;
    case QVariant::Size:
	packUInt16( out, value.asSize().width() );
	packUInt16( out, value.asSize().height() );
	break;
    case QVariant::Color:
	out << value.asColor();
	break;
    case QVariant::Point:
	packUInt16( out, value.asPoint().x() );
	packUInt16( out, value.asPoint().y() );
	break;
    case QVariant::Int:
	packUInt32( out, value.asInt() );
	break;
    case QVariant::Bool:
	out << (Q_UINT8) value.asBool();
	break;
    case QVariant::Double:
	out << value.asDouble();
	break;
    case QVariant::CString:
	packCString( strings, out, value.asCString() );
	break;
    case QVariant::Cursor:
	out << value.asCursor();
	break;
    case QVariant::Date:
	out << value.asDate();
	break;
    case QVariant::Time:
	out << value.asTime();
	break;
    case QVariant::DateTime:
	out << value.asDateTime();
	break;
    default:
	out << value;
    }
}

static void outputProperty( QMap<int, QStringList>& buddies, int objectNo,
			    UibStrTable& strings, QDataStream& out,
			    QDomElement elem )
{
    QCString name = elem.attribute( "name" ).latin1();
    QDomElement f = elem.firstChild().toElement();
    QString tag = f.tagName();
    QString comment;
    QVariant value;

    if ( name == "resizeable" )
	name = "resizable";

    if ( tag == "font" ) {
	QString family;
	Q_UINT16 pointSize = 65535;
	Q_UINT8 fontFlags = 0;

	QDomElement g = f.firstChild().toElement();
	while ( !g.isNull() ) {
	    QString text = g.firstChild().toText().data();
	    if ( g.tagName() == "family" ) {
		fontFlags |= Font_Family;
		family = text;
	    } else if ( g.tagName() == "pointsize" ) {
		fontFlags |= Font_PointSize;
		pointSize = (Q_UINT16) text.toUInt();
	    } else {
		if ( g.firstChild().toText().data().toInt() != 0 ) {
		    if ( g.tagName() == "bold" ) {
			fontFlags |= Font_Bold;
		    } else if ( g.tagName() == "italic" ) {
			fontFlags |= Font_Italic;
		    } else if ( g.tagName() == "underline" ) {
			fontFlags |= Font_Underline;
		    } else if ( g.tagName() == "strikeout" ) {
			fontFlags |= Font_StrikeOut;
		    }
		}
	    }
	    g = g.nextSibling().toElement();
	}

	out << (Q_UINT8) Object_FontProperty;
	packCString( strings, out, name );
	out << fontFlags;
	if ( fontFlags & Font_Family )
	    packString( strings, out, family );
	if ( fontFlags & Font_PointSize )
	    packUInt16( out, pointSize );
    } else if ( tag == "palette" ) {
	out << (Q_UINT8) Object_PaletteProperty;
	packCString( strings, out, name );

	QDomElement g = f.firstChild().toElement();
	while ( !g.isNull() ) {
	    QDomElement h = g.firstChild().toElement();
	    while ( !h.isNull() ) {
		value = DomTool::elementToVariant( h, Qt::gray );
		if ( h.tagName() == "color" ) {
		    out << (Q_UINT8) Palette_Color;
		    out << value.asColor();
		} else if ( h.tagName() == "pixmap" ) {
		    out << (Q_UINT8) Palette_Pixmap;
		    packVariant( strings, out, value, "pixmap" );
		}
		h = h.nextSibling().toElement();
	    }

	    if ( g.tagName() == "active" ) {
		out << (Q_UINT8) Palette_Active;
	    } else if ( g.tagName() == "inactive" ) {
		out << (Q_UINT8) Palette_Inactive;
	    } else {
		out << (Q_UINT8) Palette_Disabled;
	    }
	    g = g.nextSibling().toElement();
	}
	out << (Q_UINT8) Palette_End;
    } else {
	value = DomTool::elementToVariant( f, value, comment );
	if ( value.isValid() ) {
	    if ( name == "buddy" ) {
		buddies[objectNo] += value.asString();
	    } else {
		if ( tag == "string" ) {
		    out << (Q_UINT8) Object_TextProperty;
		    packCString( strings, out, name );
		    packCString( strings, out, value.asString().utf8() );
		    packCString( strings, out, comment.utf8() );
		} else {
		    out << (Q_UINT8) Object_VariantProperty;
		    packCString( strings, out, name );
		    packVariant( strings, out, value, tag );
		}
	    }
	}
    }
}

static void outputGridCell( QDataStream& out, QDomElement elem )
{
    int column = elem.attribute( "column", "0" ).toInt();
    int row = elem.attribute( "row", "0" ).toInt();
    int colspan = elem.attribute( "colspan", "1" ).toInt();
    int rowspan = elem.attribute( "rowspan", "1" ).toInt();
    if ( colspan < 1 )
	colspan = 1;
    if ( rowspan < 1 )
	rowspan = 1;

    if ( column != 0 || row != 0 || colspan != 1 || rowspan != 1 ) {
	out << (Q_UINT8) Object_GridCell;
	packUInt16( out, column );
	packUInt16( out, row );
	packUInt16( out, colspan );
	packUInt16( out, rowspan );
    }
}

static int outputObject( QMap<int, QStringList>& buddies,
			 UibIndexMap& objects, UibStrTable& strings,
			 QDataStream& out, QDomElement elem,
			 QCString className = "" );

static void outputLayoutWidgetsSubLayout( QMap<int, QStringList>& buddies,
					  UibIndexMap& objects,
					  UibStrTable& strings,
					  QDataStream& out, QDomElement elem )
{
    int subLayoutNo = -1;
    QCString name;
    QDomElement nameElem;

    QDomElement f = elem.firstChild().toElement();
    while ( !f.isNull() ) {
	QString tag = f.tagName();
	if ( tag == "grid" || tag == "hbox" || tag == "vbox" ) {
	    out << (Q_UINT8) Object_SubLayout;
	    subLayoutNo = outputObject( buddies, objects, strings, out, f,
					layoutForTag(tag) );
	} else if ( tag == "property" ) {
	    if ( f.attribute("name") == "name" ) {
		name = DomTool::elementToVariant( f, name ).asCString();
		nameElem = f;
	    }
	}
	f = f.nextSibling().toElement();
    }

    if ( subLayoutNo != -1 ) {
	/*
	  Remove the sub-layout's Object_End marker, append the grid
	  cell and the correct name property, and put the Object_End
	  marker back.
	*/
	out.device()->at( out.device()->at() - 1 );
	outputGridCell( out, elem );
	outputProperty( buddies, subLayoutNo, strings, out, nameElem );
	out << (Q_UINT8) Object_End;

	objects.setName( subLayoutNo, name );
    }
}

static int outputObject( QMap<int, QStringList>& buddies,
			 UibIndexMap& objects, UibStrTable& strings,
			 QDataStream& out, QDomElement elem,
			 QCString className )
{
    bool isQObject = !className.isEmpty();

    if ( className == "QToolBar" )
	out << (Q_UINT8) elem.attribute( "dock", "0" ).toInt();
    if ( className == "QWidget" )
	className = elem.attribute( "class", className ).latin1();

    int objectNo = -1;
    if ( isQObject ) {
	packCString( strings, out, className );
	objectNo = objects.insert();
    }

    outputGridCell( out, elem );

    // optimization: insert '&Foo' into string-table before 'Foo'
    if ( className == "QAction" || className == "QActionGroup" ) {
	QVariant value = DomTool::readProperty( elem, "menuText", QVariant() );
	if ( value.asString().startsWith("&") )
	    strings.insertString( value.asString() );
    }

    QDomElement f = elem.firstChild().toElement();
    while ( !f.isNull() ) {
	QString tag = f.tagName();
	if ( tag == "action" ) {
	    if ( elem.tagName() == "item" || elem.tagName() == "toolbar" ) {
		QString actionName = f.attribute( "name" );
		int no = objects.find( actionName );
		if ( no != -1 ) {
		    out << (Q_UINT8) Object_ActionRef;
		    packUInt16( out, no );
		}
	    } else {
		out << (Q_UINT8) Object_SubAction;
		outputObject( buddies, objects, strings, out, f, "QAction" );
	    }
	} else if ( tag == "actiongroup" ) {
	    out << (Q_UINT8) Object_SubAction;
	    outputObject( buddies, objects, strings, out, f, "QActionGroup" );
	} else if ( tag == "attribute" ) {
	    out << (Q_UINT8) Object_Attribute;
	    outputProperty( buddies, objectNo, strings, out, f );
	} else if ( tag == "column" ) {
	    out << (Q_UINT8) Object_Column;
	    outputObject( buddies, objects, strings, out, f );
	} else if ( tag == "event" ) {
	    out << (Q_UINT8) Object_Event;
	    packCString( strings, out, f.attribute("name").latin1() );
	    packVariant( strings, out,
			 QStringList::split(',', f.attribute("functions")) );
	} else if ( tag == "grid" || tag == "hbox" || tag == "vbox" ) {
	    out << (Q_UINT8) Object_SubLayout;
	    outputObject( buddies, objects, strings, out, f,
			  layoutForTag(tag) );
	} else if ( tag == "item" ) {
	    if ( elem.tagName() == "menubar" ) {
		out << (Q_UINT8) Object_MenuItem;
		packCString( strings, out, f.attribute("name").latin1() );
		packCString( strings, out, f.attribute("text").utf8() );
		outputObject( buddies, objects, strings, out, f );
	    } else {
		out << (Q_UINT8) Object_Item;
		outputObject( buddies, objects, strings, out, f );
	    }
	} else if ( tag == "property" ) {
	    outputProperty( buddies, objectNo, strings, out, f );
	} else if ( tag == "row" ) {
	    out << (Q_UINT8) Object_Row;
	    outputObject( buddies, objects, strings, out, f );
	} else if ( tag == "separator" ) {
	    out << (Q_UINT8) Object_Separator;
	} else if ( tag == "spacer" ) {
	    out << (Q_UINT8) Object_Spacer;
	    outputObject( buddies, objects, strings, out, f );
	} else if ( tag == "widget" ) {
	    if ( f.attribute("class") == "QLayoutWidget" &&
		 elem.tagName() != "widget" ) {
		outputLayoutWidgetsSubLayout( buddies, objects, strings, out,
					      f );
	    } else {
		out << (Q_UINT8) Object_SubWidget;
		outputObject( buddies, objects, strings, out, f, "QWidget" );
	    }
	}
	f = f.nextSibling().toElement();
    }
    out << (Q_UINT8) Object_End;
    if ( isQObject )
	objects.setName( objectNo,
			 DomTool::readProperty(elem, "name", "").asString() );
    return objectNo;
}

static void outputBlock( QDataStream& out, BlockTag tag,
			 const QByteArray& data )
{
    if ( !data.isEmpty() ) {
	out << (Q_UINT8) tag;
	packByteArray( out, data );
    }
}

void convertUiToUib( QDomDocument& doc, QDataStream& out )
{
    QByteArray introBlock;
    QByteArray actionsBlock;
    QByteArray buddiesBlock;
    QByteArray connectionsBlock;
    QByteArray functionsBlock;
    QByteArray imagesBlock;
    QByteArray menubarBlock;
    QByteArray slotsBlock;
    QByteArray tabstopsBlock;
    QByteArray toolbarsBlock;
    QByteArray variablesBlock;
    QByteArray widgetBlock;

    QDomElement actionsElem;
    QDomElement connectionsElem;
    QDomElement imagesElem;
    QDomElement menubarElem;
    QDomElement tabstopsElem;
    QDomElement toolbarsElem;
    QDomElement widgetElem;

    QMap<int, QStringList> buddies;
    UibStrTable strings;
    UibIndexMap objects;
    int widgetNo = -1;
    QCString className;
    Q_INT16 defaultMargin = -32768;
    Q_INT16 defaultSpacing = -32768;
    Q_UINT8 introFlags = 0;

    QDomElement elem = doc.firstChild().toElement().firstChild().toElement();
    while ( !elem.isNull() ) {
	QString tag = elem.tagName();

	switch ( tag[0].latin1() ) {
	case 'a':
	    if ( tag == "actions" )
		actionsElem = elem;
	    break;
	case 'c':
	    if ( tag == "class" ) {
		className = elem.firstChild().toText().data().latin1();
	    } else if ( tag == "connections" ) {
		connectionsElem = elem;
	    }
	    break;
	case 'f':
	    if ( tag == "functions" ) {
		QDataStream out2( functionsBlock, IO_WriteOnly );
		QDomElement f = elem.firstChild().toElement();
		while ( !f.isNull() ) {
		    if ( f.tagName() == "function" ) {
			packStringSplit( strings, out2,
					 f.attribute("name").latin1(), '(' );
			packString( strings, out2,
				    f.firstChild().toText().data() );
		    }
		    f = f.nextSibling().toElement();
		}
	    }
	    break;
	case 'i':
	    if ( tag == "images" ) {
		QDataStream out2( imagesBlock, IO_WriteOnly );
		QDomElement f = elem.firstChild().toElement();
		while ( !f.isNull() ) {
		    if ( f.tagName() == "image" ) {
			QString name = f.attribute( "name" );
			QDomElement g = f.firstChild().toElement();
			if ( g.tagName() == "data" ) {
			    QString format = g.attribute( "format", "PNG" );
			    QString hex = g.firstChild().toText().data();
			    int n = hex.length() / 2;
			    QByteArray data( n );
			    for ( int i = 0; i < n; i++ )
				data[i] = (char) hex.mid( 2 * i, 2 )
						    .toUInt( 0, 16 );

			    packString( strings, out2, name );
			    packString( strings, out2, format );
			    packUInt32( out2, g.attribute("length").toInt() );
			    packByteArray( out2, data );
			}
		    }
		    f = f.nextSibling().toElement();
		}
	    }
	    break;
	case 'l':
	    if ( tag == "layoutdefaults" ) {
		QString margin = elem.attribute( "margin" );
		if ( !margin.isEmpty() )
		    defaultMargin = margin.toInt();
		QString spacing = elem.attribute( "spacing" );
		if ( !spacing.isEmpty() )
		    defaultSpacing = spacing.toInt();
	    }
	    break;
	case 'm':
	    if ( tag == "menubar" )
		menubarElem = elem;
	    break;
	case 'p':
	    if ( tag == "pixmapinproject" )
		introFlags |= Intro_Pixmapinproject;
	    break;
	case 's':
	    if ( tag == "slots" ) {
		QDataStream out2( slotsBlock, IO_WriteOnly );
		QDomElement f = elem.firstChild().toElement();
		while ( !f.isNull() ) {
		    if ( f.tagName() == "slot" ) {
			QString language = f.attribute( "language", "C++" );
			QString slot = UibHack::normalize(
				f.firstChild().toText().data() );
			packString( strings, out2, language );
			packStringSplit( strings, out2, slot, '(' );
		    }
		    f = f.nextSibling().toElement();
		}
	    }
	    break;
	case 't':
	    if ( tag == "tabstops" ) {
		tabstopsElem = elem;
	    } else if ( tag == "toolbars" ) {
		toolbarsElem = elem;
	    }
	    break;
	case 'v':
	    if ( tag == "variable" ) {
		QDataStream out2( variablesBlock, IO_WriteOnly | IO_Append );
		packString( strings, out2, elem.firstChild().toText().data() );
	    } else if ( tag == "variables" ) {
		QDataStream out2( variablesBlock, IO_WriteOnly );
		QDomElement f = elem.firstChild().toElement();
		while ( !f.isNull() ) {
		    if ( f.tagName() == "variable" )
			packString( strings, out2,
				    f.firstChild().toText().data() );
		    f = f.nextSibling().toElement();
		}
	    }
	    break;
	case 'w':
	    if ( tag == "widget" )
		widgetElem = elem;
	}
	elem = elem.nextSibling().toElement();
    }

    {
	QDataStream out2( widgetBlock, IO_WriteOnly );
	widgetNo = outputObject( buddies, objects, strings, out2, widgetElem,
				 "QWidget" );
    }

    if ( !tabstopsElem.isNull() ) {
	QDataStream out2( tabstopsBlock, IO_WriteOnly );
	QDomElement f = tabstopsElem.firstChild().toElement();
	while ( !f.isNull() ) {
	    if ( f.tagName() == "tabstop" ) {
		QString widgetName = f.firstChild().toText().data();
		int no = objects.find( widgetName );
		if ( no != -1 )
		    packUInt16( out2, no );
	    }
	    f = f.nextSibling().toElement();
	}
    }

    if ( !actionsElem.isNull() ) {
	QDataStream out2( actionsBlock, IO_WriteOnly );
	outputObject( buddies, objects, strings, out2, actionsElem );
    }

    if ( !menubarElem.isNull() ) {
	QDataStream out2( menubarBlock, IO_WriteOnly );
	outputObject( buddies, objects, strings, out2, menubarElem,
		      "QMenuBar" );
    }

    if ( !toolbarsElem.isNull() ) {
	QDataStream out2( toolbarsBlock, IO_WriteOnly );
	QDomElement f = toolbarsElem.firstChild().toElement();
	while ( !f.isNull() ) {
	    if ( f.tagName() == "toolbar" )
		outputObject( buddies, objects, strings, out2, f, "QToolBar" );
	    f = f.nextSibling().toElement();
	}
    }

    if ( !buddies.isEmpty() ) {
	QDataStream out2( buddiesBlock, IO_WriteOnly );
	QMap<int, QStringList>::ConstIterator a = buddies.begin();
	while ( a != buddies.end() ) {
	    QStringList::ConstIterator b = (*a).begin();
	    while ( b != (*a).end() ) {
		int no = objects.find( *b );
		if ( no != -1 ) {
		    packUInt16( out2, a.key() );
		    packUInt16( out2, no );
		}
		++b;
	    }
	    ++a;
	}
    }

    if ( !connectionsElem.isNull() ) {
	QString prevLanguage = "C++";
	int prevSenderNo = 0;
	QString prevSignal = "clicked()";
	int prevReceiverNo = 0;
	QString prevSlot = "accept()";

	QDataStream out2( connectionsBlock, IO_WriteOnly );
	QDomElement f = connectionsElem.firstChild().toElement();
	while ( !f.isNull() ) {
	    if ( f.tagName() == "connection" ) {
		QMap<QString, QString> argMap;

		QDomElement g = f.firstChild().toElement();
		while ( !g.isNull() ) {
		    argMap[g.tagName()] = g.firstChild().toText().data();
		    g = g.nextSibling().toElement();
		}

		QString language = f.attribute( "language", "C++" );
		int senderNo = objects.find( argMap["sender"], widgetNo );
		int receiverNo = objects.find( argMap["receiver"], widgetNo );
		QString signal = UibHack::normalize( argMap["signal"] );
		QString slot = UibHack::normalize( argMap["slot"] );

		Q_UINT8 connectionFlags = 0;
		if ( language != prevLanguage )
		    connectionFlags |= Connection_Language;
		if ( senderNo != prevSenderNo )
		    connectionFlags |= Connection_Sender;
		if ( signal != prevSignal )
		    connectionFlags |= Connection_Signal;
		if ( receiverNo != prevReceiverNo )
		    connectionFlags |= Connection_Receiver;
		if ( slot != prevSlot )
		    connectionFlags |= Connection_Slot;
		out2 << connectionFlags;

		if ( connectionFlags & Connection_Language )
		    packString( strings, out2, language );
		if ( connectionFlags & Connection_Sender )
		    packUInt16( out2, senderNo );
		if ( connectionFlags & Connection_Signal )
		    packStringSplit( strings, out2, signal, '(' );
		if ( connectionFlags & Connection_Receiver )
		    packUInt16( out2, receiverNo );
		if ( connectionFlags & Connection_Slot )
		    packStringSplit( strings, out2, slot, '(' );

		prevLanguage = language;
		prevSenderNo = senderNo;
		prevSignal = signal;
		prevReceiverNo = receiverNo;
		prevSlot = slot;
	    } else if ( f.tagName() == "slot" ) {
		// ###
	    }
	    f = f.nextSibling().toElement();
	}
    }

    {
	QDataStream out2( introBlock, IO_WriteOnly );
	out2 << introFlags;
	out2 << defaultMargin;
	out2 << defaultSpacing;
	packUInt16( out2, objects.count() );
	packCString( strings, out2, className );
    }

    out << UibMagic;
    out << (Q_UINT8) '\n';
    out << (Q_UINT8) '\r';
    out << (Q_UINT8) out.version();
    outputBlock( out, Block_Strings, strings.block() );
    outputBlock( out, Block_Intro, introBlock );
    outputBlock( out, Block_Images, imagesBlock );
    outputBlock( out, Block_Widget, widgetBlock );
    outputBlock( out, Block_Slots, slotsBlock );
    outputBlock( out, Block_Tabstops, tabstopsBlock );
    outputBlock( out, Block_Actions, actionsBlock );
    outputBlock( out, Block_Menubar, menubarBlock );
    outputBlock( out, Block_Toolbars, toolbarsBlock );
    outputBlock( out, Block_Variables, variablesBlock );
    outputBlock( out, Block_Functions, functionsBlock );
    outputBlock( out, Block_Buddies, buddiesBlock );
    outputBlock( out, Block_Connections, connectionsBlock );
    out << (Q_UINT8) Block_End;
}
