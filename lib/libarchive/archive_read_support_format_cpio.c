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
/* #include <stdint.h> */ /* See archive_platform.h */
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

struct cpio_bin_header {
	unsigned char	c_magic[2];
	unsigned char	c_dev[2];
	unsigned char	c_ino[2];
	unsigned char	c_mode[2];
	unsigned char	c_uid[2];
	unsigned char	c_gid[2];
	unsigned char	c_nlink[2];
	unsigned char	c_rdev[2];
	unsigned char	c_mtime[4];
	unsigned char	c_namesize[2];
	unsigned char	c_filesize[4];
};

struct cpio_odc_header {
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

struct cpio_newc_header {
	char	c_magic[6];
	char	c_ino[8];
	char	c_mode[8];
	char	c_uid[8];
	char	c_gid[8];
	char	c_nlink[8];
	char	c_mtime[8];
	char	c_filesize[8];
	char	c_devmajor[8];
	char	c_devminor[8];
	char	c_rdevmajor[8];
	char	c_rdevminor[8];
	char	c_namesize[8];
	char	c_crc[8];
};

struct links_entry {
        struct links_entry      *next;
        struct links_entry      *previous;
        int                      links;
        dev_t                    dev;
        ino_t                    ino;
        char                    *name;
};

#define	CPIO_MAGIC   0x13141516
struct cpio {
	int			  magic;
	int			(*read_header)(struct archive_read *, struct cpio *,
				     struct archive_entry *, size_t *, size_t *);
	struct links_entry	 *links_head;
	struct archive_string	  entry_name;
	struct archive_string	  entry_linkname;
	off_t			  entry_bytes_remaining;
	off_t			  entry_offset;
	off_t			  entry_padding;
};

static int64_t	atol16(const char *, unsigned);
static int64_t	atol8(const char *, unsigned);
static int	archive_read_format_cpio_bid(struct archive_read *);
static int	archive_read_format_cpio_cleanup(struct archive_read *);
static int	archive_read_format_cpio_read_data(struct archive_read *,
		    const void **, size_t *, off_t *);
static int	archive_read_format_cpio_read_header(struct archive_read *,
		    struct archive_entry *);
static int	be4(const unsigned char *);
static int	find_odc_header(struct archive_read *);
static int	find_newc_header(struct archive_read *);
static int	header_bin_be(struct archive_read *, struct cpio *,
		    struct archive_entry *, size_t *, size_t *);
static int	header_bin_le(struct archive_read *, struct cpio *,
		    struct archive_entry *, size_t *, size_t *);
static int	header_newc(struct archive_read *, struct cpio *,
		    struct archive_entry *, size_t *, size_t *);
static int	header_odc(struct archive_read *, struct cpio *,
		    struct archive_entry *, size_t *, size_t *);
static int	is_octal(const char *, size_t);
static int	is_hex(const char *, size_t);
static int	le4(const unsigned char *);
static void	record_hardlink(struct cpio *cpio, struct archive_entry *entry);

int
archive_read_support_format_cpio(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct cpio *cpio;
	int r;

	cpio = (struct cpio *)malloc(sizeof(*cpio));
	if (cpio == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Can't allocate cpio data");
		return (ARCHIVE_FATAL);
	}
	memset(cpio, 0, sizeof(*cpio));
	cpio->magic = CPIO_MAGIC;

	r = __archive_read_register_format(a,
	    cpio,
	    archive_read_format_cpio_bid,
	    archive_read_format_cpio_read_header,
	    archive_read_format_cpio_read_data,
	    NULL,
	    archive_read_format_cpio_cleanup);

	if (r != ARCHIVE_OK)
		free(cpio);
	return (ARCHIVE_OK);
}


static int
archive_read_format_cpio_bid(struct archive_read *a)
{
	const void *h;
	const unsigned char *p;
	struct cpio *cpio;
	int bid;

	cpio = (struct cpio *)(a->format->data);

	if ((h = __archive_read_ahead(a, 6, NULL)) == NULL)
		return (-1);

	p = (const unsigned char *)h;
	bid = 0;
	if (memcmp(p, "070707", 6) == 0) {
		/* ASCII cpio archive (odc, POSIX.1) */
		cpio->read_header = header_odc;
		bid += 48;
		/*
		 * XXX TODO:  More verification; Could check that only octal
		 * digits appear in appropriate header locations. XXX
		 */
	} else if (memcmp(p, "070701", 6) == 0) {
		/* ASCII cpio archive (SVR4 without CRC) */
		cpio->read_header = header_newc;
		bid += 48;
		/*
		 * XXX TODO:  More verification; Could check that only hex
		 * digits appear in appropriate header locations. XXX
		 */
	} else if (memcmp(p, "070702", 6) == 0) {
		/* ASCII cpio archive (SVR4 with CRC) */
		/* XXX TODO: Flag that we should check the CRC. XXX */
		cpio->read_header = header_newc;
		bid += 48;
		/*
		 * XXX TODO:  More verification; Could check that only hex
		 * digits appear in appropriate header locations. XXX
		 */
	} else if (p[0] * 256 + p[1] == 070707) {
		/* big-endian binary cpio archives */
		cpio->read_header = header_bin_be;
		bid += 16;
		/* Is more verification possible here? */
	} else if (p[0] + p[1] * 256 == 070707) {
		/* little-endian binary cpio archives */
		cpio->read_header = header_bin_le;
		bid += 16;
		/* Is more verification possible here? */
	} else
		return (ARCHIVE_WARN);

	return (bid);
}

static int
archive_read_format_cpio_read_header(struct archive_read *a,
    struct archive_entry *entry)
{
	struct cpio *cpio;
	const void *h;
	size_t namelength;
	size_t name_pad;
	int r;

