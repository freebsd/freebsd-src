/*-
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

//#undef HAVE_LZO_LZOCONF_H
//#undef HAVE_LZO_LZO1X_H

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
#ifdef HAVE_LZO_LZOCONF_H
#include <lzo/lzoconf.h>
#endif
#ifdef HAVE_LZO_LZO1X_H
#include <lzo/lzo1x.h>
#endif

#include "archive.h"
#include "archive_string.h"
#include "archive_endian.h"
#include "archive_write_private.h"

enum lzo_method {
	METHOD_LZO1X_1 = 1,
	METHOD_LZO1X_1_15 = 2,
	METHOD_LZO1X_999 = 3
};
struct lzop {
	int compression_level;
#if defined(HAVE_LZO_LZOCONF_H) && defined(HAVE_LZO_LZO1X_H)
	unsigned char	*uncompressed;
	size_t		 uncompressed_buffer_size;
	size_t		 uncompressed_avail_bytes;
	unsigned char	*compressed;
	size_t		 compressed_buffer_size;
	enum lzo_method	 method;
	unsigned char	 level;
	lzo_voidp	 work_buffer;
	lzo_uint32	 work_buffer_size;
	char		 header_written;
#else
	struct archive_write_program_data *pdata;
#endif
};

static int archive_write_lzop_open(struct archive_write_filter *);
static int archive_write_lzop_options(struct archive_write_filter *,
		    const char *, const char *);
static int archive_write_lzop_write(struct archive_write_filter *,
		    const void *, size_t);
static int archive_write_lzop_close(struct archive_write_filter *);
static int archive_write_lzop_free(struct archive_write_filter *);
static void free_data(struct lzop *);

#if defined(HAVE_LZO_LZOCONF_H) && defined(HAVE_LZO_LZO1X_H)
/* Maximum block size. */
#define BLOCK_SIZE			(256 * 1024)
/* Block information is composed of uncompressed size(4 bytes),
 * compressed size(4 bytes) and the checksum of uncompressed data(4 bytes)
 * in this lzop writer. */
#define BLOCK_INfO_SIZE			12

#define HEADER_VERSION			9
#define HEADER_LIBVERSION		11
#define HEADER_METHOD			15
#define HEADER_LEVEL			16
#define HEADER_MTIME_LOW		25
#define HEADER_MTIME_HIGH		29
#define HEADER_H_CHECKSUM		34

/*
 * Header template.
 */
static const unsigned char header[] = {
	/* LZOP Magic code 9 bytes */
	0x89, 0x4c, 0x5a, 0x4f, 0x00, 0x0d, 0x0a, 0x1a, 0x0a,
	/* LZOP utility version(fake data) 2 bytes */
	0x10, 0x30,
	/* LZO library version 2 bytes */
	0x09, 0x40,
	/* Minimum required LZO library version 2 bytes */
	0x09, 0x40,
	/* Method */
	1,
	/* Level */
	5,
	/* Flags 4 bytes
	 *  -OS Unix
	 *  -Stdout
	 *  -Stdin
	 *  -Adler32 used for uncompressed data 4 bytes */
	0x03, 0x00, 0x00, 0x0d,
	/* Mode (AE_IFREG | 0644) 4 bytes */
	0x00, 0x00, 0x81, 0xa4,
	/* Mtime low 4 bytes */
	0x00, 0x00, 0x00, 0x00,
	/* Mtime high 4 bytes */
	0x00, 0x00, 0x00, 0x00,
	/* Filename length */
	0x00,
	/* Header checksum 4 bytes */
	0x00, 0x00, 0x00, 0x00,
};
#endif

