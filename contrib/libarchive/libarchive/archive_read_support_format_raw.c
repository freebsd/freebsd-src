/*-
 * Copyright (c) 2003-2009 Tim Kientzle
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
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_integer.h"
#include "archive_private.h"
#include "archive_read_private.h"

struct raw {
	int64_t offset; /* Current position in the file. */
	int64_t unconsumed;
	int     end_of_file;
};

static int	archive_read_format_raw_bid(struct archive_read *, int);
static int	archive_read_format_raw_cleanup(struct archive_read *);
static int	archive_read_format_raw_read_data(struct archive_read *,
		    const void **, size_t *, int64_t *);
static int	archive_read_format_raw_read_data_skip(struct archive_read *);
static int	archive_read_format_raw_read_header(struct archive_read *,
		    struct archive_entry *);

int
archive_read_support_format_raw(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct raw *raw;
	int r;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_read_support_format_raw");

	raw = calloc(1, sizeof(*raw));
	if (raw == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate raw data");
		return (ARCHIVE_FATAL);
	}

	r = __archive_read_register_format(a,
	    raw,
	    "raw",
	    archive_read_format_raw_bid,
	    NULL,
	    archive_read_format_raw_read_header,
	    archive_read_format_raw_read_data,
	    archive_read_format_raw_read_data_skip,
	    NULL,
	    archive_read_format_raw_cleanup,
	    NULL,
	    NULL);
	if (r != ARCHIVE_OK)
		free(raw);
	return (r);
}

/*
 * Bid 1 if this is a non-empty file.  Anyone who can really support
 * this should outbid us, so it should generally be safe to use "raw"
 * in conjunction with other formats.  But, this could really confuse
 * folks if there are bid errors or minor file damage, so we don't
 * include "raw" as part of support_format_all().
 */
static int
archive_read_format_raw_bid(struct archive_read *a, int best_bid)
{
	if (best_bid < 1 && __archive_read_ahead(a, 1, NULL) != NULL)
		return (1);
	return (-1);
}

/*
 * Mock up a fake header.
 */
static int
archive_read_format_raw_read_header(struct archive_read *a,
    struct archive_entry *entry)
{
	struct raw *raw = a->format->data;

	if (raw->end_of_file)
		return (ARCHIVE_EOF);

	a->archive.archive_format = ARCHIVE_FORMAT_RAW;
	a->archive.archive_format_name = "raw";
	archive_entry_set_pathname(entry, "data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_perm(entry, 0644);
	/* I'm deliberately leaving most fields unset here. */

	/* Let the filter fill out any fields it might have. */
	return __archive_read_header(a, entry);
}

static int
archive_read_format_raw_read_data(struct archive_read *a,
    const void **buff, size_t *size, int64_t *offset)
{
	struct raw *raw = a->format->data;
	ssize_t avail;

	/* Consume the bytes we read last time. */
	if (raw->unconsumed) {
		__archive_read_consume(a, raw->unconsumed);
		raw->unconsumed = 0;
	}

	if (raw->end_of_file)
		return (ARCHIVE_EOF);

	/* Get whatever bytes are immediately available. */
	*buff = __archive_read_ahead(a, 1, &avail);
	if (avail > 0) {
		/* Return the bytes we just read */
		*offset = raw->offset;
		if (archive_ckd_add_i64(&raw->offset, raw->offset, avail)) {
			avail = INT64_MAX - *offset;
			if (avail == 0) {
				*size = 0;
				return (ARCHIVE_FATAL);
			}
		}
		*size = avail;
		raw->unconsumed = avail;
		return (ARCHIVE_OK);
	} else if (0 == avail) {
		/* Record and return end-of-file. */
		raw->end_of_file = 1;
		*size = 0;
		*offset = raw->offset;
		return (ARCHIVE_EOF);
	} else {
		/* Record and return an error. */
		*size = 0;
		*offset = raw->offset;
		return ((int)avail);
	}
}

static int
archive_read_format_raw_read_data_skip(struct archive_read *a)
{
	struct raw *raw = a->format->data;

	/* Consume the bytes we read last time. */
	if (raw->unconsumed) {
		__archive_read_consume(a, raw->unconsumed);
		raw->unconsumed = 0;
	}
	raw->end_of_file = 1;
	return (ARCHIVE_OK);
}

static int
archive_read_format_raw_cleanup(struct archive_read *a)
{
	struct raw *raw = a->format->data;

	free(raw);
	a->format->data = NULL;
	return (ARCHIVE_OK);
}
