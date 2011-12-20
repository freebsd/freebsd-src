/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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

__FBSDID("$FreeBSD: head/lib/libarchive/archive_write_set_compression_gzip.c 201081 2009-12-28 02:04:42Z kientzle $");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <time.h>
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_write_private.h"

#ifndef HAVE_ZLIB_H
int
archive_write_set_compression_gzip(struct archive *a)
{
	archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    "gzip compression not supported on this platform");
	return (ARCHIVE_FATAL);
}
#else
/* Don't compile this if we don't have zlib. */

struct private_data {
	z_stream	 stream;
	int64_t		 total_in;
	unsigned char	*compressed;
	size_t		 compressed_buffer_size;
	unsigned long	 crc;
};

struct private_config {
	int		 compression_level;
};


/*
 * Yuck.  zlib.h is not const-correct, so I need this one bit
 * of ugly hackery to convert a const * pointer to a non-const pointer.
 */
#define	SET_NEXT_IN(st,src)					\
	(st)->stream.next_in = (Bytef *)(uintptr_t)(const void *)(src)

static int	archive_compressor_gzip_finish(struct archive_write *);
static int	archive_compressor_gzip_init(struct archive_write *);
static int	archive_compressor_gzip_options(struct archive_write *,
		    const char *, const char *);
static int	archive_compressor_gzip_write(struct archive_write *,
		    const void *, size_t);
static int	drive_compressor(struct archive_write *, struct private_data *,
		    int finishing);


/*
 * Allocate, initialize and return a archive object.
 */
int
archive_write_set_compression_gzip(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct private_config *config;
	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_set_compression_gzip");
	config = malloc(sizeof(*config));
	if (config == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Out of memory");
		return (ARCHIVE_FATAL);
	}
	a->compressor.config = config;
	a->compressor.finish = &archive_compressor_gzip_finish;
	config->compression_level = Z_DEFAULT_COMPRESSION;
	a->compressor.init = &archive_compressor_gzip_init;
	a->compressor.options = &archive_compressor_gzip_options;
	a->archive.compression_code = ARCHIVE_COMPRESSION_GZIP;
	a->archive.compression_name = "gzip";
	return (ARCHIVE_OK);
}

/*
 * Setup callback.
 */
static int
archive_compressor_gzip_init(struct archive_write *a)
{
	int ret;
	struct private_data *state;
	struct private_config *config;
	time_t t;

	config = (struct private_config *)a->compressor.config;

	if (a->client_opener != NULL) {
		ret = (a->client_opener)(&a->archive, a->client_data);
		if (ret != ARCHIVE_OK)
			return (ret);
	}

	/*
	 * The next check is a temporary workaround until the gzip
	 * code can be overhauled some.  The code should not require
	 * that compressed_buffer_size == bytes_per_block.  Removing
	 * this assumption will allow us to compress larger chunks at
	 * a time, which should improve overall performance
	 * marginally.  As a minor side-effect, such a cleanup would
	 * allow us to support truly arbitrary block sizes.
	 */
	if (a->bytes_per_block < 10) {
		archive_set_error(&a->archive, EINVAL,
		    "GZip compressor requires a minimum 10 byte block size");
		return (ARCHIVE_FATAL);
	}

	state = (struct private_data *)malloc(sizeof(*state));
	if (state == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate data for compression");
		return (ARCHIVE_FATAL);
	}
	memset(state, 0, sizeof(*state));

	/*
	 * See comment above.  We should set compressed_buffer_size to
	 * max(bytes_per_block, 65536), but the code can't handle that yet.
	 */
	state->compressed_buffer_size = a->bytes_per_block;
	state->compressed = (unsigned char *)malloc(state->compressed_buffer_size);
	state->crc = crc32(0L, NULL, 0);

	if (state->compressed == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate data for compression buffer");
		free(state);
		return (ARCHIVE_FATAL);
	}

	state->stream.next_out = state->compressed;
	state->stream.avail_out = state->compressed_buffer_size;

	/* Prime output buffer with a gzip header. */
	t = time(NULL);
	state->compressed[0] = 0x1f; /* GZip signature bytes */
	state->compressed[1] = 0x8b;
	state->compressed[2] = 0x08; /* "Deflate" compression */
	state->compressed[3] = 0; /* No options */
	state->compressed[4] = (t)&0xff;  /* Timestamp */
	state->compressed[5] = (t>>8)&0xff;
	state->compressed[6] = (t>>16)&0xff;
	state->compressed[7] = (t>>24)&0xff;
	state->compressed[8] = 0; /* No deflate options */
	state->compressed[9] = 3; /* OS=Unix */
	state->stream.next_out += 10;
	state->stream.avail_out -= 10;

	a->compressor.write = archive_compressor_gzip_write;

	/* Initialize compression library. */
	ret = deflateInit2(&(state->stream),
	    config->compression_level,
	    Z_DEFLATED,
	    -15 /* < 0 to suppress zlib header */,
	    8,
	    Z_DEFAULT_STRATEGY);

	if (ret == Z_OK) {
		a->compressor.data = state;
		return (0);
	}

	/* Library setup failed: clean up. */
	archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC, "Internal error "
	    "initializing compression library");
	free(state->compressed);
	free(state);

	/* Override the error message if we know what really went wrong. */
	switch (ret) {
	case Z_STREAM_ERROR:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing "
		    "compression library: invalid setup parameter");
		break;
	case Z_MEM_ERROR:
		archive_set_error(&a->archive, ENOMEM, "Internal error initializing "
		    "compression library");
		break;
	case Z_VERSION_ERROR:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing "
		    "compression library: invalid library version");
		break;
	}

	return (ARCHIVE_FATAL);
}

