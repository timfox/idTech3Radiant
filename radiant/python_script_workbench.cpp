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

#include "python_script_workbench.h"

#include "mainframe.h"
#include "map.h"
#include "os/path.h"
#include "preferences.h"
#include "stream/stringstream.h"

#include <QMainWindow>
#include <QDockWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QProcess>
#include <QProcessEnvironment>
#include <QMessageBox>
#include <QSplitter>
#include <QDir>
#include <QFont>
#include <QTextStream>

namespace
{

QDockWidget* g_pythonScriptDock{};
QPlainTextEdit* g_pythonScriptEditor{};
QPlainTextEdit* g_pythonScriptOutput{};
QLabel* g_pythonScriptStatusLabel{};
QString g_pythonScriptCurrentPath;
bool g_pythonScriptDirty{};
QProcess* g_pythonProcess{};

const char* const c_pythonScriptSettingsPrefix = "PythonScriptWorkbench/";

QSettings& PythonScript_settings(){
	static QSettings settings;
	return settings;
}

QString PythonScript_setting( const char* key, const QString& fallback = {} ){
	return PythonScript_settings().value( StringStream( c_pythonScriptSettingsPrefix, key ).c_str(), fallback ).toString();
}

void PythonScript_setSetting( const char* key, const QVariant& value ){
	PythonScript_settings().setValue( StringStream( c_pythonScriptSettingsPrefix, key ).c_str(), value );
}

QString PythonScript_defaultDirectory(){
	const QString last = PythonScript_setting( "LastDirectory" );
	if ( !last.isEmpty() ) {
		return last;
	}
	return QDir( QString::fromLatin1( GlobalRadiant().getAppPath() ) ).filePath( "scripts" );
}

void PythonScript_setLastDirectory( const QString& path ){
	if ( path.isEmpty() ) {
		return;
	}
	const QFileInfo info( path );
	const QString directory = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
	if ( !directory.isEmpty() ) {
		PythonScript_setSetting( "LastDirectory", directory );
	}
}

QString PythonScript_pythonExecutable(){
	const QString configured = QString::fromUtf8( g_pythonExecutable.c_str() );
	if ( !configured.isEmpty() ) {
		return configured;
	}
#ifdef WIN32
	return "python";
#else
	return "python3";
#endif
}

void PythonScript_updateDockTitle(){
	if ( g_pythonScriptDock == nullptr ) {
		return;
	}
	const QString marker = g_pythonScriptDirty ? "*" : "";
	if ( g_pythonScriptCurrentPath.isEmpty() ) {
		g_pythonScriptDock->setWindowTitle( StringStream( "Python Script", marker.toUtf8().constData() ).c_str() );
	}
	else{
		const auto name = QFileInfo( g_pythonScriptCurrentPath ).fileName();
		g_pythonScriptDock->setWindowTitle( StringStream( "Python Script - ", name.toUtf8().constData(), marker.toUtf8().constData() ).c_str() );
	}
}

void PythonScript_setDirty( bool dirty ){
	g_pythonScriptDirty = dirty;
	PythonScript_updateDockTitle();
}

void PythonScript_markDirty(){
	PythonScript_setDirty( true );
}

void PythonScript_appendOutput( const QString& text ){
	if ( g_pythonScriptOutput != nullptr ) {
		g_pythonScriptOutput->appendPlainText( text );
		g_pythonScriptOutput->verticalScrollBar()->setValue( g_pythonScriptOutput->verticalScrollBar()->maximum() );
	}
}

void PythonScript_setStatus( const QString& text ){
	if ( g_pythonScriptStatusLabel != nullptr ) {
		g_pythonScriptStatusLabel->setText( text );
	}
}

void PythonScript_runFinished( int exitCode, QProcess::ExitStatus status ){
	if ( g_pythonProcess != nullptr ) {
		g_pythonProcess->deleteLater();
		g_pythonProcess = nullptr;
	}
	if ( status == QProcess::NormalExit ) {
		PythonScript_setStatus( StringStream( "Finished (exit code ", exitCode, ")" ).c_str() );
	}
	else{
		PythonScript_setStatus( "Process crashed or failed" );
	}
}

void PythonScript_run(){
	if ( g_pythonScriptEditor == nullptr ) {
		return;
	}
	if ( g_pythonProcess != nullptr ) {
		PythonScript_appendOutput( "\n--- Run already in progress ---\n" );
		return;
	}

	const QString pythonExe = PythonScript_pythonExecutable();

	QString scriptPath = g_pythonScriptCurrentPath;
	if ( scriptPath.isEmpty() ) {
		scriptPath = PythonScript_defaultDirectory() + "/untitled.py";
		QFile file( scriptPath );
		if ( file.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream out( &file );
			out << g_pythonScriptEditor->toPlainText();
			file.close();
		}
		else{
			PythonScript_appendOutput( "Error: Cannot run unsaved script. Save the script first.\n" );
			PythonScript_setStatus( "Save required" );
			return;
		}
	}

	PythonScript_appendOutput( StringStream( "\n--- Running ", scriptPath.toUtf8().constData(), " ---\n" ).c_str() );
	PythonScript_setStatus( "Running..." );

	g_pythonProcess = new QProcess( g_pythonScriptDock );
	QObject::connect( g_pythonProcess, QOverload<int, QProcess::ExitStatus>::of( &QProcess::finished ),
	                 []( int code, QProcess::ExitStatus status ){ PythonScript_runFinished( code, status ); } );
	QObject::connect( g_pythonProcess, &QProcess::errorOccurred,
	                 []( QProcess::ProcessError error ){
		if ( error == QProcess::FailedToStart && g_pythonProcess != nullptr ) {
			PythonScript_appendOutput( StringStream( "Error: Failed to start Python: ", g_pythonProcess->errorString().toUtf8().constData() ).c_str() );
			g_pythonProcess->deleteLater();
			g_pythonProcess = nullptr;
			PythonScript_setStatus( "Failed to start" );
		}
	} );
	QObject::connect( g_pythonProcess, &QProcess::readyReadStandardOutput, [](){
		if ( g_pythonProcess != nullptr ) {
			PythonScript_appendOutput( QString::fromUtf8( g_pythonProcess->readAllStandardOutput() ) );
		}
	} );
	QObject::connect( g_pythonProcess, &QProcess::readyReadStandardError, [](){
		if ( g_pythonProcess != nullptr ) {
			PythonScript_appendOutput( QString::fromUtf8( g_pythonProcess->readAllStandardError() ) );
		}
	} );

	const QFileInfo scriptInfo( scriptPath );
	g_pythonProcess->setWorkingDirectory( scriptInfo.absolutePath() );

	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	const QString appPath = QString::fromUtf8( GlobalRadiant().getAppPath() );
	const QString enginePath = QString::fromUtf8( GlobalRadiant().getEnginePath() );
	const QString gamePath = QString::fromUtf8( GlobalRadiant().getGameToolsPath() );
	const QString mapsPath = QString::fromUtf8( GlobalRadiant().getMapsPath() );
	if ( !appPath.isEmpty() ) {
		env.insert( "RADIANT_APP_PATH", appPath );
		const QString scriptsPath = QDir( appPath ).filePath( "scripts" );
		QString pythonPath = env.value( "PYTHONPATH" );
		if ( !pythonPath.isEmpty() ) {
			pythonPath = scriptsPath + QDir::listSeparator() + pythonPath;
		}
		else{
			pythonPath = scriptsPath;
		}
		env.insert( "PYTHONPATH", pythonPath );
	}
	if ( !enginePath.isEmpty() ) {
		env.insert( "RADIANT_ENGINE_PATH", enginePath );
	}
	if ( !gamePath.isEmpty() ) {
		env.insert( "RADIANT_GAME_PATH", gamePath );
	}
	if ( !mapsPath.isEmpty() ) {
		env.insert( "RADIANT_MAPS_PATH", mapsPath );
	}
	const char* mapName = GlobalRadiant().getMapName();
	if ( mapName != nullptr && *mapName != '\0' ) {
		env.insert( "RADIANT_CURRENT_MAP", QString::fromUtf8( path_get_filename_start( mapName ) ) );
		env.insert( "RADIANT_CURRENT_MAP_PATH", QString::fromUtf8( mapName ) );
	}
	g_pythonProcess->setProcessEnvironment( env );

	g_pythonProcess->start( pythonExe, { scriptInfo.absoluteFilePath() } );
}

bool PythonScript_loadFromPath( const QString& path ){
	if ( g_pythonScriptEditor == nullptr || path.isEmpty() ) {
		return false;
	}
	QFile file( path );
	if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		return false;
	}
	QTextStream in( &file );
	g_pythonScriptEditor->setPlainText( in.readAll() );
	file.close();
	g_pythonScriptCurrentPath = QFileInfo( path ).absoluteFilePath();
	PythonScript_setLastDirectory( g_pythonScriptCurrentPath );
	PythonScript_setDirty( false );
	return true;
}

