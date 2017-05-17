/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file saveload_format.cpp Saveload filter definitions for savegame compression formats. */

#include "../stdafx.h"
#include "../debug.h"
#include "../error.h"
#include "../core/endian_func.hpp"
#include "../core/math_func.hpp"
#include "../string.h"
#include "../strings_func.h"

#include "table/strings.h"

#include "saveload_filter.h"
#include "saveload.h"
#include "saveload_error.h"

/** Save in chunks of 128 KiB. */
static const size_t MEMORY_CHUNK_SIZE = 128 * 1024;

/*******************************************
 ********** START OF LZO CODE **************
 *******************************************/

#ifdef WITH_LZO
#include <lzo/lzo1x.h>

/** Buffer size for the LZO compressor */
static const uint LZO_BUFFER_SIZE = 8192;

/** Filter using LZO compression. */
struct LZOLoadFilter : ChainLoadFilter {
	const bool buggy;

	/**
	 * Initialise this filter.
	 * @param chain The next filter in this chain.
	 */
	LZOLoadFilter(LoadFilter *chain, bool buggy = false) : ChainLoadFilter(chain), buggy(buggy)
	{
		if (lzo_init() != LZO_E_OK) throw SlException(STR_GAME_SAVELOAD_ERROR_BROKEN_INTERNAL_ERROR, "cannot initialize decompressor");
	}

	/* virtual */ size_t Read(byte *buf, size_t ssize)
	{
		assert(ssize >= LZO_BUFFER_SIZE);

		/* Buffer size is from the LZO docs plus the chunk header size. */
		byte out[LZO_BUFFER_SIZE + LZO_BUFFER_SIZE / 16 + 64 + 3 + sizeof(uint32) * 2];
		uint32 tmp[2];
		uint32 size;
		lzo_uint len = ssize;

		/* Read header*/
		if (this->chain->Read((byte*)tmp, sizeof(tmp)) != sizeof(tmp)) throw SlException(STR_GAME_SAVELOAD_ERROR_FILE_NOT_READABLE);

		/* Check if size is bad */
		((uint32*)out)[0] = size = tmp[1];

		if (!buggy) {
			tmp[0] = TO_BE32(tmp[0]);
			size = TO_BE32(size);
		}

		if (size >= sizeof(out)) throw SlCorrupt("Inconsistent size");

		/* Read block */
		if (this->chain->Read(out + sizeof(uint32), size) != size) throw SlException(STR_GAME_SAVELOAD_ERROR_FILE_NOT_READABLE);

		/* Verify checksum */
		if (tmp[0] != lzo_adler32(0, out, size + sizeof(uint32))) throw SlCorrupt("Bad checksum");

		/* Decompress */
		int ret = lzo1x_decompress_safe(out + sizeof(uint32) * 1, size, buf, &len, NULL);
		if (ret != LZO_E_OK) throw SlException(STR_GAME_SAVELOAD_ERROR_FILE_NOT_READABLE);
		return len;
	}
};

/** Instantiator for the old version 0 LZO filter */
static ChainLoadFilter *CreateLZO0LoadFilter(LoadFilter *chain)
{
	return new LZOLoadFilter(chain, true);
}

/** Filter using LZO compression. */
struct LZOSaveFilter : ChainSaveFilter {
	/**
	 * Initialise this filter.
	 * @param chain             The next filter in this chain.
	 * @param compression_level The requested level of compression.
	 */
	LZOSaveFilter(SaveFilter *chain, byte compression_level) : ChainSaveFilter(chain)
	{
		if (lzo_init() != LZO_E_OK) throw SlException(STR_GAME_SAVELOAD_ERROR_BROKEN_INTERNAL_ERROR, "cannot initialize compressor");
	}

