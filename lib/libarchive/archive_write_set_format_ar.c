/*-
 * Copyright (c) 2007 Kai Wang
 * Copyright (c) 2007 Tim Kientzle
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

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
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
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_write_private.h"

struct ar_w {
	uint64_t	 entry_bytes_remaining;
	uint64_t	 entry_padding;
	int		 is_strtab;
	int		 has_strtab;
	char		*strtab;
};

/*
 * Define structure of the "ar" header.
 */
#define AR_name_offset 0
#define AR_name_size 16
#define AR_date_offset 16
#define AR_date_size 12
#define AR_uid_offset 28
#define AR_uid_size 6
#define AR_gid_offset 34
#define AR_gid_size 6
#define AR_mode_offset 40
#define AR_mode_size 8
#define AR_size_offset 48
#define AR_size_size 10
#define AR_fmag_offset 58
#define AR_fmag_size 2

/*
 * "ar" magic numbers.
 */
#define	ARMAG		"!<arch>\n"
#define	SARMAG		8		/* strlen(ARMAG); */
#define	AR_EFMT1	"#1/"
#define	SAR_EFMT1	3		/* strlen(AR_EFMT1); */
#define	ARFMAG		"`\n"
#define	SARFMAG		2		/* strlen(ARFMAG); */

static int	__archive_write_set_format_ar(struct archive_write *);
static int	archive_write_ar_header(struct archive_write *,
		    struct archive_entry *);
static ssize_t	archive_write_ar_data(struct archive_write *, const void *buff,
		    size_t s);
static int	archive_write_ar_destroy(struct archive_write *);
static int	archive_write_ar_finish_entry(struct archive_write *);
static int	format_octal(int64_t v, char *p, int s);
static int	format_decimal(int64_t v, char *p, int s);

int
archive_write_set_format_ar_bsd(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	int r = __archive_write_set_format_ar(a);
	if (r == ARCHIVE_OK) {
		a->archive_format = ARCHIVE_FORMAT_AR_BSD;
		a->archive_format_name = "ar (BSD)";
	}
	return (r);
}

int
archive_write_set_format_ar_svr4(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	int r = __archive_write_set_format_ar(a);
	if (r == ARCHIVE_OK) {
		a->archive_format = ARCHIVE_FORMAT_AR_SVR4;
		a->archive_format_name = "ar (GNU/SVR4)";
	}
	return (r);
}

/*
 * Generic initialization.
 */
static int
__archive_write_set_format_ar(struct archive_write *a)
{
	struct ar_w *ar;

	/* If someone else was already registered, unregister them. */
	if (a->format_destroy != NULL)
		(a->format_destroy)(a);

	ar = (struct ar_w *)malloc(sizeof(*ar));
	if (ar == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Can't allocate ar data");
		return (ARCHIVE_FATAL);
	}
	memset(ar, 0, sizeof(*ar));
	a->format_data = ar;

	a->format_write_header = archive_write_ar_header;
	a->format_write_data = archive_write_ar_data;
	a->format_finish = NULL;
	a->format_destroy = archive_write_ar_destroy;
	a->format_finish_entry = archive_write_ar_finish_entry;
	return (ARCHIVE_OK);
}

