/*
cl_spray.c - spray conversion for GoldSrc protocol
Copyright (C) 2025 Xash3D FWGS contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"
#include "filesystem.h"
#include "imagelib.h"

#define SPRAY_MAX_SURFACE		12228
#define SPRAY_PALETTE_SIZE		256
#define SPRAY_PALETTE_BYTES		( SPRAY_PALETTE_SIZE * 3 )
#define SPRAY_ALPHA_THRESHOLD	254
#define SPRAY_FILENAME			"tempdecal.wad"

// adjusts spray dimensions
static void CL_AdjustSprayDimensions( int *width, int *height )
{
	float aspect = (float)( *width ) / (float)( *height );
	int   h, w;
	for( h = (( *height ) / 16 ) * 16; h >= 16; h -= 16 )
	{
		w = ((int)( h * aspect ) / 16 ) * 16;
		if( w < 16 || w > *width )
			continue;

		if( w * h < SPRAY_MAX_SURFACE )
		{
			*width = w;
			*height = h;
			return;
		}
	}
	// fallback: minimal size
	*width = 16;
	*height = 16;
}

// loads and prepares the image
static rgbdata_t *CL_LoadAndPrepareImage( const char *filename, int *width, int *height, qboolean keep8bit )
{
	rgbdata_t	*image = NULL;
	rgbdata_t	*scaled = NULL;
	byte		*pix;
	byte		*resampled_buf;
	qboolean	resampled = false;
	qboolean	indexed = false;
	int			bpp, palette_size, i;

	if( keep8bit )
	{
		Image_SetForceFlags( IL_KEEP_8BIT );
		image = FS_LoadImage( filename, NULL, 0 );
		Image_SetForceFlags( 0 );
	}
	else
	{
		image = FS_LoadImage( filename, NULL, 0 );
	}

	if( !image )
		return NULL;

	bpp = ( image->type == PF_RGBA_32 ) ? 4 : ( image->type == PF_INDEXED_32 ? 1 : 3 );
	indexed = ( image->type == PF_INDEXED_24 ) || ( image->type == PF_INDEXED_32 );
	palette_size = 256 * ( image->type == PF_INDEXED_32 ? 4 : 3 );

	*width = image->width;
	*height = image->height;
	CL_AdjustSprayDimensions( width, height );

	if( *width != image->width || *height != image->height )
	{
		// resample image to fit spray size constraints
		resampled_buf = Image_ResampleInternal(
			image->buffer, image->width, image->height,
			*width, *height, image->type, &resampled
			);

		if( !resampled_buf )
		{
			FS_FreeImage( image );
			return NULL;
		}

		scaled = Mem_Malloc( host.imagepool, sizeof( rgbdata_t ));
		*scaled = *image;
		scaled->width = *width;
		scaled->height = *height;
		scaled->size = *width * *height * (( image->type == PF_RGBA_32 ) ? 4 : ( indexed ? 1 : 3 ));
		scaled->buffer = Mem_Malloc( host.imagepool, scaled->size );
		memcpy( scaled->buffer, resampled_buf, scaled->size );

		if( keep8bit && image->palette )
		{
			// copy 8-bit palette for resampled bmp
			scaled->palette = Mem_Malloc( host.imagepool, palette_size );
			memcpy( scaled->palette, image->palette, palette_size );
		}

		FS_FreeImage( image );
		image = scaled;
	}

	return image;
}

// converts an image to WAD3 spray or miptex format
qboolean CL_ConvertImageToWAD3( const char *filename )
{
	const char	*ext;
	qboolean	is_bmp, is_indexed_img;
	int			width = 0, height = 0;
	int			i, idx;
	byte		palette[SPRAY_PALETTE_BYTES];
	byte		*indexed = NULL;
	byte		*bmp_palette = NULL;
	rgbdata_t	*image = NULL;
	rgbdata_t	*quant = NULL;
	rgbdata_t	temp_image = {0};

	ext = Q_strrchr( filename, '.' );
	is_bmp = ( ext && !Q_stricmp( ext, ".bmp" ));

	image = CL_LoadAndPrepareImage( filename, &width, &height, is_bmp );
	if( !image )
		return false;

	is_indexed_img = is_bmp && image->palette != NULL;
	if( is_indexed_img )
	{
		// copy bmp palette from rgba to rgb
		for( i = 0; i < 256; ++i )
		{
			palette[i * 3 + 0] = image->palette[i * 4 + 0]; // R
			palette[i * 3 + 1] = image->palette[i * 4 + 1]; // G
			palette[i * 3 + 2] = image->palette[i * 4 + 2]; // B
		}
		indexed = image->buffer;
	}
	else
	{
		quant = Mem_Malloc( host.imagepool, sizeof( rgbdata_t ));
		*quant = *image;
		quant->buffer = Mem_Malloc( host.imagepool, quant->size );
		memcpy( quant->buffer, image->buffer, quant->size );
		Image_Quantize( quant ); // it's so weird, it writes result to same structure as used for input data

		if( !quant || !quant->buffer || !quant->palette )
			goto cleanup;
		
		// remap palette index 255 to 254 to avoid transparency conflicts
		for( i = 0; i < width * height; ++i )
		{
			if( quant->buffer[i] == 255 )
				quant->buffer[i] = 254;
		}

		// set index 255 for transparent pixels in rgba images
		if( image->type == PF_RGBA_32 )
		{
			for( i = 0; i < width * height; ++i )
			{
				if( image->buffer[i * 4 + 3] <= SPRAY_ALPHA_THRESHOLD ) 
					quant->buffer[i] = 255;
			}
		}

		quant->palette[255 * 3 + 0] = 0;
		quant->palette[255 * 3 + 1] = 0;
		quant->palette[255 * 3 + 2] = 255;
		memcpy( palette, quant->palette, SPRAY_PALETTE_BYTES );
		indexed = quant->buffer;
	}

	temp_image.width = width;
	temp_image.height = height;
	temp_image.type = PF_INDEXED_32;
	temp_image.buffer = indexed;
	temp_image.size = width * height;
	temp_image.palette = palette;

	if( is_indexed_img )
		temp_image.flags |= IMAGE_GRADIENT_DECAL;

	return Image_SaveWAD( SPRAY_FILENAME, &temp_image );

cleanup:
	if( image )
		FS_FreeImage( image );
	if( quant )
		FS_FreeImage( quant );
	return false;
}
