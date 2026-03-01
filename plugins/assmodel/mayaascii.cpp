#include "mayaascii.h"

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include "stream/textstream.h"

namespace
{

struct MayaEdge
{
	int vertA;
	int vertB;
};

struct MayaFace
{
	std::vector<int> edgeIndices;
	std::vector<int> uvIndices;
};

struct MayaRawMesh
{
	std::string name;
	std::vector<Vector3> positions;
	std::vector<MayaEdge> edges;
	std::vector<MayaFace> faces;
	std::vector<Vector3> normals;
	std::vector<Vector2> uvs;
};

bool startsWith( const std::string& s, const char* prefix ){
	return s.compare( 0, strlen( prefix ), prefix ) == 0;
}

std::string trim( const std::string& s ){
	auto start = s.find_first_not_of( " \t\r\n" );
	if ( start == std::string::npos )
		return {};
	auto end = s.find_last_not_of( " \t\r\n" );
	return s.substr( start, end - start + 1 );
}

std::string readFullStatement( std::istringstream& stream ){
	std::string result;
	std::string line;
	while ( std::getline( stream, line ) ) {
		auto trimmed = trim( line );
		if ( trimmed.empty() || trimmed[0] == '/' )
			continue;
		result += ' ';
		result += trimmed;
		if ( !result.empty() && result.back() == ';' ) {
			result.pop_back();
			break;
		}
	}
	return result;
}

std::string extractArg( const std::string& statement, const char* flag ){
	auto pos = statement.find( flag );
	if ( pos == std::string::npos )
		return {};
	pos += strlen( flag );
	while ( pos < statement.size() && statement[pos] == ' ' )
		++pos;
	if ( pos >= statement.size() || statement[pos] != '"' )
		return {};
	auto end = statement.find( '"', pos + 1 );
	if ( end == std::string::npos )
		return {};
	return statement.substr( pos + 1, end - pos - 1 );
}

bool containsAttr( const std::string& statement, const char* attrSuffix ){
	auto pos = statement.find( attrSuffix );
	return pos != std::string::npos;
}

void parseFloats( const std::string& data, std::size_t startPos, std::vector<float>& out ){
	const char* p = data.c_str() + startPos;
	char* end;
	while ( *p ) {
		while ( *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' )
			++p;
		if ( *p == '\0' )
			break;
		float val = strtof( p, &end );
		if ( end == p )
			break;
		out.push_back( val );
		p = end;
	}
}

void parseInts( const std::string& data, std::size_t startPos, std::vector<int>& out ){
	const char* p = data.c_str() + startPos;
	char* end;
	while ( *p ) {
		while ( *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' )
			++p;
		if ( *p == '\0' )
			break;
		long val = strtol( p, &end, 10 );
		if ( end == p )
			break;
		out.push_back( static_cast<int>( val ) );
		p = end;
	}
}

std::size_t findAttrData( const std::string& stmt, const char* attr ){
	auto pos = stmt.find( attr );
	if ( pos == std::string::npos )
		return std::string::npos;
	pos += strlen( attr );
	while ( pos < stmt.size() && stmt[pos] == ' ' )
		++pos;
	return pos;
}

void parseVertices( const std::string& stmt, MayaRawMesh& mesh ){
	auto pos = findAttrData( stmt, ".vt[" );
	if ( pos == std::string::npos )
		pos = findAttrData( stmt, ".vrts[" );
	if ( pos == std::string::npos )
		return;
	pos = stmt.find( ']', pos );
	if ( pos == std::string::npos )
		return;
	++pos;

	std::vector<float> floats;
	parseFloats( stmt, pos, floats );
	for ( std::size_t i = 0; i + 2 < floats.size(); i += 3 ) {
		mesh.positions.push_back( Vector3( floats[i], floats[i + 1], floats[i + 2] ) );
	}
}

void parseEdges( const std::string& stmt, MayaRawMesh& mesh ){
	auto pos = findAttrData( stmt, ".ed[" );
	if ( pos == std::string::npos )
		return;
	pos = stmt.find( ']', pos );
	if ( pos == std::string::npos )
		return;
	++pos;

	std::vector<int> ints;
	parseInts( stmt, pos, ints );
	for ( std::size_t i = 0; i + 2 < ints.size(); i += 3 ) {
		mesh.edges.push_back( MayaEdge{ ints[i], ints[i + 1] } );
	}
}

void parseFaces( const std::string& stmt, MayaRawMesh& mesh ){
	const char* p = stmt.c_str();

	while ( ( p = strstr( p, " f " ) ) != nullptr ) {
		p += 3;
		char* end;
		int numVerts = static_cast<int>( strtol( p, &end, 10 ) );
		if ( end == p || numVerts < 3 )
			break;
		p = end;

		MayaFace face;
		for ( int i = 0; i < numVerts; ++i ) {
			long edgeIdx = strtol( p, &end, 10 );
			if ( end == p )
				break;
			face.edgeIndices.push_back( static_cast<int>( edgeIdx ) );
			p = end;
		}

		while ( *p == ' ' || *p == '\t' )
			++p;
		if ( strncmp( p, "mu", 2 ) == 0 ) {
			p += 2;
			strtol( p, &end, 10 );
			p = end;
			int uvCount = static_cast<int>( strtol( p, &end, 10 ) );
			p = end;
			for ( int i = 0; i < uvCount; ++i ) {
				long uvIdx = strtol( p, &end, 10 );
				if ( end == p )
					break;
				face.uvIndices.push_back( static_cast<int>( uvIdx ) );
				p = end;
			}
		}

		if ( static_cast<int>( face.edgeIndices.size() ) == numVerts ) {
			mesh.faces.push_back( std::move( face ) );
		}
	}
}

void parseNormals( const std::string& stmt, MayaRawMesh& mesh ){
	auto pos = findAttrData( stmt, ".n[" );
	if ( pos == std::string::npos )
		return;
	pos = stmt.find( ']', pos );
	if ( pos == std::string::npos )
		return;
	++pos;

	auto typePos = stmt.find( "\"float3\"", pos );
	if ( typePos != std::string::npos )
		pos = typePos + 8;

	std::vector<float> floats;
	parseFloats( stmt, pos, floats );
	for ( std::size_t i = 0; i + 2 < floats.size(); i += 3 ) {
		mesh.normals.push_back( Vector3( floats[i], floats[i + 1], floats[i + 2] ) );
	}
}

void parseUVs( const std::string& stmt, MayaRawMesh& mesh ){
	auto pos = findAttrData( stmt, ".uvsp[" );
	if ( pos == std::string::npos )
		pos = findAttrData( stmt, ".uvsp" );
	if ( pos == std::string::npos )
		return;
	if ( auto bracket = stmt.find( ']', pos ); bracket != std::string::npos )
		pos = bracket + 1;

	auto typePos = stmt.find( "\"float2\"", pos );
	if ( typePos != std::string::npos )
		pos = typePos + 8;

	std::vector<float> floats;
	parseFloats( stmt, pos, floats );
	for ( std::size_t i = 0; i + 1 < floats.size(); i += 2 ) {
		mesh.uvs.push_back( Vector2( floats[i], floats[i + 1] ) );
	}
}

int resolveEdgeVertex( const MayaRawMesh& mesh, int signedEdge ){
	if ( signedEdge >= 0 ) {
		if ( static_cast<std::size_t>( signedEdge ) < mesh.edges.size() )
			return mesh.edges[signedEdge].vertA;
	}
	else {
		int edgeIdx = -signedEdge - 1;
		if ( static_cast<std::size_t>( edgeIdx ) < mesh.edges.size() )
			return mesh.edges[edgeIdx].vertB;
	}
	return 0;
}

bool convertMesh( const MayaRawMesh& raw, MayaMesh& out ){
	if ( raw.positions.empty() || raw.faces.empty() ) {
		return false;
	}

	out.name = raw.name;
	out.shader = raw.name;

	std::vector<ArbitraryMeshVertex> verts;
	std::vector<RenderIndex> indices;

	int normalIdx = 0;
	const bool hasNormals = !raw.normals.empty();
	const bool hasUVs = !raw.uvs.empty();

	for ( const auto& face : raw.faces ) {
		std::vector<int> faceVerts;
		for ( int signedEdge : face.edgeIndices ) {
			faceVerts.push_back( resolveEdgeVertex( raw, signedEdge ) );
		}

		std::vector<RenderIndex> faceVertIndices;
		for ( std::size_t i = 0; i < faceVerts.size(); ++i ) {
			ArbitraryMeshVertex v;
			int vi = faceVerts[i];
			if ( static_cast<std::size_t>( vi ) < raw.positions.size() ) {
				const Vector3& p = raw.positions[vi];
				v.vertex = { static_cast<float>( p.x() ), static_cast<float>( p.y() ), static_cast<float>( p.z() ) };
			}

			if ( hasNormals && static_cast<std::size_t>( normalIdx ) < raw.normals.size() ) {
				const Vector3& n = raw.normals[normalIdx];
				v.normal = { static_cast<float>( n.x() ), static_cast<float>( n.y() ), static_cast<float>( n.z() ) };
				++normalIdx;
			}

			if ( hasUVs && i < face.uvIndices.size() ) {
				int uvIdx = face.uvIndices[i];
				if ( static_cast<std::size_t>( uvIdx ) < raw.uvs.size() ) {
					const Vector2& uv = raw.uvs[uvIdx];
					v.texcoord = { static_cast<float>( uv.x() ), static_cast<float>( uv.y() ) };
				}
			}

			faceVertIndices.push_back( static_cast<RenderIndex>( verts.size() ) );
			verts.push_back( v );
		}

		for ( std::size_t i = 2; i < faceVertIndices.size(); ++i ) {
			indices.push_back( faceVertIndices[0] );
			indices.push_back( faceVertIndices[i - 1] );
			indices.push_back( faceVertIndices[i] );
		}
	}

	if ( verts.empty() || indices.empty() )
		return false;

	if ( !hasNormals ) {
		for ( std::size_t i = 0; i + 2 < indices.size(); i += 3 ) {
			const Vector3 a = vertex3f_to_vector3( verts[indices[i]].vertex );
			const Vector3 b = vertex3f_to_vector3( verts[indices[i + 1]].vertex );
			const Vector3 c = vertex3f_to_vector3( verts[indices[i + 2]].vertex );
			Vector3 normal = vector3_cross( c - a, b - a );
			if ( vector3_length( normal ) > 1e-6f )
				vector3_normalise( normal );
			Normal3f n = normal3f_for_vector3( normal );
			verts[indices[i]].normal = n;
			verts[indices[i + 1]].normal = n;
			verts[indices[i + 2]].normal = n;
		}
	}

	// Y-up to Z-up rotation (Maya uses Y-up, Quake uses Z-up)
	for ( auto& v : verts ) {
		float oy = v.vertex.y();
		float oz = v.vertex.z();
		v.vertex = { v.vertex.x(), -oz, oy };
		oy = v.normal.y();
		oz = v.normal.z();
		v.normal = { v.normal.x(), -oz, oy };
	}

	out.vertices = std::move( verts );
	out.indices = std::move( indices );
	return true;
}

}


bool parseMayaAscii( const char* buffer, std::size_t length, std::vector<MayaMesh>& outMeshes ){
	std::string content( buffer, length );
	std::istringstream stream( content );

	std::vector<MayaRawMesh> rawMeshes;
	MayaRawMesh* currentMesh = nullptr;

	std::string line;
	while ( std::getline( stream, line ) ) {
		auto trimmed = trim( line );
		if ( trimmed.empty() || trimmed[0] == '/' )
			continue;

		if ( startsWith( trimmed, "createNode mesh" ) || startsWith( trimmed, "createNode transform" ) ) {
			if ( startsWith( trimmed, "createNode mesh" ) ) {
				rawMeshes.emplace_back();
				currentMesh = &rawMeshes.back();
				currentMesh->name = extractArg( trimmed, "-n " );
				if ( currentMesh->name.empty() )
					currentMesh->name = "mesh";
			}
			else {
				currentMesh = nullptr;
			}
			continue;
		}

		if ( startsWith( trimmed, "createNode" ) ) {
			currentMesh = nullptr;
			continue;
		}

		if ( currentMesh == nullptr )
			continue;

		if ( !startsWith( trimmed, "setAttr" ) )
			continue;

		std::string stmt = trimmed;
		if ( !stmt.empty() && stmt.back() != ';' ) {
			while ( std::getline( stream, line ) ) {
				auto t = trim( line );
				stmt += ' ';
				stmt += t;
				if ( !t.empty() && t.back() == ';' ) {
					stmt.pop_back();
					break;
				}
			}
		}
		else if ( stmt.back() == ';' ) {
			stmt.pop_back();
		}

		if ( containsAttr( stmt, ".vt[" ) || containsAttr( stmt, ".vrts[" ) ) {
			parseVertices( stmt, *currentMesh );
		}
		else if ( containsAttr( stmt, ".ed[" ) ) {
			parseEdges( stmt, *currentMesh );
		}
		else if ( containsAttr( stmt, ".fc[" ) && stmt.find( "polyFaces" ) != std::string::npos ) {
			parseFaces( stmt, *currentMesh );
		}
		else if ( containsAttr( stmt, ".n[" ) && stmt.find( "float3" ) != std::string::npos ) {
			parseNormals( stmt, *currentMesh );
		}
		else if ( containsAttr( stmt, ".uvsp" ) && stmt.find( "float2" ) != std::string::npos ) {
			parseUVs( stmt, *currentMesh );
		}
	}

	for ( const auto& raw : rawMeshes ) {
		MayaMesh mesh;
		if ( convertMesh( raw, mesh ) ) {
			outMeshes.push_back( std::move( mesh ) );
		}
	}

	if ( outMeshes.empty() ) {
		globalWarningStream() << "Maya ASCII: no valid meshes found\n";
		return false;
	}

	globalOutputStream() << "Maya ASCII: loaded " << outMeshes.size() << " mesh(es)\n";
	return true;
}
