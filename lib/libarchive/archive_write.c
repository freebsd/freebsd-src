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
__FBSDID("$FreeBSD$");

/*
 * This file contains the "essential" portions of the write API, that
 * is, stuff that will essentially always be used by any client that
 * actually needs to write a archive.  Optional pieces have been, as
 * far as possible, separated out into separate files to reduce
 * needlessly bloating statically-linked clients.
 */

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
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
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_write_private.h"

static struct archive_vtable *archive_write_vtable(void);

static int	_archive_write_close(struct archive *);
static int	_archive_write_finish(struct archive *);
static int	_archive_write_header(struct archive *, struct archive_entry *);
static int	_archive_write_finish_entry(struct archive *);
static ssize_t	_archive_write_data(struct archive *, const void *, size_t);

static struct archive_vtable *
archive_write_vtable(void)
{
	static struct archive_vtable av;
	static int inited = 0;

	if (!inited) {
		av.archive_write_close = _archive_write_close;
		av.archive_write_finish = _archive_write_finish;
		av.archive_write_header = _archive_write_header;
		av.archive_write_finish_entry = _archive_write_finish_entry;
		av.archive_write_data = _archive_write_data;
	}
	return (&av);
}

/*
 * Allocate, initialize and return an archive object.
 */
struct archive *
archive_write_new(void)
{
	struct archive_write *a;
	unsigned char *nulls;

	a = (struct archive_write *)malloc(sizeof(*a));
	if (a == NULL)
		return (NULL);
	memset(a, 0, sizeof(*a));
	a->archive.magic = ARCHIVE_WRITE_MAGIC;
	a->archive.state = ARCHIVE_STATE_NEW;
	a->archive.vtable = archive_write_vtable();
	a->bytes_per_block = ARCHIVE_DEFAULT_BYTES_PER_BLOCK;
	a->bytes_in_last_block = -1;	/* Default */

	/* Initialize a block of nulls for padding purposes. */
	a->null_length = 1024;
	nulls = (unsigned char *)malloc(a->null_length);
	if (nulls == NULL) {
		free(a);
		return (NULL);
	}
	memset(nulls, 0, a->null_length);
	a->nulls = nulls;
	/*
	 * Set default compression, but don't set a default format.
	 * Were we to set a default format here, we would force every
	 * client to link in support for that format, even if they didn't
	 * ever use it.
	 */
	archive_write_set_compression_none(&a->archive);
	return (&a->archive);
}

/*
 * Set the block size.  Returns 0 if successful.
 */
int
archive_write_set_bytes_per_block(struct archive *_a, int bytes_per_block)
{
	struct archive_write *a = (struct archive_write *)_a;
	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_set_bytes_per_block");
	a->bytes_per_block = bytes_per_block;
	return (ARCHIVE_OK);
}

/*
 * Get the current block size.  -1 if it has never been set.
 */
int
archive_write_get_bytes_per_block(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_write_get_bytes_per_block");
	return (a->bytes_per_block);
}

/*
 * Set the size for the last block.
 * Returns 0 if successful.
 */
int
archive_write_set_bytes_in_last_block(struct archive *_a, int bytes)
{
	struct archive_write *a = (struct archive_write *)_a;
	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_write_set_bytes_in_last_block");
	a->bytes_in_last_block = bytes;
	return (ARCHIVE_OK);
}

/*
 * Return the value set above.  -1 indicates it has not been set.
 */
int
archive_write_get_bytes_in_last_block(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_write_get_bytes_in_last_block");
	return (a->bytes_in_last_block);
}


/*
 * dev/ino of a file to be rejected.  Used to prevent adding
 * an archive to itself recursively.
 */
int
archive_write_set_skip_file(struct archive *_a, dev_t d, ino_t i)
{
	struct archive_write *a = (struct archive_write *)_a;
	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_write_set_skip_file");
	a->skip_file_dev = d;
	a->skip_file_ino = i;
	return (ARCHIVE_OK);
}


/*
 * Open the archive using the current settings.
 */
