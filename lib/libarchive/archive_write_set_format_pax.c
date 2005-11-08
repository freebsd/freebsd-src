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
#ifdef MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#else
#ifdef MAJOR_IN_SYSMACROS
#include <sys/sysmacros.h>
#endif
#endif
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"

struct pax {
	uint64_t	entry_bytes_remaining;
	uint64_t	entry_padding;
	struct archive_string	pax_header;
	char		written;
};

static void		 add_pax_attr(struct archive_string *, const char *key,
			     const char *value);
static void		 add_pax_attr_int(struct archive_string *,
			     const char *key, int64_t value);
static void		 add_pax_attr_time(struct archive_string *,
			     const char *key, int64_t sec,
			     unsigned long nanos);
static void		 add_pax_attr_w(struct archive_string *,
			     const char *key, const wchar_t *wvalue);
static int		 archive_write_pax_data(struct archive *,
			     const void *, size_t);
static int		 archive_write_pax_finish(struct archive *);
static int		 archive_write_pax_finish_entry(struct archive *);
static int		 archive_write_pax_header(struct archive *,
			     struct archive_entry *);
static char		*build_pax_attribute_name(char *dest, const char *src);
static char		*build_ustar_entry_name(char *dest, const char *src, const char *insert);
static char		*format_int(char *dest, int64_t);
static int		 has_non_ASCII(const wchar_t *);
static int		 write_nulls(struct archive *, size_t);

/*
 * Set output format to 'restricted pax' format.
 *
 * This is the same as normal 'pax', but tries to suppress
 * the pax header whenever possible.  This is the default for
 * bsdtar, for instance.
 */
int
archive_write_set_format_pax_restricted(struct archive *a)
{
	int r;
	r = archive_write_set_format_pax(a);
	a->archive_format = ARCHIVE_FORMAT_TAR_PAX_RESTRICTED;
	a->archive_format_name = "restricted POSIX pax interchange";
	return (r);
}

/*
 * Set output format to 'pax' format.
 */
int
archive_write_set_format_pax(struct archive *a)
{
	struct pax *pax;

	if (a->format_finish != NULL)
		(a->format_finish)(a);

	pax = malloc(sizeof(*pax));
	if (pax == NULL) {
		archive_set_error(a, ENOMEM, "Can't allocate pax data");
		return (ARCHIVE_FATAL);
	}
	memset(pax, 0, sizeof(*pax));
	a->format_data = pax;

	a->pad_uncompressed = 1;
	a->format_write_header = archive_write_pax_header;
	a->format_write_data = archive_write_pax_data;
	a->format_finish = archive_write_pax_finish;
	a->format_finish_entry = archive_write_pax_finish_entry;
	a->archive_format = ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE;
	a->archive_format_name = "POSIX pax interchange";
	return (ARCHIVE_OK);
}

/*
 * Note: This code assumes that 'nanos' has the same sign as 'sec',
 * which implies that sec=-1, nanos=200000000 represents -1.2 seconds
 * and not -0.8 seconds.  This is a pretty pedantic point, as we're
 * unlikely to encounter many real files created before Jan 1, 1970,
 * much less ones with timestamps recorded to sub-second resolution.
 */
static void
add_pax_attr_time(struct archive_string *as, const char *key,
    int64_t sec, unsigned long nanos)
{
	int digit, i;
	char *t;
	/*
	 * Note that each byte contributes fewer than 3 base-10
	 * digits, so this will always be big enough.
	 */
	char tmp[1 + 3*sizeof(sec) + 1 + 3*sizeof(nanos)];

	tmp[sizeof(tmp) - 1] = 0;
	t = tmp + sizeof(tmp) - 1;

	/* Skip trailing zeros in the fractional part. */
	for(digit = 0, i = 10; i > 0 && digit == 0; i--) {
		digit = nanos % 10;
		nanos /= 10;
	}

	/* Only format the fraction if it's non-zero. */
	if (i > 0) {
		while (i > 0) {
			*--t = "0123456789"[digit];
			digit = nanos % 10;
			nanos /= 10;
			i--;
		}
		*--t = '.';
	}
	t = format_int(t, sec);

	add_pax_attr(as, key, t);
}

static char *
format_int(char *t, int64_t i)
{
	int sign;

	if (i < 0) {
		sign = -1;
		i = -i;
	} else
		sign = 1;

	do {
		*--t = "0123456789"[i % 10];
	} while (i /= 10);
	if (sign < 0)
		*--t = '-';
	return (t);
}

