/*-
 * Copyright (c) 2003-2010 Tim Kientzle
 * Copyright (c) 2009-2012 Michihiro NAKAJIMA
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <time.h>
#ifdef HAVE_LZMA_H
#include <lzma.h>
#endif

#include "archive.h"
#include "archive_endian.h"
#include "archive_private.h"
#include "archive_write_private.h"

#if ARCHIVE_VERSION_NUMBER < 4000000
int
archive_write_set_compression_lzip(struct archive *a)
{
	__archive_write_filters_free(a);
	return (archive_write_add_filter_lzip(a));
}

int
archive_write_set_compression_lzma(struct archive *a)
{
	__archive_write_filters_free(a);
	return (archive_write_add_filter_lzma(a));
}

int
archive_write_set_compression_xz(struct archive *a)
{
	__archive_write_filters_free(a);
	return (archive_write_add_filter_xz(a));
}

#endif

#ifndef HAVE_LZMA_H
int
archive_write_add_filter_xz(struct archive *a)
{
	archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    "xz compression not supported on this platform");
	return (ARCHIVE_FATAL);
}

int
archive_write_add_filter_lzma(struct archive *a)
{
	archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    "lzma compression not supported on this platform");
	return (ARCHIVE_FATAL);
}

int
archive_write_add_filter_lzip(struct archive *a)
{
	archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    "lzma compression not supported on this platform");
	return (ARCHIVE_FATAL);
}
#else
/* Don't compile this if we don't have liblzma. */

struct xz {
	int		 compression_level;
	uint32_t	 threads;
	lzma_stream	 stream;
	lzma_filter	 lzmafilters[2];
	lzma_options_lzma lzma_opt;
	int64_t		 total_in;
	unsigned char	*compressed;
	size_t		 compressed_buffer_size;
	int64_t		 total_out;
	/* the CRC32 value of uncompressed data for lzip */
	uint32_t	 crc32;
};

static int	archive_compressor_xz_options(struct archive_write_filter *,
		    const char *, const char *);
static int	archive_compressor_xz_open(struct archive_write_filter *);
static int	archive_compressor_xz_write(struct archive_write_filter *,
		    const void *, size_t);
static int	archive_compressor_xz_close(struct archive_write_filter *);
static int	archive_compressor_xz_free(struct archive_write_filter *);
static int	drive_compressor(struct archive_write_filter *,
		    struct xz *, int finishing);
static void	free_data(struct xz *);

struct option_value {
	uint32_t dict_size;
	uint32_t nice_len;
	lzma_match_finder mf;
};
static const struct option_value option_values[] = {
	{ 1 << 16, 32, LZMA_MF_HC3},
	{ 1 << 20, 32, LZMA_MF_HC3},
	{ 3 << 19, 32, LZMA_MF_HC4},
	{ 1 << 21, 32, LZMA_MF_BT4},
	{ 3 << 20, 32, LZMA_MF_BT4},
	{ 1 << 22, 32, LZMA_MF_BT4},
	{ 1 << 23, 64, LZMA_MF_BT4},
	{ 1 << 24, 64, LZMA_MF_BT4},
	{ 3 << 23, 64, LZMA_MF_BT4},
	{ 1 << 25, 64, LZMA_MF_BT4}
};

static int
common_setup(struct archive *a, const char *name, int code)
{
	struct xz *xz;
	struct archive_write_filter *f;

	xz = calloc(1, sizeof(*xz));
	if (xz == NULL)
		goto memerr;
	xz->compression_level = LZMA_PRESET_DEFAULT;
	xz->threads = 1;

	f = __archive_write_allocate_filter(a);
	if (f == NULL)
		goto memerr;
	f->name = name;
	f->code = code;
	f->data = xz;
	f->options = archive_compressor_xz_options;
	f->open = archive_compressor_xz_open;
	f->write = archive_compressor_xz_write;
	f->close = archive_compressor_xz_close;
	f->free = archive_compressor_xz_free;

	return (ARCHIVE_OK);
memerr:
	free_data(xz);
	archive_set_error(a, ENOMEM, "Out of memory");
	return (ARCHIVE_FATAL);
}

