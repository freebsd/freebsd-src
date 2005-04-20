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
#include <unistd.h>

#include "archive.h"
#include "archive_private.h"

struct archive_decompress_none {
	char		*buffer;
	size_t		 buffer_size;
	char		*next;		/* Current read location. */
	size_t		 avail;		/* Bytes in my buffer. */
	const char	*client_buff;	/* Client buffer information. */
	size_t		 client_total;
	const char	*client_next;
	size_t		 client_avail;
	char		 end_of_file;
	char		 fatal;
};

/*
 * Size of internal buffer used for combining short reads.  This is
 * also an upper limit on the size of a read request.  Recall,
 * however, that we can (and will!) return blocks of data larger than
 * this.  The read semantics are: you ask for a minimum, I give you a
 * pointer to my best-effort match and tell you how much data is
 * there.  It could be less than you asked for, it could be much more.
 * For example, a client might use mmap() to "read" the entire file as
 * a single block.  In that case, I will return that entire block to
 * my clients.
 */
#define	BUFFER_SIZE	65536

static int	archive_decompressor_none_bid(const void *, size_t);
static int	archive_decompressor_none_finish(struct archive *);
static int	archive_decompressor_none_init(struct archive *,
		    const void *, size_t);
static ssize_t	archive_decompressor_none_read_ahead(struct archive *,
		    const void **, size_t);
static ssize_t	archive_decompressor_none_read_consume(struct archive *,
		    size_t);

int
archive_read_support_compression_none(struct archive *a)
{
	return (__archive_read_register_compression(a,
		    archive_decompressor_none_bid,
		    archive_decompressor_none_init));
}

/*
 * Try to detect an "uncompressed" archive.
 */
static int
archive_decompressor_none_bid(const void *buff, size_t len)
{
	(void)buff;
	(void)len;

	return (1); /* Default: We'll take it if noone else does. */
}

static int
archive_decompressor_none_init(struct archive *a, const void *buff, size_t n)
{
	struct archive_decompress_none	*state;

	a->compression_code = ARCHIVE_COMPRESSION_NONE;
	a->compression_name = "none";

	state = (struct archive_decompress_none *)malloc(sizeof(*state));
	if (!state) {
		archive_set_error(a, ENOMEM, "Can't allocate input data");
		return (ARCHIVE_FATAL);
	}
	memset(state, 0, sizeof(*state));

	state->buffer_size = BUFFER_SIZE;
	state->buffer = malloc(state->buffer_size);
	state->next = state->buffer;
	if (state->buffer == NULL) {
		free(state);
		archive_set_error(a, ENOMEM, "Can't allocate input buffer");
		return (ARCHIVE_FATAL);
	}

	/* Save reference to first block of data. */
	state->client_buff = buff;
	state->client_total = n;
	state->client_next = state->client_buff;
	state->client_avail = state->client_total;

	a->compression_data = state;
	a->compression_read_ahead = archive_decompressor_none_read_ahead;
	a->compression_read_consume = archive_decompressor_none_read_consume;
	a->compression_finish = archive_decompressor_none_finish;

	return (ARCHIVE_OK);
}

/*
 * We just pass through pointers to the client buffer if we can.
 * If the client buffer is short, then we copy stuff to our internal
 * buffer to combine reads.
 */
static ssize_t
archive_decompressor_none_read_ahead(struct archive *a, const void **buff,
    size_t min)
{
	struct archive_decompress_none *state;
	ssize_t bytes_read;

	state = a->compression_data;
	if (state->fatal)
		return (-1);

	/*
	 * Don't make special efforts to handle requests larger than
	 * the copy buffer.
	 */
	if (min > state->buffer_size)
		min = state->buffer_size;

	/*
	 * Try to satisfy the request directly from the client
	 * buffer.  We can do this if all of the data in the copy
	 * buffer was copied from the current client buffer.  This
	 * also covers the case where the copy buffer is empty and
	 * the client buffer has all the data we need.
	 */
	if (state->client_total >= state->client_avail + state->avail
	    && state->client_avail + state->avail >= min) {
		state->client_avail += state->avail;
		state->client_next -= state->avail;
		state->avail = 0;
		state->next = state->buffer;
		*buff = state->client_next;
		return (state->client_avail);
	}

	/*
	 * If we can't use client buffer, we'll have to use copy buffer.
	 */

	/* Move data forward in copy buffer if necessary. */
	if (state->next > state->buffer &&
	    state->next + min > state->buffer + state->buffer_size) {
		if (state->avail > 0)
			memmove(state->buffer, state->next, state->avail);
		state->next = state->buffer;
	}

	/* Collect data in copy buffer to fulfill request. */
	while (state->avail < min) {
		/* Copy data from client buffer to our copy buffer. */
		if (state->client_avail > 0) {
			/* First estimate: copy to fill rest of buffer. */
			size_t tocopy = (state->buffer + state->buffer_size)
			    - (state->next + state->avail);
			/* Don't copy more than is available. */
			if (tocopy > state->client_avail)
				tocopy = state->client_avail;
			memcpy(state->next + state->avail, state->client_next,
			    tocopy);
			state->client_next += tocopy;
			state->client_avail -= tocopy;
			state->avail += tocopy;
		} else {
			/* There is no more client data: fetch more. */
			/*
			 * It seems to me that const void ** and const
			 * char ** should be compatible, but they
			 * aren't, hence the cast.
			 */
			bytes_read = (a->client_reader)(a, a->client_data,
			    (const void **)&state->client_buff);
			if (bytes_read < 0) {		/* Read error. */
				state->client_total = state->client_avail = 0;
				state->client_next = state->client_buff = NULL;
				state->fatal = 1;
				return (-1);
			}
			if (bytes_read == 0) {		/* End-of-file. */
				state->client_total = state->client_avail = 0;
				state->client_next = state->client_buff = NULL;
				state->end_of_file = 1;
				break;
			}
			a->raw_position += bytes_read;
			state->client_total = bytes_read;
			state->client_avail = state->client_total;
			state->client_next = state->client_buff;
		}
	}

	*buff = state->next;
	return (state->avail);
}

/*
 * Mark the appropriate data as used.  Note that the request here will
 * often be much smaller than the size of the previous read_ahead
 * request.
 */
static ssize_t
archive_decompressor_none_read_consume(struct archive *a, size_t request)
{
	struct archive_decompress_none *state;

	state = a->compression_data;
	if (state->avail > 0) {
		/* Read came from copy buffer. */
		state->next += request;
		state->avail -= request;
	} else {
		/* Read came from client buffer. */
		state->client_next += request;
		state->client_avail -= request;
	}
	a->file_position += request;
	return (request);
}

static int
archive_decompressor_none_finish(struct archive *a)
{
	struct archive_decompress_none	*state;

	state = a->compression_data;
	free(state->buffer);
	free(state);
	a->compression_data = NULL;
	if (a->client_closer != NULL)
		return ((a->client_closer)(a, a->client_data));
	return (ARCHIVE_OK);
}
