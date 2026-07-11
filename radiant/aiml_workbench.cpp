#include "aiml_workbench.h"

#include "mainframe.h"
#include "stream/stringstream.h"

#include <QMainWindow>
#include <QDockWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QSettings>
#include <QSplitter>
#include <QDir>
#include <QMessageBox>
#include <QScrollBar>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QSignalBlocker>
#include <QTextCursor>
#include <QTextBlock>
#include <QFont>

namespace
{

QDockWidget* g_aimlDock{};
QPlainTextEdit* g_aimlEditor{};
QListWidget* g_aimlCategoryList{};
QLineEdit* g_aimlCategoryFilter{};
QLabel* g_aimlStatusLabel{};
QString g_aimlCurrentPath;
bool g_aimlDirty{};

struct AIMLCategoryEntry
{
	QString pattern;
	int line = 1;
};

QVector<AIMLCategoryEntry> g_aimlCategories;

const char* const c_aimlSettingsPrefix = "AIMLWorkbench/";

QSettings& AIMLWorkbench_settings(){
	static QSettings settings;
	return settings;
}

QString AIMLWorkbench_setting( const char* key, const QString& fallback = {} ){
	return AIMLWorkbench_settings().value( StringStream( c_aimlSettingsPrefix, key ).c_str(), fallback ).toString();
}

void AIMLWorkbench_setSetting( const char* key, const QVariant& value ){
	AIMLWorkbench_settings().setValue( StringStream( c_aimlSettingsPrefix, key ).c_str(), value );
}

QString AIMLWorkbench_defaultDirectory(){
	const QString last = AIMLWorkbench_setting( "LastDirectory" );
	if ( !last.isEmpty() ) {
		return last;
	}
	const QString gameTools = QString::fromLatin1( GameToolsPath_get() );
	if ( !gameTools.isEmpty() ) {
		return QDir( gameTools ).filePath( "aiml" );
	}
	return QDir( QString::fromLatin1( GlobalRadiant().getAppPath() ) ).filePath( "scripts" );
}

void AIMLWorkbench_setLastDirectory( const QString& path ){
	if ( path.isEmpty() ) {
		return;
	}
	const QFileInfo info( path );
	const QString directory = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
	if ( !directory.isEmpty() ) {
		AIMLWorkbench_setSetting( "LastDirectory", directory );
	}
}

QString AIMLWorkbench_defaultDocument(){
	return
R"(<?xml version="1.0" encoding="UTF-8"?>
<aiml version="3.0">
  <category>
    <pattern>HELLO</pattern>
    <template>Hello. How can I help you?</template>
  </category>

  <category>
    <pattern>WHAT IS THIS EDITOR</pattern>
    <template>This is the AIML 3.0 editor.</template>
  </category>
</aiml>
)";
}

void AIMLWorkbench_updateDockTitle(){
	if ( g_aimlDock == nullptr ) {
		return;
	}
	const QString marker = g_aimlDirty ? "*" : "";
	if ( g_aimlCurrentPath.isEmpty() ) {
		g_aimlDock->setWindowTitle( StringStream( "AIML 3.0 Editor", marker.toUtf8().constData() ).c_str() );
	}
	else{
		const QString name = QFileInfo( g_aimlCurrentPath ).fileName();
		g_aimlDock->setWindowTitle( StringStream( "AIML 3.0 Editor - ", name.toUtf8().constData(), marker.toUtf8().constData() ).c_str() );
	}
}

void AIMLWorkbench_setDirty( bool dirty ){
	g_aimlDirty = dirty;
	AIMLWorkbench_updateDockTitle();
}

void AIMLWorkbench_markDirty(){
	AIMLWorkbench_setDirty( true );
}

void AIMLWorkbench_setStatus( const QString& text ){
	if ( g_aimlStatusLabel != nullptr ) {
		g_aimlStatusLabel->setText( text );
	}
}

void AIMLWorkbench_gotoLine( int line ){
	if ( g_aimlEditor == nullptr ) {
		return;
	}
	QTextCursor cursor( g_aimlEditor->document()->findBlockByLineNumber( std::max( 0, line - 1 ) ) );
	if ( cursor.isNull() ) {
		return;
	}
	g_aimlEditor->setTextCursor( cursor );
	g_aimlEditor->centerCursor();
	g_aimlEditor->setFocus();
}

