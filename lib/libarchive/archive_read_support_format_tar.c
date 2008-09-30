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
#include <stddef.h>
/* #include <stdint.h> */ /* See archive_platform.h */
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

/* Obtain suitable wide-character manipulation functions. */
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#else
/* Good enough for equality testing, which is all we need. */
static int wcscmp(const wchar_t *s1, const wchar_t *s2)
{
	int diff = *s1 - *s2;
	while (*s1 && diff == 0)
		diff = (int)*++s1 - (int)*++s2;
	return diff;
}
/* Good enough for equality testing, which is all we need. */
static int wcsncmp(const wchar_t *s1, const wchar_t *s2, size_t n)
{
	int diff = *s1 - *s2;
	while (*s1 && diff == 0 && n-- > 0)
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
#include "archive_read_private.h"

#define tar_min(a,b) ((a) < (b) ? (a) : (b))

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
	 * Old GNU format doesn't use POSIX 'prefix' field; they use
	 * the 'L' (longname) entry instead.
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
	struct archive_string	 entry_pathname;
	/* For "GNU.sparse.name" and other similar path extensions. */
	struct archive_string	 entry_pathname_override;
	struct archive_string	 entry_linkpath;
	struct archive_string	 entry_uname;
	struct archive_string	 entry_gname;
	struct archive_string	 longlink;
	struct archive_string	 longname;
	struct archive_string	 pax_header;
	struct archive_string	 pax_global;
	struct archive_string	 line;
	int			 pax_hdrcharset_binary;
	wchar_t 		*pax_entry;
	size_t			 pax_entry_length;
	int			 header_recursion_depth;
	off_t			 entry_bytes_remaining;
	off_t			 entry_offset;
	off_t			 entry_padding;
	off_t			 realsize;
	struct sparse_block	*sparse_list;
	struct sparse_block	*sparse_last;
	int64_t			 sparse_offset;
	int64_t			 sparse_numbytes;
	int			 sparse_gnu_major;
	int			 sparse_gnu_minor;
	char			 sparse_gnu_pending;
};

static ssize_t	UTF8_mbrtowc(wchar_t *pwc, const char *s, size_t n);
static int	archive_block_is_null(const unsigned char *p);
static char	*base64_decode(const char *, size_t, size_t *);
static void	 gnu_add_sparse_entry(struct tar *,
		    off_t offset, off_t remaining);
static void	gnu_clear_sparse_list(struct tar *);
static int	gnu_sparse_old_read(struct archive_read *, struct tar *,
		    const struct archive_entry_header_gnutar *header);
static void	gnu_sparse_old_parse(struct tar *,
		    const struct gnu_sparse *sparse, int length);
static int	gnu_sparse_01_parse(struct tar *, const char *);
static ssize_t	gnu_sparse_10_read(struct archive_read *, struct tar *);
static int	header_Solaris_ACL(struct archive_read *,  struct tar *,
		    struct archive_entry *, const void *);
static int	header_common(struct archive_read *,  struct tar *,
		    struct archive_entry *, const void *);
static int	header_old_tar(struct archive_read *, struct tar *,
		    struct archive_entry *, const void *);
static int	header_pax_extensions(struct archive_read *, struct tar *,
		    struct archive_entry *, const void *);
static int	header_pax_global(struct archive_read *, struct tar *,
		    struct archive_entry *, const void *h);
static int	header_longlink(struct archive_read *, struct tar *,
		    struct archive_entry *, const void *h);
static int	header_longname(struct archive_read *, struct tar *,
		    struct archive_entry *, const void *h);
static int	header_volume(struct archive_read *, struct tar *,
		    struct archive_entry *, const void *h);
static int	header_ustar(struct archive_read *, struct tar *,
		    struct archive_entry *, const void *h);
static int	header_gnutar(struct archive_read *, struct tar *,
		    struct archive_entry *, const void *h);
static int	archive_read_format_tar_bid(struct archive_read *);
static int	archive_read_format_tar_cleanup(struct archive_read *);
static int	archive_read_format_tar_read_data(struct archive_read *a,
		    const void **buff, size_t *size, off_t *offset);
static int	archive_read_format_tar_skip(struct archive_read *a);
static int	archive_read_format_tar_read_header(struct archive_read *,
		    struct archive_entry *);
static int	checksum(struct archive_read *, const void *);
static int 	pax_attribute(struct tar *, struct archive_entry *,
		    char *key, char *value);
static int 	pax_header(struct archive_read *, struct tar *,
		    struct archive_entry *, char *attr);
static void	pax_time(const char *, int64_t *sec, long *nanos);
static ssize_t	readline(struct archive_read *, struct tar *, const char **,
		    ssize_t limit);
static int	read_body_to_string(struct archive_read *, struct tar *,
		    struct archive_string *, const void *h);
static int64_t	tar_atol(const char *, unsigned);
static int64_t	tar_atol10(const char *, unsigned);
static int64_t	tar_atol256(const char *, unsigned);
static int64_t	tar_atol8(const char *, unsigned);
static int	tar_read_header(struct archive_read *, struct tar *,
		    struct archive_entry *);
static int	tohex(int c);
static char	*url_decode(const char *);
static wchar_t	*utf8_decode(struct tar *, const char *, size_t length);

int
archive_read_support_format_gnutar(struct archive *a)
{
	return (archive_read_support_format_tar(a));
}


int
archive_read_support_format_tar(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct tar *tar;
	int r;

	tar = (struct tar *)malloc(sizeof(*tar));
	if (tar == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate tar data");
		return (ARCHIVE_FATAL);
	}
	memset(tar, 0, sizeof(*tar));

	r = __archive_read_register_format(a, tar,
	    archive_read_format_tar_bid,
	    archive_read_format_tar_read_header,
	    archive_read_format_tar_read_data,
	    archive_read_format_tar_skip,
	    archive_read_format_tar_cleanup);

	if (r != ARCHIVE_OK)
		free(tar);
	return (ARCHIVE_OK);
}

static int
archive_read_format_tar_cleanup(struct archive_read *a)
{
	struct tar *tar;

	tar = (struct tar *)(a->format->data);
	gnu_clear_sparse_list(tar);
	archive_string_free(&tar->acl_text);
	archive_string_free(&tar->entry_pathname);
	archive_string_free(&tar->entry_pathname_override);
	archive_string_free(&tar->entry_linkpath);
	archive_string_free(&tar->entry_uname);
	archive_string_free(&tar->entry_gname);
	archive_string_free(&tar->line);
	archive_string_free(&tar->pax_global);
	archive_string_free(&tar->pax_header);
	archive_string_free(&tar->longname);
	archive_string_free(&tar->longlink);
	free(tar->pax_entry);
	free(tar);
	(a->format->data) = NULL;
	return (ARCHIVE_OK);
}


