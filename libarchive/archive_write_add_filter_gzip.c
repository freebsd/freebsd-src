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
#include <time.h>
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#include "archive.h"
#include "archive_endian.h"
#include "archive_private.h"
#include "archive_string.h"
#include "archive_write_private.h"

#if ARCHIVE_VERSION_NUMBER < 4000000
int
archive_write_set_compression_gzip(struct archive *a)
{
	__archive_write_filters_free(a);
	return (archive_write_add_filter_gzip(a));
}
#endif

/* Don't compile this if we don't have zlib. */

struct gzip {
	int		 compression_level;
	int		 timestamp;
	char	*original_filename;
#ifdef HAVE_ZLIB_H
	z_stream	 stream;
	uint64_t	 total_in;
	unsigned char	*compressed;
	size_t		 compressed_buffer_size;
	unsigned long	 crc;
#else
	struct archive_write_program_data *pdata;
#endif
};

/*
 * Yuck.  zlib.h is not const-correct, so I need this one bit
 * of ugly hackery to convert a const * pointer to a non-const pointer.
 */
#define	SET_NEXT_IN(st,src)					\
	(st)->stream.next_in = (Bytef *)(uintptr_t)(const void *)(src)

static int archive_compressor_gzip_options(struct archive_write_filter *,
		    const char *, const char *);
static int archive_compressor_gzip_open(struct archive_write_filter *);
static int archive_compressor_gzip_write(struct archive_write_filter *,
		    const void *, size_t);
static int archive_compressor_gzip_close(struct archive_write_filter *);
static int archive_compressor_gzip_free(struct archive_write_filter *);
#ifdef HAVE_ZLIB_H
static int drive_compressor(struct archive_write_filter *,
		    struct gzip *, int finishing);
#endif
static void free_data(struct gzip *);


/*
 * Add a gzip compression filter to this write handle.
 */
int
archive_write_add_filter_gzip(struct archive *a)
{
	struct archive_write_filter *f;
	struct gzip *gzip;
	int r;

	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_add_filter_gzip");

	gzip = calloc(1, sizeof(*gzip));
	if (gzip == NULL)
		goto memerr;
	gzip->original_filename = NULL;
#ifdef HAVE_ZLIB_H
	gzip->compression_level = Z_DEFAULT_COMPRESSION;

	r = ARCHIVE_OK;
#else
	gzip->pdata = __archive_write_program_allocate("gzip");
	if (gzip->pdata == NULL)
		goto memerr;
	gzip->compression_level = 0;

	archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    "Using external gzip program");
	r = ARCHIVE_WARN;
#endif

	f = __archive_write_allocate_filter(a);
	if (f == NULL)
		goto memerr;
	f->name = "gzip";
	f->code = ARCHIVE_FILTER_GZIP;
	f->data = gzip;
	f->options = archive_compressor_gzip_options;
	f->open = archive_compressor_gzip_open;
	f->write = archive_compressor_gzip_write;
	f->close = archive_compressor_gzip_close;
	f->free = archive_compressor_gzip_free;

	return (r);
memerr:
	free_data(gzip);
	archive_set_error(a, ENOMEM, "Out of memory");
	return (ARCHIVE_FATAL);
}

static int
archive_compressor_gzip_free(struct archive_write_filter *f)
{
	free_data(f->data);
	f->data = NULL;
	return (ARCHIVE_OK);
}

/*
 * Set write options.
 */
static int
archive_compressor_gzip_options(struct archive_write_filter *f, const char *key,
    const char *value)
{
	struct gzip *gzip = f->data;

	if (strcmp(key, "compression-level") == 0) {
		if (value == NULL || !(value[0] >= '0' && value[0] <= '9') ||
		    value[1] != '\0') {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "compression-level invalid");
			return (ARCHIVE_FAILED);
		}
		gzip->compression_level = value[0] - '0';
		return (ARCHIVE_OK);
	}
	if (strcmp(key, "timestamp") == 0) {
		gzip->timestamp = (value == NULL)?-1:1;
		return (ARCHIVE_OK);
	}
	if (strcmp(key, "original-filename") == 0) {
		free((void*)gzip->original_filename);
		gzip->original_filename = NULL;
		if (value) {
			gzip->original_filename = strdup(value);
			if (gzip->original_filename == NULL)
				return (ARCHIVE_FAILED);
		}
		return (ARCHIVE_OK);
	}

	/* Note: The "warn" return is just to inform the options
	 * supervisor that we didn't handle it.  It will generate
	 * a suitable error if no one used this option. */
	return (ARCHIVE_WARN);
}