bool PythonScript_saveToPath( const QString& path, bool updatePath ){
	if ( g_pythonScriptEditor == nullptr || path.isEmpty() ) {
		return false;
	}
	QFile file( path );
	if ( !file.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
		return false;
	}
	QTextStream out( &file );
	out << g_pythonScriptEditor->toPlainText();
	file.close();
	if ( updatePath ) {
		g_pythonScriptCurrentPath = QFileInfo( path ).absoluteFilePath();
		PythonScript_setLastDirectory( g_pythonScriptCurrentPath );
	}
	PythonScript_setDirty( false );
	return true;
}

bool PythonScript_save(){
	if ( !g_pythonScriptDirty ) {
		return true;
	}
	if ( !g_pythonScriptCurrentPath.isEmpty() ) {
		return PythonScript_saveToPath( g_pythonScriptCurrentPath, true );
	}
	const QString path = QFileDialog::getSaveFileName( g_pythonScriptDock, "Save Python Script",
	                                                  PythonScript_defaultDirectory(),
	                                                  "Python Files (*.py);;All Files (*)" );
	if ( path.isEmpty() ) {
		return false;
	}
	return PythonScript_saveToPath( path, true );
}

void PythonScript_new(){
	if ( g_pythonScriptEditor == nullptr ) {
		return;
	}
	g_pythonScriptEditor->clear();
	g_pythonScriptCurrentPath.clear();
	PythonScript_setDirty( false );
	if ( g_pythonScriptOutput != nullptr ) {
		g_pythonScriptOutput->clear();
	}
}