	cpio = (struct cpio *)(a->format->data);
	r = (cpio->read_header(a, cpio, entry, &namelength, &name_pad));

	if (r < ARCHIVE_WARN)
		return (r);

	/* Read name from buffer. */
	h = __archive_read_ahead(a, namelength + name_pad, NULL);
	if (h == NULL)
	    return (ARCHIVE_FATAL);
	__archive_read_consume(a, namelength + name_pad);
	archive_strncpy(&cpio->entry_name, (const char *)h, namelength);
	archive_entry_set_pathname(entry, cpio->entry_name.s);
	cpio->entry_offset = 0;

	/* If this is a symlink, read the link contents. */
	if (archive_entry_filetype(entry) == AE_IFLNK) {
		h = __archive_read_ahead(a, cpio->entry_bytes_remaining, NULL);
		if (h == NULL)
			return (ARCHIVE_FATAL);
		__archive_read_consume(a, cpio->entry_bytes_remaining);
		archive_strncpy(&cpio->entry_linkname, (const char *)h,
		    cpio->entry_bytes_remaining);
		archive_entry_set_symlink(entry, cpio->entry_linkname.s);
		cpio->entry_bytes_remaining = 0;
	}

	/* Compare name to "TRAILER!!!" to test for end-of-archive. */
	if (namelength == 11 && strcmp((const char *)h, "TRAILER!!!") == 0) {
	    /* TODO: Store file location of start of block. */
	    archive_set_error(&a->archive, 0, NULL);
	    return (ARCHIVE_EOF);
	}

	/* Detect and record hardlinks to previously-extracted entries. */
	record_hardlink(cpio, entry);

