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

#include "console.h"

#include <chrono>
#include <ctime>
#include <utility>

#include "gtkutil/accelerator.h"
#include "gtkutil/messagebox.h"
#include "stream/stringstream.h"

#include "version.h"
#include "aboutmsg.h"
#include "mainframe.h"

#include <QPlainTextEdit>
#include <QContextMenuEvent>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QRegularExpression>
#include <QSocketNotifier>

#ifndef WIN32
#include <cerrno>
#include <fcntl.h>
#include <unistd.h> // write()
#endif

// handle to the console log file
namespace
{
FILE* g_hLogFile;
}

// called whenever we need to open/close/check the console log file
void Sys_LogFile( bool enable ){
	if ( enable && !g_hLogFile ) {
		// we should be logging and we don't have a log file .. so create it
		if ( string_empty( SettingsPath_get() ) ) {
			return; // cannot open a log file yet
		}
		// open a file to log the console
		// the file handle is g_hLogFile
		// the log file is erased
		const auto name = StringStream( SettingsPath_get(), "radiant.log" );
		g_hLogFile = fopen( name, "w" );
		if ( g_hLogFile != 0 ) {
			globalOutputStream() << "Started logging to " << name << '\n';
			time_t localtime;
			time( &localtime );
			globalOutputStream() << "Today is: " << ctime( &localtime )
			                     << "This is NetRadiant '" RADIANT_VERSION "' compiled " __DATE__ "\n" RADIANT_ABOUTMSG "\n";
		}
		else{
			qt_MessageBox( 0, "Failed to create log file, check write permissions in Radiant directory.\n",
			                "Console logging", EMessageBoxType::Error );
		}
	}
	else if ( !enable && g_hLogFile != 0 ) {
		// we should not be logging but still we have an active logfile .. close it
		time_t localtime;
		time( &localtime );
		globalOutputStream() << "Closing log file at " << ctime( &localtime ) << '\n';
		fclose( g_hLogFile );
		g_hLogFile = 0;
	}
}

QPlainTextEdit* g_console = 0;

namespace
{
QWidget* g_consoleContainer = nullptr;
QProgressBar* g_buildProgressBar = nullptr;
QLabel* g_buildProgressLabel = nullptr;
QTimer* g_buildElapsedTimer = nullptr;
std::chrono::steady_clock::time_point g_buildStartTime;

#ifndef WIN32
int g_consoleStdoutReadFd = -1;
int g_consoleStderrReadFd = -1;
int g_consoleStdoutTerminalFd = -1;
int g_consoleStderrTerminalFd = -1;
QSocketNotifier* g_consoleStdoutNotifier = nullptr;
QSocketNotifier* g_consoleStderrNotifier = nullptr;
bool g_consoleStdStreamsCaptured = false;

void Console_closeFd( int& fd ){
	if ( fd != -1 ) {
		close( fd );
		fd = -1;
	}
}

bool Console_setNonBlocking( int fd ){
	const int flags = fcntl( fd, F_GETFL, 0 );
	if ( flags == -1 ) {
		return false;
	}
	return fcntl( fd, F_SETFL, flags | O_NONBLOCK ) != -1;
}

int Console_terminalFd( int level ){
	switch ( level )
	{
	case SYS_WRN:
	case SYS_ERR:
		return g_consoleStderrTerminalFd != -1 ? g_consoleStderrTerminalFd : STDERR_FILENO;
	case SYS_STD:
	case SYS_VRB:
	default:
		return g_consoleStdoutTerminalFd != -1 ? g_consoleStdoutTerminalFd : STDOUT_FILENO;
	}
}
#endif
}

class QPlainTextEdit_console : public QPlainTextEdit
{
protected:
	void contextMenuEvent( QContextMenuEvent *event ) override {
		QMenu *menu = createStandardContextMenu();
		connect( menu->addAction( "Copy All" ), &QAction::triggered, [this](){ QApplication::clipboard()->setText( toPlainText() ); } );
		connect( menu->addAction( "Clear" ), &QAction::triggered, this, &QPlainTextEdit::clear );
		menu->addSeparator();
		connect( menu->addAction( "Find next error" ), &QAction::triggered, [](){ Console_findNext( "error|Error|ERROR|leak|Leak|LEAK|fail|Fail|FAIL" ); } );
		connect( menu->addAction( "Find next warning" ), &QAction::triggered, [](){ Console_findNext( "warning|Warning|WARNING" ); } );
		menu->exec( event->globalPos() );
		delete menu;
	}
};