	/* virtual */ void Write(const byte *buf, size_t size)
	{
		const lzo_bytep in = buf;
		/* Buffer size is from the LZO docs plus the chunk header size. */
		byte out[LZO_BUFFER_SIZE + LZO_BUFFER_SIZE / 16 + 64 + 3 + sizeof(uint32) * 2];
		byte wrkmem[LZO1X_1_MEM_COMPRESS];
		lzo_uint outlen;

		do {
			/* Compress up to LZO_BUFFER_SIZE bytes at once. */
			lzo_uint len = size > LZO_BUFFER_SIZE ? LZO_BUFFER_SIZE : (lzo_uint)size;
			lzo1x_1_compress(in, len, out + sizeof(uint32) * 2, &outlen, wrkmem);
			((uint32*)out)[1] = TO_BE32((uint32)outlen);
			((uint32*)out)[0] = TO_BE32(lzo_adler32(0, out + sizeof(uint32), outlen + sizeof(uint32)));
			this->chain->Write(out, outlen + sizeof(uint32) * 2);

			/* Move to next data chunk. */
			size -= len;
			in += len;
		} while (size > 0);
	}
};

#endif /* WITH_LZO */

/*********************************************
 ******** START OF NOCOMP CODE (uncompressed)*
 *********************************************/

/** Filter without any compression. */
struct NoCompLoadFilter : ChainLoadFilter {
	/**
	 * Initialise this filter.
	 * @param chain The next filter in this chain.
	 */
	NoCompLoadFilter(LoadFilter *chain) : ChainLoadFilter(chain)
	{
	}

	/* virtual */ size_t Read(byte *buf, size_t size)
	{
		return this->chain->Read(buf, size);
	}
};

/** Filter without any compression. */
struct NoCompSaveFilter : ChainSaveFilter {
	/**
	 * Initialise this filter.
	 * @param chain             The next filter in this chain.
	 * @param compression_level The requested level of compression.
	 */
	NoCompSaveFilter(SaveFilter *chain, byte compression_level) : ChainSaveFilter(chain)
	{
	}

	/* virtual */ void Write(const byte *buf, size_t size)
	{
		this->chain->Write(buf, size);
	}
};

/********************************************
 ********** START OF ZLIB CODE **************
 ********************************************/

#if defined(WITH_ZLIB)
#define ZLIB_CONST
#include <zlib.h>

/** Filter using Zlib compression. */
struct ZlibLoadFilter : ChainLoadFilter {
	z_stream z;                        ///< Stream state we are reading from.
	byte fread_buf[MEMORY_CHUNK_SIZE]; ///< Buffer for reading from the file.

	/**
	 * Initialise this filter.
	 * @param chain The next filter in this chain.
	 */
	ZlibLoadFilter(LoadFilter *chain) : ChainLoadFilter(chain)
	{
		memset(&this->z, 0, sizeof(this->z));
		if (inflateInit(&this->z) != Z_OK) throw SlException(STR_GAME_SAVELOAD_ERROR_BROKEN_INTERNAL_ERROR, "cannot initialize decompressor");
	}

	/** Clean everything up. */
	~ZlibLoadFilter()
	{
		inflateEnd(&this->z);
	}

	/* virtual */ size_t Read(byte *buf, size_t size)
	{
		this->z.next_out  = buf;
		this->z.avail_out = (uint)size;

		do {
			/* read more bytes from the file? */
			if (this->z.avail_in == 0) {
				this->z.next_in = this->fread_buf;
				this->z.avail_in = (uint)this->chain->Read(this->fread_buf, sizeof(this->fread_buf));
			}

			/* inflate the data */
			int r = inflate(&this->z, 0);
			if (r == Z_STREAM_END) break;

			if (r != Z_OK) throw SlException(STR_GAME_SAVELOAD_ERROR_BROKEN_INTERNAL_ERROR, "inflate() failed");
		} while (this->z.avail_out != 0);

		return size - this->z.avail_out;
	}
};

/** Filter using Zlib compression. */
struct ZlibSaveFilter : ChainSaveFilter {
	z_stream z; ///< Stream state we are writing to.

