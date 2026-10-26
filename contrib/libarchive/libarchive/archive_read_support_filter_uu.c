/*-
 * Copyright (c) 2009-2011 Michihiro NAKAJIMA
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
#include "archive_integer.h"
#include "archive_private.h"
#include "archive_read_private.h"

/* Maximum lookahead during bid phase */
#define MAX_LINE_LENGTH 128*1024 /* in bytes */

struct uu {
	unsigned char	*out_buff;
#define OUT_BUFF_SIZE	(64 * 1024)
	int		 state;
#define ST_FIND_HEAD	0
#define ST_READ_UU	1
#define ST_UUEND	2
#define ST_READ_BASE64	3
#define ST_IGNORE	4
	mode_t		mode;
	int		mode_set;
	char		*name;
};

static int	uudecode_bidder_bid(struct archive_read_filter_bidder *,
		    struct archive_read_filter *f);
static int	uudecode_bidder_init(struct archive_read_filter *);

static int	uudecode_read_header(struct archive_read_filter *,
		    struct archive_entry *entry);
static ssize_t	uudecode_filter_read(struct archive_read_filter *,
		    const void **);
static int	uudecode_filter_close(struct archive_read_filter *);

#if ARCHIVE_VERSION_NUMBER < 4000000
/* Deprecated; remove in libarchive 4.0 */
int
archive_read_support_compression_uu(struct archive *a)
{
	return archive_read_support_filter_uu(a);
}
#endif

static const struct archive_read_filter_bidder_vtable
uudecode_bidder_vtable = {
	.bid = uudecode_bidder_bid,
	.init = uudecode_bidder_init,
};

int
archive_read_support_filter_uu(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;

	return __archive_read_register_bidder(a, NULL,
			&uudecode_bidder_vtable);
}

static const unsigned char uuchar[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 00 - 0F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 10 - 1F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 20 - 2F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 30 - 3F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 40 - 4F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 50 - 5F */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 60 - 6F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 70 - 7F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 80 - 8F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 90 - 9F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* A0 - AF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* B0 - BF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* C0 - CF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* D0 - DF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* E0 - EF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* F0 - FF */
};

static const unsigned char base64[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 00 - 0F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 10 - 1F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, /* 20 - 2F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, /* 30 - 3F */
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 40 - 4F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, /* 50 - 5F */
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 60 - 6F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, /* 70 - 7F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 80 - 8F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 90 - 9F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* A0 - AF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* B0 - BF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* C0 - CF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* D0 - DF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* E0 - EF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* F0 - FF */
};

static const int base64num[128] = {
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0, /* 00 - 0F */
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0, /* 10 - 1F */
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0, 62,  0,  0,  0, 63, /* 20 - 2F */
	52, 53, 54, 55, 56, 57, 58, 59,
	60, 61,  0,  0,  0,  0,  0,  0, /* 30 - 3F */
	 0,  0,  1,  2,  3,  4,  5,  6,
	 7,  8,  9, 10, 11, 12, 13, 14, /* 40 - 4F */
	15, 16, 17, 18, 19, 20, 21, 22,
	23, 24, 25,  0,  0,  0,  0,  0, /* 50 - 5F */
	 0, 26, 27, 28, 29, 30, 31, 32,
	33, 34, 35, 36, 37, 38, 39, 40, /* 60 - 6F */
	41, 42, 43, 44, 45, 46, 47, 48,
	49, 50, 51,  0,  0,  0,  0,  0, /* 70 - 7F */
};

/*
 * Read the next line from upstream beginning at given offset. No more than
 * MAX_LINE_LENGTH bytes will be read into stream. Bytes won't be consumed.
 * The length of the line excluding the newline is stored in len.
 *
 * If parsed is non-zero, only already available bytes are considered.
 * This option is supposed to be used with a 0 offset.
 *
 * Amount of unconsumed bytes, including newline character(s), or up to
 * MAX_LINE_LENGTH if no newline was found, is stored in unconsumed.
 *
 * If a line is found, pointer to beginning of line is returned.
 */