/*
 * Add an xz compression filter to this write handle.
 */
int
archive_write_add_filter_xz(struct archive *a)
{
	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_add_filter_xz");

	return common_setup(a, "xz", ARCHIVE_FILTER_XZ);
}

/* LZMA is handled identically, we just need a different compression
 * code set.  (The liblzma setup looks at the code to determine
 * the one place that XZ and LZMA require different handling.) */
int
archive_write_add_filter_lzma(struct archive *a)
{
	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_add_filter_lzma");

	return common_setup(a, "lzma", ARCHIVE_FILTER_LZMA);
}

int
archive_write_add_filter_lzip(struct archive *a)
{
	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_add_filter_lzip");

	return common_setup(a, "lzip", ARCHIVE_FILTER_LZIP);
}

static int
archive_compressor_xz_init_stream(struct archive_write_filter *f,
    struct xz *xz)
{
	static const lzma_stream lzma_stream_init_data = LZMA_STREAM_INIT;
	int ret;
#ifdef HAVE_LZMA_STREAM_ENCODER_MT
	lzma_mt mt_options;
#endif

	xz->stream = lzma_stream_init_data;
	xz->stream.next_out = xz->compressed;
	xz->stream.avail_out = xz->compressed_buffer_size;
	if (f->code == ARCHIVE_FILTER_XZ) {
#ifdef HAVE_LZMA_STREAM_ENCODER_MT
		if (xz->threads != 1) {
			memset(&mt_options, 0, sizeof(mt_options));
			mt_options.threads = xz->threads;
			mt_options.timeout = 300;
			mt_options.filters = xz->lzmafilters;
			mt_options.check = LZMA_CHECK_CRC64;
			ret = lzma_stream_encoder_mt(&(xz->stream),
			    &mt_options);
		} else
#endif
			ret = lzma_stream_encoder(&(xz->stream),
			    xz->lzmafilters, LZMA_CHECK_CRC64);
	} else if (f->code == ARCHIVE_FILTER_LZMA) {
		ret = lzma_alone_encoder(&(xz->stream), &xz->lzma_opt);
	} else {	/* ARCHIVE_FILTER_LZIP */
		int dict_size = xz->lzma_opt.dict_size;
		int ds, log2dic, wedges;

		/* Calculate a coded dictionary size */
		if (dict_size < (1 << 12) || dict_size > (1 << 29)) {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "Unacceptable dictionary size for lzip: %d",
			    dict_size);
			return (ARCHIVE_FATAL);
		}
		for (log2dic = 29; log2dic >= 12; log2dic--) {
			if (dict_size & (1 << log2dic))
				break;
		}
		if (dict_size > (1 << log2dic)) {
			log2dic++;
			wedges =
			    ((1 << log2dic) - dict_size) / (1 << (log2dic - 4));
		} else
			wedges = 0;
		ds = ((wedges << 5) & 0xe0) | (log2dic & 0x1f);

		xz->crc32 = 0;
		/* Make a header */
		xz->compressed[0] = 0x4C;
		xz->compressed[1] = 0x5A;
		xz->compressed[2] = 0x49;
		xz->compressed[3] = 0x50;
		xz->compressed[4] = 1;/* Version */
		xz->compressed[5] = (unsigned char)ds;
		xz->stream.next_out += 6;
		xz->stream.avail_out -= 6;

		ret = lzma_raw_encoder(&(xz->stream), xz->lzmafilters);
	}
	if (ret == LZMA_OK)
		return (ARCHIVE_OK);

	switch (ret) {
	case LZMA_MEM_ERROR:
		archive_set_error(f->archive, ENOMEM,
		    "Internal error initializing compression library: "
		    "Cannot allocate memory");
		break;
	default:
		archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing compression library: "
		    "It's a bug in liblzma");
		break;
	}
	return (ARCHIVE_FATAL);
}

/*
 * Setup callback.
 */
