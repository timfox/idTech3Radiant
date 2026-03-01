#pragma once

#include <vector>
#include <string>
#include "math/vector.h"
#include "render.h"

struct MayaMesh
{
	std::string name;
	std::string shader;
	std::vector<ArbitraryMeshVertex> vertices;
	std::vector<RenderIndex> indices;
};

bool parseMayaAscii( const char* buffer, std::size_t length, std::vector<MayaMesh>& outMeshes );