	return (r);
}

static int
archive_read_format_cpio_read_data(struct archive_read *a,
    const void **buff, size_t *size, off_t *offset)
{
	ssize_t bytes_read;
	struct cpio *cpio;

	cpio = (struct cpio *)(a->format->data);
	if (cpio->entry_bytes_remaining > 0) {
		*buff = __archive_read_ahead(a, 1, &bytes_read);
		if (bytes_read <= 0)
			return (ARCHIVE_FATAL);
		if (bytes_read > cpio->entry_bytes_remaining)
			bytes_read = cpio->entry_bytes_remaining;
		*size = bytes_read;
		*offset = cpio->entry_offset;
		cpio->entry_offset += bytes_read;
		cpio->entry_bytes_remaining -= bytes_read;
		__archive_read_consume(a, bytes_read);
		return (ARCHIVE_OK);
	} else {
		while (cpio->entry_padding > 0) {
			*buff = __archive_read_ahead(a, 1, &bytes_read);
			if (bytes_read <= 0)
				return (ARCHIVE_FATAL);
			if (bytes_read > cpio->entry_padding)
				bytes_read = cpio->entry_padding;
			__archive_read_consume(a, bytes_read);
			cpio->entry_padding -= bytes_read;
		}
		*buff = NULL;
		*size = 0;
		*offset = cpio->entry_offset;
		return (ARCHIVE_EOF);
	}
}

/*
 * Skip forward to the next cpio newc header by searching for the
 * 07070[12] string.  This should be generalized and merged with
 * find_odc_header below.
 */
static int
is_hex(const char *p, size_t len)
{
	while (len-- > 0) {
		if ((*p >= '0' && *p <= '9')
		    || (*p >= 'a' && *p <= 'f')
		    || (*p >= 'A' && *p <= 'F'))
			++p;
		else
			return (0);
	}
	return (1);
}

static int
find_newc_header(struct archive_read *a)
{
	const void *h;
	const char *p, *q;
	size_t skip, bytes, skipped = 0;

	for (;;) {
		h = __archive_read_ahead(a, sizeof(struct cpio_newc_header), &bytes);
		if (h == NULL)
			return (ARCHIVE_FATAL);
		p = h;
		q = p + bytes;

		/* Try the typical case first, then go into the slow search.*/
		if (memcmp("07070", p, 5) == 0
		    && (p[5] == '1' || p[5] == '2')
		    && is_hex(p, sizeof(struct cpio_newc_header)))
			return (ARCHIVE_OK);

		/*
		 * Scan ahead until we find something that looks
		 * like an odc header.
		 */
		while (p + sizeof(struct cpio_newc_header) < q) {
			switch (p[5]) {
			case '1':
			case '2':
				if (memcmp("07070", p, 5) == 0
					&& is_hex(p, sizeof(struct cpio_newc_header))) {
					skip = p - (const char *)h;
					__archive_read_consume(a, skip);
					skipped += skip;
					if (skipped > 0) {
						archive_set_error(&a->archive,
						    0,
						    "Skipped %d bytes before "
						    "finding valid header",
						    (int)skipped);
						return (ARCHIVE_WARN);
					}
					return (ARCHIVE_OK);
				}
				p += 2;
				break;
			case '0':
				p++;
				break;
			default:
				p += 6;
				break;
			}
		}
		skip = p - (const char *)h;
		__archive_read_consume(a, skip);
		skipped += skip;
	}
}

static int
header_newc(struct archive_read *a, struct cpio *cpio,
    struct archive_entry *entry, size_t *namelength, size_t *name_pad)
{
	const void *h;
	const struct cpio_newc_header *header;
	int r;

	r = find_newc_header(a);
	if (r < ARCHIVE_WARN)
		return (r);

	/* Read fixed-size portion of header. */
	h = __archive_read_ahead(a, sizeof(struct cpio_newc_header), NULL);
	if (h == NULL)
	    return (ARCHIVE_FATAL);
	__archive_read_consume(a, sizeof(struct cpio_newc_header));

	/* Parse out hex fields. */
	header = (const struct cpio_newc_header *)h;

	if (memcmp(header->c_magic, "070701", 6) == 0) {
		a->archive.archive_format = ARCHIVE_FORMAT_CPIO_SVR4_NOCRC;
		a->archive.archive_format_name = "ASCII cpio (SVR4 with no CRC)";
	} else if (memcmp(header->c_magic, "070702", 6) == 0) {
		a->archive.archive_format = ARCHIVE_FORMAT_CPIO_SVR4_CRC;
		a->archive.archive_format_name = "ASCII cpio (SVR4 with CRC)";
	} else {
		/* TODO: Abort here? */
	}

