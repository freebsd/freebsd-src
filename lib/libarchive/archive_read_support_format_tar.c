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
#include <errno.h>
/* #include <stdint.h> */ /* See archive_platform.h */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"

/*
 * Layout of POSIX 'ustar' tar header.
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
	char	linkname[100];	/* "old format" header ends here */
	char	magic[6];	/* For POSIX: "ustar\0" */
	char	version[2];	/* For POSIX: "00" */
	char	uname[32];
	char	gname[32];
	char	devmajor[8];
	char	devminor[8];
	char	prefix[155];
};

/*
 * Data specific to this format.
 */
struct tar {
	struct archive_string	 acl_text;
	struct archive_string	 entry_name;
	struct archive_string	 entry_linkname;
	struct archive_string	 entry_uname;
	struct archive_string	 entry_gname;
	struct archive_string	 longlink;
	struct archive_string	 longname;
	struct archive_string	 pax_header;
	struct archive_string	 pax_global;
	wchar_t 		*pax_entry;
	size_t			 pax_entry_length;
	int			 header_recursion_depth;
};

static size_t	UTF8_mbrtowc(wchar_t * __restrict pwc,
		    const char * __restrict s, size_t n,
		    mbstate_t * __restrict ps __unused);
static int	archive_block_is_null(const unsigned char *p);
static int	header_Solaris_ACL(struct archive *,  struct tar *,
		    struct archive_entry *, struct stat *, const void *);
static int	header_common(struct archive *,  struct tar *,
		    struct archive_entry *, struct stat *, const void *);
static int	header_old_tar(struct archive *, struct tar *,
		    struct archive_entry *, struct stat *, const void *);
static int	header_pax_extensions(struct archive *, struct tar *,
		    struct archive_entry *, struct stat *, const void *);
static int	header_pax_global(struct archive *, struct tar *,
		    struct archive_entry *, struct stat *, const void *h);
static int	header_longlink(struct archive *, struct tar *,
		    struct archive_entry *, struct stat *, const void *h);
static int	header_longname(struct archive *, struct tar *,
		    struct archive_entry *, struct stat *, const void *h);
static int	header_ustar(struct archive *, struct tar *,
		    struct archive_entry *, struct stat *, const void *h);
static int	archive_read_format_tar_bid(struct archive *);
static int	archive_read_format_tar_cleanup(struct archive *);
static int	archive_read_format_tar_read_header(struct archive *,
		    struct archive_entry *);
static int	checksum(struct archive *, const void *);
static int 	pax_attribute(struct archive_entry *, struct stat *,
		    wchar_t *key, wchar_t *value);
static int 	pax_header(struct archive *, struct tar *,
		    struct archive_entry *, struct stat *, char *attr);
static void	pax_time(const wchar_t *, struct timespec *t);
static int	read_body_to_string(struct archive *, struct archive_string *,
		    const void *h);
static int64_t	tar_atol(const char *, unsigned);
static int64_t	tar_atol10(const wchar_t *, unsigned);
static int64_t	tar_atol256(const char *, unsigned);
static int64_t	tar_atol8(const char *, unsigned);
static int	tar_read_header(struct archive *, struct tar *,
		    struct archive_entry *, struct stat *);
static int	utf8_decode(wchar_t *, const char *, size_t length);

int
archive_read_support_format_tar(struct archive *a)
{
	struct tar *tar;

	tar = malloc(sizeof(*tar));
	memset(tar, 0, sizeof(*tar));

	return (__archive_read_register_format(a,
	    tar,
	    archive_read_format_tar_bid,
	    archive_read_format_tar_read_header,
	    archive_read_format_tar_cleanup));
}

static int
archive_read_format_tar_cleanup(struct archive *a)
{
	struct tar *tar;

	tar = *(a->pformat_data);
	if (tar->entry_name.s != NULL)
		free(tar->entry_name.s);
	if (tar->entry_linkname.s != NULL)
		free(tar->entry_linkname.s);
	if (tar->entry_uname.s != NULL)
		free(tar->entry_uname.s);
	if (tar->entry_gname.s != NULL)
		free(tar->entry_gname.s);
	if (tar->pax_header.s != NULL)
		free(tar->pax_header.s);
	if (tar->pax_global.s != NULL)
		free(tar->pax_global.s);

	free(tar);
	*(a->pformat_data) = NULL;
	return (ARCHIVE_OK);
}