QWidget* Console_constructWindow(){
	auto *container = new QWidget;
	auto *vbox = new QVBoxLayout( container );
	vbox->setContentsMargins( 0, 0, 0, 0 );

	{
		auto *progressFrame = new QWidget;
		auto *progressHbox = new QHBoxLayout( progressFrame );
		progressHbox->setContentsMargins( 4, 2, 4, 2 );
		g_buildProgressBar = new QProgressBar;
		g_buildProgressBar->setRange( 0, 100 );
		g_buildProgressBar->setTextVisible( true );
		g_buildProgressBar->setMinimumWidth( 120 );
		g_buildProgressBar->setVisible( false );
		g_buildProgressLabel = new QLabel( "Ready" );
		g_buildProgressLabel->setStyleSheet( "QLabel { color: #888; }" );
		progressHbox->addWidget( g_buildProgressLabel );

		auto *findErrorBtn = new QPushButton( "Find Error" );
		findErrorBtn->setToolTip( "Jump to next error, leak, or failure in output" );
		QObject::connect( findErrorBtn, &QPushButton::clicked, [](){ Console_findNext( "error|Error|ERROR|leak|Leak|LEAK|fail|Fail|FAIL" ); } );
		progressHbox->addWidget( findErrorBtn );

		auto *findWarningBtn = new QPushButton( "Find Warning" );
		findWarningBtn->setToolTip( "Jump to next warning in output" );
		QObject::connect( findWarningBtn, &QPushButton::clicked, [](){ Console_findNext( "warning|Warning|WARNING" ); } );
		progressHbox->addWidget( findWarningBtn );

		progressHbox->addWidget( g_buildProgressBar, 1 );
		vbox->addWidget( progressFrame );

		g_consoleContainer = container;
		g_buildElapsedTimer = new QTimer( container );
		g_buildElapsedTimer->setInterval( 1000 );
		QObject::connect( g_buildElapsedTimer, &QTimer::timeout, [](){
			if ( g_buildProgressLabel && g_buildProgressBar->isVisible() ) {
				const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>( std::chrono::steady_clock::now() - g_buildStartTime ).count();
				const int min = static_cast<int>( elapsed / 60 );
				const int sec = static_cast<int>( elapsed % 60 );
				QString timeStr = QString( "%1:%2" ).arg( min, 2, 10, QChar( '0' ) ).arg( sec, 2, 10, QChar( '0' ) );
				QString text = g_buildProgressLabel->text();
				// Update elapsed in the label (format: "Step X/Y: Name | 1:23")
				if ( text.contains( " | " ) ) {
					text = text.left( text.indexOf( " | " ) ) + " | " + timeStr;
				} else {
					text = text + " | " + timeStr;
				}
				g_buildProgressLabel->setText( text );
			}
		} );
	}

	QPlainTextEdit *text = new QPlainTextEdit_console();
	text->setReadOnly( true );
	text->setUndoRedoEnabled( false );
	text->setMinimumHeight( 10 );
	text->setFocusPolicy( Qt::FocusPolicy::NoFocus );

	{
		g_console = text;
		text->connect( text, &QObject::destroyed, [](){ g_console = nullptr; } );
	}

	vbox->addWidget( text, 1 );

	return container;
}

void Console_buildProgressUpdate( int step, int total, const char* stepName ){
	if ( g_buildProgressBar == nullptr || g_buildProgressLabel == nullptr ) {
		return;
	}
	if ( step < 0 ) {
		g_buildProgressBar->setVisible( false );
		g_buildProgressLabel->setText( "Ready" );
		g_buildProgressLabel->setStyleSheet( "QLabel { color: #888; }" );
		if ( g_buildElapsedTimer ) {
			g_buildElapsedTimer->stop();
		}
		return;
	}
	g_buildProgressBar->setVisible( true );
	g_buildProgressLabel->setStyleSheet( "QLabel { color: #35ff6b; font-weight: bold; }" );
	g_buildProgressBar->setValue( total > 0 ? ( step * 100 / total ) : 0 );
	g_buildProgressLabel->setText( QString( "Step %1/%2: %3" ).arg( step + 1 ).arg( total ).arg( stepName ? stepName : "" ) );
	if ( step == 0 ) {
		g_buildStartTime = std::chrono::steady_clock::now();
		if ( g_buildElapsedTimer ) {
			g_buildElapsedTimer->start();
		}
	}
}