	archive_entry_set_devmajor(entry, atol16(header->c_devmajor, sizeof(header->c_devmajor)));
	archive_entry_set_devminor(entry, atol16(header->c_devminor, sizeof(header->c_devminor)));
	archive_entry_set_ino(entry, atol16(header->c_ino, sizeof(header->c_ino)));
	archive_entry_set_mode(entry, atol16(header->c_mode, sizeof(header->c_mode)));
	archive_entry_set_uid(entry, atol16(header->c_uid, sizeof(header->c_uid)));
	archive_entry_set_gid(entry, atol16(header->c_gid, sizeof(header->c_gid)));
	archive_entry_set_nlink(entry, atol16(header->c_nlink, sizeof(header->c_nlink)));
	archive_entry_set_rdevmajor(entry, atol16(header->c_rdevmajor, sizeof(header->c_rdevmajor)));
	archive_entry_set_rdevminor(entry, atol16(header->c_rdevminor, sizeof(header->c_rdevminor)));
	archive_entry_set_mtime(entry, atol16(header->c_mtime, sizeof(header->c_mtime)), 0);
	*namelength = atol16(header->c_namesize, sizeof(header->c_namesize));
	/* Pad name to 2 more than a multiple of 4. */
	*name_pad = (2 - *namelength) & 3;

	/*
	 * Note: entry_bytes_remaining is at least 64 bits and
	 * therefore guaranteed to be big enough for a 33-bit file
	 * size.
	 */
	cpio->entry_bytes_remaining =
	    atol16(header->c_filesize, sizeof(header->c_filesize));
	archive_entry_set_size(entry, cpio->entry_bytes_remaining);
	/* Pad file contents to a multiple of 4. */
	cpio->entry_padding = 3 & -cpio->entry_bytes_remaining;
	return (r);
}

/*
 * Skip forward to the next cpio odc header by searching for the
 * 070707 string.  This is a hand-optimized search that could
 * probably be easily generalized to handle all character-based
 * cpio variants.
 */
static int
is_octal(const char *p, size_t len)
{
	while (len-- > 0) {
		if (*p < '0' || *p > '7')
			return (0);
	        ++p;
	}
	return (1);
}

static int
find_odc_header(struct archive_read *a)
{
	const void *h;
	const char *p, *q;
	size_t skip, bytes, skipped = 0;

	for (;;) {
		h = __archive_read_ahead(a, sizeof(struct cpio_odc_header), &bytes);
		if (h == NULL)
			return (ARCHIVE_FATAL);
		p = h;
		q = p + bytes;

		/* Try the typical case first, then go into the slow search.*/
		if (memcmp("070707", p, 6) == 0
		    && is_octal(p, sizeof(struct cpio_odc_header)))
			return (ARCHIVE_OK);

		/*
		 * Scan ahead until we find something that looks
		 * like an odc header.
		 */
		while (p + sizeof(struct cpio_odc_header) < q) {
			switch (p[5]) {
			case '7':
				if (memcmp("070707", p, 6) == 0
					&& is_octal(p, sizeof(struct cpio_odc_header))) {
					skip = p - (const char *)h;
					__archive_read_consume(a, skip);
					skipped += skip;
					if (skipped > 0) {
						archive_set_error(&a->archive,
						    0,
						    "Skipped %d bytes before "
						    "finding valid header",
						    (int)skipped);
						return (ARCHIVE_WARN);
					}
					return (ARCHIVE_OK);
				}
				p += 2;
				break;
			case '0':
				p++;
				break;
			default:
				p += 6;
				break;
			}
		}
		skip = p - (const char *)h;
		__archive_read_consume(a, skip);
		skipped += skip;
	}
}

static int
header_odc(struct archive_read *a, struct cpio *cpio,
    struct archive_entry *entry, size_t *namelength, size_t *name_pad)
{
	const void *h;
	int r;
	const struct cpio_odc_header *header;

	a->archive.archive_format = ARCHIVE_FORMAT_CPIO_POSIX;
	a->archive.archive_format_name = "POSIX octet-oriented cpio";

	/* Find the start of the next header. */
	r = find_odc_header(a);
	if (r < ARCHIVE_WARN)
		return (r);

	/* Read fixed-size portion of header. */
	h = __archive_read_ahead(a, sizeof(struct cpio_odc_header), NULL);
	if (h == NULL)
	    return (ARCHIVE_FATAL);
	__archive_read_consume(a, sizeof(struct cpio_odc_header));

	/* Parse out octal fields. */
	header = (const struct cpio_odc_header *)h;