int
archive_write_add_filter_lzop(struct archive *a)
{
	struct archive_write_filter *f;
	struct lzop *lzop;
	int r;

	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_add_filter_lzop");

	lzop = calloc(1, sizeof(*lzop));
	if (lzop == NULL)
		goto memerr;
#if defined(HAVE_LZO_LZOCONF_H) && defined(HAVE_LZO_LZO1X_H)
	if (lzo_init() != LZO_E_OK) {
		free_data(lzop);
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "lzo_init(type check) failed");
		return (ARCHIVE_FATAL);
	}
	if (lzo_version() < 0x940) {
		free_data(lzop);
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "liblzo library is too old(%s < 0.940)",
		    lzo_version_string());
		return (ARCHIVE_FATAL);
	}
	lzop->compression_level = 5;

	r = ARCHIVE_OK;
#else
	lzop->pdata = __archive_write_program_allocate("lzop");
	if (lzop->pdata == NULL)
		goto memerr;
	lzop->compression_level = 0;

	/* Note: We return "warn" to inform of using an external lzop
	 * program. */
	archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    "Using external lzop program for lzop compression");
	r = ARCHIVE_WARN;
#endif

	f = __archive_write_allocate_filter(a);
	if (f == NULL)
		goto memerr;
	f->name = "lzop";
	f->code = ARCHIVE_FILTER_LZOP;
	f->data = lzop;
	f->options = archive_write_lzop_options;
	f->open = archive_write_lzop_open;
	f->write = archive_write_lzop_write;
	f->close = archive_write_lzop_close;
	f->free = archive_write_lzop_free;

	return (r);
memerr:
	free_data(lzop);
	archive_set_error(a, ENOMEM, "Can't allocate memory");
	return (ARCHIVE_FATAL);
}

static int
archive_write_lzop_free(struct archive_write_filter *f)
{
	free_data(f->data);
	f->data = NULL;
	return (ARCHIVE_OK);
}

static int
archive_write_lzop_options(struct archive_write_filter *f, const char *key,
    const char *value)
{
	struct lzop *lzop = f->data;

	if (strcmp(key, "compression-level") == 0) {
		if (value == NULL || !(value[0] >= '1' && value[0] <= '9') ||
		    value[1] != '\0') {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "compression-level invalid");
			return (ARCHIVE_FAILED);
		}
		lzop->compression_level = value[0] - '0';
		return (ARCHIVE_OK);
	}
	/* Note: The "warn" return is just to inform the options
	 * supervisor that we didn't handle it.  It will generate
	 * a suitable error if no one used this option. */
	return (ARCHIVE_WARN);
}

#if defined(HAVE_LZO_LZOCONF_H) && defined(HAVE_LZO_LZO1X_H)
static int
archive_write_lzop_open(struct archive_write_filter *f)
{
	struct lzop *lzop = f->data;

	switch (lzop->compression_level) {
	case 1:
		lzop->method = METHOD_LZO1X_1_15; lzop->level = 1; break;
	default:
	case 2: case 3: case 4: case 5: case 6:
		lzop->method = METHOD_LZO1X_1; lzop->level = 5; break;
	case 7:
		lzop->method = METHOD_LZO1X_999; lzop->level = 7; break;
	case 8:
		lzop->method = METHOD_LZO1X_999; lzop->level = 8; break;
	case 9:
		lzop->method = METHOD_LZO1X_999; lzop->level = 9; break;
	}
	switch (lzop->method) {
	case METHOD_LZO1X_1:
		lzop->work_buffer_size = LZO1X_1_MEM_COMPRESS; break;
	case METHOD_LZO1X_1_15:
		lzop->work_buffer_size = LZO1X_1_15_MEM_COMPRESS; break;
	case METHOD_LZO1X_999:
		lzop->work_buffer_size = LZO1X_999_MEM_COMPRESS; break;
	}
	if (lzop->work_buffer == NULL) {
		lzop->work_buffer = (lzo_voidp)malloc(lzop->work_buffer_size);
		if (lzop->work_buffer == NULL) {
			archive_set_error(f->archive, ENOMEM,
			    "Can't allocate data for compression buffer");
			return (ARCHIVE_FATAL);
		}
	}
	if (lzop->compressed == NULL) {
		lzop->compressed_buffer_size = sizeof(header) +
		    BLOCK_SIZE + (BLOCK_SIZE >> 4) + 64 + 3;
		lzop->compressed = (unsigned char *)
		    malloc(lzop->compressed_buffer_size);
		if (lzop->compressed == NULL) {
			archive_set_error(f->archive, ENOMEM,
			    "Can't allocate data for compression buffer");
			return (ARCHIVE_FATAL);
		}
	}
	if (lzop->uncompressed == NULL) {
		lzop->uncompressed_buffer_size = BLOCK_SIZE;
		lzop->uncompressed = (unsigned char *)
		    malloc(lzop->uncompressed_buffer_size);
		if (lzop->uncompressed == NULL) {
			archive_set_error(f->archive, ENOMEM,
			    "Can't allocate data for compression buffer");
			return (ARCHIVE_FATAL);
		}
		lzop->uncompressed_avail_bytes = BLOCK_SIZE;
	}
	return (ARCHIVE_OK);
}

