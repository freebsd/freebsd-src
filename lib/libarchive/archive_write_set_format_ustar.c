/*-
 * Copyright (c) 2003-2004 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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

#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"

/* Constants are chosen so that 'term & 15' is number of termination bytes */
#define	TERM_BYTES(n)		((n) & 15)
#define	OCTAL_TERM_SPACE_NULL	0x12
#define	OCTAL_TERM_NULL_SPACE	0x22
#define	OCTAL_TERM_NULL		0x31
#define	OCTAL_TERM_SPACE	0x41
#define	OCTAL_TERM_NONE		0x50

struct ustar {
	uint64_t	entry_bytes_remaining;
	uint64_t	entry_padding;
	char		written;
};

/*
 * Define structure of POSIX 'ustar' tar header.
 */
struct archive_entry_header_ustar {
	char	name[100];
	char	mode[8];
	char	uid[8];
	char	gid[8];
	char	size[12];
	char	mtime[12];
	char	checksum[8];
	char	typeflag[1];
	char	linkname[100];
	char	magic[6]; /* For POSIX: "ustar\0" */
	char	version[2]; /* For POSIX: "00" */
	char	uname[32];
	char	gname[32];
	char	devmajor[8];
	char	devminor[8];
	char	prefix[155];
};

static int	archive_write_ustar_data(struct archive *a, const void *buff,
		    size_t s);
static int	archive_write_ustar_finish(struct archive *);
static int	archive_write_ustar_finish_entry(struct archive *);
static int	archive_write_ustar_header(struct archive *,
		    struct archive_entry *entry);
static int	format_octal(int64_t, char *, int, int term);
static int64_t	format_octal_recursive(int64_t, char *, int);
static int	write_nulls(struct archive *a, size_t);

/*
 * Set output format to 'ustar' format.
 */
int
archive_write_set_format_ustar(struct archive *a)
{
	struct ustar *ustar;

	/* If someone else was already registered, unregister them. */
	if (a->format_finish != NULL)
		(a->format_finish)(a);

	ustar = malloc(sizeof(*ustar));
	if (ustar == NULL) {
		archive_set_error(a, ENOMEM, "Can't allocate ustar data");
		return (ARCHIVE_FATAL);
	}
	memset(ustar, 0, sizeof(*ustar));
	a->format_data = ustar;

	a->pad_uncompressed = 1;	/* Mimic gtar in this respect. */
	a->format_write_header = archive_write_ustar_header;
	a->format_write_data = archive_write_ustar_data;
	a->format_finish = archive_write_ustar_finish;
	a->format_finish_entry = archive_write_ustar_finish_entry;
	a->archive_format = ARCHIVE_FORMAT_TAR_USTAR;
	a->archive_format_name = "POSIX ustar";
	return (ARCHIVE_OK);
}

static int
archive_write_ustar_header(struct archive *a, struct archive_entry *entry)
{
	char buff[512];
	int ret;
	struct ustar *ustar;

	ustar = a->format_data;
	ustar->written = 1;

	/* Only regular files (not hardlinks) have data. */
	if (archive_entry_hardlink(entry) != NULL ||
	    archive_entry_symlink(entry) != NULL ||
	    !S_ISREG(archive_entry_mode(entry)))
		archive_entry_set_size(entry, 0);

	ret = __archive_write_format_header_ustar(a, buff, entry, -1);
	if (ret != ARCHIVE_OK)
		return (ret);
	ret = (a->compression_write)(a, buff, 512);
	if (ret < 512)
		return (ARCHIVE_FATAL);

	ustar->entry_bytes_remaining = archive_entry_size(entry);
	ustar->entry_padding = 0x1ff & (- ustar->entry_bytes_remaining);
	return (ARCHIVE_OK);
}

/*
 * Format a basic 512-byte "ustar" header.
 *
 * Returns -1 if format failed (due to field overflow).
 * Note that this always formats as much of the header as possible.
 *
 * This is exported so that other 'tar' formats can use it.
 */
int
__archive_write_format_header_ustar(struct archive *a, char buff[512],
				    struct archive_entry *entry, int tartype)
{
	unsigned int checksum;
	struct archive_entry_header_ustar *h;
	int i, ret;
	const char *p, *pp;
	const struct stat *st;

