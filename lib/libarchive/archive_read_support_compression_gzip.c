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
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#include "archive.h"
#include "archive_private.h"

#ifdef HAVE_ZLIB_H
struct private_data {
	z_stream	 stream;
	unsigned char	*uncompressed_buffer;
	size_t		 uncompressed_buffer_size;
	unsigned char	*read_next;
	int64_t		 total_out;
	unsigned long	 crc;
	char		 header_done;
};

static int	finish(struct archive *);
static ssize_t	read_ahead(struct archive *, const void **, size_t);
static ssize_t	read_consume(struct archive *, size_t);
static int	drive_decompressor(struct archive *a, struct private_data *);
#endif

/* These two functions are defined even if we lack zlib.  See below. */
static int	bid(const void *, size_t);
static int	init(struct archive *, const void *, size_t);

int
archive_read_support_compression_gzip(struct archive *a)
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
	if (buffer[0] != 037)	/* Verify first ID byte. */
		return (0);
	bits_checked += 8;
	if (len < 2)
		return (bits_checked);

	if (buffer[1] != 0213)	/* Verify second ID byte. */
		return (0);
	bits_checked += 8;
	if (len < 3)
		return (bits_checked);

	if (buffer[2] != 8)	/* Compression must be 'deflate'. */
		return (0);
	bits_checked += 8;
	if (len < 4)
		return (bits_checked);

	if ((buffer[3] & 0xE0)!= 0)	/* No reserved flags set. */
		return (0);
	bits_checked += 3;
	if (len < 5)
		return (bits_checked);

	/*
	 * TODO: Verify more; in particular, gzip has an optional
	 * header CRC, which would give us 16 more verified bits.  We
	 * may also be able to verify certain constraints on other
	 * fields.
	 */

	return (bits_checked);
}


#ifndef HAVE_ZLIB_H

/*
 * If we don't have zlib on this system, we can't actually do the
 * decompression.  We can, however, still detect gzip-compressed
 * archives and emit a useful message.
 */