static const unsigned char *
read_line_ahead(struct archive_read_filter *f, ssize_t parsed,
    size_t offset, size_t *len, ssize_t *unconsumed)
{
	/* Read approximately two new lines each iteration. */
	size_t step = 160;
	const unsigned char *line;
	ssize_t avail;
	size_t request, start;

	start = offset;
	line = NULL;
	*len = 0;
	*unconsumed = 0;
	request = offset == 0 ? 1 : offset + step;
	do {
		size_t remaining, nl = 1;
		const unsigned char *b, *p;

		/* Read bytes from upstream filter. */
		b = __archive_read_filter_ahead(f, request, &avail);
		if (avail > MAX_LINE_LENGTH)
			avail = MAX_LINE_LENGTH;
		*unconsumed = avail;
		if (b == NULL) {
			if (avail > 0 && (size_t)avail > offset)
				b = __archive_read_filter_ahead(f, avail,
				    &avail);
			if (b == NULL)
				break;
		}
		remaining = avail - offset;
		/* Locate end of line (\r, \n, \r\n). */
		p = memchr(b + offset, '\n', remaining);
		if (p == NULL) {
			p = memchr(b + offset, '\r', remaining);
			if (p == b + avail - 1 && step > 1) {
				/* Is it \r or \r\n, missing one byte? */
				p = NULL;
				step = 1;
			}
		} else if (p > b + start && p[-1] == '\r') {
			/* "\r\n" takes two bytes. */
			nl++;
			--p;
		}

		/* Return line or prepare to read more bytes. */
		if (p != NULL) {
			line = b + start;
			*len = p - line;
			*unconsumed = *len + nl;
		} else {
			/* Keep offset for check of possibly missing \n in \r\n. */
			if (step != 1)
				offset = avail;
			if (archive_ckd_add_size(&request, avail, step))
				break;
			if (request > MAX_LINE_LENGTH)
				request = MAX_LINE_LENGTH;
		}
	} while (line == NULL && avail < MAX_LINE_LENGTH && !parsed);

	return line;
}

#define UUDECODE(c) (((c) - 0x20) & 0x3f)

/*
 * Parses the header line of a uuencoded file, "begin <mode> <name>", or of
 * a base64 encoded file, "begin-base64 <mode> <name>". The mode is a
 * sequence of at least three octal digits: encoders don't all write the same
 * number of them, for example 644, 0644 or 4755 for the same kind of file.
 *
 * Returns the length of the "begin " or "begin-base64 " prefix, or 0 if the
 * line is not a header. Stores the permission bits of the mode in *mode and
 * the offset of the name in *name_offset, unless they are NULL.
 */
static size_t
parse_header(const unsigned char *p, size_t len, mode_t *mode,
    size_t *name_offset)
{
	size_t i, l;
	mode_t m;

	if (len >= 6 && memcmp(p, "begin ", 6) == 0)
		l = 6;
	else if (len >= 13 && memcmp(p, "begin-base64 ", 13) == 0)
		l = 13;
	else
		return (0);

	m = 0;
	for (i = l; i < len && p[i] >= '0' && p[i] <= '7'; i++)
		m = ((m << 3) | (p[i] - '0')) & 0777;
	/*
	 * Need at least three digits, as too lenient a check would have
	 * the bidder take other kinds of data for a header. Then a space and
	 * a one character name.
	 */
	if (i - l < 3 || i + 1 >= len || p[i] != ' ')
		return (0);

	if (mode != NULL)
		*mode = m;
	if (name_offset != NULL)
		*name_offset = i + 1;
	return (l);
}

static int
uudecode_bidder_bid(struct archive_read_filter_bidder *b,
    struct archive_read_filter *f)
{
	const unsigned char *p;
	size_t l;
	int firstline;
	size_t len, offset;
	ssize_t unconsumed;

	(void)b; /* UNUSED */

	offset = 0;
	firstline = 20;
	for (;;) {
		p = read_line_ahead(f, 0, offset, &len, &unconsumed);
		if (p == NULL)
			return (0); /* No match found. */
		offset += unconsumed;

		l = parse_header(p, len, NULL, NULL);
		if (l != 0)
			break;
		firstline = 0;
	}
	p = read_line_ahead(f, 0, offset, &len, &unconsumed);
	if (p == NULL)
		return (0); /* No match found. */

