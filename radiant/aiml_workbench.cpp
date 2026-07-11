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
#include <QDirIterator>
#include <QMessageBox>
#include <QScrollBar>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QSignalBlocker>
#include <QTextCursor>
#include <QTextBlock>
#include <QFont>
#include <QHash>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTabWidget>

namespace
{

class AIMLHighlighter final : public QSyntaxHighlighter
{
	QTextCharFormat m_tagFormat;
	QTextCharFormat m_attributeFormat;
	QTextCharFormat m_valueFormat;
	QTextCharFormat m_commentFormat;
	QTextCharFormat m_patternFormat;
	QTextCharFormat m_templateFormat;
	QTextCharFormat m_keywordFormat;
public:
	explicit AIMLHighlighter( QTextDocument* parent ) : QSyntaxHighlighter( parent ){
		m_tagFormat.setForeground( QColor( 86, 156, 214 ) );
		m_tagFormat.setFontWeight( QFont::Bold );
		m_attributeFormat.setForeground( QColor( 156, 220, 254 ) );
		m_valueFormat.setForeground( QColor( 206, 145, 120 ) );
		m_commentFormat.setForeground( QColor( 106, 153, 85 ) );
		m_patternFormat.setForeground( QColor( 220, 220, 170 ) );
		m_templateFormat.setForeground( QColor( 181, 206, 168 ) );
		m_keywordFormat.setForeground( QColor( 197, 134, 192 ) );
		m_keywordFormat.setFontWeight( QFont::Bold );
	}
protected:
	void highlightBlock( const QString& text ) override {
		static const QRegularExpression commentRx( "<!--[^>]*-->" );
		static const QRegularExpression tagRx( "</?\\s*([A-Za-z0-9:_-]+)" );
		static const QRegularExpression attrRx( "\\b([A-Za-z0-9:_-]+)(?=\\=)" );
		static const QRegularExpression valueRx( "\"[^\"]*\"" );
		static const QRegularExpression patternRx( "<pattern>\\s*([^<]+)\\s*</pattern>", QRegularExpression::CaseInsensitiveOption );
		static const QRegularExpression templateRx( "<template>\\s*([^<]+)\\s*</template>", QRegularExpression::CaseInsensitiveOption );
		static const QStringList keywords = { "category", "pattern", "template", "that", "topic", "random", "condition", "li", "srai", "think", "set", "get", "star", "bot", "formal", "uppercase", "lowercase", "sentence" };

		for ( auto it = commentRx.globalMatch( text ); it.hasNext(); ) {
			const auto match = it.next();
			setFormat( match.capturedStart(), match.capturedLength(), m_commentFormat );
		}
		for ( auto it = tagRx.globalMatch( text ); it.hasNext(); ) {
			const auto match = it.next();
			setFormat( match.capturedStart(), match.capturedLength(), m_tagFormat );
			const QString tagName = match.captured( 1 ).toLower();
			if ( keywords.contains( tagName ) ) {
				setFormat( match.capturedStart( 1 ), match.capturedLength( 1 ), m_keywordFormat );
			}
		}
		for ( auto it = attrRx.globalMatch( text ); it.hasNext(); ) {
			const auto match = it.next();
			setFormat( match.capturedStart( 1 ), match.capturedLength( 1 ), m_attributeFormat );
		}
		for ( auto it = valueRx.globalMatch( text ); it.hasNext(); ) {
			const auto match = it.next();
			setFormat( match.capturedStart(), match.capturedLength(), m_valueFormat );
		}
		for ( auto it = patternRx.globalMatch( text ); it.hasNext(); ) {
			const auto match = it.next();
			setFormat( match.capturedStart( 1 ), match.capturedLength( 1 ), m_patternFormat );
		}
		for ( auto it = templateRx.globalMatch( text ); it.hasNext(); ) {
			const auto match = it.next();
			setFormat( match.capturedStart( 1 ), match.capturedLength( 1 ), m_templateFormat );
		}
	}
};

QDockWidget* g_aimlDock{};
QPlainTextEdit* g_aimlEditor{};
QListWidget* g_aimlCategoryList{};
QLineEdit* g_aimlCategoryFilter{};
QListWidget* g_aimlFileList{};
QLineEdit* g_aimlFileFilter{};
QLineEdit* g_aimlTestInput{};
QPlainTextEdit* g_aimlPreview{};
QListWidget* g_aimlFlowList{};
QLabel* g_aimlStatusLabel{};
AIMLHighlighter* g_aimlHighlighter{};
QString g_aimlCurrentPath;
bool g_aimlDirty{};

struct AIMLCategoryEntry
{
	QString pattern;
	QString that;
	QString topic;
	QString templatePreview;
	QString normalizedPattern;
	QStringList sraiTargets;
	QStringList setNames;
	QStringList getNames;
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

QString AIMLWorkbench_documentDirectory(){
	if ( !g_aimlCurrentPath.isEmpty() ) {
		return QFileInfo( g_aimlCurrentPath ).absolutePath();
	}
	return AIMLWorkbench_defaultDirectory();
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

void AIMLWorkbench_refreshFileList(){
	if ( g_aimlFileList == nullptr ) {
		return;
	}

	const QString rootPath = AIMLWorkbench_documentDirectory();
	const QString filter = g_aimlFileFilter != nullptr ? g_aimlFileFilter->text().trimmed() : QString();
	const QString currentPath = QFileInfo( g_aimlCurrentPath ).absoluteFilePath();

	g_aimlFileList->clear();
	if ( rootPath.isEmpty() || !QFileInfo::exists( rootPath ) ) {
		return;
	}

	QDirIterator it( rootPath, QStringList() << "*.aiml" << "*.xml", QDir::Files, QDirIterator::Subdirectories );
	while ( it.hasNext() )
	{
		const QString path = it.next();
		const QString relative = QDir( rootPath ).relativeFilePath( path );
		if ( !filter.isEmpty() && !relative.contains( filter, Qt::CaseInsensitive ) ) {
			continue;
		}
		auto* item = new QListWidgetItem( relative, g_aimlFileList );
		item->setData( Qt::UserRole, path );
		item->setToolTip( path );
		if ( QFileInfo( path ).absoluteFilePath() == currentPath ) {
			g_aimlFileList->setCurrentItem( item );
		}
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

QString AIMLWorkbench_escapeXml( const QString& text ){
	QString out = text;
	out.replace( '&', "&amp;" );
	out.replace( '<', "&lt;" );
	out.replace( '>', "&gt;" );
	out.replace( '"', "&quot;" );
	return out;
}

QString AIMLWorkbench_normalizePattern( QString pattern ){
	pattern = pattern.toUpper();
	pattern.replace( QRegularExpression( "[^A-Z0-9_* ]" ), " " );
	pattern = pattern.simplified();
	return pattern;
}

QString AIMLWorkbench_simplifyText( QString text, int maxLength = 72 ){
	text = text.simplified();
	if ( text.size() > maxLength ) {
		text = text.left( maxLength - 3 ) + "...";
	}
	return text;
}

QStringList AIMLWorkbench_extractTagValues( const QString& text, const char* tagName ){
	QStringList values;
	const QRegularExpression rx(
		StringStream( "<", tagName, ">\\s*([^<]+)\\s*</", tagName, ">" ).c_str(),
		QRegularExpression::CaseInsensitiveOption );
	for ( auto it = rx.globalMatch( text ); it.hasNext(); ) {
		const auto match = it.next();
		const QString value = match.captured( 1 ).simplified();
		if ( !value.isEmpty() ) {
			values.push_back( value );
		}
	}
	return values;
}

QStringList AIMLWorkbench_extractAttributeValues( const QString& text, const char* tagName, const char* attrName ){
	QStringList values;
	const QRegularExpression rx(
		StringStream( "<", tagName, "[^>]*\\b", attrName, "\\s*=\\s*\"([^\"]+)\"" ).c_str(),
		QRegularExpression::CaseInsensitiveOption );
	for ( auto it = rx.globalMatch( text ); it.hasNext(); ) {
		const auto match = it.next();
		const QString value = match.captured( 1 ).simplified();
		if ( !value.isEmpty() ) {
			values.push_back( value );
		}
	}
	return values;
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

void AIMLWorkbench_setPreview( const QString& text ){
	if ( g_aimlPreview != nullptr ) {
		g_aimlPreview->setPlainText( text );
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
	bool inThat = false;
	bool inTemplate = false;
	QString patternText;
	QString thatText;
	QString topicText;
	QString templateText;
	int patternLine = 1;
	QString currentTopic;
	QString categoryXml;
	int categoryDepth = 0;

	while ( !xml.atEnd() )
	{
		xml.readNext();
		if ( xml.isStartElement() ) {
			const QStringView name = xml.name();
			if ( name.compare( QStringLiteral( "topic" ), Qt::CaseInsensitive ) == 0 ) {
				currentTopic = xml.attributes().value( "name" ).toString().simplified();
			}
			if ( name.compare( QStringLiteral( "category" ), Qt::CaseInsensitive ) == 0 ) {
				inCategory = true;
				patternText.clear();
				thatText.clear();
				templateText.clear();
				topicText = currentTopic;
				patternLine = int( xml.lineNumber() );
				categoryXml.clear();
				categoryDepth = 1;
			}
			else if ( inCategory && name.compare( QStringLiteral( "pattern" ), Qt::CaseInsensitive ) == 0 ) {
				inPattern = true;
				patternLine = int( xml.lineNumber() );
			}
			else if ( inCategory && name.compare( QStringLiteral( "that" ), Qt::CaseInsensitive ) == 0 ) {
				inThat = true;
			}
			else if ( inCategory && name.compare( QStringLiteral( "template" ), Qt::CaseInsensitive ) == 0 ) {
				inTemplate = true;
			}
			if ( inCategory && !( name.compare( QStringLiteral( "category" ), Qt::CaseInsensitive ) == 0 ) ) {
				++categoryDepth;
			}
			if ( inCategory ) {
				categoryXml += '<' + name.toString();
				for ( const auto& attr : xml.attributes() )
					categoryXml += StringStream( " ", attr.name().toString().toUtf8().constData(), "=\"", AIMLWorkbench_escapeXml( attr.value().toString() ).toUtf8().constData(), "\"" ).c_str();
				categoryXml += '>';
			}
		}
		else if ( xml.isCharacters() ) {
			if ( inPattern ) {
				patternText += xml.text().toString();
			}
			if ( inThat ) {
				thatText += xml.text().toString();
			}
			if ( inTemplate ) {
				templateText += xml.text().toString();
			}
			if ( inCategory ) {
				categoryXml += AIMLWorkbench_escapeXml( xml.text().toString() );
			}
		}
		else if ( xml.isEndElement() ) {
			const QStringView name = xml.name();
			if ( inCategory ) {
				categoryXml += StringStream( "</", name.toString().toUtf8().constData(), ">" ).c_str();
			}
			if ( name.compare( QStringLiteral( "pattern" ), Qt::CaseInsensitive ) == 0 ) {
				inPattern = false;
			}
			else if ( name.compare( QStringLiteral( "that" ), Qt::CaseInsensitive ) == 0 ) {
				inThat = false;
			}
			else if ( name.compare( QStringLiteral( "template" ), Qt::CaseInsensitive ) == 0 ) {
				inTemplate = false;
			}
			else if ( name.compare( QStringLiteral( "category" ), Qt::CaseInsensitive ) == 0 ) {
				inCategory = false;
				const QString trimmed = patternText.simplified();
				if ( !trimmed.isEmpty() ) {
					AIMLCategoryEntry entry;
					entry.pattern = trimmed;
					entry.that = thatText.simplified();
					entry.topic = topicText.simplified();
					entry.templatePreview = AIMLWorkbench_simplifyText( templateText );
					entry.normalizedPattern = AIMLWorkbench_normalizePattern( entry.pattern );
					entry.sraiTargets = AIMLWorkbench_extractTagValues( categoryXml, "srai" );
					entry.setNames = AIMLWorkbench_extractAttributeValues( categoryXml, "set", "name" );
					entry.getNames = AIMLWorkbench_extractAttributeValues( categoryXml, "get", "name" );
					entry.line = patternLine;
					out.push_back( entry );
				}
				categoryDepth = 0;
			}
			else if ( name.compare( QStringLiteral( "topic" ), Qt::CaseInsensitive ) == 0 ) {
				currentTopic.clear();
			}
			if ( inCategory && categoryDepth > 0 ) {
				--categoryDepth;
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
			AIMLCategoryEntry entry;
			entry.pattern = pattern;
			entry.normalizedPattern = AIMLWorkbench_normalizePattern( entry.pattern );
			entry.line = line;
			out.push_back( entry );
		}
	}

	return out;
}

void AIMLWorkbench_refreshFlowList(){
	if ( g_aimlFlowList == nullptr ) {
		return;
	}

	g_aimlFlowList->clear();
	QHash<QString, int> byPattern;
	for ( const AIMLCategoryEntry& entry : g_aimlCategories )
	{
		if ( !entry.normalizedPattern.isEmpty() ) {
			byPattern.insert( entry.normalizedPattern, entry.line );
		}
	}

	for ( const AIMLCategoryEntry& entry : g_aimlCategories )
	{
		QStringList parts;
		parts.push_back( entry.pattern );
		if ( !entry.topic.isEmpty() ) {
			parts.push_back( StringStream( "topic:", entry.topic.toUtf8().constData() ).c_str() );
		}
		if ( !entry.that.isEmpty() ) {
			parts.push_back( StringStream( "that:", entry.that.toUtf8().constData() ).c_str() );
		}
		if ( !entry.sraiTargets.isEmpty() ) {
			parts.push_back( StringStream( "srai:", entry.sraiTargets.join( ", " ).toUtf8().constData() ).c_str() );
		}
		if ( !entry.setNames.isEmpty() ) {
			parts.push_back( StringStream( "set:", entry.setNames.join( ", " ).toUtf8().constData() ).c_str() );
		}
		if ( !entry.getNames.isEmpty() ) {
			parts.push_back( StringStream( "get:", entry.getNames.join( ", " ).toUtf8().constData() ).c_str() );
		}

		auto* item = new QListWidgetItem( parts.join( "\n" ), g_aimlFlowList );
		item->setData( Qt::UserRole, entry.line );

		QStringList toolTip;
		toolTip.push_back( StringStream( "Line ", entry.line ).c_str() );
		for ( const QString& target : entry.sraiTargets )
		{
			const QString normalized = AIMLWorkbench_normalizePattern( target );
			if ( byPattern.contains( normalized ) ) {
				toolTip.push_back( StringStream( "srai -> ", target.toUtf8().constData(), " (line ", byPattern.value( normalized ), ")" ).c_str() );
			}
			else{
				toolTip.push_back( StringStream( "srai -> ", target.toUtf8().constData(), " (unresolved)" ).c_str() );
			}
		}
		item->setToolTip( toolTip.join( "\n" ) );
	}
}

QHash<QString, int> AIMLWorkbench_collectDuplicatePatterns( const QVector<AIMLCategoryEntry>& categories ){
	QHash<QString, int> counts;
	for ( const AIMLCategoryEntry& entry : categories )
	{
		const QString key = AIMLWorkbench_normalizePattern( entry.pattern );
		if ( !key.isEmpty() ) {
			counts[key] += 1;
		}
	}
	return counts;
}

void AIMLWorkbench_refreshCategoryList(){
	if ( g_aimlCategoryList == nullptr || g_aimlEditor == nullptr ) {
		return;
	}

	g_aimlCategories = AIMLWorkbench_collectCategories( g_aimlEditor->toPlainText() );
	const QHash<QString, int> duplicates = AIMLWorkbench_collectDuplicatePatterns( g_aimlCategories );
	const QString filter = g_aimlCategoryFilter != nullptr ? g_aimlCategoryFilter->text().trimmed() : QString();

	g_aimlCategoryList->clear();
	for ( const AIMLCategoryEntry& entry : g_aimlCategories )
	{
		const QString filterText = StringStream(
			entry.pattern.toUtf8().constData(), " ",
			entry.topic.toUtf8().constData(), " ",
			entry.that.toUtf8().constData(), " ",
			entry.templatePreview.toUtf8().constData() ).c_str();
		if ( !filter.isEmpty() && !filterText.contains( filter, Qt::CaseInsensitive ) ) {
			continue;
		}
		const QString normalized = AIMLWorkbench_normalizePattern( entry.pattern );
		const bool duplicate = duplicates.value( normalized ) > 1;
		QStringList lines;
		lines.push_back( duplicate ? StringStream( entry.pattern.toUtf8().constData(), "  [duplicate]" ).c_str() : entry.pattern );
		QStringList meta;
		if ( !entry.topic.isEmpty() ) {
			meta.push_back( StringStream( "topic: ", entry.topic.toUtf8().constData() ).c_str() );
		}
		if ( !entry.that.isEmpty() ) {
			meta.push_back( StringStream( "that: ", entry.that.toUtf8().constData() ).c_str() );
		}
		if ( !meta.isEmpty() ) {
			lines.push_back( meta.join( "  •  " ) );
		}
		if ( !entry.templatePreview.isEmpty() ) {
			lines.push_back( entry.templatePreview );
		}
		auto* item = new QListWidgetItem( lines.join( '\n' ), g_aimlCategoryList );
		item->setData( Qt::UserRole, entry.line );
		item->setToolTip( StringStream(
			"Line ", entry.line,
			duplicate ? "\nDuplicate pattern detected" : "",
			entry.templatePreview.isEmpty() ? "" : "\n",
			entry.templatePreview.toUtf8().constData() ).c_str() );
	}

	if ( g_aimlCategoryList->count() > 0 ) {
		g_aimlCategoryList->setCurrentRow( 0 );
	}

	int duplicateCount = 0;
	int wildcardCount = 0;
	for ( auto it = duplicates.constBegin(); it != duplicates.constEnd(); ++it )
		if ( it.value() > 1 )
			++duplicateCount;
	for ( const AIMLCategoryEntry& entry : g_aimlCategories )
		if ( entry.pattern.contains( '*' ) || entry.pattern.contains( '_' ) )
			++wildcardCount;
	AIMLWorkbench_setStatus( StringStream( "Ready - ", g_aimlCategories.size(), " categor", g_aimlCategories.size() == 1 ? "y" : "ies", ", ", duplicateCount, " duplicate pattern", duplicateCount == 1 ? "" : "s", ", ", wildcardCount, " wildcard rule", wildcardCount == 1 ? "" : "s" ).c_str() );
	AIMLWorkbench_refreshFlowList();
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

	const QVector<AIMLCategoryEntry> categories = AIMLWorkbench_collectCategories( text );
	const QHash<QString, int> duplicates = AIMLWorkbench_collectDuplicatePatterns( categories );
	if ( categories.isEmpty() ) {
		if ( errorOut != nullptr ) {
			*errorOut = "Document has no AIML categories.";
		}
		if ( errorLineOut != nullptr ) {
			*errorLineOut = 1;
		}
		return false;
	}
	for ( const AIMLCategoryEntry& entry : categories )
	{
		const QString normalized = AIMLWorkbench_normalizePattern( entry.pattern );
		if ( duplicates.value( normalized ) > 1 ) {
			if ( errorOut != nullptr ) {
				*errorOut = StringStream( "Duplicate <pattern>: ", entry.pattern.toUtf8().constData() ).c_str();
			}
			if ( errorLineOut != nullptr ) {
				*errorLineOut = entry.line;
			}
			return false;
		}
		if ( entry.templatePreview.isEmpty() ) {
			if ( errorOut != nullptr ) {
				*errorOut = StringStream( "Category missing <template>: ", entry.pattern.toUtf8().constData() ).c_str();
			}
			if ( errorLineOut != nullptr ) {
				*errorLineOut = entry.line;
			}
			return false;
		}
	}

	return true;
}

QString AIMLWorkbench_prettyPrintXml( const QString& text, QString* errorOut = nullptr, int* errorLineOut = nullptr ){
	QXmlStreamReader xml( text );
	QString output;
	QTextStream out( &output );
	int indent = 0;

	auto writeIndent = [&out, &indent](){
		for ( int i = 0; i < indent; ++i )
			out << "  ";
	};

	while ( !xml.atEnd() )
	{
		xml.readNext();
		if ( xml.isStartDocument() ) {
			out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
		}
		else if ( xml.isStartElement() ) {
			writeIndent();
			out << '<' << xml.name().toString();
			for ( const auto& attr : xml.attributes() )
				out << ' ' << attr.name().toString() << "=\"" << AIMLWorkbench_escapeXml( attr.value().toString() ) << '"';
			out << '>';
			out << '\n';
			++indent;
		}
		else if ( xml.isEndElement() ) {
			indent = std::max( 0, indent - 1 );
			writeIndent();
			out << "</" << xml.name().toString() << ">\n";
		}
		else if ( xml.isCharacters() ) {
			const QString trimmed = xml.text().toString().trimmed();
			if ( !trimmed.isEmpty() ) {
				writeIndent();
				out << AIMLWorkbench_escapeXml( trimmed ) << '\n';
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
		return {};
	}

	return output.trimmed() + '\n';
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

void AIMLWorkbench_refreshDiagnostics(){
	QString error;
	int line = 1;
	int sraiCount = 0;
	int stateTouchCount = 0;
	QHash<QString, int> byPattern;
	for ( const AIMLCategoryEntry& entry : g_aimlCategories )
	{
		if ( !entry.normalizedPattern.isEmpty() ) {
			byPattern.insert( entry.normalizedPattern, entry.line );
		}
		sraiCount += entry.sraiTargets.size();
		stateTouchCount += entry.setNames.size() + entry.getNames.size();
	}
	int unresolvedSrai = 0;
	for ( const AIMLCategoryEntry& entry : g_aimlCategories )
		for ( const QString& target : entry.sraiTargets )
			if ( !byPattern.contains( AIMLWorkbench_normalizePattern( target ) ) )
				++unresolvedSrai;

	if ( AIMLWorkbench_validateDocument( &error, &line ) ) {
		AIMLWorkbench_setPreview( StringStream(
			"AIML 3.0 document looks valid.\n\nCategories: ", g_aimlCategories.size(),
			"\nSRAI links: ", sraiCount,
			"\nState reads/writes: ", stateTouchCount,
			unresolvedSrai > 0 ? "\nUnresolved SRAI links: " : "",
			unresolvedSrai > 0 ? StringStream( unresolvedSrai ).c_str() : "",
			"\nTry the test box to preview pattern resolution." ).c_str() );
	}
	else{
		AIMLWorkbench_setPreview( StringStream(
			"Validation issue at line ", line, ":\n", error.toUtf8().constData(),
			"\n\nSRAI links: ", sraiCount,
			"\nState reads/writes: ", stateTouchCount,
			"\nUnresolved SRAI links: ", unresolvedSrai,
			"\n\nDouble-click a category or run Validate for a focused warning." ).c_str() );
	}
}

void AIMLWorkbench_setText( const QString& text ){
	if ( g_aimlEditor == nullptr ) {
		return;
	}
	const QSignalBlocker blocker( g_aimlEditor );
	g_aimlEditor->setPlainText( text );
	AIMLWorkbench_refreshCategoryList();
	AIMLWorkbench_refreshDiagnostics();
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
	AIMLWorkbench_refreshFileList();
}

void AIMLWorkbench_formatDocument(){
	if ( g_aimlEditor == nullptr ) {
		return;
	}
	QString error;
	int line = 1;
	const QString pretty = AIMLWorkbench_prettyPrintXml( g_aimlEditor->toPlainText(), &error, &line );
	if ( pretty.isEmpty() ) {
		AIMLWorkbench_setStatus( StringStream( "Format failed at line ", line ).c_str() );
		AIMLWorkbench_gotoLine( line );
		QMessageBox::warning( MainFrame_getWindow(), "Format AIML", StringStream( "Line ", line, ": ", error.toUtf8().constData() ).c_str() );
		return;
	}
	g_aimlEditor->setPlainText( pretty );
	AIMLWorkbench_markDirty();
	AIMLWorkbench_refreshCategoryList();
	AIMLWorkbench_setStatus( "Formatted AIML document" );
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

void AIMLWorkbench_insertTopic(){
	AIMLWorkbench_insertSnippet(
R"(<topic name="mapping">
  <category>
    <pattern>LEVEL EDITING</pattern>
    <template>Let's talk about level editing.</template>
  </category>
</topic>)" );
}

void AIMLWorkbench_insertThat(){
	AIMLWorkbench_insertSnippet(
R"(
  <category>
    <pattern>YES</pattern>
    <that>DO YOU WANT TO CONTINUE</that>
    <template>Great. Let's continue.</template>
  </category>
)" );
}

void AIMLWorkbench_insertLearn(){
	AIMLWorkbench_insertSnippet(
R"(<learn>
  <category>
    <pattern>NEWLY LEARNED PATTERN</pattern>
    <template>Newly learned response.</template>
  </category>
</learn>)" );
}

void AIMLWorkbench_insertLoop(){
	AIMLWorkbench_insertSnippet(
R"(<loop>
  <condition name="conversation_step">
    <li value="1">Continue the scripted exchange.</li>
    <li>Stop looping.</li>
  </condition>
</loop>)" );
}

void AIMLWorkbench_insertMap(){
	AIMLWorkbench_insertSnippet(
R"(<map name="synonyms">
  <li><key>HELLO</key><value>HI</value></li>
  <li><key>BYE</key><value>GOODBYE</value></li>
</map>)" );
}

void AIMLWorkbench_insertBot(){
	AIMLWorkbench_insertSnippet(
R"(<template>
  My name is <bot name="name"/> and my role is <bot name="role"/>.
</template>)" );
}

void AIMLWorkbench_insertGetSet(){
	AIMLWorkbench_insertSnippet(
R"(<template>
  <think><set name="topic">mapping</set></think>
  Current topic is <get name="topic"/>.
</template>)" );
}

void AIMLWorkbench_createCategoryFromTestInput(){
	if ( g_aimlTestInput == nullptr ) {
		return;
	}
	const QString pattern = AIMLWorkbench_normalizePattern( g_aimlTestInput->text() );
	if ( pattern.isEmpty() ) {
		QMessageBox::information( MainFrame_getWindow(), "Create Category", "Type a test phrase first." );
		return;
	}
	AIMLWorkbench_insertSnippet(
		StringStream(
R"(
  <category>
    <pattern>)",
			pattern.toUtf8().constData(),
R"(</pattern>
    <template>)",
			AIMLWorkbench_escapeXml( g_aimlTestInput->text().trimmed().isEmpty() ? "New response." : StringStream( "Response for ", g_aimlTestInput->text().trimmed().toUtf8().constData() ).c_str() ).toUtf8().constData(),
R"(</template>
  </category>
)").c_str() );
}

void AIMLWorkbench_runTestInput(){
	if ( g_aimlTestInput == nullptr ) {
		return;
	}
	const QString normalized = AIMLWorkbench_normalizePattern( g_aimlTestInput->text() );
	if ( normalized.isEmpty() ) {
		AIMLWorkbench_setPreview( "Type a phrase to test pattern matching." );
		return;
	}

	for ( const AIMLCategoryEntry& entry : g_aimlCategories )
	{
		const QString candidate = AIMLWorkbench_normalizePattern( entry.pattern );
		if ( candidate == normalized ) {
			AIMLWorkbench_setPreview( StringStream(
				"Exact pattern match:\n", entry.pattern.toUtf8().constData(),
				entry.topic.isEmpty() ? "" : "\nTopic: ",
				entry.topic.toUtf8().constData(),
				entry.that.isEmpty() ? "" : "\nThat: ",
				entry.that.toUtf8().constData(),
				entry.templatePreview.isEmpty() ? "" : "\nTemplate: ",
				entry.templatePreview.toUtf8().constData(),
				"\nLine ", entry.line ).c_str() );
			AIMLWorkbench_gotoLine( entry.line );
			return;
		}
	}

	for ( const AIMLCategoryEntry& entry : g_aimlCategories )
	{
		const QString candidate = AIMLWorkbench_normalizePattern( entry.pattern );
		QString regexSource = QRegularExpression::escape( candidate );
		regexSource.replace( "\\*", ".*" );
		regexSource.replace( "_", "\\S+" );
		const QRegularExpression regex( StringStream( "^", regexSource.toUtf8().constData(), "$" ).c_str() );
		if ( regex.match( normalized ).hasMatch() ) {
			AIMLWorkbench_setPreview( StringStream(
				"Wildcard match:\n", entry.pattern.toUtf8().constData(),
				entry.topic.isEmpty() ? "" : "\nTopic: ",
				entry.topic.toUtf8().constData(),
				entry.that.isEmpty() ? "" : "\nThat: ",
				entry.that.toUtf8().constData(),
				entry.templatePreview.isEmpty() ? "" : "\nTemplate: ",
				entry.templatePreview.toUtf8().constData(),
				"\nLine ", entry.line ).c_str() );
			AIMLWorkbench_gotoLine( entry.line );
			return;
		}
	}

	AIMLWorkbench_setPreview( StringStream( "No pattern matched:\n", normalized.toUtf8().constData() ).c_str() );
}

void AIMLWorkbench_openCategoryFromList( QListWidgetItem* item ){
	if ( item == nullptr ) {
		return;
	}
	AIMLWorkbench_gotoLine( item->data( Qt::UserRole ).toInt() );
}

void AIMLWorkbench_openFileFromList( QListWidgetItem* item ){
	if ( item == nullptr ) {
		return;
	}
	if ( !AIMLWorkbench_promptSaveIfDirty( "Open AIML Document" ) ) {
		return;
	}

	const QString path = item->data( Qt::UserRole ).toString();
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
	AIMLWorkbench_refreshFileList();
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
	auto* formatButton = new QPushButton( "Format", root );
	validateButton->setStyleSheet( "font-weight: bold;" );
	toolbar->addWidget( newButton );
	toolbar->addWidget( openButton );
	toolbar->addWidget( saveButton );
	toolbar->addWidget( saveAsButton );
	toolbar->addWidget( validateButton );
	toolbar->addWidget( formatButton );
	toolbar->addStretch();
	layout->addLayout( toolbar );

	auto* snippetBar = new QHBoxLayout();
	auto* categoryButton = new QPushButton( "Category", root );
	auto* randomButton = new QPushButton( "Random", root );
	auto* conditionButton = new QPushButton( "Condition", root );
	auto* sraiButton = new QPushButton( "SRAI", root );
	auto* thinkButton = new QPushButton( "Think/Set", root );
	auto* topicButton = new QPushButton( "Topic", root );
	auto* thatButton = new QPushButton( "That", root );
	auto* learnButton = new QPushButton( "Learn", root );
	auto* loopButton = new QPushButton( "Loop", root );
	auto* mapButton = new QPushButton( "Map", root );
	auto* botButton = new QPushButton( "Bot", root );
	auto* getSetButton = new QPushButton( "Get/Set", root );
	auto* fromTestButton = new QPushButton( "From Test", root );
	snippetBar->addWidget( new QLabel( "Insert", root ) );
	snippetBar->addWidget( categoryButton );
	snippetBar->addWidget( randomButton );
	snippetBar->addWidget( conditionButton );
	snippetBar->addWidget( sraiButton );
	snippetBar->addWidget( thinkButton );
	snippetBar->addWidget( topicButton );
	snippetBar->addWidget( thatButton );
	snippetBar->addWidget( learnButton );
	snippetBar->addWidget( loopButton );
	snippetBar->addWidget( mapButton );
	snippetBar->addWidget( botButton );
	snippetBar->addWidget( getSetButton );
	snippetBar->addWidget( fromTestButton );
	snippetBar->addStretch();
	layout->addLayout( snippetBar );

	auto* testBar = new QHBoxLayout();
	testBar->addWidget( new QLabel( "Test", root ) );
	g_aimlTestInput = new QLineEdit( root );
	g_aimlTestInput->setPlaceholderText( "Type a user input phrase to test patterns" );
	auto* testButton = new QPushButton( "Run", root );
	testBar->addWidget( g_aimlTestInput, 1 );
	testBar->addWidget( testButton );
	layout->addLayout( testBar );

	auto* splitter = new QSplitter( Qt::Horizontal, root );

	auto* navPanel = new QWidget( splitter );
	auto* navLayout = new QVBoxLayout( navPanel );
	navLayout->setContentsMargins( 0, 0, 0, 0 );
	navLayout->addWidget( new QLabel( "Files", navPanel ) );
	g_aimlFileFilter = new QLineEdit( navPanel );
	g_aimlFileFilter->setPlaceholderText( "Filter files" );
	navLayout->addWidget( g_aimlFileFilter );
	g_aimlFileList = new QListWidget( navPanel );
	g_aimlFileList->setAlternatingRowColors( true );
	navLayout->addWidget( g_aimlFileList, 1 );
	navLayout->addWidget( new QLabel( "Categories", navPanel ) );
	g_aimlCategoryFilter = new QLineEdit( navPanel );
	g_aimlCategoryFilter->setPlaceholderText( "Filter patterns" );
	navLayout->addWidget( g_aimlCategoryFilter );
	g_aimlCategoryList = new QListWidget( navPanel );
	g_aimlCategoryList->setAlternatingRowColors( true );
	navLayout->addWidget( g_aimlCategoryList, 2 );
	splitter->addWidget( navPanel );

	g_aimlEditor = new QPlainTextEdit( splitter );
	g_aimlEditor->setFont( QFont( "Monospace", 9 ) );
	g_aimlEditor->setTabStopDistance( 24 );
	g_aimlEditor->setLineWrapMode( QPlainTextEdit::NoWrap );
	g_aimlEditor->setPlaceholderText( AIMLWorkbench_defaultDocument() );
	g_aimlHighlighter = new AIMLHighlighter( g_aimlEditor->document() );
	splitter->addWidget( g_aimlEditor );
	auto* previewPanel = new QWidget( splitter );
	auto* previewLayout = new QVBoxLayout( previewPanel );
	previewLayout->setContentsMargins( 0, 0, 0, 0 );
	auto* insightTabs = new QTabWidget( previewPanel );
	g_aimlPreview = new QPlainTextEdit( insightTabs );
	g_aimlPreview->setReadOnly( true );
	g_aimlPreview->setPlaceholderText( "Pattern test results and validation notes appear here." );
	insightTabs->addTab( g_aimlPreview, "Preview" );
	g_aimlFlowList = new QListWidget( insightTabs );
	g_aimlFlowList->setAlternatingRowColors( true );
	insightTabs->addTab( g_aimlFlowList, "Flow Map" );
	previewLayout->addWidget( insightTabs, 1 );
	splitter->addWidget( previewPanel );
	splitter->setSizes( { 220, 700, 260 } );
	layout->addWidget( splitter, 1 );

	g_aimlStatusLabel = new QLabel( "Ready", root );
	layout->addWidget( g_aimlStatusLabel );

	QObject::connect( newButton, &QPushButton::clicked, AIMLWorkbench_newDocument );
	QObject::connect( openButton, &QPushButton::clicked, AIMLWorkbench_openFile );
	QObject::connect( saveButton, &QPushButton::clicked, [](){ AIMLWorkbench_save(); } );
	QObject::connect( saveAsButton, &QPushButton::clicked, [](){ AIMLWorkbench_saveAs(); } );
	QObject::connect( validateButton, &QPushButton::clicked, AIMLWorkbench_validateAndReport );
	QObject::connect( formatButton, &QPushButton::clicked, AIMLWorkbench_formatDocument );
	QObject::connect( categoryButton, &QPushButton::clicked, AIMLWorkbench_insertCategory );
	QObject::connect( randomButton, &QPushButton::clicked, AIMLWorkbench_insertRandom );
	QObject::connect( conditionButton, &QPushButton::clicked, AIMLWorkbench_insertCondition );
	QObject::connect( sraiButton, &QPushButton::clicked, AIMLWorkbench_insertSrai );
	QObject::connect( thinkButton, &QPushButton::clicked, AIMLWorkbench_insertThink );
	QObject::connect( topicButton, &QPushButton::clicked, AIMLWorkbench_insertTopic );
	QObject::connect( thatButton, &QPushButton::clicked, AIMLWorkbench_insertThat );
	QObject::connect( learnButton, &QPushButton::clicked, AIMLWorkbench_insertLearn );
	QObject::connect( loopButton, &QPushButton::clicked, AIMLWorkbench_insertLoop );
	QObject::connect( mapButton, &QPushButton::clicked, AIMLWorkbench_insertMap );
	QObject::connect( botButton, &QPushButton::clicked, AIMLWorkbench_insertBot );
	QObject::connect( getSetButton, &QPushButton::clicked, AIMLWorkbench_insertGetSet );
	QObject::connect( fromTestButton, &QPushButton::clicked, AIMLWorkbench_createCategoryFromTestInput );
	QObject::connect( testButton, &QPushButton::clicked, AIMLWorkbench_runTestInput );
	QObject::connect( g_aimlTestInput, &QLineEdit::returnPressed, AIMLWorkbench_runTestInput );
	QObject::connect( g_aimlFileFilter, &QLineEdit::textChanged, [](){ AIMLWorkbench_refreshFileList(); } );
	QObject::connect( g_aimlFileList, &QListWidget::itemActivated, AIMLWorkbench_openFileFromList );
	QObject::connect( g_aimlFileList, &QListWidget::itemClicked, AIMLWorkbench_openFileFromList );
	QObject::connect( g_aimlCategoryFilter, &QLineEdit::textChanged, [](){ AIMLWorkbench_refreshCategoryList(); } );
	QObject::connect( g_aimlCategoryList, &QListWidget::itemActivated, AIMLWorkbench_openCategoryFromList );
	QObject::connect( g_aimlCategoryList, &QListWidget::itemClicked, AIMLWorkbench_openCategoryFromList );
	QObject::connect( g_aimlFlowList, &QListWidget::itemActivated, AIMLWorkbench_openCategoryFromList );
	QObject::connect( g_aimlFlowList, &QListWidget::itemClicked, AIMLWorkbench_openCategoryFromList );
	QObject::connect( g_aimlEditor, &QPlainTextEdit::textChanged, [](){
		AIMLWorkbench_markDirty();
		AIMLWorkbench_refreshCategoryList();
		AIMLWorkbench_refreshDiagnostics();
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
			AIMLWorkbench_refreshFileList();
		}
		else{
			AIMLWorkbench_newDocument();
		}
	}
	else{
		AIMLWorkbench_newDocument();
	}
	AIMLWorkbench_refreshFileList();
}

void AIMLWorkbench_stopAndRelease(){
	if ( !g_aimlCurrentPath.isEmpty() ) {
		AIMLWorkbench_setSetting( "LastFile", g_aimlCurrentPath );
	}
	g_aimlDock = nullptr;
	g_aimlEditor = nullptr;
	g_aimlCategoryList = nullptr;
	g_aimlCategoryFilter = nullptr;
	g_aimlFileList = nullptr;
	g_aimlFileFilter = nullptr;
	g_aimlTestInput = nullptr;
	g_aimlPreview = nullptr;
	g_aimlFlowList = nullptr;
	g_aimlStatusLabel = nullptr;
	g_aimlHighlighter = nullptr;
	g_aimlCurrentPath.clear();
	g_aimlDirty = false;
	g_aimlCategories.clear();
}

bool AIMLWorkbench_requestClose(){
	return AIMLWorkbench_promptSaveIfDirty( "Exit Radiant" );
}
