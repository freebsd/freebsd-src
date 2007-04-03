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
#include "archive_read_private.h"

struct ar {
	int	 bid;
	int	 has_strtab;
	off_t	 entry_bytes_remaining;
	off_t	 entry_offset;
	off_t	 entry_padding;
	char	*strtab;
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

#define isdigit(x)	(x) >= '0' && (x) <= '9'

static int	archive_read_format_ar_bid(struct archive_read *a);
static int	archive_read_format_ar_cleanup(struct archive_read *a);
static int	archive_read_format_ar_read_data(struct archive_read *a,
		    const void **buff, size_t *size, off_t *offset);
static int	archive_read_format_ar_skip(struct archive_read *a);
static int	archive_read_format_ar_read_header(struct archive_read *a,
		    struct archive_entry *e);
static int64_t	ar_atol8(const char *p, unsigned char_cnt);
static int64_t	ar_atol10(const char *p, unsigned char_cnt);
static int	ar_parse_string_table(struct archive_read *, struct ar *,
		    const void *, ssize_t);

/*
 * ANSI C99 defines constants for these, but not everyone supports
 * those constants, so I define a couple of static variables here and
 * compute the values.  These calculations should be portable to any
 * 2s-complement architecture.
 */
#ifdef UINT64_MAX
static const uint64_t max_uint64 = UINT64_MAX;
#else
static const uint64_t max_uint64 = ~(uint64_t)0;
#endif
#ifdef INT64_MAX
static const int64_t max_int64 = INT64_MAX;
#else
static const int64_t max_int64 = (int64_t)((~(uint64_t)0) >> 1);
#endif
#ifdef INT64_MIN
static const int64_t min_int64 = INT64_MIN;
#else
static const int64_t min_int64 = (int64_t)(~((~(uint64_t)0) >> 1));
#endif

int
archive_read_support_format_ar(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct ar *ar;
	int r;

	ar = (struct ar *)malloc(sizeof(*ar));
	if (ar == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate ar data");
		return (ARCHIVE_FATAL);
	}
	memset(ar, 0, sizeof(*ar));
	ar->bid = -1;

	r = __archive_read_register_format(a,
	    ar,
	    archive_read_format_ar_bid,
	    archive_read_format_ar_read_header,
	    archive_read_format_ar_read_data,
	    archive_read_format_ar_skip,
	    archive_read_format_ar_cleanup);

	if (r != ARCHIVE_OK) {
		free(ar);
		return (r);
	}
	return (ARCHIVE_OK);
}

static int
archive_read_format_ar_cleanup(struct archive_read *a)
{
	struct ar *ar;

	ar = (struct ar *)*(a->pformat_data);
	if (ar->has_strtab > 0)
		free(ar->strtab);
	free(ar);
	*(a->pformat_data) = NULL;
	return (ARCHIVE_OK);
}

static int
archive_read_format_ar_bid(struct archive_read *a)
{
	struct ar *ar;
	ssize_t bytes_read;
	const void *h;

	if (a->archive.archive_format != 0 &&
	    (a->archive.archive_format & ARCHIVE_FORMAT_BASE_MASK) !=
	    ARCHIVE_FORMAT_AR)
		return(0);

	ar = (struct ar *)*(a->pformat_data);

	if (ar->bid > 0)
		return (ar->bid);

	bytes_read = (a->compression_read_ahead)(a, &h, SARMAG);
	if (bytes_read < SARMAG)
		return (-1);

	/*
	 * Verify the global header.
	 * TODO: Do we need to check more than this?
	 */
	if (strncmp((const char*)h, ARMAG, SARMAG) == 0) {
		ar->bid = SARMAG;
		return (ar->bid);
	}
	return (-1);
}

static int
archive_read_format_ar_read_header(struct archive_read *a,
    struct archive_entry *entry)
{
	int r, bsd_append;
	ssize_t bytes;
	int64_t nval;
	char *fname, *p;
	struct ar *ar;
	const void *b;
	const char *h;

	bsd_append = 0;

	if (!a->archive.archive_format) {
		a->archive.archive_format = ARCHIVE_FORMAT_AR;
		a->archive.archive_format_name = "Unix Archiver";
	}

	if (a->archive.file_position == 0) {
		/*
		 * We are now at the beginning of the archive,
		 * so we need first consume the ar global header.
		 */
		(a->compression_read_consume)(a, SARMAG);
	}

	/* Read 60-byte header */
	bytes = (a->compression_read_ahead)(a, &b, 60);
	if (bytes < 60) {
		/*
		 * We just encountered an incomplete ar file,
		 * though the _bid function accepted it.
		 */
		return (ARCHIVE_EOF);
	}
	(a->compression_read_consume)(a, 60);

	h = (const char *)b;

	/* Consistency check */
	if (strncmp(h + AR_fmag_offset, ARFMAG, SARFMAG) != 0) {
		archive_set_error(&a->archive, EINVAL,
		    "Consistency check failed");
		return (ARCHIVE_WARN);
	}

	ar = (struct ar*)*(a->pformat_data);

