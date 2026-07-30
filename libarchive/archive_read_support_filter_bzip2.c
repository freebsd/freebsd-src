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
#ifdef HAVE_LIMITS_H
#include <limits.h>
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

#if defined(HAVE_BZLIB_H) && defined(BZ_CONFIG_ERROR)
struct bzip2 {
	bz_stream	 stream;
	char		*out_block;
	size_t		 out_block_size;
	char		 valid; /* True = decompressor is initialized */
	char		 eof; /* True = found end of compressed data. */
};

/* Bzip2 filter */
static ssize_t	bzip2_filter_read(struct archive_read_filter *, const void **);
static int	bzip2_filter_close(struct archive_read_filter *);
#endif

/*
 * Note that we can detect bzip2 archives even if we can't decompress
 * them.  (In fact, we like detecting them because we can give better
 * error messages.)
 */
static int	bzip2_reader_bid(struct archive_read_filter_bidder *, struct archive_read_filter *);
static int	bzip2_reader_init(struct archive_read_filter *);

#if ARCHIVE_VERSION_NUMBER < 4000000
/* Deprecated; remove in libarchive 4.0 */
int
archive_read_support_compression_bzip2(struct archive *a)
{
	return archive_read_support_filter_bzip2(a);
}
#endif

static const struct archive_read_filter_bidder_vtable
bzip2_bidder_vtable = {
	.bid = bzip2_reader_bid,
	.init = bzip2_reader_init,
};

int
archive_read_support_filter_bzip2(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;

	if (__archive_read_register_bidder(a, NULL, "bzip2",
				&bzip2_bidder_vtable) != ARCHIVE_OK)
		return (ARCHIVE_FATAL);

#if defined(HAVE_BZLIB_H) && defined(BZ_CONFIG_ERROR)
	return (ARCHIVE_OK);
#else
	archive_set_error(_a, ARCHIVE_ERRNO_MISC,
	    "Using external bzip2 program");
	return (ARCHIVE_WARN);
#endif
}

/*
 * Test whether we can handle this data.
 *
 * This logic returns zero if any part of the signature fails.  It
 * also tries to Do The Right Thing if a very short buffer prevents us
 * from verifying as much as we would like.
 */
static int
bzip2_reader_bid(struct archive_read_filter_bidder *b, struct archive_read_filter *f)
{
	const unsigned char *buffer;
	int bits_checked;

	(void)b; /* UNUSED */

	/* Minimal bzip2 archive is 14 bytes. */
	buffer = __archive_read_filter_ahead(f, 14, NULL);
	if (buffer == NULL)
		return (0);

	/* First three bytes must be "BZh" */
	bits_checked = 0;
	if (memcmp(buffer, "BZh", 3) != 0)
		return (0);
	bits_checked += 24;

	/* Next follows a compression flag which must be an ASCII digit. */
	if (buffer[3] < '1' || buffer[3] > '9')
		return (0);
	bits_checked += 5;

	/* After BZh[1-9], there must be either a data block
	 * which begins with 0x314159265359 or an end-of-data
	 * marker of 0x177245385090. */
	if (memcmp(buffer + 4, "\x31\x41\x59\x26\x53\x59", 6) == 0)
		bits_checked += 48;
	else if (memcmp(buffer + 4, "\x17\x72\x45\x38\x50\x90", 6) == 0)
		bits_checked += 48;
	else
		return (0);

	return (bits_checked);
}

#if !defined(HAVE_BZLIB_H) || !defined(BZ_CONFIG_ERROR)

/*
 * If we don't have the library on this system, we can't do the
 * decompression directly.  We can, however, try to run "bzip2 -d"
 * in case that's available.
 */
static int
bzip2_reader_init(struct archive_read_filter *f)
{
	int r;

	r = __archive_read_program(f, "bzip2 -d");
	/* Note: We set the format here even if __archive_read_program()
	 * above fails.  We do, after all, know what the format is
	 * even if we weren't able to read it. */
	f->code = ARCHIVE_FILTER_BZIP2;
	f->name = "bzip2";
	return (r);
}


#else

static const struct archive_read_filter_vtable
bzip2_reader_vtable = {
	.read = bzip2_filter_read,
	.close = bzip2_filter_close,
};

/*
 * Setup the callbacks.
 */