static int
make_header(struct archive_write_filter *f)
{
	struct lzop *lzop = f->data;
	int64_t t;
	uint32_t checksum;

	memcpy(lzop->compressed, header, sizeof(header));
	/* Overwrite library version. */
	lzop->compressed[HEADER_LIBVERSION] = (unsigned char )
	    (lzo_version() >> 8) & 0xff;
	lzop->compressed[HEADER_LIBVERSION + 1] = (unsigned char )
	    lzo_version() & 0xff;
	/* Overwrite method and level. */
	lzop->compressed[HEADER_METHOD] = (unsigned char)lzop->method;
	lzop->compressed[HEADER_LEVEL] = lzop->level;
	/* Overwrite mtime with current time. */
	t = (int64_t)time(NULL);
	archive_be32enc(&lzop->compressed[HEADER_MTIME_LOW],
	    (uint32_t)(t & 0xffffffff));
	archive_be32enc(&lzop->compressed[HEADER_MTIME_HIGH],
	    (uint32_t)((t >> 32) & 0xffffffff));
	/* Overwrite header checksum with calculated value. */
	checksum = lzo_adler32(1, lzop->compressed + HEADER_VERSION,
			(lzo_uint)(HEADER_H_CHECKSUM - HEADER_VERSION));
	archive_be32enc(&lzop->compressed[HEADER_H_CHECKSUM], checksum);
	return (sizeof(header));
}

