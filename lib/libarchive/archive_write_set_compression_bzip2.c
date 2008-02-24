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

/* Don't compile this if we don't have bzlib. */
#if HAVE_BZLIB_H

__FBSDID("$FreeBSD: src/lib/libarchive/archive_write_set_compression_bzip2.c,v 1.12 2007/05/29 01:00:19 kientzle Exp $");

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
#ifdef HAVE_BZLIB_H
#include <bzlib.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_write_private.h"

struct private_data {
	bz_stream	 stream;
	int64_t		 total_in;
	char		*compressed;
	size_t		 compressed_buffer_size;
};


/*
 * Yuck.  bzlib.h is not const-correct, so I need this one bit
 * of ugly hackery to convert a const * pointer to a non-const pointer.
 */
#define	SET_NEXT_IN(st,src)					\
	(st)->stream.next_in = (char *)(uintptr_t)(const void *)(src)

static int	archive_compressor_bzip2_finish(struct archive_write *);
static int	archive_compressor_bzip2_init(struct archive_write *);
static int	archive_compressor_bzip2_write(struct archive_write *,
		    const void *, size_t);
static int	drive_compressor(struct archive_write *, struct private_data *,
		    int finishing);

/*
 * Allocate, initialize and return an archive object.
 */
int
archive_write_set_compression_bzip2(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_set_compression_bzip2");
	a->compressor.init = &archive_compressor_bzip2_init;
	return (ARCHIVE_OK);
}

/*
 * Setup callback.
 */
static int
archive_compressor_bzip2_init(struct archive_write *a)
{
	int ret;
	struct private_data *state;

	a->archive.compression_code = ARCHIVE_COMPRESSION_BZIP2;
	a->archive.compression_name = "bzip2";

	if (a->client_opener != NULL) {
		ret = (a->client_opener)(&a->archive, a->client_data);
		if (ret != 0)
			return (ret);
	}

	state = (struct private_data *)malloc(sizeof(*state));
	if (state == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate data for compression");
		return (ARCHIVE_FATAL);
	}
	memset(state, 0, sizeof(*state));

	state->compressed_buffer_size = a->bytes_per_block;
	state->compressed = (char *)malloc(state->compressed_buffer_size);

	if (state->compressed == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate data for compression buffer");
		free(state);
		return (ARCHIVE_FATAL);
	}

	state->stream.next_out = state->compressed;
	state->stream.avail_out = state->compressed_buffer_size;
	a->compressor.write = archive_compressor_bzip2_write;
	a->compressor.finish = archive_compressor_bzip2_finish;

	/* Initialize compression library */
	ret = BZ2_bzCompressInit(&(state->stream), 9, 0, 30);
	if (ret == BZ_OK) {
		a->compressor.data = state;
		return (ARCHIVE_OK);
	}

	/* Library setup failed: clean up. */
	archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
	    "Internal error initializing compression library");
	free(state->compressed);
	free(state);

	/* Override the error message if we know what really went wrong. */
	switch (ret) {
	case BZ_PARAM_ERROR:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "invalid setup parameter");
		break;
	case BZ_MEM_ERROR:
		archive_set_error(&a->archive, ENOMEM,
		    "Internal error initializing compression library: "
		    "out of memory");
		break;
	case BZ_CONFIG_ERROR:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "mis-compiled library");
		break;
	}

	return (ARCHIVE_FATAL);

}

/*
 * Write data to the compressed stream.
 *
 * Returns ARCHIVE_OK if all data written, error otherwise.
 */
static int
archive_compressor_bzip2_write(struct archive_write *a, const void *buff,
    size_t length)
{
	struct private_data *state;

	state = (struct private_data *)a->compressor.data;
	if (a->client_writer == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
		    "No write callback is registered?  "
		    "This is probably an internal programming error.");
		return (ARCHIVE_FATAL);
	}

	/* Update statistics */
	state->total_in += length;

	/* Compress input data to output buffer */
	SET_NEXT_IN(state, buff);
	state->stream.avail_in = length;
	if (drive_compressor(a, state, 0))
		return (ARCHIVE_FATAL);
	a->archive.file_position += length;
	return (ARCHIVE_OK);
}


/*
 * Finish the compression.
 */
static int
archive_compressor_bzip2_finish(struct archive_write *a)
{
	ssize_t block_length;
	int ret;
	struct private_data *state;
	ssize_t target_block_length;
	ssize_t bytes_written;
	unsigned tocopy;

	state = (struct private_data *)a->compressor.data;
	ret = ARCHIVE_OK;
	if (a->client_writer == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
		    "No write callback is registered?\n"
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
			state->total_in += state->stream.avail_in;
			tocopy -= state->stream.avail_in;
			ret = drive_compressor(a, state, 0);
			if (ret != ARCHIVE_OK)
				goto cleanup;
		}
	}

	/* Finish compression cycle. */
	if ((ret = drive_compressor(a, state, 1)))
		goto cleanup;

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
	bytes_written = (a->client_writer)(&a->archive, a->client_data,
	    state->compressed, block_length);

	/* TODO: Handle short write of final block. */
	if (bytes_written <= 0)
		ret = ARCHIVE_FATAL;
	else {
		a->archive.raw_position += ret;
		ret = ARCHIVE_OK;
	}

	/* Cleanup: shut down compressor, release memory, etc. */
cleanup:
	switch (BZ2_bzCompressEnd(&(state->stream))) {
	case BZ_OK:
		break;
	default:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
		    "Failed to clean up compressor");
		ret = ARCHIVE_FATAL;
	}

	free(state->compressed);
	free(state);
	return (ret);
}

/*
 * Utility function to push input data through compressor, writing
 * full output blocks as necessary.
 *
 * Note that this handles both the regular write case (finishing ==
 * false) and the end-of-archive case (finishing == true).
 */
static int
drive_compressor(struct archive_write *a, struct private_data *state, int finishing)
{
	ssize_t	bytes_written;
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
				/* Short write: Move remainder to
				 * front and keep filling */
				memmove(state->compressed,
				    state->compressed + bytes_written,
				    state->compressed_buffer_size - bytes_written);
			}

			a->archive.raw_position += bytes_written;
			state->stream.next_out = state->compressed +
			    state->compressed_buffer_size - bytes_written;
			state->stream.avail_out = bytes_written;
		}

		ret = BZ2_bzCompress(&(state->stream),
		    finishing ? BZ_FINISH : BZ_RUN);

		switch (ret) {
		case BZ_RUN_OK:
			/* In non-finishing case, did compressor
			 * consume everything? */
			if (!finishing && state->stream.avail_in == 0)
				return (ARCHIVE_OK);
			break;
		case BZ_FINISH_OK:  /* Finishing: There's more work to do */
			break;
		case BZ_STREAM_END: /* Finishing: all done */
			/* Only occurs in finishing case */
			return (ARCHIVE_OK);
		default:
			/* Any other return value indicates an error */
			archive_set_error(&a->archive,
			    ARCHIVE_ERRNO_PROGRAMMER,
			    "Bzip2 compression failed");
			return (ARCHIVE_FATAL);
		}
	}
}

#endif /* HAVE_BZLIB_H */