static int
bzip2_reader_init(struct archive_read_filter *f)
{
	static const size_t out_block_size = 64 * 1024;
	void *out_block;
	struct bzip2 *bzip2;

	f->code = ARCHIVE_FILTER_BZIP2;
	f->name = "bzip2";

	bzip2 = calloc(1, sizeof(*bzip2));
	out_block = malloc(out_block_size);
	if (bzip2 == NULL || out_block == NULL) {
		archive_set_error(&f->archive->archive, ENOMEM,
		    "Can't allocate data for bzip2 decompression");
		free(out_block);
		free(bzip2);
		return (ARCHIVE_FATAL);
	}

	f->data = bzip2;
	bzip2->out_block_size = out_block_size;
	bzip2->out_block = out_block;
	f->vtable = &bzip2_reader_vtable;

	return (ARCHIVE_OK);
}

/*
 * Return the next block of decompressed data.
 */
static ssize_t
bzip2_filter_read(struct archive_read_filter *f, const void **p)
{
	struct bzip2 *bzip2 = f->data;
	size_t decompressed;
	const char *read_buf;
	ssize_t ret;

	if (bzip2->eof) {
		*p = NULL;
		return (0);
	}

	/* Empty our output buffer. */
	bzip2->stream.next_out = bzip2->out_block;
	bzip2->stream.avail_out = (uint32_t)bzip2->out_block_size;

	/* Try to fill the output buffer. */
	for (;;) {
		ssize_t max_in;

		if (!bzip2->valid) {
			if (bzip2_reader_bid(f->bidder, f->upstream) == 0) {
				bzip2->eof = 1;
				*p = bzip2->out_block;
				decompressed = bzip2->stream.next_out
				    - bzip2->out_block;
				return (decompressed);
			}
			/* Initialize compression library. */
			ret = BZ2_bzDecompressInit(&(bzip2->stream),
					   0 /* library verbosity */,
					   0 /* don't use low-mem algorithm */);

			/* If init fails, try low-memory algorithm instead. */
			if (ret == BZ_MEM_ERROR)
				ret = BZ2_bzDecompressInit(&(bzip2->stream),
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
				archive_set_error(&f->archive->archive, err,
				    "Internal error initializing decompressor%s%s",
				    detail == NULL ? "" : ": ",
				    detail);
				return (ARCHIVE_FATAL);
			}
			bzip2->valid = 1;
		}

		/* stream.next_in is really const, but bzlib
		 * doesn't declare it so. <sigh> */
		read_buf =
		    __archive_read_filter_ahead(f->upstream, 1, &ret);
		if (read_buf == NULL) {
			archive_set_error(&f->archive->archive,
			    ARCHIVE_ERRNO_MISC,
			    "truncated bzip2 input");
			return (ARCHIVE_FATAL);
		}
		bzip2->stream.next_in = (char *)(uintptr_t)read_buf;
		if (UINT_MAX >= SSIZE_MAX)
			max_in = SSIZE_MAX;
		else
			max_in = UINT_MAX;
		if (ret > max_in)
			ret = max_in;
		bzip2->stream.avail_in = (uint32_t)ret;

		/* Decompress as much as we can in one pass. */
		ret = BZ2_bzDecompress(&(bzip2->stream));
		__archive_read_filter_consume(f->upstream,
		    bzip2->stream.next_in - read_buf);

		switch (ret) {
		case BZ_STREAM_END: /* Found end of stream. */
			switch (BZ2_bzDecompressEnd(&(bzip2->stream))) {
			case BZ_OK:
				break;
			default:
				archive_set_error(&(f->archive->archive),
					  ARCHIVE_ERRNO_MISC,
					  "Failed to clean up decompressor");
				return (ARCHIVE_FATAL);
			}
			bzip2->valid = 0;
			/* FALLTHROUGH */
		case BZ_OK: /* Decompressor made some progress. */
			/* If we filled our buffer, update stats and return. */
			if (bzip2->stream.avail_out == 0) {
				*p = bzip2->out_block;
				decompressed = bzip2->stream.next_out
				    - bzip2->out_block;
				return (decompressed);
			}
			break;
		default: /* Return an error. */
			archive_set_error(&f->archive->archive,
			    ARCHIVE_ERRNO_MISC, "bzip decompression failed");
			return (ARCHIVE_FATAL);
		}
	}
}

/*
 * Clean up the decompressor.
 */
static int
bzip2_filter_close(struct archive_read_filter *f)
{
	struct bzip2 *bzip2 = f->data;
	int ret = ARCHIVE_OK;

	if (bzip2->valid) {
		switch (BZ2_bzDecompressEnd(&bzip2->stream)) {
		case BZ_OK:
			break;
		default:
			archive_set_error(&f->archive->archive,
					  ARCHIVE_ERRNO_MISC,
					  "Failed to clean up decompressor");
			ret = ARCHIVE_FATAL;
		}
		bzip2->valid = 0;
	}

	free(bzip2->out_block);
	free(bzip2);
	return (ret);
}

#endif /* HAVE_BZLIB_H && BZ_CONFIG_ERROR */
