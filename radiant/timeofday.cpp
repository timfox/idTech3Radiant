/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
any prior sources.
===========================================================================
*/

//
// Time of Day - configure sun/ambient lighting for worldspawn and generate q3map_sun directives
//

#include "timeofday.h"

#include "debugging/debugging.h"

#include "ientity.h"
#include "iscenegraph.h"
#include "iundo.h"
#include "math/vector.h"
#include "stringio.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QApplication>
#include <QClipboard>
#include <QFrame>

#include "gtkutil/guisettings.h"
#include "gtkutil/spinbox.h"
#include "stream/stringstream.h"
#include "string/string.h"

#include "commands.h"
#include "map.h"
#include "mainframe.h"
#include "qtmisc.h"
#include "scenelib.h"


static QDialog* g_timeOfDayDialog{};
static QDoubleSpinBox* g_sunAngle{};
static QDoubleSpinBox* g_sunElevation{};
static QDoubleSpinBox* g_sunIntensity{};
static QDoubleSpinBox* g_sunR{};
static QDoubleSpinBox* g_sunG{};
static QDoubleSpinBox* g_sunB{};
static QDoubleSpinBox* g_ambientR{};
static QDoubleSpinBox* g_ambientG{};
static QDoubleSpinBox* g_ambientB{};
static QDoubleSpinBox* g_minLight{};
static QPushButton* g_ambientColorBtn{};
static QPushButton* g_sunColorBtn{};


static void TimeOfDay_applyPreset( float sunAngle, float sunElev, float sunR, float sunG, float sunB, float sunInt,
                                   float ambR, float ambG, float ambB, float minLight ){
	if ( g_sunAngle ) g_sunAngle->setValue( sunAngle );
	if ( g_sunElevation ) g_sunElevation->setValue( sunElev );
	if ( g_sunR ) g_sunR->setValue( sunR );
	if ( g_sunG ) g_sunG->setValue( sunG );
	if ( g_sunB ) g_sunB->setValue( sunB );
	if ( g_sunIntensity ) g_sunIntensity->setValue( sunInt );
	if ( g_ambientR ) g_ambientR->setValue( ambR );
	if ( g_ambientG ) g_ambientG->setValue( ambG );
	if ( g_ambientB ) g_ambientB->setValue( ambB );
	if ( g_minLight ) g_minLight->setValue( minLight );
}


static void TimeOfDay_applyToWorldspawn(){
	scene::Node* worldNode = Map_FindWorldspawn( g_map );
	if ( worldNode == nullptr ) {
		return;
	}
	Entity* worldspawn = Node_getEntity( *worldNode );
	if ( worldspawn == nullptr ) {
		return;
	}

	UndoableCommand undo( "TimeOfDay" );

	char buf[128];
	snprintf( buf, sizeof( buf ), "%g %g %g", g_ambientR->value(), g_ambientG->value(), g_ambientB->value() );
	worldspawn->setKeyValue( "_color", buf );
	snprintf( buf, sizeof( buf ), "%g", g_minLight->value() );
	worldspawn->setKeyValue( "_minlight", buf );

	SceneChangeNotify();
}


static void TimeOfDay_copyQ3mapSun(){
	// q3map_sun r g b intensity degrees elevation
	// q3map_sunExt r g b intensity degrees elevation deviance samples
	const float r = g_sunR->value();
	const float g = g_sunG->value();
	const float b = g_sunB->value();
	const float intensity = g_sunIntensity->value();
	const float degrees = g_sunAngle->value();
	const float elevation = g_sunElevation->value();

	StringOutputStream buf( 256 );
	buf << "q3map_sunExt " << r << " " << g << " " << b << " " << intensity
	    << " " << degrees << " " << elevation << " 2 16";

	QApplication::clipboard()->setText( buf.c_str() );
}