static int
archive_read_format_tar_bid(struct archive *a)
{
	int bid;
	ssize_t bytes_read;
	const void *h;
	const struct archive_entry_header_ustar *header;

	/*
	 * If we're already reading a non-tar file, don't
	 * bother to bid.
	 */
	if (a->archive_format != 0 &&
	    (a->archive_format & ARCHIVE_FORMAT_BASE_MASK) !=
	    ARCHIVE_FORMAT_TAR)
		return (0);
	bid = 0;

	/*
	 * If we're already reading a tar format, start the bid at 1 as
	 * a failsafe.
	 */
	if ((a->archive_format & ARCHIVE_FORMAT_BASE_MASK) ==
	    ARCHIVE_FORMAT_TAR)
		bid++;

	/* If last header was my preferred format, bid a bit more. */
	if (a->archive_format == ARCHIVE_FORMAT_TAR_USTAR ||
	    a->archive_format == ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE)
		bid++;

	/* Now let's look at the actual header and see if it matches. */
	bytes_read = (a->compression_read_ahead)(a, &h, 512);
	if (bytes_read < 512)
		return (ARCHIVE_FATAL);

	/* If it's an end-of-archive mark, we can handle it. */
	if ((*(const char *)h) == 0 && archive_block_is_null(h))
		return (bid + 1);

	/* If it's not an end-of-archive mark, it must have a valid checksum.*/
	if (!checksum(a, h))
		return (0);
	bid += 48;  /* Checksum is usually 6 octal digits. */

	header = h;

	/* This distinguishes POSIX formats from GNU tar formats. */
	if ((memcmp(header->magic, "ustar\0", 6) == 0)
	    &&(memcmp(header->version, "00", 2)==0))
		bid += 56;

	/* Type flag must be null, digit or A-Z, a-z. */
	if (header->typeflag[0] != 0 &&
	    !( header->typeflag[0] >= '0' && header->typeflag[0] <= '9') &&
	    !( header->typeflag[0] >= 'A' && header->typeflag[0] <= 'Z') &&
	    !( header->typeflag[0] >= 'a' && header->typeflag[0] <= 'z') )
		return (0);

	/* Sanity check: Look at first byte of mode field. */
	switch (255 & (unsigned)header->mode[0]) {
	case 0: case 255:
		/* Base-256 value: No further verification possible! */
		break;
	case ' ': /* Not recommended, but not illegal, either. */
		break;
	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7':
		/* Octal Value. */
		/* TODO: Check format of remainder of this field. */
		break;
	default:
		/* Not a valid mode; bail out here. */
		return (0);
	}
	/* TODO: Sanity test uid/gid/size/mtime/devmajor/devminor fields. */

	return (bid);
}

/*
 * The function invoked by archive_read_header().  This
 * just sets up a few things and then calls the internal
 * tar_read_header() function below.
 */
static int
archive_read_format_tar_read_header(struct archive *a,
    struct archive_entry *entry)
{
	struct stat st;
	struct tar *tar;

	memset(&st, 0, sizeof(st));
	tar = *(a->pformat_data);

	return (tar_read_header(a, tar, entry, &st));
}

/*
 * This function recursively interprets all of the headers associated
 * with a single entry.
 */
static int
tar_read_header(struct archive *a, struct tar *tar,
    struct archive_entry *entry, struct stat *st)
{
	ssize_t bytes;
	int err;
	const void *h;
	const struct archive_entry_header_ustar *header;

	/* Read 512-byte header record */
	bytes = (a->compression_read_ahead)(a, &h, 512);
	if (bytes < 512) {
		/* TODO: Set error values */
		return (-1);
	}
	(a->compression_read_consume)(a, 512);

	/* Check for end-of-archive mark. */
	if (((*(const char *)h)==0) && archive_block_is_null(h)) {
		/* TODO: Store file location of start of block */
		archive_set_error(a, 0, NULL);
		return (ARCHIVE_EOF);
	}

	/*
	 * Note: If the checksum fails and we return ARCHIVE_RETRY,
	 * then the client is likely to just retry.  This is a very
	 * crude way to search for the next valid header!
	 *
	 * TODO: Improve this by implementing a real header scan.
	 */
	if (!checksum(a, h)) {
		archive_set_error(a, EINVAL, "Damaged tar archive");
		return (ARCHIVE_RETRY); /* Retryable: Invalid header */
	}

