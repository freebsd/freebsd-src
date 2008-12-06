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
	unsigned char	*out_block;
	size_t		 out_block_size;
	int64_t		 total_out;
	unsigned long	 crc;
	int		 header_count;
	char		 header_done;
	char		 header_state;
	char		 header_flags;
	char		 eof; /* True = found end of compressed data. */
};

/* Gzip Source. */
static ssize_t	gzip_source_read(struct archive_read_source *, const void **);
static int	gzip_source_close(struct archive_read_source *);
#endif

/*
 * Note that we can detect gzip archives even if we can't decompress
 * them.  (In fact, we like detecting them because we can give better
 * error messages.)  So the bid framework here gets compiled even
 * if zlib is unavailable.
 */
static int	gzip_reader_bid(struct archive_reader *, const void *, size_t);
static struct archive_read_source *gzip_reader_init(struct archive_read *,
    struct archive_reader *, struct archive_read_source *,
    const void *, size_t);
static int	gzip_reader_free(struct archive_reader *);

int
archive_read_support_compression_gzip(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct archive_reader *reader = __archive_read_get_reader(a);

	if (reader == NULL)
		return (ARCHIVE_FATAL);

	reader->data = NULL;
	reader->bid = gzip_reader_bid;
	reader->init = gzip_reader_init;
	reader->free = gzip_reader_free;
	return (ARCHIVE_OK);
}

