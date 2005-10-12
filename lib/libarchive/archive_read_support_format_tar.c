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
#include <stddef.h>
/* #include <stdint.h> */ /* See archive_platform.h */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Obtain suitable wide-character manipulation functions. */
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#else
static int wcscmp(const wchar_t *s1, const wchar_t *s2)
{
	int diff = *s1 - *s2;
	while(*s1 && diff == 0)
		diff = (int)*++s1 - (int)*++s2;
	return diff;
}
static size_t wcslen(const wchar_t *s)
{
	const wchar_t *p = s;
	while (*p)
		p++;
	return p - s;
}
#endif

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
	char	rdevmajor[8];
	char	rdevminor[8];
	char	prefix[155];
};

/*
 * Structure of GNU tar header
 */
struct gnu_sparse {
	char	offset[12];
	char	numbytes[12];
};

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
	char	rdevmajor[8];
	char	rdevminor[8];
	char	atime[12];
	char	ctime[12];
	char	offset[12];
	char	longnames[4];
	char	unused[1];
	struct gnu_sparse sparse[4];
	char	isextended[1];
	char	realsize[12];
	/*
	 * GNU doesn't use POSIX 'prefix' field; they use the 'L' (longname)
	 * entry instead.
	 */
};

/*
 * Data specific to this format.
 */
struct sparse_block {
	struct sparse_block	*next;
	off_t	offset;
	off_t	remaining;
};

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
	off_t			 entry_bytes_remaining;
	off_t			 entry_offset;
	off_t			 entry_padding;
	struct sparse_block	*sparse_list;
};

static size_t	UTF8_mbrtowc(wchar_t *pwc, const char *s, size_t n);
static int	archive_block_is_null(const unsigned char *p);
int		gnu_read_sparse_data(struct archive *, struct tar *,
		    const struct archive_entry_header_gnutar *header);
void		gnu_parse_sparse_data(struct archive *, struct tar *,
		    const struct gnu_sparse *sparse, int length);
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
static int	header_volume(struct archive *, struct tar *,
		    struct archive_entry *, struct stat *, const void *h);
static int	header_ustar(struct archive *, struct tar *,
		    struct archive_entry *, struct stat *, const void *h);
static int	header_gnutar(struct archive *, struct tar *,
		    struct archive_entry *, struct stat *, const void *h);
static int	archive_read_format_tar_bid(struct archive *);
static int	archive_read_format_tar_cleanup(struct archive *);
static int	archive_read_format_tar_read_data(struct archive *a,
		    const void **buff, size_t *size, off_t *offset);
static int	archive_read_format_tar_read_header(struct archive *,
		    struct archive_entry *);
static int	checksum(struct archive *, const void *);
static int 	pax_attribute(struct archive_entry *, struct stat *,
		    wchar_t *key, wchar_t *value);
static int 	pax_header(struct archive *, struct tar *,
		    struct archive_entry *, struct stat *, char *attr);
static void	pax_time(const wchar_t *, int64_t *sec, long *nanos);
static int	read_body_to_string(struct archive *, struct tar *,
		    struct archive_string *, const void *h);
static int64_t	tar_atol(const char *, unsigned);
static int64_t	tar_atol10(const wchar_t *, unsigned);
static int64_t	tar_atol256(const char *, unsigned);
static int64_t	tar_atol8(const char *, unsigned);
static int	tar_read_header(struct archive *, struct tar *,
		    struct archive_entry *, struct stat *);
static int	utf8_decode(wchar_t *, const char *, size_t length);

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
archive_read_support_format_gnutar(struct archive *a)
{
	return (archive_read_support_format_tar(a));
}


int
archive_read_support_format_tar(struct archive *a)
{
	struct tar *tar;
	int r;

	tar = malloc(sizeof(*tar));
	if (tar == NULL) {
		archive_set_error(a, ENOMEM, "Can't allocate tar data");
		return (ARCHIVE_FATAL);
	}
	memset(tar, 0, sizeof(*tar));

	r = __archive_read_register_format(a, tar,
	    archive_read_format_tar_bid,
	    archive_read_format_tar_read_header,
	    archive_read_format_tar_read_data,
	    NULL,
	    archive_read_format_tar_cleanup);

	if (r != ARCHIVE_OK)
		free(tar);
	return (ARCHIVE_OK);
}