	/**
	 * Initialise this filter.
	 * @param chain             The next filter in this chain.
	 * @param compression_level The requested level of compression.
	 */
	ZlibSaveFilter(SaveFilter *chain, byte compression_level) : ChainSaveFilter(chain)
	{
		memset(&this->z, 0, sizeof(this->z));
		if (deflateInit(&this->z, compression_level) != Z_OK) throw SlException(STR_GAME_SAVELOAD_ERROR_BROKEN_INTERNAL_ERROR, "cannot initialize compressor");
	}

	/** Clean up what we allocated. */
	~ZlibSaveFilter()
	{
		deflateEnd(&this->z);
	}

	/**
	 * Helper loop for writing the data.
	 * @param p    The bytes to write.
	 * @param len  Amount of bytes to write.
	 * @param mode Mode for deflate.
	 */
	void WriteLoop(const byte *p, size_t len, int mode)
	{
		byte buf[MEMORY_CHUNK_SIZE]; // output buffer
		uint n;
		this->z.next_in = p;
		this->z.avail_in = (uInt)len;
		do {
			this->z.next_out = buf;
			this->z.avail_out = sizeof(buf);

			/**
			 * For the poor next soul who sees many valgrind warnings of the
			 * "Conditional jump or move depends on uninitialised value(s)" kind:
			 * According to the author of zlib it is not a bug and it won't be fixed.
			 * http://groups.google.com/group/comp.compression/browse_thread/thread/b154b8def8c2a3ef/cdf9b8729ce17ee2
			 * [Mark Adler, Feb 24 2004, 'zlib-1.2.1 valgrind warnings' in the newsgroup comp.compression]
			 */
			int r = deflate(&this->z, mode);

			/* bytes were emitted? */
			if ((n = sizeof(buf) - this->z.avail_out) != 0) {
				this->chain->Write(buf, n);
			}
			if (r == Z_STREAM_END) break;

			if (r != Z_OK) throw SlException(STR_GAME_SAVELOAD_ERROR_BROKEN_INTERNAL_ERROR, "zlib returned error code");
		} while (this->z.avail_in || !this->z.avail_out);
	}

	/* virtual */ void Write(const byte *buf, size_t size)
	{
		this->WriteLoop(buf, size, 0);
	}

	/* virtual */ void Finish()
	{
		this->WriteLoop(NULL, 0, Z_FINISH);
		this->chain->Finish();
	}
};

#endif /* WITH_ZLIB */

/********************************************
 ********** START OF LZMA CODE **************
 ********************************************/

#if defined(WITH_LZMA)
#include <lzma.h>

/**
 * Have a copy of an initialised LZMA stream. We need this as it's
 * impossible to "re"-assign LZMA_STREAM_INIT to a variable in some
 * compilers, i.e. LZMA_STREAM_INIT can't be used to set something.
 * This var has to be used instead.
 */
static const lzma_stream _lzma_init = LZMA_STREAM_INIT;

/** Filter without any compression. */
struct LZMALoadFilter : ChainLoadFilter {
	lzma_stream lzma;                  ///< Stream state that we are reading from.
	byte fread_buf[MEMORY_CHUNK_SIZE]; ///< Buffer for reading from the file.

	/**
	 * Initialise this filter.
	 * @param chain The next filter in this chain.
	 */
	LZMALoadFilter(LoadFilter *chain) : ChainLoadFilter(chain), lzma(_lzma_init)
	{
		/* Allow saves up to 256 MB uncompressed */
		if (lzma_auto_decoder(&this->lzma, 1 << 28, 0) != LZMA_OK) throw SlException(STR_GAME_SAVELOAD_ERROR_BROKEN_INTERNAL_ERROR, "cannot initialize decompressor");
	}

	/** Clean everything up. */
	~LZMALoadFilter()
	{
		lzma_end(&this->lzma);
	}

	/* virtual */ size_t Read(byte *buf, size_t size)
	{
		this->lzma.next_out  = buf;
		this->lzma.avail_out = size;

		do {
			/* read more bytes from the file? */
			if (this->lzma.avail_in == 0) {
				this->lzma.next_in  = this->fread_buf;
				this->lzma.avail_in = this->chain->Read(this->fread_buf, sizeof(this->fread_buf));
			}

			/* inflate the data */
			lzma_ret r = lzma_code(&this->lzma, LZMA_RUN);
			if (r == LZMA_STREAM_END) break;
			if (r != LZMA_OK) throw SlException(STR_GAME_SAVELOAD_ERROR_BROKEN_INTERNAL_ERROR, "liblzma returned error code");
		} while (this->lzma.avail_out != 0);

		return size - this->lzma.avail_out;
	}
};