	archive_entry_set_dev(entry, atol8(header->c_dev, sizeof(header->c_dev)));
	archive_entry_set_ino(entry, atol8(header->c_ino, sizeof(header->c_ino)));
	archive_entry_set_mode(entry, atol8(header->c_mode, sizeof(header->c_mode)));
	archive_entry_set_uid(entry, atol8(header->c_uid, sizeof(header->c_uid)));
	archive_entry_set_gid(entry, atol8(header->c_gid, sizeof(header->c_gid)));
	archive_entry_set_nlink(entry, atol8(header->c_nlink, sizeof(header->c_nlink)));
	archive_entry_set_rdev(entry, atol8(header->c_rdev, sizeof(header->c_rdev)));
	archive_entry_set_mtime(entry, atol8(header->c_mtime, sizeof(header->c_mtime)), 0);
	*namelength = atol8(header->c_namesize, sizeof(header->c_namesize));
	*name_pad = 0; /* No padding of filename. */

	/*
	 * Note: entry_bytes_remaining is at least 64 bits and
	 * therefore guaranteed to be big enough for a 33-bit file
	 * size.
	 */
	cpio->entry_bytes_remaining =
	    atol8(header->c_filesize, sizeof(header->c_filesize));
	archive_entry_set_size(entry, cpio->entry_bytes_remaining);
	cpio->entry_padding = 0;
	return (r);
}

static int
header_bin_le(struct archive_read *a, struct cpio *cpio,
    struct archive_entry *entry, size_t *namelength, size_t *name_pad)
{
	const void *h;
	const struct cpio_bin_header *header;

	a->archive.archive_format = ARCHIVE_FORMAT_CPIO_BIN_LE;
	a->archive.archive_format_name = "cpio (little-endian binary)";

	/* Read fixed-size portion of header. */
	h = __archive_read_ahead(a, sizeof(struct cpio_bin_header), NULL);
	if (h == NULL)
	    return (ARCHIVE_FATAL);
	__archive_read_consume(a, sizeof(struct cpio_bin_header));

	/* Parse out binary fields. */
	header = (const struct cpio_bin_header *)h;

	archive_entry_set_dev(entry, header->c_dev[0] + header->c_dev[1] * 256);
	archive_entry_set_ino(entry, header->c_ino[0] + header->c_ino[1] * 256);
	archive_entry_set_mode(entry, header->c_mode[0] + header->c_mode[1] * 256);
	archive_entry_set_uid(entry, header->c_uid[0] + header->c_uid[1] * 256);
	archive_entry_set_gid(entry, header->c_gid[0] + header->c_gid[1] * 256);
	archive_entry_set_nlink(entry, header->c_nlink[0] + header->c_nlink[1] * 256);
	archive_entry_set_rdev(entry, header->c_rdev[0] + header->c_rdev[1] * 256);
	archive_entry_set_mtime(entry, le4(header->c_mtime), 0);
	*namelength = header->c_namesize[0] + header->c_namesize[1] * 256;
	*name_pad = *namelength & 1; /* Pad to even. */

	cpio->entry_bytes_remaining = le4(header->c_filesize);
	archive_entry_set_size(entry, cpio->entry_bytes_remaining);
	cpio->entry_padding = cpio->entry_bytes_remaining & 1; /* Pad to even. */
	return (ARCHIVE_OK);
}

static int
header_bin_be(struct archive_read *a, struct cpio *cpio,
    struct archive_entry *entry, size_t *namelength, size_t *name_pad)
{
	const void *h;
	const struct cpio_bin_header *header;

	a->archive.archive_format = ARCHIVE_FORMAT_CPIO_BIN_BE;
	a->archive.archive_format_name = "cpio (big-endian binary)";

	/* Read fixed-size portion of header. */
	h = __archive_read_ahead(a, sizeof(struct cpio_bin_header), NULL);
	if (h == NULL)
	    return (ARCHIVE_FATAL);
	__archive_read_consume(a, sizeof(struct cpio_bin_header));