static int
gzip_reader_free(struct archive_reader *self){
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
gzip_reader_bid(struct archive_reader *self, const void *buff, size_t len)
{
	const unsigned char *buffer;
	int bits_checked;

	(void)self; /* UNUSED */

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
static struct archive_read_source *
gzip_reader_init(struct archive_read *a, struct archive_reader *reader,
    struct archive_read_source *upstream, const void *buff, size_t n)
{
	(void)a;	/* UNUSED */
	(void)buff;	/* UNUSED */
	(void)n;	/* UNUSED */

	archive_set_error(&a->archive, -1,
	    "This version of libarchive was compiled without gzip support");
	return (NULL);
}

#else

/*
 * Initialize the source object.
 */
static struct archive_read_source *
gzip_reader_init(struct archive_read *a, struct archive_reader *reader,
    struct archive_read_source *upstream, const void *buff, size_t n)
{
	static const size_t out_block_size = 64 * 1024;
	void *out_block;
	struct archive_read_source *self;
	struct private_data *state;

	(void)reader; /* UNUSED */

	a->archive.compression_code = ARCHIVE_COMPRESSION_GZIP;
	a->archive.compression_name = "gzip";

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
	self->read = gzip_source_read;
	self->skip = NULL; /* not supported */
	self->close = gzip_source_close;

	state->crc = crc32(0L, NULL, 0);
	state->header_done = 0; /* We've not yet begun to parse header... */

	/*
	 * A bug in zlib.h: stream.next_in should be marked 'const'
	 * but isn't (the library never alters data through the
	 * next_in pointer, only reads it).  The result: this ugly
	 * cast to remove 'const'.
	 */
	state->stream.next_in = (Bytef *)(uintptr_t)(const void *)buff;
	state->stream.avail_in = n;

	return (self);
}

static int
header(struct archive_read_source *self)
{
	struct private_data *state;
	int ret, b;

	state = (struct private_data *)self->data;

	/*
	 * If still parsing the header, interpret the
	 * next byte.
	 */
	b = *(state->stream.next_in++);
	state->stream.avail_in--;

	/*
	 * Simple state machine to parse the GZip header one byte at
	 * a time.  If you see a way to make this easier to understand,
	 * please let me know. ;-)
	 */
	switch (state->header_state) {
	case 0: /* First byte of signature. */
		/* We only return EOF for a failure here. */
		if (b != 037)
			return (ARCHIVE_EOF);
		state->header_state = 1;
		break;
	case 1: /* Second byte of signature. */
	case 2: /* Compression type must be 8 == deflate. */
		if (b != (0xff & "\037\213\010"[(int)state->header_state])) {
			archive_set_error(&self->archive->archive,
			    ARCHIVE_ERRNO_MISC,
			    "Invalid GZip header (saw %d at offset %d)",
			    b, state->header_state);
			return (ARCHIVE_FATAL);
		}
		++state->header_state;
		break;
	case 3: /* GZip flags. */
		state->header_flags = b;
		state->header_state = 4;
		break;
	case 4: case 5: case 6: case 7: /* Mod time. */
	case 8: /* Deflate flags. */
	case 9: /* OS. */
		++state->header_state;
		break;
	case 10: /* Optional Extra: First byte of Length. */
		if ((state->header_flags & 4)) {
			state->header_count = 255 & (int)b;
			state->header_state = 11;
			break;
		}
		/* Fall through if no Optional Extra field. */
	case 11: /* Optional Extra: Second byte of Length. */
		if ((state->header_flags & 4)) {
			state->header_count
			    = (0xff00 & ((int)b << 8)) | state->header_count;
			state->header_state = 12;
			break;
		}
		/* Fall through if no Optional Extra field. */
	case 12: /* Optional Extra Field: counted length. */
		if ((state->header_flags & 4)) {
			--state->header_count;
			if (state->header_count == 0) state->header_state = 13;
			else state->header_state = 12;
			break;
		}
		/* Fall through if no Optional Extra field. */
	case 13: /* Optional Original Filename. */
		if ((state->header_flags & 8)) {
			if (b == 0) state->header_state = 14;
			else state->header_state = 13;
			break;
		}
		/* Fall through if no Optional Original Filename. */
	case 14: /* Optional Comment. */
		if ((state->header_flags & 16)) {
			if (b == 0) state->header_state = 15;
			else state->header_state = 14;
			break;
		}
		/* Fall through if no Optional Comment. */
	case 15: /* Optional Header CRC: First byte. */
		if ((state->header_flags & 2)) {
			state->header_state = 16;
			break;
		}
		/* Fall through if no Optional Header CRC. */
	case 16: /* Optional Header CRC: Second byte. */
		if ((state->header_flags & 2)) {
			state->header_state = 17;
			break;
		}
		/* Fall through if no Optional Header CRC. */
	case 17: /* First byte of compressed data. */
		state->header_done = 1; /* done with header */
		state->stream.avail_in++; /* Discard first byte. */
		state->stream.next_in--;

		/* Initialize compression library. */
		ret = inflateInit2(&(state->stream),
		    -15 /* Don't check for zlib header */);

		/* Decipher the error code. */
		switch (ret) {
		case Z_OK:
			return (ARCHIVE_OK);
		case Z_STREAM_ERROR:
			archive_set_error(&self->archive->archive,
			    ARCHIVE_ERRNO_MISC,
			    "Internal error initializing compression library: "
			    "invalid setup parameter");
			break;
		case Z_MEM_ERROR:
			archive_set_error(&self->archive->archive, ENOMEM,
			    "Internal error initializing compression library: "
			    "out of memory");
			break;
		case Z_VERSION_ERROR:
			archive_set_error(&self->archive->archive,
			    ARCHIVE_ERRNO_MISC,
			    "Internal error initializing compression library: "
			    "invalid library version");
			break;
		default:
			archive_set_error(&self->archive->archive,
			    ARCHIVE_ERRNO_MISC,
			    "Internal error initializing compression library: "
			    " Zlib error %d", ret);
			break;
		}
		return (ARCHIVE_FATAL);
	}

	return (ARCHIVE_OK);
}

static ssize_t
gzip_source_read(struct archive_read_source *self, const void **p)
{
	struct private_data *state;
	size_t read_avail, decompressed;
	const void *read_buf;
	int ret;

	state = (struct private_data *)self->data;
	read_avail = 0;

	/* Empty our output buffer. */
	state->stream.next_out = state->out_block;
	state->stream.avail_out = state->out_block_size;

	/* Try to fill the output buffer. */
	while (state->stream.avail_out > 0 && !state->eof) {
		/* If the last upstream block is done, get another one. */
		if (state->stream.avail_in == 0) {
			ret = (self->upstream->read)(self->upstream,
			    &read_buf);
			/* stream.next_in is really const, but zlib
			 * doesn't declare it so. <sigh> */
			state->stream.next_in
			    = (unsigned char *)(uintptr_t)read_buf;
			if (ret < 0)
				return (ARCHIVE_FATAL);
			state->stream.avail_in = ret;
			/* There is no more data, return whatever we have. */
			if (ret == 0) {
				state->eof = 1;
				break;
			}
		}

		/* If we're still parsing header bytes, walk through those. */
		if (!state->header_done) {
			ret = header(self);
			if (ret < ARCHIVE_OK)
				return (ret);
			if (ret == ARCHIVE_EOF)
				state->eof = 1;
		} else {
			/* Decompress as much as we can in one pass. */
			/* XXX Skip trailer XXX */
			ret = inflate(&(state->stream), 0);
			switch (ret) {
			case Z_STREAM_END: /* Found end of stream. */
				switch (inflateEnd(&(state->stream))) {
				case Z_OK:
					break;
				default:
					archive_set_error(&self->archive->archive,
						ARCHIVE_ERRNO_MISC,
						"Failed to clean up gzip decompressor");
					return (ARCHIVE_FATAL);
				}
				/* Restart header parser with the next block. */
				state->header_state = state->header_done = 0;
				/* FALL THROUGH */
			case Z_OK: /* Decompressor made some progress. */
				/* If we filled our buffer, update stats and return. */
				break;
			default:
				/* Return an error. */
				archive_set_error(&self->archive->archive,
						  ARCHIVE_ERRNO_MISC,
						  "%s decompression failed",
						  self->archive->archive.compression_name);
				return (ARCHIVE_FATAL);
			}
		}
	}

	/* We've read as much as we can. */
	decompressed = state->stream.next_out - state->out_block;
	state->total_out += decompressed;
	if (decompressed == 0)
		*p = NULL;
	else
		*p = state->out_block;
	return (decompressed);

}

/*
 * Clean up the decompressor.
 */
static int
gzip_source_close(struct archive_read_source *self)
{
	struct private_data *state;
	int ret;

	state = (struct private_data *)self->data;
	ret = ARCHIVE_OK;

	if (state->header_done) {
		switch (inflateEnd(&(state->stream))) {
		case Z_OK:
			break;
		default:
			archive_set_error(&(self->archive->archive),
					  ARCHIVE_ERRNO_MISC,
					  "Failed to clean up %s compressor",
					  self->archive->archive.compression_name);
			ret = ARCHIVE_FATAL;
		}
	}

	free(state->out_block);
	free(state);
	free(self);
	return (ret);
}

#endif /* HAVE_ZLIB_H */
