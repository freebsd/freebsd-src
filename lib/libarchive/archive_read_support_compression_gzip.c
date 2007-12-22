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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_read_private.h"

#ifdef HAVE_ZLIB_H
struct private_data {
	z_stream	 stream;
	unsigned char	*uncompressed_buffer;
	size_t		 uncompressed_buffer_size;
	unsigned char	*read_next;
	int64_t		 total_out;
	unsigned long	 crc;
	char		 header_done;
	char		 eof; /* True = found end of compressed data. */
};

static int	finish(struct archive_read *);
static ssize_t	read_ahead(struct archive_read *, const void **, size_t);
static ssize_t	read_consume(struct archive_read *, size_t);
static int	drive_decompressor(struct archive_read *a, struct private_data *);
#endif

/* These two functions are defined even if we lack the library.  See below. */
static int	bid(const void *, size_t);
static int	init(struct archive_read *, const void *, size_t);

int
archive_read_support_compression_gzip(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	if (__archive_read_register_compression(a, bid, init) != NULL)
		return (ARCHIVE_OK);
	return (ARCHIVE_FATAL);
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

	buffer = (const unsigned char *)buff;
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
 * If we don't have the library on this system, we can't actually do the
 * decompression.  We can, however, still detect compressed archives
 * and emit a useful message.
 */
static int
init(struct archive_read *a, const void *buff, size_t n)
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
init(struct archive_read *a, const void *buff, size_t n)
{
	struct private_data *state;
	int ret;

	a->archive.compression_code = ARCHIVE_COMPRESSION_GZIP;
	a->archive.compression_name = "gzip";

	state = (struct private_data *)malloc(sizeof(*state));
	if (state == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate data for %s decompression",
		    a->archive.compression_name);
		return (ARCHIVE_FATAL);
	}
	memset(state, 0, sizeof(*state));

	state->crc = crc32(0L, NULL, 0);
	state->header_done = 0; /* We've not yet begun to parse header... */

	state->uncompressed_buffer_size = 64 * 1024;
	state->uncompressed_buffer = (unsigned char *)malloc(state->uncompressed_buffer_size);
	state->stream.next_out = state->uncompressed_buffer;
	state->read_next = state->uncompressed_buffer;
	state->stream.avail_out = state->uncompressed_buffer_size;

	if (state->uncompressed_buffer == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate %s decompression buffers",
		    a->archive.compression_name);
		free(state);
		return (ARCHIVE_FATAL);
	}

	/*
	 * A bug in zlib.h: stream.next_in should be marked 'const'
	 * but isn't (the library never alters data through the
	 * next_in pointer, only reads it).  The result: this ugly
	 * cast to remove 'const'.
	 */
	state->stream.next_in = (Bytef *)(uintptr_t)(const void *)buff;
	state->stream.avail_in = n;

	a->decompressor->read_ahead = read_ahead;
	a->decompressor->consume = read_consume;
	a->decompressor->skip = NULL; /* not supported */
	a->decompressor->finish = finish;

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
		a->decompressor->data = state;
		return (ARCHIVE_OK);
	}

	/* Library setup failed: Clean up. */
	archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
	    "Internal error initializing %s library",
	    a->archive.compression_name);
	free(state->uncompressed_buffer);
	free(state);

	/* Override the error message if we know what really went wrong. */
	switch (ret) {
	case Z_STREAM_ERROR:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "invalid setup parameter");
		break;
	case Z_MEM_ERROR:
		archive_set_error(&a->archive, ENOMEM,
		    "Internal error initializing compression library: "
		    "out of memory");
		break;
	case Z_VERSION_ERROR:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
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
read_ahead(struct archive_read *a, const void **p, size_t min)
{
	struct private_data *state;
	size_t read_avail, was_avail;
	int ret;

	state = (struct private_data *)a->decompressor->data;
	if (!a->client_reader) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
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

	while (read_avail < min &&		/* Haven't satisfied min. */
	    read_avail < state->uncompressed_buffer_size) { /* !full */
		was_avail = read_avail;
		if ((ret = drive_decompressor(a, state)) < ARCHIVE_OK)
			return (ret);
		if (ret == ARCHIVE_EOF)
			break; /* Break on EOF even if we haven't met min. */
		read_avail = state->stream.next_out - state->read_next;
		if (was_avail == read_avail) /* No progress? */
			break;
	}

	*p = state->read_next;
	return (read_avail);
}

/*
 * Mark a previously-returned block of data as read.
 */
static ssize_t
read_consume(struct archive_read *a, size_t n)
{
	struct private_data *state;

	state = (struct private_data *)a->decompressor->data;
	a->archive.file_position += n;
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
finish(struct archive_read *a)
{
	struct private_data *state;
	int ret;

	state = (struct private_data *)a->decompressor->data;
	ret = ARCHIVE_OK;
	switch (inflateEnd(&(state->stream))) {
	case Z_OK:
		break;
	default:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Failed to clean up %s compressor",
		    a->archive.compression_name);
		ret = ARCHIVE_FATAL;
	}

	free(state->uncompressed_buffer);
	free(state);

	a->decompressor->data = NULL;
	return (ret);
}

/*
 * Utility function to pull data through decompressor, reading input
 * blocks as necessary.
 */
static int
drive_decompressor(struct archive_read *a, struct private_data *state)
{
	ssize_t ret;
	size_t decompressed, total_decompressed;
	int count, flags, header_state;
	unsigned char *output;
	unsigned char b;
	const void *read_buf;

	if (state->eof)
		return (ARCHIVE_EOF);
	flags = 0;
	count = 0;
	header_state = 0;
	total_decompressed = 0;
	for (;;) {
		if (state->stream.avail_in == 0) {
			read_buf = state->stream.next_in;
			ret = (a->client_reader)(&a->archive, a->client_data,
			    &read_buf);
			state->stream.next_in = (unsigned char *)(uintptr_t)read_buf;
			if (ret < 0) {
				/*
				 * TODO: Find a better way to handle
				 * this read failure.
				 */
				goto fatal;
			}
			if (ret == 0  &&  total_decompressed == 0) {
				archive_set_error(&a->archive, EIO,
				    "Premature end of %s compressed data",
				    a->archive.compression_name);
				return (ARCHIVE_FATAL);
			}
			a->archive.raw_position += ret;
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
				state->eof = 1;
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
	archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
	    "%s decompression failed", a->archive.compression_name);
	return (ARCHIVE_FATAL);
}

#endif /* HAVE_ZLIB_H */
