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
#include "archive_read_private.h"

struct ar {
	int	 bid;
	off_t	 entry_bytes_remaining;
	off_t	 entry_offset;
	off_t	 entry_padding;
	char	*strtab;
	size_t	 strtab_size;
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

#define isdigit(x)	(x) >= '0' && (x) <= '9'

static int	archive_read_format_ar_bid(struct archive_read *a);
static int	archive_read_format_ar_cleanup(struct archive_read *a);
static int	archive_read_format_ar_read_data(struct archive_read *a,
		    const void **buff, size_t *size, off_t *offset);
static int	archive_read_format_ar_skip(struct archive_read *a);
static int	archive_read_format_ar_read_header(struct archive_read *a,
		    struct archive_entry *e);
static uint64_t	ar_atol8(const char *p, unsigned char_cnt);
static uint64_t	ar_atol10(const char *p, unsigned char_cnt);
static int	ar_parse_gnu_filename_table(struct archive_read *, struct ar *,
		    const void *, size_t);
static int	ar_parse_common_header(struct ar *ar, struct archive_entry *,
		    const char *h);

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
	ar->strtab = NULL;

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

	ar = (struct ar *)(a->format->data);
	if (ar->strtab)
		free(ar->strtab);
	free(ar);
	(a->format->data) = NULL;
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

	ar = (struct ar *)(a->format->data);

	if (ar->bid > 0)
		return (ar->bid);

	/*
	 * Verify the 8-byte file signature.
	 * TODO: Do we need to check more than this?
	 */
	bytes_read = (a->decompressor->read_ahead)(a, &h, 8);
	if (bytes_read < 8)
		return (-1);
	if (strncmp((const char*)h, "!<arch>\n", 8) == 0) {
		ar->bid = 64;
		return (ar->bid);
	}
	return (-1);
}

static int
archive_read_format_ar_read_header(struct archive_read *a,
    struct archive_entry *entry)
{
	char filename[AR_name_size + 1];
	struct ar *ar;
	uint64_t number; /* Used to hold parsed numbers before validation. */
	ssize_t bytes_read;
	size_t bsd_name_length, entry_size;
	char *p;
	const void *b;
	const char *h;
	int r;

	ar = (struct ar*)(a->format->data);

	if (a->archive.file_position == 0) {
		/*
		 * We are now at the beginning of the archive,
		 * so we need first consume the ar global header.
		 */
		(a->decompressor->consume)(a, 8);
		/* Set a default format code for now. */
		a->archive.archive_format = ARCHIVE_FORMAT_AR;
	}

	/* Read the header for the next file entry. */
	bytes_read = (a->decompressor->read_ahead)(a, &b, 60);
	if (bytes_read < 60) {
		/* Broken header. */
		return (ARCHIVE_EOF);
	}
	(a->decompressor->consume)(a, 60);
	h = (const char *)b;

	/* Verify the magic signature on the file header. */
	if (strncmp(h + AR_fmag_offset, "`\n", 2) != 0) {
		archive_set_error(&a->archive, EINVAL,
		    "Consistency check failed");
		return (ARCHIVE_WARN);
	}

	/* Copy filename into work buffer. */
	strncpy(filename, h + AR_name_offset, AR_name_size);
	filename[AR_name_size] = '\0';

	/*
	 * Guess the format variant based on the filename.
	 */
	if (a->archive.archive_format == ARCHIVE_FORMAT_AR) {
		/* We don't already know the variant, so let's guess. */
		/*
		 * Biggest clue is presence of '/': GNU starts special
		 * filenames with '/', appends '/' as terminator to
		 * non-special names, so anything with '/' should be
		 * GNU except for BSD long filenames.
		 */
		if (strncmp(filename, "#1/", 3) == 0)
			a->archive.archive_format = ARCHIVE_FORMAT_AR_BSD;
		else if (strchr(filename, '/') != NULL)
			a->archive.archive_format = ARCHIVE_FORMAT_AR_GNU;
		else if (strncmp(filename, "__.SYMDEF", 9) == 0)
			a->archive.archive_format = ARCHIVE_FORMAT_AR_BSD;
		/*
		 * XXX Do GNU/SVR4 'ar' programs ever omit trailing '/'
		 * if name exactly fills 16-byte field?  If so, we
		 * can't assume entries without '/' are BSD. XXX
		 */
	}