static int
archive_read_format_tar_bid(struct archive_read *a)
{
	int bid;
	ssize_t bytes_read;
	const void *h;
	const struct archive_entry_header_ustar *header;

	bid = 0;

	/* Now let's look at the actual header and see if it matches. */
	if (a->decompressor->read_ahead != NULL)
		bytes_read = (a->decompressor->read_ahead)(a, &h, 512);
	else
		bytes_read = 0; /* Empty file. */
	if (bytes_read < 0)
		return (ARCHIVE_FATAL);
	if (bytes_read < 512)
		return (0);

	/* If it's an end-of-archive mark, we can handle it. */
	if ((*(const char *)h) == 0
	    && archive_block_is_null((const unsigned char *)h)) {
		/*
		 * Usually, I bid the number of bits verified, but
		 * in this case, 4096 seems excessive so I picked 10 as
		 * an arbitrary but reasonable-seeming value.
		 */
		return (10);
	}

	/* If it's not an end-of-archive mark, it must have a valid checksum.*/
	if (!checksum(a, h))
		return (0);
	bid += 48;  /* Checksum is usually 6 octal digits. */

	header = (const struct archive_entry_header_ustar *)h;

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
archive_read_format_tar_read_header(struct archive_read *a,
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
	struct tar *tar;
	struct sparse_block *sp;
	const char *p;
	int r;
	size_t l;

	/* Assign default device/inode values. */
	archive_entry_set_dev(entry, 1 + default_dev); /* Don't use zero. */
	archive_entry_set_ino(entry, ++default_inode); /* Don't use zero. */
	/* Limit generated st_ino number to 16 bits. */
	if (default_inode >= 0xffff) {
		++default_dev;
		default_inode = 0;
	}

	tar = (struct tar *)(a->format->data);
	tar->entry_offset = 0;
	while (tar->sparse_list != NULL) {
		sp = tar->sparse_list;
		tar->sparse_list = sp->next;
		free(sp);
	}
	tar->sparse_last = NULL;
	tar->realsize = -1; /* Mark this as "unset" */

	r = tar_read_header(a, tar, entry);

	/*
	 * "non-sparse" files are really just sparse files with
	 * a single block.
	 */
	if (tar->sparse_list == NULL)
		gnu_add_sparse_entry(tar, 0, tar->entry_bytes_remaining);

	if (r == ARCHIVE_OK) {
		/*
		 * "Regular" entry with trailing '/' is really
		 * directory: This is needed for certain old tar
		 * variants and even for some broken newer ones.
		 */
		p = archive_entry_pathname(entry);
		l = strlen(p);
		if (archive_entry_filetype(entry) == AE_IFREG
		    && p[l-1] == '/')
			archive_entry_set_filetype(entry, AE_IFDIR);
	}
	return (r);
}

static int
archive_read_format_tar_read_data(struct archive_read *a,
    const void **buff, size_t *size, off_t *offset)
{
	ssize_t bytes_read;
	struct tar *tar;
	struct sparse_block *p;

	tar = (struct tar *)(a->format->data);

	if (tar->sparse_gnu_pending) {
		if (tar->sparse_gnu_major == 1 && tar->sparse_gnu_minor == 0) {
			tar->sparse_gnu_pending = 0;
			/* Read initial sparse map. */
			bytes_read = gnu_sparse_10_read(a, tar);
			tar->entry_bytes_remaining -= bytes_read;
			if (bytes_read < 0)
				return (bytes_read);
		} else {
			*size = 0;
			*offset = 0;
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Unrecognized GNU sparse file format");
			return (ARCHIVE_WARN);
		}
		tar->sparse_gnu_pending = 0;
	}

	/* Remove exhausted entries from sparse list. */
	while (tar->sparse_list != NULL &&
	    tar->sparse_list->remaining == 0) {
		p = tar->sparse_list;
		tar->sparse_list = p->next;
		free(p);
	}

	/* If we're at end of file, return EOF. */
	if (tar->sparse_list == NULL || tar->entry_bytes_remaining == 0) {
		if ((a->decompressor->skip)(a, tar->entry_padding) < 0)
			return (ARCHIVE_FATAL);
		tar->entry_padding = 0;
		*buff = NULL;
		*size = 0;
		*offset = tar->realsize;
		return (ARCHIVE_EOF);
	}

	bytes_read = (a->decompressor->read_ahead)(a, buff, 1);
	if (bytes_read == 0) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Truncated tar archive");
		return (ARCHIVE_FATAL);
	}
	if (bytes_read < 0)
		return (ARCHIVE_FATAL);
	if (bytes_read > tar->entry_bytes_remaining)
		bytes_read = tar->entry_bytes_remaining;
	/* Don't read more than is available in the
	 * current sparse block. */
	if (tar->sparse_list->remaining < bytes_read)
		bytes_read = tar->sparse_list->remaining;
	*size = bytes_read;
	*offset = tar->sparse_list->offset;
	tar->sparse_list->remaining -= bytes_read;
	tar->sparse_list->offset += bytes_read;
	tar->entry_bytes_remaining -= bytes_read;
	(a->decompressor->consume)(a, bytes_read);
	return (ARCHIVE_OK);
}

static int
archive_read_format_tar_skip(struct archive_read *a)
{
	off_t bytes_skipped;
	struct tar* tar;

	tar = (struct tar *)(a->format->data);

	/*
	 * Compression layer skip functions are required to either skip the
	 * length requested or fail, so we can rely upon the entire entry
	 * plus padding being skipped.
	 */
	bytes_skipped = (a->decompressor->skip)(a, tar->entry_bytes_remaining +
	    tar->entry_padding);
	if (bytes_skipped < 0)
		return (ARCHIVE_FATAL);

	tar->entry_bytes_remaining = 0;
	tar->entry_padding = 0;

	/* Free the sparse list. */
	gnu_clear_sparse_list(tar);

	return (ARCHIVE_OK);
}

/*
 * This function recursively interprets all of the headers associated
 * with a single entry.
 */
static int
tar_read_header(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry)
{
	ssize_t bytes;
	int err;
	const void *h;
	const struct archive_entry_header_ustar *header;

	/* Read 512-byte header record */
	bytes = (a->decompressor->read_ahead)(a, &h, 512);
	if (bytes < 0)
		return (bytes);
	if (bytes == 0) {
		/*
		 * An archive that just ends without a proper
		 * end-of-archive marker.  Yes, there are tar programs
		 * that do this; hold our nose and accept it.
		 */
		return (ARCHIVE_EOF);
	}
	if (bytes < 512) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated tar archive");
		return (ARCHIVE_FATAL);
	}
	(a->decompressor->consume)(a, 512);


	/* Check for end-of-archive mark. */
	if (((*(const char *)h)==0) && archive_block_is_null((const unsigned char *)h)) {
		/* Try to consume a second all-null record, as well. */
		bytes = (a->decompressor->read_ahead)(a, &h, 512);
		if (bytes > 0)
			(a->decompressor->consume)(a, bytes);
		archive_set_error(&a->archive, 0, NULL);
		if (a->archive.archive_format_name == NULL) {
			a->archive.archive_format = ARCHIVE_FORMAT_TAR;
			a->archive.archive_format_name = "tar";
		}
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
		archive_set_error(&a->archive, EINVAL, "Damaged tar archive");
		return (ARCHIVE_RETRY); /* Retryable: Invalid header */
	}

	if (++tar->header_recursion_depth > 32) {
		archive_set_error(&a->archive, EINVAL, "Too many special headers");
		return (ARCHIVE_WARN);
	}

	/* Determine the format variant. */
	header = (const struct archive_entry_header_ustar *)h;
	switch(header->typeflag[0]) {
	case 'A': /* Solaris tar ACL */
		a->archive.archive_format = ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE;
		a->archive.archive_format_name = "Solaris tar";
		err = header_Solaris_ACL(a, tar, entry, h);
		break;
	case 'g': /* POSIX-standard 'g' header. */
		a->archive.archive_format = ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE;
		a->archive.archive_format_name = "POSIX pax interchange format";
		err = header_pax_global(a, tar, entry, h);
		break;
	case 'K': /* Long link name (GNU tar, others) */
		err = header_longlink(a, tar, entry, h);
		break;
	case 'L': /* Long filename (GNU tar, others) */
		err = header_longname(a, tar, entry, h);
		break;
	case 'V': /* GNU volume header */
		err = header_volume(a, tar, entry, h);
		break;
	case 'X': /* Used by SUN tar; same as 'x'. */
		a->archive.archive_format = ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE;
		a->archive.archive_format_name =
		    "POSIX pax interchange format (Sun variant)";
		err = header_pax_extensions(a, tar, entry, h);
		break;
	case 'x': /* POSIX-standard 'x' header. */
		a->archive.archive_format = ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE;
		a->archive.archive_format_name = "POSIX pax interchange format";
		err = header_pax_extensions(a, tar, entry, h);
		break;
	default:
		if (memcmp(header->magic, "ustar  \0", 8) == 0) {
			a->archive.archive_format = ARCHIVE_FORMAT_TAR_GNUTAR;
			a->archive.archive_format_name = "GNU tar format";
			err = header_gnutar(a, tar, entry, h);
		} else if (memcmp(header->magic, "ustar", 5) == 0) {
			if (a->archive.archive_format != ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE) {
				a->archive.archive_format = ARCHIVE_FORMAT_TAR_USTAR;
				a->archive.archive_format_name = "POSIX ustar format";
			}
			err = header_ustar(a, tar, entry, h);
		} else {
			a->archive.archive_format = ARCHIVE_FORMAT_TAR;
			a->archive.archive_format_name = "tar (non-POSIX)";
			err = header_old_tar(a, tar, entry, h);
		}
	}
	--tar->header_recursion_depth;
	/* We return warnings or success as-is.  Anything else is fatal. */
	if (err == ARCHIVE_WARN || err == ARCHIVE_OK)
		return (err);
	if (err == ARCHIVE_EOF)
		/* EOF when recursively reading a header is bad. */
		archive_set_error(&a->archive, EINVAL, "Damaged tar archive");
	return (ARCHIVE_FATAL);
}

