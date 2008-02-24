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

/*
 * This code borrows heavily from "compress" source code, which is
 * protected by the following copyright.  (Clause 3 dropped by request
 * of the Regents.)
 */

/*-
 * Copyright (c) 1985, 1986, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Diomidis Spinellis and James A. Woods, derived from original
 * work by Spencer Thomas and Joseph Orost.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include "archive_platform.h"
__FBSDID("$FreeBSD: src/lib/libarchive/archive_read_support_compression_compress.c,v 1.10 2007/05/29 01:00:19 kientzle Exp $");

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

#include "archive.h"
#include "archive_private.h"
#include "archive_read_private.h"

/*
 * Because LZW decompression is pretty simple, I've just implemented
 * the whole decompressor here (cribbing from "compress" source code,
 * of course), rather than relying on an external library.  I have
 * made an effort to clarify and simplify the algorithm, so the
 * names and structure here don't exactly match those used by compress.
 */

struct private_data {
	/* Input variables. */
	const unsigned char	*next_in;
	size_t			 avail_in;
	int			 bit_buffer;
	int			 bits_avail;
	size_t			 bytes_in_section;

	/* Output variables. */
	size_t			 uncompressed_buffer_size;
	void			*uncompressed_buffer;
	unsigned char		*read_next;   /* Data for client. */
	unsigned char		*next_out;    /* Where to write new data. */
	size_t			 avail_out;   /* Space at end of buffer. */

	/* Decompression status variables. */
	int			 use_reset_code;
	int			 end_of_stream;	/* EOF status. */
	int			 maxcode;	/* Largest code. */
	int			 maxcode_bits;	/* Length of largest code. */
	int			 section_end_code; /* When to increase bits. */
	int			 bits;		/* Current code length. */
	int			 oldcode;	/* Previous code. */
	int			 finbyte;	/* Last byte of prev code. */

	/* Dictionary. */
	int			 free_ent;       /* Next dictionary entry. */
	unsigned char		 suffix[65536];
	uint16_t		 prefix[65536];

	/*
	 * Scratch area for expanding dictionary entries.  Note:
	 * "worst" case here comes from compressing /dev/zero: the
	 * last code in the dictionary will code a sequence of
	 * 65536-256 zero bytes.  Thus, we need stack space to expand
	 * a 65280-byte dictionary entry.  (Of course, 32640:1
	 * compression could also be considered the "best" case. ;-)
	 */
	unsigned char		*stackp;
	unsigned char		 stack[65300];
};

static int	bid(const void *, size_t);
static int	finish(struct archive_read *);
static int	init(struct archive_read *, const void *, size_t);
static ssize_t	read_ahead(struct archive_read *, const void **, size_t);
static ssize_t	read_consume(struct archive_read *, size_t);
static int	getbits(struct archive_read *, struct private_data *, int n);
static int	next_code(struct archive_read *a, struct private_data *state);

int
archive_read_support_compression_compress(struct archive *_a)
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

	if (buffer[1] != 0235)	/* Verify second ID byte. */
		return (0);
	bits_checked += 8;
	if (len < 3)
		return (bits_checked);

	/*
	 * TODO: Verify more.
	 */

	return (bits_checked);
}

/*
 * Setup the callbacks.
 */
static int
init(struct archive_read *a, const void *buff, size_t n)
{
	struct private_data *state;
	int code;

	a->archive.compression_code = ARCHIVE_COMPRESSION_COMPRESS;
	a->archive.compression_name = "compress (.Z)";

	a->decompressor->read_ahead = read_ahead;
	a->decompressor->consume = read_consume;
	a->decompressor->skip = NULL; /* not supported */
	a->decompressor->finish = finish;

	state = (struct private_data *)malloc(sizeof(*state));
	if (state == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate data for %s decompression",
		    a->archive.compression_name);
		return (ARCHIVE_FATAL);
	}
	memset(state, 0, sizeof(*state));
	a->decompressor->data = state;

	state->uncompressed_buffer_size = 64 * 1024;
	state->uncompressed_buffer = malloc(state->uncompressed_buffer_size);

	if (state->uncompressed_buffer == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate %s decompression buffers",
		    a->archive.compression_name);
		goto fatal;
	}

	state->next_in = (const unsigned char *)buff;
	state->avail_in = n;
	state->read_next = state->next_out = (unsigned char *)state->uncompressed_buffer;
	state->avail_out = state->uncompressed_buffer_size;

	code = getbits(a, state, 8);
	if (code != 037) /* This should be impossible. */
		goto fatal;

	code = getbits(a, state, 8);
	if (code != 0235) {
		/* This can happen if the library is receiving 1-byte
		 * blocks and gzip and compress are both enabled.
		 * You can't distinguish gzip and compress only from
		 * the first byte. */
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Compress signature did not match.");
		goto fatal;
	}

	code = getbits(a, state, 8);
	state->maxcode_bits = code & 0x1f;
	state->maxcode = (1 << state->maxcode_bits);
	state->use_reset_code = code & 0x80;

	/* Initialize decompressor. */
	state->free_ent = 256;
	state->stackp = state->stack;
	if (state->use_reset_code)
		state->free_ent++;
	state->bits = 9;
	state->section_end_code = (1<<state->bits) - 1;
	state->oldcode = -1;
	for (code = 255; code >= 0; code--) {
		state->prefix[code] = 0;
		state->suffix[code] = code;
	}
	next_code(a, state);
	return (ARCHIVE_OK);

