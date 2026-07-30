/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2012 Michihiro NAKAJIMA
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
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
#ifdef HAVE_LZO_LZOCONF_H
#include <lzo/lzoconf.h>
#endif
#ifdef HAVE_LZO_LZO1X_H
#include <lzo/lzo1x.h>
#endif
#ifdef HAVE_ZLIB_H
#include <zlib.h> /* for crc32 and adler32 */
#endif

#include "archive.h"
#if !defined(HAVE_ZLIB_H) &&\
     defined(HAVE_LZO_LZOCONF_H) && defined(HAVE_LZO_LZO1X_H)
#include "archive_crc32.h"
#endif
#include "archive_endian.h"
#include "archive_private.h"
#include "archive_read_private.h"

#ifndef HAVE_ZLIB_H
#define adler32	lzo_adler32
#endif

#define LZOP_HEADER_MAGIC "\x89\x4c\x5a\x4f\x00\x0d\x0a\x1a\x0a"
#define LZOP_HEADER_MAGIC_LEN 9

#if defined(HAVE_LZO_LZOCONF_H) && defined(HAVE_LZO_LZO1X_H)
struct lzop {
	unsigned char	*out_block;
	size_t		 out_block_size;
	int		 flags;
	uint32_t	 compressed_cksum;
	uint32_t	 uncompressed_cksum;
	size_t		 compressed_size;
	size_t		 uncompressed_size;
	size_t		 unconsumed_bytes;
	char		 in_stream;
	char		 eof; /* True = found end of compressed data. */
};

#define FILTER			0x0800
#define CRC32_HEADER		0x1000
#define EXTRA_FIELD		0x0040
#define ADLER32_UNCOMPRESSED	0x0001
#define ADLER32_COMPRESSED	0x0002
#define CRC32_UNCOMPRESSED	0x0100
#define CRC32_COMPRESSED	0x0200
#define MAX_BLOCK_SIZE		(64 * 1024 * 1024)

static ssize_t  lzop_filter_read(struct archive_read_filter *, const void **);
static int	lzop_filter_close(struct archive_read_filter *);
#endif

static int lzop_bidder_bid(struct archive_read_filter_bidder *,
    struct archive_read_filter *);
static int lzop_bidder_init(struct archive_read_filter *);

static const struct archive_read_filter_bidder_vtable
lzop_bidder_vtable = {
	.bid = lzop_bidder_bid,
	.init = lzop_bidder_init,
};

int
archive_read_support_filter_lzop(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;

	if (__archive_read_register_bidder(a, NULL, "lzop",
				&lzop_bidder_vtable) != ARCHIVE_OK)
		return (ARCHIVE_FATAL);

	/* Signal the extent of lzop support with the return value here. */
#if defined(HAVE_LZO_LZOCONF_H) && defined(HAVE_LZO_LZO1X_H)
	return (ARCHIVE_OK);
#else
	archive_set_error(_a, ARCHIVE_ERRNO_MISC,
	    "Using external lzop program for lzop decompression");
	return (ARCHIVE_WARN);
#endif
}

/*
 * Bidder just verifies the header and returns the number of verified bits.
 */
static int
lzop_bidder_bid(struct archive_read_filter_bidder *b,
    struct archive_read_filter *f)
{
	const unsigned char *p;

	(void)b; /* UNUSED */

	p = __archive_read_filter_ahead(f, LZOP_HEADER_MAGIC_LEN, NULL);
	if (p == NULL)
		return (0);

	if (memcmp(p, LZOP_HEADER_MAGIC, LZOP_HEADER_MAGIC_LEN))
		return (0);

	return (LZOP_HEADER_MAGIC_LEN * 8);
}

#if !defined(HAVE_LZO_LZOCONF_H) || !defined(HAVE_LZO_LZO1X_H)
/*
 * If we don't have the library on this system, we can't do the
 * decompression directly.  We can, however, try to run "lzop -d"
 * in case that's available.
 */