static int
archive_write_ar_header(struct archive_write *a, struct archive_entry *entry)
{
	int ret, append_fn;
	char buff[60];
	char *ss, *se;
	struct ar_w *ar;
	const char *pp;
	const struct stat *st;

	ret = 0;
	append_fn = 0;
	ar = (struct ar_w *)a->format_data;
	ar->is_strtab = 0;

	if (a->archive.file_position == 0) {
		/*
		 * We are now at the beginning of the archive,
		 * so we need first write the ar global header.
		 */
		(a->compression_write)(a, ARMAG, SARMAG);
	}

	memset(buff, ' ', 60);
	strncpy(&buff[AR_fmag_offset], ARFMAG, SARFMAG);

	pp = archive_entry_pathname(entry);

	if (strcmp(pp, "/") == 0 ) {
		/* Entry is archive symbol table in GNU format */
		buff[AR_name_offset] = '/';
		goto stat;
	}
	if (strcmp(pp, "__.SYMDEF") == 0) {
		/* Entry is archive symbol table in BSD format */
		strncpy(buff + AR_name_offset, "__.SYMDEF", 9);
		goto stat;
	}
	if (strcmp(pp, "//") == 0) {
		/*
		 * Entry is archive string table, inform that we should
		 * collect strtab in next _data call.
		 */
		ar->is_strtab = 1;
		buff[AR_name_offset] = buff[AR_name_offset + 1] = '/';
		/*
		 * For archive string table, only ar_size filed should
		 * be set.
		 */
		goto size;
	}

	/* Otherwise, entry is a normal archive member. */
	if (a->archive_format == ARCHIVE_FORMAT_AR_SVR4) {
		/*
		 * SVR4/GNU variant use a "/" to mark then end of the filename,
		 * make it possible to have embedded spaces in the filename.
		 * So, the longest filename here (without extension) is
		 * actually 15 bytes.
		 */
		if (strlen(pp) <= 15) {
			strncpy(&buff[AR_name_offset], pp, strlen(pp));
			buff[AR_name_offset + strlen(pp)] = '/';
		} else {
			/*
			 * For filename longer than 15 bytes, GNU variant
			 * makes use of a string table and instead stores the
			 * offset of the real filename to in the ar_name field.
			 * The string table should have been written before.
			 */
			if (ar->has_strtab <= 0) {
				archive_set_error(&a->archive, EINVAL,
				    "Can't find string table");
				return (ARCHIVE_WARN);
			}

			se = (char *)malloc(strlen(pp) + 3);
			if (se == NULL) {
				archive_set_error(&a->archive, ENOMEM,
				    "Can't allocate filename buffer");
				return (ARCHIVE_FATAL);
			}

			strncpy(se, pp, strlen(pp));
			strcpy(se + strlen(pp), "/\n");

			ss = strstr(ar->strtab, se);
			free(se);

			if (ss == NULL) {
				archive_set_error(&a->archive, EINVAL,
				    "Invalid string table");
				return (ARCHIVE_WARN);
			}

			/*
			 * GNU variant puts "/" followed by digits into
			 * ar_name field. These digits indicates the real
			 * filename string's offset to the string table.
			 */
			buff[AR_name_offset] = '/';
			if (format_decimal(ss - ar->strtab,
			    buff + AR_name_offset + 1,
			    AR_name_size - 1)) {
				archive_set_error(&a->archive, ERANGE,
				    "string table offset too large");
				return (ARCHIVE_WARN);
			}
		}
	} else if (a->archive_format == ARCHIVE_FORMAT_AR_BSD) {
		/*
		 * BSD variant: for any file name which is more than
		 * 16 chars or contains one or more embedded space(s), the
		 * string "#1/" followed by the ASCII length of the name is
		 * put into the ar_name field. The file size (stored in the
		 * ar_size field) is incremented by the length of the name.
		 * The name is then written immediately following the
		 * archive header.
		 */
		if (strlen(pp) <= 16 && strchr(pp, ' ') == NULL) {
			strncpy(&buff[AR_name_offset], pp, strlen(pp));
			buff[AR_name_offset + strlen(pp)] = ' ';
		}
		else {
			strncpy(buff + AR_name_offset, AR_EFMT1, SAR_EFMT1);
			if (format_decimal(strlen(pp),
			    buff + AR_name_offset + SAR_EFMT1,
			    AR_name_size - SAR_EFMT1)) {
				archive_set_error(&a->archive, ERANGE,
				    "File name too long");
				return (ARCHIVE_WARN);
			}
			append_fn = 1;
			archive_entry_set_size(entry,
			    archive_entry_size(entry) + strlen(pp));
		}
	}

stat:
	st = archive_entry_stat(entry);
	if (format_decimal(st->st_mtime, buff + AR_date_offset, AR_date_size)) {
		archive_set_error(&a->archive, ERANGE,
		    "File modification time too large");
		return (ARCHIVE_WARN);
	}
	if (format_decimal(st->st_uid, buff + AR_uid_offset, AR_uid_size)) {
		archive_set_error(&a->archive, ERANGE,
		    "Numeric user ID too large");
		return (ARCHIVE_WARN);
	}
	if (format_decimal(st->st_gid, buff + AR_gid_offset, AR_gid_size)) {
		archive_set_error(&a->archive, ERANGE,
		    "Numeric group ID too large");
		return (ARCHIVE_WARN);
	}
	if (format_octal(st->st_mode, buff + AR_mode_offset, AR_mode_size)) {
		archive_set_error(&a->archive, ERANGE,
		    "Numeric mode too large");
		return (ARCHIVE_WARN);
	}

size:
	if (format_decimal(archive_entry_size(entry), buff + AR_size_offset,
	    AR_size_size)) {
		archive_set_error(&a->archive, ERANGE,
		    "File size out of range");
		return (ARCHIVE_WARN);
	}

	ret = (a->compression_write)(a, buff, 60);
	if (ret != ARCHIVE_OK)
		return (ret);

	ar->entry_bytes_remaining = archive_entry_size(entry);
	ar->entry_padding = ar->entry_bytes_remaining % 2;

	if (append_fn > 0) {
		ret = (a->compression_write)(a, pp, strlen(pp));
		if (ret != ARCHIVE_OK)
			return (ret);
		ar->entry_bytes_remaining -= strlen(pp);
	}

	return (ARCHIVE_OK);
}

