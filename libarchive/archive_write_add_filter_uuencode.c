/*-
 * Copyright (c) 2012 Michihiro NAKAJIMA
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
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive.h"
#include "archive_integer.h"
#include "archive_private.h"
#include "archive_string.h"
#include "archive_write_private.h"

#define LBYTES 45

struct uuencode {
	int			mode;
	struct archive_string	name;
	struct archive_string	encoded_buff;
	size_t			bs;
	size_t			hold_len;
	unsigned char		hold[LBYTES];
};

static int archive_filter_uuencode_options(struct archive_write_filter *,
    const char *, const char *);
static int archive_filter_uuencode_open(struct archive_write_filter *);
static int archive_filter_uuencode_write(struct archive_write_filter *,
    const void *, size_t);
static int archive_filter_uuencode_close(struct archive_write_filter *);
static int archive_filter_uuencode_free(struct archive_write_filter *);
static void uu_encode(struct archive_string *, const unsigned char *, size_t);
static int64_t atol8(const char *, size_t);
static void free_data(struct uuencode *);

/*
 * Add a compress filter to this write handle.
 */
int
archive_write_add_filter_uuencode(struct archive *a)
{
	struct archive_write_filter *f;
	struct uuencode *uuencode;

	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_add_filter_uuencode");

	uuencode = calloc(1, sizeof(*uuencode));
	if (uuencode == NULL)
		goto memerr;
	archive_strcpy(&uuencode->name, "-");
	uuencode->mode = 0644;

	f = __archive_write_allocate_filter(a);
	if (f == NULL)
		goto memerr;
	f->name = "uuencode";
	f->code = ARCHIVE_FILTER_UU;
	f->data = uuencode;
	f->options = archive_filter_uuencode_options;
	f->open = archive_filter_uuencode_open;
	f->write = archive_filter_uuencode_write;
	f->close = archive_filter_uuencode_close;
	f->free = archive_filter_uuencode_free;

	return (ARCHIVE_OK);
memerr:
	free_data(uuencode);
	archive_set_error(a, ENOMEM,
	    "Can't allocate data for uuencode filter");
	return (ARCHIVE_FATAL);
}

/*
 * Set write options.
 */
static int
archive_filter_uuencode_options(struct archive_write_filter *f, const char *key,
    const char *value)
{
	struct uuencode *uuencode = f->data;

	if (strcmp(key, "mode") == 0) {
		int64_t val;

		if (value == NULL) {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "mode option requires octal digits");
			return (ARCHIVE_FAILED);
		}
		val = atol8(value, strlen(value));
		if (val < 0 || val > INT_MAX) {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "invalid mode option");
			return (ARCHIVE_FAILED);
		}
		uuencode->mode = (int)val & 0777;
		return (ARCHIVE_OK);
	} else if (strcmp(key, "name") == 0) {
		if (value == NULL) {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "name option requires a string");
			return (ARCHIVE_FAILED);
		}
		archive_strcpy(&uuencode->name, value);
		return (ARCHIVE_OK);
	}

	/* Note: The "warn" return is just to inform the options
	 * supervisor that we didn't handle it.  It will generate
	 * a suitable error if no one used this option. */
	return (ARCHIVE_WARN);
}

/*
 * Setup callback.
 */
static int
archive_filter_uuencode_open(struct archive_write_filter *f)
{
	struct uuencode *uuencode = f->data;
	size_t bs = 65536, bpb;

	if (f->archive->magic == ARCHIVE_WRITE_MAGIC) {
		/* Buffer size should be a multiple number of the of bytes
		 * per block for performance. */
		bpb = archive_write_get_bytes_per_block(f->archive);
		if (bpb > bs)
			bs = bpb;
		else if (bpb != 0)
			bs -= bs % bpb;
	}

	uuencode->bs = bs;
	if (archive_string_ensure(&uuencode->encoded_buff, bs + 512) == NULL) {
		archive_set_error(f->archive, ENOMEM,
		    "Can't allocate data for uuencode buffer");
		return (ARCHIVE_FATAL);
	}

	archive_string_sprintf(&uuencode->encoded_buff, "begin %o %s\n",
	    (unsigned int)uuencode->mode, uuencode->name.s);

	return (ARCHIVE_OK);
}