	ret = 0;
	memset(buff, 0, 512);
	h = (struct archive_entry_header_ustar *)buff;

	/*
	 * Because the block is already null-filled, and strings
	 * are allowed to exactly fill their destination (without null),
	 * I use memcpy(dest, src, strlen()) here a lot to copy strings.
	 */

	pp = archive_entry_pathname(entry);
	if (strlen(pp) <= sizeof(h->name))
		memcpy(h->name, pp, strlen(pp));
	else {
		/* Store in two pieces, splitting at a '/'. */
		p = strchr(pp + strlen(pp) - sizeof(h->name) - 1, '/');
		/*
		 * If there is no path separator, or the prefix or
		 * remaining name are too large, return an error.
		 */
		if (!p) {
			archive_set_error(a, ENAMETOOLONG,
			    "Pathname too long");
			ret = ARCHIVE_WARN;
		} else if (p  > pp + sizeof(h->prefix)) {
			archive_set_error(a, ENAMETOOLONG,
			    "Pathname too long");
			ret = ARCHIVE_WARN;
		} else {
			/* Copy prefix and remainder to appropriate places */
			memcpy(h->prefix, pp, p - pp);
			memcpy(h->name, p + 1, pp + strlen(pp) - p - 1);
		}
	}

	p = archive_entry_hardlink(entry);
	if(p == NULL)
		p = archive_entry_symlink(entry);
	if (p != NULL && p[0] != '\0') {
		if (strlen(p) > sizeof(h->linkname)) {
			archive_set_error(a, ENAMETOOLONG,
			    "Link contents too long");
			ret = ARCHIVE_WARN;
		} else
			memcpy(h->linkname, p, strlen(p));
	}

	p = archive_entry_uname(entry);
	if (p != NULL && p[0] != '\0') {
		if (strlen(p) > sizeof(h->uname)) {
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Username too long");
			ret = ARCHIVE_WARN;
		} else
			memcpy(h->uname, p, strlen(p));
	}

	p = archive_entry_gname(entry);
	if (p != NULL && p[0] != '\0') {
		if (strlen(p) > sizeof(h->gname)) {
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Group name too long");
			ret = ARCHIVE_WARN;
		} else
			memcpy(h->gname, p, strlen(p));
	}

	st = archive_entry_stat(entry);

	if (format_octal(st->st_mode & 07777, h->mode, sizeof(h->mode),
		OCTAL_TERM_SPACE_NULL)) {
		archive_set_error(a, ERANGE, "Numeric mode too large");
		ret = ARCHIVE_WARN;
	}

	if (format_octal(st->st_uid, h->uid, sizeof(h->uid),
		OCTAL_TERM_SPACE_NULL)) {
		archive_set_error(a, ERANGE, "Numeric user ID too large");
		ret = ARCHIVE_WARN;
	}

	if (format_octal(st->st_gid, h->gid, sizeof(h->gid),
		OCTAL_TERM_SPACE_NULL)) {
		archive_set_error(a, ERANGE, "Numeric group ID too large");
		ret = ARCHIVE_WARN;
	}

	if (format_octal(st->st_size, h->size,
		sizeof(h->size), OCTAL_TERM_SPACE)) {
		archive_set_error(a, ERANGE, "File size too large");
		ret = ARCHIVE_WARN;
	}

	if (format_octal(st->st_mtime, h->mtime,
		sizeof(h->mtime), OCTAL_TERM_SPACE)) {
		archive_set_error(a, ERANGE,
		    "File modification time too large");
		ret = ARCHIVE_WARN;
	}

	if (S_ISBLK(st->st_mode) || S_ISCHR(st->st_mode)) {
		if (format_octal(major(st->st_rdev), h->devmajor,
			sizeof(h->devmajor), OCTAL_TERM_SPACE_NULL)) {
			archive_set_error(a, ERANGE,
			    "Major device number too large");
			ret = ARCHIVE_WARN;
		}

		if (format_octal(minor(st->st_rdev), h->devminor,
			sizeof(h->devminor), OCTAL_TERM_SPACE_NULL)) {
			archive_set_error(a, ERANGE,
			    "Minor device number too large");
			ret = ARCHIVE_WARN;
		}
	} else {
		format_octal(0, h->devmajor, sizeof(h->devmajor),
		    OCTAL_TERM_SPACE_NULL);
		format_octal(0, h->devminor, sizeof(h->devminor),
		    OCTAL_TERM_SPACE_NULL);
	}