	if (++tar->header_recursion_depth > 32) {
		archive_set_error(a, EINVAL, "Too many special headers");
		return (ARCHIVE_WARN);
	}

	/* Determine the format variant. */
	header = h;
	if (memcmp(header->magic, "ustar", 5) != 0) {
		a->archive_format = ARCHIVE_FORMAT_TAR;
		a->archive_format_name = "tar (non-POSIX)";
		err = header_old_tar(a, tar, entry, st, h);
	} else switch(header->typeflag[0]) {
	case 'A': /* Solaris tar ACL */
		a->archive_format = ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE;
		a->archive_format_name = "Solaris tar";
		err = header_Solaris_ACL(a, tar, entry, st, h);
		break;
	case 'g': /* POSIX-standard 'g' header. */
		a->archive_format = ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE;
		a->archive_format_name = "POSIX pax interchange format";
		err = header_pax_global(a, tar, entry, st, h);
		break;
	case 'K': /* Long link name (non-POSIX, but fairly common). */
		err = header_longlink(a, tar, entry, st, h);
		break;
	case 'L': /* Long filename (non-POSIX, but fairly common). */
		err = header_longname(a, tar, entry, st, h);
		break;
	case 'X': /* Used by SUN tar; same as 'x'. */
		a->archive_format = ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE;
		a->archive_format_name =
		    "POSIX pax interchange format (Sun variant)";
		err = header_pax_extensions(a, tar, entry, st, h);
		break;
	case 'x': /* POSIX-standard 'x' header. */
		a->archive_format = ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE;
		a->archive_format_name = "POSIX pax interchange format";
		err = header_pax_extensions(a, tar, entry, st, h);
		break;
	default:
		if (a->archive_format != ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE
		    && a->archive_format != ARCHIVE_FORMAT_TAR_USTAR) {
			a->archive_format = ARCHIVE_FORMAT_TAR_USTAR;
			a->archive_format_name = "POSIX ustar format";
		}
		err = header_ustar(a, tar, entry, st, h);
	}
	archive_entry_copy_stat(entry, st);
	--tar->header_recursion_depth;
	return (err);
}

/*
 * Return true if block checksum is correct.
 */
static int
checksum(struct archive *a, const void *h)
{
	const unsigned char *bytes;
	const struct archive_entry_header_ustar	*header;
	int check, i, sum;

	(void)a; /* UNUSED */
	bytes = h;
	header = h;

	/*
	 * Test the checksum.  Note that POSIX specifies _unsigned_
	 * bytes for this calculation.
	 */
	sum = tar_atol(header->checksum, sizeof(header->checksum));
	check = 0;
	for (i = 0; i < 148; i++)
		check += (unsigned char)bytes[i];
	for (; i < 156; i++)
		check += 32;
	for (; i < 512; i++)
		check += (unsigned char)bytes[i];
	if (sum == check)
		return (1);

	/*
	 * Repeat test with _signed_ bytes, just in case this archive
	 * was created by an old BSD, Solaris, or HP-UX tar with a
	 * broken checksum calculation.
	 */
	check = 0;
	for (i = 0; i < 148; i++)
		check += (signed char)bytes[i];
	for (; i < 156; i++)
		check += 32;
	for (; i < 512; i++)
		check += (signed char)bytes[i];
	if (sum == check)
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

	for (i = 0; i < ARCHIVE_BYTES_PER_RECORD / sizeof(*p); i++)
		if (*p++)
			return (0);
	return (1);
}

/*
 * Interpret 'A' Solaris ACL header
 */
static int
header_Solaris_ACL(struct archive *a, struct tar *tar,
    struct archive_entry *entry, struct stat *st, const void *h)
{
	int err, err2;

	err = read_body_to_string(a, &(tar->acl_text), h);
	err2 = tar_read_header(a, tar, entry, st);

	/* XXX DO SOMETHING WITH THE ACL!!! XXX */
	{
		const char *msg = "\nXXX Solaris ACL entries recognized but not yet handled!!\n";
		write(2, msg, strlen(msg));
	}

	return (err_combine(err, err2));
}