QVector<AIMLCategoryEntry> AIMLWorkbench_collectCategories( const QString& text ){
	QVector<AIMLCategoryEntry> out;
	QXmlStreamReader xml( text );
	bool inCategory = false;
	bool inPattern = false;
	QString patternText;
	int patternLine = 1;

	while ( !xml.atEnd() )
	{
		xml.readNext();
		if ( xml.isStartElement() ) {
			const QStringView name = xml.name();
			if ( name.compare( QStringLiteral( "category" ), Qt::CaseInsensitive ) == 0 ) {
				inCategory = true;
				patternText.clear();
				patternLine = int( xml.lineNumber() );
			}
			else if ( inCategory && name.compare( QStringLiteral( "pattern" ), Qt::CaseInsensitive ) == 0 ) {
				inPattern = true;
				patternLine = int( xml.lineNumber() );
			}
		}
		else if ( xml.isCharacters() && inPattern ) {
			patternText += xml.text().toString();
		}
		else if ( xml.isEndElement() ) {
			const QStringView name = xml.name();
			if ( name.compare( QStringLiteral( "pattern" ), Qt::CaseInsensitive ) == 0 ) {
				inPattern = false;
			}
			else if ( name.compare( QStringLiteral( "category" ), Qt::CaseInsensitive ) == 0 ) {
				inCategory = false;
				const QString trimmed = patternText.simplified();
				if ( !trimmed.isEmpty() ) {
					out.push_back( { trimmed, patternLine } );
				}
			}
		}
	}

	if ( out.empty() ) {
		const QRegularExpression rx( "<pattern>([^<]+)</pattern>", QRegularExpression::CaseInsensitiveOption );
		auto it = rx.globalMatch( text );
		while ( it.hasNext() )
		{
			const auto match = it.next();
			const QString pattern = match.captured( 1 ).simplified();
			if ( pattern.isEmpty() ) {
				continue;
			}
			int line = 1;
			const int start = match.capturedStart( 0 );
			for ( int i = 0; i < start && i < text.size(); ++i )
				if ( text[i] == '\n' )
					++line;
			out.push_back( { pattern, line } );
		}
	}

	return out;
}

void AIMLWorkbench_refreshCategoryList(){
	if ( g_aimlCategoryList == nullptr || g_aimlEditor == nullptr ) {
		return;
	}

	g_aimlCategories = AIMLWorkbench_collectCategories( g_aimlEditor->toPlainText() );
	const QString filter = g_aimlCategoryFilter != nullptr ? g_aimlCategoryFilter->text().trimmed() : QString();

	g_aimlCategoryList->clear();
	for ( const AIMLCategoryEntry& entry : g_aimlCategories )
	{
		if ( !filter.isEmpty() && !entry.pattern.contains( filter, Qt::CaseInsensitive ) ) {
			continue;
		}
		auto* item = new QListWidgetItem( entry.pattern, g_aimlCategoryList );
		item->setData( Qt::UserRole, entry.line );
		item->setToolTip( StringStream( "Line ", entry.line ).c_str() );
	}

	if ( g_aimlCategoryList->count() > 0 ) {
		g_aimlCategoryList->setCurrentRow( 0 );
	}

	AIMLWorkbench_setStatus( StringStream( "Ready - ", g_aimlCategories.size(), " categor", g_aimlCategories.size() == 1 ? "y" : "ies" ).c_str() );
}

bool AIMLWorkbench_validateDocument( QString* errorOut = nullptr, int* errorLineOut = nullptr ){
	if ( g_aimlEditor == nullptr ) {
		return false;
	}

	const QString text = g_aimlEditor->toPlainText();
	QXmlStreamReader xml( text );
	bool foundAimlRoot = false;
	bool version3 = false;

	while ( !xml.atEnd() )
	{
		xml.readNext();
		if ( xml.isStartElement() ) {
			if ( !foundAimlRoot ) {
				foundAimlRoot = xml.name().compare( QStringLiteral( "aiml" ), Qt::CaseInsensitive ) == 0;
				version3 = xml.attributes().value( "version" ).toString() == "3.0";
				if ( !foundAimlRoot ) {
					if ( errorOut != nullptr ) {
						*errorOut = "Root element must be <aiml>.";
					}
					if ( errorLineOut != nullptr ) {
						*errorLineOut = int( xml.lineNumber() );
					}
					return false;
				}
			}
		}
	}

	if ( xml.hasError() ) {
		if ( errorOut != nullptr ) {
			*errorOut = xml.errorString();
		}
		if ( errorLineOut != nullptr ) {
			*errorLineOut = int( xml.lineNumber() );
		}
		return false;
	}

	if ( !version3 ) {
		if ( errorOut != nullptr ) {
			*errorOut = "Document is valid XML, but <aiml version=\"3.0\"> is recommended.";
		}
		if ( errorLineOut != nullptr ) {
			*errorLineOut = 1;
		}
		return false;
	}

	return true;
}