/*
 * Return true if block checksum is correct.
 */
static int
checksum(struct archive_read *a, const void *h)
{
	const unsigned char *bytes;
	const struct archive_entry_header_ustar	*header;
	int check, i, sum;

	(void)a; /* UNUSED */
	bytes = (const unsigned char *)h;
	header = (const struct archive_entry_header_ustar *)h;

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

	for (i = 0; i < 512; i++)
		if (*p++)
			return (0);
	return (1);
}

/*
 * Interpret 'A' Solaris ACL header
 */
static int
header_Solaris_ACL(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h)
{
	const struct archive_entry_header_ustar *header;
	size_t size;
	int err;
	char *acl, *p;
	wchar_t *wp;

	/*
	 * read_body_to_string adds a NUL terminator, but we need a little
	 * more to make sure that we don't overrun acl_text later.
	 */
	header = (const struct archive_entry_header_ustar *)h;
	size = tar_atol(header->size, sizeof(header->size));
	err = read_body_to_string(a, tar, &(tar->acl_text), h);
	if (err != ARCHIVE_OK)
		return (err);
	err = tar_read_header(a, tar, entry);
	if ((err != ARCHIVE_OK) && (err != ARCHIVE_WARN))
		return (err);

	/* Skip leading octal number. */
	/* XXX TODO: Parse the octal number and sanity-check it. */
	p = acl = tar->acl_text.s;
	while (*p != '\0' && p < acl + size)
		p++;
	p++;

	if (p >= acl + size) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Malformed Solaris ACL attribute");
		return(ARCHIVE_WARN);
	}

	/* Skip leading octal number. */
	size -= (p - acl);
	acl = p;

	while (*p != '\0' && p < acl + size)
		p++;

	wp = utf8_decode(tar, acl, p - acl);
	err = __archive_entry_acl_parse_w(entry, wp,
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
	return (err);
}

/*
 * Interpret 'K' long linkname header.
 */
static int
header_longlink(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h)
{
	int err;

	err = read_body_to_string(a, tar, &(tar->longlink), h);
	if (err != ARCHIVE_OK)
		return (err);
	err = tar_read_header(a, tar, entry);
	if ((err != ARCHIVE_OK) && (err != ARCHIVE_WARN))
		return (err);
	/* Set symlink if symlink already set, else hardlink. */
	archive_entry_copy_link(entry, tar->longlink.s);
	return (ARCHIVE_OK);
}

/*
 * Interpret 'L' long filename header.
 */
static int
header_longname(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h)
{
	int err;

	err = read_body_to_string(a, tar, &(tar->longname), h);
	if (err != ARCHIVE_OK)
		return (err);
	/* Read and parse "real" header, then override name. */
	err = tar_read_header(a, tar, entry);
	if ((err != ARCHIVE_OK) && (err != ARCHIVE_WARN))
		return (err);
	archive_entry_copy_pathname(entry, tar->longname.s);
	return (ARCHIVE_OK);
}


/*
 * Interpret 'V' GNU tar volume header.
 */
static int
header_volume(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h)
{
	(void)h;

	/* Just skip this and read the next header. */
	return (tar_read_header(a, tar, entry));
}

/*
 * Read body of an archive entry into an archive_string object.
 */
static int
read_body_to_string(struct archive_read *a, struct tar *tar,
    struct archive_string *as, const void *h)
{
	off_t size, padded_size;
	ssize_t bytes_read, bytes_to_copy;
	const struct archive_entry_header_ustar *header;
	const void *src;
	char *dest;

	(void)tar; /* UNUSED */
	header = (const struct archive_entry_header_ustar *)h;
	size  = tar_atol(header->size, sizeof(header->size));
	if ((size > 1048576) || (size < 0)) {
		archive_set_error(&a->archive, EINVAL,
		    "Special header too large");
		return (ARCHIVE_FATAL);
	}