void TimeOfDay_constructWindow( QWidget* parent ){
	if ( g_timeOfDayDialog != nullptr ) {
		g_timeOfDayDialog->show();
		g_timeOfDayDialog->raise();
		g_timeOfDayDialog->activateWindow();
		return;
	}

	g_timeOfDayDialog = new QDialog( parent );
	g_timeOfDayDialog->setWindowTitle( "Time of Day" );
	g_timeOfDayDialog->setAttribute( Qt::WA_DeleteOnClose, false );
	g_guiSettings.addWindow( g_timeOfDayDialog, "TimeOfDay" );

	auto* vbox = new QVBoxLayout( g_timeOfDayDialog );

	// Presets
	{
		auto* presetGroup = new QGroupBox( "Presets", g_timeOfDayDialog );
		auto* presetLayout = new QHBoxLayout( presetGroup );
		auto addPreset = []( const char* label, float angle, float elev, float sr, float sg, float sb, float si,
		                     float ar, float ag, float ab, float ml ){
			auto* btn = new QPushButton( label );
			QObject::connect( btn, &QPushButton::clicked, [=](){
				TimeOfDay_applyPreset( angle, elev, sr, sg, sb, si, ar, ag, ab, ml );
			} );
			return btn;
		};
		presetLayout->addWidget( addPreset( "Sunrise", 90, 5, 1.0f, 0.7f, 0.4f, 100, 0.6f, 0.5f, 0.4f, 0.25f ) );
		presetLayout->addWidget( addPreset( "Noon", 180, 60, 1.0f, 1.0f, 1.0f, 150, 0.9f, 0.9f, 1.0f, 0.15f ) );
		presetLayout->addWidget( addPreset( "Sunset", 270, 5, 1.0f, 0.5f, 0.3f, 90, 0.5f, 0.4f, 0.35f, 0.2f ) );
		presetLayout->addWidget( addPreset( "Night", 0, -20, 0.2f, 0.25f, 0.4f, 30, 0.1f, 0.12f, 0.2f, 0.08f ) );
		presetLayout->addWidget( addPreset( "Overcast", 180, 35, 0.8f, 0.85f, 0.9f, 80, 0.7f, 0.72f, 0.78f, 0.35f ) );
		vbox->addWidget( presetGroup );
	}

	// Sun
	{
		auto* sunGroup = new QGroupBox( "Sun (q3map_sun - paste into sky shader)", g_timeOfDayDialog );
		auto* sunForm = new QFormLayout( sunGroup );
		g_sunAngle = new DoubleSpinBox( -360, 360, 180, 1, 5, true );
		g_sunAngle->setToolTip( "0=East, 90=North, 180=West, 270=South" );
		sunForm->addRow( "Angle (degrees)", g_sunAngle );
		g_sunElevation = new DoubleSpinBox( -90, 90, 45, 1, 5, false );
		g_sunElevation->setToolTip( "0=horizon, 90=zenith" );
		sunForm->addRow( "Elevation (degrees)", g_sunElevation );
		g_sunIntensity = new DoubleSpinBox( 0, 500, 120, 0, 10, false );
		sunForm->addRow( "Intensity", g_sunIntensity );
		auto* sunColorBox = new QHBoxLayout;
		g_sunR = new DoubleSpinBox( 0, 1, 1, 2, 0.05, false );
		g_sunG = new DoubleSpinBox( 0, 1, 1, 2, 0.05, false );
		g_sunB = new DoubleSpinBox( 0, 1, 1, 2, 0.05, false );
		g_sunColorBtn = new QPushButton( "Pick..." );
		sunColorBox->addWidget( g_sunR );
		sunColorBox->addWidget( g_sunG );
		sunColorBox->addWidget( g_sunB );
		sunColorBox->addWidget( g_sunColorBtn );
		sunForm->addRow( "Color (R G B)", sunColorBox );
		QObject::connect( g_sunColorBtn, &QPushButton::clicked, [](){
			Vector3 c( g_sunR->value(), g_sunG->value(), g_sunB->value() );
			if ( color_dialog( g_timeOfDayDialog, c, "Sun Color" ) ) {
				g_sunR->setValue( c[0] );
				g_sunG->setValue( c[1] );
				g_sunB->setValue( c[2] );
			}
		} );
		auto* copyBtn = new QPushButton( "Copy q3map_sunExt to clipboard" );
		QObject::connect( copyBtn, &QPushButton::clicked, &TimeOfDay_copyQ3mapSun );
		sunForm->addRow( "", copyBtn );
		vbox->addWidget( sunGroup );
	}

	// Ambient (worldspawn)
	{
		auto* ambGroup = new QGroupBox( "Ambient (worldspawn _color, _minlight)", g_timeOfDayDialog );
		auto* ambForm = new QFormLayout( ambGroup );
		auto* ambColorBox = new QHBoxLayout;
		g_ambientR = new DoubleSpinBox( 0, 1, 0.9, 2, 0.05, false );
		g_ambientG = new DoubleSpinBox( 0, 1, 0.9, 2, 0.05, false );
		g_ambientB = new DoubleSpinBox( 0, 1, 1, 2, 0.05, false );
		g_ambientColorBtn = new QPushButton( "Pick..." );
		ambColorBox->addWidget( g_ambientR );
		ambColorBox->addWidget( g_ambientG );
		ambColorBox->addWidget( g_ambientB );
		ambColorBox->addWidget( g_ambientColorBtn );
		ambForm->addRow( "Ambient color (_color)", ambColorBox );
		QObject::connect( g_ambientColorBtn, &QPushButton::clicked, [](){
			Vector3 c( g_ambientR->value(), g_ambientG->value(), g_ambientB->value() );
			if ( color_dialog( g_timeOfDayDialog, c, "Ambient Color" ) ) {
				g_ambientR->setValue( c[0] );
				g_ambientG->setValue( c[1] );
				g_ambientB->setValue( c[2] );
			}
		} );
		g_minLight = new DoubleSpinBox( 0, 1, 0.2, 2, 0.05, false );
		g_minLight->setToolTip( "Minimum light in shadows (0-1)" );
		ambForm->addRow( "Min light (_minlight)", g_minLight );
		auto* applyBtn = new QPushButton( "Apply to Worldspawn" );
		QObject::connect( applyBtn, &QPushButton::clicked, &TimeOfDay_applyToWorldspawn );
		ambForm->addRow( "", applyBtn );
		vbox->addWidget( ambGroup );
	}

	// Load from worldspawn
	{
		auto* loadBtn = new QPushButton( "Load from Worldspawn" );
		QObject::connect( loadBtn, &QPushButton::clicked, [](){
			scene::Node* worldNode = Map_FindWorldspawn( g_map );
			if ( worldNode == nullptr ) return;
			Entity* worldspawn = Node_getEntity( *worldNode );
			if ( worldspawn == nullptr ) return;
			Vector3 color( 1, 1, 1 );
			string_parse_vector3( worldspawn->getKeyValue( "_color" ), color );
			if ( g_ambientR ) g_ambientR->setValue( color[0] );
			if ( g_ambientG ) g_ambientG->setValue( color[1] );
			if ( g_ambientB ) g_ambientB->setValue( color[2] );
			float ml = 0;
			string_parse_float( worldspawn->getKeyValue( "_minlight" ), ml );
			if ( g_minLight ) g_minLight->setValue( ml );
		} );
		vbox->addWidget( loadBtn );
	}

	{
		auto* buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Close );
		QObject::connect( buttons, &QDialogButtonBox::rejected, g_timeOfDayDialog, &QDialog::hide );
		vbox->addWidget( buttons );
	}

	g_timeOfDayDialog->show();
}


void TimeOfDay_destroyWindow(){
	if ( g_timeOfDayDialog != nullptr ) {
		g_timeOfDayDialog->deleteLater();
		g_timeOfDayDialog = nullptr;
		g_sunAngle = nullptr;
		g_sunElevation = nullptr;
		g_sunIntensity = nullptr;
		g_sunR = g_sunG = g_sunB = nullptr;
		g_ambientR = g_ambientG = g_ambientB = nullptr;
		g_minLight = nullptr;
		g_ambientColorBtn = nullptr;
		g_sunColorBtn = nullptr;
	}
}


static void TimeOfDay_show(){
	TimeOfDay_constructWindow( MainFrame_getWindow() );
}


void TimeOfDay_Construct(){
	GlobalCommands_insert( "TimeOfDay", makeCallbackF( TimeOfDay_show ), QKeySequence( "Ctrl+Shift+T" ) );
}


void TimeOfDay_Destroy(){
	TimeOfDay_destroyWindow();
}