static int
archive_read_format_tar_cleanup(struct archive *a)
{
	struct tar *tar;

	tar = *(a->pformat_data);
	archive_string_free(&tar->acl_text);
	archive_string_free(&tar->entry_name);
	archive_string_free(&tar->entry_linkname);
	archive_string_free(&tar->entry_uname);
	archive_string_free(&tar->entry_gname);
	archive_string_free(&tar->pax_global);
	archive_string_free(&tar->pax_header);
	if (tar->pax_entry != NULL)
		free(tar->pax_entry);
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

	/* Now let's look at the actual header and see if it matches. */
	if (a->compression_read_ahead != NULL)
		bytes_read = (a->compression_read_ahead)(a, &h, 512);
	else
		bytes_read = 0; /* Empty file. */
	if (bytes_read < 0)
		return (ARCHIVE_FATAL);
	if (bytes_read == 0  &&  bid > 0) {
		/* An archive without a proper end-of-archive marker. */
		/* Hold our nose and bid 1 anyway. */
		return (1);
	}
	if (bytes_read < 512) {
		/* If it's a new archive, then just return a zero bid. */
		if (bid == 0)
			return (0);
		/*
		 * If we already know this is a tar archive,
		 * then we have a problem.
		 */
		archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated tar archive");
		return (ARCHIVE_FATAL);
	}

	/* If it's an end-of-archive mark, we can handle it. */
	if ((*(const char *)h) == 0 && archive_block_is_null(h)) {
		/* If it's a known tar file, end-of-archive is definite. */
		if ((a->archive_format & ARCHIVE_FORMAT_BASE_MASK) ==
		    ARCHIVE_FORMAT_TAR)
			return (512);
		/* Empty archive? */
		return (1);
	}

	/* If it's not an end-of-archive mark, it must have a valid checksum.*/
	if (!checksum(a, h))
		return (0);
	bid += 48;  /* Checksum is usually 6 octal digits. */

	header = h;

	/* Recognize POSIX formats. */
	if ((memcmp(header->magic, "ustar\0", 6) == 0)
	    &&(memcmp(header->version, "00", 2)==0))
		bid += 56;

	/* Recognize GNU tar format. */
	if ((memcmp(header->magic, "ustar ", 6) == 0)
	    &&(memcmp(header->version, " \0", 2)==0))
		bid += 56;

	/* Type flag must be null, digit or A-Z, a-z. */
	if (header->typeflag[0] != 0 &&
	    !( header->typeflag[0] >= '0' && header->typeflag[0] <= '9') &&
	    !( header->typeflag[0] >= 'A' && header->typeflag[0] <= 'Z') &&
	    !( header->typeflag[0] >= 'a' && header->typeflag[0] <= 'z') )
		return (0);
	bid += 2;  /* 6 bits of variation in an 8-bit field leaves 2 bits. */

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
	/* TODO: Sanity test uid/gid/size/mtime/rdevmajor/rdevminor fields. */

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
	/*
	 * When converting tar archives to cpio archives, it is
	 * essential that each distinct file have a distinct inode
	 * number.  To simplify this, we keep a static count here to
	 * assign fake dev/inode numbers to each tar entry.  Note that
	 * pax format archives may overwrite this with something more
	 * useful.
	 *
	 * Ideally, we would track every file read from the archive so
	 * that we could assign the same dev/ino pair to hardlinks,
	 * but the memory required to store a complete lookup table is
	 * probably not worthwhile just to support the relatively
	 * obscure tar->cpio conversion case.
	 */
	static int default_inode;
	static int default_dev;
	struct stat st;
	struct tar *tar;
	const char *p;
	int r;
	size_t l;

	memset(&st, 0, sizeof(st));
	/* Assign default device/inode values. */
	st.st_dev = 1 + default_dev; /* Don't use zero. */
	st.st_ino = ++default_inode; /* Don't use zero. */
	/* Limit generated st_ino number to 16 bits. */
	if (default_inode >= 0xffff) {
		++default_dev;
		default_inode = 0;
	}

	tar = *(a->pformat_data);
	tar->entry_offset = 0;

	r = tar_read_header(a, tar, entry, &st);

	if (r == ARCHIVE_OK) {
		/*
		 * "Regular" entry with trailing '/' is really
		 * directory: This is needed for certain old tar
		 * variants and even for some broken newer ones.
		 */
		p = archive_entry_pathname(entry);
		l = strlen(p);
		if (S_ISREG(st.st_mode) && p[l-1] == '/') {
			st.st_mode &= ~S_IFMT;
			st.st_mode |= S_IFDIR;
		}

		/* Copy the final stat data into the entry. */
		archive_entry_copy_stat(entry, &st);
	}
	return (r);
}