void Console_findNext( const char* pattern ){
	if ( g_console == nullptr ) {
		return;
	}
	QRegularExpression re( pattern, QRegularExpression::CaseInsensitiveOption );
	QTextCursor cursor = g_console->textCursor();
	cursor.movePosition( QTextCursor::MoveOperation::NextCharacter );
	g_console->setTextCursor( cursor );
	bool found = g_console->find( re );
	if ( !found ) {
		g_console->moveCursor( QTextCursor::MoveOperation::Start );
		found = g_console->find( re );
	}
	if ( found ) {
		g_console->ensureCursorVisible();
		g_console->setFocus();
	}
}

void Console_captureStdStreams(){
#ifndef WIN32
	if ( g_consoleStdStreamsCaptured ) {
		return;
	}

	int stdoutPipe[2] = { -1, -1 };
	int stderrPipe[2] = { -1, -1 };
	if ( pipe( stdoutPipe ) != 0 || pipe( stderrPipe ) != 0 ) {
		if ( stdoutPipe[0] != -1 ) close( stdoutPipe[0] );
		if ( stdoutPipe[1] != -1 ) close( stdoutPipe[1] );
		if ( stderrPipe[0] != -1 ) close( stderrPipe[0] );
		if ( stderrPipe[1] != -1 ) close( stderrPipe[1] );
		return;
	}

	g_consoleStdoutTerminalFd = dup( STDOUT_FILENO );
	g_consoleStderrTerminalFd = dup( STDERR_FILENO );
	if ( g_consoleStdoutTerminalFd == -1 || g_consoleStderrTerminalFd == -1 ) {
		Console_closeFd( g_consoleStdoutTerminalFd );
		Console_closeFd( g_consoleStderrTerminalFd );
		close( stdoutPipe[0] );
		close( stdoutPipe[1] );
		close( stderrPipe[0] );
		close( stderrPipe[1] );
		return;
	}

	if ( dup2( stdoutPipe[1], STDOUT_FILENO ) == -1 || dup2( stderrPipe[1], STDERR_FILENO ) == -1 ) {
		dup2( g_consoleStdoutTerminalFd, STDOUT_FILENO );
		dup2( g_consoleStderrTerminalFd, STDERR_FILENO );
		Console_closeFd( g_consoleStdoutTerminalFd );
		Console_closeFd( g_consoleStderrTerminalFd );
		close( stdoutPipe[0] );
		close( stdoutPipe[1] );
		close( stderrPipe[0] );
		close( stderrPipe[1] );
		return;
	}

	close( stdoutPipe[1] );
	close( stderrPipe[1] );
	g_consoleStdoutReadFd = stdoutPipe[0];
	g_consoleStderrReadFd = stderrPipe[0];
	Console_setNonBlocking( g_consoleStdoutReadFd );
	Console_setNonBlocking( g_consoleStderrReadFd );

	setvbuf( stdout, nullptr, _IOLBF, 0 );
	setvbuf( stderr, nullptr, _IONBF, 0 );

	g_consoleStdoutNotifier = new QSocketNotifier( g_consoleStdoutReadFd, QSocketNotifier::Read, qApp );
	QObject::connect( g_consoleStdoutNotifier, &QSocketNotifier::activated, []( int ){
		if ( g_consoleStdoutReadFd == -1 ) {
			return;
		}
		char buffer[4096];
		for (;; )
		{
			const auto readBytes = read( g_consoleStdoutReadFd, buffer, sizeof( buffer ) );
			if ( readBytes > 0 ) {
				Sys_Print( SYS_STD, buffer, static_cast<std::size_t>( readBytes ) );
			}
			else if ( readBytes == 0 || ( readBytes == -1 && ( errno == EAGAIN || errno == EWOULDBLOCK ) ) ) {
				break;
			}
			else{
				break;
			}
		}
	} );

	g_consoleStderrNotifier = new QSocketNotifier( g_consoleStderrReadFd, QSocketNotifier::Read, qApp );
	QObject::connect( g_consoleStderrNotifier, &QSocketNotifier::activated, []( int ){
		if ( g_consoleStderrReadFd == -1 ) {
			return;
		}
		char buffer[4096];
		for (;; )
		{
			const auto readBytes = read( g_consoleStderrReadFd, buffer, sizeof( buffer ) );
			if ( readBytes > 0 ) {
				Sys_Print( SYS_ERR, buffer, static_cast<std::size_t>( readBytes ) );
			}
			else if ( readBytes == 0 || ( readBytes == -1 && ( errno == EAGAIN || errno == EWOULDBLOCK ) ) ) {
				break;
			}
			else{
				break;
			}
		}
	} );

	g_consoleStdStreamsCaptured = true;
#endif
}

