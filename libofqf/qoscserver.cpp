/*
 * Copyright ( C ) 2007 Arnold Krille <arnold@arnoldarts.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "qoscserver.h"

#include <QtCore/QDebug>
#include <QtCore/QRegExp>
#include <QtNetwork/QUdpSocket>

QOscServer::QOscServer( quint16 port, QObject* p )
	: QOscBase( p )
{
	qDebug() << "QOscServer::QOscServer(" << port << "," << p << ")";
	qDebug() << " socket() gives" << socket();
	socket()->bind( QHostAddress::Any, port );
	connect( socket(), SIGNAL( readyRead() ), this, SLOT( readyRead() ) );
}
QOscServer::QOscServer( QHostAddress address, quint16 port, QObject* p )
	: QOscBase( p )
{
	qDebug() << "QOscServer::QOscServer(" << address << "," << port << "," << p << ")";
	socket()->bind( address, port );
}

QOscServer::~QOscServer() {
	qDebug() << "QOscServer::~QOscServer()";
	// manually destroy child path objects here as they need to unregister themselves
	// before `paths` gets destroyed at the end of this function
	for (PathObject* pathObject : findChildren<PathObject*>("", Qt::FindDirectChildrenOnly))
	{
		delete pathObject;
	}
}

void QOscServer::registerPathObject( PathObject* p ) {
	paths.push_back( p );
}
void QOscServer::unregisterPathObject( PathObject* p ) {
	paths.removeAll( p );
}

#define BUFFERSIZE 255

void QOscServer::readyRead() {
	qDebug() << "QOscServer::readyRead()";
	while ( socket()->hasPendingDatagrams() ) {
		QByteArray data( BUFFERSIZE, char( 0 ) );
		//data.resize( BUFFERSIZE );
		int size = socket()->readDatagram( data.data(), BUFFERSIZE );
		qDebug() << " read" << size << "(" << data.size() << ") bytes:" << data;

		//for ( int i=0; i<size; ++i )
		//	qDebug() << i << "\t" << static_cast<quint8*>( static_cast<void*>( data.data() ) )[ i ];
		QString path;
		QString args;
		QVariant arguments;

		int i=0;
		if ( data[ i ] == '/' ) {

			for ( ; i<size && data[ i ] != char( 0 ); ++i )
				path += data[ i ];

			while ( data[ i ] != ',' ) ++i;
			++i;
			while ( data[ i ] != char( 0 ) )
				args += data[ i++ ];

			if ( ! args.isEmpty() ) {
				QList<QVariant> list;

				foreach( QChar type, args ) {
					while ( i%4 != 0 ) ++i;
					//qDebug() << i << "\ttrying to convert to" << type;

					QByteArray tmp = data.right( data.size()-i );
					QVariant value;
					if ( type == 's' ) {
						QString s = toString( tmp );
						value = s;
						// string size plus one for the null terminator
						i += s.size() + 1;
					}
					if ( type == 'i' ) {
						value = toInt32( tmp );
						i+=4;
					}
					if ( type == 'f' ) {
						value = toFloat( tmp );
						i+=4;
					}
					//qDebug() << " got" << value;

					if ( args.size() > 1 )
						list.append( value );
					else
						arguments = value;
				}

				if ( args.size() > 1 )
					arguments = list;
			}
		}
		qDebug() << "path seems to be" << path << "args are" << args << ":" << arguments;

		QMap<QString,QString> replacements;
		replacements[ "!" ] = "^";
		replacements[ "{" ] = "(";
		replacements[ "}" ] = ")";
		replacements[ "," ] = "|";
		replacements[ "*" ] = ".*";
		replacements[ "?" ] = ".";

		foreach( QString rep, replacements.keys() )
			path.replace( rep, replacements[ rep ] );

		qDebug() << " after transformation to OSC-RegExp path is" << path;

		QRegExp exp( path );
		foreach( PathObject* obj, paths ) {
			if ( exp.exactMatch( obj->_path ) )
				obj->signalData( arguments );
		}
	}
}

