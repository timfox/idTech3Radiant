/* -------------------------------------------------------------------------------

   Copyright (C) 1999-2007 id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

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

   ----------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */



/* dependencies */
#include "q3map2.h"
#include "math/pi.h"
#include "png.h"
#include "ddslib.h"
#include "crnlib/crnlib.h"
#include "webplib/webplib.h"



/* -------------------------------------------------------------------------------

   this file contains image pool management with reference counting. note: it isn't
   reentrant, so only call it from init/shutdown code or wrap calls in a mutex

   ------------------------------------------------------------------------------- */

/*
   LoadDDSBuffer()
   loads a dxtc (1, 3, 5) dds buffer into a valid rgba image
 */

static void LoadDDSBuffer( byte *buffer, int size, byte **pixels, int *width, int *height ){
	int w, h;
	ddsPF_t pf;


	/* dummy check */
	if ( buffer == nullptr || size <= 0 || pixels == nullptr || width == nullptr || height == nullptr ) {
		return;
	}

	/* null out */
	*pixels = 0;
	*width = 0;
	*height = 0;

	/* get dds info */
	if ( DDSGetInfo( (ddsBuffer_t*) buffer, &w, &h, &pf ) ) {
		Sys_Warning( "Invalid DDS texture\n" );
		return;
	}

	/* only certain types of dds textures are supported */
	if ( pf != DDS_PF_ARGB8888 && pf != DDS_PF_DXT1 && pf != DDS_PF_DXT3 && pf != DDS_PF_DXT5 ) {
		Sys_Warning( "Only DDS texture formats ARGB8888, DXT1, DXT3, and DXT5 are supported (%d)\n", pf );
		return;
	}

	/* create image pixel buffer */
	*width = w;
	*height = h;
	*pixels = safe_malloc( w * h * 4 );

	/* decompress the dds texture */
	DDSDecompress( (ddsBuffer_t*) buffer, *pixels );
}


/*
    LoadCRNBuffer
    loads a crn image into a valid rgba image
*/
void LoadCRNBuffer( byte *buffer, int size, byte **pixels, int *width, int *height ) {
	/* dummy check */
	if ( buffer == nullptr || size <= 0 || pixels == nullptr || width == nullptr || height == nullptr ) {
		return;
	}
	if ( !GetCRNImageSize( buffer, size, width, height ) ) {
		Sys_Warning( "Error getting crn image dimensions.\n" );
		return;
	}
	const unsigned int outBufSize = *width * *height * 4;
	*pixels = safe_malloc( outBufSize );
	if ( !ConvertCRNtoRGBA( buffer, size, outBufSize, *pixels ) ) {
		Sys_Warning( "Error decoding crn image.\n" );
	}
}



/*
   PNGReadData()
   callback function for libpng to read from a memory buffer
   note: this function is a total hack, as it reads/writes the png struct directly!
 */

struct pngBuffer_t
{
	byte    *buffer;
	png_size_t size, offset;
};

static void PNGReadData( png_struct *png, png_byte *buffer, png_size_t size ){
	pngBuffer_t     *pb = (pngBuffer_t*) png_get_io_ptr( png );


	if ( ( pb->offset + size ) > pb->size ) {
		size = ( pb->size - pb->offset );
	}
	memcpy( buffer, &pb->buffer[ pb->offset ], size );
	pb->offset += size;
	//%	Sys_Printf( "Copying %d bytes from 0x%08X to 0x%08X (offset: %d of %d)\n", size, &pb->buffer[ pb->offset ], buffer, pb->offset, pb->size );
}



/*
   LoadPNGBuffer()
   loads a png file buffer into a valid rgba image
 */