static int
archive_compressor_xz_open(struct archive_write_filter *f)
{
	struct xz *xz = f->data;
	int ret;

	if (xz->compressed == NULL) {
		size_t bs = 65536, bpb;
		if (f->archive->magic == ARCHIVE_WRITE_MAGIC) {
			/* Buffer size should be a multiple number of the bytes
			 * per block for performance. */
			bpb = archive_write_get_bytes_per_block(f->archive);
			if (bpb > bs)
				bs = bpb;
			else if (bpb != 0)
				bs -= bs % bpb;
		}
		xz->compressed_buffer_size = bs;
		xz->compressed = malloc(xz->compressed_buffer_size);
		if (xz->compressed == NULL) {
			archive_set_error(f->archive, ENOMEM,
			    "Can't allocate data for compression buffer");
			return (ARCHIVE_FATAL);
		}
	}

	/* Initialize compression library. */
	if (f->code == ARCHIVE_FILTER_LZIP) {
		const struct option_value *val =
		    &option_values[xz->compression_level];

		xz->lzma_opt.dict_size = val->dict_size;
		xz->lzma_opt.preset_dict = NULL;
		xz->lzma_opt.preset_dict_size = 0;
		xz->lzma_opt.lc = LZMA_LC_DEFAULT;
		xz->lzma_opt.lp = LZMA_LP_DEFAULT;
		xz->lzma_opt.pb = LZMA_PB_DEFAULT;
		xz->lzma_opt.mode =
		    xz->compression_level<= 2? LZMA_MODE_FAST:LZMA_MODE_NORMAL;
		xz->lzma_opt.nice_len = val->nice_len;
		xz->lzma_opt.mf = val->mf;
		xz->lzma_opt.depth = 0;
		xz->lzmafilters[0].id = LZMA_FILTER_LZMA1;
		xz->lzmafilters[0].options = &xz->lzma_opt;
		xz->lzmafilters[1].id = LZMA_VLI_UNKNOWN;/* Terminate */
	} else {
		if (lzma_lzma_preset(&xz->lzma_opt, xz->compression_level)) {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "Internal error initializing compression library");
		}
		xz->lzmafilters[0].id = LZMA_FILTER_LZMA2;
		xz->lzmafilters[0].options = &xz->lzma_opt;
		xz->lzmafilters[1].id = LZMA_VLI_UNKNOWN;/* Terminate */
	}
	ret = archive_compressor_xz_init_stream(f, xz);
	if (ret == LZMA_OK) {
		return (ARCHIVE_OK);
	}
	return (ARCHIVE_FATAL);
}

/*
 * Set write options.
 */
static int
archive_compressor_xz_options(struct archive_write_filter *f,
    const char *key, const char *value)
{
	struct xz *xz = f->data;

	if (strcmp(key, "compression-level") == 0) {
		if (value == NULL || !(value[0] >= '0' && value[0] <= '9') ||
		    value[1] != '\0') {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "compression-level invalid");
			return (ARCHIVE_FAILED);
		}
		xz->compression_level = value[0] - '0';
		if (xz->compression_level > 9)
			xz->compression_level = 9;
		return (ARCHIVE_OK);
	} else if (strcmp(key, "threads") == 0) {
		char *endptr;
		unsigned long val;

		if (value == NULL) {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "threads option requires an argument");
			return (ARCHIVE_FAILED);
		}
		errno = 0;
		val = strtoul(value, &endptr, 10);
		if (errno != 0 || *endptr != '\0' || val > (unsigned)INT_MAX) {
			xz->threads = 1;
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "threads invalid");
			return (ARCHIVE_FAILED);
		}
		xz->threads = (int)val;
		if (xz->threads == 0) {
#ifdef HAVE_LZMA_STREAM_ENCODER_MT
			xz->threads = lzma_cputhreads();
#else
			xz->threads = 1;
#endif
		}
		return (ARCHIVE_OK);
	}

	/* Note: The "warn" return is just to inform the options
	 * supervisor that we didn't handle it.  It will generate
	 * a suitable error if no one used this option. */
	return (ARCHIVE_WARN);
}

