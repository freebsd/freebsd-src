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
#ifdef HAVE_DMALLOC
#include <dmalloc.h>
#endif
#include <err.h>
#include <errno.h>
/* #include <stdint.h> */ /* See archive_platform.h */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"

/*
 * Structure of GNU tar header
 */
struct archive_entry_header_gnutar {
	char	name[100];
	char	mode[8];
	char	uid[8];
	char	gid[8];
	char	size[12];
	char	mtime[12];
	char	checksum[8];
	char	typeflag[1];
	char	linkname[100];
	char	magic[8];  /* "ustar  \0" (note blank/blank/null at end) */
	char	uname[32];
	char	gname[32];
	char	devmajor[8];
	char	devminor[8];
	char	atime[12];
	char	ctime[12];
	char	offset[12];
	char	longnames[4];
	char	unused[1];
	struct {
	    char	offset[12];
	    char	numbytes[12];
	}	sparse[4];
	char	isextended[1];
	char	realsize[12];
	/*
	 * GNU doesn't use POSIX 'prefix' field; they use the 'L' (longname)
	 * entry instead.
	 */
};

struct gnutar {
	struct archive_string	entry_name;
	struct archive_string	entry_linkname;
	struct archive_string	entry_uname;
	struct archive_string	entry_gname;
	struct archive_string	gnu_name;
	struct archive_string	gnu_linkname;
	int		  	gnu_header_recursion_depth;
};

static int	archive_block_is_null(const unsigned char *p);
static int	archive_header_gnu(struct archive *, struct archive_entry *,
		    const void *);
static int	archive_read_format_gnutar_bid(struct archive *a);
static int	archive_read_format_gnutar_cleanup(struct archive *);
static int	archive_read_format_gnutar_read_header(struct archive *a,
		    struct archive_entry *);
static int	checksum(struct archive *a, const void *h);
static int64_t	tar_atol(const char *, unsigned);
static int64_t	tar_atol8(const char *, unsigned);
static int64_t	tar_atol256(const char *, unsigned);

/*
 * The ONLY publicly visible function in this file.
 */
int
archive_read_support_format_gnutar(struct archive *a)
{
	struct gnutar *gnutar;

	gnutar = malloc(sizeof(*gnutar));
	memset(gnutar, 0, sizeof(*gnutar));

	return (__archive_read_register_format(a,
	    gnutar,
	    archive_read_format_gnutar_bid,
	    archive_read_format_gnutar_read_header,
	    archive_read_format_gnutar_cleanup));
}

static int
archive_read_format_gnutar_cleanup(struct archive *a)
{
	struct gnutar *gnutar;

	gnutar = *(a->pformat_data);
	if (gnutar->entry_name.s != NULL)
		free(gnutar->entry_name.s);
	if (gnutar->entry_linkname.s != NULL)
		free(gnutar->entry_linkname.s);
	if (gnutar->entry_uname.s != NULL)
		free(gnutar->entry_uname.s);
	if (gnutar->entry_gname.s != NULL)
		free(gnutar->entry_gname.s);
	if (gnutar->gnu_name.s != NULL)
		free(gnutar->gnu_name.s);
	if (gnutar->gnu_linkname.s != NULL)
		free(gnutar->gnu_linkname.s);

	free(gnutar);
	*(a->pformat_data) = NULL;
	return (ARCHIVE_OK);
}

static int
archive_read_format_gnutar_bid(struct archive *a)
{
	int bid;
	size_t bytes_read;
	const void *h;
	const struct archive_entry_header_gnutar *header;

	/*
	 * If we're already reading a non-tar file, don't
	 * bother to bid.
	 */
	if (a->archive_format != 0 &&
	    (a->archive_format & ARCHIVE_FORMAT_BASE_MASK) !=
	    ARCHIVE_FORMAT_TAR)
	    return (0);

	bid = 0;

	/* If last header was my preferred format, bid a bit more. */
	if (a->archive_format == ARCHIVE_FORMAT_TAR_GNUTAR)
	    bid += 10;

	bytes_read = (a->compression_read_ahead)(a, &h, 512);
	if (bytes_read < 512)
	    return (-1);

	/*
	 * TODO: if checksum or header fail, scan ahead for
	 * next valid header.
	 */

	/* Checksum field is eight 8-bit values: 64 bits of validation. */
	if (!checksum(a, h))
	    return (0);
	bid += 64;

	header = (const struct archive_entry_header_gnutar *)h;

	/* This distinguishes GNU tar formats from POSIX formats */
	if (memcmp(header->magic, "ustar  \0", 8) != 0)
	    return (0);
	bid += 64;

	return (bid);
}

