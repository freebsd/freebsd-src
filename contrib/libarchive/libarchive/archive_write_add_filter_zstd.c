/*-
 * Copyright (c) 2017 Sean Purcell
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
#ifdef HAVE_ZSTD_H
#include <zstd.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_string.h"
#include "archive_write_private.h"

/* Don't compile this if we don't have zstd.h */

struct private_data {
	int		 compression_level;
#if HAVE_ZSTD_H && HAVE_LIBZSTD
	ZSTD_CStream	*cstream;
	int64_t		 total_in;
	ZSTD_outBuffer	 out;
#else
	struct archive_write_program_data *pdata;
#endif
};

/* If we don't have the library use default range values (zstdcli.c v1.4.0) */
#define CLEVEL_MIN -99
#define CLEVEL_STD_MIN 0 /* prior to 1.3.4 and more recent without using --fast */
#define CLEVEL_DEFAULT 3
#define CLEVEL_STD_MAX 19 /* without using --ultra */
#define CLEVEL_MAX 22

#define MINVER_NEGCLEVEL 10304
#define MINVER_MINCLEVEL 10306

static int archive_compressor_zstd_options(struct archive_write_filter *,
		    const char *, const char *);
static int archive_compressor_zstd_open(struct archive_write_filter *);
static int archive_compressor_zstd_write(struct archive_write_filter *,
		    const void *, size_t);
static int archive_compressor_zstd_close(struct archive_write_filter *);
static int archive_compressor_zstd_free(struct archive_write_filter *);
#if HAVE_ZSTD_H && HAVE_LIBZSTD
static int drive_compressor(struct archive_write_filter *,
		    struct private_data *, int, const void *, size_t);
#endif


/*
 * Add a zstd compression filter to this write handle.
 */
int
archive_write_add_filter_zstd(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct archive_write_filter *f = __archive_write_allocate_filter(_a);
	struct private_data *data;
	archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_add_filter_zstd");

	data = calloc(1, sizeof(*data));
	if (data == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Out of memory");
		return (ARCHIVE_FATAL);
	}
	f->data = data;
	f->open = &archive_compressor_zstd_open;
	f->options = &archive_compressor_zstd_options;
	f->close = &archive_compressor_zstd_close;
	f->free = &archive_compressor_zstd_free;
	f->code = ARCHIVE_FILTER_ZSTD;
	f->name = "zstd";
	data->compression_level = CLEVEL_DEFAULT;
#if HAVE_ZSTD_H && HAVE_LIBZSTD
	data->cstream = ZSTD_createCStream();
	if (data->cstream == NULL) {
		free(data);
		archive_set_error(&a->archive, ENOMEM,
		    "Failed to allocate zstd compressor object");
		return (ARCHIVE_FATAL);
	}

	return (ARCHIVE_OK);
#else
	data->pdata = __archive_write_program_allocate("zstd");
	if (data->pdata == NULL) {
		free(data);
		archive_set_error(&a->archive, ENOMEM, "Out of memory");
		return (ARCHIVE_FATAL);
	}
	archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
	    "Using external zstd program");
	return (ARCHIVE_WARN);
#endif
}

static int
archive_compressor_zstd_free(struct archive_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;
#if HAVE_ZSTD_H && HAVE_LIBZSTD
	ZSTD_freeCStream(data->cstream);
	free(data->out.dst);
#else
	__archive_write_program_free(data->pdata);
#endif
	free(data);
	f->data = NULL;
	return (ARCHIVE_OK);
}

static int string_is_numeric (const char* value)
{
       size_t len = strlen(value);
       size_t i;

       if (len == 0) {
               return (ARCHIVE_WARN);
       }
       else if (len == 1 && !(value[0] >= '0' && value[0] <= '9')) {
               return (ARCHIVE_WARN);
       }
       else if (!(value[0] >= '0' && value[0] <= '9') &&
                value[0] != '-' && value[0] != '+') {
               return (ARCHIVE_WARN);
       }

       for (i = 1; i < len; i++) {
               if (!(value[i] >= '0' && value[i] <= '9')) {
                       return (ARCHIVE_WARN);
               }
       }

       return (ARCHIVE_OK);
}

/*
 * Set write options.
 */
static int
archive_compressor_zstd_options(struct archive_write_filter *f, const char *key,
    const char *value)
{
	struct private_data *data = (struct private_data *)f->data;

	if (strcmp(key, "compression-level") == 0) {
		int level = atoi(value);
		/* If we don't have the library, hard-code the max level */
		int minimum = CLEVEL_MIN;
		int maximum = CLEVEL_MAX;
		if (string_is_numeric(value) != ARCHIVE_OK) {
			return (ARCHIVE_WARN);
		}
#if HAVE_ZSTD_H && HAVE_LIBZSTD
		maximum = ZSTD_maxCLevel();
#if ZSTD_VERSION_NUMBER >= MINVER_MINCLEVEL
		if (ZSTD_versionNumber() >= MINVER_MINCLEVEL) {
			minimum = ZSTD_minCLevel();
		}
		else
#endif
		if (ZSTD_versionNumber() < MINVER_NEGCLEVEL) {
			minimum = CLEVEL_STD_MIN;
		}
#endif
		if (level < minimum || level > maximum) {
			return (ARCHIVE_WARN);
		}
		data->compression_level = level;
		return (ARCHIVE_OK);
	}

	/* Note: The "warn" return is just to inform the options
	 * supervisor that we didn't handle it.  It will generate
	 * a suitable error if no one used this option. */
	return (ARCHIVE_WARN);
}