void Console_releaseStdStreams(){
#ifndef WIN32
	if ( g_consoleStdoutNotifier != nullptr ) {
		g_consoleStdoutNotifier->setEnabled( false );
		delete std::exchange( g_consoleStdoutNotifier, nullptr );
	}
	if ( g_consoleStderrNotifier != nullptr ) {
		g_consoleStderrNotifier->setEnabled( false );
		delete std::exchange( g_consoleStderrNotifier, nullptr );
	}

	fflush( stdout );
	fflush( stderr );

	if ( g_consoleStdoutTerminalFd != -1 ) {
		dup2( g_consoleStdoutTerminalFd, STDOUT_FILENO );
	}
	if ( g_consoleStderrTerminalFd != -1 ) {
		dup2( g_consoleStderrTerminalFd, STDERR_FILENO );
	}

	Console_closeFd( g_consoleStdoutReadFd );
	Console_closeFd( g_consoleStderrReadFd );
	Console_closeFd( g_consoleStdoutTerminalFd );
	Console_closeFd( g_consoleStderrTerminalFd );
	g_consoleStdStreamsCaptured = false;
#endif
}

//#pragma GCC push_options
//#pragma GCC optimize ("O0")

class GtkTextBufferOutputStream : public TextOutputStream
{
	QPlainTextEdit* textBuffer;
public:
	GtkTextBufferOutputStream( QPlainTextEdit* textBuffer ) : textBuffer( textBuffer ) {
	}
	std::size_t
#ifdef __GNUC__
//__attribute__((optimize("O0")))
#endif
	write( const char* buffer, std::size_t length ) override {
		textBuffer->insertPlainText( QString::fromLatin1( buffer, length ) );
		return length;
	}
};

//#pragma GCC pop_options

std::size_t Sys_Print( int level, const char* buf, std::size_t length ){
	const bool contains_newline = std::find( buf, buf + length, '\n' ) != buf + length;

	if ( level == SYS_ERR ) {
		Sys_LogFile( true );
	}

	if ( g_hLogFile != 0 ) {
		fwrite( buf, 1, length, g_hLogFile );
		if ( contains_newline ) {
			fflush( g_hLogFile );
		}
	}

	if ( level != SYS_NOCON ) {
#ifndef WIN32
		{  // on linux/macos log also to terminal
			(void)write( Console_terminalFd( level ), buf, length );
		}
#endif

		if ( g_console != 0 ) {
			g_console->moveCursor( QTextCursor::End ); // must go before setCurrentCharFormat() & insertPlainText()

			{
				QTextCharFormat format = g_console->currentCharFormat();
				switch ( level )
				{
				case SYS_WRN:
					format.setForeground( QColor( 255, 127, 0 ) );
					break;
				case SYS_ERR:
					format.setForeground( QColor( 255, 0, 0 ) );
					break;
				case SYS_STD:
				case SYS_VRB:
				default:
					format.clearForeground();
					break;
				}
				g_console->setCurrentCharFormat( format );
			}

			{
				GtkTextBufferOutputStream textBuffer( g_console );
				textBuffer << StringRange( buf, length );
			}

 			if ( contains_newline ) {
				g_console->ensureCursorVisible();

				// update console widget immediately if we're doing something time-consuming
				if ( !ScreenUpdates_Enabled() && g_console->isVisible() ) {
					ScreenUpdates_process();
				}
			}
 		}
	}
	return length;
}


template<int level>
class SysPrintStream : public TextOutputStream
{
public:
	std::size_t write( const char* buffer, std::size_t length ) override {
		return Sys_Print( level, buffer, length );
	}
};

TextOutputStream& getSysPrintOutputStream(){
	static SysPrintStream<SYS_STD> stream;
	return stream;
}

TextOutputStream& getSysPrintWarningStream(){
	static SysPrintStream<SYS_WRN> stream;
	return stream;
}

TextOutputStream& getSysPrintErrorStream(){
	static SysPrintStream<SYS_ERR> stream;
	return stream;
}