void PythonScript_openFile(){
	if ( g_pythonScriptDock == nullptr ) {
		return;
	}
	const QString path = QFileDialog::getOpenFileName( g_pythonScriptDock, "Open Python Script",
	                                                  PythonScript_defaultDirectory(),
	                                                  "Python Files (*.py);;All Files (*)" );
	if ( !path.isEmpty() ) {
		PythonScript_loadFromPath( path );
		if ( g_pythonScriptOutput != nullptr ) {
			g_pythonScriptOutput->clear();
		}
	}
}

void PythonScript_saveFile(){
	if ( g_pythonScriptDock == nullptr ) {
		return;
	}
	if ( g_pythonScriptCurrentPath.isEmpty() ) {
		const QString path = QFileDialog::getSaveFileName( g_pythonScriptDock, "Save Python Script",
		                                                  PythonScript_defaultDirectory(),
		                                                  "Python Files (*.py);;All Files (*)" );
		if ( !path.isEmpty() ) {
			PythonScript_saveToPath( path, true );
		}
	}
	else{
		PythonScript_save();
	}
}

}

void PythonScript_createDock( QMainWindow* window ){
	if ( window == nullptr || g_pythonScriptDock != nullptr ) {
		return;
	}

	g_pythonScriptDock = new QDockWidget( "Python Script", window );
	g_pythonScriptDock->setObjectName( "dock_python_script_workbench" );

	auto* root = new QWidget( g_pythonScriptDock );
	auto* layout = new QVBoxLayout( root );
	layout->setContentsMargins( 4, 4, 4, 4 );

	auto* toolbar = new QHBoxLayout();
	auto* newButton = new QPushButton( "New", root );
	auto* openButton = new QPushButton( "Open", root );
	auto* saveButton = new QPushButton( "Save", root );
	auto* runButton = new QPushButton( "Run", root );
	runButton->setStyleSheet( "font-weight: bold;" );
	toolbar->addWidget( newButton );
	toolbar->addWidget( openButton );
	toolbar->addWidget( saveButton );
	toolbar->addWidget( runButton );
	toolbar->addStretch();
	layout->addLayout( toolbar );

	auto* splitter = new QSplitter( Qt::Vertical, root );

	g_pythonScriptEditor = new QPlainTextEdit( root );
	g_pythonScriptEditor->setPlaceholderText(
		"# Python scripts can automate level editing tasks.\n"
		"# Use 'import radiant' for path helpers and utilities.\n"
		"# Example:\n"
		"#   import radiant\n"
		"#   print(radiant.engine_path())\n"
		"print('Hello from Radiant!')"
	);
	g_pythonScriptEditor->setFont( QFont( "Monospace", 9 ) );
	g_pythonScriptEditor->setTabStopDistance( 24 );
	QObject::connect( g_pythonScriptEditor, &QPlainTextEdit::textChanged, PythonScript_markDirty );
	splitter->addWidget( g_pythonScriptEditor );

	g_pythonScriptOutput = new QPlainTextEdit( root );
	g_pythonScriptOutput->setReadOnly( true );
	g_pythonScriptOutput->setMaximumBlockCount( 10000 );
	g_pythonScriptOutput->setFont( QFont( "Monospace", 9 ) );
	g_pythonScriptOutput->setPlaceholderText( "Output will appear here when you run a script." );
	splitter->addWidget( g_pythonScriptOutput );

	splitter->setSizes( { 400, 150 } );
	layout->addWidget( splitter, 1 );

	g_pythonScriptStatusLabel = new QLabel( "Ready", root );
	layout->addWidget( g_pythonScriptStatusLabel );

	QObject::connect( newButton, &QPushButton::clicked, PythonScript_new );
	QObject::connect( openButton, &QPushButton::clicked, PythonScript_openFile );
	QObject::connect( saveButton, &QPushButton::clicked, PythonScript_saveFile );
	QObject::connect( runButton, &QPushButton::clicked, PythonScript_run );

	g_pythonScriptDock->setWidget( root );
	window->addDockWidget( Qt::BottomDockWidgetArea, g_pythonScriptDock );
	g_pythonScriptDock->hide();

	PythonScript_new();
}

void PythonScript_open(){
	if ( g_pythonScriptDock == nullptr ) {
		return;
	}
	g_pythonScriptDock->show();
	g_pythonScriptDock->raise();
}

void PythonScript_stopAndRelease(){
	if ( g_pythonProcess != nullptr ) {
		g_pythonProcess->terminate();
		g_pythonProcess->waitForFinished( 2000 );
		if ( g_pythonProcess != nullptr ) {
			g_pythonProcess->kill();
			g_pythonProcess->deleteLater();
			g_pythonProcess = nullptr;
		}
	}
	g_pythonScriptDock = nullptr;
	g_pythonScriptEditor = nullptr;
	g_pythonScriptOutput = nullptr;
	g_pythonScriptStatusLabel = nullptr;
}
