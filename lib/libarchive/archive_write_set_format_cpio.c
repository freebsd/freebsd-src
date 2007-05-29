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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_write_private.h"

static ssize_t	archive_write_cpio_data(struct archive_write *,
		    const void *buff, size_t s);
static int	archive_write_cpio_finish(struct archive_write *);
static int	archive_write_cpio_destroy(struct archive_write *);
static int	archive_write_cpio_finish_entry(struct archive_write *);
static int	archive_write_cpio_header(struct archive_write *,
		    struct archive_entry *);
static int	format_octal(int64_t, void *, int);
static int64_t	format_octal_recursive(int64_t, char *, int);

struct cpio {
	uint64_t	  entry_bytes_remaining;
};

struct cpio_header {
	char	c_magic[6];
	char	c_dev[6];
	char	c_ino[6];
	char	c_mode[6];
	char	c_uid[6];
	char	c_gid[6];
	char	c_nlink[6];
	char	c_rdev[6];
	char	c_mtime[11];
	char	c_namesize[6];
	char	c_filesize[11];
};

/*
 * Set output format to 'cpio' format.
 */
int
archive_write_set_format_cpio(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct cpio *cpio;

	/* If someone else was already registered, unregister them. */
	if (a->format_destroy != NULL)
		(a->format_destroy)(a);

	cpio = (struct cpio *)malloc(sizeof(*cpio));
	if (cpio == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Can't allocate cpio data");
		return (ARCHIVE_FATAL);
	}
	memset(cpio, 0, sizeof(*cpio));
	a->format_data = cpio;

	a->pad_uncompressed = 1;
	a->format_write_header = archive_write_cpio_header;
	a->format_write_data = archive_write_cpio_data;
	a->format_finish_entry = archive_write_cpio_finish_entry;
	a->format_finish = archive_write_cpio_finish;
	a->format_destroy = archive_write_cpio_destroy;
	a->archive_format = ARCHIVE_FORMAT_CPIO_POSIX;
	a->archive_format_name = "POSIX cpio";
	return (ARCHIVE_OK);
}

static int
archive_write_cpio_header(struct archive_write *a, struct archive_entry *entry)
{
	struct cpio *cpio;
	const char *p, *path;
	int pathlength, ret;
	struct cpio_header	 h;

	cpio = (struct cpio *)a->format_data;
	ret = 0;

	path = archive_entry_pathname(entry);
	pathlength = strlen(path) + 1; /* Include trailing null. */

	memset(&h, 0, sizeof(h));
	format_octal(070707, &h.c_magic, sizeof(h.c_magic));
	format_octal(archive_entry_dev(entry), &h.c_dev, sizeof(h.c_dev));
	/*
	 * TODO: Generate artificial inode numbers rather than just
	 * re-using the ones off the disk.  That way, the 18-bit c_ino
	 * field only limits the number of files in the archive.
	 */
	if (archive_entry_ino(entry) > 0777777) {
		archive_set_error(&a->archive, ERANGE, "large inode number truncated");
		ret = ARCHIVE_WARN;
	}

	format_octal(archive_entry_ino(entry) & 0777777, &h.c_ino, sizeof(h.c_ino));
	format_octal(archive_entry_mode(entry), &h.c_mode, sizeof(h.c_mode));
	format_octal(archive_entry_uid(entry), &h.c_uid, sizeof(h.c_uid));
	format_octal(archive_entry_gid(entry), &h.c_gid, sizeof(h.c_gid));
	format_octal(archive_entry_nlink(entry), &h.c_nlink, sizeof(h.c_nlink));
	if (archive_entry_filetype(entry) == AE_IFBLK
	    || archive_entry_filetype(entry) == AE_IFCHR)
	    format_octal(archive_entry_dev(entry), &h.c_rdev, sizeof(h.c_rdev));
	else
	    format_octal(0, &h.c_rdev, sizeof(h.c_rdev));
	format_octal(archive_entry_mtime(entry), &h.c_mtime, sizeof(h.c_mtime));
	format_octal(pathlength, &h.c_namesize, sizeof(h.c_namesize));

	/* Symlinks get the link written as the body of the entry. */
	p = archive_entry_symlink(entry);
	if (p != NULL  &&  *p != '\0')
		format_octal(strlen(p), &h.c_filesize, sizeof(h.c_filesize));
	else
		format_octal(archive_entry_size(entry), &h.c_filesize, sizeof(h.c_filesize));

	ret = (a->compressor.write)(a, &h, sizeof(h));
	if (ret != ARCHIVE_OK)
		return (ARCHIVE_FATAL);

	ret = (a->compressor.write)(a, path, pathlength);
	if (ret != ARCHIVE_OK)
		return (ARCHIVE_FATAL);

	cpio->entry_bytes_remaining = archive_entry_size(entry);

	/* Write the symlink now. */
	if (p != NULL  &&  *p != '\0')
		ret = (a->compressor.write)(a, p, strlen(p));

	return (ret);
}

static ssize_t
archive_write_cpio_data(struct archive_write *a, const void *buff, size_t s)
{
	struct cpio *cpio;
	int ret;

	cpio = (struct cpio *)a->format_data;
	if (s > cpio->entry_bytes_remaining)
		s = cpio->entry_bytes_remaining;

	ret = (a->compressor.write)(a, buff, s);
	cpio->entry_bytes_remaining -= s;
	if (ret >= 0)
		return (s);
	else
		return (ret);
}

/*
 * Format a number into the specified field.
 */
static int
format_octal(int64_t v, void *p, int digits)
{
	int64_t	max;
	int	ret;

	max = (((int64_t)1) << (digits * 3)) - 1;
	if (v >= 0  &&  v <= max) {
	    format_octal_recursive(v, (char *)p, digits);
	    ret = 0;
	} else {
	    format_octal_recursive(max, (char *)p, digits);
	    ret = -1;
	}
	return (ret);
}

static int64_t
format_octal_recursive(int64_t v, char *p, int s)
{
	if (s == 0)
		return (v);
	v = format_octal_recursive(v, p+1, s-1);
	*p = '0' + (v & 7);
	return (v >>= 3);
}

static int
archive_write_cpio_finish(struct archive_write *a)
{
	struct cpio *cpio;
	int er;
	struct archive_entry *trailer;

	cpio = (struct cpio *)a->format_data;
	trailer = archive_entry_new();
	archive_entry_set_nlink(trailer, 1);
	archive_entry_set_pathname(trailer, "TRAILER!!!");
	er = archive_write_cpio_header(a, trailer);
	archive_entry_free(trailer);
	return (er);
}

static int
archive_write_cpio_destroy(struct archive_write *a)
{
	struct cpio *cpio;

	cpio = (struct cpio *)a->format_data;
	free(cpio);
	a->format_data = NULL;
	return (ARCHIVE_OK);
}

static int
archive_write_cpio_finish_entry(struct archive_write *a)
{
	struct cpio *cpio;
	int to_write, ret;

	cpio = (struct cpio *)a->format_data;
	ret = ARCHIVE_OK;
	while (cpio->entry_bytes_remaining > 0) {
		to_write = cpio->entry_bytes_remaining < a->null_length ?
		    cpio->entry_bytes_remaining : a->null_length;
		ret = (a->compressor.write)(a, a->nulls, to_write);
		if (ret != ARCHIVE_OK)
			return (ret);
		cpio->entry_bytes_remaining -= to_write;
	}
	return (ret);
}
