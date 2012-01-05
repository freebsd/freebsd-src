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
__FBSDID("$FreeBSD$");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_private.h"

struct raw_info {
	int64_t offset; /* Current position in the file. */
	int     end_of_file;
};

static int	archive_read_format_raw_bid(struct archive_read *);
static int	archive_read_format_raw_cleanup(struct archive_read *);
static int	archive_read_format_raw_read_data(struct archive_read *,
		    const void **, size_t *, off_t *);
static int	archive_read_format_raw_read_data_skip(struct archive_read *);
static int	archive_read_format_raw_read_header(struct archive_read *,
		    struct archive_entry *);

int
archive_read_support_format_raw(struct archive *_a)
{
	struct raw_info *info;
	struct archive_read *a = (struct archive_read *)_a;
	int r;

	info = (struct raw_info *)calloc(1, sizeof(*info));
	if (info == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate raw_info data");
		return (ARCHIVE_FATAL);
	}

	r = __archive_read_register_format(a,
	    info,
	    "raw",
	    archive_read_format_raw_bid,
	    NULL,
	    archive_read_format_raw_read_header,
	    archive_read_format_raw_read_data,
	    archive_read_format_raw_read_data_skip,
	    archive_read_format_raw_cleanup);
	if (r != ARCHIVE_OK)
		free(info);
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
archive_read_format_raw_bid(struct archive_read *a)
{

	if (__archive_read_ahead(a, 1, NULL) == NULL)
		return (-1);
	return (1);
}

/*
 * Mock up a fake header.
 */
static int
archive_read_format_raw_read_header(struct archive_read *a,
    struct archive_entry *entry)
{
	struct raw_info *info;

	info = (struct raw_info *)(a->format->data);
	if (info->end_of_file)
		return (ARCHIVE_EOF);

	a->archive.archive_format = ARCHIVE_FORMAT_RAW;
	a->archive.archive_format_name = "Raw data";
	archive_entry_set_pathname(entry, "data");
	/* XXX should we set mode to mimic a regular file? XXX */
	/* I'm deliberately leaving most fields unset here. */
	return (ARCHIVE_OK);
}

static int
archive_read_format_raw_read_data(struct archive_read *a,
    const void **buff, size_t *size, off_t *offset)
{
	struct raw_info *info;
	ssize_t avail;

	info = (struct raw_info *)(a->format->data);
	if (info->end_of_file)
		return (ARCHIVE_EOF);

	/* Get whatever bytes are immediately available. */
	*buff = __archive_read_ahead(a, 1, &avail);
	if (avail > 0) {
		/* Consume and return the bytes we just read */
		__archive_read_consume(a, avail);
		*size = avail;
		*offset = info->offset;
		info->offset += *size;
		return (ARCHIVE_OK);
	} else if (0 == avail) {
		/* Record and return end-of-file. */
		info->end_of_file = 1;
		*size = 0;
		*offset = info->offset;
		return (ARCHIVE_EOF);
	} else {
		/* Record and return an error. */
		*size = 0;
		*offset = info->offset;
		return (avail);
	}
}

static int
archive_read_format_raw_read_data_skip(struct archive_read *a)
{
	struct raw_info *info;
	off_t bytes_skipped;
	int64_t request = 1024 * 1024 * 1024UL; /* Skip 1 GB at a time. */

	info = (struct raw_info *)(a->format->data);
	if (info->end_of_file)
		return (ARCHIVE_EOF);
	info->end_of_file = 1;

	for (;;) {
		bytes_skipped = __archive_read_skip_lenient(a, request);
		if (bytes_skipped < 0)
			return (ARCHIVE_FATAL);
		if (bytes_skipped < request)
			return (ARCHIVE_OK);
		/* We skipped all the bytes we asked for.  There might
		 * be more, so try again. */
	}
}

static int
archive_read_format_raw_cleanup(struct archive_read *a)
{
	struct raw_info *info;

	info = (struct raw_info *)(a->format->data);
	free(info);
	a->format->data = NULL;
	return (ARCHIVE_OK);
}
