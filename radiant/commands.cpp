/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

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

#include "commands.h"

#include "debugging/debugging.h"

#include <map>
#include <vector>
#include "string/string.h"
#include "versionlib.h"
#include "gtkutil/accelerator.h"
#include "gtkutil/messagebox.h"
#include "qtmisc.h"

struct ShortcutValue{
	QKeySequence accelerator;
	const QKeySequence accelerator_default;
	int type; // 0 = !isRegistered, 1 = command, 2 = toggle
	ShortcutValue( const QKeySequence& a ) : accelerator( a ), accelerator_default( a ), type( 0 ){
	}
};
typedef std::map<CopiedString, ShortcutValue> Shortcuts;

Shortcuts g_shortcuts;

const QKeySequence& GlobalShortcuts_insert( const char* name, const QKeySequence& accelerator ){
	return ( *g_shortcuts.insert( Shortcuts::value_type( name, ShortcutValue( accelerator ) ) ).first ).second.accelerator;
}

template<typename Functor>
void GlobalShortcuts_foreach( Functor& functor ){
	for ( auto& [name, shortcut] : g_shortcuts )
		functor( name.c_str(), shortcut.accelerator );
}

void GlobalShortcuts_register( const char* name, int type ){
	Shortcuts::iterator i = g_shortcuts.find( name );
	if ( i != g_shortcuts.end() ) {
		( *i ).second.type = type;
	}
}

void GlobalShortcuts_reportUnregistered(){
	for ( const auto& [name, shortcut] : g_shortcuts )
		if ( !shortcut.accelerator.isEmpty() && shortcut.type == 0 )
			globalWarningStream() << "shortcut not registered: " << name << '\n';
}

void GlobalShortcuts_reportDuplicates(){
	std::map<QKeySequence, std::vector<CopiedString>> byAccel;
	for ( const auto& [name, shortcut] : g_shortcuts )
		if ( !shortcut.accelerator.isEmpty() )
			byAccel[shortcut.accelerator].push_back( name );
	for ( const auto& [accel, names] : byAccel )
		if ( names.size() > 1 ){
			globalWarningStream() << "duplicate shortcut " << accel.toString().toLatin1().constData() << ":";
			for ( const auto& n : names )
				globalWarningStream() << " " << n;
			globalWarningStream() << '\n';
		}
}

typedef std::map<CopiedString, Command> Commands;

Commands g_commands;

void GlobalCommands_insert( const char* name, const Callback<void()>& callback, const QKeySequence& accelerator ){
	bool added = g_commands.insert( Commands::value_type( name, Command( callback, GlobalShortcuts_insert( name, accelerator ) ) ) ).second;
	ASSERT_MESSAGE( added, "command already registered: " << Quoted( name ) );
}

const Command& GlobalCommands_find( const char* command ){
	Commands::iterator i = g_commands.find( command );
	ASSERT_MESSAGE( i != g_commands.end(), "failed to lookup command " << Quoted( command ) );
	return ( *i ).second;
}

typedef std::map<CopiedString, Toggle> Toggles;


Toggles g_toggles;

void GlobalToggles_insert( const char* name, const Callback<void()>& callback, const BoolExportCallback& exportCallback, const QKeySequence& accelerator ){
	bool added = g_toggles.insert( Toggles::value_type( name, Toggle( callback, GlobalShortcuts_insert( name, accelerator ), exportCallback ) ) ).second;
	ASSERT_MESSAGE( added, "toggle already registered: " << Quoted( name ) );
}
const Toggle& GlobalToggles_find( const char* name ){
	Toggles::iterator i = g_toggles.find( name );
	ASSERT_MESSAGE( i != g_toggles.end(), "failed to lookup toggle " << Quoted( name ) );
	return ( *i ).second;
}

typedef std::map<CopiedString, KeyEvent> KeyEvents;


KeyEvents g_keyEvents;

void GlobalKeyEvents_insert( const char* name, const Callback<void()>& keyDown, const Callback<void()>& keyUp, const QKeySequence& accelerator ){
	bool added = g_keyEvents.insert( KeyEvents::value_type( name, KeyEvent( GlobalShortcuts_insert( name, accelerator ), keyDown, keyUp ) ) ).second;
	ASSERT_MESSAGE( added, "command already registered: " << Quoted( name ) );
}
const KeyEvent& GlobalKeyEvents_find( const char* name ){
	KeyEvents::iterator i = g_keyEvents.find( name );
	ASSERT_MESSAGE( i != g_keyEvents.end(), "failed to lookup keyEvent " << Quoted( name ) );
	return ( *i ).second;
}




#include "mainframe.h"

#include "stream/textfilestream.h"
#include "stream/stringstream.h"
#include <QDialog>
#include <QTreeWidget>
#include <QGridLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QKeySequenceEdit>
#include <QKeyEvent>
#include <QApplication>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

#include <algorithm>


void disconnect_accelerator( const char *name ){
	Shortcuts::iterator i = g_shortcuts.find( name );
	if ( i != g_shortcuts.end() ) {
		switch ( ( *i ).second.type )
		{
		case 1:
			// command
			command_disconnect_accelerator( name );
			break;
		case 2:
			// toggle
			toggle_remove_accelerator( name );
			break;
		}
	}
}