static void
add_pax_attr_int(struct archive_string *as, const char *key, int64_t value)
{
	char tmp[1 + 3 * sizeof(value)];

	tmp[sizeof(tmp) - 1] = 0;
	add_pax_attr(as, key, format_int(tmp + sizeof(tmp) - 1, value));
}

static void
add_pax_attr_w(struct archive_string *as, const char *key, const wchar_t *wval)
{
	int	utf8len;
	const wchar_t *wp;
	unsigned long wc;
	char *utf8_value, *p;

	utf8len = 0;
	for (wp = wval; *wp != L'\0'; ) {
		wc = *wp++;
		if (wc <= 0x7f)
			utf8len++;
		else if (wc <= 0x7ff)
			utf8len += 2;
		else if (wc <= 0xffff)
			utf8len += 3;
		else if (wc <= 0x1fffff)
			utf8len += 4;
		else if (wc <= 0x3ffffff)
			utf8len += 5;
		else if (wc <= 0x7fffffff)
			utf8len += 6;
		/* Ignore larger values; UTF-8 can't encode them. */
	}

	utf8_value = malloc(utf8len + 1);
	if (utf8_value == NULL)
		__archive_errx(1, "Not enough memory for attributes");

	for (wp = wval, p = utf8_value; *wp != L'\0'; ) {
		wc = *wp++;
		if (wc <= 0x7f) {
			*p++ = (char)wc;
		} else if (wc <= 0x7ff) {
			p[0] = 0xc0 | ((wc >> 6) & 0x1f);
			p[1] = 0x80 | (wc & 0x3f);
			p += 2;
		} else if (wc <= 0xffff) {
			p[0] = 0xe0 | ((wc >> 12) & 0x0f);
			p[1] = 0x80 | ((wc >> 6) & 0x3f);
			p[2] = 0x80 | (wc & 0x3f);
			p += 3;
		} else if (wc <= 0x1fffff) {
			p[0] = 0xf0 | ((wc >> 18) & 0x07);
			p[1] = 0x80 | ((wc >> 12) & 0x3f);
			p[2] = 0x80 | ((wc >> 6) & 0x3f);
			p[3] = 0x80 | (wc & 0x3f);
			p += 4;
		} else if (wc <= 0x3ffffff) {
			p[0] = 0xf8 | ((wc >> 24) & 0x03);
			p[1] = 0x80 | ((wc >> 18) & 0x3f);
			p[2] = 0x80 | ((wc >> 12) & 0x3f);
			p[3] = 0x80 | ((wc >> 6) & 0x3f);
			p[4] = 0x80 | (wc & 0x3f);
			p += 5;
		} else if (wc <= 0x7fffffff) {
			p[0] = 0xfc | ((wc >> 30) & 0x01);
			p[1] = 0x80 | ((wc >> 24) & 0x3f);
			p[1] = 0x80 | ((wc >> 18) & 0x3f);
			p[2] = 0x80 | ((wc >> 12) & 0x3f);
			p[3] = 0x80 | ((wc >> 6) & 0x3f);
			p[4] = 0x80 | (wc & 0x3f);
			p += 6;
		}
		/* Ignore larger values; UTF-8 can't encode them. */
	}
	*p = '\0';
	add_pax_attr(as, key, utf8_value);
	free(utf8_value);
}

/*
 * Add a key/value attribute to the pax header.  This function handles
 * the length field and various other syntactic requirements.
 */
static void
add_pax_attr(struct archive_string *as, const char *key, const char *value)
{
	int digits, i, len, next_ten;
	char tmp[1 + 3 * sizeof(int)];	/* < 3 base-10 digits per byte */

	/*-
	 * PAX attributes have the following layout:
	 *     <len> <space> <key> <=> <value> <nl>
	 */
	len = 1 + strlen(key) + 1 + strlen(value) + 1;

	/*
	 * The <len> field includes the length of the <len> field, so
	 * computing the correct length is tricky.  I start by
	 * counting the number of base-10 digits in 'len' and
	 * computing the next higher power of 10.
	 */
	next_ten = 1;
	digits = 0;
	i = len;
	while (i > 0) {
		i = i / 10;
		digits++;
		next_ten = next_ten * 10;
	}
	/*
	 * For example, if string without the length field is 99
	 * chars, then adding the 2 digit length "99" will force the
	 * total length past 100, requiring an extra digit.  The next
	 * statement adjusts for this effect.
	 */
	if (len + digits >= next_ten)
		digits++;

	/* Now, we have the right length so we can build the line. */
	tmp[sizeof(tmp) - 1] = 0;	/* Null-terminate the work area. */
	archive_strcat(as, format_int(tmp + sizeof(tmp) - 1, len + digits));
	archive_strappend_char(as, ' ');
	archive_strcat(as, key);
	archive_strappend_char(as, '=');
	archive_strcat(as, value);
	archive_strappend_char(as, '\n');
}

