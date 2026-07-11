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
#include <QItemSelectionModel>
#include <QTimer>

#include "camwindow.h"
#include "iselection.h"
#include "scenelib.h"
#include "select.h"
#include "signal/isignal.h"
#include "treemodel.h"

extern QAbstractItemModel* scene_graph_get_tree_model();

namespace
{

QDockWidget* g_scenegraphDock{};
QTreeView* g_scenegraphTreeView{};
QSortFilterProxyModel* g_scenegraphProxyModel{};
bool g_modelAttached{};
bool g_selectionSyncDisabled{};

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

void ScenegraphInspector_connectSelectionModel();
void ScenegraphInspector_revealSelection();
void ScenegraphInspector_selectionChanged( const Selectable& );

void ScenegraphInspector_attachModel(){
	if ( g_scenegraphTreeView != nullptr && !g_modelAttached ) {
		g_scenegraphProxyModel->setSourceModel( scene_graph_get_tree_model() );
		g_scenegraphTreeView->setModel( g_scenegraphProxyModel );
		ScenegraphInspector_connectSelectionModel();
		ScenegraphInspector_revealSelection();
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

template<typename Functor>
void ScenegraphInspector_forEachIndex( QAbstractItemModel* model, const Functor& functor, const QModelIndex& parent = QModelIndex() ){
	if ( model == nullptr ) {
		return;
	}

	const int rowCount = model->rowCount( parent );
	for ( int row = 0; row < rowCount; ++row )
	{
		const QModelIndex index = model->index( row, 0, parent );
		functor( index );
		ScenegraphInspector_forEachIndex( model, functor, index );
	}
}

QModelIndex ScenegraphInspector_firstSelectedIndex(){
	if ( g_scenegraphProxyModel == nullptr ) {
		return QModelIndex();
	}

	QModelIndex selectedIndex;
	ScenegraphInspector_forEachIndex( g_scenegraphProxyModel, [&selectedIndex]( const QModelIndex& index ){
		if ( selectedIndex.isValid() ) {
			return;
		}

		scene::Instance* instance = static_cast<scene::Instance*>( index.data( c_ItemDataRole_Instance ).value<void*>() );
		if ( instance != nullptr ) {
			if ( Selectable* selectable = Instance_getSelectable( *instance ); selectable != nullptr && selectable->isSelected() ) {
				selectedIndex = index;
			}
		}
	} );

	return selectedIndex;
}

void ScenegraphInspector_revealSelection(){
	if ( g_scenegraphTreeView == nullptr || g_scenegraphProxyModel == nullptr || g_scenegraphTreeView->selectionModel() == nullptr ) {
		return;
	}

	g_selectionSyncDisabled = true;
	ScenegraphInspector_forEachIndex( g_scenegraphProxyModel, []( const QModelIndex& index ){
		scene::Instance* instance = static_cast<scene::Instance*>( index.data( c_ItemDataRole_Instance ).value<void*>() );
		if ( instance != nullptr ) {
			if ( Selectable* selectable = Instance_getSelectable( *instance ) ) {
				g_scenegraphTreeView->selectionModel()->select( index, selectable->isSelected()
					? QItemSelectionModel::SelectionFlag::Select
					: QItemSelectionModel::SelectionFlag::Deselect );
			}
		}
	} );
	g_selectionSyncDisabled = false;

	const QModelIndex selectedIndex = ScenegraphInspector_firstSelectedIndex();
	if ( selectedIndex.isValid() ) {
		for ( QModelIndex parent = selectedIndex.parent(); parent.isValid(); parent = parent.parent() )
			g_scenegraphTreeView->expand( parent );
		g_scenegraphTreeView->setCurrentIndex( selectedIndex );
		g_scenegraphTreeView->scrollTo( selectedIndex, QAbstractItemView::ScrollHint::PositionAtCenter );
	}

	g_scenegraphTreeView->viewport()->update();
}

void ScenegraphInspector_selectionChanged( const Selectable& ){
	if ( g_scenegraphDock == nullptr || !g_scenegraphDock->isVisible() ) {
		return;
	}

	QTimer::singleShot( 0, [](){ ScenegraphInspector_revealSelection(); } );
}

void ScenegraphInspector_connectSelectionModel(){
	if ( g_scenegraphTreeView == nullptr || g_scenegraphTreeView->selectionModel() == nullptr ) {
		return;
	}

	QObject::connect( g_scenegraphTreeView->selectionModel(), &QItemSelectionModel::selectionChanged,
		[]( const QItemSelection &selected, const QItemSelection &deselected ){
			if ( g_selectionSyncDisabled ) {
				return;
			}

			for ( const QModelIndex& index : deselected.indexes() )
			{
				scene::Instance* instance = static_cast<scene::Instance*>( index.data( c_ItemDataRole_Instance ).value<void*>() );
				if ( instance != nullptr ) {
					if ( Selectable* selectable = Instance_getSelectable( *instance ) ) {
						selectable->setSelected( false );
					}
				}
			}

			for ( const QModelIndex& index : selected.indexes() )
			{
				scene::Instance* instance = static_cast<scene::Instance*>( index.data( c_ItemDataRole_Instance ).value<void*>() );
				if ( instance != nullptr ) {
					if ( Selectable* selectable = Instance_getSelectable( *instance ) ) {
						selectable->setSelected( true );
					}
				}
			}
		} );
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
	auto* hideSelectedBtn = new QPushButton( "Hide Sel", root );
	auto* isolateBtn = new QPushButton( "Isolate", root );
	auto* showHiddenBtn = new QPushButton( "Show Hidden", root );
	buttonRow->addWidget( expandAllBtn );
	buttonRow->addWidget( collapseAllBtn );
	buttonRow->addWidget( hideSelectedBtn );
	buttonRow->addWidget( isolateBtn );
	buttonRow->addWidget( showHiddenBtn );
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
	QObject::connect( hideSelectedBtn, &QPushButton::clicked, [](){ HideSelected(); } );
	QObject::connect( isolateBtn, &QPushButton::clicked, [](){ Select_IsolateSelection(); } );
	QObject::connect( showHiddenBtn, &QPushButton::clicked, [](){ Select_ShowAllHidden(); } );
	QObject::connect( filterLine, &QLineEdit::textChanged, []( const QString& text ){
		if ( g_scenegraphProxyModel != nullptr ) {
			g_scenegraphProxyModel->setFilterFixedString( text );
			if ( !text.isEmpty() ) {
				ScenegraphInspector_expandAll();
			}
			ScenegraphInspector_revealSelection();
		}
	} );
	QObject::connect( g_scenegraphTreeView, &QTreeView::doubleClicked, []( const QModelIndex& index ){
		if ( index.isValid() ) {
			GlobalCamera_FocusOnSelected();
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

	typedef FreeCaller<void(const Selectable&), ScenegraphInspector_selectionChanged> ScenegraphInspectorSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback( ScenegraphInspectorSelectionChangedCaller() );
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
