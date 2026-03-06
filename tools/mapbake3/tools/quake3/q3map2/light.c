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



/* marker */
#define LIGHT_C



/* dependencies */
#include "q3map2.h"
#include "float_image_io.h"

/*
   NOTE:
   In the mapbake3 toolchain, source files are stored in a legacy "q3map2"
   path for historical reasons. Changes in this file affect mapbake3.
 */

#define WISHGI_MAX_LINKS             8
#define WISHGI_MAX_SH_COEFFS         16
#define WISHGI_ASSOC_MAGIC           "wishgi_assoc_v1"
#define WISHGI_ASSOC_DEFAULT_SUFFIX  ".wishgi_assoc.txt"
#define WISHGI_PROBES_DEFAULT_SUFFIX ".wishgi_probes.txt"
#define WISHGI_PROBES_SH_DEFAULT_SUFFIX ".wishgi_probes_sh.txt"

typedef struct wishGIOptions_s
{
	qboolean enabled;
	int probeCount;
	int linksPerVertex;
	int refinementIterations;
	int maxSampleVerts;
	qboolean loadAssociations;
	qboolean saveAssociations;
	qboolean dumpProbeColors;
	qboolean dumpProbeSH;
	int shOrder;
	float shRegularization;
	char associationFile[ 1024 ];
	char probeDumpFile[ 1024 ];
	char probeSHDumpFile[ 1024 ];
}
wishGIOptions_t;

typedef struct wishGIVertexAssociation_s
{
	unsigned short probeIndex[ WISHGI_MAX_LINKS ];
	float weights[ WISHGI_MAX_LINKS ];
}
wishGIVertexAssociation_t;

static wishGIOptions_t wishGIOptions =
{
	qfalse, 32, 2, 8, 200000, qfalse, qfalse, qfalse, qfalse, 2, 0.001f, { 0 }, { 0 }, { 0 }
};

static wishGIVertexAssociation_t *wishGIVertexAssociations = NULL;
static vec3_t *wishGIProbePositions = NULL;
static int wishGIActualProbeCount = 0;
static int wishGIActualLinksPerVertex = 0;

typedef enum skyLightMode_e
{
	SKYLIGHT_MODE_AUTO = 0,
	SKYLIGHT_MODE_LEGACY,
	SKYLIGHT_MODE_SHADER,
	SKYLIGHT_MODE_PANORAMA,
}
skyLightMode_t;

typedef struct skyFloatImage_s
{
	int width;
	int height;
	float *pixels; /* RGBA float32 */
}
skyFloatImage_t;

static skyLightMode_t g_skyLightMode = SKYLIGHT_MODE_AUTO;
static char g_skyLightPanoramaPath[ MAX_QPATH ] = { 0 };
static char g_skyLightPanoramaConvertBase[ MAX_QPATH ] = { 0 };
static int g_skyLightPanoramaFaceSize = 512;
static char g_skyLightLastConvertedPath[ MAX_QPATH ] = { 0 };
static char g_skyLightLastConvertedBase[ MAX_QPATH ] = { 0 };
static int g_skyLightLastConvertedSize = 0;

static const char *c_skyboxSuffixes[ 6 ] = { "_lf", "_rt", "_ft", "_bk", "_up", "_dn" };

static const char *SkyLightMode_ToString( skyLightMode_t mode ){
	switch ( mode )
	{
	case SKYLIGHT_MODE_LEGACY:
		return "legacy";
	case SKYLIGHT_MODE_SHADER:
		return "shader";
	case SKYLIGHT_MODE_PANORAMA:
		return "panorama";
	case SKYLIGHT_MODE_AUTO:
	default:
		return "auto";
	}
}

static skyLightMode_t SkyLightMode_Parse( const char *mode ){
	if ( !Q_stricmp( mode, "legacy" ) ) {
		return SKYLIGHT_MODE_LEGACY;
	}
	if ( !Q_stricmp( mode, "shader" ) ) {
		return SKYLIGHT_MODE_SHADER;
	}
	if ( !Q_stricmp( mode, "panorama" ) ) {
		return SKYLIGHT_MODE_PANORAMA;
	}
	return SKYLIGHT_MODE_AUTO;
}

static void SkyFloatImage_Clear( skyFloatImage_t *image ){
	if ( image->pixels != NULL ) {
		free( image->pixels );
	}
	image->pixels = NULL;
	image->width = 0;
	image->height = 0;
}

static qboolean SkyFloatImage_LoadExact( const char *path, skyFloatImage_t *image ){
	char ext[ 64 ];
	void *buffer = NULL;
	int size = 0;

	ExtractFileExtension( path, ext );
	if ( !Q_stricmp( ext, "hdr" ) || !Q_stricmp( ext, "exr" ) ) {
		size = vfsLoadFile( path, &buffer, 0 );
		if ( size <= 0 || buffer == NULL ) {
			return qfalse;
		}

		if ( !Q_stricmp( ext, "hdr" ) ) {
			if ( !MapBake3_LoadHDRBufferToRGBAF32( (const unsigned char*) buffer, size, &image->pixels, &image->width, &image->height ) ) {
				free( buffer );
				return qfalse;
			}
		}
		else{
			if ( !MapBake3_LoadEXRBufferToRGBAF32( (const unsigned char*) buffer, size, &image->pixels, &image->width, &image->height ) ) {
				free( buffer );
				return qfalse;
			}
		}

		free( buffer );
		return qtrue;
	}
	else{
		image_t *ldr = ImageLoad( path );
		size_t count;
		size_t i;
		float *pixels;

		if ( ldr == NULL ) {
			return qfalse;
		}

		count = (size_t) ldr->width * (size_t) ldr->height * 4u;
		pixels = safe_malloc( count * sizeof( float ) );
		for ( i = 0; i < count; ++i )
		{
			pixels[ i ] = ldr->pixels[ i ] / 255.0f;
		}

		image->pixels = pixels;
		image->width = ldr->width;
		image->height = ldr->height;
		return qtrue;
	}
}

static qboolean SkyFloatImage_LoadAny( const char *path, skyFloatImage_t *image, char *resolvedPath ){
	char ext[ 64 ];
	char tryPath[ MAX_QPATH ];

	ExtractFileExtension( path, ext );
	if ( ext[ 0 ] != '\0' ) {
		if ( SkyFloatImage_LoadExact( path, image ) ) {
			strcpy( resolvedPath, path );
			return qtrue;
		}
		return qfalse;
	}

	sprintf( tryPath, "%s.hdr", path );
	if ( SkyFloatImage_LoadExact( tryPath, image ) ) {
		strcpy( resolvedPath, tryPath );
		return qtrue;
	}

	sprintf( tryPath, "%s.exr", path );
	if ( SkyFloatImage_LoadExact( tryPath, image ) ) {
		strcpy( resolvedPath, tryPath );
		return qtrue;
	}

	if ( SkyFloatImage_LoadExact( path, image ) ) {
		strcpy( resolvedPath, path );
		return qtrue;
	}

	return qfalse;
}

static qboolean SkyPanorama_IsEquirectangular( int width, int height ){
	return width > 0 && height > 0 && width >= height && ( width * 3 ) <= ( height * 8 );
}

static void SkyPanorama_SampleDirection( const skyFloatImage_t *image, const vec3_t direction, vec3_t color ){
	float lon;
	float lat;
	float u;
	float v;
	float x;
	float y;
	int x0;
	int x1;
	int y0;
	int y1;
	float tx;
	float ty;
	const float *p00;
	const float *p10;
	const float *p01;
	const float *p11;

	if ( image == NULL || image->pixels == NULL || image->width <= 0 || image->height <= 0 ) {
		VectorClear( color );
		return;
	}

	lon = atan2( direction[ 1 ], direction[ 0 ] );
	lat = direction[ 2 ];
	if ( lat < -1.0f ) {
		lat = -1.0f;
	}
	else if ( lat > 1.0f ) {
		lat = 1.0f;
	}
	lat = asin( lat );
	u = lon * ( 1.0f / ( 2.0f * Q_PI ) ) + 0.5f;
	v = 0.5f - lat * ( 1.0f / Q_PI );

	x = u * image->width;
	y = v * image->height;
	while ( x < 0.0f ) x += image->width;
	while ( x >= image->width ) x -= image->width;
	if ( y < 0.0f ) y = 0.0f;
	if ( y > ( image->height - 1.0f ) ) y = image->height - 1.0f;

	x0 = (int) floor( x );
	x1 = ( x0 + 1 ) % image->width;
	y0 = (int) floor( y );
	y1 = ( y0 + 1 < image->height ) ? ( y0 + 1 ) : y0;
	tx = x - x0;
	ty = y - y0;

	p00 = image->pixels + ( ( y0 * image->width + x0 ) * 4 );
	p10 = image->pixels + ( ( y0 * image->width + x1 ) * 4 );
	p01 = image->pixels + ( ( y1 * image->width + x0 ) * 4 );
	p11 = image->pixels + ( ( y1 * image->width + x1 ) * 4 );

	color[ 0 ] = ( 1 - tx ) * ( 1 - ty ) * p00[ 0 ] + tx * ( 1 - ty ) * p10[ 0 ] + ( 1 - tx ) * ty * p01[ 0 ] + tx * ty * p11[ 0 ];
	color[ 1 ] = ( 1 - tx ) * ( 1 - ty ) * p00[ 1 ] + tx * ( 1 - ty ) * p10[ 1 ] + ( 1 - tx ) * ty * p01[ 1 ] + tx * ty * p11[ 1 ];
	color[ 2 ] = ( 1 - tx ) * ( 1 - ty ) * p00[ 2 ] + tx * ( 1 - ty ) * p10[ 2 ] + ( 1 - tx ) * ty * p01[ 2 ] + tx * ty * p11[ 2 ];
	if ( color[ 0 ] < 0.0f ) color[ 0 ] = 0.0f;
	if ( color[ 1 ] < 0.0f ) color[ 1 ] = 0.0f;
	if ( color[ 2 ] < 0.0f ) color[ 2 ] = 0.0f;
}

static void SkyPanorama_DirectionForFaceUV( int face, float u, float v, vec3_t direction ){
	const float fu = 2.0f * u - 1.0f;
	const float fv = 2.0f * v - 1.0f;

	switch ( face )
	{
	case 0: VectorSet( direction, -1.0f, fu, 1.0f - fv ); break;      /* _lf */
	case 1: VectorSet( direction, 1.0f, 1.0f - fu, 1.0f - fv ); break; /* _rt */
	case 2: VectorSet( direction, fu, 1.0f, 1.0f - fv ); break;        /* _ft */
	case 3: VectorSet( direction, 1.0f - fu, -1.0f, 1.0f - fv ); break;/* _bk */
	case 4: VectorSet( direction, 1.0f - fu, 1.0f - fv, 1.0f ); break; /* _up */
	case 5: VectorSet( direction, fu, 1.0f - fv, -1.0f ); break;       /* _dn */
	default: VectorSet( direction, 0.0f, 0.0f, 1.0f ); break;
	}

	VectorNormalize( direction, direction );
}

static void SkyPanorama_WriteConvertedFaces( const skyFloatImage_t *image, const char *sourcePath, const char *outputBase, int faceSize ){
	int face;
	int x;
	int y;
	byte *pixels;
	char outputPath[ 1024 ];
	char outputDir[ 1024 ];

	if ( image == NULL || image->pixels == NULL || outputBase == NULL || outputBase[ 0 ] == '\0' ) {
		return;
	}

	if ( sourcePath != NULL
	  && !Q_stricmp( sourcePath, g_skyLightLastConvertedPath )
	  && !Q_stricmp( outputBase, g_skyLightLastConvertedBase )
	  && faceSize == g_skyLightLastConvertedSize ) {
		return;
	}

	if ( faceSize < 16 ) {
		faceSize = 16;
	}
	else if ( faceSize > 4096 ) {
		faceSize = 4096;
	}
	pixels = safe_malloc( faceSize * faceSize * 4 );

	for ( face = 0; face < 6; ++face )
	{
		for ( y = 0; y < faceSize; ++y )
		{
			for ( x = 0; x < faceSize; ++x )
			{
				vec3_t direction;
				vec3_t rgb;
				byte *out = pixels + ( ( y * faceSize + x ) * 4 );
				SkyPanorama_DirectionForFaceUV( face, ( x + 0.5f ) / faceSize, ( y + 0.5f ) / faceSize, direction );
				SkyPanorama_SampleDirection( image, direction, rgb );
				int ir = (int) ( rgb[ 0 ] * 255.0f + 0.5f );
				int ig = (int) ( rgb[ 1 ] * 255.0f + 0.5f );
				int ib = (int) ( rgb[ 2 ] * 255.0f + 0.5f );
				if ( ir < 0 ) ir = 0;
				else if ( ir > 255 ) ir = 255;
				if ( ig < 0 ) ig = 0;
				else if ( ig > 255 ) ig = 255;
				if ( ib < 0 ) ib = 0;
				else if ( ib > 255 ) ib = 255;
				out[ 0 ] = (byte) ir;
				out[ 1 ] = (byte) ig;
				out[ 2 ] = (byte) ib;
				out[ 3 ] = 255;
			}
		}

		sprintf( outputPath, "%s%s.tga", outputBase, c_skyboxSuffixes[ face ] );
		ExtractFilePath( outputPath, outputDir );
		if ( outputDir[ 0 ] != '\0' ) {
			CreatePath( outputPath );
		}
		WriteTGA( outputPath, pixels, faceSize, faceSize );
		Sys_Printf( "Wrote panoramic sky face: %s\n", outputPath );
	}

	free( pixels );
	if ( sourcePath != NULL ) {
		strcpy( g_skyLightLastConvertedPath, sourcePath );
	}
	strcpy( g_skyLightLastConvertedBase, outputBase );
	g_skyLightLastConvertedSize = faceSize;
}

static qboolean SkyPanorama_SelectPath( const shaderInfo_t *si, char *path ){
	path[ 0 ] = '\0';

	switch ( g_skyLightMode )
	{
	case SKYLIGHT_MODE_LEGACY:
		return qfalse;
	case SKYLIGHT_MODE_PANORAMA:
		if ( g_skyLightPanoramaPath[ 0 ] != '\0' ) {
			strcpy( path, g_skyLightPanoramaPath );
			return qtrue;
		}
		return qfalse;
	case SKYLIGHT_MODE_SHADER:
		if ( si != NULL && si->skyParmsImageBase[ 0 ] != '\0' ) {
			strcpy( path, si->skyParmsImageBase );
			return qtrue;
		}
		return qfalse;
	case SKYLIGHT_MODE_AUTO:
	default:
		if ( g_skyLightPanoramaPath[ 0 ] != '\0' ) {
			strcpy( path, g_skyLightPanoramaPath );
			return qtrue;
		}
		if ( si != NULL && si->skyParmsImageBase[ 0 ] != '\0' ) {
			strcpy( path, si->skyParmsImageBase );
			return qtrue;
		}
		return qfalse;
	}
}



/*
   CreateSunLight() - ydnar
   this creates a sun light
 */

static void CreateSunLight( sun_t *sun ){
	int i;
	float photons, d, angle, elevation, da, de;
	vec3_t direction;
	light_t     *light;


	/* dummy check */
	if ( sun == NULL ) {
		return;
	}

	/* fixup */
	if ( sun->numSamples < 1 ) {
		sun->numSamples = 1;
	}

	/* set photons */
	photons = sun->photons / sun->numSamples;

	/* create the right number of suns */
	for ( i = 0; i < sun->numSamples; i++ )
	{
		/* calculate sun direction */
		if ( i == 0 ) {
			VectorCopy( sun->direction, direction );
		}
		else
		{
			/*
			    sun->direction[ 0 ] = cos( angle ) * cos( elevation );
			    sun->direction[ 1 ] = sin( angle ) * cos( elevation );
			    sun->direction[ 2 ] = sin( elevation );

			    xz_dist   = sqrt( x*x + z*z )
			    latitude  = atan2( xz_dist, y ) * RADIANS
			    longitude = atan2( x,       z ) * RADIANS
			 */

			d = sqrt( sun->direction[ 0 ] * sun->direction[ 0 ] + sun->direction[ 1 ] * sun->direction[ 1 ] );
			angle = atan2( sun->direction[ 1 ], sun->direction[ 0 ] );
			elevation = atan2( sun->direction[ 2 ], d );

			/* jitter the angles (loop to keep random sample within sun->deviance steridians) */
			do
			{
				da = ( Random() * 2.0f - 1.0f ) * sun->deviance;
				de = ( Random() * 2.0f - 1.0f ) * sun->deviance;
			}
			while ( ( da * da + de * de ) > ( sun->deviance * sun->deviance ) );
			angle += da;
			elevation += de;

			/* debug code */
			//%	Sys_Printf( "%d: Angle: %3.4f Elevation: %3.3f\n", sun->numSamples, (angle / Q_PI * 180.0f), (elevation / Q_PI * 180.0f) );

			/* create new vector */
			direction[ 0 ] = cos( angle ) * cos( elevation );
			direction[ 1 ] = sin( angle ) * cos( elevation );
			direction[ 2 ] = sin( elevation );
		}

		/* create a light */
		numSunLights++;
		light = safe_malloc( sizeof( *light ) );
		memset( light, 0, sizeof( *light ) );
		light->next = lights;
		lights = light;

		/* initialize the light */
		light->flags = LIGHT_SUN_DEFAULT;
		light->type = EMIT_SUN;
		light->fade = 1.0f;
		light->falloffTolerance = falloffTolerance;
		light->filterRadius = sun->filterRadius / sun->numSamples;
		light->style = noStyles ? LS_NORMAL : sun->style;

		/* set the light's position out to infinity */
		VectorMA( vec3_origin, ( MAX_WORLD_COORD * 8.0f ), direction, light->origin );    /* MAX_WORLD_COORD * 2.0f */

		/* set the facing to be the inverse of the sun direction */
		VectorScale( direction, -1.0, light->normal );
		light->dist = DotProduct( light->origin, light->normal );

		/* set color and photons */
		VectorCopy( sun->color, light->color );
		light->photons = photons * skyScale;
	}

	/* another sun? */
	if ( sun->next != NULL ) {
		CreateSunLight( sun->next );
	}
}



/*
   CreateSkyLights() - ydnar
   simulates sky light with multiple suns
 */

static void CreateSkyLights( shaderInfo_t *si, vec3_t color, float value, int iterations, float filterRadius, int style ){
	int i, j, numSuns;
	int angleSteps, elevationSteps;
	float angle, elevation;
	float angleStep, elevationStep;
	float basePhotons;
	sun_t sun;
	skyFloatImage_t panorama = { 0, 0, NULL };
	qboolean usePanorama = qfalse;
	char panoramaPath[ MAX_QPATH ];
	char resolvedPanoramaPath[ MAX_QPATH ];


	/* dummy check */
	if ( value <= 0.0f || iterations < 2 ) {
		return;
	}

	panoramaPath[ 0 ] = '\0';
	resolvedPanoramaPath[ 0 ] = '\0';
	if ( SkyPanorama_SelectPath( si, panoramaPath ) ) {
		if ( SkyFloatImage_LoadAny( panoramaPath, &panorama, resolvedPanoramaPath ) ) {
			if ( SkyPanorama_IsEquirectangular( panorama.width, panorama.height ) ) {
				usePanorama = qtrue;
			}
			else{
				Sys_Printf( "WARNING: Sky panorama %s is not equirectangular (expected ~2:1 aspect), using legacy skylight color.\n", resolvedPanoramaPath );
			}
		}
		else if ( g_skyLightMode == SKYLIGHT_MODE_PANORAMA ) {
			Sys_Printf( "WARNING: Failed to load panoramic skylight image %s, using legacy skylight color.\n", panoramaPath );
		}
	}

	if ( usePanorama && g_skyLightPanoramaConvertBase[ 0 ] != '\0' ) {
		SkyPanorama_WriteConvertedFaces( &panorama, resolvedPanoramaPath, g_skyLightPanoramaConvertBase, g_skyLightPanoramaFaceSize );
	}

	/* basic sun setup */
	VectorCopy( color, sun.color );
	sun.deviance = 0.0f;
	sun.filterRadius = filterRadius;
	sun.numSamples = 1;
	sun.style = noStyles ? LS_NORMAL : style;
	sun.next = NULL;

	/* setup */
	elevationSteps = iterations - 1;
	angleSteps = elevationSteps * 4;
	angle = 0.0f;
	elevationStep = DEG2RAD( 90.0f / iterations );  /* skip elevation 0 */
	angleStep = DEG2RAD( 360.0f / angleSteps );

	/* calc individual sun brightness */
	numSuns = angleSteps * elevationSteps + 1;
	basePhotons = value / numSuns;
	sun.photons = basePhotons;

	/* iterate elevation */
	elevation = elevationStep * 0.5f;
	angle = 0.0f;
	for ( i = 0, elevation = elevationStep * 0.5f; i < elevationSteps; i++ )
	{
		/* iterate angle */
		for ( j = 0; j < angleSteps; j++ )
		{
			/* create sun */
			sun.direction[ 0 ] = cos( angle ) * cos( elevation );
			sun.direction[ 1 ] = sin( angle ) * cos( elevation );
			sun.direction[ 2 ] = sin( elevation );

			if ( usePanorama ) {
				vec3_t sampled;
				float scale;
				SkyPanorama_SampleDirection( &panorama, sun.direction, sampled );
				scale = ColorNormalize( sampled, sun.color );
				if ( scale <= 0.0f ) {
					angle += angleStep;
					continue;
				}
				sun.photons = basePhotons * scale;
			}
			else{
				VectorCopy( color, sun.color );
				sun.photons = basePhotons;
			}

			CreateSunLight( &sun );

			/* move */
			angle += angleStep;
		}

		/* move */
		elevation += elevationStep;
		angle += angleStep / elevationSteps;
	}

	/* create vertical sun */
	VectorSet( sun.direction, 0.0f, 0.0f, 1.0f );
	if ( usePanorama ) {
		vec3_t sampled;
		float scale;
		SkyPanorama_SampleDirection( &panorama, sun.direction, sampled );
		scale = ColorNormalize( sampled, sun.color );
		if ( scale > 0.0f ) {
			sun.photons = basePhotons * scale;
			CreateSunLight( &sun );
		}
	}
	else{
		VectorCopy( color, sun.color );
		sun.photons = basePhotons;
		CreateSunLight( &sun );
	}

	SkyFloatImage_Clear( &panorama );

	/* short circuit */
	return;
}



