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
__FBSDID("$FreeBSD$");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_read_private.h"

struct archive_decompress_none {
	char		*buffer;
	size_t		 buffer_size;
	char		*next;		/* Current read location. */
	size_t		 avail;		/* Bytes in my buffer. */
	const void	*client_buff;	/* Client buffer information. */
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

#define minimum(a, b) (a < b ? a : b)

static int	archive_decompressor_none_bid(const void *, size_t);
static int	archive_decompressor_none_finish(struct archive_read *);
static int	archive_decompressor_none_init(struct archive_read *,
		    const void *, size_t);
static ssize_t	archive_decompressor_none_read_ahead(struct archive_read *,
		    const void **, size_t);
static ssize_t	archive_decompressor_none_read_consume(struct archive_read *,
		    size_t);
static off_t	archive_decompressor_none_skip(struct archive_read *, off_t);

int
archive_read_support_compression_none(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	if (__archive_read_register_compression(a,
		archive_decompressor_none_bid,
		archive_decompressor_none_init) != NULL)
		return (ARCHIVE_OK);
	return (ARCHIVE_FATAL);
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
archive_decompressor_none_init(struct archive_read *a, const void *buff, size_t n)
{
	struct archive_decompress_none	*state;

	a->archive.compression_code = ARCHIVE_COMPRESSION_NONE;
	a->archive.compression_name = "none";

	state = (struct archive_decompress_none *)malloc(sizeof(*state));
	if (!state) {
		archive_set_error(&a->archive, ENOMEM, "Can't allocate input data");
		return (ARCHIVE_FATAL);
	}
	memset(state, 0, sizeof(*state));

	state->buffer_size = BUFFER_SIZE;
	state->buffer = (char *)malloc(state->buffer_size);
	state->next = state->buffer;
	if (state->buffer == NULL) {
		free(state);
		archive_set_error(&a->archive, ENOMEM, "Can't allocate input buffer");
		return (ARCHIVE_FATAL);
	}

	/* Save reference to first block of data. */
	state->client_buff = buff;
	state->client_total = n;
	state->client_next = state->client_buff;
	state->client_avail = state->client_total;

	a->decompressor->data = state;
	a->decompressor->read_ahead = archive_decompressor_none_read_ahead;
	a->decompressor->consume = archive_decompressor_none_read_consume;
	a->decompressor->skip = archive_decompressor_none_skip;
	a->decompressor->finish = archive_decompressor_none_finish;

	return (ARCHIVE_OK);
}

/*
 * We just pass through pointers to the client buffer if we can.
 * If the client buffer is short, then we copy stuff to our internal
 * buffer to combine reads.
 */
static ssize_t
archive_decompressor_none_read_ahead(struct archive_read *a, const void **buff,
    size_t min)
{
	struct archive_decompress_none *state;
	ssize_t bytes_read;

	state = (struct archive_decompress_none *)a->decompressor->data;
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
			bytes_read = (a->client_reader)(&a->archive,
			    a->client_data, &state->client_buff);
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
			a->archive.raw_position += bytes_read;
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
archive_decompressor_none_read_consume(struct archive_read *a, size_t request)
{
	struct archive_decompress_none *state;

	state = (struct archive_decompress_none *)a->decompressor->data;
	if (state->avail > 0) {
		/* Read came from copy buffer. */
		state->next += request;
		state->avail -= request;
	} else {
		/* Read came from client buffer. */
		state->client_next += request;
		state->client_avail -= request;
	}
	a->archive.file_position += request;
	return (request);
}

/*
 * Skip forward by exactly the requested bytes or else return
 * ARCHIVE_FATAL.  Note that this differs from the contract for
 * read_ahead, which does not guarantee a minimum count.
 */
static off_t
archive_decompressor_none_skip(struct archive_read *a, off_t request)
{
	struct archive_decompress_none *state;
	off_t bytes_skipped, total_bytes_skipped = 0;
	size_t min;

	state = (struct archive_decompress_none *)a->decompressor->data;
	if (state->fatal)
		return (-1);
	/*
	 * If there is data in the buffers already, use that first.
	 */
	if (state->avail > 0) {
		min = minimum(request, (off_t)state->avail);
		bytes_skipped = archive_decompressor_none_read_consume(a, min);
		request -= bytes_skipped;
		total_bytes_skipped += bytes_skipped;
	}
	if (state->client_avail > 0) {
		min = minimum(request, (off_t)state->client_avail);
		bytes_skipped = archive_decompressor_none_read_consume(a, min);
		request -= bytes_skipped;
		total_bytes_skipped += bytes_skipped;
	}
	if (request == 0)
		return (total_bytes_skipped);
	/*
	 * If a client_skipper was provided, try that first.
	 */
#if ARCHIVE_API_VERSION < 2
	if ((a->client_skipper != NULL) && (request < SSIZE_MAX)) {
#else
	if (a->client_skipper != NULL) {
#endif
		bytes_skipped = (a->client_skipper)(&a->archive,
		    a->client_data, request);
		if (bytes_skipped < 0) {	/* error */
			state->client_total = state->client_avail = 0;
			state->client_next = state->client_buff = NULL;
			state->fatal = 1;
			return (bytes_skipped);
		}
		total_bytes_skipped += bytes_skipped;
		a->archive.file_position += bytes_skipped;
		request -= bytes_skipped;
		state->client_next = state->client_buff;
		a->archive.raw_position += bytes_skipped;
		state->client_avail = state->client_total = 0;
	}
	/*
	 * Note that client_skipper will usually not satisfy the
	 * full request (due to low-level blocking concerns),
	 * so even if client_skipper is provided, we may still
	 * have to use ordinary reads to finish out the request.
	 */
	while (request > 0) {
		const void* dummy_buffer;
		ssize_t bytes_read;
		bytes_read = archive_decompressor_none_read_ahead(a,
		    &dummy_buffer, request);
		if (bytes_read < 0)
			return (bytes_read);
		if (bytes_read == 0) {
			/* We hit EOF before we satisfied the skip request. */
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Truncated input file (need to skip %jd bytes)",
			    (intmax_t)request);
			return (ARCHIVE_FATAL);
		}
		min = (size_t)(minimum(bytes_read, request));
		bytes_read = archive_decompressor_none_read_consume(a, min);
		total_bytes_skipped += bytes_read;
		request -= bytes_read;
	}
	return (total_bytes_skipped);
}

static int
archive_decompressor_none_finish(struct archive_read *a)
{
	struct archive_decompress_none	*state;

	state = (struct archive_decompress_none *)a->decompressor->data;
	free(state->buffer);
	free(state);
	a->decompressor->data = NULL;
	return (ARCHIVE_OK);
}
