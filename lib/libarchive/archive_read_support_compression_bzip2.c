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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_BZLIB_H
#include <bzlib.h>
#endif

#include "archive.h"
#include "archive_private.h"

#if HAVE_BZLIB_H
struct private_data {
	bz_stream	 stream;
	char		*uncompressed_buffer;
	size_t		 uncompressed_buffer_size;
	char		*read_next;
	int64_t		 total_out;
};

static int	finish(struct archive *);
static ssize_t	read_ahead(struct archive *, const void **, size_t);
static ssize_t	read_consume(struct archive *, size_t);
static int	drive_decompressor(struct archive *a, struct private_data *);
#endif

/* These two functions are defined even if we lack bzlib.  See below. */
static int	bid(const void *, size_t);
static int	init(struct archive *, const void *, size_t);

int
archive_read_support_compression_bzip2(struct archive *a)
{
	return (__archive_read_register_compression(a, bid, init));
}

/*
 * Test whether we can handle this data.
 *
 * This logic returns zero if any part of the signature fails.  It
 * also tries to Do The Right Thing if a very short buffer prevents us
 * from verifying as much as we would like.
 */
static int
bid(const void *buff, size_t len)
{
	const unsigned char *buffer;
	int bits_checked;

	if (len < 1)
		return (0);

	buffer = buff;
	bits_checked = 0;
	if (buffer[0] != 'B')	/* Verify first ID byte. */
		return (0);
	bits_checked += 8;
	if (len < 2)
		return (bits_checked);

	if (buffer[1] != 'Z')	/* Verify second ID byte. */
		return (0);
	bits_checked += 8;
	if (len < 3)
		return (bits_checked);

	if (buffer[2] != 'h')	/* Verify third ID byte. */
		return (0);
	bits_checked += 8;
	if (len < 4)
		return (bits_checked);

	if (buffer[3] < '1' || buffer[3] > '9')
		return (0);
	bits_checked += 5;

	/*
	 * Research Question: Can we do any more to verify that this
	 * really is BZip2 format??  For 99.9% of the time, the above
	 * test is sufficient, but it would be nice to do a more
	 * thorough check.  It's especially troubling that the BZip2
	 * signature begins with all ASCII characters; a tar archive
	 * whose first filename begins with 'BZh3' would potentially
	 * fool this logic.  (It may also be possible to gaurd against
	 * such anomalies in archive_read_support_compression_none.)
	 */

	return (bits_checked);
}

#ifndef HAVE_BZLIB_H

/*
 * If we don't have bzlib on this system, we can't actually do the
 * decompression.  We can, however, still detect bzip2-compressed
 * archives and emit a useful message.
 */
static int
init(struct archive *a, const void *buff, size_t n)
{
	(void)a;	/* UNUSED */
	(void)buff;	/* UNUSED */
	(void)n;	/* UNUSED */

	archive_set_error(a, -1,
	    "This version of libarchive was compiled without bzip2 support");
	return (ARCHIVE_FATAL);
}


#else

/*
 * Setup the callbacks.
 */
static int
init(struct archive *a, const void *buff, size_t n)
{
	struct private_data *state;
	int ret;

	a->compression_code = ARCHIVE_COMPRESSION_BZIP2;
	a->compression_name = "bzip2";

	state = malloc(sizeof(*state));
	if (state == NULL) {
		archive_set_error(a, ENOMEM,
		    "Can't allocate data for %s decompression",
		    a->compression_name);
		return (ARCHIVE_FATAL);
	}
	memset(state, 0, sizeof(*state));

	state->uncompressed_buffer_size = 64 * 1024;
	state->uncompressed_buffer = malloc(state->uncompressed_buffer_size);
	state->stream.next_out = state->uncompressed_buffer;
	state->read_next = state->uncompressed_buffer;
	state->stream.avail_out = state->uncompressed_buffer_size;

	if (state->uncompressed_buffer == NULL) {
		archive_set_error(a, ENOMEM,
		    "Can't allocate %s decompression buffers",
		    a->compression_name);
		free(state);
		return (ARCHIVE_FATAL);
	}

	/*
	 * A bug in bzlib.h: stream.next_in should be marked 'const'
	 * but isn't (the library never alters data through the
	 * next_in pointer, only reads it).  The result: this ugly
	 * cast to remove 'const'.
	 */
	state->stream.next_in = (void *)(uintptr_t)(const void *)buff;
	state->stream.avail_in = n;

	a->compression_read_ahead = read_ahead;
	a->compression_read_consume = read_consume;
	a->compression_finish = finish;

	/* Initialize compression library. */
	ret = BZ2_bzDecompressInit(&(state->stream),
	    0 /* library verbosity */,
	    0 /* don't use slow low-mem algorithm */);

	/* If init fails, try using low-memory algorithm instead. */
	if (ret == BZ_MEM_ERROR) {
		ret = BZ2_bzDecompressInit(&(state->stream),
		    0 /* library verbosity */,
		    1 /* do use slow low-mem algorithm */);
	}

	if (ret == BZ_OK) {
		a->compression_data = state;
		return (ARCHIVE_OK);
	}

	/* Library setup failed: Clean up. */
	archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    "Internal error initializing %s library", a->compression_name);
	free(state->uncompressed_buffer);
	free(state);

	/* Override the error message if we know what really went wrong. */
	switch (ret) {
	case BZ_PARAM_ERROR:
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "invalid setup parameter");
		break;
	case BZ_MEM_ERROR:
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "out of memory");
		break;
	case BZ_CONFIG_ERROR:
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "mis-compiled library");
		break;
	}

	return (ARCHIVE_FATAL);
}