/*
 * TODO: Consider adding 'comment' and 'charset' fields to
 * archive_entry so that clients can specify them.  Also, consider
 * adding generic key/value tags so clients can add arbitrary
 * key/value data.
 */
static int
archive_write_pax_header(struct archive *a,
    struct archive_entry *entry_original)
{
	struct archive_entry *entry_main;
	const char *linkname, *p;
	const char *hardlink;
	const wchar_t *wp;
	const char *suffix_start;
	int need_extension, r, ret;
	struct pax *pax;
	const struct stat *st_main, *st_original;

	char paxbuff[512];
	char ustarbuff[512];
	char ustar_entry_name[256];
	char pax_entry_name[256];

	need_extension = 0;
	pax = a->format_data;
	pax->written = 1;

	st_original = archive_entry_stat(entry_original);

	hardlink = archive_entry_hardlink(entry_original);

	/* Make sure this is a type of entry that we can handle here */
	if (hardlink == NULL) {
		switch (st_original->st_mode & S_IFMT) {
		case S_IFREG:
		case S_IFLNK:
		case S_IFCHR:
		case S_IFBLK:
		case S_IFDIR:
		case S_IFIFO:
			break;
		case S_IFSOCK:
			archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
			    "tar format cannot archive socket");
			return (ARCHIVE_WARN);
		default:
			archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
			    "tar format cannot archive this (mode=0%lo)",
			    (unsigned long)st_original->st_mode);
			return (ARCHIVE_WARN);
		}
	}

	/* Copy entry so we can modify it as needed. */
	entry_main = archive_entry_clone(entry_original);
	archive_string_empty(&(pax->pax_header)); /* Blank our work area. */
	st_main = archive_entry_stat(entry_main);

	/*
	 * Determining whether or not the name is too big is ugly
	 * because of the rules for dividing names between 'name' and
	 * 'prefix' fields.  Here, I pick out the longest possible
	 * suffix, then test whether the remaining prefix is too long.
	 */
	wp = archive_entry_pathname_w(entry_main);
	p = archive_entry_pathname(entry_main);
	if (strlen(p) <= 100)	/* Short enough for just 'name' field */
		suffix_start = p;	/* Record a zero-length prefix */
	else
		/* Find the largest suffix that fits in 'name' field. */
		suffix_start = strchr(p + strlen(p) - 100 - 1, '/');

	/*
	 * If name is too long, or has non-ASCII characters, add
	 * 'path' to pax extended attrs.
	 */
	if (suffix_start == NULL || suffix_start - p > 155 || has_non_ASCII(wp)) {
		add_pax_attr_w(&(pax->pax_header), "path", wp);
		archive_entry_set_pathname(entry_main,
		    build_ustar_entry_name(ustar_entry_name, p, NULL));
		need_extension = 1;
	}

	/* If link name is too long or has non-ASCII characters, add
	 * 'linkpath' to pax extended attrs. */
	linkname = hardlink;
	if (linkname == NULL)
		linkname = archive_entry_symlink(entry_main);

	if (linkname != NULL) {
		/* There is a link name, get the wide version as well. */
		if (hardlink != NULL)
			wp = archive_entry_hardlink_w(entry_main);
		else
			wp = archive_entry_symlink_w(entry_main);

		/* If the link is long or has a non-ASCII character,
		 * store it as a pax extended attribute. */
		if (strlen(linkname) > 100 || has_non_ASCII(wp)) {
			add_pax_attr_w(&(pax->pax_header), "linkpath", wp);
			if (hardlink != NULL)
				archive_entry_set_hardlink(entry_main,
				    "././@LongHardLink");
			else
				archive_entry_set_symlink(entry_main,
				    "././@LongSymLink");
			need_extension = 1;
		}
	}

	/* If file size is too large, add 'size' to pax extended attrs. */
	if (st_main->st_size >= (((int64_t)1) << 33)) {
		add_pax_attr_int(&(pax->pax_header), "size", st_main->st_size);
		need_extension = 1;
	}

	/* If numeric GID is too large, add 'gid' to pax extended attrs. */
	if (st_main->st_gid >= (1 << 18)) {
		add_pax_attr_int(&(pax->pax_header), "gid", st_main->st_gid);
		need_extension = 1;
	}

	/* If group name is too large or has non-ASCII characters, add
	 * 'gname' to pax extended attrs. */
	p = archive_entry_gname(entry_main);
	wp = archive_entry_gname_w(entry_main);
	if (p != NULL && (strlen(p) > 31 || has_non_ASCII(wp))) {
		add_pax_attr_w(&(pax->pax_header), "gname", wp);
		archive_entry_set_gname(entry_main, NULL);
		need_extension = 1;
	}

	/* If numeric UID is too large, add 'uid' to pax extended attrs. */
	if (st_main->st_uid >= (1 << 18)) {
		add_pax_attr_int(&(pax->pax_header), "uid", st_main->st_uid);
		need_extension = 1;
	}

	/* If user name is too large, add 'uname' to pax extended attrs. */
	/* TODO: If uname has non-ASCII characters, use pax attribute. */
	p = archive_entry_uname(entry_main);
	wp = archive_entry_uname_w(entry_main);
	if (p != NULL && (strlen(p) > 31 || has_non_ASCII(wp))) {
		add_pax_attr_w(&(pax->pax_header), "uname", wp);
		archive_entry_set_uname(entry_main, NULL);
		need_extension = 1;
	}

	/*
	 * POSIX/SUSv3 doesn't provide a standard key for large device
	 * numbers.  I use the same keys here that Joerg Schilling
	 * used for 'star.'  (Which, somewhat confusingly, are called
	 * "devXXX" even though they code "rdev" values.)  No doubt,
	 * other implementations use other keys.  Note that there's no
	 * reason we can't write the same information into a number of
	 * different keys.
	 *
	 * Of course, this is only needed for block or char device entries.
	 */
	if (S_ISBLK(st_main->st_mode) ||
	    S_ISCHR(st_main->st_mode)) {
		/*
		 * If rdevmajor is too large, add 'SCHILY.devmajor' to
		 * extended attributes.
		 */
		dev_t rdevmajor, rdevminor;
		rdevmajor = major(st_main->st_rdev);
		rdevminor = minor(st_main->st_rdev);
		if (rdevmajor >= (1 << 18)) {
			add_pax_attr_int(&(pax->pax_header), "SCHILY.devmajor",
			    rdevmajor);
			/*
			 * Non-strict formatting below means we don't
			 * have to truncate here.  Not truncating improves
			 * the chance that some more modern tar archivers
			 * (such as GNU tar 1.13) can restore the full
			 * value even if they don't understand the pax
			 * extended attributes.  See my rant below about
			 * file size fields for additional details.
			 */
			/* archive_entry_set_rdevmajor(entry_main,
			   rdevmajor & ((1 << 18) - 1)); */
			need_extension = 1;
		}

		/*
		 * If devminor is too large, add 'SCHILY.devminor' to
		 * extended attributes.
		 */
		if (rdevminor >= (1 << 18)) {
			add_pax_attr_int(&(pax->pax_header), "SCHILY.devminor",
			    rdevminor);
			/* Truncation is not necessary here, either. */
			/* archive_entry_set_rdevminor(entry_main,
			   rdevminor & ((1 << 18) - 1)); */
			need_extension = 1;
		}
	}

	/*
	 * Technically, the mtime field in the ustar header can
	 * support 33 bits, but many platforms use signed 32-bit time
	 * values.  The cutoff of 0x7fffffff here is a compromise.
	 * Yes, this check is duplicated just below; this helps to
	 * avoid writing an mtime attribute just to handle a
	 * high-resolution timestamp in "restricted pax" mode.
	 */
	if (!need_extension &&
	    ((st_main->st_mtime < 0) || (st_main->st_mtime >= 0x7fffffff)))
		need_extension = 1;

	/* I use a star-compatible file flag attribute. */
	p = archive_entry_fflags_text(entry_main);
	if (!need_extension && p != NULL  &&  *p != '\0')
		need_extension = 1;

	/* If there are non-trivial ACL entries, we need an extension. */
	if (!need_extension && archive_entry_acl_count(entry_original,
		ARCHIVE_ENTRY_ACL_TYPE_ACCESS) > 0)
		need_extension = 1;

	/* If there are non-trivial ACL entries, we need an extension. */
	if (!need_extension && archive_entry_acl_count(entry_original,
		ARCHIVE_ENTRY_ACL_TYPE_DEFAULT) > 0)
		need_extension = 1;

	/*
	 * The following items are handled differently in "pax
	 * restricted" format.  In particular, in "pax restricted"
	 * format they won't be added unless need_extension is
	 * already set (we're already generating an extended header, so
	 * may as well include these).
	 */
	if (a->archive_format != ARCHIVE_FORMAT_TAR_PAX_RESTRICTED ||
	    need_extension) {

		if (st_main->st_mtime < 0  ||
		    st_main->st_mtime >= 0x7fffffff  ||
		    ARCHIVE_STAT_MTIME_NANOS(st_main) != 0)
			add_pax_attr_time(&(pax->pax_header), "mtime",
			    st_main->st_mtime,
			    ARCHIVE_STAT_MTIME_NANOS(st_main));

		if (st_main->st_ctime != 0  ||
		    ARCHIVE_STAT_CTIME_NANOS(st_main) != 0)
			add_pax_attr_time(&(pax->pax_header), "ctime",
			    st_main->st_ctime,
			    ARCHIVE_STAT_CTIME_NANOS(st_main));

		if (st_main->st_atime != 0  ||
		    ARCHIVE_STAT_ATIME_NANOS(st_main) != 0)
			add_pax_attr_time(&(pax->pax_header), "atime",
			    st_main->st_atime,
			    ARCHIVE_STAT_ATIME_NANOS(st_main));

		/* I use a star-compatible file flag attribute. */
		p = archive_entry_fflags_text(entry_main);
		if (p != NULL  &&  *p != '\0')
			add_pax_attr(&(pax->pax_header), "SCHILY.fflags", p);

		/* I use star-compatible ACL attributes. */
		wp = archive_entry_acl_text_w(entry_original,
		    ARCHIVE_ENTRY_ACL_TYPE_ACCESS |
		    ARCHIVE_ENTRY_ACL_STYLE_EXTRA_ID);
		if (wp != NULL && *wp != L'\0')
			add_pax_attr_w(&(pax->pax_header),
			    "SCHILY.acl.access", wp);
		wp = archive_entry_acl_text_w(entry_original,
		    ARCHIVE_ENTRY_ACL_TYPE_DEFAULT |
		    ARCHIVE_ENTRY_ACL_STYLE_EXTRA_ID);
		if (wp != NULL && *wp != L'\0')
			add_pax_attr_w(&(pax->pax_header),
			    "SCHILY.acl.default", wp);

		/* Include star-compatible metadata info. */
		/* Note: "SCHILY.dev{major,minor}" are NOT the
		 * major/minor portions of "SCHILY.dev". */
		add_pax_attr_int(&(pax->pax_header), "SCHILY.dev",
		    st_main->st_dev);
		add_pax_attr_int(&(pax->pax_header), "SCHILY.ino",
		    st_main->st_ino);
		add_pax_attr_int(&(pax->pax_header), "SCHILY.nlink",
		    st_main->st_nlink);
	}

	/* Only regular files have data. */
	if (!S_ISREG(archive_entry_mode(entry_main)))
		archive_entry_set_size(entry_main, 0);

	/*
	 * Pax-restricted does not store data for hardlinks, in order
	 * to improve compatibility with ustar.
	 */
	if (a->archive_format != ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE &&
	    hardlink != NULL)
		archive_entry_set_size(entry_main, 0);

	/*
	 * XXX Full pax interchange format does permit a hardlink
	 * entry to have data associated with it.  I'm not supporting
	 * that here because the client expects me to tell them whether
	 * or not this format expects data for hardlinks.  If I
	 * don't check here, then every pax archive will end up with
	 * duplicated data for hardlinks.  Someday, there may be
	 * need to select this behavior, in which case the following
	 * will need to be revisited. XXX
	 */
	if (hardlink != NULL)
		archive_entry_set_size(entry_main, 0);

	/* Format 'ustar' header for main entry.
	 *
	 * The trouble with file size: If the reader can't understand
	 * the file size, they may not be able to locate the next
	 * entry and the rest of the archive is toast.  Pax-compliant
	 * readers are supposed to ignore the file size in the main
	 * header, so the question becomes how to maximize portability
	 * for readers that don't support pax attribute extensions.
	 * For maximum compatibility, I permit numeric extensions in
	 * the main header so that the file size stored will always be
	 * correct, even if it's in a format that only some
	 * implementations understand.  The technique used here is:
	 *
	 *  a) If possible, follow the standard exactly.  This handles
	 *  files up to 8 gigabytes minus 1.
	 *
	 *  b) If that fails, try octal but omit the field terminator.
	 *  That handles files up to 64 gigabytes minus 1.
	 *
	 *  c) Otherwise, use base-256 extensions.  That handles files
	 *  up to 2^63 in this implementation, with the potential to
	 *  go up to 2^94.  That should hold us for a while. ;-)
	 *
	 * The non-strict formatter uses similar logic for other
	 * numeric fields, though they're less critical.
	 */
	__archive_write_format_header_ustar(a, ustarbuff, entry_main, -1, 0);

	/* If we built any extended attributes, write that entry first. */
	ret = ARCHIVE_OK;
	if (archive_strlen(&(pax->pax_header)) > 0) {
		struct stat st;
		struct archive_entry *pax_attr_entry;
		time_t s;
		long ns;

		memset(&st, 0, sizeof(st));
		pax_attr_entry = archive_entry_new();
		p = archive_entry_pathname(entry_main);
		archive_entry_set_pathname(pax_attr_entry,
		    build_pax_attribute_name(pax_entry_name, p));
		st.st_size = archive_strlen(&(pax->pax_header));
		/* Copy uid/gid (but clip to ustar limits). */
		st.st_uid = st_main->st_uid;
		if (st.st_uid >= 1 << 18)
			st.st_uid = (1 << 18) - 1;
		st.st_gid = st_main->st_gid;
		if (st.st_gid >= 1 << 18)
			st.st_gid = (1 << 18) - 1;
		/* Copy mode over (but not setuid/setgid bits) */
		st.st_mode = st_main->st_mode;
#ifdef S_ISUID
		st.st_mode &= ~S_ISUID;
#endif
#ifdef S_ISGID
		st.st_mode &= ~S_ISGID;
#endif
#ifdef S_ISVTX
		st.st_mode &= ~S_ISVTX;
#endif
		archive_entry_copy_stat(pax_attr_entry, &st);

		/* Copy uname/gname. */
		archive_entry_set_uname(pax_attr_entry,
		    archive_entry_uname(entry_main));
		archive_entry_set_gname(pax_attr_entry,
		    archive_entry_gname(entry_main));

		/* Copy mtime, but clip to ustar limits. */
		s = archive_entry_mtime(entry_main);
		ns = archive_entry_mtime_nsec(entry_main);
		if (s < 0) { s = 0; ns = 0; }
		if (s > 0x7fffffff) { s = 0x7fffffff; ns = 0; }
		archive_entry_set_mtime(pax_attr_entry, s, ns);

		/* Ditto for atime. */
		s = archive_entry_atime(entry_main);
		ns = archive_entry_atime_nsec(entry_main);
		if (s < 0) { s = 0; ns = 0; }
		if (s > 0x7fffffff) { s = 0x7fffffff; ns = 0; }
		archive_entry_set_atime(pax_attr_entry, s, ns);

		/* Standard ustar doesn't support ctime. */
		archive_entry_set_ctime(pax_attr_entry, 0, 0);

		ret = __archive_write_format_header_ustar(a, paxbuff,
		    pax_attr_entry, 'x', 1);

		archive_entry_free(pax_attr_entry);

		/* Note that the 'x' header shouldn't ever fail to format */
		if (ret != 0) {
			const char *msg = "archive_write_pax_header: "
			    "'x' header failed?!  This can't happen.\n";
			write(2, msg, strlen(msg));
			exit(1);
		}
		r = (a->compression_write)(a, paxbuff, 512);
		if (r != ARCHIVE_OK) {
			pax->entry_bytes_remaining = 0;
			pax->entry_padding = 0;
			return (ARCHIVE_FATAL);
		}

		pax->entry_bytes_remaining = archive_strlen(&(pax->pax_header));
		pax->entry_padding = 0x1ff & (- pax->entry_bytes_remaining);

		r = (a->compression_write)(a, pax->pax_header.s,
		    archive_strlen(&(pax->pax_header)));
		if (r != ARCHIVE_OK) {
			/* If a write fails, we're pretty much toast. */
			return (ARCHIVE_FATAL);
		}
		/* Pad out the end of the entry. */
		r = write_nulls(a, pax->entry_padding);
		if (r != ARCHIVE_OK) {
			/* If a write fails, we're pretty much toast. */
			return (ARCHIVE_FATAL);
		}
		pax->entry_bytes_remaining = pax->entry_padding = 0;
	}

	/* Write the header for main entry. */
	r = (a->compression_write)(a, ustarbuff, 512);
	if (r != ARCHIVE_OK)
		return (r);

	/*
	 * Inform the client of the on-disk size we're using, so
	 * they can avoid unnecessarily writing a body for something
	 * that we're just going to ignore.
	 */
	archive_entry_set_size(entry_original, archive_entry_size(entry_main));
	pax->entry_bytes_remaining = archive_entry_size(entry_main);
	pax->entry_padding = 0x1ff & (- pax->entry_bytes_remaining);
	archive_entry_free(entry_main);

	return (ret);
}