void AIMLWorkbench_validateAndReport(){
	QString error;
	int line = 1;
	if ( AIMLWorkbench_validateDocument( &error, &line ) ) {
		AIMLWorkbench_setStatus( StringStream( "AIML valid - ", g_aimlCategories.size(), " categories" ).c_str() );
		QMessageBox::information( MainFrame_getWindow(), "AIML Validation", "Document is valid AIML 3.0 XML." );
	}
	else{
		AIMLWorkbench_setStatus( StringStream( "Validation issue at line ", line ).c_str() );
		AIMLWorkbench_gotoLine( line );
		QMessageBox::warning( MainFrame_getWindow(), "AIML Validation", StringStream( "Line ", line, ": ", error.toUtf8().constData() ).c_str() );
	}
}

void AIMLWorkbench_setText( const QString& text ){
	if ( g_aimlEditor == nullptr ) {
		return;
	}
	const QSignalBlocker blocker( g_aimlEditor );
	g_aimlEditor->setPlainText( text );
	AIMLWorkbench_refreshCategoryList();
	AIMLWorkbench_setDirty( false );
}

bool AIMLWorkbench_saveToPath( const QString& path ){
	if ( g_aimlEditor == nullptr || path.isEmpty() ) {
		return false;
	}
	QFile file( path );
	if ( !file.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) ) {
		QMessageBox::warning( MainFrame_getWindow(), "Save AIML", StringStream( "Could not write file:\n", path.toUtf8().constData() ).c_str() );
		return false;
	}

	QTextStream out( &file );
	out.setCodec( "UTF-8" );
	out << g_aimlEditor->toPlainText();
	file.close();

	g_aimlCurrentPath = QFileInfo( path ).absoluteFilePath();
	AIMLWorkbench_setLastDirectory( g_aimlCurrentPath );
	AIMLWorkbench_setSetting( "LastFile", g_aimlCurrentPath );
	AIMLWorkbench_setDirty( false );
	AIMLWorkbench_setStatus( StringStream( "Saved ", QFileInfo( path ).fileName().toUtf8().constData() ).c_str() );
	AIMLWorkbench_updateDockTitle();
	return true;
}

bool AIMLWorkbench_saveAs(){
	const QString path = QFileDialog::getSaveFileName(
		MainFrame_getWindow(),
		"Save AIML File",
		g_aimlCurrentPath.isEmpty() ? AIMLWorkbench_defaultDirectory() + "/bot.aiml" : g_aimlCurrentPath,
		"AIML Files (*.aiml *.xml);;All Files (*)" );
	if ( path.isEmpty() ) {
		return false;
	}
	return AIMLWorkbench_saveToPath( path );
}

bool AIMLWorkbench_save(){
	if ( g_aimlCurrentPath.isEmpty() ) {
		return AIMLWorkbench_saveAs();
	}
	return AIMLWorkbench_saveToPath( g_aimlCurrentPath );
}

bool AIMLWorkbench_promptSaveIfDirty( const char* title ){
	if ( !g_aimlDirty ) {
		return true;
	}
	const auto result = QMessageBox::question(
		MainFrame_getWindow(),
		title,
		"Save changes to the AIML document?",
		QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
		QMessageBox::Save );
	if ( result == QMessageBox::Cancel ) {
		return false;
	}
	if ( result == QMessageBox::Save ) {
		return AIMLWorkbench_save();
	}
	return true;
}

void AIMLWorkbench_newDocument(){
	if ( !AIMLWorkbench_promptSaveIfDirty( "New AIML Document" ) ) {
		return;
	}
	g_aimlCurrentPath.clear();
	AIMLWorkbench_setText( AIMLWorkbench_defaultDocument() );
	AIMLWorkbench_setStatus( "New AIML 3.0 document" );
}