/*
 * Interpret 'K' long linkname header.
 */
static int
header_longlink(struct archive *a, struct tar *tar,
    struct archive_entry *entry, struct stat *st, const void *h)
{
	int err, err2;

	err = read_body_to_string(a, &(tar->longlink), h);
	err2 = tar_read_header(a, tar, entry, st);
	if (err == ARCHIVE_OK && err2 == ARCHIVE_OK) {
		if (archive_entry_tartype(entry) == '1')
			archive_entry_set_hardlink(entry, tar->longlink.s);
		else if (archive_entry_tartype(entry) == '2')
			archive_entry_set_symlink(entry, tar->longlink.s);
	}
	return (err_combine(err, err2));
}

/*
 * Interpret 'L' long filename header.
 */
static int
header_longname(struct archive *a, struct tar *tar,
    struct archive_entry *entry, struct stat *st, const void *h)
{
	int err, err2;

	err = read_body_to_string(a, &(tar->longname), h);
	/* Read and parse "real" header, then override name. */
	err2 = tar_read_header(a, tar, entry, st);
	if (err == ARCHIVE_OK && err2 == ARCHIVE_OK)
		archive_entry_set_pathname(entry, tar->longname.s);
	return (err_combine(err, err2));
}

/*
 * Read body of an archive entry into an archive_string object.
 */
static int
read_body_to_string(struct archive *a, struct archive_string *as, const void *h)
{
	const struct archive_entry_header_ustar *header;
	off_t size;
	unsigned oldstate;
	int err, err2;

	header = h;
	size  = tar_atol(header->size, sizeof(header->size));

	/* Temporarily fudge internal state for read_data call. */
	oldstate = a->state;
	a->state = ARCHIVE_STATE_DATA;

	/* Read the body into the string. */
	a->entry_bytes_remaining = size;
	a->entry_padding = 0x1ff & -size;
	archive_string_ensure(as, size+1);
	err = archive_read_data_into_buffer(a, as->s, size);
	as->s[size] = 0; /* Null terminate name! */
	err2 = archive_read_data_skip(a); /* Resync for next header. */

	/* Restore the state. */
	a->state = oldstate;

	return (err_combine(err, err2));
}

/*
 * Parse out common header elements.
 *
 * This would be the same as header_old_tar, except that the
 * filename is handled slightly differently for old and POSIX
 * entries  (POSIX entries support a 'prefix').  This factoring
 * allows header_old_tar and header_ustar
 * to handle filenames differently, while still putting most of the
 * common parsing into one place.
 */
static int
header_common(struct archive *a, struct tar *tar, struct archive_entry *entry,
    struct stat *st, const void *h)
{
	const struct archive_entry_header_ustar	*header;
	char	tartype;

	(void)a; /* UNUSED */

	header = h;
	if (header->linkname[0])
		archive_strncpy(&(tar->entry_linkname), header->linkname,
		    sizeof(header->linkname));
	else
		archive_string_empty(&(tar->entry_linkname));

	/* Parse out the numeric fields (all are octal) */
	st->st_mode  = tar_atol(header->mode, sizeof(header->mode));
	st->st_uid   = tar_atol(header->uid, sizeof(header->uid));
	st->st_gid   = tar_atol(header->gid, sizeof(header->gid));
	st->st_size  = tar_atol(header->size, sizeof(header->size));
	st->st_mtime = tar_atol(header->mtime, sizeof(header->mtime));

	/* Handle the tar type flag appropriately. */
	tartype = header->typeflag[0];
	archive_entry_set_tartype(entry, tartype);
	st->st_mode &= ~S_IFMT;