static void
uu_encode(struct archive_string *as, const unsigned char *p, size_t len)
{
	int c;

	c = (int)len;
	archive_strappend_char(as, c?c + 0x20:'`');
	for (; len >= 3; p += 3, len -= 3) {
		c = p[0] >> 2;
		archive_strappend_char(as, c?c + 0x20:'`');
		c = ((p[0] & 0x03) << 4) | ((p[1] & 0xf0) >> 4);
		archive_strappend_char(as, c?c + 0x20:'`');
		c = ((p[1] & 0x0f) << 2) | ((p[2] & 0xc0) >> 6);
		archive_strappend_char(as, c?c + 0x20:'`');
		c = p[2] & 0x3f;
		archive_strappend_char(as, c?c + 0x20:'`');
	}
	if (len > 0) {
		c = p[0] >> 2;
		archive_strappend_char(as, c?c + 0x20:'`');
		c = (p[0] & 0x03) << 4;
		if (len == 1) {
			archive_strappend_char(as, c?c + 0x20:'`');
			archive_strappend_char(as, '`');
			archive_strappend_char(as, '`');
		} else {
			c |= (p[1] & 0xf0) >> 4;
			archive_strappend_char(as, c?c + 0x20:'`');
			c = (p[1] & 0x0f) << 2;
			archive_strappend_char(as, c?c + 0x20:'`');
			archive_strappend_char(as, '`');
		}
	}
	archive_strappend_char(as, '\n');
}

/*
 * Write data to the encoded stream.
 */
static int
archive_filter_uuencode_write(struct archive_write_filter *f, const void *buff,
    size_t length)
{
	struct uuencode *uuencode = f->data;
	const unsigned char *p = buff;
	int ret = ARCHIVE_OK;

	if (length == 0)
		return (ret);

	if (uuencode->hold_len) {
		while (uuencode->hold_len < LBYTES && length > 0) {
			uuencode->hold[uuencode->hold_len++] = *p++;
			length--;
		}
		if (uuencode->hold_len < LBYTES)
			return (ret);
		uu_encode(&uuencode->encoded_buff, uuencode->hold, LBYTES);
		uuencode->hold_len = 0;
	}

	for (; length >= LBYTES; length -= LBYTES, p += LBYTES)
		uu_encode(&uuencode->encoded_buff, p, LBYTES);

	/* Save remaining bytes. */
	if (length > 0) {
		memcpy(uuencode->hold, p, length);
		uuencode->hold_len = length;
	}
	while (archive_strlen(&uuencode->encoded_buff) >= uuencode->bs) {
		ret = __archive_write_filter(f->next_filter,
		    uuencode->encoded_buff.s, uuencode->bs);
		memmove(uuencode->encoded_buff.s,
		    uuencode->encoded_buff.s + uuencode->bs,
		    uuencode->encoded_buff.length - uuencode->bs);
		uuencode->encoded_buff.length -= uuencode->bs;
	}

	return (ret);
}


/*
 * Finish the compression...
 */
static int
archive_filter_uuencode_close(struct archive_write_filter *f)
{
	struct uuencode *uuencode = f->data;

	/* Flush remaining bytes. */
	if (uuencode->hold_len != 0)
		uu_encode(&uuencode->encoded_buff, uuencode->hold,
		    uuencode->hold_len);
	archive_string_sprintf(&uuencode->encoded_buff, "`\nend\n");
	/* Write the last block */
	archive_write_set_bytes_in_last_block(f->archive, 1);
	return __archive_write_filter(f->next_filter,
	    uuencode->encoded_buff.s, archive_strlen(&uuencode->encoded_buff));
}

static int
archive_filter_uuencode_free(struct archive_write_filter *f)
{
	free_data(f->data);
	f->data = NULL;
	return (ARCHIVE_OK);
}

static int64_t
atol8(const char *p, size_t char_cnt)
{
	int64_t l;
	int digit;

	if (char_cnt == 0)
		return (-1);

	l = 0;
	while (char_cnt-- > 0) {
		if (*p >= '0' && *p <= '7')
			digit = *p - '0';
		else
			return (-1);
		p++;
		if (archive_ckd_mul_i64(&l, l, 8) ||
		    archive_ckd_add_i64(&l, l, digit))
			return (-1);
	}
	return (l);
}

static void
free_data(struct uuencode *uuencode)
{
	if (uuencode != NULL) {
		archive_string_free(&uuencode->name);
		archive_string_free(&uuencode->encoded_buff);
		free(uuencode);
	}
}
