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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_endian.h"
#include "archive_private.h"
#include "archive_read_private.h"

#ifdef HAVE_ZLIB_H
struct gzip {
	z_stream	 stream;
	char		 in_stream;
	unsigned char	*out_block;
	size_t		 out_block_size;
	unsigned long	 crc;
	uint32_t	 mtime;
	char		*name;
	char		 eof; /* True = found end of compressed data. */
};

/* Gzip Filter. */
static ssize_t	gzip_filter_read(struct archive_read_filter *, const void **);
static int	gzip_filter_close(struct archive_read_filter *);
#endif

/*
 * Note that we can detect gzip archives even if we can't decompress
 * them.  (In fact, we like detecting them because we can give better
 * error messages.)
 */
static int	gzip_bidder_bid(struct archive_read_filter_bidder *,
		    struct archive_read_filter *);
static int	gzip_bidder_init(struct archive_read_filter *);

#if ARCHIVE_VERSION_NUMBER < 4000000
/* Deprecated; remove in libarchive 4.0 */
int
archive_read_support_compression_gzip(struct archive *a)
{
	return archive_read_support_filter_gzip(a);
}
#endif

static const struct archive_read_filter_bidder_vtable
gzip_bidder_vtable = {
	.bid = gzip_bidder_bid,
	.init = gzip_bidder_init,
};

int
archive_read_support_filter_gzip(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;

	if (__archive_read_register_bidder(a, NULL, "gzip",
				&gzip_bidder_vtable) != ARCHIVE_OK)
		return (ARCHIVE_FATAL);

	/* Signal the extent of gzip support with the return value here. */
#if HAVE_ZLIB_H
	return (ARCHIVE_OK);
#else
	archive_set_error(_a, ARCHIVE_ERRNO_MISC,
	    "Using external gzip program");
	return (ARCHIVE_WARN);
#endif
}

/*
 * Read and verify the header.
 *
 * Returns zero if the header couldn't be validated, else returns
 * number of bytes in header.  If pbits is non-NULL, it receives a
 * count of bits verified, suitable for use by bidder.
 */
#define MAX_FILENAME_LENGTH (1024 * 1024L)
#define MAX_COMMENT_LENGTH (1024 * 1024L)
static ssize_t
peek_at_header(struct archive_read_filter *f, int *pbits,
#ifdef HAVE_ZLIB_H
	       struct gzip *gzip
#else
	       void *state
#endif
	      )
{
	const unsigned char *p;
	ssize_t avail, len;
	int bits = 0;
	int header_flags;
#ifndef HAVE_ZLIB_H
	(void)state; /* UNUSED */
#endif

	/* Start by looking at the first ten bytes of the header, which
	 * is all fixed layout. */
	len = 10;
	p = __archive_read_filter_ahead(f, len, &avail);
	if (p == NULL)
		return (0);
	/* We only support deflation- third byte must be 0x08. */
	if (memcmp(p, "\x1F\x8B\x08", 3) != 0)
		return (0);
	bits += 24;
	if ((p[3] & 0xE0)!= 0)	/* No reserved flags set. */
		return (0);
	bits += 3;
	header_flags = p[3];
	/* Bytes 4-7 are mod time in little endian. */
#ifdef HAVE_ZLIB_H
	if (gzip)
		gzip->mtime = archive_le32dec(p + 4);
#endif
	/* Byte 8 is deflate flags. */
	/* XXXX TODO: return deflate flags back to consume_header for use
	   in initializing the decompressor. */
	/* Byte 9 is OS. */

	/* Optional extra data:  2 byte length plus variable body. */
	if (header_flags & 4) {
		p = __archive_read_filter_ahead(f, len + 2, &avail);
		if (p == NULL)
			return (0);
		len += archive_le16dec(p + len);
		len += 2;
	}

	/* Null-terminated optional filename. */
	if (header_flags & 8) {
#ifdef HAVE_ZLIB_H
		ssize_t file_start = len;
#endif
		do {
			++len;
			if (avail < len) {
				if (avail > MAX_FILENAME_LENGTH) {
					return (0);
				}
				p = __archive_read_filter_ahead(f,
				    len, &avail);
			}
			if (p == NULL)
				return (0);
		} while (p[len - 1] != 0);

#ifdef HAVE_ZLIB_H
		if (gzip) {
			/* Reset the name in case of repeat header reads. */
			free(gzip->name);
			gzip->name = strdup((const char *)&p[file_start]);
		}
#endif
	}

	/* Null-terminated optional comment. */
	if (header_flags & 16) {
		do {
			++len;
			if (avail < len) {
				if (avail > MAX_COMMENT_LENGTH) {
					return (0);
				}
				p = __archive_read_filter_ahead(f,
				    len, &avail);
			}
			if (p == NULL)
				return (0);
		} while (p[len - 1] != 0);
	}

	/* Optional header CRC */
	if ((header_flags & 2)) {
		p = __archive_read_filter_ahead(f, len + 2, &avail);
		if (p == NULL)
			return (0);
#if 0
	int hcrc = ((int)p[len + 1] << 8) | (int)p[len];
	int crc = /* XXX TODO: Compute header CRC. */;
	if (crc != hcrc)
		return (0);
	bits += 16;
#endif
		len += 2;
	}

	if (pbits != NULL)
		*pbits = bits;
	return (len);
}

