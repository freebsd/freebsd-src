/*-
 * Copyright (c) 2014 Michihiro NAKAJIMA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_LZ4_H
#include <lz4.h>
#endif
#ifdef HAVE_LZ4HC_H
#include <lz4hc.h>
#endif

#include "archive.h"
#include "archive_endian.h"
#include "archive_private.h"
#include "archive_write_private.h"
#include "archive_xxhash.h"

#define LZ4_MAGICNUMBER	0x184d2204

struct lz4 {
	int		 compression_level;
	unsigned	 header_written:1;
	unsigned	 version_number:1;
	unsigned	 block_independence:1;
	unsigned	 block_checksum:1;
	unsigned	 stream_size:1;
	unsigned	 stream_checksum:1;
	unsigned	 preset_dictionary:1;
	unsigned	 block_maximum_size:3;
#if defined(HAVE_LIBLZ4) && LZ4_VERSION_MAJOR >= 1 && LZ4_VERSION_MINOR >= 2
	char		*out;
	char		*out_buffer;
	size_t		 out_buffer_size;
	size_t		 out_block_size;
	char		*in;
	char		*in_buffer_allocated;
	char		*in_buffer;
	size_t		 in_buffer_size;
	size_t		 block_size;

	void		*xxh32_state;
	void		*lz4_stream;
#else
	struct archive_write_program_data *pdata;
#endif
};

static int archive_filter_lz4_close(struct archive_write_filter *);
static int archive_filter_lz4_free(struct archive_write_filter *);
static int archive_filter_lz4_open(struct archive_write_filter *);
static int archive_filter_lz4_options(struct archive_write_filter *,
		    const char *, const char *);
static int archive_filter_lz4_write(struct archive_write_filter *,
		    const void *, size_t);
static void free_data(struct lz4 *);

/*
 * Add an lz4 compression filter to this write handle.
 */
int
archive_write_add_filter_lz4(struct archive *a)
{
	struct archive_write_filter *f;
	struct lz4 *lz4;
	int r;

	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_add_filter_lz4");

	lz4 = calloc(1, sizeof(*lz4));
	if (lz4 == NULL)
		goto memerr;
	/*
	 * Setup default settings.
	 */
	lz4->version_number = 0x01;
	lz4->block_independence = 1;
	lz4->block_checksum = 0;
	lz4->stream_size = 0;
	lz4->stream_checksum = 1;
	lz4->preset_dictionary = 0;
	lz4->block_maximum_size = 7;
#if defined(HAVE_LIBLZ4) && LZ4_VERSION_MAJOR >= 1 && LZ4_VERSION_MINOR >= 2
	lz4->compression_level = 1;

	r = ARCHIVE_OK;
#else
	/*
	 * We don't have lz4 library, and execute external lz4 program
	 * instead.
	 */
	lz4->pdata = __archive_write_program_allocate("lz4");
	if (lz4->pdata == NULL)
		goto memerr;
	lz4->compression_level = 0;

	archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    "Using external lz4 program");
	r = ARCHIVE_WARN;
#endif

	/*
	 * Setup a filter setting.
	 */
	f = __archive_write_allocate_filter(a);
	if (f == NULL)
		goto memerr;
	f->name = "lz4";
	f->code = ARCHIVE_FILTER_LZ4;
	f->data = lz4;
	f->options = archive_filter_lz4_options;
	f->open = archive_filter_lz4_open;
	f->write = archive_filter_lz4_write;
	f->close = archive_filter_lz4_close;
	f->free = archive_filter_lz4_free;

	return (r);
memerr:
	free_data(lz4);
	archive_set_error(a, ENOMEM, "Out of memory");
	return (ARCHIVE_FATAL);
}

static int
archive_filter_lz4_free(struct archive_write_filter *f)
{
	free_data(f->data);
	f->data = NULL;
	return (ARCHIVE_OK);
}

/*
 * Set write options.
 */