	switch (tartype) {
	case '1': /* Hard link */
		archive_entry_set_hardlink(entry, tar->entry_linkname.s);
		/*
		 * The following may seem odd, but: Technically, tar
		 * does not store the file type for a "hard link"
		 * entry, only the fact that it is a hard link.  So, I
		 * leave the type zero normally.  But, pax interchange
		 * format allows hard links to have data, which
		 * implies that the underlying entry is a regular
		 * file.
		 */
		if (st->st_size > 0)
			st->st_mode |= S_IFREG;
		break;
	case '2': /* Symlink */
		st->st_mode |= S_IFLNK;
		st->st_size = 0;
		archive_entry_set_symlink(entry, tar->entry_linkname.s);
		break;
	case '3': /* Character device */
		st->st_mode |= S_IFCHR;
		st->st_size = 0;
		break;
	case '4': /* Block device */
		st->st_mode |= S_IFBLK;
		st->st_size = 0;
		break;
	case '5': /* Dir */
		st->st_mode |= S_IFDIR;
		st->st_size = 0;
		break;
	case '6': /* FIFO device */
		st->st_mode |= S_IFIFO;
		st->st_size = 0;
		break;
	default: /* Regular file  and non-standard types */
		/*
		 * Per POSIX: non-recognized types should always be
		 * treated as regular files.
		 */
		st->st_mode |= S_IFREG;
		break;
	}
	return (0);
}

/*
 * Parse out header elements for "old-style" tar archives.
 */
static int
header_old_tar(struct archive *a, struct tar *tar, struct archive_entry *entry,
    struct stat *st, const void *h)
{
	const struct archive_entry_header_ustar	*header;

	/* Copy filename over (to ensure null termination). */
	header = h;
	archive_strncpy(&(tar->entry_name), header->name, sizeof(header->name));
	archive_entry_set_pathname(entry, tar->entry_name.s);

	/* Grab rest of common fields */
	header_common(a, tar, entry, st, h);

	/*
	 * TODO: Decide whether the following special handling
	 * is needed for POSIX headers.  Factor accordingly.
	 */

	/* "Regular" entry with trailing '/' is really directory. */
	if (S_ISREG(st->st_mode) &&
	    '/' == tar->entry_name.s[strlen(tar->entry_name.s) - 1]) {
		st->st_mode &= ~S_IFMT;
		st->st_mode |= S_IFDIR;
		archive_entry_set_tartype(entry, '5');
	}

	a->entry_bytes_remaining = st->st_size;
	a->entry_padding = 0x1ff & (-a->entry_bytes_remaining);
	return (0);
}

/*
 * Parse a file header for a pax extended archive entry.
 */
static int
header_pax_global(struct archive *a, struct tar *tar,
    struct archive_entry *entry, struct stat *st, const void *h)
{
	int err, err2;

	err = read_body_to_string(a, &(tar->pax_global), h);
	err2 = tar_read_header(a, tar, entry, st);
	return (err_combine(err, err2));
}

static int
header_pax_extensions(struct archive *a, struct tar *tar,
    struct archive_entry *entry, struct stat *st, const void *h)
{
	read_body_to_string(a, &(tar->pax_header), h);
	int err, err2;

	/* Parse the next header. */
	err = tar_read_header(a, tar, entry, st);

	/*
	 * TODO: Parse global/default options into 'entry' struct here
	 * before handling file-specific options.
	 *
	 * This design (parse standard header, then overwrite with pax
	 * extended attribute data) usually works well, but isn't ideal;
	 * it would be better to parse the pax extended attributes first
	 * and then skip any fields in the standard header that were
	 * defined in the pax header.
	 */
	err2 = pax_header(a, tar, entry, st, tar->pax_header.s);
	err =  err_combine(err, err2);
	a->entry_bytes_remaining = st->st_size;
	a->entry_padding = 0x1ff & (-a->entry_bytes_remaining);
	return (err);
}


/*
 * Parse a file header for a Posix "ustar" archive entry.  This also
 * handles "pax" or "extended ustar" entries.
 */
static int
header_ustar(struct archive *a, struct tar *tar, struct archive_entry *entry,
    struct stat *st, const void *h)
{
	const struct archive_entry_header_ustar	*header;

	header = h;

	/* Copy name into an internal buffer to ensure null-termination. */
	if (header->prefix[0]) {
		archive_strncpy(&(tar->entry_name), header->prefix,
		    sizeof(header->prefix));
		archive_strappend_char(&(tar->entry_name), '/');
		archive_strncat(&(tar->entry_name), header->name,
		    sizeof(header->name));
	} else
		archive_strncpy(&(tar->entry_name), header->name,
		    sizeof(header->name));

	archive_entry_set_pathname(entry, tar->entry_name.s);

	/* Handle rest of common fields. */
	header_common(a, tar, entry, st, h);

	/* Handle POSIX ustar fields. */
	archive_strncpy(&(tar->entry_uname), header->uname,
	    sizeof(header->uname));
	archive_entry_set_uname(entry, tar->entry_uname.s);