/** Filter using LZMA compression. */
struct LZMASaveFilter : ChainSaveFilter {
	lzma_stream lzma; ///< Stream state that we are writing to.

	/**
	 * Initialise this filter.
	 * @param chain             The next filter in this chain.
	 * @param compression_level The requested level of compression.
	 */
	LZMASaveFilter(SaveFilter *chain, byte compression_level) : ChainSaveFilter(chain), lzma(_lzma_init)
	{
		if (lzma_easy_encoder(&this->lzma, compression_level, LZMA_CHECK_CRC32) != LZMA_OK) throw SlException(STR_GAME_SAVELOAD_ERROR_BROKEN_INTERNAL_ERROR, "cannot initialize compressor");
	}

	/** Clean up what we allocated. */
	~LZMASaveFilter()
	{
		lzma_end(&this->lzma);
	}

	/**
	 * Helper loop for writing the data.
	 * @param p      The bytes to write.
	 * @param len    Amount of bytes to write.
	 * @param action Action for lzma_code.
	 */
	void WriteLoop(const byte *p, size_t len, lzma_action action)
	{
		byte buf[MEMORY_CHUNK_SIZE]; // output buffer
		size_t n;
		this->lzma.next_in = p;
		this->lzma.avail_in = len;
		do {
			this->lzma.next_out = buf;
			this->lzma.avail_out = sizeof(buf);

			lzma_ret r = lzma_code(&this->lzma, action);

			/* bytes were emitted? */
			if ((n = sizeof(buf) - this->lzma.avail_out) != 0) {
				this->chain->Write(buf, n);
			}
			if (r == LZMA_STREAM_END) break;
			if (r != LZMA_OK) throw SlException(STR_GAME_SAVELOAD_ERROR_BROKEN_INTERNAL_ERROR, "liblzma returned error code");
		} while (this->lzma.avail_in || !this->lzma.avail_out);
	}

	/* virtual */ void Write(const byte *buf, size_t size)
	{
		this->WriteLoop(buf, size, LZMA_RUN);
	}

	/* virtual */ void Finish()
	{
		this->WriteLoop(NULL, 0, LZMA_FINISH);
		this->chain->Finish();
	}
};

#endif /* WITH_LZMA */

/*******************************************
 ************* END OF CODE *****************
 *******************************************/

/** The format for a reader/writer type of a savegame */
struct SaveLoadFormat {
	const char *name;                     ///< name of the compressor/decompressor (debug-only)
	uint32 tag;                           ///< the 4-letter tag by which it is identified in the savegame
	uint32 ottd_tag;                      ///< the 4-letter tag by which it is identified in legacy savegames

	ChainLoadFilter *(*init_load)(LoadFilter *chain);                    ///< Constructor for the load filter.
	ChainSaveFilter *(*init_write)(SaveFilter *chain, byte compression); ///< Constructor for the save filter.

	byte min_compression;                 ///< the minimum compression level of this format
	byte default_compression;             ///< the default compression level of this format
	byte max_compression;                 ///< the maximum compression level of this format
};

