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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/stat.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"

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

static int64_t	atol8(const char *, unsigned);
static int	archive_read_format_cpio_bid(struct archive *);
static int	archive_read_format_cpio_read_header(struct archive *,
		    struct archive_entry *);

int
archive_read_support_format_cpio(struct archive *a)
{
	return (__archive_read_register_format(a,
	    NULL,
	    archive_read_format_cpio_bid,
	    archive_read_format_cpio_read_header,
	    NULL));
}


static int
archive_read_format_cpio_bid(struct archive *a)
{
	int bid, bytes_read;
	const void *h;
	const struct cpio_header *header;

	bid = 0;
	bytes_read =
	    (a->compression_read_ahead)(a, &h, sizeof(struct cpio_header));
	if (bytes_read < (int)sizeof(struct cpio_header))
	    return (-1);

	header = h;

	if (memcmp(header->c_magic, "070707", 6)) return 0;
	bid += 48;

	/* TODO: Verify more of header: Can at least check that only octal
	   digits appear in appropriate header locations */

	return (bid);
}

static int
archive_read_format_cpio_read_header(struct archive *a,
    struct archive_entry *entry)
{
	struct stat st;
	size_t bytes;
	const struct cpio_header *header;
	const void *h;
	size_t namelength;

	a->archive_format = ARCHIVE_FORMAT_CPIO;
	a->archive_format_name = "POSIX octet-oriented cpio";

	/* Read fixed-size portion of header. */
	bytes = (a->compression_read_ahead)(a, &h, sizeof(struct cpio_header));
	if (bytes < sizeof(struct cpio_header))
	    return (ARCHIVE_FATAL);
	(a->compression_read_consume)(a, sizeof(struct cpio_header));

	/* Parse out octal fields into struct stat */
	memset(&st, 0, sizeof(st));
	header = h;

	st.st_dev = atol8(header->c_dev, sizeof(header->c_dev));
	st.st_ino = atol8(header->c_ino, sizeof(header->c_ino));
	st.st_mode = atol8(header->c_mode, sizeof(header->c_mode));
	st.st_uid = atol8(header->c_uid, sizeof(header->c_uid));
	st.st_gid = atol8(header->c_gid, sizeof(header->c_gid));
	st.st_nlink = atol8(header->c_nlink, sizeof(header->c_nlink));
	st.st_rdev = atol8(header->c_rdev, sizeof(header->c_rdev));
	st.st_mtime = atol8(header->c_mtime, sizeof(header->c_mtime));
	namelength = atol8(header->c_namesize, sizeof(header->c_namesize));

	/*
	 * Note: entry_bytes_remaining is at least 64 bits and
	 * therefore gauranteed to be big enough for a 33-bite file
	 * size.  struct stat.st_size may only be 32 bits, so
	 * assigning there first could lose information.
	 */
	a->entry_bytes_remaining =
	    atol8(header->c_filesize, sizeof(header->c_filesize));
	st.st_size = a->entry_bytes_remaining;
	a->entry_padding = 0;

	/* Assign all of the 'stat' fields at once. */
	archive_entry_copy_stat(entry, &st);

	/* Read name from buffer. */
	bytes = (a->compression_read_ahead)(a, &h, namelength);
	if (bytes < namelength)
	    return (ARCHIVE_FATAL);
	(a->compression_read_consume)(a, namelength);
	archive_strncpy(&a->entry_name, h, namelength);
	archive_entry_set_pathname(entry, a->entry_name.s);

	/* If this is a symlink, read the link contents. */
	if (S_ISLNK(st.st_mode)) {
		bytes = (a->compression_read_ahead)(a, &h,
		    a->entry_bytes_remaining);
		if (bytes < a->entry_bytes_remaining)
			return (ARCHIVE_FATAL);
		(a->compression_read_consume)(a, a->entry_bytes_remaining);
		archive_strncpy(&a->entry_linkname, h, a->entry_bytes_remaining);
		archive_entry_set_symlink(entry, a->entry_linkname.s);
		a->entry_bytes_remaining = 0;
	}

	/* Compare name to "TRAILER!!!" to test for end-of-archive. */
	if (namelength == 11 && strcmp(h,"TRAILER!!!")==0) {
	    /* TODO: Store file location of start of block. */
	    archive_set_error(a, 0, NULL);
	    return (ARCHIVE_EOF);
	}

	return (ARCHIVE_OK);
}


/* Note that this implementation does not (and should not!) obey
 * locale settings; you cannot simply substitute strtol here, since
 * it does obey locale.
 */
static int64_t
atol8(const char *p, unsigned char_cnt)
{
	int64_t l;
	int digit;

	static const int64_t limit = INT64_MAX / 8;
	static const int base = 8;
	static const char last_digit_limit = INT64_MAX % 8;

	l = 0;
	digit = *p - '0';
	while (digit >= 0 && digit < base  && char_cnt-- > 0) {
		if (l > limit || (l == limit && digit > last_digit_limit)) {
			l = UINT64_MAX; /* Truncate on overflow */
			break;
		}
		l = (l * base) + digit;
		digit = *++p - '0';
	}
	return (l);
}
