/*-
 * Copyright (c) 2009-2011 Sean Purcell
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
#if HAVE_ZSTD_H
#include <zstd.h>
#endif

#include "archive.h"
#include "archive_endian.h"
#include "archive_integer.h"
#include "archive_private.h"
#include "archive_read_private.h"

#if HAVE_ZSTD_H && HAVE_LIBZSTD

struct zstd {
	ZSTD_DStream	*dstream;
	unsigned char	*out_block;
	size_t		 out_block_size;
	char		 in_frame; /* True = in the middle of a zstd frame. */
	char		 eof; /* True = found end of compressed data. */
};

/* Zstd Filter. */
static ssize_t	zstd_filter_read(struct archive_read_filter *, const void**);
static int	zstd_filter_close(struct archive_read_filter *);
#endif

/*
 * Note that we can detect zstd compressed files even if we can't decompress
 * them.  (In fact, we like detecting them because we can give better error
 * messages.)
 */
static int	zstd_bidder_bid(struct archive_read_filter_bidder *,
		    struct archive_read_filter *);
static int	zstd_bidder_init(struct archive_read_filter *);

static const struct archive_read_filter_bidder_vtable
zstd_bidder_vtable = {
	.bid = zstd_bidder_bid,
	.init = zstd_bidder_init,
};

int
archive_read_support_filter_zstd(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;

	if (__archive_read_register_bidder(a, NULL, "zstd",
				&zstd_bidder_vtable) != ARCHIVE_OK)
		return (ARCHIVE_FATAL);

#if HAVE_ZSTD_H && HAVE_LIBZSTD
	return (ARCHIVE_OK);
#else
	archive_set_error(_a, ARCHIVE_ERRNO_MISC,
	    "Using external zstd program for zstd decompression");
	return (ARCHIVE_WARN);
#endif
}

/*
 * Test whether we can handle this data.
 */
static int
zstd_bidder_bid(struct archive_read_filter_bidder *b,
    struct archive_read_filter *f)
{
	const unsigned char *buffer;
	ssize_t avail;
	/*
	 * Zstandard skippable frames contain a 4 byte magic number followed
	 * by a 4 byte frame data size, then that number of bytes of data.
	 * Regular frames contain a 4 byte magic number followed by a 2-14
	 * byte frame header, some data, and a 3 byte end marker.
	 */
	const size_t min_zstd_frame_size = 8;

	size_t offset_in_buffer = 0;
	const size_t max_lookahead = 64 * 1024;
	uint32_t magic_number;

	/* Zstd regular frame magic number. */
	const uint32_t zstd_magic = 0xFD2FB528U;

	/*
	 * Note: Zstd and LZ4 skippable frame magic numbers are identical.
	 * To differentiate these two, we need to look for a non-skippable
	 * frame.
	 */
	const uint32_t zstd_magic_skippable_start = 0x184D2A50;
	const uint32_t zstd_magic_skippable_mask  = 0xFFFFFFF0;

	(void) b; /* UNUSED */

	buffer = __archive_read_filter_ahead(f, min_zstd_frame_size,
	    &avail);
	if (buffer == NULL)
		return (0);

	magic_number = archive_le32dec(buffer);

	while ((magic_number & zstd_magic_skippable_mask) ==
	    zstd_magic_skippable_start) {
		size_t min;
		uint32_t frame_data_size;

		/* Skip over the magic number */
		offset_in_buffer += 4;

		/* Ensure that we can read another 4 bytes. */
		if (offset_in_buffer + 4 > (size_t)avail) {
			buffer = __archive_read_filter_ahead(f,
			    offset_in_buffer + 4, &avail);
			if (buffer == NULL)
				return (0);
		}

		frame_data_size = archive_le32dec(buffer + offset_in_buffer);

		/* Skip over the 4 frame data size bytes */
		offset_in_buffer += 4;

		/* Skip over the value stored there. */
		if (archive_ckd_add_size(&offset_in_buffer,
		    offset_in_buffer, frame_data_size))
			return (0);

		/*
		 * There should be at least one more frame
		 * if this is zstd data.
		 */
		if (archive_ckd_add_size(&min,
		    offset_in_buffer, min_zstd_frame_size))
			return (0);
		if (min > (size_t)avail) {
			if (min > max_lookahead)
				return (0);

			buffer = __archive_read_filter_ahead(f,
			    min, &avail);
			if (buffer == NULL)
				return (0);
		}

		magic_number = archive_le32dec(buffer + offset_in_buffer);
	}

	/*
	 * We have skipped over any skippable frames. Either a regular zstd
	 * frame follows, or this isn't zstd data.
	 */