static int
archive_read_format_gnutar_read_header(struct archive *a,
    struct archive_entry *entry)
{
	const void *h;
	ssize_t bytes;
	int oldstate;
	struct gnutar *gnutar;

	gnutar = *(a->pformat_data);
	a->archive_format = ARCHIVE_FORMAT_TAR_GNUTAR;
	a->archive_format_name = "GNU tar";

	/* Skip remains of previous entry. */
	oldstate = a->state;
	a->state = ARCHIVE_STATE_DATA;
	archive_read_data_skip(a);
	a->state = oldstate;

	/* Read 512-byte header record */
	bytes = (a->compression_read_ahead)(a, &h, 512);
	if (bytes < 512)
		return (ARCHIVE_FATAL);
	(a->compression_read_consume)(a, 512);

	/*
	 * If this is a block of nulls, return 0 (no more entries).
	 * Note the initial (*h)==0 test short-circuits the function call
	 * in the most common case.
	 */
	if (((*(const char *)h)==0) && archive_block_is_null(h)) {
	    /* TODO: Store file location of start of block in public area */
	    archive_set_error(a, 0, NULL);
	    return (ARCHIVE_EOF);
	}

	/* TODO: add support for scanning for next valid header */
	if (!checksum(a, h)) {
	    archive_set_error(a, EINVAL, "Damaged GNU tar archive");
	    return (ARCHIVE_FATAL); /* Not a valid header. */
	}

	/* This function gets called recursively for long name headers, etc. */
	if (++gnutar->gnu_header_recursion_depth > 32)
	    errx(EINVAL,
		 "*** Too many special headers for one entry; giving up. "
		 "(%s:%s@%d)\n",
		 __FUNCTION__, __FILE__, __LINE__);

	archive_header_gnu(a, entry, h);
	gnutar->gnu_header_recursion_depth--;
	return (0);
}

/*
 * Return true if block checksum is correct.
 */
static int
checksum(struct archive *a, const void *h)
{
	const unsigned char *bytes;
	const struct archive_entry_header_gnutar *header;
	int i, sum, signed_sum, unsigned_sum;

	(void)a; /* UNUSED */
	bytes = h;
	header = h;

	/* Test checksum: POSIX specifies UNSIGNED for this calculation. */
	sum = tar_atol(header->checksum, sizeof(header->checksum));
	unsigned_sum = 0;
	for (i = 0; i < 148; i++)
		unsigned_sum += (unsigned char)bytes[i];
	for (; i < 156; i++)
		unsigned_sum += 32;
	for (; i < 512; i++)
		unsigned_sum += (unsigned char)bytes[i];
	if (sum == unsigned_sum)
		return (1);

	/*
	 * Repeat test with SIGNED bytes, just in case this archive
	 * was created by an old BSD, Solaris, or HP-UX tar with a broken
	 * checksum calculation.
	 */
	signed_sum = 0;
	for (i = 0; i < 148; i++)
		signed_sum += (signed char)bytes[i];
	for (; i < 156; i++)
		signed_sum += 32;
	for (; i < 512; i++)
		signed_sum += (signed char)bytes[i];
	if (sum == signed_sum)
		return (1);

	return (0);
}

/*
 * Return true if this block contains only nulls.
 */
static int
archive_block_is_null(const unsigned char *p)
{
	unsigned i;

	for (i = 0; i < ARCHIVE_BYTES_PER_RECORD / sizeof(*p); i++) {
		if (*p++)
			return (0);
	}
	return (1);
}

/*
 * Parse GNU tar header
 */