static int
init(struct archive *a, const void *buff, size_t n)
{
	(void)a;	/* UNUSED */
	(void)buff;	/* UNUSED */
	(void)n;	/* UNUSED */

	archive_set_error(a, -1,
	    "This version of libarchive was compiled without gzip support");
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

	a->compression_code = ARCHIVE_COMPRESSION_GZIP;
	a->compression_name = "gzip";

	state = malloc(sizeof(*state));
	if (state == NULL) {
		archive_set_error(a, ENOMEM,
		    "Can't allocate data for %s decompression",
		    a->compression_name);
		return (ARCHIVE_FATAL);
	}
	memset(state, 0, sizeof(*state));

	state->crc = crc32(0L, NULL, 0);
	state->header_done = 0; /* We've not yet begun to parse header... */

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
	 * A bug in zlib.h: stream.next_in should be marked 'const'
	 * but isn't (the library never alters data through the
	 * next_in pointer, only reads it).  The result: this ugly
	 * cast to remove 'const'.
	 */
	state->stream.next_in = (void *)(uintptr_t)(const void *)buff;
	state->stream.avail_in = n;

	a->compression_read_ahead = read_ahead;
	a->compression_read_consume = read_consume;
	a->compression_finish = finish;

	/*
	 * TODO: Do I need to parse the gzip header before calling
	 * inflateInit2()?  In particular, one of the header bytes
	 * marks "best compression" or "fastest", which may be
	 * appropriate for setting the second parameter here.
	 * However, I think the only penalty for not setting it
	 * correctly is wasted memory.  If this is necessary, it
	 * should probably go into drive_decompressor() below.
	 */

	/* Initialize compression library. */
	ret = inflateInit2(&(state->stream),
	    -15 /* Don't check for zlib header */);
	if (ret == Z_OK) {
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
	case Z_STREAM_ERROR:
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "invalid setup parameter");
		break;
	case Z_MEM_ERROR:
		archive_set_error(a, ENOMEM,
		    "Internal error initializing compression library: "
		    "out of memory");
		break;
	case Z_VERSION_ERROR:
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "invalid library version");
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
		    "bytes from gzip decompressor");
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
	switch (inflateEnd(&(state->stream))) {
	case Z_OK:
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
	int count, flags, header_state;
	unsigned char *output;
	unsigned char b;

	flags = 0;
	count = 0;
	header_state = 0;
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

		if (!state->header_done) {
			/*
			 * If still parsing the header, interpret the
			 * next byte.
			 */
			b = *(state->stream.next_in++);
			state->stream.avail_in--;

			/*
			 * Yes, this is somewhat crude, but it works,
			 * GZip format isn't likely to change anytime
			 * in the near future, and header parsing is
			 * certainly not a performance issue, so
			 * there's little point in making this more
			 * elegant.  Of course, if you see an easy way
			 * to make this more elegant, please let me
			 * know.. ;-)
			 */
			switch (header_state) {
			case 0: /* First byte of signature. */
				if (b != 037)
					goto fatal;
				header_state = 1;
				break;
			case 1: /* Second byte of signature. */
				if (b != 0213)
					goto fatal;
				header_state = 2;
				break;
			case 2: /* Compression type must be 8. */
				if (b != 8)
					goto fatal;
				header_state = 3;
				break;
			case 3: /* GZip flags. */
				flags = b;
				header_state = 4;
				break;
			case 4: case 5: case 6: case 7: /* Mod time. */
				header_state++;
				break;
			case 8: /* Deflate flags. */
				header_state = 9;
				break;
			case 9: /* OS. */
				header_state = 10;
				break;
			case 10: /* Optional Extra: First byte of Length. */
				if ((flags & 4)) {
					count = 255 & (int)b;
					header_state = 11;
					break;
				}
				/*
				 * Fall through if there is no
				 * Optional Extra field.
				 */
			case 11: /* Optional Extra: Second byte of Length. */
				if ((flags & 4)) {
					count = (0xff00 & ((int)b << 8)) | count;
					header_state = 12;
					break;
				}
				/*
				 * Fall through if there is no
				 * Optional Extra field.
				 */
			case 12: /* Optional Extra Field: counted length. */
				if ((flags & 4)) {
					--count;
					if (count == 0) header_state = 13;
					else header_state = 12;
					break;
				}
				/*
				 * Fall through if there is no
				 * Optional Extra field.
				 */
			case 13: /* Optional Original Filename. */
				if ((flags & 8)) {
					if (b == 0) header_state = 14;
					else header_state = 13;
					break;
				}
				/*
				 * Fall through if no Optional
				 * Original Filename.
				 */
			case 14: /* Optional Comment. */
				if ((flags & 16)) {
					if (b == 0) header_state = 15;
					else header_state = 14;
					break;
				}
				/* Fall through if no Optional Comment. */
			case 15: /* Optional Header CRC: First byte. */
				if ((flags & 2)) {
					header_state = 16;
					break;
				}
				/* Fall through if no Optional Header CRC. */
			case 16: /* Optional Header CRC: Second byte. */
				if ((flags & 2)) {
					header_state = 17;
					break;
				}
				/* Fall through if no Optional Header CRC. */
			case 17: /* First byte of compressed data. */
				state->header_done = 1; /* done with header */
				state->stream.avail_in++;
				state->stream.next_in--;
			}

			/*
			 * TODO: Consider moving the inflateInit2 call
			 * here so it can include the compression type
			 * from the header?
			 */
		} else {
			output = state->stream.next_out;

			/* Decompress some data. */
			ret = inflate(&(state->stream), 0);
			decompressed = state->stream.next_out - output;

			/* Accumulate the CRC of the uncompressed data. */
			state->crc = crc32(state->crc, output, decompressed);

			/* Accumulate the total bytes of output. */
			state->total_out += decompressed;
			total_decompressed += decompressed;

			switch (ret) {
			case Z_OK: /* Decompressor made some progress. */
				if (decompressed > 0)
					return (ARCHIVE_OK);
				break;
			case Z_STREAM_END: /* Found end of stream. */
				/*
				 * TODO: Verify gzip trailer
				 * (uncompressed length and CRC).
				 */
				return (ARCHIVE_OK);
			default:
				/* Any other return value is an error. */
				archive_set_error(a, ARCHIVE_ERRNO_MISC,
				    "gzip decompression failed (%s)",
				    state->stream.msg);
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

#endif /* HAVE_ZLIB_H */