static void LoadPNGBuffer( byte *buffer, int size, byte **pixels, int *width, int *height ){
	png_struct  *png;
	png_info    *info, *end;
	pngBuffer_t pb;
	//pngBuffer_t     *pb = (pngBuffer_t*) png_get_io_ptr( png );
	int bitDepth, colorType;
	png_uint_32 w, h, i;
	byte        **rowPointers;

	/* dummy check */
	if ( buffer == nullptr || size <= 0 || pixels == nullptr || width == nullptr || height == nullptr ) {
		return;
	}

	/* null out */
	*pixels = 0;
	*width = 0;
	*height = 0;

	/* determine if this is a png file */
	if ( png_sig_cmp( buffer, 0, 8 ) != 0 ) {
		Sys_Warning( "Invalid PNG file\n" );
		return;
	}

	/* create png structs */
	png = png_create_read_struct( PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr );
	if ( png == nullptr ) {
		Sys_Warning( "Unable to create PNG read struct\n" );
		return;
	}

	info = png_create_info_struct( png );
	if ( info == nullptr ) {
		Sys_Warning( "Unable to create PNG info struct\n" );
		png_destroy_read_struct( &png, nullptr, nullptr );
		return;
	}

	end = png_create_info_struct( png );
	if ( end == nullptr ) {
		Sys_Warning( "Unable to create PNG end info struct\n" );
		png_destroy_read_struct( &png, &info, nullptr );
		return;
	}

	/* set read callback */
	pb.buffer = buffer;
	pb.size = size;
	pb.offset = 0;
	png_set_read_fn( png, &pb, PNGReadData );
	//png->io_ptr = &pb; /* hack! */

	/* set error longjmp */
	if ( setjmp( png_jmpbuf( png ) ) ) {
		Sys_Warning( "An error occurred reading PNG image\n" );
		png_destroy_read_struct( &png, &info, &end );
		return;
	}

	/* fixme: add proper i/o stuff here */

	/* read png info */
	png_read_info( png, info );

	/* read image header chunk */
	png_get_IHDR( png, info,
	              &w, &h, &bitDepth, &colorType, nullptr, nullptr, nullptr );

	/* the following will probably bork on certain types of png images, but hey... */

	/* force indexed/gray/trans chunk to rgb */
	if ( ( colorType == PNG_COLOR_TYPE_PALETTE && bitDepth <= 8 ) ||
	     ( colorType == PNG_COLOR_TYPE_GRAY && bitDepth <= 8 ) ||
	     png_get_valid( png, info, PNG_INFO_tRNS ) ) {
		png_set_expand( png );
	}

	/* strip 16bpc -> 8bpc */
	if ( bitDepth == 16 ) {
		png_set_strip_16( png );
	}

	/* pad rgb to rgba */
	if ( bitDepth == 8 && colorType == PNG_COLOR_TYPE_RGB ) {
		png_set_filler( png, 255, PNG_FILLER_AFTER );
	}

	/* create image pixel buffer */
	*width = w;
	*height = h;
	*pixels = safe_malloc( w * h * 4 );

	/* create row pointers */
	rowPointers = safe_malloc( h * sizeof( byte* ) );
	for ( i = 0; i < h; ++i )
		rowPointers[ i ] = *pixels + ( i * w * 4 );

	/* read the png */
	png_read_image( png, rowPointers );

	/* clean up */
	free( rowPointers );
	png_destroy_read_struct( &png, &info, &end );
}



static std::forward_list<image_t> images;

static struct construct_default_image
{
	construct_default_image(){
		images.emplace_front( DEFAULT_IMAGE, DEFAULT_IMAGE, 64, 64, void_ptr( memset( safe_malloc( 64 * 64 * 4 ), 255, 64 * 64 * 4 ) ) );
	}
} s_construct_default_image;

/*
   ImageFind()
   finds an existing rgba image and returns a pointer to the image_t struct or NULL if not found
   name is name without extension, as in images[ i ].name
 */

static const image_t *ImageFind( const char *name ){
	/* dummy check */
	if ( strEmptyOrNull( name ) ) {
		return nullptr;
	}

	/* search list */
	for ( const auto& img : images )
	{
		if ( striEqual( name, img.name.c_str() ) ) {
			return &img;
		}
	}

	/* no matching image found */
	return nullptr;
}



/*
   ImageLoad()
   loads an rgba image and returns a pointer to the image_t struct or NULL if not found
   expects extensionless path as input
 */