/*
   CreateEntityLights()
   creates lights from light entities
 */

void CreateEntityLights( void ){
	int i, j;
	light_t         *light, *light2;
	entity_t        *e, *e2;
	const char      *name;
	const char      *target;
	vec3_t dest;
	const char      *_color;
	float intensity, scale, deviance, filterRadius;
	int spawnflags, flags, numSamples;
	qboolean junior;


	/* go throught entity list and find lights */
	for ( i = 0; i < numEntities; i++ )
	{
		/* get entity */
		e = &entities[ i ];
		name = ValueForKey( e, "classname" );

		/* ydnar: check for lightJunior */
		if ( Q_strncasecmp( name, "lightJunior", 11 ) == 0 ) {
			junior = qtrue;
		}
		else if ( Q_strncasecmp( name, "light", 5 ) == 0 ) {
			junior = qfalse;
		}
		else{
			continue;
		}

		/* lights with target names (and therefore styles) are only parsed from BSP */
		target = ValueForKey( e, "targetname" );
		if ( target[ 0 ] != '\0' && i >= numBSPEntities ) {
			continue;
		}

		/* create a light */
		numPointLights++;
		light = safe_malloc( sizeof( *light ) );
		memset( light, 0, sizeof( *light ) );
		light->next = lights;
		lights = light;

		/* handle spawnflags */
		spawnflags = IntForKey( e, "spawnflags" );

		/* ydnar: quake 3+ light behavior */
		if ( wolfLight == qfalse ) {
			/* set default flags */
			flags = LIGHT_Q3A_DEFAULT;

			/* linear attenuation? */
			if ( spawnflags & 1 ) {
				flags |= LIGHT_ATTEN_LINEAR;
				flags &= ~LIGHT_ATTEN_ANGLE;
			}

			/* no angle attenuate? */
			if ( spawnflags & 2 ) {
				flags &= ~LIGHT_ATTEN_ANGLE;
			}
		}

		/* ydnar: wolf light behavior */
		else
		{
			/* set default flags */
			flags = LIGHT_WOLF_DEFAULT;

			/* inverse distance squared attenuation? */
			if ( spawnflags & 1 ) {
				flags &= ~LIGHT_ATTEN_LINEAR;
				flags |= LIGHT_ATTEN_ANGLE;
			}

			/* angle attenuate? */
			if ( spawnflags & 2 ) {
				flags |= LIGHT_ATTEN_ANGLE;
			}
		}

		/* other flags (borrowed from wolf) */

		/* wolf dark light? */
		if ( ( spawnflags & 4 ) || ( spawnflags & 8 ) ) {
			flags |= LIGHT_DARK;
		}

		/* nogrid? */
		if ( spawnflags & 16 ) {
			flags &= ~LIGHT_GRID;
		}

		/* junior? */
		if ( junior ) {
			flags |= LIGHT_GRID;
			flags &= ~LIGHT_SURFACES;
		}

		/* vortex: unnormalized? */
		if ( spawnflags & 32 ) {
			flags |= LIGHT_UNNORMALIZED;
		}

		/* vortex: distance atten? */
		if ( spawnflags & 64 ) {
			flags |= LIGHT_ATTEN_DISTANCE;
		}

		/* store the flags */
		light->flags = flags;

		/* ydnar: set fade key (from wolf) */
		light->fade = 1.0f;
		if ( light->flags & LIGHT_ATTEN_LINEAR ) {
			light->fade = FloatForKey( e, "fade" );
			if ( light->fade == 0.0f ) {
				light->fade = 1.0f;
			}
		}

		/* ydnar: set angle scaling (from vlight) */
		light->angleScale = FloatForKey( e, "_anglescale" );
		if ( light->angleScale != 0.0f ) {
			light->flags |= LIGHT_ATTEN_ANGLE;
		}

		/* set origin */
		GetVectorForKey( e, "origin", light->origin );
		light->style = IntForKey( e, "_style" );
		if ( light->style == LS_NORMAL ) {
			light->style = IntForKey( e, "style" );
		}
		if ( light->style < LS_NORMAL || light->style >= LS_NONE ) {
			Error( "Invalid lightstyle (%d) on entity %d", light->style, i );
		}

		if ( light->style != LS_NORMAL ) {
			Sys_FPrintf( SYS_WRN, "WARNING: Styled light found targeting %s\n **", target );
		}

		/* set light intensity */
		intensity = FloatForKey( e, "_light" );
		if ( intensity == 0.0f ) {
			intensity = FloatForKey( e, "light" );
		}
		if ( intensity == 0.0f ) {
			intensity = 300.0f;
		}

		/* ydnar: set light scale (sof2) */
		scale = FloatForKey( e, "scale" );
		if ( scale == 0.0f ) {
			scale = 1.0f;
		}
		intensity *= scale;

		/* ydnar: get deviance and samples */
		deviance = FloatForKey( e, "_deviance" );
		if ( deviance == 0.0f ) {
			deviance = FloatForKey( e, "_deviation" );
		}
		if ( deviance == 0.0f ) {
			deviance = FloatForKey( e, "_jitter" );
		}
		numSamples = IntForKey( e, "_samples" );
		if ( deviance < 0.0f || numSamples < 1 ) {
			deviance = 0.0f;
			numSamples = 1;
		}
		intensity /= numSamples;

		/* ydnar: get filter radius */
		filterRadius = FloatForKey( e, "_filterradius" );
		if ( filterRadius == 0.0f ) {
			filterRadius = FloatForKey( e, "_filteradius" );
		}
		if ( filterRadius == 0.0f ) {
			filterRadius = FloatForKey( e, "_filter" );
		}
		if ( filterRadius < 0.0f ) {
			filterRadius = 0.0f;
		}
		light->filterRadius = filterRadius;

		/* set light color */
		_color = ValueForKey( e, "_color" );
		if ( _color && _color[ 0 ] ) {
			sscanf( _color, "%f %f %f", &light->color[ 0 ], &light->color[ 1 ], &light->color[ 2 ] );
			if ( colorsRGB ) {
				light->color[0] = Image_LinearFloatFromsRGBFloat( light->color[0] );
				light->color[1] = Image_LinearFloatFromsRGBFloat( light->color[1] );
				light->color[2] = Image_LinearFloatFromsRGBFloat( light->color[2] );
			}
			if ( !( light->flags & LIGHT_UNNORMALIZED ) ) {
				ColorNormalize( light->color, light->color );
			}
		}
		else{
			light->color[ 0 ] = light->color[ 1 ] = light->color[ 2 ] = 1.0f;
		}

		light->extraDist = FloatForKey( e, "_extradist" );
		if ( light->extraDist == 0.0f ) {
			light->extraDist = extraDist;
		}

		light->photons = intensity;

		light->type = EMIT_POINT;

		/* set falloff threshold */
		light->falloffTolerance = falloffTolerance / numSamples;

		/* lights with a target will be spotlights */
		target = ValueForKey( e, "target" );
		if ( target[ 0 ] ) {
			float radius;
			float dist;
			sun_t sun;
			const char  *_sun;


			/* get target */
			e2 = FindTargetEntity( target );
			if ( e2 == NULL ) {
				Sys_Printf( "WARNING: light at (%i %i %i) has missing target\n",
							(int) light->origin[ 0 ], (int) light->origin[ 1 ], (int) light->origin[ 2 ] );
				light->photons *= pointScale;
			}
			else
			{
				/* not a point light */
				numPointLights--;
				numSpotLights++;

				/* make a spotlight */
				GetVectorForKey( e2, "origin", dest );
				VectorSubtract( dest, light->origin, light->normal );
				dist = VectorNormalize( light->normal, light->normal );
				radius = FloatForKey( e, "radius" );
				if ( !radius ) {
					radius = 64;
				}
				if ( !dist ) {
					dist = 64;
				}
				light->radiusByDist = ( radius + 16 ) / dist;
				light->type = EMIT_SPOT;

				/* ydnar: wolf mods: spotlights always use nonlinear + angle attenuation */
				light->flags &= ~LIGHT_ATTEN_LINEAR;
				light->flags |= LIGHT_ATTEN_ANGLE;
				light->fade = 1.0f;

				/* ydnar: is this a sun? */
				_sun = ValueForKey( e, "_sun" );
				if ( _sun[ 0 ] == '1' ) {
					/* not a spot light */
					numSpotLights--;

					/* unlink this light */
					lights = light->next;

					/* make a sun */
					VectorScale( light->normal, -1.0f, sun.direction );
					VectorCopy( light->color, sun.color );
					sun.photons = intensity;
					sun.deviance = deviance / 180.0f * Q_PI;
					sun.numSamples = numSamples;
					sun.style = noStyles ? LS_NORMAL : light->style;
					sun.next = NULL;

					/* make a sun light */
					CreateSunLight( &sun );

					/* free original light */
					free( light );
					light = NULL;

					/* skip the rest of this love story */
					continue;
				}
				else
				{
					light->photons *= spotScale;
				}
			}
		}
		else{
			light->photons *= pointScale;
		}

		/* jitter the light */
		for ( j = 1; j < numSamples; j++ )
		{
			/* create a light */
			light2 = safe_malloc( sizeof( *light ) );
			memcpy( light2, light, sizeof( *light ) );
			light2->next = lights;
			lights = light2;

			/* add to counts */
			if ( light->type == EMIT_SPOT ) {
				numSpotLights++;
			}
			else{
				numPointLights++;
			}

			/* jitter it */
			light2->origin[ 0 ] = light->origin[ 0 ] + ( Random() * 2.0f - 1.0f ) * deviance;
			light2->origin[ 1 ] = light->origin[ 1 ] + ( Random() * 2.0f - 1.0f ) * deviance;
			light2->origin[ 2 ] = light->origin[ 2 ] + ( Random() * 2.0f - 1.0f ) * deviance;
		}
	}
}



/*
   CreateSurfaceLights() - ydnar
   this hijacks the radiosity code to generate surface lights for first pass
 */

#define APPROX_BOUNCE   1.0f

void CreateSurfaceLights( void ){
	int i;
	bspDrawSurface_t    *ds;
	surfaceInfo_t       *info;
	shaderInfo_t        *si;
	light_t             *light;
	float subdivide;
	vec3_t origin;
	clipWork_t cw;
	const char          *nss;


	/* get sun shader supressor */
	nss = ValueForKey( &entities[ 0 ], "_noshadersun" );

	/* walk the list of surfaces */
	for ( i = 0; i < numBSPDrawSurfaces; i++ )
	{
		/* get surface and other bits */
		ds = &bspDrawSurfaces[ i ];
		info = &surfaceInfos[ i ];
		si = info->si;

		/* sunlight? */
		if ( si->sun != NULL && nss[ 0 ] != '1' ) {
			Sys_FPrintf( SYS_VRB, "Sun: %s\n", si->shader );
			CreateSunLight( si->sun );
			si->sun = NULL; /* FIXME: leak! */
		}

		/* sky light? */
		if ( si->skyLightValue > 0.0f ) {
			Sys_FPrintf( SYS_VRB, "Sky: %s\n", si->shader );
			CreateSkyLights( si, si->color, si->skyLightValue, si->skyLightIterations, si->lightFilterRadius, si->lightStyle );
			si->skyLightValue = 0.0f;   /* FIXME: hack! */
		}

		/* try to early out */
		if ( si->value <= 0 ) {
			continue;
		}

		/* autosprite shaders become point lights */
		if ( si->autosprite ) {
			/* create an average xyz */
			VectorAdd( info->mins, info->maxs, origin );
			VectorScale( origin, 0.5f, origin );

			/* create a light */
			light = safe_malloc( sizeof( *light ) );
			memset( light, 0, sizeof( *light ) );
			light->next = lights;
			lights = light;

			/* set it up */
			light->flags = LIGHT_Q3A_DEFAULT;
			light->type = EMIT_POINT;
			light->photons = si->value * pointScale;
			light->fade = 1.0f;
			light->si = si;
			VectorCopy( origin, light->origin );
			VectorCopy( si->color, light->color );
			light->falloffTolerance = falloffTolerance;
			light->style = si->lightStyle;

			/* add to point light count and continue */
			numPointLights++;
			continue;
		}

		/* get subdivision amount */
		if ( si->lightSubdivide > 0 ) {
			subdivide = si->lightSubdivide;
		}
		else{
			subdivide = defaultLightSubdivide;
		}

		/* switch on type */
		switch ( ds->surfaceType )
		{
		case MST_PLANAR:
		case MST_TRIANGLE_SOUP:
			RadLightForTriangles( i, 0, info->lm, si, APPROX_BOUNCE, subdivide, &cw );
			break;

		case MST_PATCH:
			RadLightForPatch( i, 0, info->lm, si, APPROX_BOUNCE, subdivide, &cw );
			break;

		default:
			break;
		}
	}
}



/*
   SetEntityOrigins()
   find the offset values for inline models
 */

void SetEntityOrigins( void ){
	int i, j, k, f;
	entity_t            *e;
	vec3_t origin;
	const char          *key;
	int modelnum;
	bspModel_t          *dm;
	bspDrawSurface_t    *ds;


	/* ydnar: copy drawverts into private storage for nefarious purposes */
	yDrawVerts = safe_malloc( numBSPDrawVerts * sizeof( bspDrawVert_t ) );
	memcpy( yDrawVerts, bspDrawVerts, numBSPDrawVerts * sizeof( bspDrawVert_t ) );

	/* set the entity origins */
	for ( i = 0; i < numEntities; i++ )
	{
		/* get entity and model */
		e = &entities[ i ];
		key = ValueForKey( e, "model" );
		if ( key[ 0 ] != '*' ) {
			continue;
		}
		modelnum = atoi( key + 1 );
		dm = &bspModels[ modelnum ];

		/* get entity origin */
		key = ValueForKey( e, "origin" );
		if ( key[ 0 ] == '\0' ) {
			continue;
		}
		GetVectorForKey( e, "origin", origin );

		/* set origin for all surfaces for this model */
		for ( j = 0; j < dm->numBSPSurfaces; j++ )
		{
			/* get drawsurf */
			ds = &bspDrawSurfaces[ dm->firstBSPSurface + j ];

			/* set its verts */
			for ( k = 0; k < ds->numVerts; k++ )
			{
				f = ds->firstVert + k;
				VectorAdd( origin, bspDrawVerts[ f ].xyz, yDrawVerts[ f ].xyz );
			}
		}
	}
}



/*
   PointToPolygonFormFactor()
   calculates the area over a point/normal hemisphere a winding covers
   ydnar: fixme: there has to be a faster way to calculate this
   without the expensive per-vert sqrts and transcendental functions
   ydnar 2002-09-30: added -faster switch because only 19% deviance > 10%
   between this and the approximation
 */

#define ONE_OVER_2PI    0.159154942f    //% (1.0f / (2.0f * 3.141592657f))

float PointToPolygonFormFactor( const vec3_t point, const vec3_t normal, const winding_t *w ){
	vec3_t triVector, triNormal;
	int i, j;
	vec3_t dirs[ MAX_POINTS_ON_WINDING ];
	float total;
	float dot, angle, facing;


	/* this is expensive */
	for ( i = 0; i < w->numpoints; i++ )
	{
		VectorSubtract( w->p[ i ], point, dirs[ i ] );
		VectorNormalize( dirs[ i ], dirs[ i ] );
	}

	/* duplicate first vertex to avoid mod operation */
	VectorCopy( dirs[ 0 ], dirs[ i ] );

	/* calculcate relative area */
	total = 0.0f;
	for ( i = 0; i < w->numpoints; i++ )
	{
		/* get a triangle */
		j = i + 1;
		dot = DotProduct( dirs[ i ], dirs[ j ] );

		/* roundoff can cause slight creep, which gives an IND from acos */
		if ( dot > 1.0f ) {
			dot = 1.0f;
		}
		else if ( dot < -1.0f ) {
			dot = -1.0f;
		}

		/* get the angle */
		angle = acos( dot );

		CrossProduct( dirs[ i ], dirs[ j ], triVector );
		if ( VectorNormalize( triVector, triNormal ) < 0.0001f ) {
			continue;
		}

		facing = DotProduct( normal, triNormal );
		total += facing * angle;

		/* ydnar: this was throwing too many errors with radiosity + crappy maps. ignoring it. */
		if ( total > 6.3f || total < -6.3f ) {
			return 0.0f;
		}
	}

	/* now in the range of 0 to 1 over the entire incoming hemisphere */
	//%	total /= (2.0f * 3.141592657f);
	total *= ONE_OVER_2PI;
	return total;
}



/*
   LightContributionTosample()
   determines the amount of light reaching a sample (luxel or vertex) from a given light
 */