fatal:
	finish(a);
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
	size_t read_avail;
	int ret;

	state = (struct private_data *)a->decompressor->data;
	if (!a->client_reader) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
		    "No read callback is registered?  "
		    "This is probably an internal programming error.");
		return (ARCHIVE_FATAL);
	}

	read_avail = state->next_out - state->read_next;

	if (read_avail < min  &&  state->end_of_stream) {
		if (state->end_of_stream == ARCHIVE_EOF)
			return (0);
		else
			return (-1);
	}

	if (read_avail < min) {
		memmove(state->uncompressed_buffer, state->read_next,
		    read_avail);
		state->read_next = (unsigned char *)state->uncompressed_buffer;
		state->next_out = state->read_next + read_avail;
		state->avail_out
		    = state->uncompressed_buffer_size - read_avail;

		while (read_avail < state->uncompressed_buffer_size
			&& !state->end_of_stream) {
			if (state->stackp > state->stack) {
				*state->next_out++ = *--state->stackp;
				state->avail_out--;
				read_avail++;
			} else {
				ret = next_code(a, state);
				if (ret == ARCHIVE_EOF)
					state->end_of_stream = ret;
				else if (ret != ARCHIVE_OK)
					return (ret);
			}
		}
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
	if (state->read_next > state->next_out)
		__archive_errx(1, "Request to consume too many "
		    "bytes from compress decompressor");
	return (n);
}

/*
 * Clean up the decompressor.
 */
static int
finish(struct archive_read *a)
{
	struct private_data *state;
	int ret = ARCHIVE_OK;

	state = (struct private_data *)a->decompressor->data;

	if (state != NULL) {
		if (state->uncompressed_buffer != NULL)
			free(state->uncompressed_buffer);
		free(state);
	}

	a->decompressor->data = NULL;
	return (ret);
}

/*
 * Process the next code and fill the stack with the expansion
 * of the code.  Returns ARCHIVE_FATAL if there is a fatal I/O or
 * format error, ARCHIVE_EOF if we hit end of data, ARCHIVE_OK otherwise.
 */
static int
next_code(struct archive_read *a, struct private_data *state)
{
	int code, newcode;

	static int debug_buff[1024];
	static unsigned debug_index;

	code = newcode = getbits(a, state, state->bits);
	if (code < 0)
		return (code);

	debug_buff[debug_index++] = code;
	if (debug_index >= sizeof(debug_buff)/sizeof(debug_buff[0]))
		debug_index = 0;

	/* If it's a reset code, reset the dictionary. */
	if ((code == 256) && state->use_reset_code) {
		/*
		 * The original 'compress' implementation blocked its
		 * I/O in a manner that resulted in junk bytes being
		 * inserted after every reset.  The next section skips
		 * this junk.  (Yes, the number of *bytes* to skip is
		 * a function of the current *bit* length.)
		 */
		int skip_bytes =  state->bits -
		    (state->bytes_in_section % state->bits);
		skip_bytes %= state->bits;
		state->bits_avail = 0; /* Discard rest of this byte. */
		while (skip_bytes-- > 0) {
			code = getbits(a, state, 8);
			if (code < 0)
				return (code);
		}
		/* Now, actually do the reset. */
		state->bytes_in_section = 0;
		state->bits = 9;
		state->section_end_code = (1 << state->bits) - 1;
		state->free_ent = 257;
		state->oldcode = -1;
		return (next_code(a, state));
	}

	if (code > state->free_ent) {
		/* An invalid code is a fatal error. */
		archive_set_error(&a->archive, -1, "Invalid compressed data");
		return (ARCHIVE_FATAL);
	}

	/* Special case for KwKwK string. */
	if (code >= state->free_ent) {
		*state->stackp++ = state->finbyte;
		code = state->oldcode;
	}

	/* Generate output characters in reverse order. */
	while (code >= 256) {
		*state->stackp++ = state->suffix[code];
		code = state->prefix[code];
	}
	*state->stackp++ = state->finbyte = code;

	/* Generate the new entry. */
	code = state->free_ent;
	if (code < state->maxcode && state->oldcode >= 0) {
		state->prefix[code] = state->oldcode;
		state->suffix[code] = state->finbyte;
		++state->free_ent;
	}
	if (state->free_ent > state->section_end_code) {
		state->bits++;
		state->bytes_in_section = 0;
		if (state->bits == state->maxcode_bits)
			state->section_end_code = state->maxcode;
		else
			state->section_end_code = (1 << state->bits) - 1;
	}

	/* Remember previous code. */
	state->oldcode = newcode;
	return (ARCHIVE_OK);
}

/*
 * Return next 'n' bits from stream.
 *
 * -1 indicates end of available data.
 */
static int
getbits(struct archive_read *a, struct private_data *state, int n)
{
	int code, ret;
	static const int mask[] = {
		0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff,
		0x1ff, 0x3ff, 0x7ff, 0xfff, 0x1fff, 0x3fff, 0x7fff, 0xffff
	};
	const void *read_buf;

	while (state->bits_avail < n) {
		if (state->avail_in <= 0) {
			read_buf = state->next_in;
			ret = (a->client_reader)(&a->archive, a->client_data,
			    &read_buf);
			state->next_in = read_buf;
			if (ret < 0)
				return (ARCHIVE_FATAL);
			if (ret == 0)
				return (ARCHIVE_EOF);
			a->archive.raw_position += ret;
			state->avail_in = ret;
		}
		state->bit_buffer |= *state->next_in++ << state->bits_avail;
		state->avail_in--;
		state->bits_avail += 8;
		state->bytes_in_section++;
	}

	code = state->bit_buffer;
	state->bit_buffer >>= n;
	state->bits_avail -= n;

	return (code & mask[n]);
}