static int
archive_filter_lz4_options(struct archive_write_filter *f,
    const char *key, const char *value)
{
	struct lz4 *lz4 = f->data;

	if (strcmp(key, "compression-level") == 0) {
		int val;
		if (value == NULL || !((val = value[0] - '0') >= 1 && val <= 9) ||
		    value[1] != '\0') {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "compression-level invalid");
			return (ARCHIVE_FAILED);
		}

#ifndef HAVE_LZ4HC_H
		if(val >= 3)
		{
			archive_set_error(f->archive, ARCHIVE_ERRNO_PROGRAMMER,
				"High compression not included in this build");
			return (ARCHIVE_FATAL);
		}
#endif
		lz4->compression_level = val;
		return (ARCHIVE_OK);
	}
	if (strcmp(key, "stream-checksum") == 0) {
		lz4->stream_checksum = value != NULL;
		return (ARCHIVE_OK);
	}
	if (strcmp(key, "block-checksum") == 0) {
		lz4->block_checksum = value != NULL;
		return (ARCHIVE_OK);
	}
	if (strcmp(key, "block-size") == 0) {
		if (value == NULL || !(value[0] >= '4' && value[0] <= '7') ||
		    value[1] != '\0') {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "block-size invalid");
			return (ARCHIVE_FAILED);
		}
		lz4->block_maximum_size = value[0] - '0';
		return (ARCHIVE_OK);
	}
	if (strcmp(key, "block-dependence") == 0) {
		lz4->block_independence = value == NULL;
		return (ARCHIVE_OK);
	}

	/* Note: The "warn" return is just to inform the options
	 * supervisor that we didn't handle it.  It will generate
	 * a suitable error if no one used this option. */
	return (ARCHIVE_WARN);
}

#if defined(HAVE_LIBLZ4) && LZ4_VERSION_MAJOR >= 1 && LZ4_VERSION_MINOR >= 2
/* Don't compile this if we don't have liblz4. */

static int drive_compressor(struct archive_write_filter *, const char *,
    size_t);
static int drive_compressor_independence(struct archive_write_filter *,
    const char *, size_t);
static int drive_compressor_dependence(struct archive_write_filter *,
    const char *, size_t);
static int lz4_write_stream_descriptor(struct archive_write_filter *);
static ssize_t lz4_write_one_block(struct archive_write_filter *, const char *,
    size_t);


/*
 * Setup callback.
 */
static int
archive_filter_lz4_open(struct archive_write_filter *f)
{
	struct lz4 *lz4 = f->data;
	size_t required_size;
	static const size_t bkmap[] = { 64 * 1024, 256 * 1024, 1 * 1024 * 1024,
			   4 * 1024 * 1024 };
	size_t pre_block_size;

	if (lz4->block_maximum_size < 4)
		lz4->block_size = bkmap[0];
	else
		lz4->block_size = bkmap[lz4->block_maximum_size - 4];

	required_size = 4 + 15 + 4 + lz4->block_size + 4 + 4;
	if (lz4->out_buffer_size < required_size) {
		size_t bs = required_size, bpb;
		free(lz4->out_buffer);
		if (f->archive->magic == ARCHIVE_WRITE_MAGIC) {
			/* Buffer size should be a multiple number of
			 * the bytes per block for performance. */
			bpb = archive_write_get_bytes_per_block(f->archive);
			if (bpb > bs)
				bs = bpb;
			else if (bpb != 0) {
				bs += bpb;
				bs -= bs % bpb;
			}
		}
		lz4->out_block_size = bs;
		bs += required_size;
		lz4->out_buffer = malloc(bs);
		lz4->out = lz4->out_buffer;
		lz4->out_buffer_size = bs;
	}

	pre_block_size = (lz4->block_independence)? 0: 64 * 1024;
	if (lz4->in_buffer_size < lz4->block_size + pre_block_size) {
		free(lz4->in_buffer_allocated);
		lz4->in_buffer_size = lz4->block_size;
		lz4->in_buffer_allocated =
		    malloc(lz4->in_buffer_size + pre_block_size);
		lz4->in_buffer = lz4->in_buffer_allocated + pre_block_size;
		if (!lz4->block_independence && lz4->compression_level >= 3)
		    lz4->in_buffer = lz4->in_buffer_allocated;
		lz4->in = lz4->in_buffer;
		lz4->in_buffer_size = lz4->block_size;
	}

	if (lz4->out_buffer == NULL || lz4->in_buffer_allocated == NULL) {
		archive_set_error(f->archive, ENOMEM,
		    "Can't allocate data for compression buffer");
		return (ARCHIVE_FATAL);
	}

	return (ARCHIVE_OK);
}

/*
 * Write data to the out stream.
 *
 * Returns ARCHIVE_OK if all data written, error otherwise.
 */