	/* Fail if we can't make our buffer big enough. */
	if (archive_string_ensure(as, size+1) == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "No memory");
		return (ARCHIVE_FATAL);
	}

	/* Read the body into the string. */
	padded_size = (size + 511) & ~ 511;
	dest = as->s;
	while (padded_size > 0) {
		bytes_read = (a->decompressor->read_ahead)(a, &src, padded_size);
		if (bytes_read == 0)
			return (ARCHIVE_EOF);
		if (bytes_read < 0)
			return (ARCHIVE_FATAL);
		if (bytes_read > padded_size)
			bytes_read = padded_size;
		(a->decompressor->consume)(a, bytes_read);
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
header_common(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h)
{
	const struct archive_entry_header_ustar	*header;
	char	tartype;

	(void)a; /* UNUSED */

	header = (const struct archive_entry_header_ustar *)h;
	if (header->linkname[0])
		archive_strncpy(&(tar->entry_linkpath), header->linkname,
		    sizeof(header->linkname));
	else
		archive_string_empty(&(tar->entry_linkpath));

	/* Parse out the numeric fields (all are octal) */
	archive_entry_set_mode(entry, tar_atol(header->mode, sizeof(header->mode)));
	archive_entry_set_uid(entry, tar_atol(header->uid, sizeof(header->uid)));
	archive_entry_set_gid(entry, tar_atol(header->gid, sizeof(header->gid)));
	tar->entry_bytes_remaining = tar_atol(header->size, sizeof(header->size));
	tar->realsize = tar->entry_bytes_remaining;
	archive_entry_set_size(entry, tar->entry_bytes_remaining);
	archive_entry_set_mtime(entry, tar_atol(header->mtime, sizeof(header->mtime)), 0);

	/* Handle the tar type flag appropriately. */
	tartype = header->typeflag[0];

	switch (tartype) {
	case '1': /* Hard link */
		archive_entry_copy_hardlink(entry, tar->entry_linkpath.s);
		/*
		 * The following may seem odd, but: Technically, tar
		 * does not store the file type for a "hard link"
		 * entry, only the fact that it is a hard link.  So, I
		 * leave the type zero normally.  But, pax interchange
		 * format allows hard links to have data, which
		 * implies that the underlying entry is a regular
		 * file.
		 */
		if (archive_entry_size(entry) > 0)
			archive_entry_set_filetype(entry, AE_IFREG);

		/*
		 * A tricky point: Traditionally, tar readers have
		 * ignored the size field when reading hardlink
		 * entries, and some writers put non-zero sizes even
		 * though the body is empty.  POSIX blessed this
		 * convention in the 1988 standard, but broke with
		 * this tradition in 2001 by permitting hardlink
		 * entries to store valid bodies in pax interchange
		 * format, but not in ustar format.  Since there is no
		 * hard and fast way to distinguish pax interchange
		 * from earlier archives (the 'x' and 'g' entries are
		 * optional, after all), we need a heuristic.
		 */
		if (archive_entry_size(entry) == 0) {
			/* If the size is already zero, we're done. */
		}  else if (a->archive.archive_format
		    == ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE) {
			/* Definitely pax extended; must obey hardlink size. */
		} else if (a->archive.archive_format == ARCHIVE_FORMAT_TAR
		    || a->archive.archive_format == ARCHIVE_FORMAT_TAR_GNUTAR)
		{
			/* Old-style or GNU tar: we must ignore the size. */
			archive_entry_set_size(entry, 0);
			tar->entry_bytes_remaining = 0;
		} else if (archive_read_format_tar_bid(a) > 50) {
			/*
			 * We don't know if it's pax: If the bid
			 * function sees a valid ustar header
			 * immediately following, then let's ignore
			 * the hardlink size.
			 */
			archive_entry_set_size(entry, 0);
			tar->entry_bytes_remaining = 0;
		}
		/*
		 * TODO: There are still two cases I'd like to handle:
		 *   = a ustar non-pax archive with a hardlink entry at
		 *     end-of-archive.  (Look for block of nulls following?)
		 *   = a pax archive that has not seen any pax headers
		 *     and has an entry which is a hardlink entry storing
		 *     a body containing an uncompressed tar archive.
		 * The first is worth addressing; I don't see any reliable
		 * way to deal with the second possibility.
		 */
		break;
	case '2': /* Symlink */
		archive_entry_set_filetype(entry, AE_IFLNK);
		archive_entry_set_size(entry, 0);
		tar->entry_bytes_remaining = 0;
		archive_entry_copy_symlink(entry, tar->entry_linkpath.s);
		break;
	case '3': /* Character device */
		archive_entry_set_filetype(entry, AE_IFCHR);
		archive_entry_set_size(entry, 0);
		tar->entry_bytes_remaining = 0;
		break;
	case '4': /* Block device */
		archive_entry_set_filetype(entry, AE_IFBLK);
		archive_entry_set_size(entry, 0);
		tar->entry_bytes_remaining = 0;
		break;
	case '5': /* Dir */
		archive_entry_set_filetype(entry, AE_IFDIR);
		archive_entry_set_size(entry, 0);
		tar->entry_bytes_remaining = 0;
		break;
	case '6': /* FIFO device */
		archive_entry_set_filetype(entry, AE_IFIFO);
		archive_entry_set_size(entry, 0);
		tar->entry_bytes_remaining = 0;
		break;
	case 'D': /* GNU incremental directory type */
		/*
		 * No special handling is actually required here.
		 * It might be nice someday to preprocess the file list and
		 * provide it to the client, though.
		 */
		archive_entry_set_filetype(entry, AE_IFDIR);
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
		archive_entry_set_filetype(entry, AE_IFREG);
		break;
	case 'S': /* GNU sparse files */
		/*
		 * Sparse files are really just regular files with
		 * sparse information in the extended area.
		 */
		/* FALLTHROUGH */
	default: /* Regular file  and non-standard types */
		/*
		 * Per POSIX: non-recognized types should always be
		 * treated as regular files.
		 */
		archive_entry_set_filetype(entry, AE_IFREG);
		break;
	}
	return (0);
}

/*
 * Parse out header elements for "old-style" tar archives.
 */
static int
header_old_tar(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h)
{
	const struct archive_entry_header_ustar	*header;

	/* Copy filename over (to ensure null termination). */
	header = (const struct archive_entry_header_ustar *)h;
	archive_strncpy(&(tar->entry_pathname), header->name, sizeof(header->name));
	archive_entry_copy_pathname(entry, tar->entry_pathname.s);

	/* Grab rest of common fields */
	header_common(a, tar, entry, h);

	tar->entry_padding = 0x1ff & (-tar->entry_bytes_remaining);
	return (0);
}

/*
 * Parse a file header for a pax extended archive entry.
 */
static int
header_pax_global(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h)
{
	int err;

	err = read_body_to_string(a, tar, &(tar->pax_global), h);
	if (err != ARCHIVE_OK)
		return (err);
	err = tar_read_header(a, tar, entry);
	return (err);
}

static int
header_pax_extensions(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h)
{
	int err, err2;

	err = read_body_to_string(a, tar, &(tar->pax_header), h);
	if (err != ARCHIVE_OK)
		return (err);

	/* Parse the next header. */
	err = tar_read_header(a, tar, entry);
	if ((err != ARCHIVE_OK) && (err != ARCHIVE_WARN))
		return (err);

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
	err2 = pax_header(a, tar, entry, tar->pax_header.s);
	err =  err_combine(err, err2);
	tar->entry_padding = 0x1ff & (-tar->entry_bytes_remaining);
	return (err);
}


/*
 * Parse a file header for a Posix "ustar" archive entry.  This also
 * handles "pax" or "extended ustar" entries.
 */
static int
header_ustar(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h)
{
	const struct archive_entry_header_ustar	*header;
	struct archive_string *as;

	header = (const struct archive_entry_header_ustar *)h;

	/* Copy name into an internal buffer to ensure null-termination. */
	as = &(tar->entry_pathname);
	if (header->prefix[0]) {
		archive_strncpy(as, header->prefix, sizeof(header->prefix));
		if (as->s[archive_strlen(as) - 1] != '/')
			archive_strappend_char(as, '/');
		archive_strncat(as, header->name, sizeof(header->name));
	} else
		archive_strncpy(as, header->name, sizeof(header->name));

	archive_entry_copy_pathname(entry, as->s);

	/* Handle rest of common fields. */
	header_common(a, tar, entry, h);

	/* Handle POSIX ustar fields. */
	archive_strncpy(&(tar->entry_uname), header->uname,
	    sizeof(header->uname));
	archive_entry_copy_uname(entry, tar->entry_uname.s);

	archive_strncpy(&(tar->entry_gname), header->gname,
	    sizeof(header->gname));
	archive_entry_copy_gname(entry, tar->entry_gname.s);

	/* Parse out device numbers only for char and block specials. */
	if (header->typeflag[0] == '3' || header->typeflag[0] == '4') {
		archive_entry_set_rdevmajor(entry,
		    tar_atol(header->rdevmajor, sizeof(header->rdevmajor)));
		archive_entry_set_rdevminor(entry,
		    tar_atol(header->rdevminor, sizeof(header->rdevminor)));
	}

	tar->entry_padding = 0x1ff & (-tar->entry_bytes_remaining);

	return (0);
}


/*
 * Parse the pax extended attributes record.
 *
 * Returns non-zero if there's an error in the data.
 */
static int
pax_header(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, char *attr)
{
	size_t attr_length, l, line_length;
	char *line, *p;
	char *key, *value;
	int err, err2;

	attr_length = strlen(attr);
	tar->pax_hdrcharset_binary = 0;
	archive_string_empty(&(tar->entry_gname));
	archive_string_empty(&(tar->entry_linkpath));
	archive_string_empty(&(tar->entry_pathname));
	archive_string_empty(&(tar->entry_pathname_override));
	archive_string_empty(&(tar->entry_uname));
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
			if (*p < '0' || *p > '9') {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
				    "Ignoring malformed pax extended attributes");
				return (ARCHIVE_WARN);
			}
			line_length *= 10;
			line_length += *p - '0';
			if (line_length > 999999) {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
				    "Rejecting pax extended attribute > 1MB");
				return (ARCHIVE_WARN);
			}
			p++;
			l--;
		}

		/*
		 * Parsed length must be no bigger than available data,
		 * at least 1, and the last character of the line must
		 * be '\n'.
		 */
		if (line_length > attr_length
		    || line_length < 1
		    || attr[line_length - 1] != '\n')
		{
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Ignoring malformed pax extended attribute");
			return (ARCHIVE_WARN);
		}

		/* Null-terminate the line. */
		attr[line_length - 1] = '\0';

		/* Find end of key and null terminate it. */
		key = p;
		if (key[0] == '=')
			return (-1);
		while (*p && *p != '=')
			++p;
		if (*p == '\0') {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Invalid pax extended attributes");
			return (ARCHIVE_WARN);
		}
		*p = '\0';

		/* Identify null-terminated 'value' portion. */
		value = p + 1;

		/* Identify this attribute and set it in the entry. */
		err2 = pax_attribute(tar, entry, key, value);
		err = err_combine(err, err2);

		/* Skip to next line */
		attr += line_length;
		attr_length -= line_length;
	}
	if (archive_strlen(&(tar->entry_gname)) > 0) {
		value = tar->entry_gname.s;
		if (tar->pax_hdrcharset_binary)
			archive_entry_copy_gname(entry, value);
		else {
			if (!archive_entry_update_gname_utf8(entry, value)) {
				err = ARCHIVE_WARN;
				archive_set_error(&a->archive,
				    ARCHIVE_ERRNO_FILE_FORMAT,
				    "Gname in pax header can't "
				    "be converted to current locale.");
			}
		}
	}
	if (archive_strlen(&(tar->entry_linkpath)) > 0) {
		value = tar->entry_linkpath.s;
		if (tar->pax_hdrcharset_binary)
			archive_entry_copy_link(entry, value);
		else {
			if (!archive_entry_update_link_utf8(entry, value)) {
				err = ARCHIVE_WARN;
				archive_set_error(&a->archive,
				    ARCHIVE_ERRNO_FILE_FORMAT,
				    "Linkname in pax header can't "
				    "be converted to current locale.");
			}
		}
	}
	/*
	 * Some extensions (such as the GNU sparse file extensions)
	 * deliberately store a synthetic name under the regular 'path'
	 * attribute and the real file name under a different attribute.
	 * Since we're supposed to not care about the order, we
	 * have no choice but to store all of the various filenames
	 * we find and figure it all out afterwards.  This is the
	 * figuring out part.
	 */
	value = NULL;
	if (archive_strlen(&(tar->entry_pathname_override)) > 0)
		value = tar->entry_pathname_override.s;
	else if (archive_strlen(&(tar->entry_pathname)) > 0)
		value = tar->entry_pathname.s;
	if (value != NULL) {
		if (tar->pax_hdrcharset_binary)
			archive_entry_copy_pathname(entry, value);
		else {
			if (!archive_entry_update_pathname_utf8(entry, value)) {
				err = ARCHIVE_WARN;
				archive_set_error(&a->archive,
				    ARCHIVE_ERRNO_FILE_FORMAT,
				    "Pathname in pax header can't be "
				    "converted to current locale.");
			}
		}
	}
	if (archive_strlen(&(tar->entry_uname)) > 0) {
		value = tar->entry_uname.s;
		if (tar->pax_hdrcharset_binary)
			archive_entry_copy_uname(entry, value);
		else {
			if (!archive_entry_update_uname_utf8(entry, value)) {
				err = ARCHIVE_WARN;
				archive_set_error(&a->archive,
				    ARCHIVE_ERRNO_FILE_FORMAT,
				    "Uname in pax header can't "
				    "be converted to current locale.");
			}
		}
	}
	return (err);
}