	if (tartype >= 0) {
		h->typeflag[0] = tartype;
	} else if (archive_entry_hardlink(entry) != NULL) {
		h->typeflag[0] = '1';
	} else {
		switch (st->st_mode & S_IFMT) {
		case S_IFREG: h->typeflag[0] = '0' ; break;
		case S_IFLNK: h->typeflag[0] = '2' ; break;
		case S_IFCHR: h->typeflag[0] = '3' ; break;
		case S_IFBLK: h->typeflag[0] = '4' ; break;
		case S_IFDIR: h->typeflag[0] = '5' ; break;
		case S_IFIFO: h->typeflag[0] = '6' ; break;
		case S_IFSOCK:
			archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
			    "tar format cannot archive socket");
			ret = ARCHIVE_WARN;
			break;
		default:
			archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
			    "tar format cannot archive this");
			ret = ARCHIVE_WARN;
		}
	}

	memcpy(h->magic, "ustar\0", 6);
	memcpy(h->version, "00", 2);
	memcpy(h->checksum, "        ", 8);
	checksum = 0;
	for (i = 0; i < 512; i++)
		checksum += 255 & (unsigned int)buff[i];
	if (format_octal(checksum, h->checksum,
		sizeof(h->checksum), OCTAL_TERM_NULL_SPACE)) {
		archive_set_error(a, ERANGE,
		    "Checksum too large (Internal error; this can't happen)");
		ret = ARCHIVE_WARN;
	}

	return (ret);
}

/*
 * Format a number into the specified field.
 */
static int
format_octal(int64_t v, char *p, int s, int term)
{
	/* POSIX specifies that all numeric values are unsigned. */
	int64_t	max;
	int digits, ret;

	digits = s - TERM_BYTES(term);
	max = (((int64_t)1) << (digits * 3)) - 1;

	if (v >= 0  &&  v <= max) {
		format_octal_recursive(v, p, digits);
		ret = 0;
	} else {
		format_octal_recursive(max, p, digits);
		ret = -1;
	}

	switch (term) {
	case OCTAL_TERM_SPACE_NULL:
		p[s-2] = 0x20;
		/* fall through */
	case OCTAL_TERM_NULL:
		p[s-1] = 0;
		break;
	case OCTAL_TERM_NULL_SPACE:
		p[s-2] = 0;
		/* fall through */
	case OCTAL_TERM_SPACE:
		p[s-1] = 0x20;
		break;
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
archive_write_ustar_finish(struct archive *a)
{
	struct ustar *ustar;
	int r;

	r = ARCHIVE_OK;
	ustar = a->format_data;
	/*
	 * Suppress end-of-archive if nothing else was ever written.
	 * This fixes a problem where setting one format, then another
	 * ends up writing a gratuitous end-of-archive marker.
	 */
	if (ustar->written && a->compression_write != NULL)
		r = write_nulls(a, 512*2);
	free(ustar);
	a->format_data = NULL;
	return (r);
}

static int
archive_write_ustar_finish_entry(struct archive *a)
{
	struct ustar *ustar;
	int ret;

	ustar = a->format_data;
	ret = write_nulls(a,
	    ustar->entry_bytes_remaining + ustar->entry_padding);
	ustar->entry_bytes_remaining = ustar->entry_padding = 0;
	return (ret);
}

static int
write_nulls(struct archive *a, size_t padding)
{
	int ret, to_write;

	while (padding > 0) {
		to_write = padding < a->null_length ? padding : a->null_length;
		ret = (a->compression_write)(a, a->nulls, to_write);
		if (ret < to_write)
			return (ARCHIVE_FATAL);
		padding -= to_write;
	}
	return (ARCHIVE_OK);
}

static int
archive_write_ustar_data(struct archive *a, const void *buff, size_t s)
{
	struct ustar *ustar;
	int ret;

	ustar = a->format_data;
	if (s > ustar->entry_bytes_remaining)
		s = ustar->entry_bytes_remaining;
	ret = (a->compression_write)(a, buff, s);
	ustar->entry_bytes_remaining -= s;
	return (ret);
}
