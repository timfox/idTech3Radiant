#include "float_image_io.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <zlib.h>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "assimp/contrib/stb/stb_image.h"

#define TINYEXR_USE_MINIZ 0
#define TINYEXR_IMPLEMENTATION
#include "tinyexr/tinyexr.h"

namespace
{
inline unsigned char FloatToUNorm8( float value ){
	if ( !std::isfinite( value ) || value <= 0.0f ) {
		return 0;
	}

	if ( value >= 1.0f ) {
		return 255;
	}

	return static_cast<unsigned char>( value * 255.0f + 0.5f );
}

void FloatRGBToRGBE( float r, float g, float b, unsigned char rgbe[ 4 ] ){
	const float maxc = std::max( std::max( r, g ), b );
	if ( maxc < 1.0e-32f ) {
		rgbe[ 0 ] = 0;
		rgbe[ 1 ] = 0;
		rgbe[ 2 ] = 0;
		rgbe[ 3 ] = 0;
		return;
	}

	int exponent = 0;
	const float mantissa = std::frexp( maxc, &exponent );
	const float scale = mantissa * 256.0f / maxc;
	rgbe[ 0 ] = static_cast<unsigned char>( std::clamp( r * scale, 0.0f, 255.0f ) );
	rgbe[ 1 ] = static_cast<unsigned char>( std::clamp( g * scale, 0.0f, 255.0f ) );
	rgbe[ 2 ] = static_cast<unsigned char>( std::clamp( b * scale, 0.0f, 255.0f ) );
	rgbe[ 3 ] = static_cast<unsigned char>( exponent + 128 );
}

int LoadFloatBufferToBytes( const unsigned char *buffer, int size, int requestedComponents, unsigned char **pixels, int *width, int *height ){
	int w = 0;
	int h = 0;
	int components = 0;
	float *floatPixels = stbi_loadf_from_memory( buffer, size, &w, &h, &components, requestedComponents );
	if ( floatPixels == nullptr ) {
		return 0;
	}

	const std::size_t count = static_cast<std::size_t>( w ) * static_cast<std::size_t>( h ) * static_cast<std::size_t>( requestedComponents );
	unsigned char *out = static_cast<unsigned char*>( std::malloc( count ) );
	if ( out == nullptr ) {
		stbi_image_free( floatPixels );
		return 0;
	}

	for ( std::size_t i = 0; i < count; ++i )
	{
		out[ i ] = FloatToUNorm8( floatPixels[ i ] );
	}

	stbi_image_free( floatPixels );
	*pixels = out;
	*width = w;
	*height = h;
	return 1;
}

int LoadEXRBufferToBytes( const unsigned char *buffer, int size, int requestedComponents, unsigned char **pixels, int *width, int *height ){
	float *floatPixels = nullptr;
	int w = 0;
	int h = 0;
	const char *err = nullptr;
	const int status = LoadEXRFromMemory( &floatPixels, &w, &h, buffer, static_cast<size_t>( size ), &err );
	if ( status != TINYEXR_SUCCESS ) {
		if ( err != nullptr ) {
			FreeEXRErrorMessage( err );
		}
		return 0;
	}

	const std::size_t count = static_cast<std::size_t>( w ) * static_cast<std::size_t>( h ) * static_cast<std::size_t>( requestedComponents );
	unsigned char *out = static_cast<unsigned char*>( std::malloc( count ) );
	if ( out == nullptr ) {
		std::free( floatPixels );
		return 0;
	}

	for ( int i = 0; i < ( w * h ); ++i )
	{
		const std::size_t src = static_cast<std::size_t>( i ) * 4u;
		const std::size_t dst = static_cast<std::size_t>( i ) * static_cast<std::size_t>( requestedComponents );
		out[ dst + 0 ] = FloatToUNorm8( floatPixels[ src + 0 ] );
		out[ dst + 1 ] = FloatToUNorm8( floatPixels[ src + 1 ] );
		out[ dst + 2 ] = FloatToUNorm8( floatPixels[ src + 2 ] );
		if ( requestedComponents == 4 ) {
			out[ dst + 3 ] = FloatToUNorm8( floatPixels[ src + 3 ] );
		}
	}

	std::free( floatPixels );
	*pixels = out;
	*width = w;
	*height = h;
	return 1;
}
}