/*
 * Return a block of data from the decompression buffer.  Decompress more
 * as necessary.
 */
static ssize_t
read_ahead(struct archive *a, const void **p, size_t min)
{
	struct private_data *state;
	int read_avail, was_avail, ret;

	state = a->compression_data;
	was_avail = -1;
	if (!a->client_reader) {
		archive_set_error(a, ARCHIVE_ERRNO_PROGRAMMER,
		    "No read callback is registered?  "
		    "This is probably an internal programming error.");
		return (ARCHIVE_FATAL);
	}

	read_avail = state->stream.next_out - state->read_next;

	if (read_avail + state->stream.avail_out < min) {
		memmove(state->uncompressed_buffer, state->read_next,
		    read_avail);
		state->read_next = state->uncompressed_buffer;
		state->stream.next_out = state->read_next + read_avail;
		state->stream.avail_out
		    = state->uncompressed_buffer_size - read_avail;
	}

	while (was_avail < read_avail &&	/* Made some progress. */
	    read_avail < (int)min &&		/* Haven't satisfied min. */
	    read_avail < (int)state->uncompressed_buffer_size) { /* !full */
		if ((ret = drive_decompressor(a, state)) != ARCHIVE_OK)
			return (ret);
		was_avail = read_avail;
		read_avail = state->stream.next_out - state->read_next;
	}

	*p = state->read_next;
	return (read_avail);
}

/*
 * Mark a previously-returned block of data as read.
 */
static ssize_t
read_consume(struct archive *a, size_t n)
{
	struct private_data *state;

	state = a->compression_data;
	a->file_position += n;
	state->read_next += n;
	if (state->read_next > state->stream.next_out)
		__archive_errx(1, "Request to consume too many "
		    "bytes from bzip2 decompressor");
	return (n);
}

/*
 * Clean up the decompressor.
 */
static int
finish(struct archive *a)
{
	struct private_data *state;
	int ret;

	state = a->compression_data;
	ret = ARCHIVE_OK;
	switch (BZ2_bzDecompressEnd(&(state->stream))) {
	case BZ_OK:
		break;
	default:
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "Failed to clean up %s compressor", a->compression_name);
		ret = ARCHIVE_FATAL;
	}

	free(state->uncompressed_buffer);
	free(state);

	a->compression_data = NULL;
	if (a->client_closer != NULL)
		(a->client_closer)(a, a->client_data);

	return (ret);
}

/*
 * Utility function to pull data through decompressor, reading input
 * blocks as necessary.
 */
static int
drive_decompressor(struct archive *a, struct private_data *state)
{
	ssize_t ret;
	int decompressed, total_decompressed;
	char *output;

	total_decompressed = 0;
	for (;;) {
		if (state->stream.avail_in == 0) {
			ret = (a->client_reader)(a, a->client_data,
			    (const void **)&state->stream.next_in);
			if (ret < 0) {
				/*
				 * TODO: Find a better way to handle
				 * this read failure.
				 */
				goto fatal;
			}
			if (ret == 0  &&  total_decompressed == 0) {
				archive_set_error(a, EIO,
				    "Premature end of %s compressed data",
				    a->compression_name);
				return (ARCHIVE_FATAL);
			}
			a->raw_position += ret;
			state->stream.avail_in = ret;
		}

		{
			output = state->stream.next_out;

			/* Decompress some data. */
			ret = BZ2_bzDecompress(&(state->stream));
			decompressed = state->stream.next_out - output;

			/* Accumulate the total bytes of output. */
			state->total_out += decompressed;
			total_decompressed += decompressed;

			switch (ret) {
			case BZ_OK: /* Decompressor made some progress. */
				if (decompressed > 0)
					return (ARCHIVE_OK);
				break;
			case BZ_STREAM_END:	/* Found end of stream. */
				return (ARCHIVE_OK);
			default:
				/* Any other return value is an error. */
				goto fatal;
			}
		}
	}
	return (ARCHIVE_OK);

	/* Return a fatal error. */
fatal:
	archive_set_error(a, ARCHIVE_ERRNO_MISC, "%s decompression failed",
	    a->compression_name);
	return (ARCHIVE_FATAL);
}

#endif /* HAVE_BZLIB_H */
