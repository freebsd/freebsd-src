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
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_BZLIB_H
#include <bzlib.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_read_private.h"

#if HAVE_BZLIB_H
struct private_data {
	bz_stream	 stream;
	char		*out_block;
	size_t		 out_block_size;
	char		 valid; /* True = decompressor is initialized */
	char		 eof; /* True = found end of compressed data. */
};

/* Bzip2 source */
static ssize_t	bzip2_source_read(struct archive_read_source *, const void **);
static int	bzip2_source_close(struct archive_read_source *);
#endif

/*
 * Note that we can detect bzip2 archives even if we can't decompress
 * them.  (In fact, we like detecting them because we can give better
 * error messages.)  So the bid framework here gets compiled even
 * if bzlib is unavailable.
 */
static int	bzip2_reader_bid(struct archive_reader *, const void *, size_t);
static struct archive_read_source *bzip2_reader_init(struct archive_read *,
    struct archive_reader *, struct archive_read_source *,
    const void *, size_t);
static int	bzip2_reader_free(struct archive_reader *);

int
archive_read_support_compression_bzip2(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct archive_reader *reader = __archive_read_get_reader(a);

	if (reader == NULL)
		return (ARCHIVE_FATAL);

	reader->data = NULL;
	reader->bid = bzip2_reader_bid;
	reader->init = bzip2_reader_init;
	reader->free = bzip2_reader_free;
	return (ARCHIVE_OK);
}

static int
bzip2_reader_free(struct archive_reader *self){
	(void)self; /* UNUSED */
	return (ARCHIVE_OK);
}

/*
 * Test whether we can handle this data.
 *
 * This logic returns zero if any part of the signature fails.  It
 * also tries to Do The Right Thing if a very short buffer prevents us
 * from verifying as much as we would like.
 */
static int
bzip2_reader_bid(struct archive_reader *self, const void *buff, size_t len)
{
	const unsigned char *buffer;
	int bits_checked;

	(void)self; /* UNUSED */

	if (len < 1)
		return (0);

	buffer = (const unsigned char *)buff;
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
	if (len < 5)
		return (bits_checked);

	/* After BZh[1-9], there must be either a data block
	 * which begins with 0x314159265359 or an end-of-data
	 * marker of 0x177245385090. */

	if (buffer[4] == 0x31) {
		/* Verify the data block signature. */
		size_t s = len;
		if (s > 10) s = 10;
		if (memcmp(buffer + 4, "\x31\x41\x59\x26\x53\x59", s - 4) != 0)
			return (0);
		bits_checked += 8 * (s - 4);
	} else if (buffer[4] == 0x17) {
		/* Verify the end-of-data marker. */
		size_t s = len;
		if (s > 10) s = 10;
		if (memcmp(buffer + 4, "\x17\x72\x45\x38\x50\x90", s - 4) != 0)
			return (0);
		bits_checked += 8 * (s - 4);
	} else
		return (0);

	return (bits_checked);
}

#ifndef HAVE_BZLIB_H

/*
 * If we don't have the library on this system, we can't actually do the
 * decompression.  We can, however, still detect compressed archives
 * and emit a useful message.
 */
static struct archive_read_source *
bzip2_reader_init(struct archive_read *a, struct archive_reader *reader,
    struct archive_read_source *upstream, const void *buff, size_t n)
{
	(void)a;	/* UNUSED */
	(void)reader;	/* UNUSED */
	(void)upstream; /* UNUSED */
	(void)buff;	/* UNUSED */
	(void)n;	/* UNUSED */

	archive_set_error(&a->archive, -1,
	    "This version of libarchive was compiled without bzip2 support");
	return (NULL);
}


#else

/*
 * Setup the callbacks.
 */
static struct archive_read_source *
bzip2_reader_init(struct archive_read *a, struct archive_reader *reader,
    struct archive_read_source *upstream, const void *buff, size_t n)
{
	static const size_t out_block_size = 64 * 1024;
	void *out_block;
	struct archive_read_source *self;
	struct private_data *state;

	(void)reader; /* UNUSED */

	a->archive.compression_code = ARCHIVE_COMPRESSION_BZIP2;
	a->archive.compression_name = "bzip2";

	self = calloc(sizeof(*self), 1);
	state = (struct private_data *)calloc(sizeof(*state), 1);
	out_block = (unsigned char *)malloc(out_block_size);
	if (self == NULL || state == NULL || out_block == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate data for %s decompression",
		    a->archive.compression_name);
		free(out_block);
		free(state);
		free(self);
		return (NULL);
	}


	self->archive = a;
	self->data = state;
	state->out_block_size = out_block_size;
	state->out_block = out_block;
	self->upstream = upstream;
	self->read = bzip2_source_read;
	self->skip = NULL; /* not supported */
	self->close = bzip2_source_close;

	/*
	 * A bug in bzlib.h: stream.next_in should be marked 'const'
	 * but isn't (the library never alters data through the
	 * next_in pointer, only reads it).  The result: this ugly
	 * cast to remove 'const'.
	 */
	state->stream.next_in = (char *)(uintptr_t)(const void *)buff;
	state->stream.avail_in = n;

	state->stream.next_out = state->out_block;
	state->stream.avail_out = state->out_block_size;

	return (self);
}

