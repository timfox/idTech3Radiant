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

#include "trayicon.h"

#include "mainframe.h"
#include "commands.h"
#include "gtkutil/image.h"

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QApplication>

namespace
{
QSystemTrayIcon* g_trayIcon = nullptr;
QMenu* g_trayMenu = nullptr;
}

static void tray_showWindow(){
	QWidget* w = MainFrame_getWindow();
	if ( w ) {
		w->showNormal();
		w->activateWindow();
		w->raise();
	}
}

static void tray_hideWindow(){
	QWidget* w = MainFrame_getWindow();
	if ( w ) {
		w->hide();
	}
}

static void tray_toggleVisibility(){
	QWidget* w = MainFrame_getWindow();
	if ( w ) {
		if ( w->isVisible() ) {
			w->hide();
		}
		else {
			tray_showWindow();
		}
	}
}

static void tray_executeCommand( const char* name ){
	const Command& cmd = GlobalCommands_find( name );
	cmd.m_callback();
}

bool TrayIcon_isAvailable(){
	return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayIcon_setVisible( bool visible ){
	if ( g_trayIcon ) {
		if ( visible ) {
			g_trayIcon->show();
		}
		else {
			g_trayIcon->hide();
		}
	}
}

void TrayIcon_construct(){
	if ( !QSystemTrayIcon::isSystemTrayAvailable() ) {
		return;
	}

	g_trayIcon = new QSystemTrayIcon( new_local_icon( "radiant.ico" ) );
	g_trayIcon->setToolTip( "Radiant" );

	g_trayMenu = new QMenu;

	auto* showAct = g_trayMenu->addAction( "Show" );
	QObject::connect( showAct, &QAction::triggered, tray_showWindow );

	auto* hideAct = g_trayMenu->addAction( "Hide" );
	QObject::connect( hideAct, &QAction::triggered, tray_hideWindow );

	g_trayMenu->addSeparator();

	auto* saveAct = g_trayMenu->addAction( "Save" );
	QObject::connect( saveAct, &QAction::triggered, [](){ tray_executeCommand( "SaveMap" ); } );

	auto* buildAct = g_trayMenu->addAction( "Build (F5)" );
	QObject::connect( buildAct, &QAction::triggered, [](){ tray_executeCommand( "Build_runRecentExecutedBuild" ); } );

	g_trayMenu->addSeparator();

	auto* newMapAct = g_trayMenu->addAction( "New Map" );
	QObject::connect( newMapAct, &QAction::triggered, [](){ tray_executeCommand( "NewMap" ); } );

	auto* openMapAct = g_trayMenu->addAction( "Open Map" );
	QObject::connect( openMapAct, &QAction::triggered, [](){ tray_executeCommand( "OpenMap" ); } );

	g_trayMenu->addSeparator();

	auto* prefsAct = g_trayMenu->addAction( "Preferences" );
	QObject::connect( prefsAct, &QAction::triggered, [](){ tray_executeCommand( "Preferences" ); } );

	g_trayMenu->addSeparator();

	auto* quitAct = g_trayMenu->addAction( "Quit" );
	QObject::connect( quitAct, &QAction::triggered, [](){ tray_executeCommand( "Exit" ); } );

	g_trayIcon->setContextMenu( g_trayMenu );

	QObject::connect( g_trayIcon, &QSystemTrayIcon::activated, []( QSystemTrayIcon::ActivationReason reason ){
		if ( reason == QSystemTrayIcon::ActivationReason::DoubleClick
		  || reason == QSystemTrayIcon::ActivationReason::Trigger ) {
			tray_toggleVisibility();
		}
	} );

	if ( g_trayIconEnabled ) {
		g_trayIcon->show();
	}
}

void TrayIcon_destroy(){
	if ( g_trayIcon ) {
		g_trayIcon->hide();
		delete g_trayIcon;
		g_trayIcon = nullptr;
	}
	if ( g_trayMenu ) {
		delete g_trayMenu;
		g_trayMenu = nullptr;
	}
}