/*
 * We need a valid name for the regular 'ustar' entry.  This routine
 * tries to hack something more-or-less reasonable.
 *
 * The approach here tries to preserve leading dir names.  We do so by
 * working with four sections:
 *   1) "prefix" directory names,
 *   2) "suffix" directory names,
 *   3) inserted dir name (optional),
 *   4) filename.
 *
 * These sections must satisfy the following requirements:
 *   * Parts 1 & 2 together form an initial portion of the dir name.
 *   * Part 3 is specified by the caller.  (It should not contain a leading
 *     or trailing '/'.)
 *   * Part 4 forms an initial portion of the base filename.
 *   * The filename must be <= 99 chars to fit the ustar 'name' field.
 *   * Parts 2, 3, 4 together must be <= 99 chars to fit the ustar 'name' fld.
 *   * Part 1 must be <= 155 chars to fit the ustar 'prefix' field.
 *   * If the original name ends in a '/', the new name must also end in a '/'
 *   * Trailing '/.' sequences may be stripped.
 *
 * Note: Recall that the ustar format does not store the '/' separating
 * parts 1 & 2, but does store the '/' separating parts 2 & 3.
 */
static char *
build_ustar_entry_name(char *dest, const char *src, const char *insert)
{
	const char *prefix, *prefix_end;
	const char *suffix, *suffix_end;
	const char *filename, *filename_end;
	char *p;
	size_t s;
	int need_slash = 0; /* Was there a trailing slash? */
	size_t suffix_length = 99;
	int insert_length;

	/* Length of additional dir element to be added. */
	if (insert == NULL)
		insert_length = 0;
	else
		/* +2 here allows for '/' before and after the insert. */
		insert_length = strlen(insert) + 2;

	/* Step 0: Quick bailout in a common case. */
	s = strlen(src);
	if (s < 100 && insert == NULL) {
		strcpy(dest, src);
		return (dest);
	}

	/* Step 1: Locate filename and enforce the length restriction. */
	filename_end = src + s;
	/* Remove trailing '/' chars and '/.' pairs. */
	for (;;) {
		if (filename_end > src && filename_end[-1] == '/') {
			filename_end --;
			need_slash = 1; /* Remember to restore trailing '/'. */
			continue;
		}
		if (filename_end > src + 1 && filename_end[-1] == '.'
		    && filename_end[-2] == '/') {
			filename_end -= 2;
			need_slash = 1; /* "foo/." will become "foo/" */
			continue;
		}
		break;
	}
	if (need_slash)
		suffix_length--;
	/* Find start of filename. */
	filename = filename_end - 1;
	while ((filename > src) && (*filename != '/'))
		filename --;
	if ((*filename == '/') && (filename < filename_end - 1))
		filename ++;
	/* Adjust filename_end so that filename + insert fits in 99 chars. */
	suffix_length -= insert_length;
	if (filename_end > filename + suffix_length)
		filename_end = filename + suffix_length;
	/* Calculate max size for "suffix" section (#3 above). */
	suffix_length -= filename_end - filename;

	/* Step 2: Locate the "prefix" section of the dirname, including
	 * trailing '/'. */
	prefix = src;
	prefix_end = prefix + 155;
	if (prefix_end > filename)
		prefix_end = filename;
	while (prefix_end > prefix && *prefix_end != '/')
		prefix_end--;
	if ((prefix_end < filename) && (*prefix_end == '/'))
		prefix_end++;

	/* Step 3: Locate the "suffix" section of the dirname,
	 * including trailing '/'. */
	suffix = prefix_end;
	suffix_end = suffix + suffix_length; /* Enforce limit. */
	if (suffix_end > filename)
		suffix_end = filename;
	if (suffix_end < suffix)
		suffix_end = suffix;
	while (suffix_end > suffix && *suffix_end != '/')
		suffix_end--;
	if ((suffix_end < filename) && (*suffix_end == '/'))
		suffix_end++;

	/* Step 4: Build the new name. */
	/* The OpenBSD strlcpy function is safer, but less portable. */
	/* Rather than maintain two versions, just use the strncpy version. */
	p = dest;
	if (prefix_end > prefix) {
		strncpy(p, prefix, prefix_end - prefix);
		p += prefix_end - prefix;
	}
	if (suffix_end > suffix) {
		strncpy(p, suffix, suffix_end - suffix);
		p += suffix_end - suffix;
	}
	if (insert != NULL) {
		if (prefix_end > prefix || suffix_end > suffix)
			*p++ = '/';
		strcpy(p, insert);
		p += strlen(insert);
		*p++ = '/';
	}
	strncpy(p, filename, filename_end - filename);
	p += filename_end - filename;
	if (need_slash)
		*p++ = '/';
	*p++ = '\0';

	return (dest);
}

