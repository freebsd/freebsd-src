/*-
 * Copyright (c) 2009 Michihiro NAKAJIMA
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

__FBSDID("$FreeBSD$");

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
#ifdef HAVE_LZMA_H
#include <lzma.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_write_private.h"

#ifndef HAVE_LZMA_H
int
archive_write_set_compression_xz(struct archive *a)
{
	archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    "xz compression not supported on this platform");
	return (ARCHIVE_FATAL);
}

int
archive_write_set_compression_lzma(struct archive *a)
{
	archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    "lzma compression not supported on this platform");
	return (ARCHIVE_FATAL);
}
#else
/* Don't compile this if we don't have liblzma. */

struct private_data {
	lzma_stream	 stream;
	lzma_filter	 lzmafilters[2];
	lzma_options_lzma lzma_opt;
	int64_t		 total_in;
	unsigned char	*compressed;
	size_t		 compressed_buffer_size;
};

struct private_config {
	int		 compression_level;
};

static int	archive_compressor_xz_init(struct archive_write *);
static int	archive_compressor_xz_options(struct archive_write *,
		    const char *, const char *);
static int	archive_compressor_xz_finish(struct archive_write *);
static int	archive_compressor_xz_write(struct archive_write *,
		    const void *, size_t);
static int	drive_compressor(struct archive_write *, struct private_data *,
		    int finishing);


/*
 * Allocate, initialize and return a archive object.
 */
int
archive_write_set_compression_xz(struct archive *_a)
{
	struct private_config *config;
	struct archive_write *a = (struct archive_write *)_a;
	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_set_compression_xz");
	config = calloc(1, sizeof(*config));
	if (config == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Out of memory");
		return (ARCHIVE_FATAL);
	}
	a->compressor.config = config;
	a->compressor.finish = archive_compressor_xz_finish;
	config->compression_level = LZMA_PRESET_DEFAULT;
	a->compressor.init = &archive_compressor_xz_init;
	a->compressor.options = &archive_compressor_xz_options;
	a->archive.compression_code = ARCHIVE_COMPRESSION_XZ;
	a->archive.compression_name = "xz";
	return (ARCHIVE_OK);
}

/* LZMA is handled identically, we just need a different compression
 * code set.  (The liblzma setup looks at the code to determine
 * the one place that XZ and LZMA require different handling.) */
int
archive_write_set_compression_lzma(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	int r = archive_write_set_compression_xz(_a);
	if (r != ARCHIVE_OK)
		return (r);
	a->archive.compression_code = ARCHIVE_COMPRESSION_LZMA;
	a->archive.compression_name = "lzma";
	return (ARCHIVE_OK);
}

static int
archive_compressor_xz_init_stream(struct archive_write *a,
    struct private_data *state)
{
	int ret;

	state->stream = (lzma_stream)LZMA_STREAM_INIT;
	state->stream.next_out = state->compressed;
	state->stream.avail_out = state->compressed_buffer_size;
	if (a->archive.compression_code == ARCHIVE_COMPRESSION_XZ)
		ret = lzma_stream_encoder(&(state->stream),
		    state->lzmafilters, LZMA_CHECK_CRC64);
	else
		ret = lzma_alone_encoder(&(state->stream), &state->lzma_opt);
	if (ret == LZMA_OK)
		return (ARCHIVE_OK);

	switch (ret) {
	case LZMA_MEM_ERROR:
		archive_set_error(&a->archive, ENOMEM,
		    "Internal error initializing compression library: "
		    "Cannot allocate memory");
		break;
	default:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "It's a bug in liblzma");
		break;
	}
	return (ARCHIVE_FATAL);
}

/*
 * Setup callback.
 */