	if (l == 6) {
		/* "begin " */
		if (!uuchar[*p])
			return (0);
		/* Get a length of decoded bytes. */
		l = UUDECODE(*p++); len--;
		if (l == 0)
			/* empty encoded file */
			return (firstline+30);
		if (l > 45)
			/* Normally, maximum length is 45(character 'M'). */
			return (0);
		if (l > len)
			return (0); /* Line too short. */
		while (l) {
			if (!uuchar[*p++])
				return (0);
			--len;
			--l;
		}
		if (len == 1 &&
		    (uuchar[*p] ||		 /* Check sum. */
		     (*p >= 'a' && *p <= 'z'))) {/* Padding data(MINIX). */
			++p;
			--len;
		}
		offset += unconsumed;
		p = read_line_ahead(f, 0, offset, &len, &unconsumed);
		if (p != NULL && uuchar[*p])
			return (firstline+30);
	} else if (l == 13) {
		/* "begin-base64 " */
		while (len > 0) {
			if (!base64[*p++])
				return (0);
			len--;
		}
		if (len == 0)
			return (firstline+30);
		if (len == 4 && memcmp(p, "====", 4) == 0)
			return (firstline+40);
	}

	return (0);
}

static const struct archive_read_filter_vtable
uudecode_reader_vtable = {
	.read = uudecode_filter_read,
	.close = uudecode_filter_close,
	.read_header = uudecode_read_header
};

static int
uudecode_bidder_init(struct archive_read_filter *f)
{
	struct uu *uu;
	void *out_buff;

	f->code = ARCHIVE_FILTER_UU;
	f->name = "uu";

	uu = calloc(1, sizeof(*uu));
	out_buff = malloc(OUT_BUFF_SIZE);
	if (uu == NULL || out_buff == NULL) {
		archive_set_error(&f->archive->archive, ENOMEM,
		    "Can't allocate data for uudecode");
		free(uu);
		free(out_buff);
		return (ARCHIVE_FATAL);
	}

	f->data = uu;
	uu->out_buff = out_buff;
	uu->state = ST_FIND_HEAD;
	uu->mode_set = 0;
	uu->name = NULL;
	f->vtable = &uudecode_reader_vtable;

	return (ARCHIVE_OK);
}

static int
uudecode_read_header(struct archive_read_filter *f, struct archive_entry *entry)
{
	struct uu *uu = f->data;

	if (uu->mode_set != 0)
		archive_entry_set_mode(entry, S_IFREG | uu->mode);

	if (uu->name != NULL)
		archive_entry_set_pathname(entry, uu->name);

	return (ARCHIVE_OK);
}

