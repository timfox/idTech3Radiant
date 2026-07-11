#include "cli.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#include "stream/textstream.h"
#include "stream/stringstream.h"
#include "os/path.h"
#include "commandlib.h"

#include "map.h"
#include "mainframe.h"
#include "environment.h"
#include "preferences.h"
#include "commands.h"
#include "build.h"
#include "qe3.h"
#include "iselection.h"

#include <QCoreApplication>

bool g_headless = false;

bool cli_is_headless( int argc, char* argv[] ){
	for ( int i = 1; i < argc; ++i ){
		if ( strcmp( argv[i], "--headless" ) == 0
		  || strcmp( argv[i], "--cli" ) == 0
		  || strcmp( argv[i], "--exec" ) == 0
		  || strcmp( argv[i], "--batch" ) == 0 ){
			return true;
		}
	}
	return false;
}

namespace
{

void cli_print_help(){
	globalOutputStream()
		<< "NetRadiant-Custom CLI Mode\n"
		<< "Usage: radiant --headless [options] [commands]\n"
		<< "       radiant --exec <script.txt>\n"
		<< "\n"
		<< "Options:\n"
		<< "  --headless          Start in headless CLI mode\n"
		<< "  --exec <file>       Execute commands from a script file\n"
		<< "  --batch <cmds>      Execute semicolon-separated commands\n"
		<< "\n"
		<< "Commands (use in --exec scripts or --batch):\n"
		<< "  new                 Create a new empty map\n"
		<< "  open <file.map>     Open a map file\n"
		<< "  save                Save the current map\n"
		<< "  saveas <file.map>   Save the current map to a new file\n"
		<< "  import <file.map>   Import a map file into the current map\n"
		<< "  export <file.map>   Export selected brushes/entities to a file\n"
		<< "  info                Print map info (entity/brush counts)\n"
		<< "  selectall           Select all objects\n"
		<< "  deselectall         Deselect all objects\n"
		<< "  cmd <CommandName>   Execute any registered Radiant command\n"
		<< "  quit                Exit the editor\n"
		<< "  help                Show this help message\n"
		<< "\n";
}

bool cli_exec_command( const std::string& line ){
	if ( line.empty() || line[0] == '#' ) return true;

	std::istringstream iss( line );
	std::string cmd;
	iss >> cmd;

	if ( cmd == "help" || cmd == "?" ){
		cli_print_help();
	}
	else if ( cmd == "quit" || cmd == "exit" ){
		return false;
	}
	else if ( cmd == "new" ){
		Map_New();
		globalOutputStream() << "CLI: new map created\n";
	}
	else if ( cmd == "open" || cmd == "load" ){
		std::string path;
		iss >> path;
		if ( path.empty() ){
			globalErrorStream() << "CLI: open requires a file path\n";
		}
		else{
			Map_Free();
			Map_LoadFile( path.c_str() );
			globalOutputStream() << "CLI: loaded " << path.c_str() << '\n';
		}
	}
	else if ( cmd == "save" ){
		if ( Map_Unnamed( g_map ) ){
			globalErrorStream() << "CLI: map is unnamed, use 'saveas <path>'\n";
		}
		else{
			Map_SaveFile( Map_Name( g_map ) );
			globalOutputStream() << "CLI: saved " << Map_Name( g_map ) << '\n';
		}
	}
	else if ( cmd == "saveas" ){
		std::string path;
		iss >> path;
		if ( path.empty() ){
			globalErrorStream() << "CLI: saveas requires a file path\n";
		}
		else{
			Map_Rename( path.c_str() );
			Map_SaveFile( path.c_str() );
			globalOutputStream() << "CLI: saved as " << path.c_str() << '\n';
		}
	}
	else if ( cmd == "import" ){
		std::string path;
		iss >> path;
		if ( path.empty() ){
			globalErrorStream() << "CLI: import requires a file path\n";
		}
		else{
			Map_ImportFile( path.c_str() );
			globalOutputStream() << "CLI: imported " << path.c_str() << '\n';
		}
	}
	else if ( cmd == "export" ){
		std::string path;
		iss >> path;
		if ( path.empty() ){
			globalErrorStream() << "CLI: export requires a file path\n";
		}
		else{
			Map_SaveSelected( path.c_str() );
			globalOutputStream() << "CLI: exported to " << path.c_str() << '\n';
		}
	}
	else if ( cmd == "selectall" ){
		GlobalSelectionSystem().setSelectedAll( true );
		globalOutputStream() << "CLI: selected all\n";
	}
	else if ( cmd == "deselectall" ){
		GlobalSelectionSystem().setSelectedAll( false );
		globalOutputStream() << "CLI: deselected all\n";
	}
	else if ( cmd == "info" ){
		globalOutputStream() << "CLI: map = " << Map_Name( g_map ) << '\n';
	}
	else if ( cmd == "cmd" ){
		std::string cmdName;
		iss >> cmdName;
		if ( cmdName.empty() ){
			globalErrorStream() << "CLI: cmd requires a command name\n";
		}
		else{
			GlobalCommands_find( cmdName.c_str() ).m_callback();
			globalOutputStream() << "CLI: executed command '" << cmdName.c_str() << "'\n";
		}
	}
	else{
		globalErrorStream() << "CLI: unknown command '" << cmd.c_str() << "' (type 'help' for usage)\n";
	}
	return true;
}

void cli_exec_file( const char* filename ){
	std::ifstream file( filename );
	if ( !file.is_open() ){
		globalErrorStream() << "CLI: could not open script file: " << filename << '\n';
		return;
	}
	globalOutputStream() << "CLI: executing script " << filename << '\n';
	std::string line;
	int lineNum = 0;
	while ( std::getline( file, line ) ){
		++lineNum;
		auto trimmed = line;
		auto start = trimmed.find_first_not_of( " \t" );
		if ( start != std::string::npos ) trimmed = trimmed.substr( start );
		if ( !cli_exec_command( trimmed ) ) break;
	}
	globalOutputStream() << "CLI: script finished (" << lineNum << " lines)\n";
}

void cli_exec_batch( const char* commands ){
	std::string cmds( commands );
	std::istringstream iss( cmds );
	std::string cmd;
	while ( std::getline( iss, cmd, ';' ) ){
		auto start = cmd.find_first_not_of( " \t" );
		if ( start != std::string::npos ) cmd = cmd.substr( start );
		if ( !cli_exec_command( cmd ) ) break;
	}
}

void cli_interactive(){
	globalOutputStream() << "NetRadiant-Custom CLI Mode (type 'help' for commands, 'quit' to exit)\n";
	char buf[4096];
	while ( true ){
		fprintf( stdout, "radiant> " );
		fflush( stdout );
		if ( !fgets( buf, sizeof( buf ), stdin ) ) break;
		std::string line( buf );
		if ( !line.empty() && line.back() == '\n' ) line.pop_back();
		if ( !cli_exec_command( line ) ) break;
	}
}

}

int cli_main( int argc, char* argv[] ){
	g_headless = true;

	const char* execFile = nullptr;
	const char* batchCmds = nullptr;

	for ( int i = 1; i < argc; ++i ){
		if ( strcmp( argv[i], "--exec" ) == 0 && i + 1 < argc ){
			execFile = argv[++i];
		}
		else if ( strcmp( argv[i], "--batch" ) == 0 && i + 1 < argc ){
			batchCmds = argv[++i];
		}
	}

	Map_New();

	if( !g_openMapByCmd.empty() ){
		Map_Free();
		Map_LoadFile( g_openMapByCmd.c_str() );
	}

	if ( execFile != nullptr ){
		cli_exec_file( execFile );
	}
	else if ( batchCmds != nullptr ){
		cli_exec_batch( batchCmds );
	}
	else{
		cli_interactive();
	}

	Map_Free();
	return EXIT_SUCCESS;
}
