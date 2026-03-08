/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 */

#include "scenegraphinspector.h"

#include <QMainWindow>
#include <QDockWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeView>
#include <QPushButton>
#include <QHeaderView>

#include "treemodel.h"

extern QAbstractItemModel* scene_graph_get_tree_model();

namespace
{

QDockWidget* g_scenegraphDock{};
QTreeView* g_scenegraphTreeView{};
bool g_modelAttached{};

void ScenegraphInspector_attachModel(){
	if ( g_scenegraphTreeView != nullptr && !g_modelAttached ) {
		g_scenegraphTreeView->setModel( scene_graph_get_tree_model() );
		g_modelAttached = true;
	}
}

void ScenegraphInspector_detachModel(){
	if ( g_scenegraphTreeView != nullptr && g_modelAttached ) {
		g_scenegraphTreeView->setModel( nullptr );
		g_modelAttached = false;
	}
}

void ScenegraphInspector_expandAll(){
	if ( g_scenegraphTreeView != nullptr ) {
		g_scenegraphTreeView->expandAll();
	}
}

void ScenegraphInspector_collapseAll(){
	if ( g_scenegraphTreeView != nullptr ) {
		g_scenegraphTreeView->collapseAll();
	}
}

}

void ScenegraphInspector_createDock( QMainWindow* window ){
	if ( window == nullptr || g_scenegraphDock != nullptr ) {
		return;
	}

	g_scenegraphDock = new QDockWidget( "Scenegraph Inspector", window );
	g_scenegraphDock->setObjectName( "dock_scenegraph_inspector" );

	auto* root = new QWidget( g_scenegraphDock );
	auto* layout = new QVBoxLayout( root );
	layout->setContentsMargins( 4, 4, 4, 4 );

	auto* buttonRow = new QHBoxLayout();
	auto* expandAllBtn = new QPushButton( "Expand All", root );
	auto* collapseAllBtn = new QPushButton( "Collapse All", root );
	buttonRow->addWidget( expandAllBtn );
	buttonRow->addWidget( collapseAllBtn );
	buttonRow->addStretch();
	layout->addLayout( buttonRow );

	g_scenegraphTreeView = new QTreeView( root );
	g_scenegraphTreeView->setHeaderHidden( true );
	g_scenegraphTreeView->setEditTriggers( QAbstractItemView::EditTrigger::NoEditTriggers );
	g_scenegraphTreeView->setUniformRowHeights( true );
	g_scenegraphTreeView->setSizeAdjustPolicy( QAbstractScrollArea::SizeAdjustPolicy::AdjustToContents );
	g_scenegraphTreeView->header()->setStretchLastSection( false );
	g_scenegraphTreeView->header()->setSectionResizeMode( QHeaderView::ResizeMode::ResizeToContents );
	g_scenegraphTreeView->setSelectionMode( QAbstractItemView::SelectionMode::ExtendedSelection );
	layout->addWidget( g_scenegraphTreeView, 1 );

	QObject::connect( expandAllBtn, &QPushButton::clicked, ScenegraphInspector_expandAll );
	QObject::connect( collapseAllBtn, &QPushButton::clicked, ScenegraphInspector_collapseAll );
	QObject::connect( g_scenegraphDock, &QDockWidget::visibilityChanged, []( bool visible ){
		if ( visible )
			ScenegraphInspector_attachModel();
		else
			ScenegraphInspector_detachModel();
	} );

	g_scenegraphDock->setWidget( root );
	window->addDockWidget( Qt::RightDockWidgetArea, g_scenegraphDock );
	g_scenegraphDock->hide();
}

void ScenegraphInspector_destroyDock(){
	if ( g_scenegraphDock != nullptr ) {
		ScenegraphInspector_detachModel();
		g_scenegraphTreeView = nullptr;
		delete g_scenegraphDock;
		g_scenegraphDock = nullptr;
	}
}

void ScenegraphInspector_toggleShown(){
	if ( g_scenegraphDock != nullptr ) {
		const bool visible = g_scenegraphDock->isVisible();
		g_scenegraphDock->setVisible( !visible );
		if ( !visible ) {
			g_scenegraphDock->raise();
		}
	}
}