int LightContributionToSample( trace_t *trace ){
	light_t         *light;
	float angle;
	float add;
	float dist;
	float addDeluxe = 0.0f, addDeluxeBounceScale = 0.25f;
	qboolean angledDeluxe = qtrue;
	float colorBrightness;
	qboolean doAddDeluxe = qtrue;

	/* get light */
	light = trace->light;

	/* clear color */
	trace->forceSubsampling = 0.0f; /* to make sure */
	VectorClear( trace->color );
	VectorClear( trace->colorNoShadow );
	VectorClear( trace->directionContribution );

	colorBrightness = RGBTOGRAY( light->color ) * ( 1.0f / 255.0f );

	/* ydnar: early out */
	if ( !( light->flags & LIGHT_SURFACES ) || light->envelope <= 0.0f ) {
		return 0;
	}

	/* do some culling checks */
	if ( light->type != EMIT_SUN ) {
		/* MrE: if the light is behind the surface */
		if ( trace->twoSided == qfalse ) {
			if ( DotProduct( light->origin, trace->normal ) - DotProduct( trace->origin, trace->normal ) < 0.0f ) {
				return 0;
			}
		}

		/* ydnar: test pvs */
		if ( !ClusterVisible( trace->cluster, light->cluster ) ) {
			return 0;
		}
	}

	/* exact point to polygon form factor */
	if ( light->type == EMIT_AREA ) {
		float factor;
		float d;
		vec3_t pushedOrigin;

		/* project sample point into light plane */
		d = DotProduct( trace->origin, light->normal ) - light->dist;
		if ( d < 3.0f ) {
			/* sample point behind plane? */
			if ( !( light->flags & LIGHT_TWOSIDED ) && d < -1.0f ) {
				return 0;
			}

			/* sample plane coincident? */
			if ( d > -3.0f && DotProduct( trace->normal, light->normal ) > 0.9f ) {
				return 0;
			}
		}

		/* nudge the point so that it is clearly forward of the light */
		/* so that surfaces meeting a light emitter don't get black edges */
		if ( d > -8.0f && d < 8.0f ) {
			VectorMA( trace->origin, ( 8.0f - d ), light->normal, pushedOrigin );
		}
		else{
			VectorCopy( trace->origin, pushedOrigin );
		}

		/* get direction and distance */
		VectorCopy( light->origin, trace->end );
		dist = SetupTrace( trace );
		if ( dist >= light->envelope ) {
			return 0;
		}

		/* ptpff approximation */
		if ( faster ) {
			/* angle attenuation */
			angle = DotProduct( trace->normal, trace->direction );

			/* twosided lighting */
			if ( trace->twoSided && angle < 0 ) {
				angle = -angle;

				/* no deluxemap contribution from "other side" light */
				doAddDeluxe = qfalse;
			}

			/* attenuate */
			angle *= -DotProduct( light->normal, trace->direction );
			if ( angle == 0.0f ) {
				return 0;
			}
			else if ( angle < 0.0f &&
					  ( trace->twoSided || ( light->flags & LIGHT_TWOSIDED ) ) ) {
				angle = -angle;

				/* no deluxemap contribution from "other side" light */
				doAddDeluxe = qfalse;
			}

			/* clamp the distance to prevent super hot spots */
			dist = sqrt( dist * dist + light->extraDist * light->extraDist );
			if ( dist < 16.0f ) {
				dist = 16.0f;
			}

			add = light->photons / ( dist * dist ) * angle;

			if ( deluxemap ) {
				if ( angledDeluxe ) {
					addDeluxe = light->photons / ( dist * dist ) * angle;
				}
				else{
					addDeluxe = light->photons / ( dist * dist );
				}
			}
		}
		else
		{
			/* calculate the contribution */
			factor = PointToPolygonFormFactor( pushedOrigin, trace->normal, light->w );
			if ( factor == 0.0f ) {
				return 0;
			}
			else if ( factor < 0.0f ) {
				/* twosided lighting */
				if ( trace->twoSided || ( light->flags & LIGHT_TWOSIDED ) ) {
					factor = -factor;

					/* push light origin to other side of the plane */
					VectorMA( light->origin, -2.0f, light->normal, trace->end );
					dist = SetupTrace( trace );
					if ( dist >= light->envelope ) {
						return 0;
					}

					/* no deluxemap contribution from "other side" light */
					doAddDeluxe = qfalse;
				}
				else{
					return 0;
				}
			}

			/* also don't deluxe if the direction is on the wrong side */
			if ( DotProduct( trace->normal, trace->direction ) < 0 ) {
				/* no deluxemap contribution from "other side" light */
				doAddDeluxe = qfalse;
			}

			/* ydnar: moved to here */
			add = factor * light->add;

			if ( deluxemap ) {
				addDeluxe = add;
			}
		}
	}

	/* point/spot lights */
	else if ( light->type == EMIT_POINT || light->type == EMIT_SPOT ) {
		/* get direction and distance */
		VectorCopy( light->origin, trace->end );
		dist = SetupTrace( trace );
		if ( dist >= light->envelope ) {
			return 0;
		}

		/* clamp the distance to prevent super hot spots */
		dist = sqrt( dist * dist + light->extraDist * light->extraDist );
		if ( dist < 16.0f ) {
			dist = 16.0f;
		}

		/* angle attenuation */
		if ( light->flags & LIGHT_ATTEN_ANGLE ) {
			/* standard Lambert attenuation */
			float dot = DotProduct( trace->normal, trace->direction );

			/* twosided lighting */
			if ( trace->twoSided && dot < 0 ) {
				dot = -dot;

				/* no deluxemap contribution from "other side" light */
				doAddDeluxe = qfalse;
			}

			/* jal: optional half Lambert attenuation (http://developer.valvesoftware.com/wiki/Half_Lambert) */
			if ( lightAngleHL ) {
				if ( dot > 0.001f ) { // skip coplanar
					if ( dot > 1.0f ) {
						dot = 1.0f;
					}
					dot = ( dot * 0.5f ) + 0.5f;
					dot *= dot;
				}
				else{
					dot = 0;
				}
			}

			angle = dot;
		}
		else{
			angle = 1.0f;
		}

		if ( light->angleScale != 0.0f ) {
			angle /= light->angleScale;
			if ( angle > 1.0f ) {
				angle = 1.0f;
			}
		}

		/* attenuate */
		if ( light->flags & LIGHT_ATTEN_LINEAR ) {
			add = angle * light->photons * linearScale - ( dist * light->fade );
			if ( add < 0.0f ) {
				add = 0.0f;
			}

			if ( deluxemap ) {
				if ( angledDeluxe ) {
					addDeluxe = angle * light->photons * linearScale - ( dist * light->fade );
				}
				else{
					addDeluxe = light->photons * linearScale - ( dist * light->fade );
				}

				if ( addDeluxe < 0.0f ) {
					addDeluxe = 0.0f;
				}
			}
		}
		else
		{
			add = ( light->photons / ( dist * dist ) ) * angle;
			if ( add < 0.0f ) {
				add = 0.0f;
			}

			if ( deluxemap ) {
				if ( angledDeluxe ) {
					addDeluxe = ( light->photons / ( dist * dist ) ) * angle;
				}
				else{
					addDeluxe = ( light->photons / ( dist * dist ) );
				}
			}

			if ( addDeluxe < 0.0f ) {
				addDeluxe = 0.0f;
			}
		}

		/* handle spotlights */
		if ( light->type == EMIT_SPOT ) {
			float distByNormal, radiusAtDist, sampleRadius;
			vec3_t pointAtDist, distToSample;

			/* do cone calculation */
			distByNormal = -DotProduct( trace->displacement, light->normal );
			if ( distByNormal < 0.0f ) {
				return 0;
			}
			VectorMA( light->origin, distByNormal, light->normal, pointAtDist );
			radiusAtDist = light->radiusByDist * distByNormal;
			VectorSubtract( trace->origin, pointAtDist, distToSample );
			sampleRadius = VectorLength( distToSample );

			/* outside the cone */
			if ( sampleRadius >= radiusAtDist ) {
				return 0;
			}

			/* attenuate */
			if ( sampleRadius > ( radiusAtDist - 32.0f ) ) {
				add *= ( ( radiusAtDist - sampleRadius ) / 32.0f );
				if ( add < 0.0f ) {
					add = 0.0f;
				}

				addDeluxe *= ( ( radiusAtDist - sampleRadius ) / 32.0f );

				if ( addDeluxe < 0.0f ) {
					addDeluxe = 0.0f;
				}
			}
		}
	}

	/* ydnar: sunlight */
	else if ( light->type == EMIT_SUN ) {
		/* get origin and direction */
		VectorAdd( trace->origin, light->origin, trace->end );
		dist = SetupTrace( trace );

		/* angle attenuation */
		if ( light->flags & LIGHT_ATTEN_ANGLE ) {
			/* standard Lambert attenuation */
			float dot = DotProduct( trace->normal, trace->direction );

			/* twosided lighting */
			if ( trace->twoSided && dot < 0 ) {
				dot = -dot;

				/* no deluxemap contribution from "other side" light */
				doAddDeluxe = qfalse;
			}

			/* jal: optional half Lambert attenuation (http://developer.valvesoftware.com/wiki/Half_Lambert) */
			if ( lightAngleHL ) {
				if ( dot > 0.001f ) { // skip coplanar
					if ( dot > 1.0f ) {
						dot = 1.0f;
					}
					dot = ( dot * 0.5f ) + 0.5f;
					dot *= dot;
				}
				else{
					dot = 0;
				}
			}

			angle = dot;
		}
		else{
			angle = 1.0f;
		}

		/* attenuate */
		add = light->photons * angle;

		if ( deluxemap ) {
			if ( angledDeluxe ) {
				addDeluxe = light->photons * angle;
			}
			else{
				addDeluxe = light->photons;
			}

			if ( addDeluxe < 0.0f ) {
				addDeluxe = 0.0f;
			}
		}

		if ( add <= 0.0f ) {
			return 0;
		}

		/* VorteX: set noShadow color */
		VectorScale( light->color, add, trace->colorNoShadow );

		addDeluxe *= colorBrightness;

		if ( bouncing ) {
			addDeluxe *= addDeluxeBounceScale;
			if ( addDeluxe < 0.00390625f ) {
				addDeluxe = 0.00390625f;
			}
		}

		VectorScale( trace->direction, addDeluxe, trace->directionContribution );

		/* setup trace */
		trace->testAll = qtrue;
		VectorScale( light->color, add, trace->color );

		/* trace to point */
		if ( trace->testOcclusion && !trace->forceSunlight ) {
			/* trace */
			TraceLine( trace );
			trace->forceSubsampling *= add;
			if ( !( trace->compileFlags & C_SKY ) || trace->opaque ) {
				VectorClear( trace->color );
				VectorClear( trace->directionContribution );

				return -1;
			}
		}

		/* return to sender */
		return 1;
	}
	else{
		Error( "Light of undefined type!" );
	}

	/* VorteX: set noShadow color */
	VectorScale( light->color, add, trace->colorNoShadow );

	/* ydnar: changed to a variable number */
	if ( add <= 0.0f || ( add <= light->falloffTolerance && ( light->flags & LIGHT_FAST_ACTUAL ) ) ) {
		return 0;
	}

	addDeluxe *= colorBrightness;

	/* hack land: scale down the radiosity contribution to light directionality.
	   Deluxemaps fusion many light directions into one. In a rtl process all lights
	   would contribute individually to the bump map, so several light sources together
	   would make it more directional (example: a yellow and red lights received from
	   opposing sides would light one side in red and the other in blue, adding
	   the effect of 2 directions applied. In the deluxemapping case, this 2 lights would
	   neutralize each other making it look like having no direction.
	   Same thing happens with radiosity. In deluxemapping case the radiosity contribution
	   is modifying the direction applied from directional lights, making it go closer and closer
	   to the surface normal the bigger is the amount of radiosity received.
	   So, for preserving the directional lights contributions, we scale down the radiosity
	   contribution. It's a hack, but there's a reason behind it */
	if ( bouncing ) {
		addDeluxe *= addDeluxeBounceScale;
		/* better NOT increase it beyond the original value
		   if( addDeluxe < 0.00390625f )
		    addDeluxe = 0.00390625f;
		 */
	}

	if ( doAddDeluxe ) {
		VectorScale( trace->direction, addDeluxe, trace->directionContribution );
	}

	/* setup trace */
	trace->testAll = qfalse;
	VectorScale( light->color, add, trace->color );

	/* raytrace */
	TraceLine( trace );
	trace->forceSubsampling *= add;
	if ( trace->passSolid || trace->opaque ) {
		VectorClear( trace->color );
		VectorClear( trace->directionContribution );

		return -1;
	}

	/* return to sender */
	return 1;
}



/*
   LightingAtSample()
   determines the amount of light reaching a sample (luxel or vertex)
 */

void LightingAtSample( trace_t *trace, byte styles[ MAX_LIGHTMAPS ], vec3_t colors[ MAX_LIGHTMAPS ] ){
	int i, lightmapNum;


	/* clear colors */
	for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
		VectorClear( colors[ lightmapNum ] );

	/* ydnar: normalmap */
	if ( normalmap ) {
		colors[ 0 ][ 0 ] = ( trace->normal[ 0 ] + 1.0f ) * 127.5f;
		colors[ 0 ][ 1 ] = ( trace->normal[ 1 ] + 1.0f ) * 127.5f;
		colors[ 0 ][ 2 ] = ( trace->normal[ 2 ] + 1.0f ) * 127.5f;
		return;
	}

	/* ydnar: don't bounce ambient all the time */
	if ( !bouncing ) {
		VectorCopy( ambientColor, colors[ 0 ] );
	}

	/* ydnar: trace to all the list of lights pre-stored in tw */
	for ( i = 0; i < trace->numLights && trace->lights[ i ] != NULL; i++ )
	{
		/* set light */
		trace->light = trace->lights[ i ];

		/* style check */
		for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
		{
			if ( styles[ lightmapNum ] == trace->light->style ||
				 styles[ lightmapNum ] == LS_NONE ) {
				break;
			}
		}

		/* max of MAX_LIGHTMAPS (4) styles allowed to hit a sample */
		if ( lightmapNum >= MAX_LIGHTMAPS ) {
			continue;
		}

		/* sample light */
		LightContributionToSample( trace );
		if ( trace->color[ 0 ] == 0.0f && trace->color[ 1 ] == 0.0f && trace->color[ 2 ] == 0.0f ) {
			continue;
		}

		/* handle negative light */
		if ( trace->light->flags & LIGHT_NEGATIVE ) {
			VectorScale( trace->color, -1.0f, trace->color );
		}

		/* set style */
		styles[ lightmapNum ] = trace->light->style;

		/* add it */
		VectorAdd( colors[ lightmapNum ], trace->color, colors[ lightmapNum ] );

		/* cheap mode */
		if ( cheap &&
			 colors[ 0 ][ 0 ] >= 255.0f &&
			 colors[ 0 ][ 1 ] >= 255.0f &&
			 colors[ 0 ][ 2 ] >= 255.0f ) {
			break;
		}
	}
}



/*
   LightContributionToPoint()
   for a given light, how much light/color reaches a given point in space (with no facing)
   note: this is similar to LightContributionToSample() but optimized for omnidirectional sampling
 */

int LightContributionToPoint( trace_t *trace ){
	light_t     *light;
	float add, dist;


	/* get light */
	light = trace->light;

	/* clear color */
	VectorClear( trace->color );

	/* ydnar: early out */
	if ( !( light->flags & LIGHT_GRID ) || light->envelope <= 0.0f ) {
		return qfalse;
	}

	/* is this a sun? */
	if ( light->type != EMIT_SUN ) {
		/* sun only? */
		if ( sunOnly ) {
			return qfalse;
		}

		/* test pvs */
		if ( !ClusterVisible( trace->cluster, light->cluster ) ) {
			return qfalse;
		}
	}

	/* ydnar: check origin against light's pvs envelope */
	if ( trace->origin[ 0 ] > light->maxs[ 0 ] || trace->origin[ 0 ] < light->mins[ 0 ] ||
		 trace->origin[ 1 ] > light->maxs[ 1 ] || trace->origin[ 1 ] < light->mins[ 1 ] ||
		 trace->origin[ 2 ] > light->maxs[ 2 ] || trace->origin[ 2 ] < light->mins[ 2 ] ) {
		gridBoundsCulled++;
		return qfalse;
	}

	/* set light origin */
	if ( light->type == EMIT_SUN ) {
		VectorAdd( trace->origin, light->origin, trace->end );
	}
	else{
		VectorCopy( light->origin, trace->end );
	}

	/* set direction */
	dist = SetupTrace( trace );

	/* test envelope */
	if ( dist > light->envelope ) {
		gridEnvelopeCulled++;
		return qfalse;
	}

	/* ptpff approximation */
	if ( light->type == EMIT_AREA && faster ) {
		/* clamp the distance to prevent super hot spots */
		dist = sqrt( dist * dist + light->extraDist * light->extraDist );
		if ( dist < 16.0f ) {
			dist = 16.0f;
		}

		/* attenuate */
		add = light->photons / ( dist * dist );
	}

	/* exact point to polygon form factor */
	else if ( light->type == EMIT_AREA ) {
		float factor, d;
		vec3_t pushedOrigin;


		/* see if the point is behind the light */
		d = DotProduct( trace->origin, light->normal ) - light->dist;
		if ( !( light->flags & LIGHT_TWOSIDED ) && d < -1.0f ) {
			return qfalse;
		}

		/* nudge the point so that it is clearly forward of the light */
		/* so that surfaces meeting a light emiter don't get black edges */
		if ( d > -8.0f && d < 8.0f ) {
			VectorMA( trace->origin, ( 8.0f - d ), light->normal, pushedOrigin );
		}
		else{
			VectorCopy( trace->origin, pushedOrigin );
		}

		/* calculate the contribution (ydnar 2002-10-21: [bug 642] bad normal calc) */
		factor = PointToPolygonFormFactor( pushedOrigin, trace->direction, light->w );
		if ( factor == 0.0f ) {
			return qfalse;
		}
		else if ( factor < 0.0f ) {
			if ( light->flags & LIGHT_TWOSIDED ) {
				factor = -factor;
			}
			else{
				return qfalse;
			}
		}

		/* ydnar: moved to here */
		add = factor * light->add;
	}

	/* point/spot lights */
	else if ( light->type == EMIT_POINT || light->type == EMIT_SPOT ) {
		/* clamp the distance to prevent super hot spots */
		dist = sqrt( dist * dist + light->extraDist * light->extraDist );
		if ( dist < 16.0f ) {
			dist = 16.0f;
		}

		/* attenuate */
		if ( light->flags & LIGHT_ATTEN_LINEAR ) {
			add = light->photons * linearScale - ( dist * light->fade );
			if ( add < 0.0f ) {
				add = 0.0f;
			}
		}
		else{
			add = light->photons / ( dist * dist );
		}

		/* handle spotlights */
		if ( light->type == EMIT_SPOT ) {
			float distByNormal, radiusAtDist, sampleRadius;
			vec3_t pointAtDist, distToSample;


			/* do cone calculation */
			distByNormal = -DotProduct( trace->displacement, light->normal );
			if ( distByNormal < 0.0f ) {
				return qfalse;
			}
			VectorMA( light->origin, distByNormal, light->normal, pointAtDist );
			radiusAtDist = light->radiusByDist * distByNormal;
			VectorSubtract( trace->origin, pointAtDist, distToSample );
			sampleRadius = VectorLength( distToSample );

			/* outside the cone */
			if ( sampleRadius >= radiusAtDist ) {
				return qfalse;
			}

			/* attenuate */
			if ( sampleRadius > ( radiusAtDist - 32.0f ) ) {
				add *= ( ( radiusAtDist - sampleRadius ) / 32.0f );
			}
		}
	}

	/* ydnar: sunlight */
	else if ( light->type == EMIT_SUN ) {
		/* attenuate */
		add = light->photons;
		if ( add <= 0.0f ) {
			return qfalse;
		}

		/* setup trace */
		trace->testAll = qtrue;
		VectorScale( light->color, add, trace->color );

		/* trace to point */
		if ( trace->testOcclusion && !trace->forceSunlight ) {
			/* trace */
			TraceLine( trace );
			if ( !( trace->compileFlags & C_SKY ) || trace->opaque ) {
				VectorClear( trace->color );
				return -1;
			}
		}

		/* return to sender */
		return qtrue;
	}

	/* unknown light type */
	else{
		return qfalse;
	}

	/* ydnar: changed to a variable number */
	if ( add <= 0.0f || ( add <= light->falloffTolerance && ( light->flags & LIGHT_FAST_ACTUAL ) ) ) {
		return qfalse;
	}

	/* setup trace */
	trace->testAll = qfalse;
	VectorScale( light->color, add, trace->color );

	/* trace */
	TraceLine( trace );
	if ( trace->passSolid ) {
		VectorClear( trace->color );
		return qfalse;
	}

	/* we have a valid sample */
	return qtrue;
}



/*
   TraceGrid()
   grid samples are for quickly determining the lighting
   of dynamically placed entities in the world
 */

#define MAX_CONTRIBUTIONS   32768

typedef struct
{
	vec3_t dir;
	vec3_t color;
	vec3_t ambient;
	int style;
}
contribution_t;