	archive_strncpy(&(tar->entry_gname), header->gname,
	    sizeof(header->gname));
	archive_entry_set_gname(entry, tar->entry_gname.s);

	/* Parse out device numbers only for char and block specials. */
	if (header->typeflag[0] == '3' || header->typeflag[0] == '4') {
		st->st_rdev = makedev(
		    tar_atol(header->devmajor, sizeof(header->devmajor)),
		    tar_atol(header->devminor, sizeof(header->devminor)));
	}

	a->entry_bytes_remaining = st->st_size;
	a->entry_padding = 0x1ff & (-a->entry_bytes_remaining);

	return (0);
}


/*
 * Parse the pax extended attributes record.
 *
 * Returns non-zero if there's an error in the data.
 */
static int
pax_header(struct archive *a, struct tar *tar, struct archive_entry *entry,
    struct stat *st, char *attr)
{
	size_t attr_length, l, line_length;
	char *line, *p;
	wchar_t *key, *wp, *value;
	int err, err2;

	attr_length = strlen(attr);
	err = ARCHIVE_OK;
	while (attr_length > 0) {
		/* Parse decimal length field at start of line. */
		line_length = 0;
		l = attr_length;
		line = p = attr; /* Record start of line. */
		while (l>0) {
			if (*p == ' ') {
				p++;
				l--;
				break;
			}
			if (*p < '0' || *p > '9')
				return (-1);
			line_length *= 10;
			line_length += *p - '0';
			if (line_length > 999999)
				return (-1);
			p++;
			l--;
		}

		if (line_length > attr_length)
			return (0);

		/* Ensure pax_entry buffer is big enough. */
		if (tar->pax_entry_length <= line_length) {
			if (tar->pax_entry_length <= 0)
				tar->pax_entry_length = 256;
			while (tar->pax_entry_length <= line_length + 1)
				tar->pax_entry_length *= 2;

			/* XXX Error handling here */
			tar->pax_entry = realloc(tar->pax_entry,
			    tar->pax_entry_length * sizeof(wchar_t));
		}

		/* Decode UTF-8 to wchar_t, null-terminate result. */
		if (utf8_decode(tar->pax_entry, p,
			line_length - (p - attr) - 1)) {
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			   "Invalid UTF8 character in pax extended attribute");
			err = err_combine(err, ARCHIVE_WARN);
		}

		/* Null-terminate 'key' value. */
		key = tar->pax_entry;
		if (key[0] == L'=')
			return (-1);
		wp = wcschr(key, L'=');
		if (wp == NULL) {
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Invalid pax extended attributes");
			return (ARCHIVE_WARN);
		}
		*wp = 0;

		/* Identify null-terminated 'value' portion. */
		value = wp + 1;

		/* Identify this attribute and set it in the entry. */
		err2 = pax_attribute(entry, st, key, value);
		err = err_combine(err, err2);

		/* Skip to next line */
		attr += line_length;
		attr_length -= line_length;
	}
	return (err);
}



/*
 * Parse a single key=value attribute.  key/value pointers are
 * assumed to point into reasonably long-lived storage.
 *
 * Note that POSIX reserves all-lowercase keywords.  Vendor-specific
 * extensions should always have keywords of the form "VENDOR.attribute"
 * In particular, it's quite feasible to support many different
 * vendor extensions here.  I'm using "LIBARCHIVE" for extensions
 * unique to this library (currently, there are none).
 *
 * Investigate other vendor-specific extensions, as well and see if
 * any of them look useful.
 */