int
archive_write_open(struct archive *_a, void *client_data,
    archive_open_callback *opener, archive_write_callback *writer,
    archive_close_callback *closer)
{
	struct archive_write *a = (struct archive_write *)_a;
	int ret;

	ret = ARCHIVE_OK;
	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_open");
	archive_clear_error(&a->archive);
	a->archive.state = ARCHIVE_STATE_HEADER;
	a->client_data = client_data;
	a->client_writer = writer;
	a->client_opener = opener;
	a->client_closer = closer;
	ret = (a->compressor.init)(a);
	if (a->format_init && ret == ARCHIVE_OK)
		ret = (a->format_init)(a);
	return (ret);
}


/*
 * Close out the archive.
 *
 * Be careful: user might just call write_new and then write_finish.
 * Don't assume we actually wrote anything or performed any non-trivial
 * initialization.
 */
static int
_archive_write_close(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	int r = ARCHIVE_OK, r1 = ARCHIVE_OK;

	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_write_close");

	/* Finish the last entry. */
	if (a->archive.state & ARCHIVE_STATE_DATA)
		r = ((a->format_finish_entry)(a));

	/* Finish off the archive. */
	if (a->format_finish != NULL) {
		r1 = (a->format_finish)(a);
		if (r1 < r)
			r = r1;
	}

	/* Release format resources. */
	if (a->format_destroy != NULL) {
		r1 = (a->format_destroy)(a);
		if (r1 < r)
			r = r1;
	}

	/* Finish the compression and close the stream. */
	if (a->compressor.finish != NULL) {
		r1 = (a->compressor.finish)(a);
		if (r1 < r)
			r = r1;
	}

	/* Close out the client stream. */
	if (a->client_closer != NULL) {
		r1 = (a->client_closer)(&a->archive, a->client_data);
		if (r1 < r)
			r = r1;
	}

	a->archive.state = ARCHIVE_STATE_CLOSED;
	return (r);
}

/*
 * Destroy the archive structure.
 */
static int
_archive_write_finish(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	int r = ARCHIVE_OK;

	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_write_finish");
	if (a->archive.state != ARCHIVE_STATE_CLOSED)
		r = archive_write_close(&a->archive);

	/* Release various dynamic buffers. */
	free((void *)(uintptr_t)(const void *)a->nulls);
	archive_string_free(&a->archive.error_string);
	a->archive.magic = 0;
	free(a);
	return (r);
}

/*
 * Write the appropriate header.
 */
static int
_archive_write_header(struct archive *_a, struct archive_entry *entry)
{
	struct archive_write *a = (struct archive_write *)_a;
	int ret, r2;

	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_DATA | ARCHIVE_STATE_HEADER, "archive_write_header");
	archive_clear_error(&a->archive);

	/* In particular, "retry" and "fatal" get returned immediately. */
	ret = archive_write_finish_entry(&a->archive);
	if (ret < ARCHIVE_OK && ret != ARCHIVE_WARN)
		return (ret);

	if (a->skip_file_dev != 0 &&
	    archive_entry_dev(entry) == a->skip_file_dev &&
	    a->skip_file_ino != 0 &&
	    archive_entry_ino(entry) == a->skip_file_ino) {
		archive_set_error(&a->archive, 0,
		    "Can't add archive to itself");
		return (ARCHIVE_FAILED);
	}

	/* Format and write header. */
	r2 = ((a->format_write_header)(a, entry));
	if (r2 < ret)
		ret = r2;

	a->archive.state = ARCHIVE_STATE_DATA;
	return (ret);
}

static int
_archive_write_finish_entry(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	int ret = ARCHIVE_OK;

	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_HEADER | ARCHIVE_STATE_DATA,
	    "archive_write_finish_entry");
	if (a->archive.state & ARCHIVE_STATE_DATA)
		ret = (a->format_finish_entry)(a);
	a->archive.state = ARCHIVE_STATE_HEADER;
	return (ret);
}

/*
 * Note that the compressor is responsible for blocking.
 */
static ssize_t
_archive_write_data(struct archive *_a, const void *buff, size_t s)
{
	struct archive_write *a = (struct archive_write *)_a;
	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_DATA, "archive_write_data");
	archive_clear_error(&a->archive);
	return ((a->format_write_data)(a, buff, s));
}