void TraceGrid( int num ){
	int i, j, x, y, z, mod, numCon, numStyles;
	float d, step;
	vec3_t baseOrigin, cheapColor, color, thisdir;
	rawGridPoint_t          *gp;
	bspGridPoint_t          *bgp;
	contribution_t contributions[ MAX_CONTRIBUTIONS ];
	trace_t trace;

	/* get grid points */
	gp = &rawGridPoints[ num ];
	bgp = &bspGridPoints[ num ];

	/* get grid origin */
	mod = num;
	z = mod / ( gridBounds[ 0 ] * gridBounds[ 1 ] );
	mod -= z * ( gridBounds[ 0 ] * gridBounds[ 1 ] );
	y = mod / gridBounds[ 0 ];
	mod -= y * gridBounds[ 0 ];
	x = mod;

	trace.origin[ 0 ] = gridMins[ 0 ] + x * gridSize[ 0 ];
	trace.origin[ 1 ] = gridMins[ 1 ] + y * gridSize[ 1 ];
	trace.origin[ 2 ] = gridMins[ 2 ] + z * gridSize[ 2 ];

	/* set inhibit sphere */
	if ( gridSize[ 0 ] > gridSize[ 1 ] && gridSize[ 0 ] > gridSize[ 2 ] ) {
		trace.inhibitRadius = gridSize[ 0 ] * 0.5f;
	}
	else if ( gridSize[ 1 ] > gridSize[ 0 ] && gridSize[ 1 ] > gridSize[ 2 ] ) {
		trace.inhibitRadius = gridSize[ 1 ] * 0.5f;
	}
	else{
		trace.inhibitRadius = gridSize[ 2 ] * 0.5f;
	}

	/* find point cluster */
	trace.cluster = ClusterForPointExt( trace.origin, GRID_EPSILON );
	if ( trace.cluster < 0 ) {
		/* try to nudge the origin around to find a valid point */
		VectorCopy( trace.origin, baseOrigin );
		for ( step = 0; ( step += 0.005 ) <= 1.0; )
		{
			VectorCopy( baseOrigin, trace.origin );
			trace.origin[ 0 ] += step * ( Random() - 0.5 ) * gridSize[0];
			trace.origin[ 1 ] += step * ( Random() - 0.5 ) * gridSize[1];
			trace.origin[ 2 ] += step * ( Random() - 0.5 ) * gridSize[2];

			/* ydnar: changed to find cluster num */
			trace.cluster = ClusterForPointExt( trace.origin, VERTEX_EPSILON );
			if ( trace.cluster >= 0 ) {
				break;
			}
		}

		/* can't find a valid point at all */
		if ( step > 1.0 ) {
			return;
		}
	}

	/* setup trace */
	trace.testOcclusion = !noTrace;
	trace.forceSunlight = qfalse;
	trace.recvShadows = WORLDSPAWN_RECV_SHADOWS;
	trace.numSurfaces = 0;
	trace.surfaces = NULL;
	trace.numLights = 0;
	trace.lights = NULL;

	/* clear */
	numCon = 0;
	VectorClear( cheapColor );

	/* trace to all the lights, find the major light direction, and divide the
	   total light between that along the direction and the remaining in the ambient */
	for ( trace.light = lights; trace.light != NULL; trace.light = trace.light->next )
	{
		float addSize;


		/* sample light */
		if ( !LightContributionToPoint( &trace ) ) {
			continue;
		}

		/* handle negative light */
		if ( trace.light->flags & LIGHT_NEGATIVE ) {
			VectorScale( trace.color, -1.0f, trace.color );
		}

		/* add a contribution */
		VectorCopy( trace.color, contributions[ numCon ].color );
		VectorCopy( trace.direction, contributions[ numCon ].dir );
		VectorClear( contributions[ numCon ].ambient );
		contributions[ numCon ].style = trace.light->style;
		numCon++;

		/* push average direction around */
		addSize = VectorLength( trace.color );
		VectorMA( gp->dir, addSize, trace.direction, gp->dir );

		/* stop after a while */
		if ( numCon >= ( MAX_CONTRIBUTIONS - 1 ) ) {
			break;
		}

		/* ydnar: cheap mode */
		VectorAdd( cheapColor, trace.color, cheapColor );
		if ( cheapgrid && cheapColor[ 0 ] >= 255.0f && cheapColor[ 1 ] >= 255.0f && cheapColor[ 2 ] >= 255.0f ) {
			break;
		}
	}

	/////// Floodlighting for point //////////////////
	//do our floodlight ambient occlusion loop, and add a single contribution based on the brightest dir
	if ( floodlighty ) {
		int k;
		float addSize, f;
		vec3_t dir = { 0, 0, 1 };
		float ambientFrac = 0.25f;

		trace.testOcclusion = qtrue;
		trace.forceSunlight = qfalse;
		trace.inhibitRadius = DEFAULT_INHIBIT_RADIUS;
		trace.testAll = qtrue;

		for ( k = 0; k < 2; k++ )
		{
			if ( k == 0 ) { // upper hemisphere
				trace.normal[0] = 0;
				trace.normal[1] = 0;
				trace.normal[2] = 1;
			}
			else //lower hemisphere
			{
				trace.normal[0] = 0;
				trace.normal[1] = 0;
				trace.normal[2] = -1;
			}

			f = FloodLightForSample( &trace, floodlightDistance, floodlight_lowquality );

			/* add a fraction as pure ambient, half as top-down direction */
			contributions[ numCon ].color[0] = floodlightRGB[0] * floodlightIntensity * f * ( 1.0f - ambientFrac );
			contributions[ numCon ].color[1] = floodlightRGB[1] * floodlightIntensity * f * ( 1.0f - ambientFrac );
			contributions[ numCon ].color[2] = floodlightRGB[2] * floodlightIntensity * f * ( 1.0f - ambientFrac );

			contributions[ numCon ].ambient[0] = floodlightRGB[0] * floodlightIntensity * f * ambientFrac;
			contributions[ numCon ].ambient[1] = floodlightRGB[1] * floodlightIntensity * f * ambientFrac;
			contributions[ numCon ].ambient[2] = floodlightRGB[2] * floodlightIntensity * f * ambientFrac;

			contributions[ numCon ].dir[0] = dir[0];
			contributions[ numCon ].dir[1] = dir[1];
			contributions[ numCon ].dir[2] = dir[2];

			contributions[ numCon ].style = 0;

			/* push average direction around */
			addSize = VectorLength( contributions[ numCon ].color );
			VectorMA( gp->dir, addSize, dir, gp->dir );

			numCon++;
		}
	}
	/////////////////////

	/* normalize to get primary light direction */
	VectorNormalize( gp->dir, thisdir );

	/* now that we have identified the primary light direction,
	   go back and separate all the light into directed and ambient */

	numStyles = 1;
	for ( i = 0; i < numCon; i++ )
	{
		/* get relative directed strength */
		d = DotProduct( contributions[ i ].dir, thisdir );
		/* we map 1 to gridDirectionality, and 0 to gridAmbientDirectionality */
		d = gridAmbientDirectionality + d * ( gridDirectionality - gridAmbientDirectionality );
		if ( d < 0.0f ) {
			d = 0.0f;
		}

		/* find appropriate style */
		for ( j = 0; j < numStyles; j++ )
		{
			if ( gp->styles[ j ] == contributions[ i ].style ) {
				break;
			}
		}

		/* style not found? */
		if ( j >= numStyles ) {
			/* add a new style */
			if ( numStyles < MAX_LIGHTMAPS ) {
				gp->styles[ numStyles ] = contributions[ i ].style;
				bgp->styles[ numStyles ] = contributions[ i ].style;
				numStyles++;
				//%	Sys_Printf( "(%d, %d) ", num, contributions[ i ].style );
			}

			/* fallback */
			else{
				j = 0;
			}
		}

		/* add the directed color */
		VectorMA( gp->directed[ j ], d, contributions[ i ].color, gp->directed[ j ] );

		/* ambient light will be at 1/4 the value of directed light */
		/* (ydnar: nuke this in favor of more dramatic lighting?) */
		/* (PM: how about actually making it work? d=1 when it got here for single lights/sun :P */
//		d = 0.25f;
		/* (Hobbes: always setting it to .25 is hardly any better) */
		d = 0.25f * ( 1.0f - d );
		VectorMA( gp->ambient[ j ], d, contributions[ i ].color, gp->ambient[ j ] );

		VectorAdd( gp->ambient[ j ], contributions[ i ].ambient, gp->ambient[ j ] );

/*
 * div0:
 * the total light average = ambient value + 0.25 * sum of all directional values
 * we can also get the total light average as 0.25 * the sum of all contributions
 *
 * 0.25 * sum(contribution_i) == ambient + 0.25 * sum(d_i contribution_i)
 *
 * THIS YIELDS:
 * ambient == 0.25 * sum((1 - d_i) contribution_i)
 *
 * So, 0.25f * (1.0f - d) IS RIGHT. If you want to tune it, tune d BEFORE.
 */
	}


	/* store off sample */
	for ( i = 0; i < MAX_LIGHTMAPS; i++ )
	{
#if 0
		/* do some fudging to keep the ambient from being too low (2003-07-05: 0.25 -> 0.125) */
		if ( !bouncing ) {
			VectorMA( gp->ambient[ i ], 0.125f, gp->directed[ i ], gp->ambient[ i ] );
		}
#endif

		/* set minimum light and copy off to bytes */
		VectorCopy( gp->ambient[ i ], color );
		for ( j = 0; j < 3; j++ )
			if ( color[ j ] < minGridLight[ j ] ) {
				color[ j ] = minGridLight[ j ];
			}

		/* vortex: apply gridscale and gridambientscale here */
		ColorToBytes( color, bgp->ambient[ i ], gridScale * gridAmbientScale );
		ColorToBytes( gp->directed[ i ], bgp->directed[ i ], gridScale );
	}

	/* debug code */
	#if 0
	//%	Sys_FPrintf( SYS_VRB, "%10d %10d %10d ", &gp->ambient[ 0 ][ 0 ], &gp->ambient[ 0 ][ 1 ], &gp->ambient[ 0 ][ 2 ] );
	Sys_FPrintf( SYS_VRB, "%9d Amb: (%03.1f %03.1f %03.1f) Dir: (%03.1f %03.1f %03.1f)\n",
				 num,
				 gp->ambient[ 0 ][ 0 ], gp->ambient[ 0 ][ 1 ], gp->ambient[ 0 ][ 2 ],
				 gp->directed[ 0 ][ 0 ], gp->directed[ 0 ][ 1 ], gp->directed[ 0 ][ 2 ] );
	#endif

	/* store direction */
	NormalToLatLong( thisdir, bgp->latLong );
}



/*
   SetupGrid()
   calculates the size of the lightgrid and allocates memory
 */

void SetupGrid( void ){
	int i, j;
	vec3_t maxs, oldGridSize;
	const char  *value;
	char temp[ 64 ];


	/* don't do this if not grid lighting */
	if ( noGridLighting ) {
		return;
	}

	/* ydnar: set grid size */
	value = ValueForKey( &entities[ 0 ], "gridsize" );
	if ( value[ 0 ] != '\0' ) {
		sscanf( value, "%f %f %f", &gridSize[ 0 ], &gridSize[ 1 ], &gridSize[ 2 ] );
	}

	/* quantize it */
	VectorCopy( gridSize, oldGridSize );
	for ( i = 0; i < 3; i++ )
		gridSize[ i ] = gridSize[ i ] >= 8.0f ? floor( gridSize[ i ] ) : 8.0f;

	/* ydnar: increase gridSize until grid count is smaller than max allowed */
	numRawGridPoints = MAX_MAP_LIGHTGRID + 1;
	j = 0;
	while ( numRawGridPoints > MAX_MAP_LIGHTGRID )
	{
		/* get world bounds */
		for ( i = 0; i < 3; i++ )
		{
			gridMins[ i ] = gridSize[ i ] * ceil( bspModels[ 0 ].mins[ i ] / gridSize[ i ] );
			maxs[ i ] = gridSize[ i ] * floor( bspModels[ 0 ].maxs[ i ] / gridSize[ i ] );
			gridBounds[ i ] = ( maxs[ i ] - gridMins[ i ] ) / gridSize[ i ] + 1;
		}

		/* set grid size */
		numRawGridPoints = gridBounds[ 0 ] * gridBounds[ 1 ] * gridBounds[ 2 ];

		/* increase grid size a bit */
		if ( numRawGridPoints > MAX_MAP_LIGHTGRID ) {
			gridSize[ j++ % 3 ] += 16.0f;
		}
	}

	/* print it */
	Sys_Printf( "Grid size = { %1.0f, %1.0f, %1.0f }\n", gridSize[ 0 ], gridSize[ 1 ], gridSize[ 2 ] );

	/* different? */
	if ( !VectorCompare( gridSize, oldGridSize ) ) {
		sprintf( temp, "%.0f %.0f %.0f", gridSize[ 0 ], gridSize[ 1 ], gridSize[ 2 ] );
		SetKeyValue( &entities[ 0 ], "gridsize", (const char*) temp );
		Sys_FPrintf( SYS_VRB, "Storing adjusted grid size\n" );
	}

	/* 2nd variable. fixme: is this silly? */
	numBSPGridPoints = numRawGridPoints;

	/* allocate lightgrid */
	rawGridPoints = safe_malloc( numRawGridPoints * sizeof( *rawGridPoints ) );
	memset( rawGridPoints, 0, numRawGridPoints * sizeof( *rawGridPoints ) );

	if ( bspGridPoints != NULL ) {
		free( bspGridPoints );
	}
	bspGridPoints = safe_malloc( numBSPGridPoints * sizeof( *bspGridPoints ) );
	memset( bspGridPoints, 0, numBSPGridPoints * sizeof( *bspGridPoints ) );

	/* clear lightgrid */
	for ( i = 0; i < numRawGridPoints; i++ )
	{
		VectorCopy( ambientColor, rawGridPoints[ i ].ambient[ j ] );
		rawGridPoints[ i ].styles[ 0 ] = LS_NORMAL;
		bspGridPoints[ i ].styles[ 0 ] = LS_NORMAL;
		for ( j = 1; j < MAX_LIGHTMAPS; j++ )
		{
			rawGridPoints[ i ].styles[ j ] = LS_NONE;
			bspGridPoints[ i ].styles[ j ] = LS_NONE;
		}
	}

	/* note it */
	Sys_Printf( "%9d grid points\n", numRawGridPoints );
}



/*
   LightWorld()
   does what it says...
 */

void LightWorld( const char *BSPFilePath, qboolean fastAllocate ){
	vec3_t color;
	float f;
	int b, bt;
	qboolean minVertex, minGrid;
	const char  *value;


	/* ydnar: smooth normals */
	if ( shade ) {
		Sys_Printf( "--- SmoothNormals ---\n" );
		SmoothNormals();
	}

	/* determine the number of grid points */
	Sys_Printf( "--- SetupGrid ---\n" );
	SetupGrid();

	/* find the optional minimum lighting values */
	GetVectorForKey( &entities[ 0 ], "_color", color );
	if ( VectorLength( color ) == 0.0f ) {
		VectorSet( color, 1.0, 1.0, 1.0 );
	}
	if ( colorsRGB ) {
		color[0] = Image_LinearFloatFromsRGBFloat( color[0] );
		color[1] = Image_LinearFloatFromsRGBFloat( color[1] );
		color[2] = Image_LinearFloatFromsRGBFloat( color[2] );
	}

	/* ambient */
	f = FloatForKey( &entities[ 0 ], "_ambient" );
	if ( f == 0.0f ) {
		f = FloatForKey( &entities[ 0 ], "ambient" );
	}
	VectorScale( color, f, ambientColor );

	/* minvertexlight */
	minVertex = qfalse;
	value = ValueForKey( &entities[ 0 ], "_minvertexlight" );
	if ( value[ 0 ] != '\0' ) {
		minVertex = qtrue;
		f = atof( value );
		VectorScale( color, f, minVertexLight );
	}

	/* mingridlight */
	minGrid = qfalse;
	value = ValueForKey( &entities[ 0 ], "_mingridlight" );
	if ( value[ 0 ] != '\0' ) {
		minGrid = qtrue;
		f = atof( value );
		VectorScale( color, f, minGridLight );
	}

	/* minlight */
	value = ValueForKey( &entities[ 0 ], "_minlight" );
	if ( value[ 0 ] != '\0' ) {
		f = atof( value );
		VectorScale( color, f, minLight );
		if ( minVertex == qfalse ) {
			VectorScale( color, f, minVertexLight );
		}
		if ( minGrid == qfalse ) {
			VectorScale( color, f, minGridLight );
		}
	}

	/* create world lights */
	Sys_FPrintf( SYS_VRB, "--- CreateLights ---\n" );
	CreateEntityLights();
	CreateSurfaceLights();
	Sys_Printf( "%9d point lights\n", numPointLights );
	Sys_Printf( "%9d spotlights\n", numSpotLights );
	Sys_Printf( "%9d diffuse (area) lights\n", numDiffuseLights );
	Sys_Printf( "%9d sun/sky lights\n", numSunLights );

	/* calculate lightgrid */
	if ( !noGridLighting ) {
		/* ydnar: set up light envelopes */
		SetupEnvelopes( qtrue, fastgrid );

		Sys_Printf( "--- TraceGrid ---\n" );
		inGrid = qtrue;
		RunThreadsOnIndividual( numRawGridPoints, qtrue, TraceGrid );
		inGrid = qfalse;
		Sys_Printf( "%d x %d x %d = %d grid\n",
					gridBounds[ 0 ], gridBounds[ 1 ], gridBounds[ 2 ], numBSPGridPoints );

		/* ydnar: emit statistics on light culling */
		Sys_FPrintf( SYS_VRB, "%9d grid points envelope culled\n", gridEnvelopeCulled );
		Sys_FPrintf( SYS_VRB, "%9d grid points bounds culled\n", gridBoundsCulled );
	}

	/* slight optimization to remove a sqrt */
	subdivideThreshold *= subdivideThreshold;

	/* map the world luxels */
	Sys_Printf( "--- MapRawLightmap ---\n" );
	RunThreadsOnIndividual( numRawLightmaps, qtrue, MapRawLightmap );
	Sys_Printf( "%9d luxels\n", numLuxels );
	Sys_Printf( "%9d luxels mapped\n", numLuxelsMapped );
	Sys_Printf( "%9d luxels occluded\n", numLuxelsOccluded );

	/* dirty them up */
	if ( dirty ) {
		Sys_Printf( "--- DirtyRawLightmap ---\n" );
		RunThreadsOnIndividual( numRawLightmaps, qtrue, DirtyRawLightmap );
	}

	/* floodlight pass */
	FloodlightRawLightmaps();

	/* ydnar: set up light envelopes */
	SetupEnvelopes( qfalse, fast );

	/* light up my world */
	lightsPlaneCulled = 0;
	lightsEnvelopeCulled = 0;
	lightsBoundsCulled = 0;
	lightsClusterCulled = 0;

	Sys_Printf( "--- IlluminateRawLightmap ---\n" );
	RunThreadsOnIndividual( numRawLightmaps, qtrue, IlluminateRawLightmap );
	Sys_Printf( "%9d luxels illuminated\n", numLuxelsIlluminated );

	StitchSurfaceLightmaps();

	Sys_Printf( "--- IlluminateVertexes ---\n" );
	RunThreadsOnIndividual( numBSPDrawSurfaces, qtrue, IlluminateVertexes );
	Sys_Printf( "%9d vertexes illuminated\n", numVertsIlluminated );

	/* ydnar: emit statistics on light culling */
	Sys_FPrintf( SYS_VRB, "%9d lights plane culled\n", lightsPlaneCulled );
	Sys_FPrintf( SYS_VRB, "%9d lights envelope culled\n", lightsEnvelopeCulled );
	Sys_FPrintf( SYS_VRB, "%9d lights bounds culled\n", lightsBoundsCulled );
	Sys_FPrintf( SYS_VRB, "%9d lights cluster culled\n", lightsClusterCulled );

	/* radiosity */
	b = 1;
	bt = bounce;
	while ( bounce > 0 )
	{
		/* store off the bsp between bounces */
		StoreSurfaceLightmaps( fastAllocate );
		UnparseEntities();
		Sys_Printf( "Writing %s\n", BSPFilePath );
		WriteBSPFile( BSPFilePath );

		/* note it */
		Sys_Printf( "\n--- Radiosity (bounce %d of %d) ---\n", b, bt );

		/* flag bouncing */
		bouncing = qtrue;
		VectorClear( ambientColor );
		floodlighty = qfalse;

		/* generate diffuse lights */
		RadFreeLights();
		RadCreateDiffuseLights();

		/* setup light envelopes */
		SetupEnvelopes( qfalse, fastbounce );
		if ( numLights == 0 ) {
			Sys_Printf( "No diffuse light to calculate, ending radiosity.\n" );
			break;
		}

		/* add to lightgrid */
		if ( bouncegrid ) {
			gridEnvelopeCulled = 0;
			gridBoundsCulled = 0;

			Sys_Printf( "--- BounceGrid ---\n" );
			inGrid = qtrue;
			RunThreadsOnIndividual( numRawGridPoints, qtrue, TraceGrid );
			inGrid = qfalse;
			Sys_FPrintf( SYS_VRB, "%9d grid points envelope culled\n", gridEnvelopeCulled );
			Sys_FPrintf( SYS_VRB, "%9d grid points bounds culled\n", gridBoundsCulled );
		}

		/* light up my world */
		lightsPlaneCulled = 0;
		lightsEnvelopeCulled = 0;
		lightsBoundsCulled = 0;
		lightsClusterCulled = 0;

		Sys_Printf( "--- IlluminateRawLightmap ---\n" );
		RunThreadsOnIndividual( numRawLightmaps, qtrue, IlluminateRawLightmap );
		Sys_Printf( "%9d luxels illuminated\n", numLuxelsIlluminated );
		Sys_Printf( "%9d vertexes illuminated\n", numVertsIlluminated );

		StitchSurfaceLightmaps();

		Sys_Printf( "--- IlluminateVertexes ---\n" );
		RunThreadsOnIndividual( numBSPDrawSurfaces, qtrue, IlluminateVertexes );
		Sys_Printf( "%9d vertexes illuminated\n", numVertsIlluminated );

		/* ydnar: emit statistics on light culling */
		Sys_FPrintf( SYS_VRB, "%9d lights plane culled\n", lightsPlaneCulled );
		Sys_FPrintf( SYS_VRB, "%9d lights envelope culled\n", lightsEnvelopeCulled );
		Sys_FPrintf( SYS_VRB, "%9d lights bounds culled\n", lightsBoundsCulled );
		Sys_FPrintf( SYS_VRB, "%9d lights cluster culled\n", lightsClusterCulled );

		/* interate */
		bounce--;
		b++;
	}
}



/*
   LightMain()
   main routine for light processing
 */

static int WishGI_ClampInt( int value, int minValue, int maxValue ){
	if ( value < minValue ) {
		return minValue;
	}
	if ( value > maxValue ) {
		return maxValue;
	}
	return value;
}

static float WishGI_DistanceSquared( const vec3_t a, const vec3_t b ){
	float dx, dy, dz;
	dx = a[ 0 ] - b[ 0 ];
	dy = a[ 1 ] - b[ 1 ];
	dz = a[ 2 ] - b[ 2 ];
	return ( dx * dx ) + ( dy * dy ) + ( dz * dz );
}

static int WishGI_SampleVertexIndex( int sampleIndex, int sampleCount, int totalVerts ){
	double t;

	if ( totalVerts <= 1 || sampleCount <= 1 ) {
		return 0;
	}

	t = (double) sampleIndex / (double) ( sampleCount - 1 );
	return (int) ( t * ( totalVerts - 1 ) );
}

static void WishGI_ClearAssociations( void ){
	if ( wishGIVertexAssociations != NULL ) {
		free( wishGIVertexAssociations );
		wishGIVertexAssociations = NULL;
	}

	if ( wishGIProbePositions != NULL ) {
		free( wishGIProbePositions );
		wishGIProbePositions = NULL;
	}

	wishGIActualProbeCount = 0;
	wishGIActualLinksPerVertex = 0;
}