	if (strncmp(h + AR_name_offset, "//", 2) == 0) {
		/*
		 * An archive member with ar_name "//" is an archive
		 * string table.
		 */
		nval = ar_atol10(h + AR_size_offset, AR_size_size);
		bytes = (a->compression_read_ahead)(a, &b, nval);
		if (bytes <= 0)
			return (ARCHIVE_FATAL);
		if (bytes < nval) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Truncated input file");
			return (ARCHIVE_FATAL);
		}

		r = ar_parse_string_table(a, ar, b, nval);
		if (r == ARCHIVE_OK) {
			/*
			 * Archive string table only have ar_name and ar_size fileds
			 * in its header.
			 */
			archive_entry_copy_pathname(entry, "//");
			nval = ar_atol10(h + AR_size_offset, AR_size_size);
			archive_entry_set_size(entry, nval);

			ar->entry_offset = 0;
			ar->entry_bytes_remaining = nval;
			ar->entry_padding = ar->entry_bytes_remaining % 2;
		}
		return (r);
	}

	if (h[AR_name_offset] == '/' && isdigit(h[AR_name_offset + 1])) {
		/*
		 * Archive member is common format with SVR4/GNU variant.
		 * "/" followed by one or more digit(s) in the ar_name
		 * filed indicates an index to the string table.
		 */
		if (ar->has_strtab > 0) {
			nval = ar_atol10(h + AR_name_offset + 1,
			    AR_name_size - 1);
			archive_entry_copy_pathname(entry, &ar->strtab[nval]);
		} else {
			archive_set_error(&a->archive, EINVAL,
			    "String table does not exist");
			return (ARCHIVE_WARN);
		}
		goto remain;
	}

	if (strncmp(h + AR_name_offset, AR_EFMT1, SAR_EFMT1) == 0) {
		/*
		 * Archive member is common format with BSD variant.
		 * AR_EFMT1 is followed by one or more digit(s) indicating
		 * the length of the real filename which is appended
		 * to the header.
		 */
		nval = ar_atol10(h + AR_name_offset + SAR_EFMT1,
		    AR_name_size - SAR_EFMT1);
		bytes = (a->compression_read_ahead)(a, &b, nval);
		if (bytes <= 0)
			return (ARCHIVE_FATAL);
		if (bytes < nval) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Truncated input file");
			return (ARCHIVE_FATAL);
		}

		(a->compression_read_consume)(a, nval);

		fname = (char *)malloc(nval + 1);
		if (fname == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't allocate fname buffer");
			return (ARCHIVE_FATAL);
		}
		strncpy(fname, b, nval);
		fname[nval] = '\0';
		archive_entry_copy_pathname(entry, fname);
		free(fname);
		fname = NULL;

		bsd_append = nval;
		goto remain;
	}

	/*
	 * "/" followed by one or more spaces indicate a
	 * SVR4/GNU archive symbol table.
	 *
	 */
	if (strncmp(h + AR_name_offset, "/ ", 2) == 0) {
		archive_entry_copy_pathname(entry, "/");
		goto remain;
	}
	/*
	 * "__.SYMDEF" indicates a BSD archive symbol table.
	 */
	if (strncmp(h + AR_name_offset, "__.SYMDEF", 9) == 0) {
		archive_entry_copy_pathname(entry, "__.SYMDEF");
		goto remain;
	}

	/*
	 * Otherwise, the ar_name fields stores the real
	 * filename.
	 * SVR4/GNU variant append a '/' to mark the end of
	 * filename, while BSD variant use a space.
	 */
	fname = (char *)malloc(AR_name_size + 1);
	strncpy(fname, h + AR_name_offset, AR_name_size);
	fname[AR_name_size] = '\0';

	if ((p = strchr(fname, '/')) != NULL) {
		/* SVR4/GNU format */
		*p = '\0';
		archive_entry_copy_pathname(entry, fname);
		free(fname);
		fname = NULL;
		goto remain;
	}

	/* BSD format */
	if ((p = strchr(fname, ' ')) != NULL)
		*p = '\0';
	archive_entry_copy_pathname(entry, fname);
	free(fname);
	fname = NULL;

remain:
	/* Copy remaining header */
	archive_entry_set_mtime(entry,
	    ar_atol10(h + AR_date_offset, AR_date_size), 0);
	archive_entry_set_uid(entry,
	    ar_atol10(h + AR_uid_offset, AR_uid_size));
	archive_entry_set_gid(entry,
	    ar_atol10(h + AR_gid_offset, AR_gid_size));
	archive_entry_set_mode(entry,
	    ar_atol8(h + AR_mode_offset, AR_mode_size));
	nval = ar_atol10(h + AR_size_offset, AR_size_size);

	ar->entry_offset = 0;
	ar->entry_padding = nval % 2;

	/*
	 * For BSD variant, we should subtract the length of
	 * the appended filename string from ar_size to get the
	 * real file size. But remember we should do this only
	 * after we had calculated the padding.
	 */
	if (bsd_append > 0)
		nval -= bsd_append;

	archive_entry_set_size(entry, nval);
	ar->entry_bytes_remaining = nval;

	return (ARCHIVE_OK);
}