/*
 * Bidder just verifies the header and returns the number of verified bits.
 */
static int
gzip_bidder_bid(struct archive_read_filter_bidder *b,
    struct archive_read_filter *f)
{
	int bits_checked;

	(void)b; /* UNUSED */

	if (peek_at_header(f, &bits_checked, NULL))
		return (bits_checked);
	return (0);
}

#ifndef HAVE_ZLIB_H

/*
 * If we don't have the library on this system, we can't do the
 * decompression directly.  We can, however, try to run "gzip -d"
 * in case that's available.
 */
static int
gzip_bidder_init(struct archive_read_filter *f)
{
	int r;

	r = __archive_read_program(f, "gzip -d");
	/* Note: We set the format here even if __archive_read_program()
	 * above fails.  We do, after all, know what the format is
	 * even if we weren't able to read it. */
	f->code = ARCHIVE_FILTER_GZIP;
	f->name = "gzip";
	return (r);
}

#else

static int
gzip_read_header(struct archive_read_filter *f, struct archive_entry *entry)
{
	struct gzip *gzip = f->data;

	/* An mtime of 0 is considered invalid/missing. */
	if (gzip->mtime != 0)
		archive_entry_set_mtime(entry, gzip->mtime, 0);

	/* If the name is available, extract it. */
	if (gzip->name)
		archive_entry_set_pathname(entry, gzip->name);

	return (ARCHIVE_OK);
}

static const struct archive_read_filter_vtable
gzip_reader_vtable = {
	.read = gzip_filter_read,
	.close = gzip_filter_close,
#ifdef HAVE_ZLIB_H
	.read_header = gzip_read_header,
#endif
};

/*
 * Initialize the filter object.
 */
static int
gzip_bidder_init(struct archive_read_filter *f)
{
	struct gzip *gzip;
	static const size_t out_block_size = 64 * 1024;
	void *out_block;

	f->code = ARCHIVE_FILTER_GZIP;
	f->name = "gzip";

	gzip = calloc(1, sizeof(*gzip));
	out_block = malloc(out_block_size);
	if (gzip == NULL || out_block == NULL) {
		free(out_block);
		free(gzip);
		archive_set_error(&f->archive->archive, ENOMEM,
		    "Can't allocate data for gzip decompression");
		return (ARCHIVE_FATAL);
	}

	f->data = gzip;
	gzip->out_block_size = out_block_size;
	gzip->out_block = out_block;
	f->vtable = &gzip_reader_vtable;

	gzip->in_stream = 0; /* We're not actually within a stream yet. */

	return (ARCHIVE_OK);
}

static int
consume_header(struct archive_read_filter *f)
{
	struct gzip *gzip = f->data;
	ssize_t avail, max_in;
	size_t len;
	int ret;

	/* If this is a real header, consume it. */
	len = peek_at_header(f->upstream, NULL, gzip);
	if (len == 0)
		return (ARCHIVE_EOF);
	__archive_read_filter_consume(f->upstream, len);

	/* Initialize CRC accumulator. */
	gzip->crc = crc32(0L, NULL, 0);

	/* Initialize compression library. */
	gzip->stream.next_in = (unsigned char *)(uintptr_t)
	    __archive_read_filter_ahead(f->upstream, 1, &avail);
	if (avail < 0) {
		archive_set_error(&f->archive->archive,
		    ARCHIVE_ERRNO_MISC,
		    "Failed to read gzip input");
		return (ARCHIVE_FATAL);
	}
	if (UINT_MAX >= SSIZE_MAX)
		max_in = SSIZE_MAX;
	else
		max_in = UINT_MAX;
	if (avail > max_in)
		avail = max_in;
	gzip->stream.avail_in = (uInt)avail;
	ret = inflateInit2(&(gzip->stream),
	    -15 /* Don't check for zlib header */);

	/* Decipher the error code. */
	switch (ret) {
	case Z_OK:
		gzip->in_stream = 1;
		return (ARCHIVE_OK);
	case Z_STREAM_ERROR:
		archive_set_error(&f->archive->archive,
		    ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "invalid setup parameter");
		break;
	case Z_MEM_ERROR:
		archive_set_error(&f->archive->archive, ENOMEM,
		    "Internal error initializing compression library: "
		    "out of memory");
		break;
	case Z_VERSION_ERROR:
		archive_set_error(&f->archive->archive,
		    ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "invalid library version");
		break;
	default:
		archive_set_error(&f->archive->archive,
		    ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    " Zlib error %d", ret);
		break;
	}
	return (ARCHIVE_FATAL);
}

