/*
   Copyright (C) 2026, NetRadiant contributors.
   All Rights Reserved.

   This file is part of NetRadiant.

   NetRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   NetRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with NetRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "hdr.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <zlib.h>

#include "iarchive.h"
#include "imagelib.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "assimp/contrib/stb/stb_image.h"

#define TINYEXR_USE_MINIZ 0
#define TINYEXR_IMPLEMENTATION
#include "tinyexr/tinyexr.h"

namespace
{
inline unsigned char linearToDisplayByte( float x ){
	if ( !std::isfinite( x ) || x <= 0.0f ) {
		return 0;
	}
	// Simple Reinhard tonemap + gamma transform for editor preview.
	x = x / ( 1.0f + x );
	x = std::pow( std::clamp( x, 0.0f, 1.0f ), 1.0f / 2.2f );
	return static_cast<unsigned char>( std::clamp( x * 255.0f + 0.5f, 0.0f, 255.0f ) );
}

inline unsigned char alphaToByte( float x ){
	if ( !std::isfinite( x ) ) {
		return 255;
	}
	return static_cast<unsigned char>( std::clamp( x, 0.0f, 1.0f ) * 255.0f + 0.5f );
}

Image* floatRGBA32ToImage( const float* rgba, int width, int height ){
	if ( rgba == nullptr || width <= 0 || height <= 0 ) {
		return nullptr;
	}

	auto* image = new RGBAImage( static_cast<unsigned int>( width ), static_cast<unsigned int>( height ) );
	auto* out = image->getRGBAPixels();

	const std::size_t pixelsCount = static_cast<std::size_t>( width ) * static_cast<std::size_t>( height );
	for ( std::size_t i = 0; i < pixelsCount; ++i )
	{
		out[i * 4 + 0] = linearToDisplayByte( rgba[i * 4 + 0] );
		out[i * 4 + 1] = linearToDisplayByte( rgba[i * 4 + 1] );
		out[i * 4 + 2] = linearToDisplayByte( rgba[i * 4 + 2] );
		out[i * 4 + 3] = alphaToByte( rgba[i * 4 + 3] );
	}

	return image;
}
}

Image* LoadHDR( ArchiveFile& file ){
	ScopedArchiveBuffer buffer( file );
	if ( buffer.buffer == nullptr || buffer.length == 0
	     || buffer.length > static_cast<std::size_t>( std::numeric_limits<int>::max() ) ) {
		return nullptr;
	}

	int width = 0;
	int height = 0;
	int channels = 0;
	float* pixels = stbi_loadf_from_memory( buffer.buffer, static_cast<int>( buffer.length ), &width, &height, &channels, 4 );
	if ( pixels == nullptr ) {
		return nullptr;
	}

	Image* image = floatRGBA32ToImage( pixels, width, height );
	stbi_image_free( pixels );
	return image;
}

Image* LoadEXR( ArchiveFile& file ){
	ScopedArchiveBuffer buffer( file );
	if ( buffer.buffer == nullptr || buffer.length == 0 ) {
		return nullptr;
	}

	float* pixels = nullptr;
	int width = 0;
	int height = 0;
	const char* err = nullptr;
	const int status = LoadEXRFromMemory( &pixels, &width, &height, buffer.buffer, buffer.length, &err );
	if ( status != TINYEXR_SUCCESS ) {
		if ( err != nullptr ) {
			FreeEXRErrorMessage( err );
		}
		return nullptr;
	}

	Image* image = floatRGBA32ToImage( pixels, width, height );
	free( pixels );
	return image;
}