static int
archive_read_format_ar_read_data(struct archive_read *a,
    const void **buff, size_t *size, off_t *offset)
{
	ssize_t bytes_read;
	struct ar *ar;

	ar = (struct ar *)*(a->pformat_data);

	if (ar->entry_bytes_remaining > 0) {
		bytes_read = (a->compression_read_ahead)(a, buff, 1);
		if (bytes_read == 0) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Truncated ar archive");
			return (ARCHIVE_FATAL);
		}
		if (bytes_read < 0)
			return (ARCHIVE_FATAL);
		if (bytes_read > ar->entry_bytes_remaining)
			bytes_read = ar->entry_bytes_remaining;
		*size = bytes_read;
		*offset = ar->entry_offset;
		ar->entry_offset += bytes_read;
		ar->entry_bytes_remaining -= bytes_read;
		(a->compression_read_consume)(a, bytes_read);
		return (ARCHIVE_OK);
	} else {
		while (ar->entry_padding > 0) {
			bytes_read = (a->compression_read_ahead)(a, buff, 1);
			if (bytes_read <= 0)
				return (ARCHIVE_FATAL);
			if (bytes_read > ar->entry_padding)
				bytes_read = ar->entry_padding;
			(a->compression_read_consume)(a, bytes_read);
			ar->entry_padding -= bytes_read;
		}
		*buff = NULL;
		*size = 0;
		*offset = ar->entry_offset;
		return (ARCHIVE_EOF);
	}
}

static int
archive_read_format_ar_skip(struct archive_read *a)
{
	off_t bytes_skipped;
	struct ar* ar;
	int r = ARCHIVE_OK;
	const void *b;		/* Dummy variables */
	size_t s;
	off_t o;

	ar = (struct ar *)*(a->pformat_data);
	if (a->compression_skip == NULL) {
		while (r == ARCHIVE_OK)
			r = archive_read_format_ar_read_data(a, &b, &s, &o);
		return (r);
	}

	bytes_skipped = (a->compression_skip)(a, ar->entry_bytes_remaining +
	    ar->entry_padding);
	if (bytes_skipped < 0)
		return (ARCHIVE_FATAL);

	ar->entry_bytes_remaining = 0;
	ar->entry_padding = 0;

	return (ARCHIVE_OK);
}

static int
ar_parse_string_table(struct archive_read *a, struct ar *ar,
    const void *h, ssize_t size)
{
	char *p;

	if (ar->has_strtab > 0) {
		archive_set_error(&a->archive, EINVAL,
		    "More than one string tables exist");
		return (ARCHIVE_WARN);
	}

	ar->strtab = (char *)malloc(size);
	if (ar->strtab == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate string table buffer");
		return (ARCHIVE_FATAL);
	}

	(void)memcpy(ar->strtab, h, size);
	p = ar->strtab;
	while (p < ar->strtab + size - 1) {
		if (*p == '/') {
			*p++ = '\0';
			if (*p == '\n')
				*p++ = '\0';
			else {
				archive_set_error(&a->archive, EINVAL,
				    "Invalid string table");
				free(ar->strtab);
				return (ARCHIVE_WARN);
			}
		} else
			p++;
	}
	/*
	 * Sanity check, last two chars must be `/\n' or '\n\n',
	 * depending on whether the string table is padded by a '\n'
	 * (string table produced by GNU ar always has a even size).
	 */
	if (p != ar->strtab + size && *p != '\n') {
		archive_set_error(&a->archive, EINVAL,
		    "Invalid string table");
		free(ar->strtab);
		return (ARCHIVE_WARN);
	}

	ar->has_strtab = 1;
	return (ARCHIVE_OK);
}

static int64_t
ar_atol8(const char *p, unsigned char_cnt)
{
	int64_t	l, limit, last_digit_limit;
	int digit, sign, base;

	base = 8;
	limit = max_int64 / base;
	last_digit_limit = max_int64 % base;

	while (*p == ' ' || *p == '\t')
		p++;
	if (*p == '-') {
		sign = -1;
		p++;
	} else
		sign = 1;

	l = 0;
	digit = *p - '0';
	while (digit >= 0 && digit < base  && char_cnt-- > 0) {
		if (l>limit || (l == limit && digit > last_digit_limit)) {
			l = max_uint64; /* Truncate on overflow. */
			break;
		}
		l = (l * base) + digit;
		digit = *++p - '0';
	}
	return (sign < 0) ? -l : l;
}

static int64_t
ar_atol10(const char *p, unsigned char_cnt)
{
	int64_t l, limit, last_digit_limit;
	int base, digit, sign;

	base = 10;
	limit = max_int64 / base;
	last_digit_limit = max_int64 % base;

	while (*p == ' ' || *p == '\t')
		p++;
	if (*p == '-') {
		sign = -1;
		p++;
	} else
		sign = 1;

	l = 0;
	digit = *p - '0';
	while (digit >= 0 && digit < base  && char_cnt-- > 0) {
		if (l > limit || (l == limit && digit > last_digit_limit)) {
			l = max_uint64; /* Truncate on overflow. */
			break;
		}
		l = (l * base) + digit;
		digit = *++p - '0';
	}
	return (sign < 0) ? -l : l;
}