	/* Update format name from the code. */
	if (a->archive.archive_format == ARCHIVE_FORMAT_AR_GNU)
		a->archive.archive_format_name = "ar (GNU/SVR4)";
	else if (a->archive.archive_format == ARCHIVE_FORMAT_AR_BSD)
		a->archive.archive_format_name = "ar (BSD)";
	else
		a->archive.archive_format_name = "ar";

	/*
	 * Remove trailing spaces from the filename.  GNU and BSD
	 * variants both pad filename area out with spaces.
	 * This will only be wrong if GNU/SVR4 'ar' implementations
	 * omit trailing '/' for 16-char filenames and we have
	 * a 16-char filename that ends in ' '.
	 */
	p = filename + AR_name_size - 1;
	while (p >= filename && *p == ' ') {
		*p = '\0';
		p--;
	}

	/*
	 * Remove trailing slash unless first character is '/'.
	 * (BSD entries never end in '/', so this will only trim
	 * GNU-format entries.  GNU special entries start with '/'
	 * and are not terminated in '/', so we don't trim anything
	 * that starts with '/'.)
	 */
	if (filename[0] != '/' && *p == '/')
		*p = '\0';

	/*
	 * '//' is the GNU filename table.
	 * Later entries can refer to names in this table.
	 */
	if (strcmp(filename, "//") == 0) {
		/* This must come before any call to _read_ahead. */
		ar_parse_common_header(ar, entry, h);
		archive_entry_copy_pathname(entry, filename);
		archive_entry_set_mode(entry,
		    S_IFREG | (archive_entry_mode(entry) & 0777));
		/* Get the size of the filename table. */
		number = ar_atol10(h + AR_size_offset, AR_size_size);
		if (number > SIZE_MAX) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Filename table too large");
			return (ARCHIVE_FATAL);
		}
		entry_size = (size_t)number;
		/* Read the filename table into memory. */
		bytes_read = (a->decompressor->read_ahead)(a, &b, entry_size);
		if (bytes_read <= 0)
			return (ARCHIVE_FATAL);
		if ((size_t)bytes_read < entry_size) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Truncated input file");
			return (ARCHIVE_FATAL);
		}
		/*
		 * Don't consume the contents, so the client will
		 * also get a shot at reading it.
		 */

		/* Parse the filename table. */
		return (ar_parse_gnu_filename_table(a, ar, b, entry_size));
	}

	/*
	 * GNU variant handles long filenames by storing /<number>
	 * to indicate a name stored in the filename table.
	 */
	if (filename[0] == '/' && isdigit(filename[1])) {
		number = ar_atol10(h + AR_name_offset + 1, AR_name_size - 1);
		/*
		 * If we can't look up the real name, warn and return
		 * the entry with the wrong name.
		 */
		if (ar->strtab == NULL || number > ar->strtab_size) {
			archive_set_error(&a->archive, EINVAL,
			    "Can't find long filename for entry");
			archive_entry_copy_pathname(entry, filename);
			/* Parse the time, owner, mode, size fields. */
			ar_parse_common_header(ar, entry, h);
			return (ARCHIVE_WARN);
		}

		archive_entry_copy_pathname(entry, &ar->strtab[(size_t)number]);
		/* Parse the time, owner, mode, size fields. */
		return (ar_parse_common_header(ar, entry, h));
	}

	/*
	 * BSD handles long filenames by storing "#1/" followed by the
	 * length of filename as a decimal number, then prepends the
	 * the filename to the file contents.
	 */
	if (strncmp(filename, "#1/", 3) == 0) {
		/* Parse the time, owner, mode, size fields. */
		/* This must occur before _read_ahead is called again. */
		ar_parse_common_header(ar, entry, h);

		/* Parse the size of the name, adjust the file size. */
		number = ar_atol10(h + AR_name_offset + 3, AR_name_size - 3);
		if ((off_t)number > ar->entry_bytes_remaining) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Bad input file size");
			return (ARCHIVE_FATAL);
		}
		bsd_name_length = (size_t)number;
		ar->entry_bytes_remaining -= bsd_name_length;
		/* Adjust file size reported to client. */
		archive_entry_set_size(entry, ar->entry_bytes_remaining);

		/* Read the long name into memory. */
		bytes_read = (a->decompressor->read_ahead)(a, &b, bsd_name_length);
		if (bytes_read <= 0)
			return (ARCHIVE_FATAL);
		if ((size_t)bytes_read < bsd_name_length) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Truncated input file");
			return (ARCHIVE_FATAL);
		}
		(a->decompressor->consume)(a, bsd_name_length);

		/* Store it in the entry. */
		p = (char *)malloc(bsd_name_length + 1);
		if (p == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't allocate fname buffer");
			return (ARCHIVE_FATAL);
		}
		strncpy(p, b, bsd_name_length);
		p[bsd_name_length] = '\0';
		archive_entry_copy_pathname(entry, p);
		free(p);
		return (ARCHIVE_OK);
	}

	/*
	 * "/" is the SVR4/GNU archive symbol table.
	 */
	if (strcmp(filename, "/") == 0) {
		archive_entry_copy_pathname(entry, "/");
		/* Parse the time, owner, mode, size fields. */
		r = ar_parse_common_header(ar, entry, h);
		/* Force the file type to a regular file. */
		archive_entry_set_mode(entry,
		    S_IFREG | (archive_entry_mode(entry) & 0777));
		return (r);
	}

	/*
	 * "__.SYMDEF" is a BSD archive symbol table.
	 */
	if (strcmp(filename, "__.SYMDEF") == 0) {
		archive_entry_copy_pathname(entry, filename);
		/* Parse the time, owner, mode, size fields. */
		return (ar_parse_common_header(ar, entry, h));
	}

	/*
	 * Otherwise, this is a standard entry.  The filename
	 * has already been trimmed as much as possible, based
	 * on our current knowledge of the format.
	 */
	archive_entry_copy_pathname(entry, filename);
	return (ar_parse_common_header(ar, entry, h));
}