static int
pax_attribute_xattr(struct archive_entry *entry,
	char *name, char *value)
{
	char *name_decoded;
	void *value_decoded;
	size_t value_len;

	if (strlen(name) < 18 || (strncmp(name, "LIBARCHIVE.xattr.", 17)) != 0)
		return 3;

	name += 17;

	/* URL-decode name */
	name_decoded = url_decode(name);
	if (name_decoded == NULL)
		return 2;

	/* Base-64 decode value */
	value_decoded = base64_decode(value, strlen(value), &value_len);
	if (value_decoded == NULL) {
		free(name_decoded);
		return 1;
	}

	archive_entry_xattr_add_entry(entry, name_decoded,
		value_decoded, value_len);

	free(name_decoded);
	free(value_decoded);
	return 0;
}

/*
 * Parse a single key=value attribute.  key/value pointers are
 * assumed to point into reasonably long-lived storage.
 *
 * Note that POSIX reserves all-lowercase keywords.  Vendor-specific
 * extensions should always have keywords of the form "VENDOR.attribute"
 * In particular, it's quite feasible to support many different
 * vendor extensions here.  I'm using "LIBARCHIVE" for extensions
 * unique to this library.
 *
 * Investigate other vendor-specific extensions and see if
 * any of them look useful.
 */