/** The different saveload formats known/understood by OpenTTD. */
static const SaveLoadFormat _saveload_formats[] = {
#if defined(WITH_LZO)
	/* Roughly 75% larger than zlib level 6 at only ~7% of the CPU usage. */
	{"lzo",    TO_BE32X('LZO\0'),   TO_BE32X('OTTD'), CreateLoadFilter<LZOLoadFilter>,    CreateSaveFilter<LZOSaveFilter>,    0, 0, 0},
#else
	{"lzo",    TO_BE32X('LZO\0'),   TO_BE32X('OTTD'), NULL,                               NULL,                               0, 0, 0},
#endif
	/* Roughly 5 times larger at only 1% of the CPU usage over zlib level 6. */
	{"none",   TO_BE32X('RAW\0'),   TO_BE32X('OTTN'), CreateLoadFilter<NoCompLoadFilter>, CreateSaveFilter<NoCompSaveFilter>, 0, 0, 0},
#if defined(WITH_ZLIB)
	/* After level 6 the speed reduction is significant (1.5x to 2.5x slower per level), but the reduction in filesize is
	 * fairly insignificant (~1% for each step). Lower levels become ~5-10% bigger by each level than level 6 while level
	 * 1 is "only" 3 times as fast. Level 0 results in uncompressed savegames at about 8 times the cost of "none". */
	{"zlib",   TO_BE32X('Z\0\0\0'), TO_BE32X('OTTZ'), CreateLoadFilter<ZlibLoadFilter>,   CreateSaveFilter<ZlibSaveFilter>,   0, 6, 9},
#else
	{"zlib",   TO_BE32X('Z\0\0\0'), TO_BE32X('OTTZ'), NULL,                               NULL,                               0, 0, 0},
#endif
#if defined(WITH_LZMA)
	/* Level 2 compression is speed wise as fast as zlib level 6 compression (old default), but results in ~10% smaller saves.
	 * Higher compression levels are possible, and might improve savegame size by up to 25%, but are also up to 10 times slower.
	 * The next significant reduction in file size is at level 4, but that is already 4 times slower. Level 3 is primarily 50%
	 * slower while not improving the filesize, while level 0 and 1 are faster, but don't reduce savegame size much.
	 * It's OTTX and not e.g. OTTL because liblzma is part of xz-utils and .tar.xz is preferred over .tar.lzma. */
	{"lzma",   TO_BE32X('XZ\0\0'),  TO_BE32X('OTTX'), CreateLoadFilter<LZMALoadFilter>,   CreateSaveFilter<LZMASaveFilter>,   0, 2, 9},
#else
	{"lzma",   TO_BE32X('XZ\0\0'),  TO_BE32X('OTTX'), NULL,                               NULL,                               0, 0, 0},
#endif
};

/**
 * Return the savegameformat of the game. Whether it was created with ZLIB compression
 * uncompressed, or another type
 * @param s Name of the savegame format. If NULL it picks the first available one
 * @param compression_level Output for telling what compression level we want.
 * @return Pointer to SaveLoadFormat struct giving all characteristics of this type of savegame
 */
static const SaveLoadFormat *GetSavegameFormat(char *s, byte *compression_level)
{
	const SaveLoadFormat *def = lastof(_saveload_formats);

	/* find default savegame format, the highest one with which files can be written */
	while (!def->init_write) def--;

	if (!StrEmpty(s)) {
		/* Get the ":..." of the compression level out of the way */
		char *complevel = strrchr(s, ':');
		if (complevel != NULL) *complevel = '\0';

		for (const SaveLoadFormat *slf = &_saveload_formats[0]; slf != endof(_saveload_formats); slf++) {
			if (slf->init_write != NULL && strcmp(s, slf->name) == 0) {
				*compression_level = slf->default_compression;
				if (complevel != NULL) {
					/* There is a compression level in the string.
					 * First restore the : we removed to do proper name matching,
					 * then move the the begin of the actual version. */
					*complevel = ':';
					complevel++;

					/* Get the version and determine whether all went fine. */
					char *end;
					long level = strtol(complevel, &end, 10);
					if (end == complevel || level != Clamp(level, slf->min_compression, slf->max_compression)) {
						SetDParamStr(0, complevel);
						ShowErrorMessage(STR_CONFIG_ERROR, STR_CONFIG_ERROR_INVALID_SAVEGAME_COMPRESSION_LEVEL, WL_CRITICAL);
					} else {
						*compression_level = level;
					}
				}
				return slf;
			}
		}

		SetDParamStr(0, s);
		SetDParamStr(1, def->name);
		ShowErrorMessage(STR_CONFIG_ERROR, STR_CONFIG_ERROR_INVALID_SAVEGAME_COMPRESSION_ALGORITHM, WL_CRITICAL);

		/* Restore the string by adding the : back */
		if (complevel != NULL) *complevel = ':';
	}
	*compression_level = def->default_compression;
	return def;
}