static int
drive_compressor(struct archive_write_filter *f)
{
	struct lzop *lzop = f->data;
	unsigned char *p;
	const int block_info_bytes = 12;
	int header_bytes, r;
	lzo_uint usize, csize;
	uint32_t checksum;

	if (!lzop->header_written) {
		header_bytes = make_header(f);
		lzop->header_written = 1;
	} else
		header_bytes = 0;
	p = lzop->compressed;

	usize = (lzo_uint)
	    (lzop->uncompressed_buffer_size - lzop->uncompressed_avail_bytes);
	csize = 0;
	switch (lzop->method) {
	default:
	case METHOD_LZO1X_1:
		r = lzo1x_1_compress(lzop->uncompressed, usize,
			p + header_bytes + block_info_bytes, &csize,
			lzop->work_buffer);
		break;
	case METHOD_LZO1X_1_15:
		r = lzo1x_1_15_compress(lzop->uncompressed, usize,
			p + header_bytes + block_info_bytes, &csize,
			lzop->work_buffer);
		break;
	case METHOD_LZO1X_999:
		r = lzo1x_999_compress_level(lzop->uncompressed, usize,
			p + header_bytes + block_info_bytes, &csize,
			lzop->work_buffer, NULL, 0, 0, lzop->level);
		break;
	}
	if (r != LZO_E_OK) {
		archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
		    "Lzop compression failed: returned status %d", r);
		return (ARCHIVE_FATAL);
	}

	/* Store uncompressed size. */
	archive_be32enc(p + header_bytes, (uint32_t)usize);
	/* Store the checksum of the uncompressed data. */
	checksum = lzo_adler32(1, lzop->uncompressed, usize);
	archive_be32enc(p + header_bytes + 8, checksum);

	if (csize < usize) {
		/* Store compressed size. */
		archive_be32enc(p + header_bytes + 4, (uint32_t)csize);
		r = __archive_write_filter(f->next_filter, lzop->compressed,
			header_bytes + block_info_bytes + csize);
	} else {
		/*
		 * This case, we output uncompressed data instead.
		 */
		/* Store uncompressed size as compressed size. */
		archive_be32enc(p + header_bytes + 4, (uint32_t)usize);
		r = __archive_write_filter(f->next_filter, lzop->compressed,
			header_bytes + block_info_bytes);
		if (r != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
		r = __archive_write_filter(f->next_filter, lzop->uncompressed,
			usize);
	}

	if (r != ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	return (ARCHIVE_OK);
}

static int
archive_write_lzop_write(struct archive_write_filter *f,
    const void *buff, size_t length)
{
	struct lzop *lzop = f->data;
	const char *p = buff;
	int r;

	do {
		if (lzop->uncompressed_avail_bytes > length) {
			memcpy(lzop->uncompressed
				+ lzop->uncompressed_buffer_size
				- lzop->uncompressed_avail_bytes,
			    p, length);
			lzop->uncompressed_avail_bytes -= length;
			return (ARCHIVE_OK);
		}

		memcpy(lzop->uncompressed + lzop->uncompressed_buffer_size
			- lzop->uncompressed_avail_bytes,
		    p, lzop->uncompressed_avail_bytes);
		length -= lzop->uncompressed_avail_bytes;
		p += lzop->uncompressed_avail_bytes;
		lzop->uncompressed_avail_bytes = 0;

		r = drive_compressor(f);
		if (r != ARCHIVE_OK) return (r);
		lzop->uncompressed_avail_bytes = BLOCK_SIZE;
	} while (length);

	return (ARCHIVE_OK);
}

static int
archive_write_lzop_close(struct archive_write_filter *f)
{
	struct lzop *lzop = f->data;
	const uint32_t endmark = 0;
	int r;

	if (lzop->uncompressed_avail_bytes < BLOCK_SIZE) {
		/* Compress and output remaining data. */
		r = drive_compressor(f);
		if (r != ARCHIVE_OK)
			return (r);
	}
	/* Write a zero uncompressed size as the end mark of the series of
	 * compressed block. */
	return __archive_write_filter(f->next_filter, &endmark, sizeof(endmark));
}

static void
free_data(struct lzop *lzop)
{
	if (lzop != NULL) {
		free(lzop->uncompressed);
		free(lzop->compressed);
		free(lzop->work_buffer);
		free(lzop);
	}
}

#else
static int
archive_write_lzop_open(struct archive_write_filter *f)
{
	struct lzop *lzop = f->data;
	struct archive_string as;
	int r;

	archive_string_init(&as);
	archive_strcpy(&as, "lzop");
	/* Specify compression level. */
	if (lzop->compression_level > 0) {
		archive_strappend_char(&as, ' ');
		archive_strappend_char(&as, '-');
		archive_strappend_char(&as, '0' + lzop->compression_level);
	}

	r = __archive_write_program_open(f, lzop->pdata, as.s);
	archive_string_free(&as);
	return (r);
}

static int
archive_write_lzop_write(struct archive_write_filter *f,
    const void *buff, size_t length)
{
	struct lzop *lzop = f->data;

	return __archive_write_program_write(f, lzop->pdata, buff, length);
}

static int
archive_write_lzop_close(struct archive_write_filter *f)
{
	struct lzop *lzop = f->data;

	return __archive_write_program_close(f, lzop->pdata);
}

static void
free_data(struct lzop *lzop)
{
	if (lzop != NULL) {
		__archive_write_program_free(lzop->pdata);
		free(lzop);
	}
}
#endif