void AIMLWorkbench_openFile(){
	if ( !AIMLWorkbench_promptSaveIfDirty( "Open AIML Document" ) ) {
		return;
	}

	const QString path = QFileDialog::getOpenFileName(
		MainFrame_getWindow(),
		"Open AIML File",
		g_aimlCurrentPath.isEmpty() ? AIMLWorkbench_defaultDirectory() : g_aimlCurrentPath,
		"AIML Files (*.aiml *.xml);;All Files (*)" );
	if ( path.isEmpty() ) {
		return;
	}

	QFile file( path );
	if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		QMessageBox::warning( MainFrame_getWindow(), "Open AIML", StringStream( "Could not open file:\n", path.toUtf8().constData() ).c_str() );
		return;
	}

	QTextStream in( &file );
	in.setCodec( "UTF-8" );
	const QString text = in.readAll();
	file.close();

	g_aimlCurrentPath = QFileInfo( path ).absoluteFilePath();
	AIMLWorkbench_setLastDirectory( g_aimlCurrentPath );
	AIMLWorkbench_setSetting( "LastFile", g_aimlCurrentPath );
	AIMLWorkbench_setText( text );
	AIMLWorkbench_setStatus( StringStream( "Opened ", QFileInfo( path ).fileName().toUtf8().constData() ).c_str() );
	AIMLWorkbench_updateDockTitle();
}

void AIMLWorkbench_insertSnippet( const QString& snippet ){
	if ( g_aimlEditor == nullptr ) {
		return;
	}
	QTextCursor cursor = g_aimlEditor->textCursor();
	cursor.insertText( snippet );
	g_aimlEditor->setTextCursor( cursor );
	g_aimlEditor->setFocus();
	AIMLWorkbench_markDirty();
	AIMLWorkbench_refreshCategoryList();
}

void AIMLWorkbench_insertCategory(){
	AIMLWorkbench_insertSnippet(
R"(
  <category>
    <pattern>NEW PATTERN</pattern>
    <template>New response.</template>
  </category>
)" );
}

void AIMLWorkbench_insertRandom(){
	AIMLWorkbench_insertSnippet(
R"(<random>
  <li>First response.</li>
  <li>Second response.</li>
</random>)" );
}

void AIMLWorkbench_insertCondition(){
	AIMLWorkbench_insertSnippet(
R"(<condition name="topic">
  <li value="mapping">Let's talk about level editing.</li>
  <li>Tell me more.</li>
</condition>)" );
}

void AIMLWorkbench_insertSrai(){
	AIMLWorkbench_insertSnippet( "<srai>HELLO</srai>" );
}

void AIMLWorkbench_insertThink(){
	AIMLWorkbench_insertSnippet(
R"(<think>
  <set name="topic">mapping</set>
</think>)" );
}

void AIMLWorkbench_openCategoryFromList( QListWidgetItem* item ){
	if ( item == nullptr ) {
		return;
	}
	AIMLWorkbench_gotoLine( item->data( Qt::UserRole ).toInt() );
}

}

void AIMLWorkbench_open(){
	if ( g_aimlDock == nullptr ) {
		return;
	}
	g_aimlDock->show();
	g_aimlDock->raise();
}

