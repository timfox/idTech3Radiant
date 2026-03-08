/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
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

//
// Sky Browser - browse and select skybox shaders
//

#include "skybrowser.h"

#include "debugging/debugging.h"

#include "ishaders.h"
#include "iselection.h"
#include "iscenegraph.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QApplication>

#include "gtkutil/guisettings.h"
#include "gtkutil/image.h"
#include "stream/stringstream.h"
#include "string/string.h"
#include "os/path.h"

#include "commands.h"
#include "select.h"
#include "brushmanip.h"
#include "patchmanip.h"
#include "texwindow.h"
#include "groupdialog.h"
#include "mainframe.h"


static QDialog* g_skyBrowserDialog{};
static QListWidget* g_skyList{};
static QLineEdit* g_skyFilter{};


static void SkyBrowser_populate(){
	if ( g_skyList == nullptr ) {
		return;
	}
	g_skyList->clear();

	const char* filter = g_skyFilter != nullptr ? g_skyFilter->text().trimmed().toLatin1().constData() : "";
	const std::size_t filterLen = strlen( filter );

	GlobalShaderSystem().foreachShaderName( []( const char* name ){
		if ( !shader_equal_prefix( name, "textures/" ) ) {
			return;
		}
		IShader* shader = QERApp_Shader_ForName( name );
		const bool isSky = ( shader->getFlags() & QER_SKY ) != 0;
		shader->DecRef();
		if ( !isSky ) {
			return;
		}
		if ( g_skyFilter != nullptr ) {
			const char* f = g_skyFilter->text().trimmed().toLatin1().constData();
			if ( f[0] != '\0' && !strstr( name, f ) ) {
				return;
			}
		}
		auto* item = new QListWidgetItem( name );
		item->setIcon( new_local_icon( "skybox.png" ) );
		item->setData( Qt::UserRole, name );
		g_skyList->addItem( item );
	} );
}


static void SkyBrowser_applySelected(){
	QListWidgetItem* item = g_skyList != nullptr ? g_skyList->currentItem() : nullptr;
	if ( item == nullptr ) {
		return;
	}
	QByteArray shaderBa = item->data( Qt::UserRole ).toString().toLatin1();
	const char* shader = shaderBa.constData();
	if ( shader[0] == '\0' ) {
		return;
	}
	Select_SetShader_Undo( shader );
	TextureBrowser_SetSelectedShader( shader );
	TextureBrowser_ShowDirectoryOfShader( shader );
	SceneChangeNotify();
}


static void SkyBrowser_constructWindow( QWidget* parent ){
	if ( g_skyBrowserDialog != nullptr ) {
		g_skyBrowserDialog->show();
		g_skyBrowserDialog->raise();
		g_skyBrowserDialog->activateWindow();
		return;
	}

	g_skyBrowserDialog = new QDialog( parent );
	g_skyBrowserDialog->setWindowTitle( "Sky Browser" );
	g_skyBrowserDialog->setAttribute( Qt::WA_DeleteOnClose, false );
	g_guiSettings.addWindow( g_skyBrowserDialog, "SkyBrowser" );

	auto* vbox = new QVBoxLayout( g_skyBrowserDialog );

	{
		auto* filterBox = new QHBoxLayout;
		filterBox->addWidget( new QLabel( "Filter:" ) );
		g_skyFilter = new QLineEdit;
		g_skyFilter->setPlaceholderText( "Filter by name..." );
		g_skyFilter->setClearButtonEnabled( true );
		QObject::connect( g_skyFilter, &QLineEdit::textChanged, []( const QString& ){
			SkyBrowser_populate();
		} );
		filterBox->addWidget( g_skyFilter );
		vbox->addLayout( filterBox );
	}

	g_skyList = new QListWidget;
	g_skyList->setViewMode( QListView::ListMode );
	g_skyList->setIconSize( QSize( 24, 24 ) );
	g_skyList->setAlternatingRowColors( true );
	g_skyList->setSelectionMode( QAbstractItemView::SingleSelection );
	QObject::connect( g_skyList, &QListWidget::itemDoubleClicked, []( QListWidgetItem* ){
		SkyBrowser_applySelected();
	} );
	vbox->addWidget( g_skyList );

	{
		auto* buttons = new QDialogButtonBox;
		auto* applyBtn = buttons->addButton( "Apply to Selection", QDialogButtonBox::AcceptRole );
		auto* refreshBtn = buttons->addButton( "Refresh", QDialogButtonBox::ActionRole );
		buttons->addButton( QDialogButtonBox::Close );

		QObject::connect( applyBtn, &QPushButton::clicked, [](){
			SkyBrowser_applySelected();
		} );
		QObject::connect( refreshBtn, &QPushButton::clicked, [](){
			SkyBrowser_populate();
		} );
		QObject::connect( buttons, &QDialogButtonBox::rejected, g_skyBrowserDialog, &QDialog::hide );

		vbox->addWidget( buttons );
	}

	SkyBrowser_populate();
	g_skyBrowserDialog->show();
}


static void SkyBrowser_destroyWindow(){
	if ( g_skyBrowserDialog != nullptr ) {
		g_skyBrowserDialog->deleteLater();
		g_skyBrowserDialog = nullptr;
		g_skyList = nullptr;
		g_skyFilter = nullptr;
	}
}


static void SkyBrowser_show(){
	SkyBrowser_constructWindow( MainFrame_getWindow() );
}


void SkyBrowser_Construct(){
	GlobalCommands_insert( "SkyBrowser", makeCallbackF( SkyBrowser_show ), QKeySequence( "Ctrl+Shift+Y" ) );
}


void SkyBrowser_Destroy(){
	SkyBrowser_destroyWindow();
}