const image_t *ImageLoad( const char *name ){
	/* dummy check */
	if ( strEmptyOrNull( name ) ) {
		return nullptr;
	}

	/* try to find existing image */
	if ( const auto *img = ImageFind( name ) ) {
		return img;
	}

	/* none found, so let's create a new one */
	byte *pixels = nullptr;
	int width, height;
	char filename[ 1024 ];
	MemBuffer buffer;
	bool alphaHack = false;

	/* attempt to load various formats */
	if ( sprintf( filename, "%s.tga", name ); ( buffer = vfsLoadFile( filename ) ) ) // StripExtension( name ); already
	{
		LoadTGABuffer( buffer.data(), buffer.size(), &pixels, &width, &height );
	}
	else if( path_set_extension( filename, ".png" ); ( buffer = vfsLoadFile( filename ) ) )
	{
		LoadPNGBuffer( buffer.data(), buffer.size(), &pixels, &width, &height );
	}
	else if( path_set_extension( filename, ".jpg" ); ( buffer = vfsLoadFile( filename ) ) )
	{
		if ( LoadJPGBuff( buffer.data(), buffer.size(), &pixels, &width, &height ) == -1 && pixels != nullptr ) {
			// On error, LoadJPGBuff might store a pointer to the error message in pixels
			Sys_Warning( "LoadJPGBuff %s %s\n", filename, (unsigned char*) pixels );
			pixels = nullptr;
		}
		alphaHack = true;
	}
	else if( path_set_extension( filename, ".dds" ); ( buffer = vfsLoadFile( filename ) ) )
	{
		LoadDDSBuffer( buffer.data(), buffer.size(), &pixels, &width, &height );
		/* debug code */
		#if 0
		{
			ddsPF_t pf;
			DDSGetInfo( (ddsBuffer_t*) buffer, nullptr, nullptr, &pf );
			Sys_Printf( "pf = %d\n", pf );
			if ( width > 0 ) {
				path_set_extension( filename, "_converted.tga" );
				WriteTGA( "C:\\games\\quake3\\baseq3\\textures\\rad\\dds_converted.tga", pixels, width, height );
			}
		}
		#endif
	}
	else if( path_set_extension( filename, ".ktx" ); ( buffer = vfsLoadFile( filename ) ) )
	{
		LoadKTXBufferFirstImage( buffer.data(), buffer.size(), &pixels, &width, &height );
	}
	else if( path_set_extension( filename, ".crn" ); ( buffer = vfsLoadFile( filename ) ) )
	{
		LoadCRNBuffer( buffer.data(), buffer.size(), &pixels, &width, &height );
	}
	else if( path_set_extension( filename, ".webp" ); ( buffer = vfsLoadFile( filename ) ) )
	{
		pixels = ConvertWebptoRGBA( buffer.data(), buffer.size(), width, height );
	}

	/* make sure everything's kosher */
	if ( !buffer || width <= 0 || height <= 0 || pixels == nullptr ) {
		//%	Sys_Printf( "size = %zu  width = %d  height = %d  pixels = 0x%08x (%s)\n",
		//%		buffer.size(), width, height, pixels, filename );
		return nullptr;
	}

	/* everybody's in the place, create new image */
	image_t& image = *images.emplace_after( images.cbegin(), name, filename, width, height, pixels );

	if ( alphaHack ) {
		if ( path_set_extension( filename, "_alpha.jpg" ); ( buffer = vfsLoadFile( filename ) ) ) {
			if ( LoadJPGBuff( buffer.data(), buffer.size(), &pixels, &width, &height ) == -1 ) {
				if ( pixels ) {
					// On error, LoadJPGBuff might store a pointer to the error message in pixels
					Sys_Warning( "LoadJPGBuff %s %s\n", filename, (unsigned char*) pixels );
				}
			} else {
				if ( width == image.width && height == image.height ) {
					for ( int i = 0; i < width * height; ++i )
						image.pixels[4 * i + 3] = pixels[4 * i + 2];  // copy alpha from blue channel
				}
				free( pixels );
			}
		}
	}

	/* return the image */
	return &image;
}


/*
   EquirectangularToCubeFace()
   samples equirectangular image to produce one cube face
   face order: _lf(-X), _rt(+X), _ft(+Y), _bk(-Y), _up(+Z), _dn(-Z)
 */