/**
 * Return a savegame writer for the given compression format.
 * @param format Name of the savegame format, or NULL for the default format.
 * @param version Version number to write.
 * @param writer Underlying I/O writer.
 * @return A chain writer to write the savegame.
 */
ChainSaveFilter *GetSavegameWriter(char *format, uint version, SaveFilter *writer)
{
	extern const uint16 TRACERESTRICT_VERSION;

	static const uint32 magic = TO_BE32X('FTTD');

	byte compression;
	const SaveLoadFormat *fmt = GetSavegameFormat(format, &compression);

	writer->Write((const byte*)&magic, sizeof(magic));
	writer->Write((const byte*)&fmt->tag, sizeof(fmt->tag));

	uint32 hdr[4] = { TO_BE32(version), TO_BE32X('TRRT'), 0, TRACERESTRICT_VERSION };
	writer->Write((byte*)&hdr, sizeof(hdr));

	return fmt->init_write(writer, compression);
}

/**
 * Return the reader construction function corresponding to a tag.
 * @param tag Tag of the savegame format.
 * @return Pointer to the reader construction function for this type of savegame
 */
ChainLoadFilter* (*GetSavegameLoader(uint32 tag))(LoadFilter *chain)
{
	const SaveLoadFormat *fmt;

	for (fmt = _saveload_formats; fmt != endof(_saveload_formats); fmt++) {
		if (fmt->tag == tag) {
			if (fmt->init_load == NULL) {
				throw SlException(STR_GAME_SAVELOAD_ERROR_MISSING_LOADER, fmt->name);
			}

			return fmt->init_load;
		}
	}

	throw SlCorrupt("Unknown savegame compression tag");
}

/**
 * Return the reader construction function corresponding to a legacy tag.
 * @param tag Tag of the savegame format.
 * @return Pointer to the reader construction function for this type of savegame
 */
ChainLoadFilter* (*GetOTTDSavegameLoader(uint32 tag))(LoadFilter *chain)
{
	const SaveLoadFormat *fmt;

	for (fmt = _saveload_formats; fmt != endof(_saveload_formats); fmt++) {
		if (fmt->ottd_tag == tag) {
			if (fmt->init_load == NULL) {
				throw SlException(STR_GAME_SAVELOAD_ERROR_MISSING_LOADER, fmt->name);
			}

			return fmt->init_load;
		}
	}

	return NULL;
}

/**
 * Return the reader construction function corresponding to the buggy version 0 LZO format
 * @return Pointer to the reader construction function for this type of savegame
 */
ChainLoadFilter* (*GetLZO0SavegameLoader())(LoadFilter *chain)
{
#ifdef WITH_LZO
	return &CreateLZO0LoadFilter;
#else
	throw SlException(STR_GAME_SAVELOAD_ERROR_MISSING_LOADER, "lzo");
#endif
}

#if 0
/**
 * Function to get the type of the savegame by looking at the file header.
 * NOTICE: Not used right now, but could be used if extensions of savegames are garbled
 * @param file Savegame to be checked
 * @return SL_OLD_LOAD or SL_LOAD of the file
 */
int GetSavegameType(char *file)
{
	const SaveLoadFormat *fmt;
	uint32 hdr;
	FILE *f;
	int mode = SL_OLD_LOAD;

	f = fopen(file, "rb");
	if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
		DEBUG(sl, 0, "Savegame is obsolete or invalid format");
		mode = SL_LOAD; // don't try to get filename, just show name as it is written
	} else {
		/* see if we have any loader for this type. */
		for (fmt = _saveload_formats; fmt != endof(_saveload_formats); fmt++) {
			if (fmt->ottd_tag == hdr) {
				mode = SL_LOAD; // new type of savegame
				break;
			}
		}
	}

	fclose(f);
	return mode;
}
#endif