static int
archive_compressor_xz_init(struct archive_write *a)
{
	int ret;
	struct private_data *state;
	struct private_config *config;

	if (a->client_opener != NULL) {
		ret = (a->client_opener)(&a->archive, a->client_data);
		if (ret != ARCHIVE_OK)
			return (ret);
	}

	state = (struct private_data *)malloc(sizeof(*state));
	if (state == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate data for compression");
		return (ARCHIVE_FATAL);
	}
	memset(state, 0, sizeof(*state));
	config = a->compressor.config;

	/*
	 * See comment above.  We should set compressed_buffer_size to
	 * max(bytes_per_block, 65536), but the code can't handle that yet.
	 */
	state->compressed_buffer_size = a->bytes_per_block;
	state->compressed = (unsigned char *)malloc(state->compressed_buffer_size);
	if (state->compressed == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate data for compression buffer");
		free(state);
		return (ARCHIVE_FATAL);
	}
	a->compressor.write = archive_compressor_xz_write;

	/* Initialize compression library. */
	if (lzma_lzma_preset(&state->lzma_opt, config->compression_level)) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library");
		free(state->compressed);
		free(state);
	}
	state->lzmafilters[0].id = LZMA_FILTER_LZMA2;
	state->lzmafilters[0].options = &state->lzma_opt;
	state->lzmafilters[1].id = LZMA_VLI_UNKNOWN;/* Terminate */
	ret = archive_compressor_xz_init_stream(a, state);
	if (ret == LZMA_OK) {
		a->compressor.data = state;
		return (0);
	}
	/* Library setup failed: clean up. */
	free(state->compressed);
	free(state);

	return (ARCHIVE_FATAL);
}

/*
 * Set write options.
 */
static int
archive_compressor_xz_options(struct archive_write *a, const char *key,
    const char *value)
{
	struct private_config *config;

	config = (struct private_config *)a->compressor.config;
	if (strcmp(key, "compression-level") == 0) {
		if (value == NULL || !(value[0] >= '0' && value[0] <= '9') ||
		    value[1] != '\0')
			return (ARCHIVE_WARN);
		config->compression_level = value[0] - '0';
		if (config->compression_level > 6)
			config->compression_level = 6;
		return (ARCHIVE_OK);
	}

	return (ARCHIVE_WARN);
}

/*
 * Write data to the compressed stream.
 */
static int
archive_compressor_xz_write(struct archive_write *a, const void *buff,
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
	state->total_in += length;

	/* Compress input data to output buffer */
	state->stream.next_in = buff;
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
archive_compressor_xz_finish(struct archive_write *a)
{
	ssize_t block_length, target_block_length, bytes_written;
	int ret;
	struct private_data *state;
	unsigned tocopy;

	ret = ARCHIVE_OK;
	state = (struct private_data *)a->compressor.data;
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
				state->stream.next_in = a->nulls;
				state->stream.avail_in = tocopy < a->null_length ?
				    tocopy : a->null_length;
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
		lzma_end(&(state->stream));
		free(state->compressed);
		free(state);
	}
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

		ret = lzma_code(&(state->stream),
		    finishing ? LZMA_FINISH : LZMA_RUN );

		switch (ret) {
		case LZMA_OK:
			/* In non-finishing case, check if compressor
			 * consumed everything */
			if (!finishing && state->stream.avail_in == 0)
				return (ARCHIVE_OK);
			/* In finishing case, this return always means
			 * there's more work */
			break;
		case LZMA_STREAM_END:
			/* This return can only occur in finishing case. */
			if (finishing)
				return (ARCHIVE_OK);
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "lzma compression data error");
			return (ARCHIVE_FATAL);
		case LZMA_MEMLIMIT_ERROR:
			archive_set_error(&a->archive, ENOMEM,
			    "lzma compression error: "
			    "%ju MiB would have been needed",
			    (lzma_memusage(&(state->stream)) + 1024 * 1024 -1)
			    / (1024 * 1024));
			return (ARCHIVE_FATAL);
		default:
			/* Any other return value indicates an error. */
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "lzma compression failed:"
			    " lzma_code() call returned status %d",
			    ret);
			return (ARCHIVE_FATAL);
		}
	}
}

#endif /* HAVE_LZMA_H */