static int
pax_attribute(struct tar *tar, struct archive_entry *entry,
    char *key, char *value)
{
	int64_t s;
	long n;
	wchar_t *wp;

	switch (key[0]) {
	case 'G':
		/* GNU "0.0" sparse pax format. */
		if (strcmp(key, "GNU.sparse.numblocks") == 0) {
			tar->sparse_offset = -1;
			tar->sparse_numbytes = -1;
			tar->sparse_gnu_major = 0;
			tar->sparse_gnu_minor = 0;
		}
		if (strcmp(key, "GNU.sparse.offset") == 0) {
			tar->sparse_offset = tar_atol10(value, strlen(value));
			if (tar->sparse_numbytes != -1) {
				gnu_add_sparse_entry(tar,
				    tar->sparse_offset, tar->sparse_numbytes);
				tar->sparse_offset = -1;
				tar->sparse_numbytes = -1;
			}
		}
		if (strcmp(key, "GNU.sparse.numbytes") == 0) {
			tar->sparse_numbytes = tar_atol10(value, strlen(value));
			if (tar->sparse_numbytes != -1) {
				gnu_add_sparse_entry(tar,
				    tar->sparse_offset, tar->sparse_numbytes);
				tar->sparse_offset = -1;
				tar->sparse_numbytes = -1;
			}
		}
		if (strcmp(key, "GNU.sparse.size") == 0) {
			tar->realsize = tar_atol10(value, strlen(value));
			archive_entry_set_size(entry, tar->realsize);
		}

		/* GNU "0.1" sparse pax format. */
		if (strcmp(key, "GNU.sparse.map") == 0) {
			tar->sparse_gnu_major = 0;
			tar->sparse_gnu_minor = 1;
			if (gnu_sparse_01_parse(tar, value) != ARCHIVE_OK)
				return (ARCHIVE_WARN);
		}

		/* GNU "1.0" sparse pax format */
		if (strcmp(key, "GNU.sparse.major") == 0) {
			tar->sparse_gnu_major = tar_atol10(value, strlen(value));
			tar->sparse_gnu_pending = 1;
		}
		if (strcmp(key, "GNU.sparse.minor") == 0) {
			tar->sparse_gnu_minor = tar_atol10(value, strlen(value));
			tar->sparse_gnu_pending = 1;
		}
		if (strcmp(key, "GNU.sparse.name") == 0) {
			/*
			 * The real filename; when storing sparse
			 * files, GNU tar puts a synthesized name into
			 * the regular 'path' attribute in an attempt
			 * to limit confusion. ;-)
			 */
			archive_strcpy(&(tar->entry_pathname_override), value);
		}
		if (strcmp(key, "GNU.sparse.realsize") == 0) {
			tar->realsize = tar_atol10(value, strlen(value));
			archive_entry_set_size(entry, tar->realsize);
		}
		break;
	case 'L':
		/* Our extensions */
/* TODO: Handle arbitrary extended attributes... */
/*
		if (strcmp(key, "LIBARCHIVE.xxxxxxx")==0)
			archive_entry_set_xxxxxx(entry, value);
*/
		if (strcmp(key, "LIBARCHIVE.creationtime")==0) {
			pax_time(value, &s, &n);
			archive_entry_set_birthtime(entry, s, n);
		}
		if (strncmp(key, "LIBARCHIVE.xattr.", 17)==0)
			pax_attribute_xattr(entry, key, value);
		break;
	case 'S':
		/* We support some keys used by the "star" archiver */
		if (strcmp(key, "SCHILY.acl.access")==0) {
			wp = utf8_decode(tar, value, strlen(value));
			/* TODO: if (wp == NULL) */
			__archive_entry_acl_parse_w(entry, wp,
			    ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
		} else if (strcmp(key, "SCHILY.acl.default")==0) {
			wp = utf8_decode(tar, value, strlen(value));
			/* TODO: if (wp == NULL) */
			__archive_entry_acl_parse_w(entry, wp,
			    ARCHIVE_ENTRY_ACL_TYPE_DEFAULT);
		} else if (strcmp(key, "SCHILY.devmajor")==0) {
			archive_entry_set_rdevmajor(entry,
			    tar_atol10(value, strlen(value)));
		} else if (strcmp(key, "SCHILY.devminor")==0) {
			archive_entry_set_rdevminor(entry,
			    tar_atol10(value, strlen(value)));
		} else if (strcmp(key, "SCHILY.fflags")==0) {
			archive_entry_copy_fflags_text(entry, value);
		} else if (strcmp(key, "SCHILY.dev")==0) {
			archive_entry_set_dev(entry,
			    tar_atol10(value, strlen(value)));
		} else if (strcmp(key, "SCHILY.ino")==0) {
			archive_entry_set_ino(entry,
			    tar_atol10(value, strlen(value)));
		} else if (strcmp(key, "SCHILY.nlink")==0) {
			archive_entry_set_nlink(entry,
			    tar_atol10(value, strlen(value)));
		} else if (strcmp(key, "SCHILY.realsize")==0) {
			tar->realsize = tar_atol10(value, strlen(value));
			archive_entry_set_size(entry, tar->realsize);
		}
		break;
	case 'a':
		if (strcmp(key, "atime")==0) {
			pax_time(value, &s, &n);
			archive_entry_set_atime(entry, s, n);
		}
		break;
	case 'c':
		if (strcmp(key, "ctime")==0) {
			pax_time(value, &s, &n);
			archive_entry_set_ctime(entry, s, n);
		} else if (strcmp(key, "charset")==0) {
			/* TODO: Publish charset information in entry. */
		} else if (strcmp(key, "comment")==0) {
			/* TODO: Publish comment in entry. */
		}
		break;
	case 'g':
		if (strcmp(key, "gid")==0) {
			archive_entry_set_gid(entry,
			    tar_atol10(value, strlen(value)));
		} else if (strcmp(key, "gname")==0) {
			archive_strcpy(&(tar->entry_gname), value);
		}
		break;
	case 'h':
		if (strcmp(key, "hdrcharset") == 0) {
			if (strcmp(value, "BINARY") == 0)
				tar->pax_hdrcharset_binary = 1;
			else if (strcmp(value, "ISO-IR 10646 2000 UTF-8") == 0)
				tar->pax_hdrcharset_binary = 0;
			else {
				/* TODO: Warn about unsupported hdrcharset */
			}
		}
		break;
	case 'l':
		/* pax interchange doesn't distinguish hardlink vs. symlink. */
		if (strcmp(key, "linkpath")==0) {
			archive_strcpy(&(tar->entry_linkpath), value);
		}
		break;
	case 'm':
		if (strcmp(key, "mtime")==0) {
			pax_time(value, &s, &n);
			archive_entry_set_mtime(entry, s, n);
		}
		break;
	case 'p':
		if (strcmp(key, "path")==0) {
			archive_strcpy(&(tar->entry_pathname), value);
		}
		break;
	case 'r':
		/* POSIX has reserved 'realtime.*' */
		break;
	case 's':
		/* POSIX has reserved 'security.*' */
		/* Someday: if (strcmp(key, "security.acl")==0) { ... } */
		if (strcmp(key, "size")==0) {
			/* "size" is the size of the data in the entry. */
			tar->entry_bytes_remaining
			    = tar_atol10(value, strlen(value));
			/*
			 * But, "size" is not necessarily the size of
			 * the file on disk; if this is a sparse file,
			 * the disk size may have already been set from
			 * GNU.sparse.realsize or GNU.sparse.size or
			 * an old GNU header field or SCHILY.realsize
			 * or ....
			 */
			if (tar->realsize < 0) {
				archive_entry_set_size(entry,
				    tar->entry_bytes_remaining);
				tar->realsize
				    = tar->entry_bytes_remaining;
			}
		}
		break;
	case 'u':
		if (strcmp(key, "uid")==0) {
			archive_entry_set_uid(entry,
			    tar_atol10(value, strlen(value)));
		} else if (strcmp(key, "uname")==0) {
			archive_strcpy(&(tar->entry_uname), value);
		}
		break;
	}
	return (0);
}



/*
 * parse a decimal time value, which may include a fractional portion
 */
static void
pax_time(const char *p, int64_t *ps, long *pn)
{
	char digit;
	int64_t	s;
	unsigned long l;
	int sign;
	int64_t limit, last_digit_limit;

	limit = INT64_MAX / 10;
	last_digit_limit = INT64_MAX % 10;

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
			s = UINT64_MAX;
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
header_gnutar(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h)
{
	const struct archive_entry_header_gnutar *header;

	(void)a;

	/*
	 * GNU header is like POSIX ustar, except 'prefix' is
	 * replaced with some other fields. This also means the
	 * filename is stored as in old-style archives.
	 */

	/* Grab fields common to all tar variants. */
	header_common(a, tar, entry, h);

	/* Copy filename over (to ensure null termination). */
	header = (const struct archive_entry_header_gnutar *)h;
	archive_strncpy(&(tar->entry_pathname), header->name,
	    sizeof(header->name));
	archive_entry_copy_pathname(entry, tar->entry_pathname.s);

	/* Fields common to ustar and GNU */
	/* XXX Can the following be factored out since it's common
	 * to ustar and gnu tar?  Is it okay to move it down into
	 * header_common, perhaps?  */
	archive_strncpy(&(tar->entry_uname),
	    header->uname, sizeof(header->uname));
	archive_entry_copy_uname(entry, tar->entry_uname.s);

	archive_strncpy(&(tar->entry_gname),
	    header->gname, sizeof(header->gname));
	archive_entry_copy_gname(entry, tar->entry_gname.s);

	/* Parse out device numbers only for char and block specials */
	if (header->typeflag[0] == '3' || header->typeflag[0] == '4') {
		archive_entry_set_rdevmajor(entry,
		    tar_atol(header->rdevmajor, sizeof(header->rdevmajor)));
		archive_entry_set_rdevminor(entry,
		    tar_atol(header->rdevminor, sizeof(header->rdevminor)));
	} else
		archive_entry_set_rdev(entry, 0);

	tar->entry_padding = 0x1ff & (-tar->entry_bytes_remaining);

	/* Grab GNU-specific fields. */
	archive_entry_set_atime(entry,
	    tar_atol(header->atime, sizeof(header->atime)), 0);
	archive_entry_set_ctime(entry,
	    tar_atol(header->ctime, sizeof(header->ctime)), 0);
	if (header->realsize[0] != 0) {
		tar->realsize
		    = tar_atol(header->realsize, sizeof(header->realsize));
		archive_entry_set_size(entry, tar->realsize);
	}

	if (header->sparse[0].offset[0] != 0) {
		gnu_sparse_old_read(a, tar, header);
	} else {
		if (header->isextended[0] != 0) {
			/* XXX WTF? XXX */
		}
	}

	return (0);
}

static void
gnu_add_sparse_entry(struct tar *tar, off_t offset, off_t remaining)
{
	struct sparse_block *p;

	p = (struct sparse_block *)malloc(sizeof(*p));
	if (p == NULL)
		__archive_errx(1, "Out of memory");
	memset(p, 0, sizeof(*p));
	if (tar->sparse_last != NULL)
		tar->sparse_last->next = p;
	else
		tar->sparse_list = p;
	tar->sparse_last = p;
	p->offset = offset;
	p->remaining = remaining;
}

static void
gnu_clear_sparse_list(struct tar *tar)
{
	struct sparse_block *p;

	while (tar->sparse_list != NULL) {
		p = tar->sparse_list;
		tar->sparse_list = p->next;
		free(p);
	}
	tar->sparse_last = NULL;
}

/*
 * GNU tar old-format sparse data.
 *
 * GNU old-format sparse data is stored in a fixed-field
 * format.  Offset/size values are 11-byte octal fields (same
 * format as 'size' field in ustart header).  These are
 * stored in the header, allocating subsequent header blocks
 * as needed.  Extending the header in this way is a pretty
 * severe POSIX violation; this design has earned GNU tar a
 * lot of criticism.
 */

static int
gnu_sparse_old_read(struct archive_read *a, struct tar *tar,
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

	gnu_sparse_old_parse(tar, header->sparse, 4);
	if (header->isextended[0] == 0)
		return (ARCHIVE_OK);

	do {
		bytes_read = (a->decompressor->read_ahead)(a, &data, 512);
		if (bytes_read < 0)
			return (ARCHIVE_FATAL);
		if (bytes_read < 512) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
			    "Truncated tar archive "
			    "detected while reading sparse file data");
			return (ARCHIVE_FATAL);
		}
		(a->decompressor->consume)(a, 512);
		ext = (const struct extended *)data;
		gnu_sparse_old_parse(tar, ext->sparse, 21);
	} while (ext->isextended[0] != 0);
	if (tar->sparse_list != NULL)
		tar->entry_offset = tar->sparse_list->offset;
	return (ARCHIVE_OK);
}