/*
 * Write data to the compressed stream.
 */
static int
archive_compressor_xz_write(struct archive_write_filter *f,
    const void *buff, size_t length)
{
	struct xz *xz = f->data;
	int ret;

	/* Update statistics */
	xz->total_in += length;
	if (f->code == ARCHIVE_FILTER_LZIP)
		xz->crc32 = lzma_crc32(buff, length, xz->crc32);

	/* Compress input data to output buffer */
	xz->stream.next_in = buff;
	xz->stream.avail_in = length;
	if ((ret = drive_compressor(f, xz, 0)) != ARCHIVE_OK)
		return (ret);

	return (ARCHIVE_OK);
}


/*
 * Finish the compression...
 */
static int
archive_compressor_xz_close(struct archive_write_filter *f)
{
	struct xz *xz = f->data;
	int ret;

	ret = drive_compressor(f, xz, 1);
	if (ret == ARCHIVE_OK) {
		xz->total_out +=
		    xz->compressed_buffer_size - xz->stream.avail_out;
		ret = __archive_write_filter(f->next_filter,
		    xz->compressed,
		    xz->compressed_buffer_size - xz->stream.avail_out);
		if (f->code == ARCHIVE_FILTER_LZIP && ret == ARCHIVE_OK) {
			archive_le32enc(xz->compressed, xz->crc32);
			archive_le64enc(xz->compressed+4, xz->total_in);
			archive_le64enc(xz->compressed+12, xz->total_out + 20);
			ret = __archive_write_filter(f->next_filter,
			    xz->compressed, 20);
		}
	}
	lzma_end(&(xz->stream));
	return ret;
}

static int
archive_compressor_xz_free(struct archive_write_filter *f)
{
	free_data(f->data);
	f->data = NULL;
	return (ARCHIVE_OK);
}

/*
 * Utility function to push input data through compressor,
 * writing full output blocks as necessary.
 *
 * Note that this handles both the regular write case (finishing ==
 * false) and the end-of-archive case (finishing == true).
 */
static int
drive_compressor(struct archive_write_filter *f,
    struct xz *xz, int finishing)
{
	int ret;

	for (;;) {
		if (xz->stream.avail_out == 0) {
			xz->total_out += xz->compressed_buffer_size;
			ret = __archive_write_filter(f->next_filter,
			    xz->compressed,
			    xz->compressed_buffer_size);
			if (ret != ARCHIVE_OK)
				return (ARCHIVE_FATAL);
			xz->stream.next_out = xz->compressed;
			xz->stream.avail_out = xz->compressed_buffer_size;
		}

		/* If there's nothing to do, we're done. */
		if (!finishing && xz->stream.avail_in == 0)
			return (ARCHIVE_OK);

		ret = lzma_code(&(xz->stream),
		    finishing ? LZMA_FINISH : LZMA_RUN );

		switch (ret) {
		case LZMA_OK:
			/* In non-finishing case, check if compressor
			 * consumed everything */
			if (!finishing && xz->stream.avail_in == 0)
				return (ARCHIVE_OK);
			/* In finishing case, this return always means
			 * there's more work */
			break;
		case LZMA_STREAM_END:
			/* This return can only occur in finishing case. */
			if (finishing)
				return (ARCHIVE_OK);
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "lzma compression data error");
			return (ARCHIVE_FATAL);
		case LZMA_MEMLIMIT_ERROR:
			archive_set_error(f->archive, ENOMEM,
			    "lzma compression error: "
			    "%ju MiB would have been needed",
			    (uintmax_t)((lzma_memusage(&(xz->stream))
				    + 1024 * 1024 -1)
				/ (1024 * 1024)));
			return (ARCHIVE_FATAL);
		default:
			/* Any other return value indicates an error. */
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "lzma compression failed:"
			    " lzma_code() call returned status %d",
			    ret);
			return (ARCHIVE_FATAL);
		}
	}
}

static void
free_data(struct xz *xz)
{
	if (xz != NULL) {
		free(xz->compressed);
		free(xz);
	}
}

#endif /* HAVE_LZMA_H */