static int
ar_parse_common_header(struct ar *ar, struct archive_entry *entry,
    const char *h)
{
	uint64_t n;

	/* Copy remaining header */
	archive_entry_set_mtime(entry,
	    (time_t)ar_atol10(h + AR_date_offset, AR_date_size), 0L);
	archive_entry_set_uid(entry,
	    (uid_t)ar_atol10(h + AR_uid_offset, AR_uid_size));
	archive_entry_set_gid(entry,
	    (gid_t)ar_atol10(h + AR_gid_offset, AR_gid_size));
	archive_entry_set_mode(entry,
	    (mode_t)ar_atol8(h + AR_mode_offset, AR_mode_size));
	n = ar_atol10(h + AR_size_offset, AR_size_size);

	ar->entry_offset = 0;
	ar->entry_padding = n % 2;
	archive_entry_set_size(entry, n);
	ar->entry_bytes_remaining = n;
	return (ARCHIVE_OK);
}

static int
archive_read_format_ar_read_data(struct archive_read *a,
    const void **buff, size_t *size, off_t *offset)
{
	ssize_t bytes_read;
	struct ar *ar;

	ar = (struct ar *)(a->format->data);

	if (ar->entry_bytes_remaining > 0) {
		bytes_read = (a->decompressor->read_ahead)(a, buff, 1);
		if (bytes_read == 0) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Truncated ar archive");
			return (ARCHIVE_FATAL);
		}
		if (bytes_read < 0)
			return (ARCHIVE_FATAL);
		if (bytes_read > ar->entry_bytes_remaining)
			bytes_read = (ssize_t)ar->entry_bytes_remaining;
		*size = bytes_read;
		*offset = ar->entry_offset;
		ar->entry_offset += bytes_read;
		ar->entry_bytes_remaining -= bytes_read;
		(a->decompressor->consume)(a, (size_t)bytes_read);
		return (ARCHIVE_OK);
	} else {
		while (ar->entry_padding > 0) {
			bytes_read = (a->decompressor->read_ahead)(a, buff, 1);
			if (bytes_read <= 0)
				return (ARCHIVE_FATAL);
			if (bytes_read > ar->entry_padding)
				bytes_read = (ssize_t)ar->entry_padding;
			(a->decompressor->consume)(a, (size_t)bytes_read);
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

	ar = (struct ar *)(a->format->data);
	if (a->decompressor->skip == NULL) {
		while (r == ARCHIVE_OK)
			r = archive_read_format_ar_read_data(a, &b, &s, &o);
		return (r);
	}

	bytes_skipped = (a->decompressor->skip)(a, ar->entry_bytes_remaining +
	    ar->entry_padding);
	if (bytes_skipped < 0)
		return (ARCHIVE_FATAL);

	ar->entry_bytes_remaining = 0;
	ar->entry_padding = 0;

	return (ARCHIVE_OK);
}

static int
ar_parse_gnu_filename_table(struct archive_read *a, struct ar *ar,
    const void *h, size_t size)
{
	char *p;

	if (ar->strtab != NULL) {
		archive_set_error(&a->archive, EINVAL,
		    "More than one string tables exist");
		return (ARCHIVE_WARN);
	}

	if (size == 0) {
		archive_set_error(&a->archive, EINVAL, "Invalid string table");
		return (ARCHIVE_WARN);
	}

	ar->strtab_size = size;
	ar->strtab = malloc(size);
	if (ar->strtab == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate string table buffer");
		return (ARCHIVE_FATAL);
	}

	(void)memcpy(ar->strtab, h, size);
	for (p = ar->strtab; p < ar->strtab + size - 1; ++p) {
		if (*p == '/') {
			*p++ = '\0';
			if (*p != '\n')
				goto bad_string_table;
			*p = '\0';
		}
	}
	/*
	 * Sanity check, last two chars must be `/\n' or '\n\n',
	 * depending on whether the string table is padded by a '\n'
	 * (string table produced by GNU ar always has a even size).
	 */
	if (p != ar->strtab + size && *p != '\n')
		goto bad_string_table;

	/* Enforce zero termination. */
	ar->strtab[size - 1] = '\0';

	return (ARCHIVE_OK);

bad_string_table:
	archive_set_error(&a->archive, EINVAL,
	    "Invalid string table");
	free(ar->strtab);
	ar->strtab = NULL;
	return (ARCHIVE_WARN);
}

static uint64_t
ar_atol8(const char *p, unsigned char_cnt)
{
	uint64_t l, limit, last_digit_limit;
	unsigned int digit, base;

	base = 8;
	limit = UINT64_MAX / base;
	last_digit_limit = UINT64_MAX % base;

	while ((*p == ' ' || *p == '\t') && char_cnt-- > 0)
		p++;

	l = 0;
	digit = *p - '0';
	while (*p >= '0' && digit < base  && char_cnt-- > 0) {
		if (l>limit || (l == limit && digit > last_digit_limit)) {
			l = UINT64_MAX; /* Truncate on overflow. */
			break;
		}
		l = (l * base) + digit;
		digit = *++p - '0';
	}
	return (l);
}

static uint64_t
ar_atol10(const char *p, unsigned char_cnt)
{
	uint64_t l, limit, last_digit_limit;
	unsigned int base, digit;

	base = 10;
	limit = UINT64_MAX / base;
	last_digit_limit = UINT64_MAX % base;

	while ((*p == ' ' || *p == '\t') && char_cnt-- > 0)
		p++;
	l = 0;
	digit = *p - '0';
	while (*p >= '0' && digit < base  && char_cnt-- > 0) {
		if (l > limit || (l == limit && digit > last_digit_limit)) {
			l = UINT64_MAX; /* Truncate on overflow. */
			break;
		}
		l = (l * base) + digit;
		digit = *++p - '0';
	}
	return (l);
}