void connect_accelerator( const char *name ){
	Shortcuts::iterator i = g_shortcuts.find( name );
	if ( i != g_shortcuts.end() ) {
		switch ( ( *i ).second.type )
		{
		case 1:
			// command
			command_connect_accelerator( name );
			break;
		case 2:
			// toggle
			toggle_add_accelerator( name );
			break;
		}
	}
}


inline void accelerator_item_set_icon( QTreeWidgetItem *item, const ShortcutValue& value ){
	value.accelerator != value.accelerator_default
	? item->setIcon( 1, QApplication::style()->standardIcon( QStyle::StandardPixmap::SP_DialogNoButton ) )
	: item->setIcon( 1, {} );
}


void accelerator_clear_button_clicked( QTreeWidgetItem *item ){
	const auto commandName = item->text( 0 ).toLatin1();

	// clear the ACTUAL accelerator too!
	disconnect_accelerator( commandName );

	Shortcuts::iterator thisShortcutIterator = g_shortcuts.find( commandName.constData() );
	if ( thisShortcutIterator != g_shortcuts.end() ) {
		thisShortcutIterator->second.accelerator = {};
		item->setText( 1, {} );
		accelerator_item_set_icon( item, thisShortcutIterator->second );
	}
}


// note: ideally this should also consider some shortcuts being KeyEvent and thus enabled by occasion
// so technically they do not definitely clash with Command/Toggle with the same shortcut
class VerifyAcceleratorNotTaken
{
	const char *commandName;
	const QKeySequence newAccel;
	QTreeWidget *tree;
public:
	bool allow;
	VerifyAcceleratorNotTaken( const char *name, const QKeySequence accelerator, QTreeWidget *tree ) :
		commandName( name ), newAccel( accelerator ), tree( tree ), allow( true ){
	}
	void operator()( const char* name, QKeySequence& accelerator ){
		if ( !allow
		  || !QKeySequence_valid( accelerator )
		  || !strcmp( name, commandName ) ) {
			return;
		}
		if ( accelerator == newAccel ) {
			const auto msg = StringStream( "The command <b>", name, "</b> is already assigned to the key <b>", accelerator, "</b>.<br><br>",
			                               "Do you want to unassign <b>", name, "</b> first?" );
			const EMessageBoxReturn r = qt_MessageBox( tree->window(), msg, "Key already used", EMessageBoxType::Question, eIDYES | eIDNO | eIDCANCEL );
			if ( r == eIDYES ) {
				// clear the ACTUAL accelerator too!
				disconnect_accelerator( name );
				// delete the modifier
				accelerator = {};
				// empty the cell of the key binds dialog
				for( QTreeWidgetItemIterator it( tree ); *it; ++it )
				{
					if( ( *it )->text( 0 ) == name ){
						( *it )->setText( 1, {} );
						Shortcuts::const_iterator thisShortcutIterator = g_shortcuts.find( name );
						if ( thisShortcutIterator != g_shortcuts.end() ) {
							accelerator_item_set_icon( ( *it ), thisShortcutIterator->second );
						}
						break;
					}
				}
			}
			else if ( r == eIDCANCEL ) {
				// aborted
				allow = false;
			}
			// eIDNO : keep duplicate key
		}
	}
};
// multipurpose function: invalid accelerator = reset to default
static void accelerator_alter( QTreeWidgetItem *item, const QKeySequence accelerator ){
	// 7. find the name of the accelerator
	auto commandName = item->text( 0 ).toLatin1();

	Shortcuts::iterator thisShortcutIterator = g_shortcuts.find( commandName.constData() );
	if ( thisShortcutIterator == g_shortcuts.end() ) {
		globalErrorStream() << "commandName " << Quoted( commandName.constData() ) << " not found in g_shortcuts.\n";
		return;
	}

	// 8. build an Accelerator
	const QKeySequence newAccel( QKeySequence_valid( accelerator )? accelerator : thisShortcutIterator->second.accelerator_default );
	// note: can skip the rest, if newAccel == current accel
	// 8. verify the key is still free, show a dialog to ask what to do if not
	VerifyAcceleratorNotTaken verify_visitor( commandName, newAccel, item->treeWidget() );
	GlobalShortcuts_foreach( verify_visitor );
	if ( verify_visitor.allow ) {
		// clear the ACTUAL accelerator first
		disconnect_accelerator( commandName );

		thisShortcutIterator->second.accelerator = newAccel;

		// write into the cell
		item->setText( 1, newAccel.toString() );
		accelerator_item_set_icon( item, thisShortcutIterator->second );

		// set the ACTUAL accelerator too!
		connect_accelerator( commandName );
	}
}