static int
archive_header_gnu(struct archive *a, struct archive_entry *entry,
    const void *h)
{
	struct stat st;
	const struct archive_entry_header_gnutar *header;
	struct gnutar *gnutar;
	char tartype;
	unsigned oldstate;

	/* Clear out entry structure */
	memset(&st, 0, sizeof(st));
	gnutar = *(a->pformat_data);

	/*
	 * GNU header is like POSIX, except 'prefix' is
	 * replaced with some other fields. This also means the
	 * filename is stored as in old-style archives.
	 */

	/* Copy filename over (to ensure null termination). */
	header = h;
	archive_strncpy(&(gnutar->entry_name), header->name,
	    sizeof(header->name));
	archive_entry_set_pathname(entry, gnutar->entry_name.s);

	/* Copy linkname over */
	if (header->linkname[0])
		archive_strncpy(&(gnutar->entry_linkname), header->linkname,
		    sizeof(header->linkname));

	/* Parse out the numeric fields (all are octal) */
	st.st_mode  = tar_atol(header->mode, sizeof(header->mode));
	st.st_uid   = tar_atol(header->uid, sizeof(header->uid));
	st.st_gid   = tar_atol(header->gid, sizeof(header->gid));
	st.st_size  = tar_atol(header->size, sizeof(header->size));
	st.st_mtime = tar_atol(header->mtime, sizeof(header->mtime));

	/* Handle the tar type flag appropriately. */
	tartype = header->typeflag[0];
	archive_entry_set_tartype(entry, tartype);
	st.st_mode &= ~S_IFMT;

	/* Fields common to ustar and GNU */
	archive_strncpy(&(gnutar->entry_uname),
	    header->uname, sizeof(header->uname));
	archive_entry_set_uname(entry, gnutar->entry_uname.s);

	archive_strncpy(&(gnutar->entry_gname),
	    header->gname, sizeof(header->gname));
	archive_entry_set_gname(entry, gnutar->entry_gname.s);

	/* Parse out device numbers only for char and block specials */
	if (header->typeflag[0] == '3' || header->typeflag[0] == '4')
		st.st_rdev = makedev (
		    tar_atol(header->devmajor, sizeof(header->devmajor)),
		    tar_atol(header->devminor, sizeof(header->devminor)));
	else
		st.st_rdev = 0;

	/* Grab additional GNU fields. */
	/* TODO: FILL THIS IN!!! */
	st.st_atime = tar_atol(header->atime, sizeof(header->atime));
	st.st_ctime = tar_atol(header->atime, sizeof(header->ctime));

	/* Set internal counter for locating next header */
	a->entry_bytes_remaining = st.st_size;
	a->entry_padding = 0x1ff & (-a->entry_bytes_remaining);

	/* Interpret entry type */
	switch (tartype) {
	case '1': /* Hard link */
		archive_entry_set_hardlink(entry, gnutar->entry_linkname.s);
		/*
		 * Note: Technically, tar does not store the file type
		 * for a "hard link" entry, only the fact that it is a
		 * hard link.  So, I leave the file type in st_mode
		 * zero here.
		 */
		archive_entry_copy_stat(entry, &st);
		break;
	case '2': /* Symlink */
		st.st_mode |= S_IFLNK;
		st.st_size = 0;
		archive_entry_set_symlink(entry, gnutar->entry_linkname.s);
		archive_entry_copy_stat(entry, &st);
		break;
	case '3': /* Character device */
		st.st_mode |= S_IFCHR;
		st.st_size = 0;
		archive_entry_copy_stat(entry, &st);
		break;
	case '4': /* Block device */
		st.st_mode |= S_IFBLK;
		st.st_size = 0;
		archive_entry_copy_stat(entry, &st);
		break;
	case '5': /* POSIX Dir */
		st.st_mode |= S_IFDIR;
		st.st_size = 0;
		archive_entry_copy_stat(entry, &st);
		break;
	case '6': /* FIFO device */
		st.st_mode |= S_IFIFO;
		st.st_size = 0;
		archive_entry_copy_stat(entry, &st);
		break;
	case 'D': /* GNU incremental directory type */
		/*
		 * No special handling is actually required here.
		 * It might be nice someday to preprocess the file list and
		 * provide it to the client, though.
		 */
		st.st_mode &= ~ S_IFMT;
		st.st_mode |= S_IFDIR;
		archive_entry_copy_stat(entry, &st);
		break;
	case 'K': /* GNU long linkname */
		/* Entry body is full name of link for next header. */
		archive_string_ensure(&(gnutar->gnu_linkname), st.st_size+1);
		/* Temporarily fudge internal state for read_data call. */
		oldstate = a->state;
		a->state = ARCHIVE_STATE_DATA;
		archive_read_data_into_buffer(a, gnutar->gnu_linkname.s,
		    st.st_size);
		a->state = oldstate;
		gnutar->gnu_linkname.s[st.st_size] = 0; /* Null term name! */
		/*
		 * This next call will usually overwrite
		 * gnutar->entry_linkname, which is why we _must_ have
		 * a separate gnu_linkname field.
		 */
		archive_read_format_gnutar_read_header(a, entry);
		if (archive_entry_tartype(entry) == '1')
			archive_entry_set_hardlink(entry, gnutar->gnu_linkname.s);
		else if (archive_entry_tartype(entry) == '2')
			archive_entry_set_symlink(entry, gnutar->gnu_linkname.s);
		/* TODO: else { ... } */
		break;
	case 'L': /* GNU long filename */
		/* Entry body is full pathname for next header. */
		archive_string_ensure(&(gnutar->gnu_name), st.st_size+1);
		/* Temporarily fudge internal state for read_data call. */
		oldstate = a->state;
		a->state = ARCHIVE_STATE_DATA;
		archive_read_data_into_buffer(a, gnutar->gnu_name.s,
		    st.st_size);
		a->state = oldstate;
		gnutar->gnu_name.s[st.st_size] = 0; /* Null terminate name! */
		/*
		 * This next call will typically overwrite
		 * gnutar->entry_name, which is why we _must_ have a
		 * separate gnu_name field.
		 */
		archive_read_format_gnutar_read_header(a, entry);
		archive_entry_set_pathname(entry, gnutar->gnu_name.s);
		break;
	case 'M': /* GNU Multi-volume (remainder of file from last archive) */
		/*
		 * As far as I can tell, this is just like a regular file
		 * entry, except that the contents should be _appended_ to
		 * the indicated file at the indicated offset.  This may
		 * require some API work to fully support.
		 */
		break;
	case 'N': /* Old GNU long filename; this will never be supported */
		/* Essentially, body of this entry is a script for
		 * renaming previously-extracted entries.  Ugh.  */
		break;
	case 'S': /* GNU Sparse files: These are really ugly, and unlikely
		   * to be supported anytime soon. */
		break;
	case 'V': /* GNU volume header */
		/* Just skip it */
		return (archive_read_format_gnutar_read_header(a, entry));
	default: /* Regular file  and non-standard types */
		/* Per POSIX: non-recognized types should always be
		 * treated as regular files.  Of course, GNU
		 * extensions aren't compatible with this dictum.
		 * <sigh> */
		st.st_mode |= S_IFREG;
		archive_entry_copy_stat(entry, &st);
		break;
	}

	return (0);
}

