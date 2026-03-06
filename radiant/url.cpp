/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "url.h"

#include "mainframe.h"
#include "gtkutil/messagebox.h"
#include <QDesktopServices>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>
#include <QUrl>

#if defined( __linux__ )
namespace
{
QProcessEnvironment OpenURL_childEnvironment(){
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

	// GTK3 provides ATK bridge natively; inherited forced module lists can emit warnings.
	const QString gtkModules = env.value( "GTK_MODULES" );
	if ( !gtkModules.isEmpty() ) {
		QStringList filtered;
		for ( const auto& module : gtkModules.split( ':', Qt::SkipEmptyParts ) )
		{
			if ( !module.contains( "atk-bridge", Qt::CaseInsensitive ) ) {
				filtered.push_back( module );
			}
		}
		if ( filtered.isEmpty() ) {
			env.remove( "GTK_MODULES" );
		}
		else{
			env.insert( "GTK_MODULES", filtered.join( ":" ) );
		}
	}

	return env;
}

bool OpenURL_linuxDetached( const QUrl& qurl ){
	QProcess process;
	process.setProcessEnvironment( OpenURL_childEnvironment() );
	process.setProgram( "xdg-open" );
	process.setArguments( { qurl.toString() } );
	process.setStandardInputFile( "/dev/null" );
	process.setStandardOutputFile( "/dev/null" );
	process.setStandardErrorFile( "/dev/null" );
	return process.startDetached();
}
}
#endif

void OpenURL( const char *url ){
	globalOutputStream() << "OpenURL: " << url << '\n';
	// QUrl::fromUserInput works for urls and local paths with spaces.
	const QUrl qurl = QUrl::fromUserInput( url );
#if defined( __linux__ )
	if ( OpenURL_linuxDetached( qurl ) ) {
		return;
	}
#endif
	if ( !QDesktopServices::openUrl( qurl ) ) {
		qt_MessageBox( MainFrame_getWindow(), "Failed to launch browser!" );
	}
}