static int
archive_read_format_tar_read_data(struct archive *a,
    const void **buff, size_t *size, off_t *offset)
{
	ssize_t bytes_read;
	struct tar *tar;
	struct sparse_block *p;

	tar = *(a->pformat_data);
	if (tar->sparse_list != NULL) {
		/* Remove exhausted entries from sparse list. */
		while (tar->sparse_list != NULL &&
		    tar->sparse_list->remaining == 0) {
			p = tar->sparse_list;
			tar->sparse_list = p->next;
			free(p);
		}
		if (tar->sparse_list == NULL) {
			/* We exhausted the entire sparse list. */
			tar->entry_bytes_remaining = 0;
		}
	}

	if (tar->entry_bytes_remaining > 0) {
		bytes_read = (a->compression_read_ahead)(a, buff, 1);
		if (bytes_read <= 0)
			return (ARCHIVE_FATAL);
		if (bytes_read > tar->entry_bytes_remaining)
			bytes_read = tar->entry_bytes_remaining;
		if (tar->sparse_list != NULL) {
			/* Don't read more than is available in the
			 * current sparse block. */
			if (tar->sparse_list->remaining < bytes_read)
				bytes_read = tar->sparse_list->remaining;
			tar->entry_offset = tar->sparse_list->offset;
			tar->sparse_list->remaining -= bytes_read;
			tar->sparse_list->offset += bytes_read;
		}
		*size = bytes_read;
		*offset = tar->entry_offset;
		tar->entry_offset += bytes_read;
		tar->entry_bytes_remaining -= bytes_read;
		(a->compression_read_consume)(a, bytes_read);
		return (ARCHIVE_OK);
	} else {
		while (tar->entry_padding > 0) {
			bytes_read = (a->compression_read_ahead)(a, buff, 1);
			if (bytes_read <= 0)
				return (ARCHIVE_FATAL);
			if (bytes_read > tar->entry_padding)
				bytes_read = tar->entry_padding;
			(a->compression_read_consume)(a, bytes_read);
			tar->entry_padding -= bytes_read;
		}
		*buff = NULL;
		*size = 0;
		*offset = tar->entry_offset;
		return (ARCHIVE_EOF);
	}
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
		/*
		 * If we're here, it's becase the _bid function accepted
		 * this file.  So just call a short read end-of-archive
		 * and be done with it.
		 */
		return (ARCHIVE_EOF);
	}
	(a->compression_read_consume)(a, 512);

	/* Check for end-of-archive mark. */
	if (((*(const char *)h)==0) && archive_block_is_null(h)) {
		/* Try to consume a second all-null record, as well. */
		bytes = (a->compression_read_ahead)(a, &h, 512);
		if (bytes > 0)
			(a->compression_read_consume)(a, bytes);
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
	switch(header->typeflag[0]) {
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
	case 'K': /* Long link name (GNU tar, others) */
		err = header_longlink(a, tar, entry, st, h);
		break;
	case 'L': /* Long filename (GNU tar, others) */
		err = header_longname(a, tar, entry, st, h);
		break;
	case 'V': /* GNU volume header */
		err = header_volume(a, tar, entry, st, h);
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
		if (memcmp(header->magic, "ustar  \0", 8) == 0) {
			a->archive_format = ARCHIVE_FORMAT_TAR_GNUTAR;
			a->archive_format_name = "GNU tar format";
			err = header_gnutar(a, tar, entry, st, h);
		} else if (memcmp(header->magic, "ustar", 5) == 0) {
			if (a->archive_format != ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE) {
				a->archive_format = ARCHIVE_FORMAT_TAR_USTAR;
				a->archive_format_name = "POSIX ustar format";
			}
			err = header_ustar(a, tar, entry, st, h);
		} else {
			a->archive_format = ARCHIVE_FORMAT_TAR;
			a->archive_format_name = "tar (non-POSIX)";
			err = header_old_tar(a, tar, entry, st, h);
		}
	}
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
	char *p;
	wchar_t *wp;

	err = read_body_to_string(a, tar, &(tar->acl_text), h);
	err2 = tar_read_header(a, tar, entry, st);
	err = err_combine(err, err2);

	/* XXX Ensure p doesn't overrun acl_text */

	/* Skip leading octal number. */
	/* XXX TODO: Parse the octal number and sanity-check it. */
	p = tar->acl_text.s;
	while (*p != '\0')
		p++;
	p++;

	wp = malloc((strlen(p) + 1) * sizeof(wchar_t));
	if (wp != NULL) {
		utf8_decode(wp, p, strlen(p));
		err2 = __archive_entry_acl_parse_w(entry, wp,
		    ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
		err = err_combine(err, err2);
		free(wp);
	}

	return (err);
}

/*
 * Interpret 'K' long linkname header.
 */
static int
header_longlink(struct archive *a, struct tar *tar,
    struct archive_entry *entry, struct stat *st, const void *h)
{
	int err, err2;

	err = read_body_to_string(a, tar, &(tar->longlink), h);
	err2 = tar_read_header(a, tar, entry, st);
	if (err == ARCHIVE_OK && err2 == ARCHIVE_OK) {
		/* Set symlink if symlink already set, else hardlink. */
		archive_entry_set_link(entry, tar->longlink.s);
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

	err = read_body_to_string(a, tar, &(tar->longname), h);
	/* Read and parse "real" header, then override name. */
	err2 = tar_read_header(a, tar, entry, st);
	if (err == ARCHIVE_OK && err2 == ARCHIVE_OK)
		archive_entry_set_pathname(entry, tar->longname.s);
	return (err_combine(err, err2));
}


/*
 * Interpret 'V' GNU tar volume header.
 */
static int
header_volume(struct archive *a, struct tar *tar,
    struct archive_entry *entry, struct stat *st, const void *h)
{
	(void)h;

	/* Just skip this and read the next header. */
	return (tar_read_header(a, tar, entry, st));
}

/*
 * Read body of an archive entry into an archive_string object.
 */
static int
read_body_to_string(struct archive *a, struct tar *tar,
    struct archive_string *as, const void *h)
{
	off_t size, padded_size;
	ssize_t bytes_read, bytes_to_copy;
	const struct archive_entry_header_ustar *header;
	const void *src;
	char *dest;

	(void)tar; /* UNUSED */
	header = h;
	size  = tar_atol(header->size, sizeof(header->size));

	/* Read the body into the string. */
	archive_string_ensure(as, size+1);
	padded_size = (size + 511) & ~ 511;
	dest = as->s;
	while (padded_size > 0) {
		bytes_read = (a->compression_read_ahead)(a, &src, padded_size);
		if (bytes_read < 0)
			return (ARCHIVE_FATAL);
		if (bytes_read > padded_size)
			bytes_read = padded_size;
		(a->compression_read_consume)(a, bytes_read);
		bytes_to_copy = bytes_read;
		if ((off_t)bytes_to_copy > size)
			bytes_to_copy = (ssize_t)size;
		memcpy(dest, src, bytes_to_copy);
		dest += bytes_to_copy;
		size -= bytes_to_copy;
		padded_size -= bytes_read;
	}
	*dest = '\0';
	return (ARCHIVE_OK);
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

		/*
		 * A tricky point: Traditionally, tar readers have
		 * ignored the size field when reading hardlink
		 * entries, and some writers put non-zero sizes even
		 * though the body is empty.  POSIX.1-2001 broke with
		 * this tradition by permitting hardlink entries to
		 * store valid bodies in pax interchange format, but
		 * not in ustar format.  Since there is no hard and
		 * fast way to distinguish pax interchange from
		 * earlier archives (the 'x' and 'g' entries are
		 * optional, after all), we need a heuristic.  Here, I
		 * use the bid function to test whether or not there's
		 * a valid header following.  Of course, if we know
		 * this is pax interchange format, then we must obey
		 * the size.
		 *
		 * This heuristic will only fail for a pax interchange
		 * archive that is storing hardlink bodies, no pax
		 * extended attribute entries have yet occurred, and
		 * we encounter a hardlink entry for a file that is
		 * itself an uncompressed tar archive.
		 */
		if (st->st_size > 0  &&
		    a->archive_format != ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE  &&
		    archive_read_format_tar_bid(a) > 50)
			st->st_size = 0;
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
	case 'D': /* GNU incremental directory type */
		/*
		 * No special handling is actually required here.
		 * It might be nice someday to preprocess the file list and
		 * provide it to the client, though.
		 */
		st->st_mode |= S_IFDIR;
		break;
	case 'M': /* GNU "Multi-volume" (remainder of file from last archive)*/
		/*
		 * As far as I can tell, this is just like a regular file
		 * entry, except that the contents should be _appended_ to
		 * the indicated file at the indicated offset.  This may
		 * require some API work to fully support.
		 */
		break;
	case 'N': /* Old GNU "long filename" entry. */
		/* The body of this entry is a script for renaming
		 * previously-extracted entries.  Ugh.  It will never
		 * be supported by libarchive. */
		st->st_mode |= S_IFREG;
		break;
	case 'S': /* GNU sparse files */
		/*
		 * Sparse files are really just regular files with
		 * sparse information in the extended area.
		 */
		/* FALL THROUGH */
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

	tar->entry_bytes_remaining = st->st_size;
	tar->entry_padding = 0x1ff & (-tar->entry_bytes_remaining);
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

	err = read_body_to_string(a, tar, &(tar->pax_global), h);
	err2 = tar_read_header(a, tar, entry, st);
	return (err_combine(err, err2));
}

static int
header_pax_extensions(struct archive *a, struct tar *tar,
    struct archive_entry *entry, struct stat *st, const void *h)
{
	int err, err2;

	read_body_to_string(a, tar, &(tar->pax_header), h);

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
	tar->entry_bytes_remaining = st->st_size;
	tar->entry_padding = 0x1ff & (-tar->entry_bytes_remaining);
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
	struct archive_string *as;

	header = h;

	/* Copy name into an internal buffer to ensure null-termination. */
	as = &(tar->entry_name);
	if (header->prefix[0]) {
		archive_strncpy(as, header->prefix, sizeof(header->prefix));
		if (as->s[archive_strlen(as) - 1] != '/')
			archive_strappend_char(as, '/');
		archive_strncat(as, header->name, sizeof(header->name));
	} else
		archive_strncpy(as, header->name, sizeof(header->name));

	archive_entry_set_pathname(entry, as->s);

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
		    tar_atol(header->rdevmajor, sizeof(header->rdevmajor)),
		    tar_atol(header->rdevminor, sizeof(header->rdevminor)));
	}

	tar->entry_bytes_remaining = st->st_size;
	tar->entry_padding = 0x1ff & (-tar->entry_bytes_remaining);

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
			if (line_length > 999999) {
				archive_set_error(a, ARCHIVE_ERRNO_MISC,
				    "Rejecting pax extended attribute > 1MB");
				return (ARCHIVE_WARN);
			}
			p++;
			l--;
		}

		if (line_length > attr_length)
			return (0);

		/* Ensure pax_entry buffer is big enough. */
		if (tar->pax_entry_length <= line_length) {
			wchar_t *old_entry = tar->pax_entry;

			if (tar->pax_entry_length <= 0)
				tar->pax_entry_length = 1024;
			while (tar->pax_entry_length <= line_length + 1)
				tar->pax_entry_length *= 2;

			old_entry = tar->pax_entry;
			tar->pax_entry = realloc(tar->pax_entry,
			    tar->pax_entry_length * sizeof(wchar_t));
			if (tar->pax_entry == NULL) {
				free(old_entry);
				archive_set_error(a, ENOMEM,
					"No memory");
				return (ARCHIVE_FATAL);
			}
		}

		/* Decode UTF-8 to wchar_t, null-terminate result. */
		if (utf8_decode(tar->pax_entry, p,
			line_length - (p - attr) - 1)) {
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			   "Invalid UTF8 character in pax extended attribute");
			err = err_combine(err, ARCHIVE_WARN);
		}

		/* Null-terminate 'key' value. */
		wp = key = tar->pax_entry;
		if (key[0] == L'=')
			return (-1);
		while (*wp && *wp != L'=')
			++wp;
		if (*wp == L'\0' || wp == NULL) {
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
	int64_t s;
	long n;

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
			__archive_entry_acl_parse_w(entry, value,
			    ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
		else if (wcscmp(key, L"SCHILY.acl.default")==0)
			__archive_entry_acl_parse_w(entry, value,
			    ARCHIVE_ENTRY_ACL_TYPE_DEFAULT);
		else if (wcscmp(key, L"SCHILY.devmajor")==0)
			st->st_rdev = makedev(tar_atol10(value, wcslen(value)),
			    minor(st->st_rdev));
		else if (wcscmp(key, L"SCHILY.devminor")==0)
			st->st_rdev = makedev(major(st->st_rdev),
			    tar_atol10(value, wcslen(value)));
		else if (wcscmp(key, L"SCHILY.fflags")==0)
			archive_entry_copy_fflags_text_w(entry, value);
		else if (wcscmp(key, L"SCHILY.nlink")==0)
			st->st_nlink = tar_atol10(value, wcslen(value));
		break;
	case 'a':
		if (wcscmp(key, L"atime")==0) {
			pax_time(value, &s, &n);
			st->st_atime = s;
			ARCHIVE_STAT_SET_ATIME_NANOS(st, n);
		}
		break;
	case 'c':
		if (wcscmp(key, L"ctime")==0) {
			pax_time(value, &s, &n);
			st->st_ctime = s;
			ARCHIVE_STAT_SET_CTIME_NANOS(st, n);
		} else if (wcscmp(key, L"charset")==0) {
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
		if (wcscmp(key, L"mtime")==0) {
			pax_time(value, &s, &n);
			st->st_mtime = s;
			ARCHIVE_STAT_SET_MTIME_NANOS(st, n);
		}
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
pax_time(const wchar_t *p, int64_t *ps, long *pn)
{
	char digit;
	int64_t	s;
	unsigned long l;
	int sign;
	int64_t limit, last_digit_limit;

	limit = max_int64 / 10;
	last_digit_limit = max_int64 % 10;

	s = 0;
	sign = 1;
	if (*p == '-') {
		sign = -1;
		p++;
	}
	while (*p >= '0' && *p <= '9') {
		digit = *p - '0';
		if (s > limit ||
		    (s == limit && digit > last_digit_limit)) {
			s = max_uint64;
			break;
		}
		s = (s * 10) + digit;
		++p;
	}

	*ps = s * sign;

	/* Calculate nanoseconds. */
	*pn = 0;

	if (*p != '.')
		return;

	l = 100000000UL;
	do {
		++p;
		if (*p >= '0' && *p <= '9')
			*pn += (*p - '0') * l;
		else
			break;
	} while (l /= 10);
}

/*
 * Parse GNU tar header
 */
static int
header_gnutar(struct archive *a, struct tar *tar, struct archive_entry *entry,
    struct stat *st, const void *h)
{
	const struct archive_entry_header_gnutar *header;

	(void)a;

	/*
	 * GNU header is like POSIX ustar, except 'prefix' is
	 * replaced with some other fields. This also means the
	 * filename is stored as in old-style archives.
	 */

	/* Grab fields common to all tar variants. */
	header_common(a, tar, entry, st, h);

	/* Copy filename over (to ensure null termination). */
	header = h;
	archive_strncpy(&(tar->entry_name), header->name,
	    sizeof(header->name));
	archive_entry_set_pathname(entry, tar->entry_name.s);

	/* Fields common to ustar and GNU */
	/* XXX Can the following be factored out since it's common
	 * to ustar and gnu tar?  Is it okay to move it down into
	 * header_common, perhaps?  */
	archive_strncpy(&(tar->entry_uname),
	    header->uname, sizeof(header->uname));
	archive_entry_set_uname(entry, tar->entry_uname.s);

	archive_strncpy(&(tar->entry_gname),
	    header->gname, sizeof(header->gname));
	archive_entry_set_gname(entry, tar->entry_gname.s);

	/* Parse out device numbers only for char and block specials */
	if (header->typeflag[0] == '3' || header->typeflag[0] == '4')
		st->st_rdev = makedev (
		    tar_atol(header->rdevmajor, sizeof(header->rdevmajor)),
		    tar_atol(header->rdevminor, sizeof(header->rdevminor)));
	else
		st->st_rdev = 0;

	tar->entry_bytes_remaining = st->st_size;
	tar->entry_padding = 0x1ff & (-tar->entry_bytes_remaining);

	/* Grab GNU-specific fields. */
	st->st_atime = tar_atol(header->atime, sizeof(header->atime));
	st->st_ctime = tar_atol(header->ctime, sizeof(header->ctime));
	if (header->realsize[0] != 0) {
		st->st_size = tar_atol(header->realsize,
		    sizeof(header->realsize));
	}

	if (header->sparse[0].offset[0] != 0) {
		gnu_read_sparse_data(a, tar, header);
	} else {
		if (header->isextended[0] != 0) {
			/* XXX WTF? XXX */
		}
	}

	return (0);
}

int
gnu_read_sparse_data(struct archive *a, struct tar *tar,
    const struct archive_entry_header_gnutar *header)
{
	ssize_t bytes_read;
	const void *data;
	struct extended {
		struct gnu_sparse sparse[21];
		char	isextended[1];
		char	padding[7];
	};
	const struct extended *ext;

	gnu_parse_sparse_data(a, tar, header->sparse, 4);
	if (header->isextended[0] == 0)
		return (ARCHIVE_OK);

	do {
		bytes_read = (a->compression_read_ahead)(a, &data, 512);
		if (bytes_read < 0)
			return (ARCHIVE_FATAL);
		if (bytes_read < 512) {
			archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
			    "Truncated tar archive "
			    "detected while reading sparse file data");
			return (ARCHIVE_FATAL);
		}
		(a->compression_read_consume)(a, 512);
		ext = (const struct extended *)data;
		gnu_parse_sparse_data(a, tar, ext->sparse, 21);
	} while (ext->isextended[0] != 0);
	if (tar->sparse_list != NULL)
		tar->entry_offset = tar->sparse_list->offset;
	return (ARCHIVE_OK);
}

void
gnu_parse_sparse_data(struct archive *a, struct tar *tar,
    const struct gnu_sparse *sparse, int length)
{
	struct sparse_block *last;
	struct sparse_block *p;

	(void)a; /* UNUSED */

	last = tar->sparse_list;
	while (last != NULL && last->next != NULL)
		last = last->next;

	while (length > 0 && sparse->offset[0] != 0) {
		p = malloc(sizeof(*p));
		if (p == NULL)
			__archive_errx(1, "Out of memory");
		memset(p, 0, sizeof(*p));
		if (last != NULL)
			last->next = p;
		else
			tar->sparse_list = p;
		last = p;
		p->offset = tar_atol(sparse->offset, sizeof(sparse->offset));
		p->remaining =
		    tar_atol(sparse->numbytes, sizeof(sparse->numbytes));
		sparse++;
		length--;
	}
}

/*-
 * Convert text->integer.
 *
 * Traditional tar formats (including POSIX) specify base-8 for
 * all of the standard numeric fields.  This is a significant limitation
 * in practice:
 *   = file size is limited to 8GB
 *   = rdevmajor and rdevminor are limited to 21 bits
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
	/*
	 * Technically, GNU tar considers a field to be in base-256
	 * only if the first byte is 0xff or 0x80.
	 */
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


/*
 * Note that this implementation does not (and should not!) obey
 * locale settings; you cannot simply substitute strtol here, since
 * it does obey locale.
 */
static int64_t
tar_atol10(const wchar_t *p, unsigned char_cnt)
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

/*
 * Parse a base-256 integer.  This is just a straight signed binary
 * value in big-endian order, except that the high-order bit is
 * ignored.  Remember that "int64_t" may or may not be exactly 64
 * bits; the implementation here tries to avoid making any assumptions
 * about the actual size of an int64_t.  It does assume we're using
 * twos-complement arithmetic, though.
 */
static int64_t
tar_atol256(const char *_p, unsigned char_cnt)
{
	int64_t	l, upper_limit, lower_limit;
	const unsigned char *p = (const unsigned char *)_p;

	upper_limit = max_int64 / 256;
	lower_limit = min_int64 / 256;

	/* Pad with 1 or 0 bits, depending on sign. */
	if ((0x40 & *p) == 0x40)
		l = (int64_t)-1;
	else
		l = 0;
	l = (l << 6) | (0x3f & *p++);
	while (--char_cnt > 0) {
		if (l > upper_limit) {
			l = max_int64; /* Truncate on overflow */
			break;
		} else if (l < lower_limit) {
			l = min_int64;
			break;
		}
		l = (l << 8) | (0xff & (int64_t)*p++);
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
		n = UTF8_mbrtowc(dest, src, length);
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
UTF8_mbrtowc(wchar_t *pwc, const char *s, size_t n)
{
        int ch, i, len, mask;
        unsigned long lbound, wch;

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
        if (pwc != NULL) {
		/* Assign the value to the output; out-of-range values
		 * just get truncated. */
		*pwc = (wchar_t)wch;
#ifdef WCHAR_MAX
		/*
		 * If platform has WCHAR_MAX, we can do something
		 * more sensible with out-of-range values.
		 */
		if (wch >= WCHAR_MAX)
			*pwc = '?';
#endif
	}
        return (wch == L'\0' ? 0 : len);
}