void accelerator_reset_all_button_clicked( QTreeWidget *tree ){
	for ( const auto&[name, value] : g_shortcuts ){ // at first disconnect all to avoid conflicts during connecting
		if( value.accelerator != value.accelerator_default ){ // can just do this for all, but it breaks menu accelerator labels :b
			// clear the ACTUAL accelerator
			disconnect_accelerator( name.c_str() );
		}
	}
	for ( auto&[name, value] : g_shortcuts ){
		if( value.accelerator != value.accelerator_default ){
			value.accelerator = value.accelerator_default;
			// set the ACTUAL accelerator
			connect_accelerator( name.c_str() );
		}
	}
	// update tree view
	for( QTreeWidgetItemIterator it( tree ); *it; ++it )
	{
		Shortcuts::const_iterator thisShortcutIterator = g_shortcuts.find( ( *it )->text( 0 ).toLatin1().constData() );
		if ( thisShortcutIterator != g_shortcuts.end() ) {
			// write into the cell
			( *it )->setText( 1, thisShortcutIterator->second.accelerator.toString() );
			accelerator_item_set_icon( ( *it ), thisShortcutIterator->second );
		}
	}
}


class Single_QKeySequenceEdit : public QKeySequenceEdit
{
protected:
	void keyPressEvent( QKeyEvent *e ) override {
		QKeySequenceEdit::keyPressEvent( e );
		if( e->modifiers() & Qt::KeypadModifier ) //. workaround Qt issue: Qt::KeypadModifier is ignored
			setKeySequence( QKeySequence( keySequence()[0] | Qt::KeypadModifier ) );
		if( QKeySequence_valid( keySequence() ) )
			clearFocus(); // trigger editingFinished(); via losing focus 🙉
			              // because this can still receive focus loss b4 getting deleted (practically because modal msgbox)
			              // and two editingFinished(); b no good
	}
	void focusOutEvent( QFocusEvent *event ) override {
		editingFinished();
	}
	bool event( QEvent *event ) override { // comsume ALL key presses including Tab
		if( event->type() == QEvent::KeyPress ){
			keyPressEvent( static_cast<QKeyEvent*>( event ) );
			return true;
		}
		return QKeySequenceEdit::event( event );
	}
};

void accelerator_edit( QTreeWidgetItem *item ){
		auto *edit = new Single_QKeySequenceEdit;
		QObject::connect( edit, &QKeySequenceEdit::editingFinished, [item, edit](){
			const QKeySequence accelerator = edit->keySequence();
			item->treeWidget()->setItemWidget( item, 1, nullptr );
			if( QKeySequence_valid( accelerator ) )
				accelerator_alter( item, accelerator );
		} );
		item->treeWidget()->setItemWidget( item, 1, edit );
		edit->setFocus(); // track sanity gently via edit being focused property
}

