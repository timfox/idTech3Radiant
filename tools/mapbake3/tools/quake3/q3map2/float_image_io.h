#ifndef FLOAT_IMAGE_IO_H
#define FLOAT_IMAGE_IO_H

#ifdef __cplusplus
extern "C" {
#endif

int MapBake3_LoadHDRBufferToRGBA8( const unsigned char *buffer, int size, unsigned char **pixels, int *width, int *height );
int MapBake3_LoadEXRBufferToRGBA8( const unsigned char *buffer, int size, unsigned char **pixels, int *width, int *height );

int MapBake3_LoadHDRBufferToRGB8( const unsigned char *buffer, int size, unsigned char **pixels, int *width, int *height );
int MapBake3_LoadEXRBufferToRGB8( const unsigned char *buffer, int size, unsigned char **pixels, int *width, int *height );

int MapBake3_LoadHDRBufferToRGBAF32( const unsigned char *buffer, int size, float **pixels, int *width, int *height );
int MapBake3_LoadEXRBufferToRGBAF32( const unsigned char *buffer, int size, float **pixels, int *width, int *height );

int MapBake3_WriteRGB8AsHDR32( const char *filename, const unsigned char *pixels, int width, int height, int flip );
int MapBake3_WriteRGB8AsEXR32( const char *filename, const unsigned char *pixels, int width, int height, int flip );

#ifdef __cplusplus
}
#endif

#endif /* FLOAT_IMAGE_IO_H */