static void
gnu_sparse_old_parse(struct tar *tar,
    const struct gnu_sparse *sparse, int length)
{
	while (length > 0 && sparse->offset[0] != 0) {
		gnu_add_sparse_entry(tar,
		    tar_atol(sparse->offset, sizeof(sparse->offset)),
		    tar_atol(sparse->numbytes, sizeof(sparse->numbytes)));
		sparse++;
		length--;
	}
}

/*
 * GNU tar sparse format 0.0
 *
 * Beginning with GNU tar 1.15, sparse files are stored using
 * information in the pax extended header.  The GNU tar maintainers
 * have gone through a number of variations in the process of working
 * out this scheme; furtunately, they're all numbered.
 *
 * Sparse format 0.0 uses attribute GNU.sparse.numblocks to store the
 * number of blocks, and GNU.sparse.offset/GNU.sparse.numbytes to
 * store offset/size for each block.  The repeated instances of these
 * latter fields violate the pax specification (which frowns on
 * duplicate keys), so this format was quickly replaced.
 */

/*
 * GNU tar sparse format 0.1
 *
 * This version replaced the offset/numbytes attributes with
 * a single "map" attribute that stored a list of integers.  This
 * format had two problems: First, the "map" attribute could be very
 * long, which caused problems for some implementations.  More
 * importantly, the sparse data was lost when extracted by archivers
 * that didn't recognize this extension.
 */

static int
gnu_sparse_01_parse(struct tar *tar, const char *p)
{
	const char *e;
	off_t offset = -1, size = -1;

	for (;;) {
		e = p;
		while (*e != '\0' && *e != ',') {
			if (*e < '0' || *e > '9')
				return (ARCHIVE_WARN);
			e++;
		}
		if (offset < 0) {
			offset = tar_atol10(p, e - p);
			if (offset < 0)
				return (ARCHIVE_WARN);
		} else {
			size = tar_atol10(p, e - p);
			if (size < 0)
				return (ARCHIVE_WARN);
			gnu_add_sparse_entry(tar, offset, size);
			offset = -1;
		}
		if (*e == '\0')
			return (ARCHIVE_OK);
		p = e + 1;
	}
}

/*
 * GNU tar sparse format 1.0
 *
 * The idea: The offset/size data is stored as a series of base-10
 * ASCII numbers prepended to the file data, so that dearchivers that
 * don't support this format will extract the block map along with the
 * data and a separate post-process can restore the sparseness.
 *
 * Unfortunately, GNU tar 1.16 had a bug that added unnecessary
 * padding to the body of the file when using this format.  GNU tar
 * 1.17 corrected this bug without bumping the version number, so
 * it's not possible to support both variants.  This code supports
 * the later variant at the expense of not supporting the former.
 *
 * This variant also replaced GNU.sparse.size with GNU.sparse.realsize
 * and introduced the GNU.sparse.major/GNU.sparse.minor attributes.
 */

/*
 * Read the next line from the input, and parse it as a decimal
 * integer followed by '\n'.  Returns positive integer value or
 * negative on error.
 */
static int64_t
gnu_sparse_10_atol(struct archive_read *a, struct tar *tar,
    ssize_t *remaining)
{
	int64_t l, limit, last_digit_limit;
	const char *p;
	ssize_t bytes_read;
	int base, digit;

	base = 10;
	limit = INT64_MAX / base;
	last_digit_limit = INT64_MAX % base;

	/*
	 * Skip any lines starting with '#'; GNU tar specs
	 * don't require this, but they should.
	 */
	do {
		bytes_read = readline(a, tar, &p, tar_min(*remaining, 100));
		if (bytes_read <= 0)
			return (ARCHIVE_FATAL);
		*remaining -= bytes_read;
	} while (p[0] == '#');

	l = 0;
	while (bytes_read > 0) {
		if (*p == '\n')
			return (l);
		if (*p < '0' || *p >= '0' + base)
			return (ARCHIVE_WARN);
		digit = *p - '0';
		if (l > limit || (l == limit && digit > last_digit_limit))
			l = UINT64_MAX; /* Truncate on overflow. */
		else
			l = (l * base) + digit;
		p++;
		bytes_read--;
	}
	/* TODO: Error message. */
	return (ARCHIVE_WARN);
}

/*
 * Returns length (in bytes) of the sparse data description
 * that was read.
 */
