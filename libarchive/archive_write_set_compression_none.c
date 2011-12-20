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
__FBSDID("$FreeBSD: head/lib/libarchive/archive_write_set_compression_none.c 201080 2009-12-28 02:03:54Z kientzle $");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_write_private.h"

static int	archive_compressor_none_finish(struct archive_write *a);
static int	archive_compressor_none_init(struct archive_write *);
static int	archive_compressor_none_write(struct archive_write *,
		    const void *, size_t);

struct archive_none {
	char	*buffer;
	ssize_t	 buffer_size;
	char	*next;		/* Current insert location */
	ssize_t	 avail;		/* Free space left in buffer */
};

int
archive_write_set_compression_none(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_set_compression_none");
	a->compressor.init = &archive_compressor_none_init;
	return (0);
}

/*
 * Setup callback.
 */
static int
archive_compressor_none_init(struct archive_write *a)
{
	int ret;
	struct archive_none *state;

	a->archive.compression_code = ARCHIVE_COMPRESSION_NONE;
	a->archive.compression_name = "none";

	if (a->client_opener != NULL) {
		ret = (a->client_opener)(&a->archive, a->client_data);
		if (ret != 0)
			return (ret);
	}

	state = (struct archive_none *)malloc(sizeof(*state));
	if (state == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate data for output buffering");
		return (ARCHIVE_FATAL);
	}
	memset(state, 0, sizeof(*state));

	state->buffer_size = a->bytes_per_block;
	if (state->buffer_size != 0) {
		state->buffer = (char *)malloc(state->buffer_size);
		if (state->buffer == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't allocate output buffer");
			free(state);
			return (ARCHIVE_FATAL);
		}
	}

	state->next = state->buffer;
	state->avail = state->buffer_size;

	a->compressor.data = state;
	a->compressor.write = archive_compressor_none_write;
	a->compressor.finish = archive_compressor_none_finish;
	return (ARCHIVE_OK);
}

/*
 * Write data to the stream.
 */
static int
archive_compressor_none_write(struct archive_write *a, const void *vbuff,
    size_t length)
{
	const char *buff;
	ssize_t remaining, to_copy;
	ssize_t bytes_written;
	struct archive_none *state;

	state = (struct archive_none *)a->compressor.data;
	buff = (const char *)vbuff;
	if (a->client_writer == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
		    "No write callback is registered?  "
		    "This is probably an internal programming error.");
		return (ARCHIVE_FATAL);
	}

	remaining = length;

	/*
	 * If there is no buffer for blocking, just pass the data
	 * straight through to the client write callback.  In
	 * particular, this supports "no write delay" operation for
	 * special applications.  Just set the block size to zero.
	 */
	if (state->buffer_size == 0) {
		while (remaining > 0) {
			bytes_written = (a->client_writer)(&a->archive,
			    a->client_data, buff, remaining);
			if (bytes_written <= 0)
				return (ARCHIVE_FATAL);
			a->archive.raw_position += bytes_written;
			remaining -= bytes_written;
			buff += bytes_written;
		}
		a->archive.file_position += length;
		return (ARCHIVE_OK);
	}

	/* If the copy buffer isn't empty, try to fill it. */
	if (state->avail < state->buffer_size) {
		/* If buffer is not empty... */
		/* ... copy data into buffer ... */
		to_copy = (remaining > state->avail) ?
		    state->avail : remaining;
		memcpy(state->next, buff, to_copy);
		state->next += to_copy;
		state->avail -= to_copy;
		buff += to_copy;
		remaining -= to_copy;
		/* ... if it's full, write it out. */
		if (state->avail == 0) {
			bytes_written = (a->client_writer)(&a->archive,
			    a->client_data, state->buffer, state->buffer_size);
			if (bytes_written <= 0)
				return (ARCHIVE_FATAL);
			/* XXX TODO: if bytes_written < state->buffer_size */
			a->archive.raw_position += bytes_written;
			state->next = state->buffer;
			state->avail = state->buffer_size;
		}
	}

	while (remaining > state->buffer_size) {
		/* Write out full blocks directly to client. */
		bytes_written = (a->client_writer)(&a->archive,
		    a->client_data, buff, state->buffer_size);
		if (bytes_written <= 0)
			return (ARCHIVE_FATAL);
		a->archive.raw_position += bytes_written;
		buff += bytes_written;
		remaining -= bytes_written;
	}

	if (remaining > 0) {
		/* Copy last bit into copy buffer. */
		memcpy(state->next, buff, remaining);
		state->next += remaining;
		state->avail -= remaining;
	}

	a->archive.file_position += length;
	return (ARCHIVE_OK);
}


/*
 * Finish the compression.
 */
static int
archive_compressor_none_finish(struct archive_write *a)
{
	ssize_t block_length;
	ssize_t target_block_length;
	ssize_t bytes_written;
	int ret;
	struct archive_none *state;

	state = (struct archive_none *)a->compressor.data;
	ret = ARCHIVE_OK;
	if (a->client_writer == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
		    "No write callback is registered?  "
		    "This is probably an internal programming error.");
		return (ARCHIVE_FATAL);
	}

	/* If there's pending data, pad and write the last block */
	if (state->next != state->buffer) {
		block_length = state->buffer_size - state->avail;

		/* Tricky calculation to determine size of last block */
		if (a->bytes_in_last_block <= 0)
			/* Default or Zero: pad to full block */
			target_block_length = a->bytes_per_block;
		else
			/* Round to next multiple of bytes_in_last_block. */
			target_block_length = a->bytes_in_last_block *
			    ( (block_length + a->bytes_in_last_block - 1) /
				a->bytes_in_last_block);
		if (target_block_length > a->bytes_per_block)
			target_block_length = a->bytes_per_block;
		if (block_length < target_block_length) {
			memset(state->next, 0,
			    target_block_length - block_length);
			block_length = target_block_length;
		}
		bytes_written = (a->client_writer)(&a->archive,
		    a->client_data, state->buffer, block_length);
		if (bytes_written <= 0)
			ret = ARCHIVE_FATAL;
		else {
			a->archive.raw_position += bytes_written;
			ret = ARCHIVE_OK;
		}
	}
	if (state->buffer)
		free(state->buffer);
	free(state);
	a->compressor.data = NULL;

	return (ret);
}