static ssize_t
archive_write_ar_data(struct archive_write *a, const void *buff, size_t s)
{
	struct ar_w *ar;
	int ret;

	ar = (struct ar_w *)a->format_data;
	if (s > ar->entry_bytes_remaining)
		s = ar->entry_bytes_remaining;

	if (ar->is_strtab > 0) {
		if (ar->has_strtab > 0) {
			archive_set_error(&a->archive, EINVAL,
			    "More than one string tables exist");
			return (ARCHIVE_WARN);
		}

		ar->strtab = (char *)malloc(s);
		if (ar->strtab == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't allocate strtab buffer");
			return (ARCHIVE_FATAL);
		}
		strncpy(ar->strtab, buff, s);
		ar->has_strtab = 1;
	}

	ret = (a->compression_write)(a, buff, s);
	if (ret != ARCHIVE_OK)
		return (ret);

	ar->entry_bytes_remaining -= s;
	return (s);
}

static int
archive_write_ar_destroy(struct archive_write *a)
{
	struct ar_w *ar;

	ar = (struct ar_w *)a->format_data;

	if (ar->has_strtab > 0) {
		free(ar->strtab);
		ar->strtab = NULL;
	}

	free(ar);
	a->format_data = NULL;
	return (ARCHIVE_OK);
}

static int
archive_write_ar_finish_entry(struct archive_write *a)
{
	struct ar_w *ar;
	int ret;

	ar = (struct ar_w *)a->format_data;

	if (ar->entry_bytes_remaining != 0) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Entry remaining bytes larger than 0");
		return (ARCHIVE_WARN);
	}

	if (ar->entry_padding == 0) {
		return (ARCHIVE_OK);
	}

	if (ar->entry_padding != 1) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Padding wrong size: %d should be 1 or 0",
		    ar->entry_padding);
		return (ARCHIVE_WARN);
	}

	ret = (a->compression_write)(a, "\n", 1);
	return (ret);
}

/*
 * Format a number into the specified field using base-8.
 * NB: This version is slightly different from the one in
 * _ustar.c
 */
static int
format_octal(int64_t v, char *p, int s)
{
	int len;
	char *h;

	len = s;
	h = p;

	/* Octal values can't be negative, so use 0. */
	if (v < 0) {
		while (len-- > 0)
			*p++ = '0';
		return (-1);
	}

	p += s;		/* Start at the end and work backwards. */
	do {
		*--p = (char)('0' + (v & 7));
		v >>= 3;
	} while (--s > 0 && v > 0);

	if (v == 0) {
		memmove(h, p, len - s);
		p = h + len - s;
		while (s-- > 0)
			*p++ = ' ';
		return (0);
	}
	/* If it overflowed, fill field with max value. */
	while (len-- > 0)
		*p++ = '7';

	return (-1);
}

/*
 * Format a number into the specified field using base-10.
 */
static int
format_decimal(int64_t v, char *p, int s)
{
	int len;
	char *h;

	len = s;
	h = p;

	/* Negative values in ar header are meaningless , so use 0. */
	if (v < 0) {
		while (len-- > 0)
			*p++ = '0';
		return (-1);
	}

	p += s;
	do {
		*--p = (char)('0' + (v % 10));
		v /= 10;
	} while (--s > 0 && v > 0);

	if (v == 0) {
		memmove(h, p, len - s);
		p = h + len - s;
		while (s-- > 0)
			*p++ = ' ';
		return (0);
	}
	/* If it overflowed, fill field with max value. */
	while (len-- > 0)
		*p++ = '9';

	return (-1);
}