static void WishGI_DefaultPath( const char *bspFilePath, const char *suffix, char *outPath, int outPathSize ){
	int length;

	if ( bspFilePath == NULL || suffix == NULL || outPath == NULL || outPathSize <= 0 ) {
		return;
	}

	if ( (int) strlen( bspFilePath ) >= outPathSize ) {
		Error( "WishGI path is too long: %s", bspFilePath );
	}

	strcpy( outPath, bspFilePath );
	StripExtension( outPath );
	length = strlen( outPath );
	if ( ( length + strlen( suffix ) + 1 ) >= outPathSize ) {
		Error( "WishGI path is too long after suffix append: %s", outPath );
	}
	strcat( outPath, suffix );
}

static int WishGI_FindNearestProbe( const vec3_t point, int probeCount, vec3_t *probePositions ){
	int probeIndex, nearestProbe;
	float distanceSquared, bestDistanceSquared;

	nearestProbe = 0;
	bestDistanceSquared = 9.99999968e+37f;

	for ( probeIndex = 0; probeIndex < probeCount; probeIndex++ )
	{
		distanceSquared = WishGI_DistanceSquared( point, probePositions[ probeIndex ] );
		if ( distanceSquared < bestDistanceSquared ) {
			bestDistanceSquared = distanceSquared;
			nearestProbe = probeIndex;
		}
	}

	return nearestProbe;
}

static void WishGI_InitializeProbePositions( int probeCount, vec3_t *probePositions ){
	int probeIndex, vertIndex;

	for ( probeIndex = 0; probeIndex < probeCount; probeIndex++ )
	{
		vertIndex = WishGI_SampleVertexIndex( probeIndex, probeCount, numBSPDrawVerts );
		VectorCopy( yDrawVerts[ vertIndex ].xyz, probePositions[ probeIndex ] );
	}
}

static void WishGI_RefineProbePositions( int probeCount, vec3_t *probePositions ){
	double *sumX, *sumY, *sumZ;
	int *sampleCountPerProbe;
	int sampleCount;
	int iteration, sampleIndex, vertexIndex, probeIndex, nearestProbe, fallbackSampleIndex;
	float movement, dx, dy, dz;
	vec3_t updatedPosition;

	if ( probeCount <= 0 ) {
		return;
	}

	sampleCount = numBSPDrawVerts;
	if ( wishGIOptions.maxSampleVerts > 0 && sampleCount > wishGIOptions.maxSampleVerts ) {
		sampleCount = wishGIOptions.maxSampleVerts;
	}
	sampleCount = WishGI_ClampInt( sampleCount, 1, numBSPDrawVerts );

	sumX = safe_malloc( probeCount * sizeof( *sumX ) );
	sumY = safe_malloc( probeCount * sizeof( *sumY ) );
	sumZ = safe_malloc( probeCount * sizeof( *sumZ ) );
	sampleCountPerProbe = safe_malloc( probeCount * sizeof( *sampleCountPerProbe ) );

	for ( iteration = 0; iteration < wishGIOptions.refinementIterations; iteration++ )
	{
		memset( sumX, 0, probeCount * sizeof( *sumX ) );
		memset( sumY, 0, probeCount * sizeof( *sumY ) );
		memset( sumZ, 0, probeCount * sizeof( *sumZ ) );
		memset( sampleCountPerProbe, 0, probeCount * sizeof( *sampleCountPerProbe ) );

		for ( sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++ )
		{
			vertexIndex = WishGI_SampleVertexIndex( sampleIndex, sampleCount, numBSPDrawVerts );
			nearestProbe = WishGI_FindNearestProbe( yDrawVerts[ vertexIndex ].xyz, probeCount, probePositions );
			sumX[ nearestProbe ] += yDrawVerts[ vertexIndex ].xyz[ 0 ];
			sumY[ nearestProbe ] += yDrawVerts[ vertexIndex ].xyz[ 1 ];
			sumZ[ nearestProbe ] += yDrawVerts[ vertexIndex ].xyz[ 2 ];
			sampleCountPerProbe[ nearestProbe ]++;
		}

		movement = 0.0f;
		for ( probeIndex = 0; probeIndex < probeCount; probeIndex++ )
		{
			if ( sampleCountPerProbe[ probeIndex ] > 0 ) {
				updatedPosition[ 0 ] = (float) ( sumX[ probeIndex ] / sampleCountPerProbe[ probeIndex ] );
				updatedPosition[ 1 ] = (float) ( sumY[ probeIndex ] / sampleCountPerProbe[ probeIndex ] );
				updatedPosition[ 2 ] = (float) ( sumZ[ probeIndex ] / sampleCountPerProbe[ probeIndex ] );
			}
			else
			{
				/* Keep unreferenced probes alive by re-seeding from a deterministic sample. */
				fallbackSampleIndex = ( probeIndex * 104729 + iteration * 1543 ) % sampleCount;
				vertexIndex = WishGI_SampleVertexIndex( fallbackSampleIndex, sampleCount, numBSPDrawVerts );
				VectorCopy( yDrawVerts[ vertexIndex ].xyz, updatedPosition );
			}

			dx = updatedPosition[ 0 ] - probePositions[ probeIndex ][ 0 ];
			dy = updatedPosition[ 1 ] - probePositions[ probeIndex ][ 1 ];
			dz = updatedPosition[ 2 ] - probePositions[ probeIndex ][ 2 ];
			movement += sqrt( ( dx * dx ) + ( dy * dy ) + ( dz * dz ) );

			VectorCopy( updatedPosition, probePositions[ probeIndex ] );
		}

		Sys_Printf( "WishGI: inverse probe refinement %d/%d movement %.3f\n",
					iteration + 1, wishGIOptions.refinementIterations, movement );
	}

	free( sumX );
	free( sumY );
	free( sumZ );
	free( sampleCountPerProbe );
}

static void WishGI_BuildVertexAssociations( int linksPerVertex ){
	int vertexIndex, probeIndex, linkIndex, insertIndex;
	float bestDistances[ WISHGI_MAX_LINKS ];
	int bestProbeIndices[ WISHGI_MAX_LINKS ];
	float distanceSquared, weightSum, nearestDistance;
	double averageNearestDistance;
	float inverseDistance;

	if ( wishGIVertexAssociations == NULL || wishGIProbePositions == NULL || wishGIActualProbeCount <= 0 ) {
		return;
	}

	averageNearestDistance = 0.0;
	for ( vertexIndex = 0; vertexIndex < numBSPDrawVerts; vertexIndex++ )
	{
		for ( linkIndex = 0; linkIndex < linksPerVertex; linkIndex++ )
		{
			bestDistances[ linkIndex ] = 9.99999968e+37f;
			bestProbeIndices[ linkIndex ] = 0;
		}

		for ( probeIndex = 0; probeIndex < wishGIActualProbeCount; probeIndex++ )
		{
			distanceSquared = WishGI_DistanceSquared( yDrawVerts[ vertexIndex ].xyz, wishGIProbePositions[ probeIndex ] );
			if ( distanceSquared >= bestDistances[ linksPerVertex - 1 ] ) {
				continue;
			}

			insertIndex = linksPerVertex - 1;
			while ( insertIndex > 0 && distanceSquared < bestDistances[ insertIndex - 1 ] )
			{
				bestDistances[ insertIndex ] = bestDistances[ insertIndex - 1 ];
				bestProbeIndices[ insertIndex ] = bestProbeIndices[ insertIndex - 1 ];
				insertIndex--;
			}

			bestDistances[ insertIndex ] = distanceSquared;
			bestProbeIndices[ insertIndex ] = probeIndex;
		}

		nearestDistance = sqrt( bestDistances[ 0 ] );
		averageNearestDistance += nearestDistance;

		if ( bestDistances[ 0 ] <= 1.0e-6f ) {
			wishGIVertexAssociations[ vertexIndex ].probeIndex[ 0 ] = bestProbeIndices[ 0 ];
			wishGIVertexAssociations[ vertexIndex ].weights[ 0 ] = 1.0f;
			for ( linkIndex = 1; linkIndex < WISHGI_MAX_LINKS; linkIndex++ )
			{
				wishGIVertexAssociations[ vertexIndex ].probeIndex[ linkIndex ] = bestProbeIndices[ 0 ];
				wishGIVertexAssociations[ vertexIndex ].weights[ linkIndex ] = 0.0f;
			}
			continue;
		}

		weightSum = 0.0f;
		for ( linkIndex = 0; linkIndex < linksPerVertex; linkIndex++ )
		{
			inverseDistance = 1.0f / ( sqrt( bestDistances[ linkIndex ] ) + 1.0e-6f );
			wishGIVertexAssociations[ vertexIndex ].probeIndex[ linkIndex ] = bestProbeIndices[ linkIndex ];
			wishGIVertexAssociations[ vertexIndex ].weights[ linkIndex ] = inverseDistance;
			weightSum += inverseDistance;
		}

		if ( weightSum > 1.0e-6f ) {
			for ( linkIndex = 0; linkIndex < linksPerVertex; linkIndex++ )
				wishGIVertexAssociations[ vertexIndex ].weights[ linkIndex ] /= weightSum;
		}
		else{
			wishGIVertexAssociations[ vertexIndex ].weights[ 0 ] = 1.0f;
			for ( linkIndex = 1; linkIndex < linksPerVertex; linkIndex++ )
				wishGIVertexAssociations[ vertexIndex ].weights[ linkIndex ] = 0.0f;
		}

		for ( linkIndex = linksPerVertex; linkIndex < WISHGI_MAX_LINKS; linkIndex++ )
		{
			wishGIVertexAssociations[ vertexIndex ].probeIndex[ linkIndex ] = wishGIVertexAssociations[ vertexIndex ].probeIndex[ 0 ];
			wishGIVertexAssociations[ vertexIndex ].weights[ linkIndex ] = 0.0f;
		}
	}

	averageNearestDistance /= numBSPDrawVerts;
	Sys_Printf( "WishGI: inverse probe distribution complete (avg nearest distance %.3f)\n", averageNearestDistance );
}

static qboolean WishGI_SaveAssociations( const char *associationPath ){
	FILE *file;
	int probeIndex, vertexIndex, linkIndex;

	if ( associationPath == NULL || associationPath[ 0 ] == '\0' ) {
		return qfalse;
	}
	if ( wishGIVertexAssociations == NULL || wishGIProbePositions == NULL ) {
		return qfalse;
	}

	file = fopen( associationPath, "w" );
	if ( file == NULL ) {
		Sys_Printf( "WARNING: WishGI failed to open %s for writing\n", associationPath );
		return qfalse;
	}

	fprintf( file, "%s %d %d %d\n", WISHGI_ASSOC_MAGIC, numBSPDrawVerts, wishGIActualProbeCount, wishGIActualLinksPerVertex );
	for ( probeIndex = 0; probeIndex < wishGIActualProbeCount; probeIndex++ )
		fprintf( file, "%.9g %.9g %.9g\n",
				 wishGIProbePositions[ probeIndex ][ 0 ],
				 wishGIProbePositions[ probeIndex ][ 1 ],
				 wishGIProbePositions[ probeIndex ][ 2 ] );

	for ( vertexIndex = 0; vertexIndex < numBSPDrawVerts; vertexIndex++ )
	{
		for ( linkIndex = 0; linkIndex < wishGIActualLinksPerVertex; linkIndex++ )
			fprintf( file, "%hu %.9g ",
					 wishGIVertexAssociations[ vertexIndex ].probeIndex[ linkIndex ],
					 wishGIVertexAssociations[ vertexIndex ].weights[ linkIndex ] );
		fprintf( file, "\n" );
	}

	fclose( file );
	Sys_Printf( "WishGI: wrote probe association data to %s\n", associationPath );
	return qtrue;
}

static qboolean WishGI_LoadAssociations( const char *associationPath ){
	FILE *file;
	char magic[ 64 ];
	int storedVertCount, storedProbeCount, storedLinksPerVertex;
	int probeIndex, vertexIndex, linkIndex;
	int parsedProbeIndex;
	float parsedWeight;
	float weightSum;

	if ( associationPath == NULL || associationPath[ 0 ] == '\0' ) {
		return qfalse;
	}

	file = fopen( associationPath, "r" );
	if ( file == NULL ) {
		Sys_Printf( "WishGI: association file not found: %s\n", associationPath );
		return qfalse;
	}

	if ( fscanf( file, "%63s %d %d %d", magic, &storedVertCount, &storedProbeCount, &storedLinksPerVertex ) != 4 ) {
		Sys_Printf( "WARNING: WishGI invalid association header in %s\n", associationPath );
		fclose( file );
		return qfalse;
	}

	if ( strcmp( magic, WISHGI_ASSOC_MAGIC ) != 0 ) {
		Sys_Printf( "WARNING: WishGI unknown association format in %s\n", associationPath );
		fclose( file );
		return qfalse;
	}
	if ( storedVertCount != numBSPDrawVerts ) {
		Sys_Printf( "WARNING: WishGI association vertex count mismatch (%d in file, %d in map)\n",
					storedVertCount, numBSPDrawVerts );
		fclose( file );
		return qfalse;
	}
	if ( storedProbeCount <= 0 || storedLinksPerVertex <= 0 || storedLinksPerVertex > WISHGI_MAX_LINKS ) {
		Sys_Printf( "WARNING: WishGI invalid association dimensions in %s\n", associationPath );
		fclose( file );
		return qfalse;
	}

	WishGI_ClearAssociations();
	wishGIActualProbeCount = storedProbeCount;
	wishGIActualLinksPerVertex = storedLinksPerVertex;
	wishGIProbePositions = safe_malloc( wishGIActualProbeCount * sizeof( *wishGIProbePositions ) );
	wishGIVertexAssociations = safe_malloc( numBSPDrawVerts * sizeof( *wishGIVertexAssociations ) );
	memset( wishGIVertexAssociations, 0, numBSPDrawVerts * sizeof( *wishGIVertexAssociations ) );

	for ( probeIndex = 0; probeIndex < wishGIActualProbeCount; probeIndex++ )
	{
		if ( fscanf( file, "%f %f %f",
					 &wishGIProbePositions[ probeIndex ][ 0 ],
					 &wishGIProbePositions[ probeIndex ][ 1 ],
					 &wishGIProbePositions[ probeIndex ][ 2 ] ) != 3 ) {
			Sys_Printf( "WARNING: WishGI failed to parse probe position %d in %s\n", probeIndex, associationPath );
			fclose( file );
			WishGI_ClearAssociations();
			return qfalse;
		}
	}

	for ( vertexIndex = 0; vertexIndex < numBSPDrawVerts; vertexIndex++ )
	{
		weightSum = 0.0f;
		for ( linkIndex = 0; linkIndex < wishGIActualLinksPerVertex; linkIndex++ )
		{
			if ( fscanf( file, "%d %f", &parsedProbeIndex, &parsedWeight ) != 2 ) {
				Sys_Printf( "WARNING: WishGI failed to parse association for vertex %d in %s\n", vertexIndex, associationPath );
				fclose( file );
				WishGI_ClearAssociations();
				return qfalse;
			}

			parsedProbeIndex = WishGI_ClampInt( parsedProbeIndex, 0, wishGIActualProbeCount - 1 );
			if ( parsedWeight < 0.0f ) {
				parsedWeight = 0.0f;
			}

			wishGIVertexAssociations[ vertexIndex ].probeIndex[ linkIndex ] = parsedProbeIndex;
			wishGIVertexAssociations[ vertexIndex ].weights[ linkIndex ] = parsedWeight;
			weightSum += parsedWeight;
		}

		if ( weightSum > 1.0e-6f ) {
			for ( linkIndex = 0; linkIndex < wishGIActualLinksPerVertex; linkIndex++ )
				wishGIVertexAssociations[ vertexIndex ].weights[ linkIndex ] /= weightSum;
		}
		else{
			wishGIVertexAssociations[ vertexIndex ].weights[ 0 ] = 1.0f;
			for ( linkIndex = 1; linkIndex < wishGIActualLinksPerVertex; linkIndex++ )
				wishGIVertexAssociations[ vertexIndex ].weights[ linkIndex ] = 0.0f;
		}

		for ( linkIndex = wishGIActualLinksPerVertex; linkIndex < WISHGI_MAX_LINKS; linkIndex++ )
		{
			wishGIVertexAssociations[ vertexIndex ].probeIndex[ linkIndex ] = wishGIVertexAssociations[ vertexIndex ].probeIndex[ 0 ];
			wishGIVertexAssociations[ vertexIndex ].weights[ linkIndex ] = 0.0f;
		}
	}

	fclose( file );
	Sys_Printf( "WishGI: loaded probe association data from %s\n", associationPath );
	return qtrue;
}

static void WishGI_PrepareAssociations( const char *bspFilePath ){
	char associationPath[ 1024 ];
	int probeCount, linksPerVertex;

	if ( !wishGIOptions.enabled ) {
		return;
	}

	if ( numBSPDrawVerts <= 0 || yDrawVerts == NULL ) {
		Sys_Printf( "WishGI: no draw vertices available, skipping.\n" );
		return;
	}

	if ( wishGIOptions.associationFile[ 0 ] != '\0' ) {
		strcpy( associationPath, wishGIOptions.associationFile );
	}
	else{
		WishGI_DefaultPath( bspFilePath, WISHGI_ASSOC_DEFAULT_SUFFIX, associationPath, sizeof( associationPath ) );
	}

	if ( wishGIOptions.loadAssociations && WishGI_LoadAssociations( associationPath ) ) {
		return;
	}

	probeCount = WishGI_ClampInt( wishGIOptions.probeCount, 1, numBSPDrawVerts );
	linksPerVertex = WishGI_ClampInt( wishGIOptions.linksPerVertex, 1, WISHGI_MAX_LINKS );
	if ( linksPerVertex > probeCount ) {
		linksPerVertex = probeCount;
	}

	WishGI_ClearAssociations();
	wishGIActualProbeCount = probeCount;
	wishGIActualLinksPerVertex = linksPerVertex;
	wishGIProbePositions = safe_malloc( wishGIActualProbeCount * sizeof( *wishGIProbePositions ) );
	wishGIVertexAssociations = safe_malloc( numBSPDrawVerts * sizeof( *wishGIVertexAssociations ) );
	memset( wishGIVertexAssociations, 0, numBSPDrawVerts * sizeof( *wishGIVertexAssociations ) );

	Sys_Printf( "WishGI: generating inverse probe distribution (%d probes, %d links, %d refine iterations)\n",
				wishGIActualProbeCount, wishGIActualLinksPerVertex, wishGIOptions.refinementIterations );

	WishGI_InitializeProbePositions( wishGIActualProbeCount, wishGIProbePositions );
	if ( wishGIOptions.refinementIterations > 0 ) {
		WishGI_RefineProbePositions( wishGIActualProbeCount, wishGIProbePositions );
	}
	WishGI_BuildVertexAssociations( wishGIActualLinksPerVertex );

	if ( wishGIOptions.saveAssociations ) {
		WishGI_SaveAssociations( associationPath );
	}
}