	if (magic_number == zstd_magic)
		return (offset_in_buffer + 4);

	return (0);
}

#if !(HAVE_ZSTD_H && HAVE_LIBZSTD)

/*
 * If we don't have the library on this system, we can't do the
 * decompression directly.  We can, however, try to run "zstd -d -qq"
 * in case that's available.
 */
static int
zstd_bidder_init(struct archive_read_filter *f)
{
	int r;

	r = __archive_read_program(f, "zstd -d -qq");
	/* Note: We set the format here even if __archive_read_program()
	 * above fails.  We do, after all, know what the format is
	 * even if we weren't able to read it. */
	f->code = ARCHIVE_FILTER_ZSTD;
	f->name = "zstd";
	return (r);
}

#else

static const struct archive_read_filter_vtable
zstd_reader_vtable = {
	.read = zstd_filter_read,
	.close = zstd_filter_close,
};

/*
 * Initialize the filter object
 */
static int
zstd_bidder_init(struct archive_read_filter *f)
{
	struct zstd *zstd;
	size_t out_block_size = ZSTD_DStreamOutSize();
	void *out_block;
	ZSTD_DStream *dstream;

	f->code = ARCHIVE_FILTER_ZSTD;
	f->name = "zstd";

	zstd = calloc(1, sizeof(*zstd));
	out_block = malloc(out_block_size);
	dstream = ZSTD_createDStream();

	if (zstd == NULL || out_block == NULL || dstream == NULL) {
		free(out_block);
		free(zstd);
		ZSTD_freeDStream(dstream); /* supports free on NULL */
		archive_set_error(&f->archive->archive, ENOMEM,
		    "Can't allocate data for zstd decompression");
		return (ARCHIVE_FATAL);
	}

	f->data = zstd;

	zstd->out_block_size = out_block_size;
	zstd->out_block = out_block;
	zstd->dstream = dstream;
	f->vtable = &zstd_reader_vtable;

	zstd->eof = 0;
	zstd->in_frame = 0;

	return (ARCHIVE_OK);
}

static ssize_t
zstd_filter_read(struct archive_read_filter *f, const void **p)
{
	struct zstd *zstd = f->data;
	size_t decompressed;
	ssize_t avail_in;
	ZSTD_outBuffer out;
	ZSTD_inBuffer in;
	size_t ret;

	out = (ZSTD_outBuffer) { zstd->out_block, zstd->out_block_size, 0 };

	/* Try to fill the output buffer. */
	while (out.pos < out.size && !zstd->eof) {
		if (!zstd->in_frame) {
			ret = ZSTD_initDStream(zstd->dstream);
			if (ZSTD_isError(ret)) {
				archive_set_error(&f->archive->archive,
				    ARCHIVE_ERRNO_MISC,
				    "Error initializing zstd decompressor: %s",
				    ZSTD_getErrorName(ret));
				return (ARCHIVE_FATAL);
			}
		}
		in.src = __archive_read_filter_ahead(f->upstream, 1,
		    &avail_in);
		if (avail_in < 0) {
			return avail_in;
		}
		if (in.src == NULL && avail_in == 0) {
			if (!zstd->in_frame) {
				/* end of stream */
				zstd->eof = 1;
				break;
			} else {
				archive_set_error(&f->archive->archive,
				    ARCHIVE_ERRNO_MISC,
				    "Truncated zstd input");
				return (ARCHIVE_FATAL);
			}
		}
		in.size = avail_in;
		in.pos = 0;

		{
			ret = ZSTD_decompressStream(zstd->dstream, &out, &in);

			if (ZSTD_isError(ret)) {
				archive_set_error(&f->archive->archive,
				    ARCHIVE_ERRNO_MISC,
				    "Zstd decompression failed: %s",
				    ZSTD_getErrorName(ret));
				return (ARCHIVE_FATAL);
			}

			/* Decompressor made some progress */
			__archive_read_filter_consume(f->upstream, in.pos);

			/* ret guaranteed to be > 0 if frame isn't done yet */
			zstd->in_frame = (ret != 0);
		}
	}

	decompressed = out.pos;
	if (decompressed == 0)
		*p = NULL;
	else
		*p = zstd->out_block;
	return (decompressed);
}

/*
 * Clean up the decompressor.
 */
static int
zstd_filter_close(struct archive_read_filter *f)
{
	struct zstd *zstd = f->data;

	ZSTD_freeDStream(zstd->dstream);
	free(zstd->out_block);
	free(zstd);

	return (ARCHIVE_OK);
}

#endif /* HAVE_ZLIB_H && HAVE_LIBZSTD */