static int
pax_attribute(struct archive_entry *entry, struct stat *st,
    wchar_t *key, wchar_t *value)
{
	switch (key[0]) {
	case 'L':
		/* Our extensions */
/* TODO: Handle arbitrary extended attributes... */
/*
		if (strcmp(key, "LIBARCHIVE.xxxxxxx")==0)
			archive_entry_set_xxxxxx(entry, value);
*/
		break;
	case 'S':
		/* We support some keys used by the "star" archiver */
		if (wcscmp(key, L"SCHILY.acl.access")==0)
			archive_entry_copy_acl_w(entry, value);
		else if (wcscmp(key, L"SCHILY.acl.default")==0)
			archive_entry_copy_acl_default_w(entry, value);
		else if (wcscmp(key, L"SCHILY.devmajor")==0)
			st->st_rdev = makedev(tar_atol10(value, wcslen(value)),
			    minor(st->st_dev));
		else if (wcscmp(key, L"SCHILY.devminor")==0)
			st->st_rdev = makedev(major(st->st_dev),
			    tar_atol10(value, wcslen(value)));
		else if (wcscmp(key, L"SCHILY.fflags")==0)
			archive_entry_copy_fflags_w(entry, value);
		else if (wcscmp(key, L"SCHILY.nlink")==0)
			st->st_nlink = tar_atol10(value, wcslen(value));
		break;
	case 'a':
		if (wcscmp(key, L"atime")==0)
			pax_time(value, &(st->st_atimespec));
		break;
	case 'c':
		if (wcscmp(key, L"ctime")==0)
			pax_time(value, &(st->st_ctimespec));
		else if (wcscmp(key, L"charset")==0) {
			/* TODO: Publish charset information in entry. */
		} else if (wcscmp(key, L"comment")==0) {
			/* TODO: Publish comment in entry. */
		}
		break;
	case 'g':
		if (wcscmp(key, L"gid")==0)
			st->st_gid = tar_atol10(value, wcslen(value));
		else if (wcscmp(key, L"gname")==0)
			archive_entry_copy_gname_w(entry, value);
		break;
	case 'l':
		/* pax interchange doesn't distinguish hardlink vs. symlink. */
		if (wcscmp(key, L"linkpath")==0) {
			if (archive_entry_hardlink(entry))
				archive_entry_copy_hardlink_w(entry, value);
			else
				archive_entry_copy_symlink_w(entry, value);
		}
		break;
	case 'm':
		if (wcscmp(key, L"mtime")==0)
			pax_time(value, &(st->st_mtimespec));
		break;
	case 'p':
		if (wcscmp(key, L"path")==0)
			archive_entry_copy_pathname_w(entry, value);
		break;
	case 'r':
		/* POSIX has reserved 'realtime.*' */
		break;
	case 's':
		/* POSIX has reserved 'security.*' */
		/* Someday: if (wcscmp(key, L"security.acl")==0) { ... } */
		if (wcscmp(key, L"size")==0)
			st->st_size = tar_atol10(value, wcslen(value));
		break;
	case 'u':
		if (wcscmp(key, L"uid")==0)
			st->st_uid = tar_atol10(value, wcslen(value));
		else if (wcscmp(key, L"uname")==0)
			archive_entry_copy_uname_w(entry, value);
		break;
	}
	return (0);
}



/*
 * parse a decimal time value, which may include a fractional portion
 */
static void
pax_time(const wchar_t *p, struct timespec *t)
{
	char digit;
	int64_t	s;
	unsigned long l;
	int sign;

	static const int64_t limit64 = INT64_MAX / 10;
	static const char last_digit_limit64 = INT64_MAX % 10;

	s = 0;
	sign = 1;
	if (*p == '-') {
		sign = -1;
		p++;
	}
	while (*p >= '0' && *p <= '9') {
		digit = *p - '0';
		if (s > limit64 ||
		    (s == limit64 && digit > last_digit_limit64)) {
			s = UINT64_MAX; /* Truncate on overflow */
			break;
		}
		s = (s * 10) + digit;
		++p;
	}

	t->tv_sec = s * sign;

	/* Calculate nanoseconds. */
	t->tv_nsec = 0;

	if (*p != '.')
		return;

	l = 100000000UL;
	do {
		++p;
		if (*p >= '0' && *p <= '9')
			t->tv_nsec += (*p - '0') * l;
		else
			break;
	} while (l /= 10);
}

/*-
 * Convert text->integer.
 *
 * Traditional tar formats (including POSIX) specify base-8 for
 * all of the standard numeric fields.  This is a significant limitation
 * in practice:
 *   = file size is limited to 8GB
 *   = devmajor and devminor are limited to 21 bits
 *   = uid/gid are limited to 21 bits
 *
 * There are two workarounds for this:
 *   = pax extended headers, which use variable-length string fields
 *   = GNU tar and STAR both allow either base-8 or base-256 in
 *      most fields.  The high bit is set to indicate base-256.
 *
 * On read, this implementation supports both extensions.
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
			l = UINT64_MAX; /* Truncate on overflow */
			break;
		}
		l = (l * base) + digit;
		digit = *++p - '0';
	}
	return (sign < 0) ? -l : l;
}