/*
 * Convert text->integer.
 *
 * Traditional tar formats (including POSIX) specify base-8 for
 * all of the standard numeric fields.  GNU tar supports base-256
 * as well in many of the numeric fields.  There is also an old
 * and short-lived base-64 format, but I doubt I'll ever see
 * an archive that uses it.  (According to the changelog for GNU tar,
 * that format was only implemented for a couple of weeks!)
 */
static int64_t
tar_atol(const char *p, unsigned char_cnt)
{
	if (*p & 0x80)
		return (tar_atol256(p, char_cnt));
	return (tar_atol8(p, char_cnt));
}

/*
 * Note that this implementation does not (and should not!) obey
 * locale settings; you cannot simply substitute strtol here, since
 * it does obey locale.
 */
static int64_t
tar_atol8(const char *p, unsigned char_cnt)
{
	int64_t	l;
	int digit, sign;

	static const int64_t	limit = INT64_MAX / 8;
	static const int	base = 8;
	static const char	last_digit_limit = INT64_MAX % 8;

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
			l = INT64_MAX; /* Truncate on overflow */
			break;
		}
		l = ( l * base ) + digit;
		digit = *++p - '0';
	}
	return (sign < 0) ? -l : l;
}

/*
 * Parse a base-256 integer.
 *
 * TODO: This overflows very quickly for negative values; fix this.
 */
static int64_t
tar_atol256(const char *p, unsigned char_cnt)
{
	int64_t	l;
	int digit;

	const int64_t	limit = INT64_MAX / 256;

	/* Ignore high bit of first byte (that's the base-256 flag). */
	l = 0;
	digit = 0x7f & *(const unsigned char *)p;
	while (char_cnt-- > 0) {
		if (l > limit) {
			l = INT64_MAX; /* Truncate on overflow */
			break;
		}
		l = (l << 8) + digit;
		digit = *(const unsigned char *)++p;
	}
	return (l);
}