static ssize_t
uudecode_filter_read(struct archive_read_filter *f, const void **buff)
{
	struct uu *uu = f->data;
	unsigned char *out;
	ssize_t total;

	out = uu->out_buff;
	total = 0;

	while (uu->state != ST_IGNORE) {
		const unsigned char *b;
		ssize_t unconsumed;
		ssize_t l, body;
		ssize_t namelen;
		size_t len, name_offset;
		mode_t mode;

		b = read_line_ahead(f->upstream, total, 0,
		    &len, &unconsumed);
		if (b == NULL) {
			if (unconsumed < 0)
				return (ARCHIVE_FATAL);
			if (unconsumed == MAX_LINE_LENGTH) {
				archive_set_error(&f->archive->archive,
				    ARCHIVE_ERRNO_FILE_FORMAT,
				    "Invalid format data");
				return (ARCHIVE_FATAL);
			}
			if (total != 0)
				break;
			if (uu->state != ST_FIND_HEAD) {
				archive_set_error(&f->archive->archive,
				    ARCHIVE_ERRNO_MISC,
				    "Insufficient compressed data");
				return (ARCHIVE_FATAL);
			}
			/* Non-ascii character or end of stream is found. */
			uu->state = ST_IGNORE;
		}

		switch (uu->state) {
		case ST_IGNORE:
		default:
			break;
		case ST_FIND_HEAD:
			l = parse_header(b, len, &mode, &name_offset);
			if (l != 0) {
				if (l == 6)
					uu->state = ST_READ_UU;
				else
					uu->state = ST_READ_BASE64;
				uu->mode = mode;
				uu->mode_set = 1;
				namelen = len - name_offset;
				if (namelen > 1) {
					free(uu->name);
					uu->name = malloc(namelen + 1);
					if (uu->name == NULL) {
						archive_set_error(
						    &f->archive->archive,
						    ENOMEM,
						    "Can't allocate data for uudecode");
						return (ARCHIVE_FATAL);
					}
					strncpy(uu->name,
					    (const char *)(b + name_offset),
					    namelen);
					uu->name[namelen] = '\0';
				}
			}
			break;
		case ST_READ_UU:
			if (total + len * 2 > OUT_BUFF_SIZE)
				goto finish;
			body = len;
			if (!uuchar[*b] || body <= 0) {
				archive_set_error(&f->archive->archive,
				    ARCHIVE_ERRNO_MISC,
				    "Insufficient compressed data");
				return (ARCHIVE_FATAL);
			}
			/* Get length of undecoded bytes of current line. */
			l = UUDECODE(*b++);
			body--;
			if (l > body) {
				archive_set_error(&f->archive->archive,
				    ARCHIVE_ERRNO_MISC,
				    "Insufficient compressed data");
				return (ARCHIVE_FATAL);
			}
			if (l == 0) {
				uu->state = ST_UUEND;
				break;
			}
			while (l > 0) {
				int n = 0;

				if (!uuchar[b[0]] || !uuchar[b[1]])
					break;
				n = UUDECODE(*b++) << 18;
				n |= UUDECODE(*b++) << 12;
				*out++ = n >> 16; total++;
				--l;

				if (l > 0) {
					if (!uuchar[b[0]])
						break;
					n |= UUDECODE(*b++) << 6;
					*out++ = (n >> 8) & 0xFF; total++;
					--l;
				}
				if (l > 0) {
					if (!uuchar[b[0]])
						break;
					n |= UUDECODE(*b++);
					*out++ = n & 0xFF; total++;
					--l;
				}
			}
			if (l) {
				archive_set_error(&f->archive->archive,
				    ARCHIVE_ERRNO_MISC,
				    "Insufficient compressed data");
				return (ARCHIVE_FATAL);
			}
			break;
		case ST_UUEND:
			if (len == 3 && memcmp(b, "end", 3) == 0)
				uu->state = ST_IGNORE;
			else {
				archive_set_error(&f->archive->archive,
				    ARCHIVE_ERRNO_MISC,
				    "Insufficient compressed data");
				return (ARCHIVE_FATAL);
			}
			break;
		case ST_READ_BASE64:
			if (total + len * 2 > OUT_BUFF_SIZE)
				goto finish;
			l = len;
			if (l >= 3 && b[0] == '=' && b[1] == '=' &&
			    b[2] == '=') {
				uu->state = ST_IGNORE;
				break;
			}
			while (l > 0) {
				int n = 0;

				if (!base64[b[0]] || !base64[b[1]])
					break;
				n = base64num[*b++] << 18;
				n |= base64num[*b++] << 12;
				*out++ = n >> 16; total++;
				l -= 2;

				if (l > 0) {
					if (*b == '=')
						break;
					if (!base64[*b])
						break;
					n |= base64num[*b++] << 6;
					*out++ = (n >> 8) & 0xFF; total++;
					--l;
				}
				if (l > 0) {
					if (*b == '=')
						break;
					if (!base64[*b])
						break;
					n |= base64num[*b++];
					*out++ = n & 0xFF; total++;
					--l;
				}
			}
			if (l && *b != '=') {
				archive_set_error(&f->archive->archive,
				    ARCHIVE_ERRNO_MISC,
				    "Insufficient compressed data");
				return (ARCHIVE_FATAL);
			}
			break;
		}
		__archive_read_filter_consume(f->upstream, unconsumed);
	}
finish:
	*buff = uu->out_buff;
	return (total);
}

static int
uudecode_filter_close(struct archive_read_filter *f)
{
	struct uu *uu = f->data;

	free(uu->out_buff);
	free(uu->name);
	free(uu);

	return (ARCHIVE_OK);
}

