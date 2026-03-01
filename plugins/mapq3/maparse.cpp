#include "maparse.h"

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

struct MaVec3 { double x, y, z; };

MaVec3 operator+( const MaVec3& a, const MaVec3& b ){ return { a.x + b.x, a.y + b.y, a.z + b.z }; }
MaVec3 operator-( const MaVec3& a, const MaVec3& b ){ return { a.x - b.x, a.y - b.y, a.z - b.z }; }
MaVec3 operator*( const MaVec3& a, double s ){ return { a.x * s, a.y * s, a.z * s }; }
MaVec3 cross( const MaVec3& a, const MaVec3& b ){ return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
double dot( const MaVec3& a, const MaVec3& b ){ return a.x * b.x + a.y * b.y + a.z * b.z; }
double length( const MaVec3& a ){ return std::sqrt( dot( a, a ) ); }
MaVec3 normalise( const MaVec3& a ){ double l = length( a ); return l > 1e-8 ? a * ( 1.0 / l ) : MaVec3{ 0, 0, 1 }; }

MaVec3 yUpToZUp( const MaVec3& v ){
	return { v.x, -v.z, v.y };
}

struct MaEdge { int a, b; };

struct MaFace {
	std::vector<int> edgeIndices;
	std::vector<int> uvIndices;
};

struct MaMesh {
	std::string name;
	std::string parent;
	std::vector<MaVec3> verts;
	std::vector<MaEdge> edges;
	std::vector<MaFace> faces;
	std::vector<MaVec3> normals;
	std::vector<MaVec3> faceNormals;
};

struct MaTransform {
	std::string name;
	std::string parent;
	MaVec3 translate;
	MaVec3 rotate;
	MaVec3 scale;
	bool hasTranslate;
	bool hasRotate;
	bool hasScale;
};

struct MaLight {
	std::string name;
	std::string parent;
	std::string type;
	double intensity;
	MaVec3 color;
};

struct MaNurbsSurface {
	std::string name;
	std::string parent;
	int degreeU, degreeV;
	int formU, formV;
	std::vector<double> knotsU, knotsV;
	int numCVsU, numCVsV;
	std::vector<MaVec3> cvs;
};

struct MaScene {
	std::vector<MaMesh> meshes;
	std::vector<MaTransform> transforms;
	std::vector<MaLight> lights;
	std::vector<MaNurbsSurface> nurbs;
};


std::string trim( const std::string& s ){
	auto a = s.find_first_not_of( " \t\r\n" );
	if ( a == std::string::npos ) return {};
	return s.substr( a, s.find_last_not_of( " \t\r\n" ) - a + 1 );
}

bool startsWith( const std::string& s, const char* p ){
	return s.compare( 0, strlen( p ), p ) == 0;
}

std::string extractFlag( const std::string& line, const char* flag ){
	auto pos = line.find( flag );
	if ( pos == std::string::npos ) return {};
	pos += strlen( flag );
	while ( pos < line.size() && line[pos] == ' ' ) ++pos;
	if ( pos >= line.size() || line[pos] != '"' ) return {};
	auto end = line.find( '"', pos + 1 );
	if ( end == std::string::npos ) return {};
	return line.substr( pos + 1, end - pos - 1 );
}

std::string readStatement( std::istringstream& stream, std::string& firstLine ){
	std::string stmt = firstLine;
	if ( !stmt.empty() && stmt.back() == ';' ){ stmt.pop_back(); return stmt; }
	std::string line;
	while ( std::getline( stream, line ) ){
		auto t = trim( line );
		stmt += ' ';
		stmt += t;
		if ( !t.empty() && t.back() == ';' ){ stmt.pop_back(); return stmt; }
	}
	return stmt;
}

void parseFloats( const char* p, std::vector<double>& out ){
	char* end;
	while ( *p ){
		while ( *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' ) ++p;
		if ( !*p ) break;
		double v = strtod( p, &end );
		if ( end == p ) break;
		out.push_back( v );
		p = end;
	}
}

void parseInts( const char* p, std::vector<int>& out ){
	char* end;
	while ( *p ){
		while ( *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' ) ++p;
		if ( !*p ) break;
		long v = strtol( p, &end, 10 );
		if ( end == p ) break;
		out.push_back( static_cast<int>( v ) );
		p = end;
	}
}

const char* findAfterBracket( const std::string& stmt, const char* attr ){
	auto pos = stmt.find( attr );
	if ( pos == std::string::npos ) return nullptr;
	pos = stmt.find( ']', pos );
	if ( pos == std::string::npos ) return nullptr;
	return stmt.c_str() + pos + 1;
}

const char* findAfterType( const char* p, const char* typeName ){
	const char* t = strstr( p, typeName );
	if ( t ) return t + strlen( typeName );
	return p;
}

void parseMeshAttr( const std::string& stmt, MaMesh& mesh ){
	if ( stmt.find( ".vt[" ) != std::string::npos || stmt.find( ".vrts[" ) != std::string::npos ){
		const char* p = findAfterBracket( stmt, ".vt[" );
		if ( !p ) p = findAfterBracket( stmt, ".vrts[" );
		if ( !p ) return;
		std::vector<double> f; parseFloats( p, f );
		for ( size_t i = 0; i + 2 < f.size(); i += 3 )
			mesh.verts.push_back( { f[i], f[i+1], f[i+2] } );
	}
	else if ( stmt.find( ".ed[" ) != std::string::npos ){
		const char* p = findAfterBracket( stmt, ".ed[" );
		if ( !p ) return;
		std::vector<int> iv; parseInts( p, iv );
		for ( size_t i = 0; i + 2 < iv.size(); i += 3 )
			mesh.edges.push_back( { iv[i], iv[i+1] } );
	}
	else if ( stmt.find( ".fc[" ) != std::string::npos && stmt.find( "polyFaces" ) != std::string::npos ){
		const char* p = stmt.c_str();
		while ( ( p = strstr( p, " f " ) ) != nullptr ){
			p += 3;
			char* end;
			int nv = static_cast<int>( strtol( p, &end, 10 ) );
			if ( end == p || nv < 3 ) break;
			p = end;
			MaFace face;
			for ( int i = 0; i < nv; ++i ){
				face.edgeIndices.push_back( static_cast<int>( strtol( p, &end, 10 ) ) );
				p = end;
			}
			while ( *p == ' ' || *p == '\t' ) ++p;
			if ( strncmp( p, "mu", 2 ) == 0 ){
				p += 2;
				strtol( p, &end, 10 ); p = end;
				int uc = static_cast<int>( strtol( p, &end, 10 ) ); p = end;
				for ( int i = 0; i < uc; ++i ){
					face.uvIndices.push_back( static_cast<int>( strtol( p, &end, 10 ) ) );
					p = end;
				}
			}
			if ( static_cast<int>( face.edgeIndices.size() ) == nv )
				mesh.faces.push_back( std::move( face ) );
		}
	}
}

void parseTransformAttr( const std::string& stmt, MaTransform& xf ){
	if ( stmt.find( ".t " ) != std::string::npos || stmt.find( ".t\"" ) != std::string::npos
	  || stmt.find( "\"translate\"" ) != std::string::npos || stmt.find( "\".t\"" ) != std::string::npos ){
		auto tp = stmt.find( "double3" );
		if ( tp == std::string::npos ) return;
		std::vector<double> f; parseFloats( stmt.c_str() + tp + 7, f );
		if ( f.size() >= 3 ){ xf.translate = { f[0], f[1], f[2] }; xf.hasTranslate = true; }
	}
	else if ( stmt.find( ".r " ) != std::string::npos || stmt.find( "\"rotate\"" ) != std::string::npos ){
		auto tp = stmt.find( "double3" );
		if ( tp == std::string::npos ) return;
		std::vector<double> f; parseFloats( stmt.c_str() + tp + 7, f );
		if ( f.size() >= 3 ){ xf.rotate = { f[0], f[1], f[2] }; xf.hasRotate = true; }
	}
	else if ( stmt.find( ".s " ) != std::string::npos || stmt.find( "\"scale\"" ) != std::string::npos ){
		auto tp = stmt.find( "double3" );
		if ( tp == std::string::npos ) return;
		std::vector<double> f; parseFloats( stmt.c_str() + tp + 7, f );
		if ( f.size() >= 3 ){ xf.scale = { f[0], f[1], f[2] }; xf.hasScale = true; }
	}
}

void parseLightAttr( const std::string& stmt, MaLight& light ){
	if ( stmt.find( ".in " ) != std::string::npos || stmt.find( ".intensity" ) != std::string::npos ){
		std::vector<double> f; parseFloats( stmt.c_str() + stmt.size() - 20, f );
		if ( !f.empty() ) light.intensity = f.back();
	}
	else if ( stmt.find( ".cl " ) != std::string::npos || stmt.find( ".color" ) != std::string::npos ){
		auto tp = stmt.find( "float3" );
		if ( tp == std::string::npos ) tp = stmt.find( "double3" );
		if ( tp != std::string::npos ){
			std::vector<double> f; parseFloats( stmt.c_str() + tp + 6, f );
			if ( f.size() >= 3 ) light.color = { f[0], f[1], f[2] };
		}
	}
}

void parseNurbsAttr( const std::string& stmt, MaNurbsSurface& ns ){
	if ( stmt.find( ".cc" ) != std::string::npos && stmt.find( "nurbsSurface" ) != std::string::npos ){
		const char* p = strstr( stmt.c_str(), "nurbsSurface" );
		if ( !p ) return;
		p += 12;
		char* end;
		ns.degreeU = static_cast<int>( strtol( p, &end, 10 ) ); p = end;
		ns.degreeV = static_cast<int>( strtol( p, &end, 10 ) ); p = end;
		ns.formU = static_cast<int>( strtol( p, &end, 10 ) ); p = end;
		ns.formV = static_cast<int>( strtol( p, &end, 10 ) ); p = end;
		int nku = static_cast<int>( strtol( p, &end, 10 ) ); p = end;
		for ( int i = 0; i < nku; ++i ){ ns.knotsU.push_back( strtod( p, &end ) ); p = end; }
		int nkv = static_cast<int>( strtol( p, &end, 10 ) ); p = end;
		for ( int i = 0; i < nkv; ++i ){ ns.knotsV.push_back( strtod( p, &end ) ); p = end; }
		ns.numCVsU = nku - ns.degreeU - 1;
		ns.numCVsV = nkv - ns.degreeV - 1;
		while ( *p ){
			while ( *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' ) ++p;
			if ( !*p ) break;
			double x = strtod( p, &end ); if ( end == p ) break; p = end;
			double y = strtod( p, &end ); if ( end == p ) break; p = end;
			double z = strtod( p, &end ); if ( end == p ) break; p = end;
			double w = strtod( p, &end ); if ( end == p ) break; p = end;
			if ( std::fabs( w ) > 1e-10 ){ x /= w; y /= w; z /= w; }
			ns.cvs.push_back( { x, y, z } );
		}
	}
}


MaScene parseMaFile( const char* data, size_t len ){
	MaScene scene;
	std::string content( data, len );
	std::istringstream stream( content );

	enum NodeType { None, Transform, Mesh, PointLight, DirLight, NurbsSurface };
	NodeType curType = None;
	MaMesh* curMesh = nullptr;
	MaTransform* curXf = nullptr;
	MaLight* curLight = nullptr;
	MaNurbsSurface* curNurbs = nullptr;

	std::string line;
	while ( std::getline( stream, line ) ){
		auto t = trim( line );
		if ( t.empty() || t[0] == '/' ) continue;

		if ( startsWith( t, "createNode " ) ){
			curType = None; curMesh = nullptr; curXf = nullptr; curLight = nullptr; curNurbs = nullptr;
			std::string nodeName = extractFlag( t, "-n " );
			std::string parent = extractFlag( t, "-p " );

			if ( startsWith( t, "createNode transform" ) || startsWith( t, "createNode joint" ) ){
				curType = Transform;
				scene.transforms.push_back( {} );
				curXf = &scene.transforms.back();
				curXf->name = nodeName;
				curXf->parent = parent;
				curXf->translate = { 0, 0, 0 };
				curXf->rotate = { 0, 0, 0 };
				curXf->scale = { 1, 1, 1 };
				curXf->hasTranslate = false;
				curXf->hasRotate = false;
				curXf->hasScale = false;
			}
			else if ( startsWith( t, "createNode mesh" ) ){
				curType = Mesh;
				scene.meshes.push_back( {} );
				curMesh = &scene.meshes.back();
				curMesh->name = nodeName;
				curMesh->parent = parent;
			}
			else if ( startsWith( t, "createNode pointLight" ) ){
				curType = PointLight;
				scene.lights.push_back( {} );
				curLight = &scene.lights.back();
				curLight->name = nodeName;
				curLight->parent = parent;
				curLight->type = "point";
				curLight->intensity = 1.0;
				curLight->color = { 1, 1, 1 };
			}
			else if ( startsWith( t, "createNode directionalLight" ) ){
				curType = DirLight;
				scene.lights.push_back( {} );
				curLight = &scene.lights.back();
				curLight->name = nodeName;
				curLight->parent = parent;
				curLight->type = "directional";
				curLight->intensity = 1.0;
				curLight->color = { 1, 1, 1 };
			}
			else if ( startsWith( t, "createNode nurbsSurface" ) ){
				curType = NurbsSurface;
				scene.nurbs.push_back( {} );
				curNurbs = &scene.nurbs.back();
				curNurbs->name = nodeName;
				curNurbs->parent = parent;
				curNurbs->degreeU = curNurbs->degreeV = 3;
			}
			continue;
		}

		if ( !startsWith( t, "setAttr" ) ) continue;

		std::string stmt = readStatement( stream, t );

		switch ( curType ){
		case Mesh: if ( curMesh ) parseMeshAttr( stmt, *curMesh ); break;
		case Transform: if ( curXf ) parseTransformAttr( stmt, *curXf ); break;
		case PointLight:
		case DirLight: if ( curLight ) parseLightAttr( stmt, *curLight ); break;
		case NurbsSurface: if ( curNurbs ) parseNurbsAttr( stmt, *curNurbs ); break;
		default: break;
		}
	}
	return scene;
}


int resolveEdge( const MaMesh& mesh, int signedIdx ){
	if ( signedIdx >= 0 ){
		return ( static_cast<size_t>( signedIdx ) < mesh.edges.size() ) ? mesh.edges[signedIdx].a : 0;
	}
	int idx = -signedIdx - 1;
	return ( static_cast<size_t>( idx ) < mesh.edges.size() ) ? mesh.edges[idx].b : 0;
}

std::vector<int> resolveFaceVerts( const MaMesh& mesh, const MaFace& face ){
	std::vector<int> v;
	for ( int e : face.edgeIndices )
		v.push_back( resolveEdge( mesh, e ) );
	return v;
}


MaVec3 getTransformOrigin( const MaScene& scene, const std::string& name ){
	for ( const auto& xf : scene.transforms ){
		if ( xf.name == name && xf.hasTranslate )
			return xf.translate;
	}
	return { 0, 0, 0 };
}

MaVec3 getMeshParentOrigin( const MaScene& scene, const MaMesh& mesh ){
	if ( mesh.parent.empty() ) return { 0, 0, 0 };
	return getTransformOrigin( scene, mesh.parent );
}


bool isEntityClassName( const std::string& name ){
	static const char* prefixes[] = {
		"info_", "light", "ammo_", "item_", "weapon_",
		"trigger_", "target_", "func_", "misc_", "worldspawn",
		"team_", "shooter_", "holdable_", "monster_", nullptr
	};
	for ( const char** p = prefixes; *p; ++p ){
		if ( name.compare( 0, strlen( *p ), *p ) == 0 )
			return true;
	}
	return false;
}


void addBrushForMesh( scene::Node& worldspawn, const MaScene& scene, const MaMesh& mesh ){
	if ( mesh.verts.empty() || mesh.edges.empty() || mesh.faces.empty() ) return;
	if ( mesh.faces.size() < 4 ) return;

	MaVec3 origin = getMeshParentOrigin( scene, mesh );

	std::vector<MaVec3> worldVerts( mesh.verts.size() );
	for ( size_t i = 0; i < mesh.verts.size(); ++i ){
		worldVerts[i] = yUpToZUp( mesh.verts[i] + origin );
	}

	static const char shader[] = "textures/common/caulk";

	std::vector<_QERFaceData> faceDatas;
	for ( const auto& face : mesh.faces ){
		auto vi = resolveFaceVerts( mesh, face );
		if ( vi.size() < 3 ) continue;

		int i0 = vi[0], i1 = vi[1], i2 = vi[2];
		if ( static_cast<size_t>( i0 ) >= worldVerts.size()
		  || static_cast<size_t>( i1 ) >= worldVerts.size()
		  || static_cast<size_t>( i2 ) >= worldVerts.size() ) continue;

		const MaVec3& p0 = worldVerts[i0];
		const MaVec3& p1 = worldVerts[i1];
		const MaVec3& p2 = worldVerts[i2];

		MaVec3 e1 = p1 - p0, e2 = p2 - p0;
		MaVec3 n = cross( e2, e1 );
		if ( length( n ) < 1e-8 ) continue;

		_QERFaceData faceData;
		faceData.m_p0 = DoubleVector3( p0.x, p0.y, p0.z );
		faceData.m_p1 = DoubleVector3( p1.x, p1.y, p1.z );
		faceData.m_p2 = DoubleVector3( p2.x, p2.y, p2.z );
		faceData.m_shader = shader;
		faceData.m_texdef.shift[0] = 0;
		faceData.m_texdef.shift[1] = 0;
		faceData.m_texdef.rotate = 0;
		faceData.m_texdef.scale[0] = 0.5f;
		faceData.m_texdef.scale[1] = 0.5f;
		faceData.contents = 0;
		faceData.flags = 0;
		faceData.value = 0;

		faceDatas.push_back( faceData );
	}

	if ( faceDatas.size() < 4 ) return;

	scene::Node& brushNode = GlobalBrushCreator().createBrush();
	for ( const auto& fd : faceDatas ){
		GlobalBrushCreator().Brush_addFace( brushNode, fd );
	}

	Node_getTraversable( worldspawn )->insert( brushNode );
}


void addPatchForNurbs( scene::Node& worldspawn, const MaScene& scene, const MaNurbsSurface& ns ){
	if ( ns.cvs.empty() || ns.numCVsU < 2 || ns.numCVsV < 2 ) return;

	MaVec3 origin = { 0, 0, 0 };
	if ( !ns.parent.empty() )
		origin = getTransformOrigin( scene, ns.parent );

	int patchW = ns.numCVsU;
	int patchH = ns.numCVsV;
	if ( patchW % 2 == 0 ) ++patchW;
	if ( patchH % 2 == 0 ) ++patchH;
	if ( patchW < 3 ) patchW = 3;
	if ( patchH < 3 ) patchH = 3;

	scene::Node& patchNode = GlobalPatchCreator().createPatch();
	GlobalPatchCreator().Patch_resize( patchNode, patchW, patchH );

	PatchControlMatrix ctrl = GlobalPatchCreator().Patch_getControlPoints( patchNode );
	for ( int r = 0; r < patchH; ++r ){
		for ( int c = 0; c < patchW; ++c ){
			int srcR = std::min( r, ns.numCVsV - 1 );
			int srcC = std::min( c, ns.numCVsU - 1 );
			int idx = srcR * ns.numCVsU + srcC;
			if ( static_cast<size_t>( idx ) < ns.cvs.size() ){
				MaVec3 p = yUpToZUp( ns.cvs[idx] + origin );
				ctrl( r, c ).m_vertex = Vector3( static_cast<float>( p.x ), static_cast<float>( p.y ), static_cast<float>( p.z ) );
			}
			double u = ( patchW > 1 ) ? static_cast<double>( c ) / ( patchW - 1 ) : 0;
			double v = ( patchH > 1 ) ? static_cast<double>( r ) / ( patchH - 1 ) : 0;
			ctrl( r, c ).m_texcoord = Vector2( static_cast<float>( u ), static_cast<float>( v ) );
		}
	}
	GlobalPatchCreator().Patch_controlPointsChanged( patchNode );
	GlobalPatchCreator().Patch_setShader( patchNode, "textures/common/caulk" );

	scene::Traversable* traversable = Node_getTraversable( worldspawn );
	if ( traversable ) traversable->insert( patchNode );
}


}


void MayaAscii_Read( scene::Node& root, TextInputStream& inputStream, EntityCreator& entityTable ){
	const std::size_t chunkSize = 65536;
	std::string fileContent;
	char buf[chunkSize];
	for (;;){
		std::size_t n = inputStream.read( buf, chunkSize );
		if ( n == 0 ) break;
		fileContent.append( buf, n );
	}

	if ( fileContent.empty() ){
		globalWarningStream() << "Maya ASCII: empty file\n";
		return;
	}

	MaScene scene = parseMaFile( fileContent.c_str(), fileContent.size() );

	globalOutputStream() << "Maya ASCII: " << scene.transforms.size() << " transforms, "
	                     << scene.meshes.size() << " meshes, "
	                     << scene.lights.size() << " lights, "
	                     << scene.nurbs.size() << " nurbs\n";

	EntityClass* worldspawnClass = GlobalEntityClassManager().findOrInsert( "worldspawn", true );
	scene::Node& worldspawn = entityTable.createEntity( worldspawnClass );
	Node_getEntity( worldspawn )->setKeyValue( "classname", "worldspawn" );
	Node_getTraversable( root )->insert( worldspawn );

	for ( const auto& mesh : scene.meshes ){
		addBrushForMesh( worldspawn, scene, mesh );
	}

	for ( const auto& ns : scene.nurbs ){
		addPatchForNurbs( worldspawn, scene, ns );
	}

	auto createPointEntity = [&]( const char* classname, const MaVec3& pos ){
		EntityClass* ec = GlobalEntityClassManager().findOrInsert( classname, false );
		scene::Node& ent = entityTable.createEntity( ec );
		Node_getEntity( ent )->setKeyValue( "classname", classname );
		char originStr[128];
		snprintf( originStr, sizeof( originStr ), "%.0f %.0f %.0f", pos.x, pos.y, pos.z );
		Node_getEntity( ent )->setKeyValue( "origin", originStr );
		return &ent;
	};

	for ( const auto& light : scene.lights ){
		MaVec3 origin = { 0, 0, 0 };
		if ( !light.parent.empty() )
			origin = getTransformOrigin( scene, light.parent );
		MaVec3 pos = yUpToZUp( origin );

		scene::Node* ent = createPointEntity( "light", pos );

		int lightVal = static_cast<int>( light.intensity * 300.0 );
		if ( lightVal < 1 ) lightVal = 300;
		char lightStr[32];
		snprintf( lightStr, sizeof( lightStr ), "%d", lightVal );
		Node_getEntity( *ent )->setKeyValue( "light", lightStr );

		char colorStr[64];
		snprintf( colorStr, sizeof( colorStr ), "%.3f %.3f %.3f", light.color.x, light.color.y, light.color.z );
		Node_getEntity( *ent )->setKeyValue( "_color", colorStr );

		Node_getTraversable( root )->insert( *ent );
	}

	for ( const auto& xf : scene.transforms ){
		if ( !isEntityClassName( xf.name ) ) continue;

		bool hasMesh = false;
		for ( const auto& m : scene.meshes ){
			if ( m.parent == xf.name ){ hasMesh = true; break; }
		}
		if ( hasMesh ) continue;

		MaVec3 pos = yUpToZUp( xf.translate );
		scene::Node* ent = createPointEntity( xf.name.c_str(), pos );

		if ( xf.hasRotate && ( xf.rotate.x != 0 || xf.rotate.y != 0 || xf.rotate.z != 0 ) ){
			char angleStr[32];
			snprintf( angleStr, sizeof( angleStr ), "%.0f", xf.rotate.y );
			Node_getEntity( *ent )->setKeyValue( "angle", angleStr );
		}

		Node_getTraversable( root )->insert( *ent );
	}

	globalOutputStream() << "Maya ASCII: map loaded successfully\n";
}