void AIMLWorkbench_createDock( QMainWindow* window ){
	if ( window == nullptr || g_aimlDock != nullptr ) {
		return;
	}

	g_aimlDock = new QDockWidget( "AIML 3.0 Editor", window );
	g_aimlDock->setObjectName( "dock_aiml_workbench" );

	auto* root = new QWidget( g_aimlDock );
	auto* layout = new QVBoxLayout( root );
	layout->setContentsMargins( 4, 4, 4, 4 );

	auto* toolbar = new QHBoxLayout();
	auto* newButton = new QPushButton( "New", root );
	auto* openButton = new QPushButton( "Open", root );
	auto* saveButton = new QPushButton( "Save", root );
	auto* saveAsButton = new QPushButton( "Save As", root );
	auto* validateButton = new QPushButton( "Validate", root );
	validateButton->setStyleSheet( "font-weight: bold;" );
	toolbar->addWidget( newButton );
	toolbar->addWidget( openButton );
	toolbar->addWidget( saveButton );
	toolbar->addWidget( saveAsButton );
	toolbar->addWidget( validateButton );
	toolbar->addStretch();
	layout->addLayout( toolbar );

	auto* snippetBar = new QHBoxLayout();
	auto* categoryButton = new QPushButton( "Category", root );
	auto* randomButton = new QPushButton( "Random", root );
	auto* conditionButton = new QPushButton( "Condition", root );
	auto* sraiButton = new QPushButton( "SRAI", root );
	auto* thinkButton = new QPushButton( "Think/Set", root );
	snippetBar->addWidget( new QLabel( "Insert", root ) );
	snippetBar->addWidget( categoryButton );
	snippetBar->addWidget( randomButton );
	snippetBar->addWidget( conditionButton );
	snippetBar->addWidget( sraiButton );
	snippetBar->addWidget( thinkButton );
	snippetBar->addStretch();
	layout->addLayout( snippetBar );

	auto* splitter = new QSplitter( Qt::Horizontal, root );

	auto* navPanel = new QWidget( splitter );
	auto* navLayout = new QVBoxLayout( navPanel );
	navLayout->setContentsMargins( 0, 0, 0, 0 );
	navLayout->addWidget( new QLabel( "Categories", navPanel ) );
	g_aimlCategoryFilter = new QLineEdit( navPanel );
	g_aimlCategoryFilter->setPlaceholderText( "Filter patterns" );
	navLayout->addWidget( g_aimlCategoryFilter );
	g_aimlCategoryList = new QListWidget( navPanel );
	g_aimlCategoryList->setAlternatingRowColors( true );
	navLayout->addWidget( g_aimlCategoryList, 1 );
	splitter->addWidget( navPanel );

	g_aimlEditor = new QPlainTextEdit( splitter );
	g_aimlEditor->setFont( QFont( "Monospace", 9 ) );
	g_aimlEditor->setTabStopDistance( 24 );
	g_aimlEditor->setLineWrapMode( QPlainTextEdit::NoWrap );
	g_aimlEditor->setPlaceholderText( AIMLWorkbench_defaultDocument() );
	splitter->addWidget( g_aimlEditor );
	splitter->setSizes( { 240, 760 } );
	layout->addWidget( splitter, 1 );

	g_aimlStatusLabel = new QLabel( "Ready", root );
	layout->addWidget( g_aimlStatusLabel );

	QObject::connect( newButton, &QPushButton::clicked, AIMLWorkbench_newDocument );
	QObject::connect( openButton, &QPushButton::clicked, AIMLWorkbench_openFile );
	QObject::connect( saveButton, &QPushButton::clicked, [](){ AIMLWorkbench_save(); } );
	QObject::connect( saveAsButton, &QPushButton::clicked, [](){ AIMLWorkbench_saveAs(); } );
	QObject::connect( validateButton, &QPushButton::clicked, AIMLWorkbench_validateAndReport );
	QObject::connect( categoryButton, &QPushButton::clicked, AIMLWorkbench_insertCategory );
	QObject::connect( randomButton, &QPushButton::clicked, AIMLWorkbench_insertRandom );
	QObject::connect( conditionButton, &QPushButton::clicked, AIMLWorkbench_insertCondition );
	QObject::connect( sraiButton, &QPushButton::clicked, AIMLWorkbench_insertSrai );
	QObject::connect( thinkButton, &QPushButton::clicked, AIMLWorkbench_insertThink );
	QObject::connect( g_aimlCategoryFilter, &QLineEdit::textChanged, [](){ AIMLWorkbench_refreshCategoryList(); } );
	QObject::connect( g_aimlCategoryList, &QListWidget::itemActivated, AIMLWorkbench_openCategoryFromList );
	QObject::connect( g_aimlCategoryList, &QListWidget::itemClicked, AIMLWorkbench_openCategoryFromList );
	QObject::connect( g_aimlEditor, &QPlainTextEdit::textChanged, [](){
		AIMLWorkbench_markDirty();
		AIMLWorkbench_refreshCategoryList();
	} );

	g_aimlDock->setWidget( root );
	window->addDockWidget( Qt::BottomDockWidgetArea, g_aimlDock );
	g_aimlDock->hide();

	const QString lastFile = AIMLWorkbench_setting( "LastFile" );
	if ( !lastFile.isEmpty() && QFileInfo::exists( lastFile ) ) {
		g_aimlCurrentPath = lastFile;
		QFile file( lastFile );
		if ( file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
			QTextStream in( &file );
			in.setCodec( "UTF-8" );
			AIMLWorkbench_setText( in.readAll() );
			file.close();
			AIMLWorkbench_setStatus( StringStream( "Loaded ", QFileInfo( lastFile ).fileName().toUtf8().constData() ).c_str() );
		}
		else{
			AIMLWorkbench_newDocument();
		}
	}
	else{
		AIMLWorkbench_newDocument();
	}
}

void AIMLWorkbench_stopAndRelease(){
	if ( !g_aimlCurrentPath.isEmpty() ) {
		AIMLWorkbench_setSetting( "LastFile", g_aimlCurrentPath );
	}
	g_aimlDock = nullptr;
	g_aimlEditor = nullptr;
	g_aimlCategoryList = nullptr;
	g_aimlCategoryFilter = nullptr;
	g_aimlStatusLabel = nullptr;
	g_aimlCurrentPath.clear();
	g_aimlDirty = false;
	g_aimlCategories.clear();
}

bool AIMLWorkbench_requestClose(){
	return AIMLWorkbench_promptSaveIfDirty( "Exit Radiant" );
}