static int
lzop_bidder_init(struct archive_read_filter *f)
{
	int r;

	r = __archive_read_program(f, "lzop -d");
	/* Note: We set the format here even if __archive_read_program()
	 * above fails.  We do, after all, know what the format is
	 * even if we weren't able to read it. */
	f->code = ARCHIVE_FILTER_LZOP;
	f->name = "lzop";
	return (r);
}
#else

static const struct archive_read_filter_vtable
lzop_reader_vtable = {
	.read = lzop_filter_read,
	.close = lzop_filter_close
};

/*
 * Initialize the filter object.
 */
static int
lzop_bidder_init(struct archive_read_filter *f)
{
	struct lzop *lzop;

	f->code = ARCHIVE_FILTER_LZOP;
	f->name = "lzop";

	lzop = calloc(1, sizeof(*lzop));
	if (lzop == NULL) {
		archive_set_error(&f->archive->archive, ENOMEM,
		    "Can't allocate data for lzop decompression");
		return (ARCHIVE_FATAL);
	}

	f->data = lzop;
	f->vtable = &lzop_reader_vtable;

	return (ARCHIVE_OK);
}

static int
consume_header(struct archive_read_filter *f)
{
	struct lzop *lzop = f->data;
	const unsigned char *p, *_p;
	unsigned checksum, flags, len, method, version;

	/*
	 * Check LZOP magic code.
	 */
	p = __archive_read_filter_ahead(f->upstream,
		LZOP_HEADER_MAGIC_LEN, NULL);
	if (p == NULL)
		return (ARCHIVE_EOF);

	if (memcmp(p, LZOP_HEADER_MAGIC, LZOP_HEADER_MAGIC_LEN))
		return (ARCHIVE_EOF);
	__archive_read_filter_consume(f->upstream,
	    LZOP_HEADER_MAGIC_LEN);

	p = __archive_read_filter_ahead(f->upstream, 29, NULL);
	if (p == NULL)
		goto truncated;
	_p = p;
	version = archive_be16dec(p);
	p += 4;/* version(2 bytes) + library version(2 bytes) */

	if (version >= 0x940) {
		unsigned reqversion = archive_be16dec(p); p += 2;
		if (reqversion < 0x900) {
			archive_set_error(&f->archive->archive,
			    ARCHIVE_ERRNO_MISC, "Invalid required version");
			return (ARCHIVE_FAILED);
		}
	}

	method = *p++;
	if (method < 1 || method > 3) {
		archive_set_error(&f->archive->archive, ARCHIVE_ERRNO_MISC,
		    "Unsupported method");
		return (ARCHIVE_FAILED);
	}

	if (version >= 0x940) {
		unsigned level = *p++;
#if 0
		unsigned default_level[] = {0, 3, 1, 9};
#endif
		if (level == 0)
			/* Method is 1..3 here due to check above. */
#if 0	/* Avoid an error Clang Static Analyzer claims
	  "Value stored to 'level' is never read". */
			level = default_level[method];
#else
			;/* NOP */
#endif
		else if (level > 9) {
			archive_set_error(&f->archive->archive,
			    ARCHIVE_ERRNO_MISC, "Invalid level");
			return (ARCHIVE_FAILED);
		}
	}

	flags = archive_be32dec(p); p += 4;

	if (flags & FILTER)
		p += 4; /* Skip filter */
	p += 4; /* Skip mode */
	if (version >= 0x940)
		p += 8; /* Skip mtime */
	else
		p += 4; /* Skip mtime */
	len = *p++; /* Read filename length */
	len += p - _p;
	/* Make sure we have all bytes we need to calculate checksum. */
	p = __archive_read_filter_ahead(f->upstream, len + 4, NULL);
	if (p == NULL)
		goto truncated;
	if (flags & CRC32_HEADER)
		checksum = crc32(crc32(0, NULL, 0), p, len);
	else
		checksum = adler32(adler32(0, NULL, 0), p, len);
#ifndef DONT_FAIL_ON_CRC_ERROR
	if (archive_be32dec(p + len) != checksum)
		goto corrupted;
#endif
	__archive_read_filter_consume(f->upstream, len + 4);
	if (flags & EXTRA_FIELD) {
		/* Skip extra field */
		p = __archive_read_filter_ahead(f->upstream, 4, NULL);
		if (p == NULL)
			goto truncated;
		len = archive_be32dec(p);
		__archive_read_filter_consume(f->upstream,
		    (int64_t)len + 4 + 4);
	}
	lzop->flags = flags;
	lzop->in_stream = 1;
	return (ARCHIVE_OK);
truncated:
	archive_set_error(&f->archive->archive,
	    ARCHIVE_ERRNO_FILE_FORMAT, "Truncated lzop data");
	return (ARCHIVE_FAILED);
corrupted:
	archive_set_error(&f->archive->archive,
	    ARCHIVE_ERRNO_FILE_FORMAT, "Corrupted lzop header");
	return (ARCHIVE_FAILED);
}