#if HAVE_ZSTD_H && HAVE_LIBZSTD
/*
 * Setup callback.
 */
static int
archive_compressor_zstd_open(struct archive_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;

	if (data->out.dst == NULL) {
		size_t bs = ZSTD_CStreamOutSize(), bpb;
		if (f->archive->magic == ARCHIVE_WRITE_MAGIC) {
			/* Buffer size should be a multiple number of
			 * the of bytes per block for performance. */
			bpb = archive_write_get_bytes_per_block(f->archive);
			if (bpb > bs)
				bs = bpb;
			else if (bpb != 0)
				bs -= bs % bpb;
		}
		data->out.size = bs;
		data->out.pos = 0;
		data->out.dst
		    = (unsigned char *)malloc(data->out.size);
		if (data->out.dst == NULL) {
			archive_set_error(f->archive, ENOMEM,
			    "Can't allocate data for compression buffer");
			return (ARCHIVE_FATAL);
		}
	}

	f->write = archive_compressor_zstd_write;

	if (ZSTD_isError(ZSTD_initCStream(data->cstream,
	    data->compression_level))) {
		archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing zstd compressor object");
		return (ARCHIVE_FATAL);
	}

	return (ARCHIVE_OK);
}

/*
 * Write data to the compressed stream.
 */
static int
archive_compressor_zstd_write(struct archive_write_filter *f, const void *buff,
    size_t length)
{
	struct private_data *data = (struct private_data *)f->data;
	int ret;

	/* Update statistics */
	data->total_in += length;

	if ((ret = drive_compressor(f, data, 0, buff, length)) != ARCHIVE_OK)
		return (ret);

	return (ARCHIVE_OK);
}

/*
 * Finish the compression...
 */
static int
archive_compressor_zstd_close(struct archive_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;

	/* Finish zstd frame */
	return drive_compressor(f, data, 1, NULL, 0);
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
    struct private_data *data, int finishing, const void *src, size_t length)
{
	ZSTD_inBuffer in = (ZSTD_inBuffer) { src, length, 0 };

	for (;;) {
		if (data->out.pos == data->out.size) {
			const int ret = __archive_write_filter(f->next_filter,
			    data->out.dst, data->out.size);
			if (ret != ARCHIVE_OK)
				return (ARCHIVE_FATAL);
			data->out.pos = 0;
		}

		/* If there's nothing to do, we're done. */
		if (!finishing && in.pos == in.size)
			return (ARCHIVE_OK);

		{
			const size_t zstdret = !finishing ?
			    ZSTD_compressStream(data->cstream, &data->out, &in)
			    : ZSTD_endStream(data->cstream, &data->out);

			if (ZSTD_isError(zstdret)) {
				archive_set_error(f->archive,
				    ARCHIVE_ERRNO_MISC,
				    "Zstd compression failed: %s",
				    ZSTD_getErrorName(zstdret));
				return (ARCHIVE_FATAL);
			}

			/* If we're finishing, 0 means nothing left to flush */
			if (finishing && zstdret == 0) {
				const int ret = __archive_write_filter(f->next_filter,
				    data->out.dst, data->out.pos);
				return (ret);
			}
		}
	}
}

#else /* HAVE_ZSTD_H && HAVE_LIBZSTD */

static int
archive_compressor_zstd_open(struct archive_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;
	struct archive_string as;
	int r;

	archive_string_init(&as);
	/* --no-check matches library default */
	archive_strcpy(&as, "zstd --no-check");

	if (data->compression_level < CLEVEL_STD_MIN) {
		struct archive_string as2;
		archive_string_init(&as2);
		archive_string_sprintf(&as2, " --fast=%d", -data->compression_level);
		archive_string_concat(&as, &as2);
		archive_string_free(&as2);
	} else {
		struct archive_string as2;
		archive_string_init(&as2);
		archive_string_sprintf(&as2, " -%d", data->compression_level);
		archive_string_concat(&as, &as2);
		archive_string_free(&as2);
	}

	if (data->compression_level > CLEVEL_STD_MAX) {
		archive_strcat(&as, " --ultra");
	}

	f->write = archive_compressor_zstd_write;
	r = __archive_write_program_open(f, data->pdata, as.s);
	archive_string_free(&as);
	return (r);
}

static int
archive_compressor_zstd_write(struct archive_write_filter *f, const void *buff,
    size_t length)
{
	struct private_data *data = (struct private_data *)f->data;

	return __archive_write_program_write(f, data->pdata, buff, length);
}

static int
archive_compressor_zstd_close(struct archive_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;

	return __archive_write_program_close(f, data->pdata);
}

#endif /* HAVE_ZSTD_H && HAVE_LIBZSTD */