	/* Parse out binary fields. */
	header = (const struct cpio_bin_header *)h;
	archive_entry_set_dev(entry, header->c_dev[0] * 256 + header->c_dev[1]);
	archive_entry_set_ino(entry, header->c_ino[0] * 256 + header->c_ino[1]);
	archive_entry_set_mode(entry, header->c_mode[0] * 256 + header->c_mode[1]);
	archive_entry_set_uid(entry, header->c_uid[0] * 256 + header->c_uid[1]);
	archive_entry_set_gid(entry, header->c_gid[0] * 256 + header->c_gid[1]);
	archive_entry_set_nlink(entry, header->c_nlink[0] * 256 + header->c_nlink[1]);
	archive_entry_set_rdev(entry, header->c_rdev[0] * 256 + header->c_rdev[1]);
	archive_entry_set_mtime(entry, be4(header->c_mtime), 0);
	*namelength = header->c_namesize[0] * 256 + header->c_namesize[1];
	*name_pad = *namelength & 1; /* Pad to even. */

	cpio->entry_bytes_remaining = be4(header->c_filesize);
	archive_entry_set_size(entry, cpio->entry_bytes_remaining);
	cpio->entry_padding = cpio->entry_bytes_remaining & 1; /* Pad to even. */
	return (ARCHIVE_OK);
}

static int
archive_read_format_cpio_cleanup(struct archive_read *a)
{
	struct cpio *cpio;

	cpio = (struct cpio *)(a->format->data);
        /* Free inode->name map */
        while (cpio->links_head != NULL) {
                struct links_entry *lp = cpio->links_head->next;

                if (cpio->links_head->name)
                        free(cpio->links_head->name);
                free(cpio->links_head);
                cpio->links_head = lp;
        }
	archive_string_free(&cpio->entry_name);
	free(cpio);
	(a->format->data) = NULL;
	return (ARCHIVE_OK);
}

static int
le4(const unsigned char *p)
{
	return ((p[0]<<16) + (p[1]<<24) + (p[2]<<0) + (p[3]<<8));
}


static int
be4(const unsigned char *p)
{
	return (p[0] + (p[1]<<8) + (p[2]<<16) + (p[3]<<24));
}

/*
 * Note that this implementation does not (and should not!) obey
 * locale settings; you cannot simply substitute strtol here, since
 * it does obey locale.
 */
static int64_t
atol8(const char *p, unsigned char_cnt)
{
	int64_t l;
	int digit;

	l = 0;
	while (char_cnt-- > 0) {
		if (*p >= '0' && *p <= '7')
			digit = *p - '0';
		else
			return (l);
		p++;
		l <<= 3;
		l |= digit;
	}
	return (l);
}

static int64_t
atol16(const char *p, unsigned char_cnt)
{
	int64_t l;
	int digit;

	l = 0;
	while (char_cnt-- > 0) {
		if (*p >= 'a' && *p <= 'f')
			digit = *p - 'a' + 10;
		else if (*p >= 'A' && *p <= 'F')
			digit = *p - 'A' + 10;
		else if (*p >= '0' && *p <= '9')
			digit = *p - '0';
		else
			return (l);
		p++;
		l <<= 4;
		l |= digit;
	}
	return (l);
}

static void
record_hardlink(struct cpio *cpio, struct archive_entry *entry)
{
        struct links_entry      *le;
	dev_t dev;
	ino_t ino;

	dev = archive_entry_dev(entry);
	ino = archive_entry_ino(entry);

        /*
         * First look in the list of multiply-linked files.  If we've
         * already dumped it, convert this entry to a hard link entry.
         */
        for (le = cpio->links_head; le; le = le->next) {
                if (le->dev == dev && le->ino == ino) {
                        archive_entry_copy_hardlink(entry, le->name);

                        if (--le->links <= 0) {
                                if (le->previous != NULL)
                                        le->previous->next = le->next;
                                if (le->next != NULL)
                                        le->next->previous = le->previous;
                                if (cpio->links_head == le)
                                        cpio->links_head = le->next;
				free(le->name);
                                free(le);
                        }

                        return;
                }
        }

        le = (struct links_entry *)malloc(sizeof(struct links_entry));
	if (le == NULL)
		__archive_errx(1, "Out of memory adding file to list");
        if (cpio->links_head != NULL)
                cpio->links_head->previous = le;
        le->next = cpio->links_head;
        le->previous = NULL;
        cpio->links_head = le;
        le->dev = dev;
        le->ino = ino;
        le->links = archive_entry_nlink(entry) - 1;
        le->name = strdup(archive_entry_pathname(entry));
	if (le->name == NULL)
		__archive_errx(1, "Out of memory adding file to list");
}