static int
consume_trailer(struct archive_read_filter *f)
{
	struct gzip *gzip = f->data;
	const unsigned char *p;

	gzip->in_stream = 0;
	switch (inflateEnd(&(gzip->stream))) {
	case Z_OK:
		break;
	default:
		archive_set_error(&f->archive->archive,
		    ARCHIVE_ERRNO_MISC,
		    "Failed to clean up gzip decompressor");
		return (ARCHIVE_FATAL);
	}

	/* GZip trailer is a fixed 8 byte structure. */
	p = __archive_read_filter_ahead(f->upstream, 8, NULL);
	if (p == NULL)
		return (ARCHIVE_FATAL);

	/* XXX TODO: Verify the length and CRC. */

	/* We've verified the trailer, so consume it now. */
	__archive_read_filter_consume(f->upstream, 8);

	return (ARCHIVE_OK);
}

static ssize_t
gzip_filter_read(struct archive_read_filter *f, const void **p)
{
	struct gzip *gzip = f->data;
	size_t decompressed;
	ssize_t avail_in, max_in;
	int ret;

	/* Empty our output buffer. */
	gzip->stream.next_out = gzip->out_block;
	gzip->stream.avail_out = (uInt)gzip->out_block_size;

	/* Try to fill the output buffer. */
	while (gzip->stream.avail_out > 0 && !gzip->eof) {
		/* If we're not in a stream, read a header
		 * and initialize the decompression library. */
		if (!gzip->in_stream) {
			ret = consume_header(f);
			if (ret == ARCHIVE_EOF) {
				gzip->eof = 1;
				break;
			}
			if (ret < ARCHIVE_OK)
				return (ret);
		}

		/* Peek at the next available data. */
		/* ZLib treats stream.next_in as const but doesn't declare
		 * it so, hence this ugly cast. */
		gzip->stream.next_in = (unsigned char *)(uintptr_t)
		    __archive_read_filter_ahead(f->upstream, 1, &avail_in);
		if (gzip->stream.next_in == NULL) {
			archive_set_error(&f->archive->archive,
			    ARCHIVE_ERRNO_MISC,
			    "truncated gzip input");
			return (ARCHIVE_FATAL);
		}
		if (UINT_MAX >= SSIZE_MAX)
			max_in = SSIZE_MAX;
		else
			max_in = UINT_MAX;
		if (avail_in > max_in)
			avail_in = max_in;
		gzip->stream.avail_in = (uInt)avail_in;

		/* Decompress and consume some of that data. */
		ret = inflate(&(gzip->stream), 0);
		switch (ret) {
		case Z_OK: /* Decompressor made some progress. */
			__archive_read_filter_consume(f->upstream,
			    avail_in - gzip->stream.avail_in);
			break;
		case Z_STREAM_END: /* Found end of stream. */
			__archive_read_filter_consume(f->upstream,
			    avail_in - gzip->stream.avail_in);
			/* Consume the stream trailer; release the
			 * decompression library. */
			ret = consume_trailer(f);
			if (ret < ARCHIVE_OK)
				return (ret);
			break;
		default:
			/* Return an error. */
			archive_set_error(&f->archive->archive,
			    ARCHIVE_ERRNO_MISC,
			    "gzip decompression failed");
			return (ARCHIVE_FATAL);
		}
	}

	/* We've read as much as we can. */
	decompressed = gzip->stream.next_out - gzip->out_block;
	if (decompressed == 0)
		*p = NULL;
	else
		*p = gzip->out_block;
	return (decompressed);
}

/*
 * Clean up the decompressor.
 */
static int
gzip_filter_close(struct archive_read_filter *f)
{
	struct gzip *gzip = f->data;
	int ret;

	ret = ARCHIVE_OK;

	if (gzip->in_stream) {
		switch (inflateEnd(&(gzip->stream))) {
		case Z_OK:
			break;
		default:
			archive_set_error(&(f->archive->archive),
			    ARCHIVE_ERRNO_MISC,
			    "Failed to clean up gzip compressor");
			ret = ARCHIVE_FATAL;
		}
	}

	free(gzip->name);
	free(gzip->out_block);
	free(gzip);
	return (ret);
}

#endif /* HAVE_ZLIB_H */
