#include "usdparse.h"

#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>

#include "ientity.h"
#include "ibrush.h"
#include "ipatch.h"
#include "ieclass.h"
#include "scenelib.h"
#include "eclasslib.h"
#include "stream/textstream.h"
#include "math/vector.h"

namespace
{

struct UsdVec3 { double x, y, z; };

UsdVec3 operator+( const UsdVec3& a, const UsdVec3& b ){ return { a.x + b.x, a.y + b.y, a.z + b.z }; }
UsdVec3 operator-( const UsdVec3& a, const UsdVec3& b ){ return { a.x - b.x, a.y - b.y, a.z - b.z }; }
UsdVec3 operator*( const UsdVec3& a, double s ){ return { a.x * s, a.y * s, a.z * s }; }
double dot( const UsdVec3& a, const UsdVec3& b ){ return a.x * b.x + a.y * b.y + a.z * b.z; }
UsdVec3 cross( const UsdVec3& a, const UsdVec3& b ){ return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
double length( const UsdVec3& a ){ return std::sqrt( dot( a, a ) ); }

UsdVec3 yUpToZUp( const UsdVec3& v ){
	return { v.x, -v.z, v.y };
}

struct UsdMesh {
	std::string name;
	std::vector<UsdVec3> points;
	std::vector<int> faceVertexCounts;
	std::vector<int> faceVertexIndices;
	UsdVec3 translate;
	bool hasTranslate;
};

struct UsdLight {
	std::string name;
	std::string type;
	double intensity;
	UsdVec3 color;
	UsdVec3 translate;
};

struct UsdXform {
	std::string name;
	UsdVec3 translate;
	bool hasTranslate;
};

struct UsdScene {
	std::vector<UsdMesh> meshes;
	std::vector<UsdLight> lights;
	std::vector<UsdXform> xforms;
};


std::string trim( const std::string& s ){
	auto a = s.find_first_not_of( " \t\r\n" );
	if ( a == std::string::npos ) return {};
	return s.substr( a, s.find_last_not_of( " \t\r\n" ) - a + 1 );
}

bool startsWith( const std::string& s, const char* p ){
	return s.compare( 0, strlen( p ), p ) == 0;
}

std::string extractQuoted( const std::string& line, size_t startPos = 0 ){
	auto q1 = line.find( '"', startPos );
	if ( q1 == std::string::npos ) return {};
	auto q2 = line.find( '"', q1 + 1 );
	if ( q2 == std::string::npos ) return {};
	return line.substr( q1 + 1, q2 - q1 - 1 );
}

void parseFloatArray( const std::string& block, std::vector<double>& out ){
	const char* p = block.c_str();
	char* end;
	while ( *p ){
		while ( *p && ( *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',' || *p == '(' || *p == ')' || *p == '[' || *p == ']' ) )
			++p;
		if ( !*p ) break;
		double v = strtod( p, &end );
		if ( end == p ) { ++p; continue; }
		out.push_back( v );
		p = end;
	}
}

void parseIntArray( const std::string& block, std::vector<int>& out ){
	const char* p = block.c_str();
	char* end;
	while ( *p ){
		while ( *p && ( *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',' || *p == '[' || *p == ']' ) )
			++p;
		if ( !*p ) break;
		long v = strtol( p, &end, 10 );
		if ( end == p ) { ++p; continue; }
		out.push_back( static_cast<int>( v ) );
		p = end;
	}
}

std::string readBlock( std::istringstream& stream, const std::string& firstLine ){
	std::string block = firstLine;
	int depth = 0;
	for ( char c : firstLine ){
		if ( c == '[' || c == '(' ) ++depth;
		if ( c == ']' || c == ')' ) --depth;
	}
	if ( depth <= 0 ) return block;
	std::string line;
	while ( std::getline( stream, line ) ){
		block += '\n';
		block += line;
		for ( char c : line ){
			if ( c == '[' || c == '(' ) ++depth;
			if ( c == ']' || c == ')' ) --depth;
		}
		if ( depth <= 0 ) break;
	}
	return block;
}

UsdScene parseUsdFile( const char* data, size_t len ){
	UsdScene scene;
	std::string content( data, len );
	std::istringstream stream( content );

	enum DefType { None, Mesh, DistantLight, SphereLight, DomeLight, RectLight, Xform, Scope, Camera };
	DefType curType = None;
	UsdMesh* curMesh = nullptr;
	UsdLight* curLight = nullptr;
	UsdXform* curXform = nullptr;
	int braceDepth = 0;
	int defDepth = -1;

	std::string line;
	while ( std::getline( stream, line ) ){
		auto t = trim( line );
		if ( t.empty() || t[0] == '#' ) continue;

		for ( char c : t ){
			if ( c == '{' ) ++braceDepth;
			if ( c == '}' ) --braceDepth;
		}

		if ( braceDepth <= defDepth && defDepth >= 0 ){
			curType = None;
			curMesh = nullptr;
			curLight = nullptr;
			curXform = nullptr;
			defDepth = -1;
		}

		if ( startsWith( t, "def " ) ){
			std::string name = extractQuoted( t );
			defDepth = braceDepth;

			if ( t.find( "Mesh" ) != std::string::npos ){
				curType = Mesh;
				scene.meshes.push_back( {} );
				curMesh = &scene.meshes.back();
				curMesh->name = name;
				curMesh->translate = { 0, 0, 0 };
				curMesh->hasTranslate = false;
			}
			else if ( t.find( "DistantLight" ) != std::string::npos || t.find( "SphereLight" ) != std::string::npos
			       || t.find( "RectLight" ) != std::string::npos || t.find( "DomeLight" ) != std::string::npos ){
				curType = SphereLight;
				scene.lights.push_back( {} );
				curLight = &scene.lights.back();
				curLight->name = name;
				curLight->intensity = 1.0;
				curLight->color = { 1, 1, 1 };
				curLight->translate = { 0, 0, 0 };
			}
			else if ( t.find( "Xform" ) != std::string::npos ){
				curType = Xform;
				scene.xforms.push_back( {} );
				curXform = &scene.xforms.back();
				curXform->name = name;
				curXform->translate = { 0, 0, 0 };
				curXform->hasTranslate = false;
			}
			continue;
		}

		if ( curType == Mesh && curMesh != nullptr ){
			if ( t.find( "point3f[] points" ) != std::string::npos ){
				std::string block = readBlock( stream, t );
				std::vector<double> f;
				parseFloatArray( block, f );
				for ( size_t i = 0; i + 2 < f.size(); i += 3 )
					curMesh->points.push_back( { f[i], f[i+1], f[i+2] } );
			}
			else if ( t.find( "int[] faceVertexCounts" ) != std::string::npos ){
				std::string block = readBlock( stream, t );
				parseIntArray( block, curMesh->faceVertexCounts );
			}
			else if ( t.find( "int[] faceVertexIndices" ) != std::string::npos ){
				std::string block = readBlock( stream, t );
				parseIntArray( block, curMesh->faceVertexIndices );
			}
			else if ( t.find( "double3 xformOp:translate" ) != std::string::npos ){
				std::vector<double> f;
				parseFloatArray( t, f );
				if ( f.size() >= 3 ){
					curMesh->translate = { f[0], f[1], f[2] };
					curMesh->hasTranslate = true;
				}
			}
		}
		else if ( curType == SphereLight && curLight != nullptr ){
			if ( t.find( "float inputs:intensity" ) != std::string::npos ){
				std::vector<double> f;
				parseFloatArray( t, f );
				if ( !f.empty() ) curLight->intensity = f.back();
			}
			else if ( t.find( "color3f inputs:color" ) != std::string::npos ){
				std::vector<double> f;
				parseFloatArray( t, f );
				if ( f.size() >= 3 ) curLight->color = { f[0], f[1], f[2] };
			}
			else if ( t.find( "double3 xformOp:translate" ) != std::string::npos ){
				std::vector<double> f;
				parseFloatArray( t, f );
				if ( f.size() >= 3 ) curLight->translate = { f[0], f[1], f[2] };
			}
		}
		else if ( curType == Xform && curXform != nullptr ){
			if ( t.find( "double3 xformOp:translate" ) != std::string::npos ){
				std::vector<double> f;
				parseFloatArray( t, f );
				if ( f.size() >= 3 ){
					curXform->translate = { f[0], f[1], f[2] };
					curXform->hasTranslate = true;
				}
			}
		}
	}
	return scene;
}

bool isEntityClassName( const std::string& name ){
	static const char* prefixes[] = {
		"info_", "light", "ammo_", "item_", "weapon_",
		"trigger_", "target_", "func_", "misc_", "worldspawn",
		"team_", "shooter_", "holdable_", "monster_", nullptr
	};
	for ( const char** p = prefixes; *p; ++p )
		if ( name.compare( 0, strlen( *p ), *p ) == 0 )
			return true;
	return false;
}

void addBrushForMesh( scene::Node& worldspawn, const UsdMesh& mesh ){
	if ( mesh.points.empty() || mesh.faceVertexCounts.empty() || mesh.faceVertexIndices.empty() ) return;
	if ( mesh.faceVertexCounts.size() < 4 ) return;

	std::vector<UsdVec3> pts( mesh.points.size() );
	for ( size_t i = 0; i < mesh.points.size(); ++i )
		pts[i] = yUpToZUp( mesh.points[i] + mesh.translate );

	static const char shader[] = "textures/common/caulk";
	std::vector<_QERFaceData> faces;

	int idx = 0;
	for ( int count : mesh.faceVertexCounts ){
		if ( count < 3 || static_cast<size_t>( idx + count ) > mesh.faceVertexIndices.size() ){
			idx += count;
			continue;
		}
		int i0 = mesh.faceVertexIndices[idx];
		int i1 = mesh.faceVertexIndices[idx + 1];
		int i2 = mesh.faceVertexIndices[idx + 2];
		idx += count;

		if ( static_cast<size_t>( i0 ) >= pts.size() || static_cast<size_t>( i1 ) >= pts.size() || static_cast<size_t>( i2 ) >= pts.size() )
			continue;

		const UsdVec3& p0 = pts[i0];
		const UsdVec3& p1 = pts[i1];
		const UsdVec3& p2 = pts[i2];
		UsdVec3 n = cross( p1 - p0, p2 - p0 );
		if ( length( n ) < 1e-8 ) continue;

		_QERFaceData fd;
		fd.m_p0 = DoubleVector3( p0.x, p0.y, p0.z );
		fd.m_p1 = DoubleVector3( p1.x, p1.y, p1.z );
		fd.m_p2 = DoubleVector3( p2.x, p2.y, p2.z );
		fd.m_shader = shader;
		fd.m_texdef.shift[0] = 0; fd.m_texdef.shift[1] = 0;
		fd.m_texdef.rotate = 0;
		fd.m_texdef.scale[0] = 0.5f; fd.m_texdef.scale[1] = 0.5f;
		fd.contents = 0; fd.flags = 0; fd.value = 0;
		faces.push_back( fd );
	}

	if ( faces.size() < 4 ) return;

	scene::Node& brushNode = GlobalBrushCreator().createBrush();
	for ( const auto& f : faces )
		GlobalBrushCreator().Brush_addFace( brushNode, f );
	Node_getTraversable( worldspawn )->insert( brushNode );
}

}


void UsdAscii_Read( scene::Node& root, TextInputStream& inputStream, EntityCreator& entityTable ){
	const size_t chunkSize = 65536;
	std::string content;
	char buf[chunkSize];
	for (;;){
		size_t n = inputStream.read( buf, chunkSize );
		if ( n == 0 ) break;
		content.append( buf, n );
	}

	if ( content.empty() ){
		globalWarningStream() << "USD: empty file\n";
		return;
	}

	UsdScene scene = parseUsdFile( content.c_str(), content.size() );

	globalOutputStream() << "USD: " << scene.meshes.size() << " meshes, "
	                     << scene.lights.size() << " lights, "
	                     << scene.xforms.size() << " xforms\n";

	EntityClass* worldspawnClass = GlobalEntityClassManager().findOrInsert( "worldspawn", true );
	scene::Node& worldspawn = entityTable.createEntity( worldspawnClass );
	Node_getEntity( worldspawn )->setKeyValue( "classname", "worldspawn" );
	Node_getTraversable( root )->insert( worldspawn );

	for ( const auto& mesh : scene.meshes )
		addBrushForMesh( worldspawn, mesh );

	for ( const auto& light : scene.lights ){
		UsdVec3 pos = yUpToZUp( light.translate );
		EntityClass* ec = GlobalEntityClassManager().findOrInsert( "light", false );
		scene::Node& ent = entityTable.createEntity( ec );
		Node_getEntity( ent )->setKeyValue( "classname", "light" );
		char originStr[128];
		snprintf( originStr, sizeof( originStr ), "%.0f %.0f %.0f", pos.x, pos.y, pos.z );
		Node_getEntity( ent )->setKeyValue( "origin", originStr );
		int lightVal = static_cast<int>( light.intensity * 300.0 );
		if ( lightVal < 1 ) lightVal = 300;
		char lightStr[32];
		snprintf( lightStr, sizeof( lightStr ), "%d", lightVal );
		Node_getEntity( ent )->setKeyValue( "light", lightStr );
		char colorStr[64];
		snprintf( colorStr, sizeof( colorStr ), "%.3f %.3f %.3f", light.color.x, light.color.y, light.color.z );
		Node_getEntity( ent )->setKeyValue( "_color", colorStr );
		Node_getTraversable( root )->insert( ent );
	}

	for ( const auto& xf : scene.xforms ){
		if ( !isEntityClassName( xf.name ) || !xf.hasTranslate ) continue;
		UsdVec3 pos = yUpToZUp( xf.translate );
		EntityClass* ec = GlobalEntityClassManager().findOrInsert( xf.name.c_str(), false );
		scene::Node& ent = entityTable.createEntity( ec );
		Node_getEntity( ent )->setKeyValue( "classname", xf.name.c_str() );
		char originStr[128];
		snprintf( originStr, sizeof( originStr ), "%.0f %.0f %.0f", pos.x, pos.y, pos.z );
		Node_getEntity( ent )->setKeyValue( "origin", originStr );
		Node_getTraversable( root )->insert( ent );
	}

	globalOutputStream() << "USD: map loaded successfully\n";
}