static int
consume_block_info(struct archive_read_filter *f)
{
	struct lzop *lzop = f->data;
	const unsigned char *p;
	unsigned flags = lzop->flags;

	p = __archive_read_filter_ahead(f->upstream, 4, NULL);
	if (p == NULL)
		goto truncated;
	lzop->uncompressed_size = archive_be32dec(p);
	__archive_read_filter_consume(f->upstream, 4);
	if (lzop->uncompressed_size == 0)
		return (ARCHIVE_EOF);
	if (lzop->uncompressed_size > MAX_BLOCK_SIZE)
		goto corrupted;

	p = __archive_read_filter_ahead(f->upstream, 4, NULL);
	if (p == NULL)
		goto truncated;
	lzop->compressed_size = archive_be32dec(p);
	__archive_read_filter_consume(f->upstream, 4);
	if (lzop->compressed_size > lzop->uncompressed_size)
		goto corrupted;

	if (flags & (CRC32_UNCOMPRESSED | ADLER32_UNCOMPRESSED)) {
		p = __archive_read_filter_ahead(f->upstream, 4, NULL);
		if (p == NULL)
			goto truncated;
		lzop->compressed_cksum = lzop->uncompressed_cksum =
		    archive_be32dec(p);
		__archive_read_filter_consume(f->upstream, 4);
	}
	if ((flags & (CRC32_COMPRESSED | ADLER32_COMPRESSED)) &&
	    lzop->compressed_size < lzop->uncompressed_size) {
		p = __archive_read_filter_ahead(f->upstream, 4, NULL);
		if (p == NULL)
			goto truncated;
		lzop->compressed_cksum = archive_be32dec(p);
		__archive_read_filter_consume(f->upstream, 4);
	}
	return (ARCHIVE_OK);
truncated:
	archive_set_error(&f->archive->archive,
	    ARCHIVE_ERRNO_FILE_FORMAT, "Truncated lzop data");
	return (ARCHIVE_FAILED);
corrupted:
	archive_set_error(&f->archive->archive,
	    ARCHIVE_ERRNO_FILE_FORMAT, "Corrupted lzop header");
	return (ARCHIVE_FAILED);
}