static qboolean WishGI_DumpProbeColors( const char *bspFilePath ){
	char dumpPath[ 1024 ];
	FILE *file;
	double *sumR, *sumG, *sumB, *sumWeight;
	int vertexIndex, probeIndex, linkIndex;
	int associatedProbe;
	float associatedWeight;
	float r, g, b;

	if ( !wishGIOptions.enabled || !wishGIOptions.dumpProbeColors ) {
		return qfalse;
	}
	if ( wishGIVertexAssociations == NULL || wishGIProbePositions == NULL ) {
		Sys_Printf( "WishGI: cannot dump probe colors without associations.\n" );
		return qfalse;
	}

	if ( wishGIOptions.probeDumpFile[ 0 ] != '\0' ) {
		strcpy( dumpPath, wishGIOptions.probeDumpFile );
	}
	else{
		WishGI_DefaultPath( bspFilePath, WISHGI_PROBES_DEFAULT_SUFFIX, dumpPath, sizeof( dumpPath ) );
	}

	sumR = safe_malloc( wishGIActualProbeCount * sizeof( *sumR ) );
	sumG = safe_malloc( wishGIActualProbeCount * sizeof( *sumG ) );
	sumB = safe_malloc( wishGIActualProbeCount * sizeof( *sumB ) );
	sumWeight = safe_malloc( wishGIActualProbeCount * sizeof( *sumWeight ) );
	memset( sumR, 0, wishGIActualProbeCount * sizeof( *sumR ) );
	memset( sumG, 0, wishGIActualProbeCount * sizeof( *sumG ) );
	memset( sumB, 0, wishGIActualProbeCount * sizeof( *sumB ) );
	memset( sumWeight, 0, wishGIActualProbeCount * sizeof( *sumWeight ) );

	for ( vertexIndex = 0; vertexIndex < numBSPDrawVerts; vertexIndex++ )
	{
		r = yDrawVerts[ vertexIndex ].color[ 0 ][ 0 ] / 255.0f;
		g = yDrawVerts[ vertexIndex ].color[ 0 ][ 1 ] / 255.0f;
		b = yDrawVerts[ vertexIndex ].color[ 0 ][ 2 ] / 255.0f;

		for ( linkIndex = 0; linkIndex < wishGIActualLinksPerVertex; linkIndex++ )
		{
			associatedProbe = wishGIVertexAssociations[ vertexIndex ].probeIndex[ linkIndex ];
			associatedWeight = wishGIVertexAssociations[ vertexIndex ].weights[ linkIndex ];

			if ( associatedProbe < 0 || associatedProbe >= wishGIActualProbeCount || associatedWeight <= 0.0f ) {
				continue;
			}

			sumR[ associatedProbe ] += (double) ( r * associatedWeight );
			sumG[ associatedProbe ] += (double) ( g * associatedWeight );
			sumB[ associatedProbe ] += (double) ( b * associatedWeight );
			sumWeight[ associatedProbe ] += (double) associatedWeight;
		}
	}

	file = fopen( dumpPath, "w" );
	if ( file == NULL ) {
		Sys_Printf( "WARNING: WishGI failed to open %s for writing\n", dumpPath );
		free( sumR );
		free( sumG );
		free( sumB );
		free( sumWeight );
		return qfalse;
	}

	fprintf( file, "wishgi_probe_colors_v1 %d\n", wishGIActualProbeCount );
	for ( probeIndex = 0; probeIndex < wishGIActualProbeCount; probeIndex++ )
	{
		if ( sumWeight[ probeIndex ] > 1.0e-12 ) {
			r = (float) ( sumR[ probeIndex ] / sumWeight[ probeIndex ] );
			g = (float) ( sumG[ probeIndex ] / sumWeight[ probeIndex ] );
			b = (float) ( sumB[ probeIndex ] / sumWeight[ probeIndex ] );
		}
		else{
			r = g = b = 0.0f;
		}

		fprintf( file, "%d %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",
				 probeIndex,
				 wishGIProbePositions[ probeIndex ][ 0 ],
				 wishGIProbePositions[ probeIndex ][ 1 ],
				 wishGIProbePositions[ probeIndex ][ 2 ],
				 r, g, b,
				 (float) sumWeight[ probeIndex ] );
	}

	fclose( file );
	free( sumR );
	free( sumG );
	free( sumB );
	free( sumWeight );

	Sys_Printf( "WishGI: wrote probe color fit dump to %s\n", dumpPath );
	return qtrue;
}

static int WishGI_SHBasisCount( int shOrder ){
	if ( shOrder <= 0 ) {
		return 1;
	}
	if ( shOrder == 1 ) {
		return 4;
	}
	if ( shOrder == 2 ) {
		return 9;
	}
	return 16;
}

static void WishGI_EvaluateSHBasis( const vec3_t direction, int shOrder, float *basis ){
	float x, y, z, xx, yy, zz;
	int i;

	for ( i = 0; i < WISHGI_MAX_SH_COEFFS; i++ )
		basis[ i ] = 0.0f;

	x = direction[ 0 ];
	y = direction[ 1 ];
	z = direction[ 2 ];
	xx = x * x;
	yy = y * y;
	zz = z * z;

	/* Real SH basis up to l=3 (common graphics convention). */
	basis[ 0 ] = 0.2820947918f;
	if ( shOrder < 1 ) {
		return;
	}

	basis[ 1 ] = 0.4886025119f * y;
	basis[ 2 ] = 0.4886025119f * z;
	basis[ 3 ] = 0.4886025119f * x;
	if ( shOrder < 2 ) {
		return;
	}

	basis[ 4 ] = 1.0925484306f * x * y;
	basis[ 5 ] = 1.0925484306f * y * z;
	basis[ 6 ] = 0.3153915653f * ( 3.0f * zz - 1.0f );
	basis[ 7 ] = 1.0925484306f * x * z;
	basis[ 8 ] = 0.5462742153f * ( xx - yy );
	if ( shOrder < 3 ) {
		return;
	}

	basis[ 9 ] = 0.5900435899f * y * ( 3.0f * xx - yy );
	basis[ 10 ] = 2.8906114426f * x * y * z;
	basis[ 11 ] = 0.4570457995f * y * ( 5.0f * zz - 1.0f );
	basis[ 12 ] = 0.3731763325f * z * ( 5.0f * zz - 3.0f );
	basis[ 13 ] = 0.4570457995f * x * ( 5.0f * zz - 1.0f );
	basis[ 14 ] = 1.4453057213f * z * ( xx - yy );
	basis[ 15 ] = 0.5900435899f * x * ( xx - 3.0f * yy );
}

static qboolean WishGI_SolveLinearSystem( int size, const double *matrix, const double *rhs, double *solution ){
	double *a, *b;
	double pivotAbs, pivotValue, factor, temp, sum;
	int row, col, pivotRow, swapCol;

	a = safe_malloc( size * size * sizeof( *a ) );
	b = safe_malloc( size * sizeof( *b ) );
	memcpy( a, matrix, size * size * sizeof( *a ) );
	memcpy( b, rhs, size * sizeof( *b ) );

	for ( row = 0; row < size; row++ )
	{
		pivotRow = row;
		pivotAbs = fabs( a[ row * size + row ] );
		for ( col = row + 1; col < size; col++ )
		{
			temp = fabs( a[ col * size + row ] );
			if ( temp > pivotAbs ) {
				pivotAbs = temp;
				pivotRow = col;
			}
		}

		if ( pivotAbs < 1.0e-12 ) {
			free( a );
			free( b );
			return qfalse;
		}

		if ( pivotRow != row ) {
			for ( swapCol = row; swapCol < size; swapCol++ )
			{
				temp = a[ row * size + swapCol ];
				a[ row * size + swapCol ] = a[ pivotRow * size + swapCol ];
				a[ pivotRow * size + swapCol ] = temp;
			}

			temp = b[ row ];
			b[ row ] = b[ pivotRow ];
			b[ pivotRow ] = temp;
		}

		pivotValue = a[ row * size + row ];
		for ( col = row + 1; col < size; col++ )
		{
			factor = a[ col * size + row ] / pivotValue;
			if ( fabs( factor ) < 1.0e-20 ) {
				continue;
			}

			a[ col * size + row ] = 0.0;
			for ( swapCol = row + 1; swapCol < size; swapCol++ )
				a[ col * size + swapCol ] -= factor * a[ row * size + swapCol ];
			b[ col ] -= factor * b[ row ];
		}
	}

	for ( row = size - 1; row >= 0; row-- )
	{
		sum = b[ row ];
		for ( col = row + 1; col < size; col++ )
			sum -= a[ row * size + col ] * solution[ col ];

		if ( fabs( a[ row * size + row ] ) < 1.0e-12 ) {
			free( a );
			free( b );
			return qfalse;
		}

		solution[ row ] = sum / a[ row * size + row ];
	}

	free( a );
	free( b );
	return qtrue;
}

static qboolean WishGI_DumpProbeSHCoefficients( const char *bspFilePath ){
	char dumpPath[ 1024 ];
	FILE *file;
	double *normalMatrices, *rhsR, *rhsG, *rhsB, *sampleWeights;
	double matrixCopy[ WISHGI_MAX_SH_COEFFS * WISHGI_MAX_SH_COEFFS ];
	double rhsCopy[ WISHGI_MAX_SH_COEFFS ];
	double shR[ WISHGI_MAX_SH_COEFFS ], shG[ WISHGI_MAX_SH_COEFFS ], shB[ WISHGI_MAX_SH_COEFFS ];
	float basis[ WISHGI_MAX_SH_COEFFS ];
	float sampleR, sampleG, sampleB;
	float directionLength, solveLambda;
	qboolean solveOkR, solveOkG, solveOkB;
	int basisCount;
	int vertexIndex, probeIndex, linkIndex, coeffA, coeffB;
	int matrixBase, rhsBase;
	int associatedProbe;
	float associatedWeight;
	int failedProbeCount;
	vec3_t normal;

	if ( !wishGIOptions.enabled || !wishGIOptions.dumpProbeSH ) {
		return qfalse;
	}
	if ( wishGIVertexAssociations == NULL || wishGIProbePositions == NULL ) {
		Sys_Printf( "WishGI: cannot dump SH coefficients without associations.\n" );
		return qfalse;
	}

	wishGIOptions.shOrder = WishGI_ClampInt( wishGIOptions.shOrder, 0, 3 );
	basisCount = WishGI_SHBasisCount( wishGIOptions.shOrder );
	solveLambda = wishGIOptions.shRegularization;
	if ( solveLambda <= 0.0f ) {
		solveLambda = 0.001f;
	}

	if ( wishGIOptions.probeSHDumpFile[ 0 ] != '\0' ) {
		strcpy( dumpPath, wishGIOptions.probeSHDumpFile );
	}
	else{
		WishGI_DefaultPath( bspFilePath, WISHGI_PROBES_SH_DEFAULT_SUFFIX, dumpPath, sizeof( dumpPath ) );
	}

	normalMatrices = safe_malloc( wishGIActualProbeCount * basisCount * basisCount * sizeof( *normalMatrices ) );
	rhsR = safe_malloc( wishGIActualProbeCount * basisCount * sizeof( *rhsR ) );
	rhsG = safe_malloc( wishGIActualProbeCount * basisCount * sizeof( *rhsG ) );
	rhsB = safe_malloc( wishGIActualProbeCount * basisCount * sizeof( *rhsB ) );
	sampleWeights = safe_malloc( wishGIActualProbeCount * sizeof( *sampleWeights ) );
	memset( normalMatrices, 0, wishGIActualProbeCount * basisCount * basisCount * sizeof( *normalMatrices ) );
	memset( rhsR, 0, wishGIActualProbeCount * basisCount * sizeof( *rhsR ) );
	memset( rhsG, 0, wishGIActualProbeCount * basisCount * sizeof( *rhsG ) );
	memset( rhsB, 0, wishGIActualProbeCount * basisCount * sizeof( *rhsB ) );
	memset( sampleWeights, 0, wishGIActualProbeCount * sizeof( *sampleWeights ) );

	for ( vertexIndex = 0; vertexIndex < numBSPDrawVerts; vertexIndex++ )
	{
		VectorCopy( yDrawVerts[ vertexIndex ].normal, normal );
		directionLength = VectorNormalize( normal, normal );
		if ( directionLength <= 1.0e-6f ) {
			continue;
		}

		WishGI_EvaluateSHBasis( normal, wishGIOptions.shOrder, basis );
		sampleR = yDrawVerts[ vertexIndex ].color[ 0 ][ 0 ] / 255.0f;
		sampleG = yDrawVerts[ vertexIndex ].color[ 0 ][ 1 ] / 255.0f;
		sampleB = yDrawVerts[ vertexIndex ].color[ 0 ][ 2 ] / 255.0f;

		for ( linkIndex = 0; linkIndex < wishGIActualLinksPerVertex; linkIndex++ )
		{
			associatedProbe = wishGIVertexAssociations[ vertexIndex ].probeIndex[ linkIndex ];
			associatedWeight = wishGIVertexAssociations[ vertexIndex ].weights[ linkIndex ];
			if ( associatedProbe < 0 || associatedProbe >= wishGIActualProbeCount || associatedWeight <= 0.0f ) {
				continue;
			}

			matrixBase = associatedProbe * basisCount * basisCount;
			rhsBase = associatedProbe * basisCount;
			sampleWeights[ associatedProbe ] += associatedWeight;

			for ( coeffA = 0; coeffA < basisCount; coeffA++ )
			{
				rhsR[ rhsBase + coeffA ] += associatedWeight * basis[ coeffA ] * sampleR;
				rhsG[ rhsBase + coeffA ] += associatedWeight * basis[ coeffA ] * sampleG;
				rhsB[ rhsBase + coeffA ] += associatedWeight * basis[ coeffA ] * sampleB;

				for ( coeffB = 0; coeffB < basisCount; coeffB++ )
					normalMatrices[ matrixBase + coeffA * basisCount + coeffB ] += associatedWeight * basis[ coeffA ] * basis[ coeffB ];
			}
		}
	}

	file = fopen( dumpPath, "w" );
	if ( file == NULL ) {
		Sys_Printf( "WARNING: WishGI failed to open %s for SH coefficient output\n", dumpPath );
		free( normalMatrices );
		free( rhsR );
		free( rhsG );
		free( rhsB );
		free( sampleWeights );
		return qfalse;
	}

	fprintf( file, "wishgi_probe_sh_v1 %d %d %d\n", wishGIActualProbeCount, wishGIOptions.shOrder, basisCount );
	fprintf( file, "# probe x y z sampleWeight then (r g b) triplets for each SH coefficient\n" );

	failedProbeCount = 0;
	for ( probeIndex = 0; probeIndex < wishGIActualProbeCount; probeIndex++ )
	{
		matrixBase = probeIndex * basisCount * basisCount;
		rhsBase = probeIndex * basisCount;

		for ( coeffA = 0; coeffA < basisCount; coeffA++ )
			for ( coeffB = 0; coeffB < basisCount; coeffB++ )
				matrixCopy[ coeffA * basisCount + coeffB ] = normalMatrices[ matrixBase + coeffA * basisCount + coeffB ];

		for ( coeffA = 0; coeffA < basisCount; coeffA++ )
			matrixCopy[ coeffA * basisCount + coeffA ] += solveLambda * ( sampleWeights[ probeIndex ] + 1.0 );

		for ( coeffA = 0; coeffA < basisCount; coeffA++ ) {
			rhsCopy[ coeffA ] = rhsR[ rhsBase + coeffA ];
			shR[ coeffA ] = 0.0;
		}
		solveOkR = WishGI_SolveLinearSystem( basisCount, matrixCopy, rhsCopy, shR );

		for ( coeffA = 0; coeffA < basisCount; coeffA++ )
			for ( coeffB = 0; coeffB < basisCount; coeffB++ )
				matrixCopy[ coeffA * basisCount + coeffB ] = normalMatrices[ matrixBase + coeffA * basisCount + coeffB ];
		for ( coeffA = 0; coeffA < basisCount; coeffA++ )
			matrixCopy[ coeffA * basisCount + coeffA ] += solveLambda * ( sampleWeights[ probeIndex ] + 1.0 );
		for ( coeffA = 0; coeffA < basisCount; coeffA++ ) {
			rhsCopy[ coeffA ] = rhsG[ rhsBase + coeffA ];
			shG[ coeffA ] = 0.0;
		}
		solveOkG = WishGI_SolveLinearSystem( basisCount, matrixCopy, rhsCopy, shG );

		for ( coeffA = 0; coeffA < basisCount; coeffA++ )
			for ( coeffB = 0; coeffB < basisCount; coeffB++ )
				matrixCopy[ coeffA * basisCount + coeffB ] = normalMatrices[ matrixBase + coeffA * basisCount + coeffB ];
		for ( coeffA = 0; coeffA < basisCount; coeffA++ )
			matrixCopy[ coeffA * basisCount + coeffA ] += solveLambda * ( sampleWeights[ probeIndex ] + 1.0 );
		for ( coeffA = 0; coeffA < basisCount; coeffA++ ) {
			rhsCopy[ coeffA ] = rhsB[ rhsBase + coeffA ];
			shB[ coeffA ] = 0.0;
		}
		solveOkB = WishGI_SolveLinearSystem( basisCount, matrixCopy, rhsCopy, shB );

		if ( !solveOkR || !solveOkG || !solveOkB ) {
			failedProbeCount++;
			for ( coeffA = 0; coeffA < basisCount; coeffA++ ) {
				shR[ coeffA ] = 0.0;
				shG[ coeffA ] = 0.0;
				shB[ coeffA ] = 0.0;
			}
		}

		fprintf( file, "%d %.9g %.9g %.9g %.9g",
				 probeIndex,
				 wishGIProbePositions[ probeIndex ][ 0 ],
				 wishGIProbePositions[ probeIndex ][ 1 ],
				 wishGIProbePositions[ probeIndex ][ 2 ],
				 (float) sampleWeights[ probeIndex ] );

		for ( coeffA = 0; coeffA < basisCount; coeffA++ )
			fprintf( file, " %.9g %.9g %.9g", (float) shR[ coeffA ], (float) shG[ coeffA ], (float) shB[ coeffA ] );
		fprintf( file, "\n" );
	}

	fclose( file );
	free( normalMatrices );
	free( rhsR );
	free( rhsG );
	free( rhsB );
	free( sampleWeights );

	Sys_Printf( "WishGI: wrote SH coefficient fit dump to %s (%d coefficient set(s), order %d)\n",
				dumpPath, wishGIActualProbeCount, wishGIOptions.shOrder );
	if ( failedProbeCount > 0 ) {
		Sys_Printf( "WishGI: SH fit fallback to zero for %d probe(s) due to degenerate solves.\n", failedProbeCount );
	}
	return qtrue;
}

static qboolean ParseLightmapFormatString( const char *value ){
	if ( value == NULL || value[ 0 ] == '\0' ) {
		return qfalse;
	}

	if ( !Q_stricmp( value, "tga" ) ) {
		lightmapFileFormat = LIGHTMAP_FILEFORMAT_TGA;
		return qtrue;
	}
	if ( !Q_stricmp( value, "hdr" ) ) {
		lightmapFileFormat = LIGHTMAP_FILEFORMAT_HDR;
		return qtrue;
	}
	if ( !Q_stricmp( value, "exr" ) ) {
		lightmapFileFormat = LIGHTMAP_FILEFORMAT_EXR;
		return qtrue;
	}

	return qfalse;
}