static ssize_t
gnu_sparse_10_read(struct archive_read *a, struct tar *tar)
{
	ssize_t remaining, bytes_read;
	int entries;
	off_t offset, size, to_skip;

	/* Clear out the existing sparse list. */
	gnu_clear_sparse_list(tar);

	remaining = tar->entry_bytes_remaining;

	/* Parse entries. */
	entries = gnu_sparse_10_atol(a, tar, &remaining);
	if (entries < 0)
		return (ARCHIVE_FATAL);
	/* Parse the individual entries. */
	while (entries-- > 0) {
		/* Parse offset/size */
		offset = gnu_sparse_10_atol(a, tar, &remaining);
		if (offset < 0)
			return (ARCHIVE_FATAL);
		size = gnu_sparse_10_atol(a, tar, &remaining);
		if (size < 0)
			return (ARCHIVE_FATAL);
		/* Add a new sparse entry. */
		gnu_add_sparse_entry(tar, offset, size);
	}
	/* Skip rest of block... */
	bytes_read = tar->entry_bytes_remaining - remaining;
	to_skip = 0x1ff & -bytes_read;
	if (to_skip != (a->decompressor->skip)(a, to_skip))
		return (ARCHIVE_FATAL);
	return (bytes_read + to_skip);
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
	limit = INT64_MAX / base;
	last_digit_limit = INT64_MAX % base;

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
			l = UINT64_MAX; /* Truncate on overflow. */
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
tar_atol10(const char *p, unsigned char_cnt)
{
	int64_t l, limit, last_digit_limit;
	int base, digit, sign;

	base = 10;
	limit = INT64_MAX / base;
	last_digit_limit = INT64_MAX % base;

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
			l = UINT64_MAX; /* Truncate on overflow. */
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
 * ignored.
 */
static int64_t
tar_atol256(const char *_p, unsigned char_cnt)
{
	int64_t	l, upper_limit, lower_limit;
	const unsigned char *p = (const unsigned char *)_p;

	upper_limit = INT64_MAX / 256;
	lower_limit = INT64_MIN / 256;

	/* Pad with 1 or 0 bits, depending on sign. */
	if ((0x40 & *p) == 0x40)
		l = (int64_t)-1;
	else
		l = 0;
	l = (l << 6) | (0x3f & *p++);
	while (--char_cnt > 0) {
		if (l > upper_limit) {
			l = INT64_MAX; /* Truncate on overflow */
			break;
		} else if (l < lower_limit) {
			l = INT64_MIN;
			break;
		}
		l = (l << 8) | (0xff & (int64_t)*p++);
	}
	return (l);
}

/*
 * Returns length of line (including trailing newline)
 * or negative on error.  'start' argument is updated to
 * point to first character of line.  This avoids copying
 * when possible.
 */
static ssize_t
readline(struct archive_read *a, struct tar *tar, const char **start,
    ssize_t limit)
{
	ssize_t bytes_read;
	ssize_t total_size = 0;
	const void *t;
	const char *s;
	void *p;

	bytes_read = (a->decompressor->read_ahead)(a, &t, 1);
	if (bytes_read <= 0)
		return (ARCHIVE_FATAL);
	s = t;  /* Start of line? */
	p = memchr(t, '\n', bytes_read);
	/* If we found '\n' in the read buffer, return pointer to that. */
	if (p != NULL) {
		bytes_read = 1 + ((const char *)p) - s;
		if (bytes_read > limit) {
			archive_set_error(&a->archive,
			    ARCHIVE_ERRNO_FILE_FORMAT,
			    "Line too long");
			return (ARCHIVE_FATAL);
		}
		(a->decompressor->consume)(a, bytes_read);
		*start = s;
		return (bytes_read);
	}
	/* Otherwise, we need to accumulate in a line buffer. */
	for (;;) {
		if (total_size + bytes_read > limit) {
			archive_set_error(&a->archive,
			    ARCHIVE_ERRNO_FILE_FORMAT,
			    "Line too long");
			return (ARCHIVE_FATAL);
		}
		if (archive_string_ensure(&tar->line, total_size + bytes_read) == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't allocate working buffer");
			return (ARCHIVE_FATAL);
		}
		memcpy(tar->line.s + total_size, t, bytes_read);
		(a->decompressor->consume)(a, bytes_read);
		total_size += bytes_read;
		/* If we found '\n', clean up and return. */
		if (p != NULL) {
			*start = tar->line.s;
			return (total_size);
		}
		/* Read some more. */
		bytes_read = (a->decompressor->read_ahead)(a, &t, 1);
		if (bytes_read <= 0)
			return (ARCHIVE_FATAL);
		s = t;  /* Start of line? */
		p = memchr(t, '\n', bytes_read);
		/* If we found '\n', trim the read. */
		if (p != NULL) {
			bytes_read = 1 + ((const char *)p) - s;
		}
	}
}

static wchar_t *
utf8_decode(struct tar *tar, const char *src, size_t length)
{
	wchar_t *dest;
	ssize_t n;

	/* Ensure pax_entry buffer is big enough. */
	if (tar->pax_entry_length <= length) {
		wchar_t *old_entry = tar->pax_entry;

		if (tar->pax_entry_length <= 0)
			tar->pax_entry_length = 1024;
		while (tar->pax_entry_length <= length + 1)
			tar->pax_entry_length *= 2;

		old_entry = tar->pax_entry;
		tar->pax_entry = (wchar_t *)realloc(tar->pax_entry,
		    tar->pax_entry_length * sizeof(wchar_t));
		if (tar->pax_entry == NULL) {
			free(old_entry);
			/* TODO: Handle this error. */
			return (NULL);
		}
	}

	dest = tar->pax_entry;
	while (length > 0) {
		n = UTF8_mbrtowc(dest, src, length);
		if (n < 0)
			return (NULL);
		if (n == 0)
			break;
		dest++;
		src += n;
		length -= n;
	}
	*dest++ = L'\0';
	return (tar->pax_entry);
}

/*
 * Copied and simplified from FreeBSD libc/locale.
 */
static ssize_t
UTF8_mbrtowc(wchar_t *pwc, const char *s, size_t n)
{
        int ch, i, len, mask;
        unsigned long wch;

        if (s == NULL || n == 0 || pwc == NULL)
                return (0);

        /*
         * Determine the number of octets that make up this character from
         * the first octet, and a mask that extracts the interesting bits of
         * the first octet.
         */
        ch = (unsigned char)*s;
        if ((ch & 0x80) == 0) {
                mask = 0x7f;
                len = 1;
        } else if ((ch & 0xe0) == 0xc0) {
                mask = 0x1f;
                len = 2;
        } else if ((ch & 0xf0) == 0xe0) {
                mask = 0x0f;
                len = 3;
        } else if ((ch & 0xf8) == 0xf0) {
                mask = 0x07;
                len = 4;
        } else {
		/* Invalid first byte. */
		return (-1);
        }

        if (n < (size_t)len) {
		/* Valid first byte but truncated. */
                return (-2);
	}

        /*
         * Decode the octet sequence representing the character in chunks
         * of 6 bits, most significant first.
         */
        wch = (unsigned char)*s++ & mask;
        i = len;
        while (--i != 0) {
                if ((*s & 0xc0) != 0x80) {
			/* Invalid intermediate byte; consume one byte and
			 * emit '?' */
			*pwc = '?';
			return (1);
                }
                wch <<= 6;
                wch |= *s++ & 0x3f;
        }

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
	/* Return number of bytes input consumed: 0 for end-of-string. */
        return (wch == L'\0' ? 0 : len);
}


/*
 * base64_decode - Base64 decode
 *
 * This accepts most variations of base-64 encoding, including:
 *    * with or without line breaks
 *    * with or without the final group padded with '=' or '_' characters
 * (The most economical Base-64 variant does not pad the last group and
 * omits line breaks; RFC1341 used for MIME requires both.)
 */
static char *
base64_decode(const char *s, size_t len, size_t *out_len)
{
	static const unsigned char digits[64] = {
		'A','B','C','D','E','F','G','H','I','J','K','L','M','N',
		'O','P','Q','R','S','T','U','V','W','X','Y','Z','a','b',
		'c','d','e','f','g','h','i','j','k','l','m','n','o','p',
		'q','r','s','t','u','v','w','x','y','z','0','1','2','3',
		'4','5','6','7','8','9','+','/' };
	static unsigned char decode_table[128];
	char *out, *d;
	const unsigned char *src = (const unsigned char *)s;

	/* If the decode table is not yet initialized, prepare it. */
	if (decode_table[digits[1]] != 1) {
		size_t i;
		memset(decode_table, 0xff, sizeof(decode_table));
		for (i = 0; i < sizeof(digits); i++)
			decode_table[digits[i]] = i;
	}

	/* Allocate enough space to hold the entire output. */
	/* Note that we may not use all of this... */
	out = (char *)malloc(len - len / 4 + 1);
	if (out == NULL) {
		*out_len = 0;
		return (NULL);
	}
	d = out;

	while (len > 0) {
		/* Collect the next group of (up to) four characters. */
		int v = 0;
		int group_size = 0;
		while (group_size < 4 && len > 0) {
			/* '=' or '_' padding indicates final group. */
			if (*src == '=' || *src == '_') {
				len = 0;
				break;
			}
			/* Skip illegal characters (including line breaks) */
			if (*src > 127 || *src < 32
			    || decode_table[*src] == 0xff) {
				len--;
				src++;
				continue;
			}
			v <<= 6;
			v |= decode_table[*src++];
			len --;
			group_size++;
		}
		/* Align a short group properly. */
		v <<= 6 * (4 - group_size);
		/* Unpack the group we just collected. */
		switch (group_size) {
		case 4: d[2] = v & 0xff;
			/* FALLTHROUGH */
		case 3: d[1] = (v >> 8) & 0xff;
			/* FALLTHROUGH */
		case 2: d[0] = (v >> 16) & 0xff;
			break;
		case 1: /* this is invalid! */
			break;
		}
		d += group_size * 3 / 4;
	}

	*out_len = d - out;
	return (out);
}

static char *
url_decode(const char *in)
{
	char *out, *d;
	const char *s;

	out = (char *)malloc(strlen(in) + 1);
	if (out == NULL)
		return (NULL);
	for (s = in, d = out; *s != '\0'; ) {
		if (s[0] == '%' && s[1] != '\0' && s[2] != '\0') {
			/* Try to convert % escape */
			int digit1 = tohex(s[1]);
			int digit2 = tohex(s[2]);
			if (digit1 >= 0 && digit2 >= 0) {
				/* Looks good, consume three chars */
				s += 3;
				/* Convert output */
				*d++ = ((digit1 << 4) | digit2);
				continue;
			}
			/* Else fall through and treat '%' as normal char */
		}
		*d++ = *s++;
	}
	*d = '\0';
	return (out);
}

static int
tohex(int c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	else if (c >= 'A' && c <= 'F')
		return (c - 'A' + 10);
	else if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);
	else
		return (-1);
}