static int
archive_filter_lz4_write(struct archive_write_filter *f,
    const void *buff, size_t length)
{
	struct lz4 *lz4 = f->data;
	int ret = ARCHIVE_OK;
	const char *p;
	size_t remaining;
	ssize_t size;

	/* If we haven't written a stream descriptor, we have to do it first. */
	if (!lz4->header_written) {
		ret = lz4_write_stream_descriptor(f);
		if (ret != ARCHIVE_OK)
			return (ret);
		lz4->header_written = 1;
	}

	p = (const char *)buff;
	remaining = length;
	while (remaining) {
		size_t l;
		/* Compress input data to output buffer */
		size = lz4_write_one_block(f, p, remaining);
		if (size < ARCHIVE_OK)
			return (ARCHIVE_FATAL);
		l = lz4->out - lz4->out_buffer;
		if (l >= lz4->out_block_size) {
			ret = __archive_write_filter(f->next_filter,
			    lz4->out_buffer, lz4->out_block_size);
			l -= lz4->out_block_size;
			memcpy(lz4->out_buffer,
			    lz4->out_buffer + lz4->out_block_size, l);
			lz4->out = lz4->out_buffer + l;
			if (ret < ARCHIVE_WARN)
				break;
		}
		p += size;
		remaining -= size;
	}

	return (ret);
}

/*
 * Finish the compression.
 */
static int
archive_filter_lz4_close(struct archive_write_filter *f)
{
	struct lz4 *lz4 = f->data;
	int ret;

	/* Finish compression cycle. */
	ret = (int)lz4_write_one_block(f, NULL, 0);
	if (ret >= 0) {
		/*
		 * Write the last block and the end of the stream data.
		 */

		/* Write End Of Stream. */
		memset(lz4->out, 0, 4); lz4->out += 4;
		/* Write Stream checksum if needed. */
		if (lz4->stream_checksum) {
			unsigned int checksum;
			checksum = __archive_xxhash.XXH32_digest(
					lz4->xxh32_state);
			lz4->xxh32_state = NULL;
			archive_le32enc(lz4->out, checksum);
			lz4->out += 4;
		}
		ret = __archive_write_filter(f->next_filter,
			    lz4->out_buffer, lz4->out - lz4->out_buffer);
	}
	return ret;
}

static int
lz4_write_stream_descriptor(struct archive_write_filter *f)
{
	struct lz4 *lz4 = f->data;
	uint8_t *sd;

	sd = (uint8_t *)lz4->out;
	/* Write Magic Number. */
	archive_le32enc(&sd[0], LZ4_MAGICNUMBER);
	/* FLG */
	sd[4] = (lz4->version_number << 6)
	      | (lz4->block_independence << 5)
	      | (lz4->block_checksum << 4)
	      | (lz4->stream_size << 3)
	      | (lz4->stream_checksum << 2)
	      | (lz4->preset_dictionary << 0);
	/* BD */
	sd[5] = (lz4->block_maximum_size << 4);
	sd[6] = (__archive_xxhash.XXH32(&sd[4], 2, 0) >> 8) & 0xff;
	lz4->out += 7;
	if (lz4->stream_checksum) {
		lz4->xxh32_state = __archive_xxhash.XXH32_init(0);
		if (lz4->xxh32_state == NULL)
			return (ARCHIVE_FATAL);
	} else
		lz4->xxh32_state = NULL;
	return (ARCHIVE_OK);
}

static ssize_t
lz4_write_one_block(struct archive_write_filter *f, const char *p,
    size_t length)
{
	struct lz4 *lz4 = f->data;
	ssize_t r;

	if (p == NULL) {
		/* Compress remaining uncompressed data. */
		if (lz4->in_buffer == lz4->in)
			return 0;
		else {
			size_t l = lz4->in - lz4->in_buffer;
			r = drive_compressor(f, lz4->in_buffer, l);
			if (r == ARCHIVE_OK)
				r = (ssize_t)l;
		}
	} else if ((lz4->block_independence || lz4->compression_level < 3) &&
	    lz4->in_buffer == lz4->in && length >= lz4->block_size) {
		r = drive_compressor(f, p, lz4->block_size);
		if (r == ARCHIVE_OK)
			r = (ssize_t)lz4->block_size;
	} else {
		size_t remaining_size = lz4->in_buffer_size -
			(lz4->in - lz4->in_buffer);
		size_t l = (remaining_size > length)? length: remaining_size;
		memcpy(lz4->in, p, l);
		lz4->in += l;
		if (l == remaining_size) {
			r = drive_compressor(f, lz4->in_buffer,
			    lz4->block_size);
			if (r == ARCHIVE_OK)
				r = (ssize_t)l;
			lz4->in = lz4->in_buffer;
		} else
			r = (ssize_t)l;
	}

	return (r);
}