static ssize_t
lzop_filter_read(struct archive_read_filter *f, const void **p)
{
	struct lzop *lzop = f->data;
	const void *b;
	lzo_uint out_size;
	uint32_t cksum;
	int ret, r;

	if (lzop->unconsumed_bytes) {
		__archive_read_filter_consume(f->upstream,
		    lzop->unconsumed_bytes);
		lzop->unconsumed_bytes = 0;
	}
	if (lzop->eof)
		return (0);

	for (;;) {
		if (!lzop->in_stream) {
			ret = consume_header(f);
			if (ret < ARCHIVE_OK)
				return (ret);
			if (ret == ARCHIVE_EOF) {
				lzop->eof = 1;
				return (0);
			}
		}
		ret = consume_block_info(f);
		if (ret < ARCHIVE_OK)
			return (ret);
		if (ret == ARCHIVE_EOF)
			lzop->in_stream = 0;
		else
			break;
	}

	if (lzop->out_block == NULL ||
	    lzop->out_block_size < lzop->uncompressed_size) {
		void *new_block;

		new_block = realloc(lzop->out_block, lzop->uncompressed_size);
		if (new_block == NULL) {
			archive_set_error(&f->archive->archive, ENOMEM,
			    "Can't allocate data for lzop decompression");
			return (ARCHIVE_FATAL);
		}
		lzop->out_block = new_block;
		lzop->out_block_size = lzop->uncompressed_size;
	}

	b = __archive_read_filter_ahead(f->upstream,
		lzop->compressed_size, NULL);
	if (b == NULL) {
		archive_set_error(&f->archive->archive,
		    ARCHIVE_ERRNO_FILE_FORMAT, "Truncated lzop data");
		return (ARCHIVE_FATAL);
	}
	if (lzop->flags & CRC32_COMPRESSED)
		cksum = crc32(crc32(0, NULL, 0), b, lzop->compressed_size);
	else if (lzop->flags & ADLER32_COMPRESSED)
		cksum = adler32(adler32(0, NULL, 0), b, lzop->compressed_size);
	else
		cksum = lzop->compressed_cksum;
	if (cksum != lzop->compressed_cksum) {
		archive_set_error(&f->archive->archive,
		    ARCHIVE_ERRNO_MISC, "Corrupted data");
		return (ARCHIVE_FATAL);
	}

	/*
	 * If both uncompressed size and compressed size are the same,
	 * we do not decompress this block.
	 */
	if (lzop->uncompressed_size == lzop->compressed_size) {
		*p = b;
		lzop->unconsumed_bytes = lzop->compressed_size;
		return ((ssize_t)lzop->uncompressed_size);
	}

	/*
	 * Drive lzo uncompression.
	 */
	out_size = (lzo_uint)lzop->uncompressed_size;
	r = lzo1x_decompress_safe(b, (lzo_uint)lzop->compressed_size,
		lzop->out_block, &out_size, NULL);
	switch (r) {
	case LZO_E_OK:
		if (out_size == lzop->uncompressed_size)
			break;
		archive_set_error(&f->archive->archive,
		    ARCHIVE_ERRNO_MISC, "Corrupted data");
		return (ARCHIVE_FATAL);
	case LZO_E_OUT_OF_MEMORY:
		archive_set_error(&f->archive->archive, ENOMEM,
		    "lzop decompression failed: out of memory");
		return (ARCHIVE_FATAL);
	default:
		archive_set_error(&f->archive->archive, ARCHIVE_ERRNO_MISC,
		    "lzop decompression failed: %d", r);
		return (ARCHIVE_FATAL);
	}

	if (lzop->flags & CRC32_UNCOMPRESSED)
		cksum = crc32(crc32(0, NULL, 0), lzop->out_block,
		    lzop->uncompressed_size);
	else if (lzop->flags & ADLER32_UNCOMPRESSED)
		cksum = adler32(adler32(0, NULL, 0), lzop->out_block,
		    lzop->uncompressed_size);
	else
		cksum = lzop->uncompressed_cksum;
	if (cksum != lzop->uncompressed_cksum) {
		archive_set_error(&f->archive->archive,
		    ARCHIVE_ERRNO_MISC, "Corrupted data");
		return (ARCHIVE_FATAL);
	}

	__archive_read_filter_consume(f->upstream, lzop->compressed_size);
	*p = lzop->out_block;
	return ((ssize_t)out_size);
}

/*
 * Clean up the decompressor.
 */
static int
lzop_filter_close(struct archive_read_filter *f)
{
	struct lzop *lzop = f->data;

	free(lzop->out_block);
	free(lzop);
	return (ARCHIVE_OK);
}

#endif
