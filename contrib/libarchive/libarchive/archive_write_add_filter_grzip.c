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

#include "archive.h"
#include "archive_write_private.h"

struct grzip {
	struct archive_write_program_data *pdata;
};

static int archive_write_grzip_open(struct archive_write_filter *);
static int archive_write_grzip_options(struct archive_write_filter *,
		    const char *, const char *);
static int archive_write_grzip_write(struct archive_write_filter *,
		    const void *, size_t);
static int archive_write_grzip_close(struct archive_write_filter *);
static int archive_write_grzip_free(struct archive_write_filter *);
static void free_data(struct grzip *);

int
archive_write_add_filter_grzip(struct archive *a)
{
	struct archive_write_filter *f;
	struct grzip *grzip;

	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_add_filter_grzip");

	grzip = calloc(1, sizeof(*grzip));
	if (grzip == NULL)
		goto memerr;
	grzip->pdata = __archive_write_program_allocate("grzip");
	if (grzip->pdata == NULL)
		goto memerr;

	f = __archive_write_allocate_filter(a);
	if (f == NULL)
		goto memerr;
	f->name = "grzip";
	f->code = ARCHIVE_FILTER_GRZIP;
	f->data = grzip;
	f->options = archive_write_grzip_options;
	f->open = archive_write_grzip_open;
	f->write = archive_write_grzip_write;
	f->close = archive_write_grzip_close;
	f->free = archive_write_grzip_free;

	/* Note: This filter always uses an external program, so we
	 * return "warn" to inform of the fact. */
	archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    "Using external grzip program for grzip compression");
	return (ARCHIVE_WARN);
memerr:
	free_data(grzip);
	archive_set_error(a, ENOMEM, "Can't allocate memory");
	return (ARCHIVE_FATAL);
}

static int
archive_write_grzip_options(struct archive_write_filter *f, const char *key,
    const char *value)
{
	(void)f; /* UNUSED */
	(void)key; /* UNUSED */
	(void)value; /* UNUSED */
	/* Note: The "warn" return is just to inform the options
	 * supervisor that we didn't handle it.  It will generate
	 * a suitable error if no one used this option. */
	return (ARCHIVE_WARN);
}

static int
archive_write_grzip_open(struct archive_write_filter *f)
{
	struct grzip *grzip = f->data;

	return __archive_write_program_open(f, grzip->pdata, "grzip");
}

static int
archive_write_grzip_write(struct archive_write_filter *f,
    const void *buff, size_t length)
{
	struct grzip *grzip = f->data;

	return __archive_write_program_write(f, grzip->pdata, buff, length);
}

static int
archive_write_grzip_close(struct archive_write_filter *f)
{
	struct grzip *grzip = f->data;

	return __archive_write_program_close(f, grzip->pdata);
}

static int
archive_write_grzip_free(struct archive_write_filter *f)
{
	free_data(f->data);
	f->data = NULL;
	return (ARCHIVE_OK);
}

static void
free_data(struct grzip *grzip)
{
	if (grzip != NULL) {
		__archive_write_program_free(grzip->pdata);
		free(grzip);
	}
}