extern "C"
{
int MapBake3_LoadHDRBufferToRGBA8( const unsigned char *buffer, int size, unsigned char **pixels, int *width, int *height ){
	if ( buffer == nullptr || size <= 0 || pixels == nullptr || width == nullptr || height == nullptr ) {
		return 0;
	}

	return LoadFloatBufferToBytes( buffer, size, 4, pixels, width, height );
}

int MapBake3_LoadEXRBufferToRGBA8( const unsigned char *buffer, int size, unsigned char **pixels, int *width, int *height ){
	if ( buffer == nullptr || size <= 0 || pixels == nullptr || width == nullptr || height == nullptr ) {
		return 0;
	}

	return LoadEXRBufferToBytes( buffer, size, 4, pixels, width, height );
}

int MapBake3_LoadHDRBufferToRGB8( const unsigned char *buffer, int size, unsigned char **pixels, int *width, int *height ){
	if ( buffer == nullptr || size <= 0 || pixels == nullptr || width == nullptr || height == nullptr ) {
		return 0;
	}

	return LoadFloatBufferToBytes( buffer, size, 3, pixels, width, height );
}

int MapBake3_LoadEXRBufferToRGB8( const unsigned char *buffer, int size, unsigned char **pixels, int *width, int *height ){
	if ( buffer == nullptr || size <= 0 || pixels == nullptr || width == nullptr || height == nullptr ) {
		return 0;
	}

	return LoadEXRBufferToBytes( buffer, size, 3, pixels, width, height );
}

int MapBake3_WriteRGB8AsHDR32( const char *filename, const unsigned char *pixels, int width, int height, int flip ){
	if ( filename == nullptr || pixels == nullptr || width <= 0 || height <= 0 ) {
		return 0;
	}

	FILE *file = std::fopen( filename, "wb" );
	if ( file == nullptr ) {
		return 0;
	}

	std::fprintf( file, "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y %d +X %d\n", height, width );

	unsigned char rgbe[ 4 ];
	for ( int y = 0; y < height; ++y )
	{
		const int srcY = flip ? ( height - 1 - y ) : y;
		const unsigned char *row = pixels + ( static_cast<std::size_t>( srcY ) * static_cast<std::size_t>( width ) * 3u );
		for ( int x = 0; x < width; ++x )
		{
			const std::size_t src = static_cast<std::size_t>( x ) * 3u;
			const float r = static_cast<float>( row[ src + 0 ] ) / 255.0f;
			const float g = static_cast<float>( row[ src + 1 ] ) / 255.0f;
			const float b = static_cast<float>( row[ src + 2 ] ) / 255.0f;
			FloatRGBToRGBE( r, g, b, rgbe );
			std::fwrite( rgbe, 1, 4, file );
		}
	}

	std::fclose( file );
	return 1;
}

int MapBake3_WriteRGB8AsEXR32( const char *filename, const unsigned char *pixels, int width, int height, int flip ){
	if ( filename == nullptr || pixels == nullptr || width <= 0 || height <= 0 ) {
		return 0;
	}

	std::vector<float> floatPixels( static_cast<std::size_t>( width ) * static_cast<std::size_t>( height ) * 3u );
	for ( int y = 0; y < height; ++y )
	{
		const int srcY = flip ? ( height - 1 - y ) : y;
		const unsigned char *srcRow = pixels + ( static_cast<std::size_t>( srcY ) * static_cast<std::size_t>( width ) * 3u );
		float *dstRow = floatPixels.data() + ( static_cast<std::size_t>( y ) * static_cast<std::size_t>( width ) * 3u );
		for ( int x = 0; x < width; ++x )
		{
			const std::size_t idx = static_cast<std::size_t>( x ) * 3u;
			dstRow[ idx + 0 ] = static_cast<float>( srcRow[ idx + 0 ] ) / 255.0f;
			dstRow[ idx + 1 ] = static_cast<float>( srcRow[ idx + 1 ] ) / 255.0f;
			dstRow[ idx + 2 ] = static_cast<float>( srcRow[ idx + 2 ] ) / 255.0f;
		}
	}

	const char *err = nullptr;
	const int status = SaveEXR( floatPixels.data(), width, height, 3, 0, filename, &err );
	if ( status != TINYEXR_SUCCESS ) {
		if ( err != nullptr ) {
			FreeEXRErrorMessage( err );
		}
		return 0;
	}

	return 1;
}
}
