/*-
 * Copyright (c) 2003-2004 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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

/* Don't compile this if we don't have zlib. */
#if HAVE_ZLIB_H

__FBSDID("$FreeBSD$");

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#include "archive.h"
#include "archive_private.h"

struct private_data {
	z_stream	 stream;
	int64_t		 total_in;
	unsigned char	*compressed;
	size_t		 compressed_buffer_size;
	unsigned long	 crc;
};


/*
 * Yuck.  zlib.h is not const-correct, so I need this one bit
 * of ugly hackery to convert a const * pointer to a non-const pointer.
 */
#define	SET_NEXT_IN(st,src)					\
	(st)->stream.next_in = (void *)(uintptr_t)(const void *)(src)

static int	archive_compressor_gzip_finish(struct archive *);
static int	archive_compressor_gzip_init(struct archive *);
static int	archive_compressor_gzip_write(struct archive *, const void *,
		    size_t);
static int	drive_compressor(struct archive *, struct private_data *,
		    int finishing);


/*
 * Allocate, initialize and return a archive object.
 */
int
archive_write_set_compression_gzip(struct archive *a)
{
	__archive_check_magic(a, ARCHIVE_WRITE_MAGIC, ARCHIVE_STATE_NEW, "archive_write_set_compression_gzip");
	a->compression_init = &archive_compressor_gzip_init;
	a->compression_code = ARCHIVE_COMPRESSION_GZIP;
	a->compression_name = "gzip";
	return (ARCHIVE_OK);
}

/*
 * Setup callback.
 */
static int
archive_compressor_gzip_init(struct archive *a)
{
	int ret;
	struct private_data *state;
	time_t t;

	a->compression_code = ARCHIVE_COMPRESSION_GZIP;
	a->compression_name = "gzip";

	if (a->client_opener != NULL) {
		ret = (a->client_opener)(a, a->client_data);
		if (ret != ARCHIVE_OK)
			return (ret);
	}

	state = (struct private_data *)malloc(sizeof(*state));
	if (state == NULL) {
		archive_set_error(a, ENOMEM,
		    "Can't allocate data for compression");
		return (ARCHIVE_FATAL);
	}
	memset(state, 0, sizeof(*state));

	state->compressed_buffer_size = a->bytes_per_block;
	state->compressed = malloc(state->compressed_buffer_size);
	state->crc = crc32(0L, NULL, 0);

	if (state->compressed == NULL) {
		archive_set_error(a, ENOMEM,
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

	a->compression_write = archive_compressor_gzip_write;
	a->compression_finish = archive_compressor_gzip_finish;

	/* Initialize compression library. */
	ret = deflateInit2(&(state->stream),
	    Z_DEFAULT_COMPRESSION,
	    Z_DEFLATED,
	    -15 /* < 0 to suppress zlib header */,
	    8,
	    Z_DEFAULT_STRATEGY);

	if (ret == Z_OK) {
		a->compression_data = state;
		return (0);
	}

	/* Library setup failed: clean up. */
	archive_set_error(a, ARCHIVE_ERRNO_MISC, "Internal error "
	    "initializing compression library");
	free(state->compressed);
	free(state);

	/* Override the error message if we know what really went wrong. */
	switch (ret) {
	case Z_STREAM_ERROR:
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing "
		    "compression library: invalid setup parameter");
		break;
	case Z_MEM_ERROR:
		archive_set_error(a, ENOMEM, "Internal error initializing "
		    "compression library");
		break;
	case Z_VERSION_ERROR:
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing "
		    "compression library: invalid library version");
		break;
	}

	return (ARCHIVE_FATAL);
}

/*
 * Write data to the compressed stream.
 */
static int
archive_compressor_gzip_write(struct archive *a, const void *buff,
    size_t length)
{
	struct private_data *state;
	int ret;

	state = a->compression_data;
	if (a->client_writer == NULL) {
		archive_set_error(a, ARCHIVE_ERRNO_PROGRAMMER,
		    "No write callback is registered?  "
		    "This is probably an internal programming error.");
		return (ARCHIVE_FATAL);
	}

	/* Update statistics */
	state->crc = crc32(state->crc, buff, length);
	state->total_in += length;

	/* Compress input data to output buffer */
	SET_NEXT_IN(state, buff);
	state->stream.avail_in = length;
	if ((ret = drive_compressor(a, state, 0)) != ARCHIVE_OK)
		return (ret);

	a->file_position += length;
	return (ARCHIVE_OK);
}


/*
 * Finish the compression...
 */
static int
archive_compressor_gzip_finish(struct archive *a)
{
	ssize_t block_length, target_block_length, bytes_written;
	int ret;
	struct private_data *state;
	unsigned tocopy;
	unsigned char trailer[8];

	state = a->compression_data;
	ret = 0;
	if (a->client_writer == NULL) {
		archive_set_error(a, ARCHIVE_ERRNO_PROGRAMMER,
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
		bytes_written = (a->client_writer)(a, a->client_data,
		    state->compressed, state->compressed_buffer_size);
		if (bytes_written <= 0) {
			ret = ARCHIVE_FATAL;
			goto cleanup;
		}
		a->raw_position += bytes_written;
		state->stream.next_out = state->compressed;
		state->stream.avail_out = state->compressed_buffer_size;
		memcpy(state->stream.next_out, trailer + tocopy, 8-tocopy);
		state->stream.next_out += 8-tocopy;
		state->stream.avail_out -= 8-tocopy;
	}

	/* Optionally, pad the final compressed block. */
	block_length = state->stream.next_out - state->compressed;


	/* Tricky calculation to determine size of last block. */
	target_block_length = block_length;
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
	bytes_written = (a->client_writer)(a, a->client_data,
	    state->compressed, block_length);
	if (bytes_written <= 0) {
		ret = ARCHIVE_FATAL;
		goto cleanup;
	}
	a->raw_position += bytes_written;

	/* Cleanup: shut down compressor, release memory, etc. */
cleanup:
	switch (deflateEnd(&(state->stream))) {
	case Z_OK:
		break;
	default:
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "Failed to clean up compressor");
		ret = ARCHIVE_FATAL;
	}
	free(state->compressed);
	free(state);

	/* Close the output */
	if (a->client_closer != NULL)
		(a->client_closer)(a, a->client_data);

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
drive_compressor(struct archive *a, struct private_data *state, int finishing)
{
	ssize_t bytes_written;
	int ret;

	for (;;) {
		if (state->stream.avail_out == 0) {
			bytes_written = (a->client_writer)(a, a->client_data,
			    state->compressed, state->compressed_buffer_size);
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
			a->raw_position += bytes_written;
			state->stream.next_out
			    = state->compressed +
			    state->compressed_buffer_size - bytes_written;
			state->stream.avail_out = bytes_written;
		}

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
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "GZip compression failed");
			return (ARCHIVE_FATAL);
		}
	}
}

#endif /* HAVE_ZLIB_H */