/*
 * Utility function to push input data through compressor, writing
 * full output blocks as necessary.
 *
 * Note that this handles both the regular write case (finishing ==
 * false) and the end-of-archive case (finishing == true).
 */
static int
drive_compressor(struct archive_write_filter *f, const char *p, size_t length)
{
	struct lz4 *lz4 = f->data;

	if (lz4->stream_checksum)
		__archive_xxhash.XXH32_update(lz4->xxh32_state,
			p, (int)length);
	if (lz4->block_independence)
		return drive_compressor_independence(f, p, length);
	else
		return drive_compressor_dependence(f, p, length);
}

static int
drive_compressor_independence(struct archive_write_filter *f, const char *p,
    size_t length)
{
	struct lz4 *lz4 = f->data;
	unsigned int outsize;

#ifdef HAVE_LZ4HC_H
	if (lz4->compression_level >= 3)
#if LZ4_VERSION_MAJOR >= 1 && LZ4_VERSION_MINOR >= 7
		outsize = LZ4_compress_HC(p, lz4->out + 4,
		     (int)length, (int)lz4->block_size,
		    lz4->compression_level);
#else
		outsize = LZ4_compressHC2_limitedOutput(p, lz4->out + 4,
		    (int)length, (int)lz4->block_size,
		    lz4->compression_level);
#endif
	else
#endif
#if LZ4_VERSION_MAJOR >= 1 && LZ4_VERSION_MINOR >= 7
		outsize = LZ4_compress_default(p, lz4->out + 4,
		    (int)length, (int)lz4->block_size);
#else
		outsize = LZ4_compress_limitedOutput(p, lz4->out + 4,
		    (int)length, (int)lz4->block_size);
#endif

	if (outsize) {
		/* The buffer is compressed. */
		archive_le32enc(lz4->out, outsize);
		lz4->out += 4;
	} else {
		/* The buffer is not compressed. The compressed size was
		 * bigger than its uncompressed size. */
		archive_le32enc(lz4->out, (uint32_t)(length | 0x80000000));
		lz4->out += 4;
		memcpy(lz4->out, p, length);
		outsize = (uint32_t)length;
	}
	lz4->out += outsize;
	if (lz4->block_checksum) {
		unsigned int checksum =
		    __archive_xxhash.XXH32(lz4->out - outsize, outsize, 0);
		archive_le32enc(lz4->out, checksum);
		lz4->out += 4;
	}
	return (ARCHIVE_OK);
}