void DoCommandListDlg(){
	QDialog dialog( MainFrame_getWindow(), Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Mapped Commands" );

	auto *grid = new QGridLayout( &dialog );

	auto *tree = new QTreeWidget;
	grid->addWidget( tree, 1, 0, 1, 2 );
	tree->setColumnCount( 2 );
	tree->setSortingEnabled( true );
	tree->sortByColumn( 0, Qt::SortOrder::AscendingOrder );
	tree->setUniformRowHeights( true ); // optimization
	tree->setHorizontalScrollBarPolicy( Qt::ScrollBarPolicy::ScrollBarAlwaysOff );
	tree->setSizeAdjustPolicy( QAbstractScrollArea::SizeAdjustPolicy::AdjustToContents ); // scroll area will inherit column size
	tree->header()->setStretchLastSection( false ); // non greedy column sizing
	tree->header()->setSectionResizeMode( QHeaderView::ResizeMode::ResizeToContents ); // no text elision
	tree->setRootIsDecorated( false );
	tree->setHeaderLabels( { "Command", "Key" } );

	QObject::connect( tree, &QTreeWidget::itemActivated, []( QTreeWidgetItem *item, int column ){
		if( item != nullptr )
			accelerator_edit( item );
	} );

	{
		// Initialize dialog
		const auto path = StringStream( SettingsPath_get(), "commandlist.txt" );
		globalOutputStream() << "Writing the command list to " << path << '\n';

		TextFileOutputStream commandList( path );

		for( const auto&[ name, value ] : g_shortcuts )
		{
			auto *item = new QTreeWidgetItem( tree, { name.c_str(), value.accelerator.toString() } );
			accelerator_item_set_icon( item, value );

			if ( !commandList.failed() ) {
				int l = strlen( name.c_str() );
				commandList << name.c_str();
				while ( l++ < 32 )
					commandList << ' ';
				commandList << value.accelerator << '\n';
			}
		}
	}

	{
		auto *commandLine = new QLineEdit;
		grid->addWidget( commandLine, 0, 0 );
		commandLine->setClearButtonEnabled( true );
		commandLine->setPlaceholderText( QString::fromUtf8( "🔍 by command name" ) );

		auto *keyLine = new QLineEdit;
		grid->addWidget( keyLine, 0, 1 );
		keyLine->setClearButtonEnabled( true );
		keyLine->setPlaceholderText( QString::fromUtf8( "🔍 by keys" ) );

		const auto filter = [tree]( const int column, const QString& text ){
			for( QTreeWidgetItemIterator it( tree ); *it; ++it )
			{
				( *it )->setHidden( !( *it )->text( column ).contains( text, Qt::CaseSensitivity::CaseInsensitive ) );
			}
		};
		QObject::connect( commandLine, &QLineEdit::textChanged, [filter]( const QString& text ){ filter( 0, text ); } );
		QObject::connect( keyLine, &QLineEdit::textChanged, [filter]( const QString& text ){ filter( 1, text ); } );
	}

	{
		auto *buttons = new QDialogButtonBox( Qt::Orientation::Vertical );
		grid->addWidget( buttons, 1, 2, 1, 1 );

		QPushButton *editbutton = buttons->addButton( "Edit", QDialogButtonBox::ButtonRole::ActionRole );
		QObject::connect( editbutton, &QPushButton::clicked, [tree](){
			if( const auto items = tree->selectedItems(); !items.isEmpty() )
				accelerator_edit( items.first() );
		} );

		QPushButton *clearbutton = buttons->addButton( "Clear", QDialogButtonBox::ButtonRole::ActionRole );
		QObject::connect( clearbutton, &QPushButton::clicked, [tree](){
			if( const auto items = tree->selectedItems(); !items.isEmpty() )
				accelerator_clear_button_clicked( items.first() );
		} );

		QPushButton *resetbutton = buttons->addButton( "Reset", QDialogButtonBox::ButtonRole::ResetRole );
		QObject::connect( resetbutton, &QPushButton::clicked, [tree](){
			if( const auto items = tree->selectedItems(); !items.isEmpty() )
				accelerator_alter( items.first(), {} );
		} );

		QPushButton *resetallbutton = buttons->addButton( "Reset All", QDialogButtonBox::ButtonRole::ResetRole );
		QObject::connect( resetallbutton, &QPushButton::clicked, [tree](){
			if( eIDYES == qt_MessageBox( tree, "Surely reset all shortcuts now?", "Boo!", EMessageBoxType::Question ) )
				accelerator_reset_all_button_clicked( tree );
		} );
	}

	dialog.exec();
}

namespace
{
struct CommandLauncherEntry
{
	QString id;
	QString title;
	QString subtitle;
	QString search;
	int type{};
	int score{};
};

QString humanizeCommandName( const char* name ){
	QString out;
	const QString source = QString::fromLatin1( name );
	for( int i = 0; i < source.size(); ++i ){
		const QChar c = source[i];
		const QChar prev = i > 0 ? source[i - 1] : QChar();
		if( c == '_' || c == ':' || c == '-' ){
			out += ' ';
			continue;
		}
		if( i > 0 && c.isUpper() && ( prev.isLower() || prev.isDigit() ) ){
			out += ' ';
		}
		out += c;
	}
	return out.simplified();
}

QString commandLauncherTitle( const char* name ){
	if( string_equal( name, "MakeHollow" ) ){
		return "Make Hollow";
	}
	if( string_equal( name, "BrushExpand" ) ){
		return "Expand Brush";
	}
	if( string_equal( name, "BrushShrink" ) ){
		return "Shrink Brush";
	}
	if( string_equal( name, "CSGroom" ) ){
		return "Make Room";
	}
	if( string_equal( name, "CSGTool" ) ){
		return "Brush Shell Tool";
	}
	if( string_equal( name, "CSGSubtract" ) ){
		return "Carve / Subtract";
	}
	if( string_equal( name, "ToggleClipper" ) ){
		return "Toggle Clipper Tool";
	}
	if( string_equal( name, "ClipperClip" ) ){
		return "Clip Selected";
	}
	if( string_equal( name, "ClipperSplit" ) ){
		return "Split Selected";
	}
	if( string_equal( name, "ClipperFlip" ) ){
		return "Flip Clip Plane";
	}
	if( string_equal( name, "EntityMovePrimitivesToLast" ) ){
		return "Move Selection to Entity";
	}
	if( string_equal( name, "EntityMovePrimitivesToFirst" ) ){
		return "Move Selection to World";
	}
	if( string_equal( name, "EntityUngroup" ) ){
		return "Ungroup Entity";
	}
	if( string_equal( name, "AddEntityByName" ) ){
		return "Create Entity";
	}
	if( string_equal( name, "AddInfoPlayerStart" ) ){
		return "Create Player Start";
	}
	if( string_equal( name, "AddInfoPlayerDeathmatch" ) ){
		return "Create Deathmatch Start";
	}
	if( string_equal( name, "MakeDetail" ) ){
		return "Make Detail";
	}
	if( string_equal( name, "MakeStructural" ) ){
		return "Make Structural";
	}
	if( string_equal( name, "ShowHidden" ) || string_equal( name, "ShowHiddenAlt" ) ){
		return "Show Hidden";
	}
	if( string_equal( name, "HideSelected" ) ){
		return "Hide Selected";
	}
	if( string_equal( name, "SnapToGrid" ) ){
		return "Snap to Grid";
	}
	if( string_equal( name, "SelectAllOfType" ) ){
		return "Select All of Type";
	}
	if( string_equal( name, "MoveToCamera" ) ){
		return "Move to Camera";
	}
	if( string_equal( name, "PasteToCamera" ) ){
		return "Paste to Camera";
	}
	if( string_equal( name, "TransformDialog" ) ){
		return "Transform";
	}
	if( string_equal( name, "OpenAIMLEditor" ) ){
		return "AIML 3.0 Editor";
	}
	if( string_equal( name, "XYFocusOnSelected" ) ){
		return "Focus All 2D Views on Selected";
	}
	if( string_equal( name, "XYFocusActiveOnSelected" ) ){
		return "Focus Active 2D View on Selected";
	}
	if( string_equal( name, "CommandLauncher" ) ){
		return "Command Launcher";
	}
	if( string_equal( name, "OpenWysiwygWorkspace" ) ){
		return "Open WYSIWYG Workspace";
	}
	if( string_equal( name, "ToggleExperimentalProperties" ) ){
		return "Toggle Inspector";
	}
	if( string_equal( name, "ToggleExperimentalPreview" ) ){
		return "Toggle Viewport Preview";
	}
	if( string_equal( name, "ToggleExperimentalAssets" ) ){
		return "Toggle Asset Browser";
	}
	if( string_equal( name, "ToggleExperimentalHistory" ) ){
		return "Toggle History";
	}
	if( string_equal( name, "ToggleExperimentalSync" ) ){
		return "Toggle Live Sync";
	}
	if( string_equal( name, "ToggleExperimentalUSD" ) ){
		return "Toggle Outliner";
	}
	if( string_equal( name, "ToggleExperimentalECS" ) ){
		return "Toggle Entity Palette";
	}
	if( string_equal( name, "ImportUSDStructure" ) ){
		return "Import USD Scene Hierarchy";
	}
	if( string_equal( name, "ExportToUSDA" ) ){
		return "Export USDA Scene Hierarchy";
	}
	if( string_equal( name, "ImportMayaASCII" ) ){
		return "Import Maya ASCII";
	}
	if( string_equal( name, "ExportToMayaASCII" ) ){
		return "Export Maya ASCII";
	}
	return humanizeCommandName( name );
}

QString normalizeText( QString text ){
	text = text.toLower();
	for( QChar& c : text ){
		if( !c.isLetterOrNumber() ){
			c = ' ';
		}
	}
	return text.simplified();
}

QStringList normalizedWords( const QString& text ){
	const QString normalized = normalizeText( text );
	return normalized.isEmpty() ? QStringList() : normalized.split( ' ', Qt::SkipEmptyParts );
}

bool isSubsequence( const QString& needle, const QString& haystack ){
	if( needle.isEmpty() ){
		return true;
	}
	int i = 0;
	for( const QChar c : haystack ){
		if( c == needle[i] ){
			++i;
			if( i == needle.size() ){
				return true;
			}
		}
	}
	return false;
}

int boundedEditDistance( const QString& a, const QString& b, int maxDistance ){
	if( std::abs( a.size() - b.size() ) > maxDistance ){
		return maxDistance + 1;
	}

	std::vector<int> previous( b.size() + 1 );
	std::vector<int> current( b.size() + 1 );
	for( int j = 0; j <= b.size(); ++j )
		previous[j] = j;

	for( int i = 1; i <= a.size(); ++i ){
		current[0] = i;
		int rowBest = current[0];
		for( int j = 1; j <= b.size(); ++j ){
			const int substitutionCost = a[i - 1] == b[j - 1] ? 0 : 1;
			current[j] = std::min( {
				previous[j] + 1,
				current[j - 1] + 1,
				previous[j - 1] + substitutionCost
			} );
			rowBest = std::min( rowBest, current[j] );
		}
		if( rowBest > maxDistance ){
			return maxDistance + 1;
		}
		std::swap( previous, current );
	}
	return previous[b.size()];
}

int scoreWordMatch( const QString& queryWord, const QString& targetWord ){
	if( queryWord == targetWord ){
		return 140;
	}
	if( targetWord.startsWith( queryWord ) ){
		return 110;
	}
	if( targetWord.contains( queryWord ) ){
		return 90;
	}
	const int distance = boundedEditDistance( queryWord, targetWord, 2 );
	if( distance == 1 ){
		return 70;
	}
	if( distance == 2 ){
		return 55;
	}
	if( isSubsequence( queryWord, targetWord ) ){
		return 45;
	}
	return -1;
}

QStringList commandSearchAliases( const char* name ){
	if( string_equal( name, "MakeHollow" ) ){
		return { "make hollow", "make hallow", "hollow", "shell", "thicken brush", "wall thickness" };
	}
	if( string_equal( name, "BrushExpand" ) ){
		return { "expand", "inflate", "grow brush", "push faces out" };
	}
	if( string_equal( name, "BrushShrink" ) ){
		return { "shrink", "contract", "inset brush", "pull faces in" };
	}
	if( string_equal( name, "CSGroom" ) ){
		return { "make room", "make hollow", "make hallow", "hollow" };
	}
	if( string_equal( name, "CSGTool" ) ){
		return { "hollow tool", "shell tool", "brush shell", "thickness tool", "expand shrink hollow" };
	}
	if( string_equal( name, "CSGSubtract" ) ){
		return { "carve", "subtract", "boolean subtract", "cut out" };
	}
	if( string_equal( name, "ToggleClipper" ) ){
		return { "clip tool", "knife tool", "slice tool", "plane cut" };
	}
	if( string_equal( name, "ClipperClip" ) ){
		return { "clip selection", "cut selection", "apply clip" };
	}
	if( string_equal( name, "ClipperSplit" ) ){
		return { "split brush", "slice brush", "split selection" };
	}
	if( string_equal( name, "ClipperFlip" ) ){
		return { "flip clip", "invert clip", "reverse cut" };
	}
	if( string_equal( name, "CommandLauncher" ) ){
		return { "command palette", "tool finder", "action search" };
	}
	if( string_equal( name, "OpenWysiwygWorkspace" ) ){
		return { "workspace", "editor workspace", "unity layout", "unreal layout", "wysiwyg", "inspector", "outliner", "asset browser" };
	}
	if( string_equal( name, "ToggleExperimentalProperties" ) ){
		return { "inspector", "details panel", "properties" };
	}
	if( string_equal( name, "ToggleExperimentalPreview" ) ){
		return { "preview", "viewport preview", "material preview" };
	}
	if( string_equal( name, "ToggleExperimentalAssets" ) ){
		return { "asset browser", "content browser", "assets", "materials" };
	}
	if( string_equal( name, "ToggleExperimentalUSD" ) ){
		return { "outliner", "scene hierarchy", "hierarchy" };
	}
	if( string_equal( name, "CloneSelection" ) ){
		return { "duplicate" };
	}
	if( string_equal( name, "CloneSelectionAndMakeUnique" ) ){
		return { "duplicate unique" };
	}
	if( string_equal( name, "EntityMovePrimitivesToLast" ) ){
		return { "tie to entity", "group to entity", "parent to entity" };
	}
	if( string_equal( name, "EntityMovePrimitivesToFirst" ) ){
		return { "move to world", "to worldspawn", "remove from entity" };
	}
	if( string_equal( name, "EntityUngroup" ) ){
		return { "ungroup", "break apart entity", "detach children" };
	}
	if( string_equal( name, "AddEntityByName" ) ){
		return { "entity browser", "entity palette", "entity search", "create entity", "place entity", "add entity" };
	}
	if( string_equal( name, "AddInfoPlayerStart" ) ){
		return { "player start", "spawn point", "info player start", "start position" };
	}
	if( string_equal( name, "AddInfoPlayerDeathmatch" ) ){
		return { "deathmatch spawn", "dm start", "spawn point" };
	}
	if( string_equal( name, "MakeDetail" ) ){
		return { "detail", "convert to detail", "detail brush", "non structural" };
	}
	if( string_equal( name, "MakeStructural" ) ){
		return { "structural", "convert to structural", "world brush", "blocking brush" };
	}
	if( string_equal( name, "ShowHidden" ) || string_equal( name, "ShowHiddenAlt" ) ){
		return { "unhide", "show all hidden", "reveal hidden" };
	}
	if( string_equal( name, "HideSelected" ) ){
		return { "hide", "hide selection", "isolate inverse" };
	}
	if( string_equal( name, "HideUnselected" ) ){
		return { "show only selected" };
	}
	if( string_equal( name, "IsolateSelection" ) ){
		return { "solo selected", "isolate" };
	}
	if( string_equal( name, "SnapToGrid" ) ){
		return { "snap", "align to grid", "grid snap" };
	}
	if( string_equal( name, "SelectAllOfType" ) ){
		return { "select similar", "select same type", "select same class" };
	}
	if( string_equal( name, "SelectTextured" ) ){
		return { "select same material", "select same texture", "select textured" };
	}
	if( string_equal( name, "MoveToCamera" ) ){
		return { "bring to camera", "move selected to camera", "drop at camera" };
	}
	if( string_equal( name, "PasteToCamera" ) ){
		return { "paste at camera", "spawn at camera" };
	}
	if( string_equal( name, "TransformDialog" ) ){
		return { "position rotation scale", "prs", "transform panel", "numeric transform" };
	}
	if( string_equal( name, "RepeatTransforms" ) ){
		return { "repeat last transform", "repeat move", "repeat rotate", "repeat scale" };
	}
	if( string_equal( name, "OpenAIMLEditor" ) ){
		return { "aiml", "aiml 3", "chatbot", "bot editor", "bot script", "pattern template editor", "alice markup language" };
	}
	return {};
}

int computeLauncherScore( const QString& query, const QString& searchText, const QString& title, const QString& id ){
	if( query.isEmpty() ){
		return 1;
	}

	int score = 0;
	if( title.startsWith( query, Qt::CaseInsensitive ) ){
		score += 300;
	}
	if( normalizeText( title ).contains( query ) ){
		score += 220;
	}
	if( normalizeText( id ).contains( query ) ){
		score += 140;
	}
	if( searchText.startsWith( query ) ){
		score += 160;
	}
	if( searchText.contains( query ) ){
		score += 120;
	}
	if( isSubsequence( query, searchText ) ){
		score += 60;
	}

	const QStringList queryWords = normalizedWords( query );
	const QStringList targetWords = normalizedWords( searchText );
	for( const QString& queryWord : queryWords ){
		int best = -1;
		for( const QString& targetWord : targetWords ){
			best = std::max( best, scoreWordMatch( queryWord, targetWord ) );
		}
		if( best < 0 ){
			return 0;
		}
		score += best;
	}
	return score;
}

template<typename Functor>
void GlobalCommands_foreach( Functor&& functor ){
	for ( const auto& [name, command] : g_commands )
		functor( name.c_str(), command );
}

template<typename Functor>
void GlobalToggles_foreach( Functor&& functor ){
	for ( const auto& [name, toggle] : g_toggles )
		functor( name.c_str(), toggle );
}

std::vector<CommandLauncherEntry> collectLauncherEntries(){
	std::vector<CommandLauncherEntry> entries;
	entries.reserve( g_commands.size() + g_toggles.size() );

	GlobalCommands_foreach( [&]( const char* name, const Command& command ){
		if( string_equal( name, "Shortcuts" ) ){
			return;
		}
		CommandLauncherEntry entry;
		entry.id = name;
		entry.title = commandLauncherTitle( name );
		entry.subtitle = command.m_accelerator.toString();
		entry.search = normalizeText( entry.title + ' ' + entry.id + ' ' + commandSearchAliases( name ).join( ' ' ) );
		entry.type = 1;
		entries.emplace_back( std::move( entry ) );
	} );

	GlobalToggles_foreach( [&]( const char* name, const Toggle& toggle ){
		CommandLauncherEntry entry;
		entry.id = name;
		entry.title = commandLauncherTitle( name );
		entry.subtitle = toggle.m_command.m_accelerator.toString();
		entry.search = normalizeText( entry.title + ' ' + entry.id + ' ' + commandSearchAliases( name ).join( ' ' ) );
		entry.type = 2;
		entries.emplace_back( std::move( entry ) );
	} );

	return entries;
}

void executeLauncherEntry( const CommandLauncherEntry& entry ){
	if( entry.type == 1 ){
		GlobalCommands_find( entry.id.toLatin1().constData() ).m_callback();
	}
	else if( entry.type == 2 ){
		GlobalToggles_find( entry.id.toLatin1().constData() ).m_command.m_callback();
	}
}

void refillLauncherList( QListWidget& list, const std::vector<CommandLauncherEntry>& sourceEntries, const QString& query ){
	std::vector<CommandLauncherEntry> matches;
	matches.reserve( sourceEntries.size() );
	for( CommandLauncherEntry entry : sourceEntries ){
		entry.score = computeLauncherScore( query, entry.search, entry.title, entry.id );
		if( entry.score > 0 ){
			matches.emplace_back( std::move( entry ) );
		}
	}

	std::sort( matches.begin(), matches.end(), []( const CommandLauncherEntry& a, const CommandLauncherEntry& b ){
		if( a.score != b.score ){
			return a.score > b.score;
		}
		return QString::compare( a.title, b.title, Qt::CaseInsensitive ) < 0;
	} );

	list.clear();
	for( const CommandLauncherEntry& entry : matches ){
		auto *item = new QListWidgetItem( entry.title, &list );
		item->setData( Qt::UserRole, entry.id );
		item->setData( Qt::UserRole + 1, entry.type );
		item->setToolTip( entry.subtitle.isEmpty() ? entry.id : QString( "%1\n%2" ).arg( entry.id, entry.subtitle ) );
		if( !entry.subtitle.isEmpty() ){
			item->setText( QString( "%1    %2" ).arg( entry.title, entry.subtitle ) );
		}
	}

	if( list.count() > 0 ){
		list.setCurrentRow( 0 );
	}
}
}

void DoCommandLauncher(){
	QDialog dialog( MainFrame_getWindow(), Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Command Launcher" );
	dialog.resize( 720, 520 );

	auto *layout = new QVBoxLayout( &dialog );
	auto *searchLine = new QLineEdit( &dialog );
	searchLine->setClearButtonEnabled( true );
	searchLine->setPlaceholderText( "Type a tool or action name" );
	layout->addWidget( searchLine );

	auto *hintLabel = new QLabel( "Enter runs the selected action. Search matches command names, tool phrases, and shortcuts.", &dialog );
	layout->addWidget( hintLabel );

	auto *list = new QListWidget( &dialog );
	list->setUniformItemSizes( true );
	list->setAlternatingRowColors( true );
	layout->addWidget( list );

	auto *buttons = new QDialogButtonBox( QDialogButtonBox::Close, &dialog );
	auto *runButton = buttons->addButton( "Run", QDialogButtonBox::ButtonRole::AcceptRole );
	layout->addWidget( buttons );

	const std::vector<CommandLauncherEntry> entries = collectLauncherEntries();
	const auto refill = [&](){ refillLauncherList( *list, entries, normalizeText( searchLine->text() ) ); };
	refill();

	const auto runSelected = [&](){
		if( QListWidgetItem *item = list->currentItem() ){
			CommandLauncherEntry entry;
			entry.id = item->data( Qt::UserRole ).toString();
			entry.type = item->data( Qt::UserRole + 1 ).toInt();
			dialog.accept();
			executeLauncherEntry( entry );
		}
	};

	QObject::connect( searchLine, &QLineEdit::textChanged, [&](){ refill(); } );
	QObject::connect( searchLine, &QLineEdit::returnPressed, runSelected );
	QObject::connect( list, &QListWidget::itemActivated, [&]( QListWidgetItem* ){ runSelected(); } );
	QObject::connect( runButton, &QPushButton::clicked, runSelected );
	QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );

	searchLine->setFocus();
	dialog.exec();
}



#include "profile/profile2.h"

const char* const COMMANDS_VERSION = "1.0-gtk-accelnames";

static void MigrateLegacyShortcuts(){
	auto toggleTextures = g_shortcuts.find( "ToggleTextures" );
	auto mouseTransform = g_shortcuts.find( "MouseTransform" );
	if ( toggleTextures != g_shortcuts.end() && mouseTransform != g_shortcuts.end() ) {
		// Migrate only the exact legacy defaults:
		// ToggleTextures=Alt+T and MouseTransform=T.
		// This avoids overriding user-customized mappings.
		if ( toggleTextures->second.accelerator == QKeySequence( "Alt+T" )
		  && mouseTransform->second.accelerator == QKeySequence( "T" ) ) {
			toggleTextures->second.accelerator = QKeySequence( "T" );
			mouseTransform->second.accelerator = {};
			globalOutputStream() << "migrated legacy shortcuts: ToggleTextures Alt+T->T, MouseTransform T->unbound\n";
		}
	}

	// Migrate NavMesh rebuild away from RegionSetSelection legacy clash.
	auto navmeshRebuild = g_shortcuts.find( "NavMesh_Rebuild" );
	auto regionSetSelection = g_shortcuts.find( "RegionSetSelection" );
	if ( navmeshRebuild != g_shortcuts.end() && regionSetSelection != g_shortcuts.end()
	  && navmeshRebuild->second.accelerator_default == QKeySequence( "Ctrl+Shift+N" )
	  && navmeshRebuild->second.accelerator == QKeySequence( "Ctrl+Shift+R" )
	  && regionSetSelection->second.accelerator == QKeySequence( "Ctrl+Shift+R" ) ) {
		navmeshRebuild->second.accelerator = navmeshRebuild->second.accelerator_default;
		globalOutputStream() << "migrated legacy shortcuts: NavMesh_Rebuild Ctrl+Shift+R->Ctrl+Shift+N\n";
	}
}

void SaveCommandMap( const char* path ){
	const auto strINI = StringStream( path, "shortcuts.ini" );

	TextFileOutputStream file( strINI );
	if ( !file.failed() ) {
		file << "[Version]\n";
		file << "number=" << COMMANDS_VERSION << '\n';
		file << '\n';
		file << "[Commands]\n";

		auto writeCommandMap = [&file]( const char* name, const QKeySequence& accelerator ){
			file << name << '=';
			file << accelerator;
			file << '\n';
		};
		GlobalShortcuts_foreach( writeCommandMap );
	}
}

class ReadCommandMap
{
	const IniFile& m_ini;
	std::size_t m_count;
public:
	ReadCommandMap( const IniFile& ini ) : m_ini( ini ), m_count( 0 ){
	}
	void operator()( const char* name, QKeySequence& accelerator ){
		if ( auto value = m_ini.getValue( "Commands", name ) ) {
			if ( string_empty( *value ) ) {
				accelerator = {};
			}
			else{
				accelerator = QKeySequence( *value );
				if ( QKeySequence_valid( accelerator ) ) {
					++m_count;
				}
				else
				{
					globalWarningStream() << "WARNING: failed to parse user command " << Quoted( name ) << ": unknown key " << Quoted( *value ) << '\n';
				}
			}
		}
	}
	std::size_t count() const {
		return m_count;
	}
};

void LoadCommandMap( const char* path ){
	const auto strINI = StringStream( path, "shortcuts.ini" );

	if ( IniFile ini; ini.read( strINI ) ) {
		globalOutputStream() << "loading custom shortcuts list from " << Quoted( strINI ) << '\n';

		const Version version = version_parse( COMMANDS_VERSION );
		const Version dataVersion = version_parse( ini.getValue( "Version", "number" ).value_or( "" ) );

		if ( version_compatible( version, dataVersion ) ) {
			globalOutputStream() << "commands import: data version " << dataVersion << " is compatible with code version " << version << '\n';
			ReadCommandMap visitor( ini );
			GlobalShortcuts_foreach( visitor );
			MigrateLegacyShortcuts();
			globalOutputStream() << "parsed " << visitor.count() << " custom shortcuts\n";
		}
		else
		{
			globalWarningStream() << "commands import: data version " << dataVersion << " is not compatible with code version " << version << '\n';
		}
	}
	else
	{
		globalWarningStream() << "failed to load custom shortcuts from " << Quoted( strINI ) << '\n';
	}
}