/*
 * Note that this implementation does not (and should not!) obey
 * locale settings; you cannot simply substitute strtol here, since
 * it does obey locale.
 */
static int64_t
tar_atol10(const wchar_t *p, unsigned char_cnt)
{
	int64_t l;
	int digit, sign;

	static const int64_t	limit = INT64_MAX / 10;
	static const int	base = 10;
	static const char	last_digit_limit = INT64_MAX % 10;

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
			l = UINT64_MAX; /* Truncate on overflow */
			break;
		}
		l = (l * base) + digit;
		digit = *++p - '0';
	}
	return (sign < 0) ? -l : l;
}



/*
 * Parse a base-256 integer.
 */
static int64_t
tar_atol256(const char *p, unsigned char_cnt)
{
	int64_t	l;
	int digit;

	const int64_t limit = INT64_MAX / 256;

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

static int
utf8_decode(wchar_t *dest, const char *src, size_t length)
{
	size_t n;
	int err;

	err = 0;
	while(length > 0) {
		n = UTF8_mbrtowc(dest, src, length, NULL);
		if (n == 0)
			break;
		if (n > 8) {
			/* Invalid byte encountered; try to keep going. */
			*dest = L'?';
			n = 1;
			err = 1;
		}
		dest++;
		src += n;
		length -= n;
	}
	*dest++ = L'\0';
	return (err);
}

/*
 * Copied from FreeBSD libc/locale.
 */
static size_t
UTF8_mbrtowc(wchar_t * __restrict pwc, const char * __restrict s, size_t n,
    mbstate_t * __restrict ps __unused)
{
        int ch, i, len, mask;
        wchar_t lbound, wch;

        if (s == NULL)
                /* Reset to initial shift state (no-op) */
                return (0);
        if (n == 0)
                /* Incomplete multibyte sequence */
                return ((size_t)-2);

        /*
         * Determine the number of octets that make up this character from
         * the first octet, and a mask that extracts the interesting bits of
         * the first octet.
         *
         * We also specify a lower bound for the character code to detect
         * redundant, non-"shortest form" encodings. For example, the
         * sequence C0 80 is _not_ a legal representation of the null
         * character. This enforces a 1-to-1 mapping between character
         * codes and their multibyte representations.
         */
        ch = (unsigned char)*s;
        if ((ch & 0x80) == 0) {
                mask = 0x7f;
                len = 1;
                lbound = 0;
        } else if ((ch & 0xe0) == 0xc0) {
                mask = 0x1f;
                len = 2;
                lbound = 0x80;
        } else if ((ch & 0xf0) == 0xe0) {
                mask = 0x0f;
                len = 3;
                lbound = 0x800;
        } else if ((ch & 0xf8) == 0xf0) {
                mask = 0x07;
                len = 4;
                lbound = 0x10000;
        } else if ((ch & 0xfc) == 0xf8) {
                mask = 0x03;
                len = 5;
                lbound = 0x200000;
        } else if ((ch & 0xfc) == 0xfc) {
                mask = 0x01;
                len = 6;
                lbound = 0x4000000;
        } else {
                /*
                 * Malformed input; input is not UTF-8.
                 */
                errno = EILSEQ;
                return ((size_t)-1);
        }

        if (n < (size_t)len)
                /* Incomplete multibyte sequence */
                return ((size_t)-2);

        /*
         * Decode the octet sequence representing the character in chunks
         * of 6 bits, most significant first.
         */
        wch = (unsigned char)*s++ & mask;
        i = len;
        while (--i != 0) {
                if ((*s & 0xc0) != 0x80) {
                        /*
                         * Malformed input; bad characters in the middle
                         * of a character.
                         */
                        errno = EILSEQ;
                        return ((size_t)-1);
                }
                wch <<= 6;
                wch |= *s++ & 0x3f;
        }
        if (wch < lbound) {
                /*
                 * Malformed input; redundant encoding.
                 */
                errno = EILSEQ;
                return ((size_t)-1);
        }
        if (pwc != NULL)
                *pwc = wch;
        return (wch == L'\0' ? 0 : len);
}