/*
 * The ustar header for the pax extended attributes must have a
 * reasonable name:  SUSv3 suggests 'dirname'/PaxHeader/'filename'
 *
 * Joerg Schiling has argued that this is unnecessary because, in practice,
 * if the pax extended attributes get extracted as regular files, noone is
 * going to bother reading those attributes to manually restore them.
 * Based on this, 'star' uses /tmp/PaxHeader/'basename' as the ustar header
 * name.  This is a tempting argument, but I'm not entirely convinced.
 * I'm also uncomfortable with the fact that "/tmp" is a Unix-ism.
 *
 * The following routine implements the SUSv3 recommendation, and is
 * much simpler because build_ustar_entry_name() above already does
 * most of the work (we just need to give it an extra path element to
 * insert and handle a few pathological cases).
 */
static char *
build_pax_attribute_name(char *dest, const char *src)
{
	char *p;

	/* Handle the null filename case. */
	if (src == NULL || *src == '\0') {
		strcpy(dest, "PaxHeader/blank");
		return (dest);
	}

	/* Prune final '/' and other unwanted final elements. */
	p = dest + strlen(dest);
	for (;;) {
		/* Ends in "/", remove the '/' */
		if (p > dest && p[-1] == '/') {
			*--p = '\0';
			continue;
		}
		/* Ends in "/.", remove the '.' */
		if (p > dest + 1 && p[-1] == '.'
		    && p[-2] == '/') {
			*--p = '\0';
			continue;
		}
		break;
	}

	/* Pathological case: After above, there was nothing left. */
	if (p == dest) {
		strcpy(dest, "/PaxHeader/rootdir");
		return (dest);
	}

	/* Convert unadorned "." into "dot" */
	if (*src == '.' && src[1] == '\0') {
		strcpy(dest, "PaxHeader/currentdir");
		return (dest);
	}

	/* General case: build a ustar-compatible name adding "/PaxHeader/". */
	build_ustar_entry_name(dest, src, "PaxHeader");

	return (dest);
}

