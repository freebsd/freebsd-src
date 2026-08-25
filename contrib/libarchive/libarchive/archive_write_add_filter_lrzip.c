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

#include "archive.h"
#include "archive_string.h"
#include "archive_write_private.h"

struct lrzip {
	struct archive_write_program_data *pdata;
	int	compression_level;
	enum { lzma = 0, bzip2, gzip, lzo, none, zpaq } compression;
};

static int archive_write_lrzip_open(struct archive_write_filter *);
static int archive_write_lrzip_options(struct archive_write_filter *,
		    const char *, const char *);
static int archive_write_lrzip_write(struct archive_write_filter *,
		    const void *, size_t);
static int archive_write_lrzip_close(struct archive_write_filter *);
static int archive_write_lrzip_free(struct archive_write_filter *);
static void free_data(struct lrzip *);

int
archive_write_add_filter_lrzip(struct archive *a)
{
	struct archive_write_filter *f;
	struct lrzip *lrzip;

	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_add_filter_lrzip");

	lrzip = calloc(1, sizeof(*lrzip));
	if (lrzip == NULL)
		goto memerr;
	lrzip->pdata = __archive_write_program_allocate("lrzip");
	if (lrzip->pdata == NULL)
		goto memerr;

	f = __archive_write_allocate_filter(a);
	if (f == NULL)
		goto memerr;
	f->name = "lrzip";
	f->code = ARCHIVE_FILTER_LRZIP;
	f->data = lrzip;
	f->options = archive_write_lrzip_options;
	f->open = archive_write_lrzip_open;
	f->write = archive_write_lrzip_write;
	f->close = archive_write_lrzip_close;
	f->free = archive_write_lrzip_free;

	/* Note: This filter always uses an external program, so we
	 * return "warn" to inform of the fact. */
	archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    "Using external lrzip program for lrzip compression");
	return (ARCHIVE_WARN);
memerr:
	free_data(lrzip);
	archive_set_error(a, ENOMEM, "Can't allocate memory");
	return (ARCHIVE_FATAL);
}

static int
archive_write_lrzip_options(struct archive_write_filter *f, const char *key,
    const char *value)
{
	struct lrzip *lrzip = f->data;

	if (strcmp(key, "compression") == 0) {
		if (value == NULL) {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "compression option requires an argument");
			return (ARCHIVE_FAILED);
		}
		else if (strcmp(value, "bzip2") == 0)
			lrzip->compression = bzip2;
		else if (strcmp(value, "gzip") == 0)
			lrzip->compression = gzip;
		else if (strcmp(value, "lzo") == 0)
			lrzip->compression = lzo;
		else if (strcmp(value, "none") == 0)
			lrzip->compression = none;
		else if (strcmp(value, "zpaq") == 0)
			lrzip->compression = zpaq;
		else {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "compression invalid");
			return (ARCHIVE_FAILED);
		}
		return (ARCHIVE_OK);
	} else if (strcmp(key, "compression-level") == 0) {
		if (value == NULL || !(value[0] >= '1' && value[0] <= '9') ||
		    value[1] != '\0') {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "compression-level invalid");
			return (ARCHIVE_FAILED);
		}
		lrzip->compression_level = value[0] - '0';
		return (ARCHIVE_OK);
	}
	/* Note: The "warn" return is just to inform the options
	 * supervisor that we didn't handle it.  It will generate
	 * a suitable error if no one used this option. */
	return (ARCHIVE_WARN);
}

static int
archive_write_lrzip_open(struct archive_write_filter *f)
{
	struct lrzip *lrzip = f->data;
	struct archive_string as;
	int r;

	archive_string_init(&as);
	archive_strcpy(&as, "lrzip -q");

	/* Specify compression type. */
	switch (lrzip->compression) {
	case lzma:/* default compression */
		break;
	case bzip2:
		archive_strcat(&as, " -b");
		break;
	case gzip:
		archive_strcat(&as, " -g");
		break;
	case lzo:
		archive_strcat(&as, " -l");
		break;
	case none:
		archive_strcat(&as, " -n");
		break;
	case zpaq:
		archive_strcat(&as, " -z");
		break;
	}

	/* Specify compression level. */
	if (lrzip->compression_level > 0) {
		archive_strcat(&as, " -L ");
		archive_strappend_char(&as, '0' + lrzip->compression_level);
	}

	r = __archive_write_program_open(f, lrzip->pdata, as.s);
	archive_string_free(&as);
	return (r);
}

static int
archive_write_lrzip_write(struct archive_write_filter *f,
    const void *buff, size_t length)
{
	struct lrzip *lrzip = f->data;

	return __archive_write_program_write(f, lrzip->pdata, buff, length);
}

static int
archive_write_lrzip_close(struct archive_write_filter *f)
{
	struct lrzip *lrzip = f->data;

	return __archive_write_program_close(f, lrzip->pdata);
}

static int
archive_write_lrzip_free(struct archive_write_filter *f)
{
	free_data(f->data);
	f->data = NULL;
	return (ARCHIVE_OK);
}

static void
free_data(struct lrzip *lrzip)
{
	if (lrzip != NULL) {
		__archive_write_program_free(lrzip->pdata);
		free(lrzip);
	}
}
