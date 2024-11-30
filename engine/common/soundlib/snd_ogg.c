/*
snd_ogg.c - loading and streaming of Ogg format with Vorbis/Opus codec
Copyright (C) 2024 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "soundlib.h"
#include "crtlib.h"
#include "xash3d_mathlib.h"
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include <string.h>
#include <stdint.h>

typedef struct ogg_filestream_s
{
	const char *name;
	const byte *buffer;
	size_t filesize;
	size_t position;
} ogg_filestream_t;

static void OggFilestream_Init( ogg_filestream_t *filestream, const char *name, const byte *buffer, size_t filesize )
{
	filestream->name = name;
	filestream->buffer = buffer;
	filestream->filesize = filesize;
	filestream->position = 0;
}

static size_t OggFilestream_Read( void *ptr, size_t blockSize, size_t nmemb, void *datasource )
{
	ogg_filestream_t *filestream = (ogg_filestream_t*)datasource;
	size_t remain = filestream->filesize - filestream->position;
	size_t dataSize = blockSize * nmemb;

	// reads as many blocks as fits in remaining memory
	if( dataSize > remain )
		dataSize = remain - remain % blockSize; 

	memcpy( ptr, filestream->buffer + filestream->position, dataSize );
	filestream->position += dataSize;
	return dataSize / blockSize;
}

static int OggFilestream_Seek( void *datasource, int64_t offset, int whence )
{
	int64_t position;
	ogg_filestream_t *filestream = (ogg_filestream_t*)datasource;

	if( whence == SEEK_SET )
		position = offset;
	else if( whence == SEEK_CUR )
		position = offset + filestream->position;
	else if( whence == SEEK_END )
		position = offset + filestream->filesize;
	else
		return -1;

	if( position < 0 || position > filestream->filesize )
		return -1;

	filestream->position = position;
	return 0;
}

static long OggFilestream_Tell( void *datasource )
{
	ogg_filestream_t *filestream = (ogg_filestream_t*)datasource;
	return filestream->position;
}

static const ov_callbacks ov_callbacks_membuf = {
	OggFilestream_Read,
	OggFilestream_Seek,
	NULL,
	OggFilestream_Tell
};

qboolean Sound_LoadOggVorbis( const char *name, const byte *buffer, fs_offset_t filesize )
{
	long ret;
	int section;
	size_t written = 0;
	ogg_filestream_t file;
	OggVorbis_File vorbisFile;
	
	if( !buffer )
		return false;

	OggFilestream_Init( &file, name, buffer, filesize );
	if( ov_open_callbacks( &file, &vorbisFile, NULL, 0, ov_callbacks_membuf ) < 0 ) {
		Con_DPrintf( S_ERROR "%s: failed to load (%s): file reading error\n", __func__, file.name );
		return false;
	}

	vorbis_info *info = ov_info( &vorbisFile, -1 );
	if( info->channels < 1 || info->channels > 2 ) {
		Con_DPrintf( S_ERROR "%s: failed to load (%s): unsuppored channels count\n", __func__, file.name );
		return false;
	}

	sound.channels = info->channels;
	sound.rate = info->rate;
	sound.width = 2; // always 16-bit PCM
	sound.type = WF_PCMDATA;
	sound.samples = ov_pcm_total( &vorbisFile, -1 );
	sound.size = sound.samples * sound.width * sound.channels;
	sound.wav = (byte *)Mem_Calloc( host.soundpool, sound.size );

	while(( ret = ov_read( &vorbisFile, (char*)sound.wav + written, sound.size, 0, sound.width, 1, &section )) != 0 )
	{
		if( ret < 0 )
		{
			Con_DPrintf( S_ERROR "%s: failed to load (%s): error during Vorbis data decoding\n", __func__, file.name );
			return false;
		}
		written += ret;
	}

	ov_clear( &vorbisFile );
	return true;
}