static void EquirectangularToCubeFace( const byte *equirectPixels, int eqWidth, int eqHeight,
                                       int faceSize, byte *facePixels, int faceIndex ){
	/* direction from (u,v) on cube face to world direction */
	const auto dirFromFace = []( float u, float v, int face ) -> Vector3 {
		const float fu = 2.f * u - 1.f;
		const float fv = 2.f * v - 1.f;
		switch ( face ) {
		case 0: return Vector3( -1, fu, 1 - fv );   /* _lf -X */
		case 1: return Vector3( 1, 1 - fu, 1 - fv ); /* _rt +X */
		case 2: return Vector3( fu, 1, 1 - fv );    /* _ft +Y */
		case 3: return Vector3( 1 - fu, -1, 1 - fv ); /* _bk -Y */
		case 4: return Vector3( 1 - fu, 1 - fv, 1 ); /* _up +Z */
		case 5: return Vector3( fu, 1 - fv, -1 );   /* _dn -Z */
		default: return Vector3( 0, 0, 1 );
		}
	};

	for ( int v = 0; v < faceSize; ++v ) {
		for ( int u = 0; u < faceSize; ++u ) {
			const Vector3 dir = vector3_normalised( dirFromFace( ( u + 0.5f ) / faceSize, ( v + 0.5f ) / faceSize, faceIndex ) );
			/* direction to equirectangular UV */
			const float lon = atan2( dir.y(), dir.x() );
			const float lat = asin( std::max( -1.f, std::min( 1.f, dir.z() ) ) );
			const float eqU = ( float )( lon * ( 1.0 / ( 2.0 * c_pi ) ) + 0.5 ) * eqWidth;
			const float eqV = ( float )( 0.5 - lat / c_pi ) * eqHeight;
			const int x0 = std::max( 0, std::min( eqWidth - 1, (int)eqU ) );
			const int y0 = std::max( 0, std::min( eqHeight - 1, (int)eqV ) );
			const int x1 = std::min( eqWidth - 1, x0 + 1 );
			const int y1 = std::min( eqHeight - 1, y0 + 1 );
			const float fx = eqU - x0;
			const float fy = eqV - y0;
			/* bilinear sample */
			Vector3 color( 0 );
			for ( int dy = 0; dy <= 1; ++dy ) {
				for ( int dx = 0; dx <= 1; ++dx ) {
					const int x = dx ? x1 : x0;
					const int y = dy ? y1 : y0;
					const float w = ( dx ? fx : ( 1 - fx ) ) * ( dy ? fy : ( 1 - fy ) );
					const byte *p = equirectPixels + 4 * ( y * eqWidth + x );
					color += Vector3( p[0], p[1], p[2] ) * w;
				}
			}
			byte *out = facePixels + 4 * ( v * faceSize + u );
			out[0] = (byte)std::max( 0, std::min( 255, (int)color.x() ) );
			out[1] = (byte)std::max( 0, std::min( 255, (int)color.y() ) );
			out[2] = (byte)std::max( 0, std::min( 255, (int)color.z() ) );
			out[3] = 255;
		}
	}
}

static constexpr int SKYBOX_FACE_SIZE = 512;
static constexpr const char *SKYBOX_SUFFIXES[] = { "_lf", "_rt", "_ft", "_bk", "_up", "_dn" };

/* returns true if equirectangular (roughly 2:1 aspect) */
static bool ImageIsEquirectangular( int width, int height ){
	return width > 0 && height > 0 && width >= height && ( width * 3 ) <= ( height * 8 );
}

const image_t *const *ImageLoadSkyboxFaces( const char *basePath ){
	static std::array<const image_t *, 6> s_faces;
	static std::array<image_t, 6> s_faceImages;

	/* try loading 6 cube faces first */
	bool allLoaded = true;
	for ( int i = 0; i < 6; ++i ) {
		if ( nullptr == ( s_faces[i] = ImageLoad( StringStream<64>( basePath, SKYBOX_SUFFIXES[i] ) ) ) ) {
			allLoaded = false;
			break;
		}
	}
	if ( allLoaded ) {
		return s_faces.data();
	}

	/* try equirectangular image */
	const image_t *equirect = ImageLoad( basePath );
	if ( equirect == nullptr || !ImageIsEquirectangular( equirect->width, equirect->height ) ) {
		return nullptr;
	}

	Sys_Printf( "Converting equirectangular skybox %s to cube faces (%dx%d)\n", basePath, equirect->width, equirect->height );

	const char *writeDir = vfsGetWriteDir();
	const bool canWrite = writeDir[0] != '\0';

	for ( int i = 0; i < 6; ++i ) {
		byte *pixels = (byte*)safe_malloc( SKYBOX_FACE_SIZE * SKYBOX_FACE_SIZE * 4 );
		EquirectangularToCubeFace( equirect->pixels, equirect->width, equirect->height, SKYBOX_FACE_SIZE, pixels, i );
		CopiedString faceName( StringStream<64>( basePath, SKYBOX_SUFFIXES[i] ) );
		s_faceImages[i] = image_t( faceName.c_str(), StringStream<64>( faceName, ".tga" ).c_str(), SKYBOX_FACE_SIZE, SKYBOX_FACE_SIZE, pixels );
		s_faces[i] = &s_faceImages[i];

		if ( canWrite ) {
			const CopiedString outPath( StringStream( writeDir, faceName, ".tga" ) );
			const CopiedString outDir( PathFilenameless( outPath.c_str() ) );
			Q_mkdir( outDir.c_str() );
			WriteTGA( outPath.c_str(), pixels, SKYBOX_FACE_SIZE, SKYBOX_FACE_SIZE );
			Sys_FPrintf( SYS_VRB, "Wrote %s\n", outPath.c_str() );
		}
	}

	return s_faces.data();
}