#ifdef HAVE_ZLIB_H
/*
 * Setup callback.
 */
static int
archive_compressor_gzip_open(struct archive_write_filter *f)
{
	struct gzip *gzip = f->data;
	int ret = ARCHIVE_OK;
	int init_success;

	if (gzip->compressed == NULL) {
		size_t bs = 65536, bpb;
		if (f->archive->magic == ARCHIVE_WRITE_MAGIC) {
			/* Buffer size should be a multiple number of
			 * the of bytes per block for performance. */
			bpb = archive_write_get_bytes_per_block(f->archive);
			if (bpb > bs)
				bs = bpb;
			else if (bpb != 0)
				bs -= bs % bpb;
		}
		gzip->compressed_buffer_size = bs;
		gzip->compressed = malloc(gzip->compressed_buffer_size);
		if (gzip->compressed == NULL) {
			archive_set_error(f->archive, ENOMEM,
			    "Can't allocate data for compression buffer");
			return (ARCHIVE_FATAL);
		}
	}

	gzip->crc = crc32(0L, NULL, 0);
	gzip->stream.next_out = gzip->compressed;
	gzip->stream.avail_out = (uInt)gzip->compressed_buffer_size;

	/* Prime output buffer with a gzip header. */
	gzip->compressed[0] = 0x1f; /* GZip signature bytes */
	gzip->compressed[1] = 0x8b;
	gzip->compressed[2] = 0x08; /* "Deflate" compression */
	gzip->compressed[3] = 0x00; /* Flags */
	if (gzip->timestamp >= 0) {
		uint32_t t = (uint32_t)time(NULL);
		archive_le32enc(gzip->compressed + 4, t); /* Timestamp */
	} else {
		memset(&gzip->compressed[4], 0, 4);
	}
	if (gzip->compression_level == 9) {
		gzip->compressed[8] = 2;
	} else if(gzip->compression_level == 1) {
		gzip->compressed[8] = 4;
	} else {
		gzip->compressed[8] = 0;
	}
	gzip->compressed[9] = 3; /* OS=Unix */
	gzip->stream.next_out += 10;
	gzip->stream.avail_out -= 10;

	if (gzip->original_filename != NULL) {
		/* Limit "original filename" to 32k or the
		 * remaining space in the buffer, whichever is smaller.
		 */
		size_t ofn_length = strlen(gzip->original_filename);
		size_t ofn_max_length = 32768;
		size_t ofn_space_available = gzip->compressed
			+ gzip->compressed_buffer_size
			- gzip->stream.next_out
			- 1;
		if (ofn_max_length > ofn_space_available) {
			ofn_max_length = ofn_space_available;
		}
		if (ofn_length < ofn_max_length) {
			gzip->compressed[3] |= 0x8;
			strcpy((char*)gzip->compressed + 10,
			       gzip->original_filename);
			gzip->stream.next_out += ofn_length + 1;
			gzip->stream.avail_out -= ofn_length + 1;
		} else {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
					  "Gzip 'Original Filename' ignored because it is too long");
			ret = ARCHIVE_WARN;
		}
	}

	/* Initialize compression library. */
	init_success = deflateInit2(&(gzip->stream),
	    gzip->compression_level,
	    Z_DEFLATED,
	    -15 /* < 0 to suppress zlib header */,
	    8,
	    Z_DEFAULT_STRATEGY);

	if (init_success == Z_OK) {
		return (ret);
	}

	/* Library setup failed: clean up. */
	archive_set_error(f->archive, ARCHIVE_ERRNO_MISC, "Internal error "
	    "initializing compression library");

	/* Override the error message if we know what really went wrong. */
	switch (init_success) {
	case Z_STREAM_ERROR:
		archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing "
		    "compression library: invalid setup parameter");
		break;
	case Z_MEM_ERROR:
		archive_set_error(f->archive, ENOMEM,
		    "Internal error initializing compression library");
		break;
	case Z_VERSION_ERROR:
		archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing "
		    "compression library: invalid library version");
		break;
	}

	return (ARCHIVE_FATAL);
}

/*
 * Write data to the compressed stream.
 */