/* Write two null blocks for the end of archive */
static int
archive_write_pax_finish(struct archive *a)
{
	struct pax *pax;
	int r;

	r = ARCHIVE_OK;
	pax = a->format_data;
	if (pax->written && a->compression_write != NULL)
		r = write_nulls(a, 512 * 2);
	archive_string_free(&pax->pax_header);
	free(pax);
	a->format_data = NULL;
	return (r);
}

static int
archive_write_pax_finish_entry(struct archive *a)
{
	struct pax *pax;
	int ret;

	pax = a->format_data;
	ret = write_nulls(a, pax->entry_bytes_remaining + pax->entry_padding);
	pax->entry_bytes_remaining = pax->entry_padding = 0;
	return (ret);
}

static int
write_nulls(struct archive *a, size_t padding)
{
	int ret, to_write;

	while (padding > 0) {
		to_write = padding < a->null_length ? padding : a->null_length;
		ret = (a->compression_write)(a, a->nulls, to_write);
		if (ret != ARCHIVE_OK)
			return (ret);
		padding -= to_write;
	}
	return (ARCHIVE_OK);
}

static int
archive_write_pax_data(struct archive *a, const void *buff, size_t s)
{
	struct pax *pax;
	int ret;

	pax = a->format_data;
	pax->written = 1;
	if (s > pax->entry_bytes_remaining)
		s = pax->entry_bytes_remaining;

	ret = (a->compression_write)(a, buff, s);
	pax->entry_bytes_remaining -= s;
	return (ret);
}

static int
has_non_ASCII(const wchar_t *wp)
{
	while (*wp != L'\0' && *wp < 128)
		wp++;
	return (*wp != L'\0');
}
