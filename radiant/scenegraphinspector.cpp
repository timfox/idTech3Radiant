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
#include <QLineEdit>
#include <QSortFilterProxyModel>

#include "treemodel.h"

extern QAbstractItemModel* scene_graph_get_tree_model();

namespace
{

QDockWidget* g_scenegraphDock{};
QTreeView* g_scenegraphTreeView{};
QSortFilterProxyModel* g_scenegraphProxyModel{};
bool g_modelAttached{};

class OutlinerFilterModel : public QSortFilterProxyModel
{
public:
	using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
	bool filterAcceptsRow( int sourceRow, const QModelIndex& sourceParent ) const override {
		if ( filterRegularExpression().pattern().isEmpty() ) {
			return true;
		}

		const QModelIndex index = sourceModel()->index( sourceRow, 0, sourceParent );
		if ( sourceModel()->data( index ).toString().contains( filterRegularExpression() ) ) {
			return true;
		}

		const int childCount = sourceModel()->rowCount( index );
		for ( int child = 0; child < childCount; ++child )
			if ( filterAcceptsRow( child, index ) )
				return true;

		return false;
	}
};

void ScenegraphInspector_attachModel(){
	if ( g_scenegraphTreeView != nullptr && !g_modelAttached ) {
		g_scenegraphProxyModel->setSourceModel( scene_graph_get_tree_model() );
		g_scenegraphTreeView->setModel( g_scenegraphProxyModel );
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

	g_scenegraphDock = new QDockWidget( "Outliner", window );
	g_scenegraphDock->setObjectName( "dock_scenegraph_inspector" );

	auto* root = new QWidget( g_scenegraphDock );
	auto* layout = new QVBoxLayout( root );
	layout->setContentsMargins( 4, 4, 4, 4 );

	auto* filterLine = new QLineEdit( root );
	filterLine->setClearButtonEnabled( true );
	filterLine->setPlaceholderText( "Filter hierarchy" );
	layout->addWidget( filterLine );

	auto* buttonRow = new QHBoxLayout();
	auto* expandAllBtn = new QPushButton( "Expand All", root );
	auto* collapseAllBtn = new QPushButton( "Collapse All", root );
	buttonRow->addWidget( expandAllBtn );
	buttonRow->addWidget( collapseAllBtn );
	buttonRow->addStretch();
	layout->addLayout( buttonRow );

	g_scenegraphTreeView = new QTreeView( root );
	g_scenegraphProxyModel = new OutlinerFilterModel( g_scenegraphTreeView );
	g_scenegraphProxyModel->setFilterCaseSensitivity( Qt::CaseInsensitive );
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
	QObject::connect( filterLine, &QLineEdit::textChanged, []( const QString& text ){
		if ( g_scenegraphProxyModel != nullptr ) {
			g_scenegraphProxyModel->setFilterFixedString( text );
			if ( !text.isEmpty() ) {
				ScenegraphInspector_expandAll();
			}
		}
	} );
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
		g_scenegraphProxyModel = nullptr;
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