static int
drive_compressor_dependence(struct archive_write_filter *f, const char *p,
    size_t length)
{
	struct lz4 *lz4 = f->data;
	int outsize;

#define DICT_SIZE	(64 * 1024)
#ifdef HAVE_LZ4HC_H
	if (lz4->compression_level >= 3) {
		if (lz4->lz4_stream == NULL) {
#if LZ4_VERSION_MAJOR >= 1 && LZ4_VERSION_MINOR >= 7
			lz4->lz4_stream = LZ4_createStreamHC();
			LZ4_resetStreamHC(lz4->lz4_stream, lz4->compression_level);
#else
			lz4->lz4_stream =
			    LZ4_createHC(lz4->in_buffer_allocated);
#endif
			if (lz4->lz4_stream == NULL) {
				archive_set_error(f->archive, ENOMEM,
				    "Can't allocate data for compression"
				    " buffer");
				return (ARCHIVE_FATAL);
			}
		}
		else
			LZ4_loadDictHC(lz4->lz4_stream, lz4->in_buffer_allocated, DICT_SIZE);

#if LZ4_VERSION_MAJOR >= 1 && LZ4_VERSION_MINOR >= 7
		outsize = LZ4_compress_HC_continue(
		    lz4->lz4_stream, p, lz4->out + 4, (int)length,
		    (int)lz4->block_size);
#else
		outsize = LZ4_compressHC2_limitedOutput_continue(
		    lz4->lz4_stream, p, lz4->out + 4, (int)length,
		    (int)lz4->block_size, lz4->compression_level);
#endif
	} else
#endif
	{
		if (lz4->lz4_stream == NULL) {
			lz4->lz4_stream = LZ4_createStream();
			if (lz4->lz4_stream == NULL) {
				archive_set_error(f->archive, ENOMEM,
				    "Can't allocate data for compression"
				    " buffer");
				return (ARCHIVE_FATAL);
			}
		}
		else
			LZ4_loadDict(lz4->lz4_stream, lz4->in_buffer_allocated, DICT_SIZE);

#if LZ4_VERSION_MAJOR >= 1 && LZ4_VERSION_MINOR >= 7
		outsize = LZ4_compress_fast_continue(
		    lz4->lz4_stream, p, lz4->out + 4, (int)length,
		    (int)lz4->block_size, 1);
#else
		outsize = LZ4_compress_limitedOutput_continue(
		    lz4->lz4_stream, p, lz4->out + 4, (int)length,
		    (int)lz4->block_size);
#endif
	}

	if (outsize) {
		/* The buffer is compressed. */
		archive_le32enc(lz4->out, outsize);
		lz4->out += 4;
	} else {
		/* The buffer is not compressed. The compressed size was
		 * bigger than its uncompressed size. */
		archive_le32enc(lz4->out, (uint32_t)(length | 0x80000000));
		lz4->out += 4;
		memcpy(lz4->out, p, length);
		outsize = (uint32_t)length;
	}
	lz4->out += outsize;
	if (lz4->block_checksum) {
		unsigned int checksum =
		    __archive_xxhash.XXH32(lz4->out - outsize, outsize, 0);
		archive_le32enc(lz4->out, checksum);
		lz4->out += 4;
	}

	if (length == lz4->block_size) {
#ifdef HAVE_LZ4HC_H
		if (lz4->compression_level >= 3) {
#if LZ4_VERSION_MAJOR >= 1 && LZ4_VERSION_MINOR >= 7
			LZ4_saveDictHC(lz4->lz4_stream, lz4->in_buffer_allocated, DICT_SIZE);
#else
			LZ4_slideInputBufferHC(lz4->lz4_stream);
#endif
			lz4->in_buffer = lz4->in_buffer_allocated + DICT_SIZE;
		}
		else
#endif
			LZ4_saveDict(lz4->lz4_stream,
			    lz4->in_buffer_allocated, DICT_SIZE);
#undef DICT_SIZE
	}
	return (ARCHIVE_OK);
}

static void
free_data(struct lz4 *lz4)
{
	if (lz4 != NULL) {
		if (lz4->lz4_stream != NULL) {
#ifdef HAVE_LZ4HC_H
			if (lz4->compression_level >= 3)
#if LZ4_VERSION_MAJOR >= 1 && LZ4_VERSION_MINOR >= 7
				LZ4_freeStreamHC(lz4->lz4_stream);
#else
				LZ4_freeHC(lz4->lz4_stream);
#endif
			else
#endif
#if LZ4_VERSION_MINOR >= 3
				LZ4_freeStream(lz4->lz4_stream);
#else
				LZ4_free(lz4->lz4_stream);
#endif
		}
		free(lz4->out_buffer);
		free(lz4->in_buffer_allocated);
		free(lz4->xxh32_state);
		free(lz4);
	}
}

#else /* HAVE_LIBLZ4 */

static int
archive_filter_lz4_open(struct archive_write_filter *f)
{
	struct lz4 *lz4 = f->data;
	struct archive_string as;
	int r;

	archive_string_init(&as);
	archive_strcpy(&as, "lz4 -z -q -q");

	/* Specify a compression level. */
	if (lz4->compression_level > 0) {
		archive_strcat(&as, " -");
		archive_strappend_char(&as, '0' + lz4->compression_level);
	}
	/* Specify a block size. */
	archive_strcat(&as, " -B");
	archive_strappend_char(&as, '0' + lz4->block_maximum_size);

	if (lz4->block_checksum)
		archive_strcat(&as, " -BX");
	if (lz4->stream_checksum == 0)
		archive_strcat(&as, " --no-frame-crc");
	if (lz4->block_independence == 0)
		archive_strcat(&as, " -BD");

	r = __archive_write_program_open(f, lz4->pdata, as.s);
	archive_string_free(&as);
	return (r);
}

static int
archive_filter_lz4_write(struct archive_write_filter *f, const void *buff,
    size_t length)
{
	struct lz4 *lz4 = f->data;

	return __archive_write_program_write(f, lz4->pdata, buff, length);
}

static int
archive_filter_lz4_close(struct archive_write_filter *f)
{
	struct lz4 *lz4 = f->data;

	return __archive_write_program_close(f, lz4->pdata);
}

static void
free_data(struct lz4 *lz4)
{
	if (lz4 != NULL) {
		__archive_write_program_free(lz4->pdata);
		free(lz4);
	}
}

#endif /* HAVE_LIBLZ4 */