int LightMain( int argc, char **argv ){
	int i;
	float f;
	char BSPFilePath[ 1024 ];
	char surfaceFilePath[ 1024 ];
	BSPFilePath[0] = 0;
	surfaceFilePath[0] = 0;
	const char  *value;
	int lightmapMergeSize = 0;
	qboolean lightSamplesInsist = qfalse;
	qboolean fastAllocate = qfalse;


	/* note it */
	Sys_Printf( "--- Light ---\n" );
	Sys_Printf( "--- ProcessGameSpecific ---\n" );

	/* set standard game flags */
	wolfLight = game->wolfLight;
	if ( wolfLight == qtrue ) {
		Sys_Printf( " lightning model: wolf\n" );
	}
	else{
		Sys_Printf( " lightning model: quake3\n" );
	}

	lmCustomSize = game->lightmapSize;
	Sys_Printf( " lightmap size: %d x %d pixels\n", lmCustomSize, lmCustomSize );

	lightmapGamma = game->lightmapGamma;
	Sys_Printf( " lightning gamma: %f\n", lightmapGamma );

	lightmapsRGB = game->lightmapsRGB;
	if ( lightmapsRGB ) {
		Sys_Printf( " lightmap colorspace: sRGB\n" );
	}
	else{
		Sys_Printf( " lightmap colorspace: linear\n" );
	}

	texturesRGB = game->texturesRGB;
	if ( texturesRGB ) {
		Sys_Printf( " texture colorspace: sRGB\n" );
	}
	else{
		Sys_Printf( " texture colorspace: linear\n" );
	}

	colorsRGB = game->colorsRGB;
	if ( colorsRGB ) {
		Sys_Printf( " _color colorspace: sRGB\n" );
	}
	else{
		Sys_Printf( " _color colorspace: linear\n" );
	}

	lightmapCompensate = game->lightmapCompensate;
	Sys_Printf( " lightning compensation: %f\n", lightmapCompensate );

	lightmapExposure = game->lightmapExposure;
	Sys_Printf( " lightning exposure: %f\n", lightmapExposure );

	gridScale = game->gridScale;
	Sys_Printf( " lightgrid scale: %f\n", gridScale );

	gridAmbientScale = game->gridAmbientScale;
	Sys_Printf( " lightgrid ambient scale: %f\n", gridAmbientScale );

	lightAngleHL = game->lightAngleHL;
	if ( lightAngleHL ) {
		Sys_Printf( " half lambert light angle attenuation enabled \n" );
	}

	noStyles = game->noStyles;
	if ( noStyles == qtrue ) {
		Sys_Printf( " shader lightstyles hack: disabled\n" );
	}
	else{
		Sys_Printf( " shader lightstyles hack: enabled\n" );
	}

	patchShadows = game->patchShadows;
	if ( patchShadows == qtrue ) {
		Sys_Printf( " patch shadows: enabled\n" );
	}
	else{
		Sys_Printf( " patch shadows: disabled\n" );
	}

	deluxemap = game->deluxeMap;
	deluxemode = game->deluxeMode;
	if ( deluxemap == qtrue ) {
		if ( deluxemode ) {
			Sys_Printf( " deluxemapping: enabled with tangentspace deluxemaps\n" );
		}
		else{
			Sys_Printf( " deluxemapping: enabled with modelspace deluxemaps\n" );
		}
	}
	else{
		Sys_Printf( " deluxemapping: disabled\n" );
	}

	if ( dirty ) {
		Sys_Printf( " ambient occlusion: enabled\n" );
	}
	else{
		Sys_Printf( " ambient occlusion: disabled\n" );
	}

	if ( floodlighty ) {
		Sys_Printf( " ibl ambient (floodlight): enabled\n" );
	}
	else{
		Sys_Printf( " ibl ambient (floodlight): disabled\n" );
	}

	Sys_Printf( "--- ProcessCommandLine ---\n" );

	/* process commandline arguments */
	for ( i = 1; i < ( argc - 1 ); i++ )
	{
		/* lightsource scaling */
		if ( !strcmp( argv[ i ], "-point" ) || !strcmp( argv[ i ], "-pointscale" ) ) {
			f = atof( argv[ i + 1 ] );
			pointScale *= f;
			spotScale *= f;
			Sys_Printf( "Spherical point (entity) light scaled by %f to %f\n", f, pointScale );
			Sys_Printf( "Spot point (entity) light scaled by %f to %f\n", f, spotScale );
			i++;
		}

		else if ( !strcmp( argv[ i ], "-spherical" ) || !strcmp( argv[ i ], "-sphericalscale" ) ) {
			f = atof( argv[ i + 1 ] );
			pointScale *= f;
			Sys_Printf( "Spherical point (entity) light scaled by %f to %f\n", f, pointScale );
			i++;
		}

		else if ( !strcmp( argv[ i ], "-spot" ) || !strcmp( argv[ i ], "-spotscale" ) ) {
			f = atof( argv[ i + 1 ] );
			spotScale *= f;
			Sys_Printf( "Spot point (entity) light scaled by %f to %f\n", f, spotScale );
			i++;
		}

		else if ( !strcmp( argv[ i ], "-area" ) || !strcmp( argv[ i ], "-areascale" ) ) {
			f = atof( argv[ i + 1 ] );
			areaScale *= f;
			Sys_Printf( "Area (shader) light scaled by %f to %f\n", f, areaScale );
			i++;
		}

		else if ( !strcmp( argv[ i ], "-sky" ) || !strcmp( argv[ i ], "-skyscale" ) ) {
			f = atof( argv[ i + 1 ] );
			skyScale *= f;
			Sys_Printf( "Sky/sun light scaled by %f to %f\n", f, skyScale );
			i++;
		}
		else if ( !strcmp( argv[ i ], "-skylightmode" ) ) {
			skyLightMode_t parsedMode;
			if ( i + 1 >= argc ) {
				Error( "Missing mode for -skylightmode (expected: auto legacy shader panorama)" );
			}
			parsedMode = SkyLightMode_Parse( argv[ i + 1 ] );
			if ( parsedMode == SKYLIGHT_MODE_AUTO && Q_stricmp( argv[ i + 1 ], "auto" ) ) {
				Error( "Invalid -skylightmode '%s' (expected: auto legacy shader panorama)", argv[ i + 1 ] );
			}
			g_skyLightMode = parsedMode;
			Sys_Printf( "Skylight panorama mode set to %s\n", SkyLightMode_ToString( g_skyLightMode ) );
			i++;
		}
		else if ( !strcmp( argv[ i ], "-skylightlegacy" ) ) {
			g_skyLightMode = SKYLIGHT_MODE_LEGACY;
			Sys_Printf( "Skylight panorama mode set to %s\n", SkyLightMode_ToString( g_skyLightMode ) );
		}
		else if ( !strcmp( argv[ i ], "-skylightshader" ) ) {
			g_skyLightMode = SKYLIGHT_MODE_SHADER;
			Sys_Printf( "Skylight panorama mode set to %s\n", SkyLightMode_ToString( g_skyLightMode ) );
		}
		else if ( !strcmp( argv[ i ], "-skylightpanorama" ) || !strcmp( argv[ i ], "-skypanorama" ) ) {
			if ( i + 1 >= argc ) {
				Error( "Missing file path for -skylightpanorama" );
			}
			strcpy( g_skyLightPanoramaPath, argv[ i + 1 ] );
			Sys_Printf( "Skylight panorama source set to %s\n", g_skyLightPanoramaPath );
			i++;
		}
		else if ( !strcmp( argv[ i ], "-skylightpanoramafacesize" ) || !strcmp( argv[ i ], "-skypanofacesize" ) ) {
			if ( i + 1 >= argc ) {
				Error( "Missing integer for -skylightpanoramafacesize" );
			}
			g_skyLightPanoramaFaceSize = atoi( argv[ i + 1 ] );
			if ( g_skyLightPanoramaFaceSize < 16 ) {
				g_skyLightPanoramaFaceSize = 16;
			}
			else if ( g_skyLightPanoramaFaceSize > 4096 ) {
				g_skyLightPanoramaFaceSize = 4096;
			}
			Sys_Printf( "Skylight panorama conversion face size set to %d\n", g_skyLightPanoramaFaceSize );
			i++;
		}
		else if ( !strcmp( argv[ i ], "-skylightpanoramaconvert" ) || !strcmp( argv[ i ], "-skypanoconvert" ) ) {
			if ( i + 1 >= argc ) {
				Error( "Missing output base path for -skylightpanoramaconvert" );
			}
			strcpy( g_skyLightPanoramaConvertBase, argv[ i + 1 ] );
			Sys_Printf( "Skylight panorama cube-face export base set to %s\n", g_skyLightPanoramaConvertBase );
			i++;
		}
		else if ( !strcmp( argv[ i ], "-skylightauto" ) ) {
			g_skyLightMode = SKYLIGHT_MODE_AUTO;
			Sys_Printf( "Skylight panorama mode set to %s\n", SkyLightMode_ToString( g_skyLightMode ) );
		}

		else if ( !strcmp( argv[ i ], "-bouncescale" ) ) {
			f = atof( argv[ i + 1 ] );
			bounceScale *= f;
			Sys_Printf( "Bounce (radiosity) light scaled by %f to %f\n", f, bounceScale );
			i++;
		}

		else if ( !strcmp( argv[ i ], "-scale" ) ) {
			f = atof( argv[ i + 1 ] );
			pointScale *= f;
			spotScale *= f;
			areaScale *= f;
			skyScale *= f;
			bounceScale *= f;
			Sys_Printf( "All light scaled by %f\n", f );
			i++;
		}

		else if ( !strcmp( argv[ i ], "-gridscale" ) ) {
			f = atof( argv[ i + 1 ] );
			Sys_Printf( "Grid lightning scaled by %f\n", f );
			gridScale *= f;
			i++;
		}

		else if ( !strcmp( argv[ i ], "-gridambientscale" ) ) {
			f = atof( argv[ i + 1 ] );
			Sys_Printf( "Grid ambient lightning scaled by %f\n", f );
			gridAmbientScale *= f;
			i++;
		}

		else if ( !strcmp( argv[ i ], "-griddirectionality" ) ) {
			f = atof( argv[ i + 1 ] );
			if ( f > 1 ) {
				f = 1;
			}
			if ( f < gridAmbientDirectionality ) {
				gridAmbientDirectionality = f;
			}
			Sys_Printf( "Grid directionality is %f\n", f );
			gridDirectionality = f;
			i++;
		}

		else if ( !strcmp( argv[ i ], "-gridambientdirectionality" ) ) {
			f = atof( argv[ i + 1 ] );
			if ( f < -1 ) {
				f = -1;
			}
			if ( f > gridDirectionality ) {
				gridDirectionality = f;
			}
			Sys_Printf( "Grid ambient directionality is %f\n", f );
			gridAmbientDirectionality = f;
			i++;
		}

		else if ( !strcmp( argv[ i ], "-gamma" ) ) {
			f = atof( argv[ i + 1 ] );
			lightmapGamma = f;
			Sys_Printf( "Lighting gamma set to %f\n", lightmapGamma );
			i++;
		}

		else if ( !strcmp( argv[ i ], "-sRGBlight" ) ) {
			lightmapsRGB = qtrue;
			Sys_Printf( "Lighting is in sRGB\n" );
		}

		else if ( !strcmp( argv[ i ], "-nosRGBlight" ) ) {
			lightmapsRGB = qfalse;
			Sys_Printf( "Lighting is linear\n" );
		}

		else if ( !strcmp( argv[ i ], "-sRGBtex" ) ) {
			texturesRGB = qtrue;
			Sys_Printf( "Textures are in sRGB\n" );
		}

		else if ( !strcmp( argv[ i ], "-nosRGBtex" ) ) {
			texturesRGB = qfalse;
			Sys_Printf( "Textures are linear\n" );
		}

		else if ( !strcmp( argv[ i ], "-sRGBcolor" ) ) {
			colorsRGB = qtrue;
			Sys_Printf( "Colors are in sRGB\n" );
		}

		else if ( !strcmp( argv[ i ], "-nosRGBcolor" ) ) {
			colorsRGB = qfalse;
			Sys_Printf( "Colors are linear\n" );
		}

		else if ( !strcmp( argv[ i ], "-sRGB" ) ) {
			lightmapsRGB = qtrue;
			Sys_Printf( "Lighting is in sRGB\n" );
			texturesRGB = qtrue;
			Sys_Printf( "Textures are in sRGB\n" );
			colorsRGB = qtrue;
			Sys_Printf( "Colors are in sRGB\n" );
		}

		else if ( !strcmp( argv[ i ], "-nosRGB" ) ) {
			lightmapsRGB = qfalse;
			Sys_Printf( "Lighting is linear\n" );
			texturesRGB = qfalse;
			Sys_Printf( "Textures are linear\n" );
			colorsRGB = qfalse;
			Sys_Printf( "Colors are linear\n" );
		}

		else if ( !strcmp( argv[ i ], "-exposure" ) ) {
			f = atof( argv[ i + 1 ] );
			lightmapExposure = f;
			Sys_Printf( "Lighting exposure set to %f\n", lightmapExposure );
			i++;
		}

		else if ( !strcmp( argv[ i ], "-compensate" ) ) {
			f = atof( argv[ i + 1 ] );
			if ( f <= 0.0f ) {
				f = 1.0f;
			}
			lightmapCompensate = f;
			Sys_Printf( "Lighting compensation set to 1/%f\n", lightmapCompensate );
			i++;
		}

		/* ydnar switches */
		else if ( !strcmp( argv[ i ], "-bounce" ) ) {
			bounce = atoi( argv[ i + 1 ] );
			if ( bounce < 0 ) {
				bounce = 0;
			}
			else if ( bounce > 0 ) {
				Sys_Printf( "Radiosity enabled with %d bounce(s)\n", bounce );
			}
			i++;
		}

		else if ( !strcmp( argv[ i ], "-supersample" ) || !strcmp( argv[ i ], "-super" ) ) {
			superSample = atoi( argv[ i + 1 ] );
			if ( superSample < 1 ) {
				superSample = 1;
			}
			else if ( superSample > 1 ) {
				Sys_Printf( "Ordered-grid supersampling enabled with %d sample(s) per lightmap texel\n", ( superSample * superSample ) );
			}
			i++;
		}

		else if ( !strcmp( argv[ i ], "-randomsamples" ) ) {
			lightRandomSamples = qtrue;
			Sys_Printf( "Random sampling enabled\n", lightRandomSamples );
		}

		else if ( !strcmp( argv[ i ], "-samples" ) ) {
			if ( *argv[i + 1] == '+' ) {
				lightSamplesInsist = qtrue;
			}
			else{
				lightSamplesInsist = qfalse;
			}
			lightSamples = atoi( argv[ i + 1 ] );
			if ( lightSamples < 1 ) {
				lightSamples = 1;
			}
			else if ( lightSamples > 1 ) {
				Sys_Printf( "Adaptive supersampling enabled with %d sample(s) per lightmap texel\n", lightSamples );
			}
			i++;
		}

		else if ( !strcmp( argv[ i ], "-samplessearchboxsize" ) ) {
			lightSamplesSearchBoxSize = atoi( argv[ i + 1 ] );
			if ( lightSamplesSearchBoxSize <= 0 ) {
				lightSamplesSearchBoxSize = 1;
			}
			if ( lightSamplesSearchBoxSize > 4 ) {
				lightSamplesSearchBoxSize = 4; /* more makes no sense */
			}
			else if ( lightSamplesSearchBoxSize != 1 ) {
				Sys_Printf( "Adaptive supersampling uses %f times the normal search box size\n", lightSamplesSearchBoxSize );
			}
			i++;
		}

		else if ( !strcmp( argv[ i ], "-filter" ) ) {
			filter = qtrue;
			Sys_Printf( "Lightmap filtering enabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-dark" ) ) {
			dark = qtrue;
			Sys_Printf( "Dark lightmap seams enabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-shadeangle" ) ) {
			shadeAngleDegrees = atof( argv[ i + 1 ] );
			if ( shadeAngleDegrees < 0.0f ) {
				shadeAngleDegrees = 0.0f;
			}
			else if ( shadeAngleDegrees > 0.0f ) {
				shade = qtrue;
				Sys_Printf( "Phong shading enabled with a breaking angle of %f degrees\n", shadeAngleDegrees );
			}
			i++;
		}

		else if ( !strcmp( argv[ i ], "-thresh" ) ) {
			subdivideThreshold = atof( argv[ i + 1 ] );
			if ( subdivideThreshold < 0 ) {
				subdivideThreshold = DEFAULT_SUBDIVIDE_THRESHOLD;
			}
			else{
				Sys_Printf( "Subdivision threshold set at %.3f\n", subdivideThreshold );
			}
			i++;
		}

		else if ( !strcmp( argv[ i ], "-approx" ) ) {
			approximateTolerance = atoi( argv[ i + 1 ] );
			if ( approximateTolerance < 0 ) {
				approximateTolerance = 0;
			}
			else if ( approximateTolerance > 0 ) {
				Sys_Printf( "Approximating lightmaps within a byte tolerance of %d\n", approximateTolerance );
			}
			i++;
		}
		else if ( !strcmp( argv[ i ], "-deluxe" ) || !strcmp( argv[ i ], "-deluxemap" ) ) {
			deluxemap = qtrue;
			Sys_Printf( "Generating deluxemaps for average light direction\n" );
		}
		else if ( !strcmp( argv[ i ], "-deluxemode" ) ) {
			deluxemode = atoi( argv[ i + 1 ] );
			if ( deluxemode == 0 || deluxemode > 1 || deluxemode < 0 ) {
				Sys_Printf( "Generating modelspace deluxemaps\n" );
				deluxemode = 0;
			}
			else{
				Sys_Printf( "Generating tangentspace deluxemaps\n" );
			}
			i++;
		}
		else if ( !strcmp( argv[ i ], "-nodeluxe" ) || !strcmp( argv[ i ], "-nodeluxemap" ) ) {
			deluxemap = qfalse;
			Sys_Printf( "Disabling generating of deluxemaps for average light direction\n" );
		}
		else if ( !strcmp( argv[ i ], "-pbr" ) ) {
			deluxemap = qtrue;
			deluxemode = 1;
			lightmapsRGB = qtrue;
			texturesRGB = qtrue;
			colorsRGB = qtrue;
			dirty = qtrue;
			floodlighty = qtrue;
			if ( bounce < 1 ) {
				bounce = 1;
			}
			Sys_Printf( "PBR preset enabled (deluxemap+tangentspace+sRGB+IBL+AO+bounce)\n" );
		}
		else if ( !strcmp( argv[ i ], "-wishgi" ) ) {
			wishGIOptions.enabled = qtrue;
			Sys_Printf( "WishGI experimental probe baking enabled\n" );
		}
		else if ( !strcmp( argv[ i ], "-nowishgi" ) ) {
			wishGIOptions.enabled = qfalse;
			Sys_Printf( "WishGI experimental probe baking disabled\n" );
		}
		else if ( !strcmp( argv[ i ], "-wishgi-probes" ) ) {
			wishGIOptions.enabled = qtrue;
			wishGIOptions.probeCount = atoi( argv[ i + 1 ] );
			Sys_Printf( "WishGI probe count set to %d\n", wishGIOptions.probeCount );
			i++;
		}
		else if ( !strcmp( argv[ i ], "-wishgi-links" ) ) {
			wishGIOptions.enabled = qtrue;
			wishGIOptions.linksPerVertex = atoi( argv[ i + 1 ] );
			Sys_Printf( "WishGI links per vertex set to %d\n", wishGIOptions.linksPerVertex );
			i++;
		}
		else if ( !strcmp( argv[ i ], "-wishgi-iterations" ) ) {
			wishGIOptions.enabled = qtrue;
			wishGIOptions.refinementIterations = atoi( argv[ i + 1 ] );
			if ( wishGIOptions.refinementIterations < 0 ) {
				wishGIOptions.refinementIterations = 0;
			}
			Sys_Printf( "WishGI inverse distribution refinement iterations set to %d\n", wishGIOptions.refinementIterations );
			i++;
		}
		else if ( !strcmp( argv[ i ], "-wishgi-maxsamples" ) ) {
			wishGIOptions.enabled = qtrue;
			wishGIOptions.maxSampleVerts = atoi( argv[ i + 1 ] );
			if ( wishGIOptions.maxSampleVerts < 1 ) {
				wishGIOptions.maxSampleVerts = 1;
			}
			Sys_Printf( "WishGI max sampled vertices set to %d\n", wishGIOptions.maxSampleVerts );
			i++;
		}
		else if ( !strcmp( argv[ i ], "-wishgi-assoc" ) ) {
			wishGIOptions.enabled = qtrue;
			strcpy( wishGIOptions.associationFile, argv[ i + 1 ] );
			Sys_Printf( "WishGI association path set to %s\n", wishGIOptions.associationFile );
			i++;
		}
		else if ( !strcmp( argv[ i ], "-wishgi-loadassoc" ) ) {
			wishGIOptions.enabled = qtrue;
			wishGIOptions.loadAssociations = qtrue;
			Sys_Printf( "WishGI association loading enabled\n" );
		}
		else if ( !strcmp( argv[ i ], "-wishgi-saveassoc" ) ) {
			wishGIOptions.enabled = qtrue;
			wishGIOptions.saveAssociations = qtrue;
			Sys_Printf( "WishGI association writing enabled\n" );
		}
		else if ( !strcmp( argv[ i ], "-wishgi-dumpprobes" ) ) {
			wishGIOptions.enabled = qtrue;
			wishGIOptions.dumpProbeColors = qtrue;
			strcpy( wishGIOptions.probeDumpFile, argv[ i + 1 ] );
			Sys_Printf( "WishGI probe color dump path set to %s\n", wishGIOptions.probeDumpFile );
			i++;
		}
		else if ( !strcmp( argv[ i ], "-wishgi-dumpprobes-default" ) ) {
			wishGIOptions.enabled = qtrue;
			wishGIOptions.dumpProbeColors = qtrue;
			wishGIOptions.probeDumpFile[ 0 ] = '\0';
			Sys_Printf( "WishGI probe color dump enabled (default output path)\n" );
		}
		else if ( !strcmp( argv[ i ], "-wishgi-dumpsh" ) ) {
			wishGIOptions.enabled = qtrue;
			wishGIOptions.dumpProbeSH = qtrue;
			strcpy( wishGIOptions.probeSHDumpFile, argv[ i + 1 ] );
			Sys_Printf( "WishGI SH coefficient dump path set to %s\n", wishGIOptions.probeSHDumpFile );
			i++;
		}
		else if ( !strcmp( argv[ i ], "-wishgi-dumpsh-default" ) ) {
			wishGIOptions.enabled = qtrue;
			wishGIOptions.dumpProbeSH = qtrue;
			wishGIOptions.probeSHDumpFile[ 0 ] = '\0';
			Sys_Printf( "WishGI SH coefficient dump enabled (default output path)\n" );
		}
		else if ( !strcmp( argv[ i ], "-wishgi-shorder" ) ) {
			wishGIOptions.enabled = qtrue;
			wishGIOptions.shOrder = WishGI_ClampInt( atoi( argv[ i + 1 ] ), 0, 3 );
			Sys_Printf( "WishGI SH order set to %d\n", wishGIOptions.shOrder );
			i++;
		}
		else if ( !strcmp( argv[ i ], "-wishgi-shlambda" ) ) {
			wishGIOptions.enabled = qtrue;
			wishGIOptions.shRegularization = atof( argv[ i + 1 ] );
			if ( wishGIOptions.shRegularization <= 0.0f ) {
				wishGIOptions.shRegularization = 0.001f;
			}
			Sys_Printf( "WishGI SH fit regularization set to %.6f\n", wishGIOptions.shRegularization );
			i++;
		}
		else if ( !strcmp( argv[ i ], "-ibl" ) ) {
			floodlighty = qtrue;
			Sys_Printf( "IBL-style ambient floodlight enabled\n" );
		}
		else if ( !strcmp( argv[ i ], "-noibl" ) ) {
			floodlighty = qfalse;
			Sys_Printf( "IBL-style ambient floodlight disabled\n" );
		}
		else if ( !strcmp( argv[ i ], "-external" ) ) {
			externalLightmaps = qtrue;
			Sys_Printf( "Storing all lightmaps externally\n" );
		}
		else if ( !strcmp( argv[ i ], "-hdr32" ) ) {
			lightmapFileFormat = LIGHTMAP_FILEFORMAT_HDR;
			externalLightmaps = qtrue;
			Sys_Printf( "32-bit HDR lightmap output selected (external lightmaps enabled)\n" );
		}
		else if ( !strcmp( argv[ i ], "-exr32" ) ) {
			lightmapFileFormat = LIGHTMAP_FILEFORMAT_EXR;
			externalLightmaps = qtrue;
			Sys_Printf( "32-bit EXR lightmap output selected (external lightmaps enabled)\n" );
		}
		else if ( !strcmp( argv[ i ], "-lmformat" ) || !strcmp( argv[ i ], "-lightmapformat" ) ) {
			if ( i + 1 >= ( argc - 1 ) || !ParseLightmapFormatString( argv[ i + 1 ] ) ) {
				Error( "Invalid lightmap format, expected one of: tga hdr exr" );
			}
			if ( lightmapFileFormat != LIGHTMAP_FILEFORMAT_TGA ) {
				externalLightmaps = qtrue;
			}
			Sys_Printf( "Lightmap file format set to %s\n", argv[ i + 1 ] );
			i++;
		}

		else if ( !strcmp( argv[ i ], "-lightmapsize" ) ) {
			lmCustomSize = atoi( argv[ i + 1 ] );

			/* must be a power of 2 and greater than 2 */
			if ( ( ( lmCustomSize - 1 ) & lmCustomSize ) || lmCustomSize < 2 ) {
				Sys_Printf( "WARNING: Lightmap size must be a power of 2, greater or equal to 2 pixels.\n" );
				lmCustomSize = game->lightmapSize;
			}
			i++;
			Sys_Printf( "Default lightmap size set to %d x %d pixels\n", lmCustomSize, lmCustomSize );

			/* enable external lightmaps */
			if ( lmCustomSize != game->lightmapSize ) {
				externalLightmaps = qtrue;
				Sys_Printf( "Storing all lightmaps externally\n" );
			}
		}

		else if ( !strcmp( argv[ i ], "-rawlightmapsizelimit" ) ) {
			lmLimitSize = atoi( argv[ i + 1 ] );

			i++;
			Sys_Printf( "Raw lightmap size limit set to %d x %d pixels\n", lmLimitSize, lmLimitSize );
		}

		else if ( !strcmp( argv[ i ], "-lightmapdir" ) ) {
			lmCustomDir = argv[i + 1];
			i++;
			Sys_Printf( "Lightmap directory set to %s\n", lmCustomDir );
			externalLightmaps = qtrue;
			Sys_Printf( "Storing all lightmaps externally\n" );
		}

		/* ydnar: add this to suppress warnings */
		else if ( !strcmp( argv[ i ],  "-custinfoparms" ) ) {
			Sys_Printf( "Custom info parms enabled\n" );
			useCustomInfoParms = qtrue;
		}

		else if ( !strcmp( argv[ i ], "-wolf" ) ) {
			/* -game should already be set */
			wolfLight = qtrue;
			Sys_Printf( "Enabling Wolf lighting model (linear default)\n" );
		}

		else if ( !strcmp( argv[ i ], "-q3" ) ) {
			/* -game should already be set */
			wolfLight = qfalse;
			Sys_Printf( "Enabling Quake 3 lighting model (nonlinear default)\n" );
		}

		else if ( !strcmp( argv[ i ], "-extradist" ) ) {
			extraDist = atof( argv[ i + 1 ] );
			if ( extraDist < 0 ) {
				extraDist = 0;
			}
			i++;
			Sys_Printf( "Default extra radius set to %f units\n", extraDist );
		}

		else if ( !strcmp( argv[ i ], "-sunonly" ) ) {
			sunOnly = qtrue;
			Sys_Printf( "Only computing sunlight\n" );
		}

		else if ( !strcmp( argv[ i ], "-bounceonly" ) ) {
			bounceOnly = qtrue;
			Sys_Printf( "Storing bounced light (radiosity) only\n" );
		}

		else if ( !strcmp( argv[ i ], "-nocollapse" ) ) {
			noCollapse = qtrue;
			Sys_Printf( "Identical lightmap collapsing disabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-nolightmapsearch" ) ) {
			lightmapSearchBlockSize = 1;
			Sys_Printf( "No lightmap searching - all lightmaps will be sequential\n" );
		}

		else if ( !strcmp( argv[ i ], "-lightmapsearchpower" ) ) {
			lightmapMergeSize = ( game->lightmapSize << atoi( argv[i + 1] ) );
			++i;
			Sys_Printf( "Restricted lightmap searching enabled - optimize for lightmap merge power %d (size %d)\n", atoi( argv[i] ), lightmapMergeSize );
		}

		else if ( !strcmp( argv[ i ], "-lightmapsearchblocksize" ) ) {
			lightmapSearchBlockSize = atoi( argv[i + 1] );
			++i;
			Sys_Printf( "Restricted lightmap searching enabled - block size set to %d\n", lightmapSearchBlockSize );
		}

		else if ( !strcmp( argv[ i ], "-shade" ) ) {
			shade = qtrue;
			Sys_Printf( "Phong shading enabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-bouncegrid" ) ) {
			bouncegrid = qtrue;
			if ( bounce > 0 ) {
				Sys_Printf( "Grid lighting with radiosity enabled\n" );
			}
		}

		else if ( !strcmp( argv[ i ], "-smooth" ) ) {
			lightSamples = EXTRA_SCALE;
			Sys_Printf( "The -smooth argument is deprecated, use \"-samples 2\" instead\n" );
		}

		else if ( !strcmp( argv[ i ], "-nofastpoint" ) ) {
			fastpoint = qfalse;
			Sys_Printf( "Automatic fast mode for point lights disabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-fast" ) ) {
			fast = qtrue;
			fastgrid = qtrue;
			fastbounce = qtrue;
			Sys_Printf( "Fast mode enabled for all area lights\n" );
		}

		else if ( !strcmp( argv[ i ], "-faster" ) ) {
			faster = qtrue;
			fast = qtrue;
			fastgrid = qtrue;
			fastbounce = qtrue;
			Sys_Printf( "Faster mode enabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-fastallocate" ) ) {
			fastAllocate = qtrue;
			Sys_Printf( "Fast allocation mode enabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-fastgrid" ) ) {
			fastgrid = qtrue;
			Sys_Printf( "Fast grid lighting enabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-fastbounce" ) ) {
			fastbounce = qtrue;
			Sys_Printf( "Fast bounce mode enabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-cheap" ) ) {
			cheap = qtrue;
			cheapgrid = qtrue;
			Sys_Printf( "Cheap mode enabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-cheapgrid" ) ) {
			cheapgrid = qtrue;
			Sys_Printf( "Cheap grid mode enabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-normalmap" ) ) {
			normalmap = qtrue;
			Sys_Printf( "Storing normal map instead of lightmap\n" );
		}

		else if ( !strcmp( argv[ i ], "-trisoup" ) ) {
			trisoup = qtrue;
			Sys_Printf( "Converting brush faces to triangle soup\n" );
		}

		else if ( !strcmp( argv[ i ], "-debug" ) ) {
			debug = qtrue;
			Sys_Printf( "Lightmap debugging enabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-debugsurfaces" ) || !strcmp( argv[ i ], "-debugsurface" ) ) {
			debugSurfaces = qtrue;
			Sys_Printf( "Lightmap surface debugging enabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-debugunused" ) ) {
			debugUnused = qtrue;
			Sys_Printf( "Unused luxel debugging enabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-debugaxis" ) ) {
			debugAxis = qtrue;
			Sys_Printf( "Lightmap axis debugging enabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-debugcluster" ) ) {
			debugCluster = qtrue;
			Sys_Printf( "Luxel cluster debugging enabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-debugorigin" ) ) {
			debugOrigin = qtrue;
			Sys_Printf( "Luxel origin debugging enabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-debugdeluxe" ) ) {
			deluxemap = qtrue;
			debugDeluxemap = qtrue;
			Sys_Printf( "Deluxemap debugging enabled\n" );
		}

		else if ( !strcmp( argv[ i ], "-export" ) ) {
			exportLightmaps = qtrue;
			Sys_Printf( "Exporting lightmaps\n" );
		}

		else if ( !strcmp( argv[ i ], "-notrace" ) ) {
			noTrace = qtrue;
			Sys_Printf( "Shadow occlusion disabled\n" );
		}
		else if ( !strcmp( argv[ i ], "-patchshadows" ) ) {
			patchShadows = qtrue;
			Sys_Printf( "Patch shadow casting enabled\n" );
		}
		else if ( !strcmp( argv[ i ], "-extra" ) ) {
			superSample = EXTRA_SCALE;      /* ydnar */
			Sys_Printf( "The -extra argument is deprecated, use \"-super 2\" instead\n" );
		}
		else if ( !strcmp( argv[ i ], "-extrawide" ) ) {
			superSample = EXTRAWIDE_SCALE;  /* ydnar */
			filter = qtrue;                 /* ydnar */
			Sys_Printf( "The -extrawide argument is deprecated, use \"-filter [-super 2]\" instead\n" );
		}
		else if ( !strcmp( argv[ i ], "-samplesize" ) ) {
			sampleSize = atoi( argv[ i + 1 ] );
			if ( sampleSize < 1 ) {
				sampleSize = 1;
			}
			i++;
			Sys_Printf( "Default lightmap sample size set to %dx%d units\n", sampleSize, sampleSize );
		}
		else if ( !strcmp( argv[ i ], "-minsamplesize" ) ) {
			minSampleSize = atoi( argv[ i + 1 ] );
			if ( minSampleSize < 1 ) {
				minSampleSize = 1;
			}
			i++;
			Sys_Printf( "Minimum lightmap sample size set to %dx%d units\n", minSampleSize, minSampleSize );
		}
		else if ( !strcmp( argv[ i ],  "-samplescale" ) ) {
			sampleScale = atoi( argv[ i + 1 ] );
			i++;
			Sys_Printf( "Lightmaps sample scale set to %d\n", sampleScale );
		}
		else if ( !strcmp( argv[ i ], "-novertex" ) ) {
			noVertexLighting = qtrue;
			Sys_Printf( "Disabling vertex lighting\n" );
		}
		else if ( !strcmp( argv[ i ], "-nogrid" ) ) {
			noGridLighting = qtrue;
			Sys_Printf( "Disabling grid lighting\n" );
		}
		else if ( !strcmp( argv[ i ], "-border" ) ) {
			lightmapBorder = qtrue;
			Sys_Printf( "Adding debug border to lightmaps\n" );
		}
		else if ( !strcmp( argv[ i ], "-nosurf" ) ) {
			noSurfaces = qtrue;
			Sys_Printf( "Not tracing against surfaces\n" );
		}
		else if ( !strcmp( argv[ i ], "-dump" ) ) {
			dump = qtrue;
			Sys_Printf( "Dumping radiosity lights into numbered prefabs\n" );
		}
		else if ( !strcmp( argv[ i ], "-lomem" ) ) {
			loMem = qtrue;
			Sys_Printf( "Enabling low-memory (potentially slower) lighting mode\n" );
		}
		else if ( !strcmp( argv[ i ], "-lightanglehl" ) ) {
			if ( ( atoi( argv[ i + 1 ] ) != 0 ) != lightAngleHL ) {
				lightAngleHL = ( atoi( argv[ i + 1 ] ) != 0 );
				if ( lightAngleHL ) {
					Sys_Printf( "Enabling half lambert light angle attenuation\n" );
				}
				else{
					Sys_Printf( "Disabling half lambert light angle attenuation\n" );
				}
			}
		}
		else if ( !strcmp( argv[ i ], "-nostyle" ) || !strcmp( argv[ i ], "-nostyles" ) ) {
			noStyles = qtrue;
			Sys_Printf( "Disabling lightstyles\n" );
		}
		else if ( !strcmp( argv[ i ], "-style" ) || !strcmp( argv[ i ], "-styles" ) ) {
			noStyles = qfalse;
			Sys_Printf( "Enabling lightstyles\n" );
		}
		else if ( !strcmp( argv[ i ], "-cpma" ) ) {
			cpmaHack = qtrue;
			Sys_Printf( "Enabling Challenge Pro Mode Asstacular Vertex Lighting Mode (tm)\n" );
		}
		else if ( !strcmp( argv[ i ], "-floodlight" ) ) {
			floodlighty = qtrue;
			Sys_Printf( "FloodLighting enabled\n" );
		}
		else if ( !strcmp( argv[ i ], "-debugnormals" ) ) {
			debugnormals = qtrue;
			Sys_Printf( "DebugNormals enabled\n" );
		}
		else if ( !strcmp( argv[ i ], "-lowquality" ) ) {
			floodlight_lowquality = qtrue;
			Sys_Printf( "Low Quality FloodLighting enabled\n" );
		}

		/* r7: dirtmapping */
		else if ( !strcmp( argv[ i ], "-dirty" ) ) {
			dirty = qtrue;
			Sys_Printf( "Dirtmapping enabled\n" );
		}
		else if ( !strcmp( argv[ i ], "-ao" ) ) {
			dirty = qtrue;
			Sys_Printf( "Ambient occlusion enabled\n" );
		}
		else if ( !strcmp( argv[ i ], "-nodirty" ) || !strcmp( argv[ i ], "-noao" ) ) {
			dirty = qfalse;
			Sys_Printf( "Ambient occlusion disabled\n" );
		}
		else if ( !strcmp( argv[ i ], "-dirtdebug" ) || !strcmp( argv[ i ], "-debugdirt" ) ) {
			dirtDebug = qtrue;
			Sys_Printf( "Dirtmap debugging enabled\n" );
		}
		else if ( !strcmp( argv[ i ], "-dirtmode" ) ) {
			dirtMode = atoi( argv[ i + 1 ] );
			if ( dirtMode != 0 && dirtMode != 1 ) {
				dirtMode = 0;
			}
			if ( dirtMode == 1 ) {
				Sys_Printf( "Enabling randomized dirtmapping\n" );
			}
			else{
				Sys_Printf( "Enabling ordered dir mapping\n" );
			}
			i++;
		}
		else if ( !strcmp( argv[ i ], "-dirtdepth" ) ) {
			dirtDepth = atof( argv[ i + 1 ] );
			if ( dirtDepth <= 0.0f ) {
				dirtDepth = 128.0f;
			}
			Sys_Printf( "Dirtmapping depth set to %.1f\n", dirtDepth );
			i++;
		}
		else if ( !strcmp( argv[ i ], "-dirtscale" ) ) {
			dirtScale = atof( argv[ i + 1 ] );
			if ( dirtScale <= 0.0f ) {
				dirtScale = 1.0f;
			}
			Sys_Printf( "Dirtmapping scale set to %.1f\n", dirtScale );
			i++;
		}
		else if ( !strcmp( argv[ i ], "-dirtgain" ) ) {
			dirtGain = atof( argv[ i + 1 ] );
			if ( dirtGain <= 0.0f ) {
				dirtGain = 1.0f;
			}
			Sys_Printf( "Dirtmapping gain set to %.1f\n", dirtGain );
			i++;
		}
		else if ( !strcmp( argv[ i ], "-trianglecheck" ) ) {
			lightmapTriangleCheck = qtrue;
		}
		else if ( !strcmp( argv[ i ], "-extravisnudge" ) ) {
			lightmapExtraVisClusterNudge = qtrue;
		}
		else if ( !strcmp( argv[ i ], "-fill" ) ) {
			lightmapFill = qtrue;
			Sys_Printf( "Filling lightmap colors from surrounding pixels to improve JPEG compression\n" );
		}
		else if ( !strcmp( argv[ i ], "-bspfile" ) )
		{
			strcpy( BSPFilePath, argv[i + 1] );
			i++;
			Sys_Printf( "Use %s as bsp file\n", BSPFilePath );
		}
		else if ( !strcmp( argv[ i ], "-srffile" ) )
		{
			strcpy( surfaceFilePath, argv[i + 1] );
			i++;
			Sys_Printf( "Use %s as surface file\n", surfaceFilePath );
		}
		/* unhandled args */
		else
		{
			Sys_Printf( "WARNING: Unknown argument \"%s\"\n", argv[ i ] );
		}

	}

	/* fix up falloff tolerance for sRGB */
	if ( lightmapsRGB ) {
		falloffTolerance = Image_LinearFloatFromsRGBFloat( falloffTolerance * ( 1.0 / 255.0 ) ) * 255.0;
	}

	/* fix up samples count */
	if ( lightRandomSamples ) {
		if ( !lightSamplesInsist ) {
			/* approximately match -samples in quality */
			switch ( lightSamples )
			{
			/* somewhat okay */
			case 1:
			case 2:
				lightSamples = 16;
				Sys_Printf( "Adaptive supersampling preset enabled with %d random sample(s) per lightmap texel\n", lightSamples );
				break;

			/* good */
			case 3:
				lightSamples = 64;
				Sys_Printf( "Adaptive supersampling preset enabled with %d random sample(s) per lightmap texel\n", lightSamples );
				break;

			/* perfect */
			case 4:
				lightSamples = 256;
				Sys_Printf( "Adaptive supersampling preset enabled with %d random sample(s) per lightmap texel\n", lightSamples );
				break;

			default: break;
			}
		}
	}

	/* fix up lightmap search power */
	if ( lightmapMergeSize ) {
		lightmapSearchBlockSize = ( lightmapMergeSize / lmCustomSize ) * ( lightmapMergeSize / lmCustomSize );
		if ( lightmapSearchBlockSize < 1 ) {
			lightmapSearchBlockSize = 1;
		}

		Sys_Printf( "Restricted lightmap searching enabled - block size adjusted to %d\n", lightmapSearchBlockSize );
	}

	if ( lightmapFileFormat == LIGHTMAP_FILEFORMAT_HDR || lightmapFileFormat == LIGHTMAP_FILEFORMAT_EXR ) {
		externalLightmaps = qtrue;
	}

	Sys_Printf( "Ambient occlusion: %s\n", dirty ? "enabled" : "disabled" );
	Sys_Printf( "IBL ambient floodlight: %s\n", floodlighty ? "enabled" : "disabled" );
	Sys_Printf( "Lightmap file format: %s\n",
				( lightmapFileFormat == LIGHTMAP_FILEFORMAT_HDR ) ? "hdr" :
				( lightmapFileFormat == LIGHTMAP_FILEFORMAT_EXR ) ? "exr" : "tga" );
	if ( wishGIOptions.enabled ) {
		Sys_Printf( "WishGI experimental mode: enabled (probes %d, links %d, refinement %d, SH order %d, SH lambda %.6f)\n",
					wishGIOptions.probeCount, wishGIOptions.linksPerVertex, wishGIOptions.refinementIterations,
					wishGIOptions.shOrder, wishGIOptions.shRegularization );
	}
	else{
		Sys_Printf( "WishGI experimental mode: disabled\n" );
	}

	strcpy( source, ExpandArg( argv[ i ] ) );
	StripExtension( source );
	DefaultExtension( source, ".map" );

	if (!BSPFilePath[0]) {
		strcpy( BSPFilePath, ExpandArg( argv[ i ] ) );
		StripExtension( BSPFilePath );
		DefaultExtension( BSPFilePath, ".bsp" );
	}

	if (!surfaceFilePath[0]) {
		strcpy( surfaceFilePath, ExpandArg( argv[ i ] ) );
		StripExtension( surfaceFilePath );
		DefaultExtension( surfaceFilePath, ".srf" );
	}

	/* ydnar: set default sample size */
	SetDefaultSampleSize( sampleSize );

	/* ydnar: handle shaders */
	BeginMapShaderFile( BSPFilePath );
	LoadShaderInfo();

	/* note loading */
	Sys_Printf( "Loading %s\n", source );

	/* ydnar: load surface file */
	LoadSurfaceExtraFile( surfaceFilePath );

	/* load bsp file */
	LoadBSPFile( BSPFilePath );

	/* parse bsp entities */
	ParseEntities();

	/* inject command line parameters */
	InjectCommandLine( argv, 0, argc - 1 );

	/* load map file */
	value = ValueForKey( &entities[ 0 ], "_keepLights" );
	if ( value[ 0 ] != '1' ) {
		LoadMapFile( source, qtrue, qfalse );
	}

	/* set the entity/model origins and init yDrawVerts */
	SetEntityOrigins();
	WishGI_PrepareAssociations( BSPFilePath );

	/* ydnar: set up optimization */
	SetupBrushes();
	SetupDirt();
	SetupFloodLight();
	SetupSurfaceLightmaps();

	/* initialize the surface facet tracing */
	SetupTraceNodes();

	/* light the world */
	LightWorld( BSPFilePath, fastAllocate );

	/* Optional debug dump: per-probe RGB fit from baked vertex lighting. */
	WishGI_DumpProbeColors( BSPFilePath );
	WishGI_DumpProbeSHCoefficients( BSPFilePath );

	/* ydnar: store off lightmaps */
	StoreSurfaceLightmaps( fastAllocate );

	/* write out the bsp */
	UnparseEntities();
	Sys_Printf( "Writing %s\n", BSPFilePath );
	WriteBSPFile( BSPFilePath );

	/* ydnar: export lightmaps */
	if ( exportLightmaps && !externalLightmaps ) {
		ExportLightmaps();
	}

	/* return to sender */
	WishGI_ClearAssociations();
	return 0;
}