static int
archive_compressor_gzip_write(struct archive_write_filter *f, const void *buff,
    size_t length)
{
	struct gzip *gzip = f->data;
	int ret;

	/* Update statistics */
	gzip->crc = crc32(gzip->crc, (const Bytef *)buff, (uInt)length);
	gzip->total_in += length;

	/* Compress input data to output buffer */
	SET_NEXT_IN(gzip, buff);
	gzip->stream.avail_in = (uInt)length;
	if ((ret = drive_compressor(f, gzip, 0)) != ARCHIVE_OK)
		return (ret);

	return (ARCHIVE_OK);
}

/*
 * Finish the compression...
 */
static int
archive_compressor_gzip_close(struct archive_write_filter *f)
{
	struct gzip *gzip = f->data;
	unsigned char trailer[8];
	int ret;

	/* Finish compression cycle */
	ret = drive_compressor(f, gzip, 1);
	if (ret == ARCHIVE_OK) {
		/* Write the last compressed data. */
		ret = __archive_write_filter(f->next_filter,
		    gzip->compressed,
		    gzip->compressed_buffer_size - gzip->stream.avail_out);
	}
	if (ret == ARCHIVE_OK) {
		/* Build and write out 8-byte trailer. */
		archive_le32enc(trailer, gzip->crc);
		archive_le32enc(trailer + 4, gzip->total_in);
		ret = __archive_write_filter(f->next_filter, trailer, 8);
	}

	switch (deflateEnd(&(gzip->stream))) {
	case Z_OK:
		break;
	default:
		archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
		    "Failed to clean up compressor");
		ret = ARCHIVE_FATAL;
	}
	return ret;
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
    struct gzip *gzip, int finishing)
{
	int ret;

	for (;;) {
		if (gzip->stream.avail_out == 0) {
			ret = __archive_write_filter(f->next_filter,
			    gzip->compressed,
			    gzip->compressed_buffer_size);
			if (ret != ARCHIVE_OK)
				return (ARCHIVE_FATAL);
			gzip->stream.next_out = gzip->compressed;
			gzip->stream.avail_out =
			    (uInt)gzip->compressed_buffer_size;
		}

		/* If there's nothing to do, we're done. */
		if (!finishing && gzip->stream.avail_in == 0)
			return (ARCHIVE_OK);

		ret = deflate(&(gzip->stream),
		    finishing ? Z_FINISH : Z_NO_FLUSH );

		switch (ret) {
		case Z_OK:
			/* In non-finishing case, check if compressor
			 * consumed everything */
			if (!finishing && gzip->stream.avail_in == 0)
				return (ARCHIVE_OK);
			/* In finishing case, this return always means
			 * there's more work */
			break;
		case Z_STREAM_END:
			/* This return can only occur in finishing case. */
			return (ARCHIVE_OK);
		default:
			/* Any other return value indicates an error. */
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "GZip compression failed:"
			    " deflate() call returned status %d",
			    ret);
			return (ARCHIVE_FATAL);
		}
	}
}

static void
free_data(struct gzip *gzip)
{
	if (gzip != NULL) {
		free(gzip->compressed);
		free(gzip->original_filename);
		free(gzip);
	}
}

#else /* HAVE_ZLIB_H */

static int
archive_compressor_gzip_open(struct archive_write_filter *f)
{
	struct gzip *gzip = f->data;
	struct archive_string as;
	int r;

	archive_string_init(&as);
	archive_strcpy(&as, "gzip");

	/* Specify compression level. */
	if (gzip->compression_level > 0) {
		archive_strcat(&as, " -");
		archive_strappend_char(&as, '0' + gzip->compression_level);
	}
	if (gzip->timestamp < 0)
		/* Do not save timestamp. */
		archive_strcat(&as, " -n");
	else if (gzip->timestamp > 0)
		/* Save timestamp. */
		archive_strcat(&as, " -N");

	r = __archive_write_program_open(f, gzip->pdata, as.s);
	archive_string_free(&as);
	return (r);
}

static int
archive_compressor_gzip_write(struct archive_write_filter *f, const void *buff,
    size_t length)
{
	struct gzip *gzip = f->data;

	return __archive_write_program_write(f, gzip->pdata, buff, length);
}

static int
archive_compressor_gzip_close(struct archive_write_filter *f)
{
	struct gzip *gzip = f->data;

	return __archive_write_program_close(f, gzip->pdata);
}

static void
free_data(struct gzip *gzip)
{
	if (gzip != NULL) {
		__archive_write_program_free(gzip->pdata);
		free(gzip->original_filename);
		free(gzip);
	}
}
#endif /* HAVE_ZLIB_H */
