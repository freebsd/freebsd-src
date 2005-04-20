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
__FBSDID("$FreeBSD$");

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"
#include "archive_private.h"

static int	archive_compressor_none_finish(struct archive *a);
static int	archive_compressor_none_init(struct archive *);
static int	archive_compressor_none_write(struct archive *, const void *,
		    size_t);

struct archive_none {
	char	*buffer;
	ssize_t	 buffer_size;
	char	*next;		/* Current insert location */
	ssize_t	 avail;		/* Free space left in buffer */
};

int
archive_write_set_compression_none(struct archive *a)
{
	archive_check_magic(a, ARCHIVE_WRITE_MAGIC, ARCHIVE_STATE_NEW);
	a->compression_init = &archive_compressor_none_init;
	a->compression_code = ARCHIVE_COMPRESSION_NONE;
	a->compression_name = "none";
	return (0);
}

/*
 * Setup callback.
 */
static int
archive_compressor_none_init(struct archive *a)
{
	int ret;
	struct archive_none *state;

	a->compression_code = ARCHIVE_COMPRESSION_NONE;
	a->compression_name = "none";

	if (a->client_opener != NULL) {
		ret = (a->client_opener)(a, a->client_data);
		if (ret != 0)
			return (ret);
	}

	state = (struct archive_none *)malloc(sizeof(*state));
	if (state == NULL) {
		archive_set_error(a, ENOMEM,
		    "Can't allocate data for output buffering");
		return (ARCHIVE_FATAL);
	}
	memset(state, 0, sizeof(*state));

	state->buffer_size = a->bytes_per_block;
	state->buffer = malloc(state->buffer_size);

	if (state->buffer == NULL) {
		archive_set_error(a, ENOMEM,
		    "Can't allocate output buffer");
		free(state);
		return (ARCHIVE_FATAL);
	}

	state->next = state->buffer;
	state->avail = state->buffer_size;

	a->compression_data = state;
	a->compression_write = archive_compressor_none_write;
	a->compression_finish = archive_compressor_none_finish;
	return (ARCHIVE_OK);
}

/*
 * Write data to the stream.
 */
static int
archive_compressor_none_write(struct archive *a, const void *vbuff,
    size_t length)
{
	const char *buff;
	ssize_t remaining, to_copy;
	ssize_t bytes_written;
	struct archive_none *state;

	state = a->compression_data;
	buff = vbuff;
	if (a->client_writer == NULL) {
		archive_set_error(a, ARCHIVE_ERRNO_PROGRAMMER,
		    "No write callback is registered?  "
		    "This is probably an internal programming error.");
		return (ARCHIVE_FATAL);
	}

	remaining = length;
	while (remaining > 0) {
		/*
		 * If we have a full output block, write it and reset the
		 * output buffer.
		 */
		if (state->avail == 0) {
			bytes_written = (a->client_writer)(a, a->client_data,
			    state->buffer, state->buffer_size);
			if (bytes_written <= 0)
				return (ARCHIVE_FATAL);
			/* XXX TODO: if bytes_written < state->buffer_size */
			a->raw_position += bytes_written;
			state->next = state->buffer;
			state->avail = state->buffer_size;
		}

		/* Now we have space in the buffer; copy new data into it. */
		to_copy = (remaining > state->avail) ?
		    state->avail : remaining;
		memcpy(state->next, buff, to_copy);
		state->next += to_copy;
		state->avail -= to_copy;
		buff += to_copy;
		remaining -= to_copy;
	}
	a->file_position += length;
	return (ARCHIVE_OK);
}


/*
 * Finish the compression.
 */
static int
archive_compressor_none_finish(struct archive *a)
{
	ssize_t block_length;
	ssize_t target_block_length;
	ssize_t bytes_written;
	int ret;
	int ret2;
	struct archive_none *state;

	state = a->compression_data;
	ret = ret2 = ARCHIVE_OK;
	if (a->client_writer == NULL) {
		archive_set_error(a, ARCHIVE_ERRNO_PROGRAMMER,
		    "No write callback is registered?  "
		    "This is probably an internal programming error.");
		return (ARCHIVE_FATAL);
	}

	/* If there's pending data, pad and write the last block */
	if (state->next != state->buffer) {
		block_length = state->buffer_size - state->avail;

		/* Tricky calculation to determine size of last block */
		target_block_length = block_length;
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
		bytes_written = (a->client_writer)(a, a->client_data,
		    state->buffer, block_length);
		if (bytes_written <= 0)
			ret = ARCHIVE_FATAL;
		else {
			a->raw_position += bytes_written;
			ret = ARCHIVE_OK;
		}
	}

	/* Close the output */
	if (a->client_closer != NULL)
		ret2 = (a->client_closer)(a, a->client_data);

	free(state->buffer);
	free(state);
	a->compression_data = NULL;

	return (ret != ARCHIVE_OK ? ret : ret2);
}