/*
 * Set write options.
 */
static int
archive_compressor_gzip_options(struct archive_write *a, const char *key,
    const char *value)
{
	struct private_config *config;

	config = (struct private_config *)a->compressor.config;
	if (strcmp(key, "compression-level") == 0) {
		if (value == NULL || !(value[0] >= '0' && value[0] <= '9') ||
		    value[1] != '\0')
			return (ARCHIVE_WARN);
		config->compression_level = value[0] - '0';
		return (ARCHIVE_OK);
	}

	return (ARCHIVE_WARN);
}

/*
 * Write data to the compressed stream.
 */
static int
archive_compressor_gzip_write(struct archive_write *a, const void *buff,
    size_t length)
{
	struct private_data *state;
	int ret;

	state = (struct private_data *)a->compressor.data;
	if (a->client_writer == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
		    "No write callback is registered?  "
		    "This is probably an internal programming error.");
		return (ARCHIVE_FATAL);
	}

	/* Update statistics */
	state->crc = crc32(state->crc, (const Bytef *)buff, length);
	state->total_in += length;

	/* Compress input data to output buffer */
	SET_NEXT_IN(state, buff);
	state->stream.avail_in = length;
	if ((ret = drive_compressor(a, state, 0)) != ARCHIVE_OK)
		return (ret);

	a->archive.file_position += length;
	return (ARCHIVE_OK);
}

/*
 * Finish the compression...
 */