/*
 * Return the next block of decompressed data.
 */
static ssize_t
bzip2_source_read(struct archive_read_source *self, const void **p)
{
	struct private_data *state;
	size_t read_avail, decompressed;
	const void *read_buf;
	int ret;

	state = (struct private_data *)self->data;
	read_avail = 0;

	if (state->eof) {
		*p = NULL;
		return (0);
	}

	/* Empty our output buffer. */
	state->stream.next_out = state->out_block;
	state->stream.avail_out = state->out_block_size;

	/* Try to fill the output buffer. */
	for (;;) {
		/* If the last upstream block is done, get another one. */
		if (state->stream.avail_in == 0) {
			ret = (self->upstream->read)(self->upstream,
			    &read_buf);
			/* stream.next_in is really const, but bzlib
			 * doesn't declare it so. <sigh> */
			state->stream.next_in
			    = (unsigned char *)(uintptr_t)read_buf;
			if (ret < 0)
				return (ARCHIVE_FATAL);
			/* There is no more data, return whatever we have. */
			if (ret == 0) {
				state->eof = 1;
				*p = state->out_block;
				decompressed = state->stream.next_out
				    - state->out_block;
				return (decompressed);
			}
			state->stream.avail_in = ret;
		}

		if (!state->valid) {
			if (state->stream.next_in[0] != 'B') {
				state->eof = 1;
				*p = state->out_block;
				decompressed = state->stream.next_out
				    - state->out_block;
				return (decompressed);
			}
			/* Initialize compression library. */
			ret = BZ2_bzDecompressInit(&(state->stream),
					   0 /* library verbosity */,
					   0 /* don't use low-mem algorithm */);

			/* If init fails, try low-memory algorithm instead. */
			if (ret == BZ_MEM_ERROR)
				ret = BZ2_bzDecompressInit(&(state->stream),
					   0 /* library verbosity */,
					   1 /* do use low-mem algo */);

			if (ret != BZ_OK) {
				const char *detail = NULL;
				int err = ARCHIVE_ERRNO_MISC;
				switch (ret) {
				case BZ_PARAM_ERROR:
					detail = "invalid setup parameter";
					break;
				case BZ_MEM_ERROR:
					err = ENOMEM;
					detail = "out of memory";
					break;
				case BZ_CONFIG_ERROR:
					detail = "mis-compiled library";
					break;
				}
				archive_set_error(&self->archive->archive, err,
				    "Internal error initializing decompressor%s%s",
				    detail == NULL ? "" : ": ",
				    detail);
				return (ARCHIVE_FATAL);
			}
			state->valid = 1;
		}

		/* Decompress as much as we can in one pass. */
		ret = BZ2_bzDecompress(&(state->stream));
		switch (ret) {
		case BZ_STREAM_END: /* Found end of stream. */
			switch (BZ2_bzDecompressEnd(&(state->stream))) {
			case BZ_OK:
				break;
			default:
				archive_set_error(&(self->archive->archive),
					  ARCHIVE_ERRNO_MISC,
					  "Failed to clean up decompressor");
				return (ARCHIVE_FATAL);
			}
			state->valid = 0;
			/* FALLTHROUGH */
		case BZ_OK: /* Decompressor made some progress. */
			/* If we filled our buffer, update stats and return. */
			if (state->stream.avail_out == 0) {
				*p = state->out_block;
				decompressed = state->stream.next_out
				    - state->out_block;
				return (decompressed);
			}
			break;
		default: /* Return an error. */
			archive_set_error(&self->archive->archive,
			    ARCHIVE_ERRNO_MISC, "bzip decompression failed");
			return (ARCHIVE_FATAL);
		}
	}
}

/*
 * Clean up the decompressor.
 */
static int
bzip2_source_close(struct archive_read_source *self)
{
	struct private_data *state;
	int ret = ARCHIVE_OK;

	state = (struct private_data *)self->data;

	if (state->valid) {
		switch (BZ2_bzDecompressEnd(&state->stream)) {
		case BZ_OK:
			break;
		default:
			archive_set_error(&self->archive->archive,
					  ARCHIVE_ERRNO_MISC,
					  "Failed to clean up decompressor");
			ret = ARCHIVE_FATAL;
		}
	}

	free(state->out_block);
	free(state);
	free(self);
	return (ARCHIVE_OK);
}

#endif /* HAVE_BZLIB_H */