static int
archive_compressor_gzip_finish(struct archive_write *a)
{
	ssize_t block_length, target_block_length, bytes_written;
	int ret;
	struct private_data *state;
	unsigned tocopy;
	unsigned char trailer[8];

	state = (struct private_data *)a->compressor.data;
	ret = 0;
	if (state != NULL) {
		if (a->client_writer == NULL) {
			archive_set_error(&a->archive,
			    ARCHIVE_ERRNO_PROGRAMMER,
			    "No write callback is registered?  "
			    "This is probably an internal programming error.");
			ret = ARCHIVE_FATAL;
			goto cleanup;
		}

		/* By default, always pad the uncompressed data. */
		if (a->pad_uncompressed) {
			tocopy = a->bytes_per_block -
			    (state->total_in % a->bytes_per_block);
			while (tocopy > 0 && tocopy < (unsigned)a->bytes_per_block) {
				SET_NEXT_IN(state, a->nulls);
				state->stream.avail_in = tocopy < a->null_length ?
				    tocopy : a->null_length;
				state->crc = crc32(state->crc, a->nulls,
				    state->stream.avail_in);
				state->total_in += state->stream.avail_in;
				tocopy -= state->stream.avail_in;
				ret = drive_compressor(a, state, 0);
				if (ret != ARCHIVE_OK)
					goto cleanup;
			}
		}

		/* Finish compression cycle */
		if (((ret = drive_compressor(a, state, 1))) != ARCHIVE_OK)
			goto cleanup;

		/* Build trailer: 4-byte CRC and 4-byte length. */
		trailer[0] = (state->crc)&0xff;
		trailer[1] = (state->crc >> 8)&0xff;
		trailer[2] = (state->crc >> 16)&0xff;
		trailer[3] = (state->crc >> 24)&0xff;
		trailer[4] = (state->total_in)&0xff;
		trailer[5] = (state->total_in >> 8)&0xff;
		trailer[6] = (state->total_in >> 16)&0xff;
		trailer[7] = (state->total_in >> 24)&0xff;

		/* Add trailer to current block. */
		tocopy = 8;
		if (tocopy > state->stream.avail_out)
			tocopy = state->stream.avail_out;
		memcpy(state->stream.next_out, trailer, tocopy);
		state->stream.next_out += tocopy;
		state->stream.avail_out -= tocopy;

		/* If it overflowed, flush and start a new block. */
		if (tocopy < 8) {
			bytes_written = (a->client_writer)(&a->archive, a->client_data,
			    state->compressed, state->compressed_buffer_size);
			if (bytes_written <= 0) {
				ret = ARCHIVE_FATAL;
				goto cleanup;
			}
			a->archive.raw_position += bytes_written;
			state->stream.next_out = state->compressed;
			state->stream.avail_out = state->compressed_buffer_size;
			memcpy(state->stream.next_out, trailer + tocopy, 8-tocopy);
			state->stream.next_out += 8-tocopy;
			state->stream.avail_out -= 8-tocopy;
		}

		/* Optionally, pad the final compressed block. */
		block_length = state->stream.next_out - state->compressed;

		/* Tricky calculation to determine size of last block. */
		if (a->bytes_in_last_block <= 0)
			/* Default or Zero: pad to full block */
			target_block_length = a->bytes_per_block;
		else
			/* Round length to next multiple of bytes_in_last_block. */
			target_block_length = a->bytes_in_last_block *
			    ( (block_length + a->bytes_in_last_block - 1) /
				a->bytes_in_last_block);
		if (target_block_length > a->bytes_per_block)
			target_block_length = a->bytes_per_block;
		if (block_length < target_block_length) {
			memset(state->stream.next_out, 0,
			    target_block_length - block_length);
			block_length = target_block_length;
		}

		/* Write the last block */
		bytes_written = (a->client_writer)(&a->archive, a->client_data,
		    state->compressed, block_length);
		if (bytes_written <= 0) {
			ret = ARCHIVE_FATAL;
			goto cleanup;
		}
		a->archive.raw_position += bytes_written;

		/* Cleanup: shut down compressor, release memory, etc. */
	cleanup:
		switch (deflateEnd(&(state->stream))) {
		case Z_OK:
			break;
		default:
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Failed to clean up compressor");
			ret = ARCHIVE_FATAL;
		}
		free(state->compressed);
		free(state);
	}
	/* Clean up config area even if we never initialized. */
	free(a->compressor.config);
	a->compressor.config = NULL;
	return (ret);
}

/*
 * Utility function to push input data through compressor,
 * writing full output blocks as necessary.
 *
 * Note that this handles both the regular write case (finishing ==
 * false) and the end-of-archive case (finishing == true).
 */
static int
drive_compressor(struct archive_write *a, struct private_data *state, int finishing)
{
	ssize_t bytes_written;
	int ret;

	for (;;) {
		if (state->stream.avail_out == 0) {
			bytes_written = (a->client_writer)(&a->archive,
			    a->client_data, state->compressed,
			    state->compressed_buffer_size);
			if (bytes_written <= 0) {
				/* TODO: Handle this write failure */
				return (ARCHIVE_FATAL);
			} else if ((size_t)bytes_written < state->compressed_buffer_size) {
				/* Short write: Move remaining to
				 * front of block and keep filling */
				memmove(state->compressed,
				    state->compressed + bytes_written,
				    state->compressed_buffer_size - bytes_written);
			}
			a->archive.raw_position += bytes_written;
			state->stream.next_out
			    = state->compressed +
			    state->compressed_buffer_size - bytes_written;
			state->stream.avail_out = bytes_written;
		}

		/* If there's nothing to do, we're done. */
		if (!finishing && state->stream.avail_in == 0)
			return (ARCHIVE_OK);

		ret = deflate(&(state->stream),
		    finishing ? Z_FINISH : Z_NO_FLUSH );

		switch (ret) {
		case Z_OK:
			/* In non-finishing case, check if compressor
			 * consumed everything */
			if (!finishing && state->stream.avail_in == 0)
				return (ARCHIVE_OK);
			/* In finishing case, this return always means
			 * there's more work */
			break;
		case Z_STREAM_END:
			/* This return can only occur in finishing case. */
			return (ARCHIVE_OK);
		default:
			/* Any other return value indicates an error. */
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "GZip compression failed:"
			    " deflate() call returned status %d",
			    ret);
			return (ARCHIVE_FATAL);
		}
	}
}

#endif /* HAVE_ZLIB_H */
