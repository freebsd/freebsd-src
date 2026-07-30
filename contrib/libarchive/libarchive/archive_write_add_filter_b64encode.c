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

#define LBYTES	57

struct b64encode {
	int			mode;
	struct archive_string	name;
	struct archive_string	encoded_buff;
	size_t			bs;
	size_t			hold_len;
	unsigned char		hold[LBYTES];
};

static int archive_filter_b64encode_options(struct archive_write_filter *,
    const char *, const char *);
static int archive_filter_b64encode_open(struct archive_write_filter *);
static int archive_filter_b64encode_write(struct archive_write_filter *,
    const void *, size_t);
static int archive_filter_b64encode_close(struct archive_write_filter *);
static int archive_filter_b64encode_free(struct archive_write_filter *);
static void la_b64_encode(struct archive_string *, const unsigned char *, size_t);
static int64_t atol8(const char *, size_t);
static void free_data(struct b64encode *);

static const char base64[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/'
};

/*
 * Add a compress filter to this write handle.
 */
int
archive_write_add_filter_b64encode(struct archive *a)
{
	struct archive_write_filter *f;
	struct b64encode *b64encode;

	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_add_filter_b64encode");

	b64encode = calloc(1, sizeof(*b64encode));
	if (b64encode == NULL)
		goto memerr;
	archive_strcpy(&b64encode->name, "-");
	b64encode->mode = 0644;

	f = __archive_write_allocate_filter(a);
	if (f == NULL)
		goto memerr;
	f->name = "b64encode";
	f->code = ARCHIVE_FILTER_UU;
	f->data = b64encode;
	f->options = archive_filter_b64encode_options;
	f->open = archive_filter_b64encode_open;
	f->write = archive_filter_b64encode_write;
	f->close = archive_filter_b64encode_close;
	f->free = archive_filter_b64encode_free;

	return (ARCHIVE_OK);
memerr:
	free_data(b64encode);
	archive_set_error(a, ENOMEM,
	    "Can't allocate data for b64encode filter");
	return (ARCHIVE_FATAL);
}

/*
 * Set write options.
 */
static int
archive_filter_b64encode_options(struct archive_write_filter *f, const char *key,
    const char *value)
{
	struct b64encode *b64encode = f->data;

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
		b64encode->mode = (int)val & 0777;
		return (ARCHIVE_OK);
	} else if (strcmp(key, "name") == 0) {
		if (value == NULL) {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "name option requires a string");
			return (ARCHIVE_FAILED);
		}
		archive_strcpy(&b64encode->name, value);
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
archive_filter_b64encode_open(struct archive_write_filter *f)
{
	struct b64encode *b64encode = f->data;
	size_t bs = 65536, bpb;

	if (f->archive->magic == ARCHIVE_WRITE_MAGIC) {
		/* Buffer size should be a multiple number of the bytes
		 * per block for performance. */
		bpb = archive_write_get_bytes_per_block(f->archive);
		if (bpb > bs)
			bs = bpb;
		else if (bpb != 0)
			bs -= bs % bpb;
	}

	b64encode->bs = bs;
	if (archive_string_ensure(&b64encode->encoded_buff, bs + 512) == NULL) {
		archive_set_error(f->archive, ENOMEM,
		    "Can't allocate data for b64encode buffer");
		return (ARCHIVE_FATAL);
	}

	archive_string_sprintf(&b64encode->encoded_buff, "begin-base64 %o %s\n",
	    (unsigned int)b64encode->mode, b64encode->name.s);

	return (ARCHIVE_OK);
}

static void
la_b64_encode(struct archive_string *as, const unsigned char *p, size_t len)
{
	int c;

	for (; len >= 3; p += 3, len -= 3) {
		c = p[0] >> 2;
		archive_strappend_char(as, base64[c]);
		c = ((p[0] & 0x03) << 4) | ((p[1] & 0xf0) >> 4);
		archive_strappend_char(as, base64[c]);
		c = ((p[1] & 0x0f) << 2) | ((p[2] & 0xc0) >> 6);
		archive_strappend_char(as, base64[c]);
		c = p[2] & 0x3f;
		archive_strappend_char(as, base64[c]);
	}
	if (len > 0) {
		c = p[0] >> 2;
		archive_strappend_char(as, base64[c]);
		c = (p[0] & 0x03) << 4;
		if (len == 1) {
			archive_strappend_char(as, base64[c]);
			archive_strappend_char(as, '=');
			archive_strappend_char(as, '=');
		} else {
			c |= (p[1] & 0xf0) >> 4;
			archive_strappend_char(as, base64[c]);
			c = (p[1] & 0x0f) << 2;
			archive_strappend_char(as, base64[c]);
			archive_strappend_char(as, '=');
		}
	}
	archive_strappend_char(as, '\n');
}

/*
 * Write data to the encoded stream.
 */
static int
archive_filter_b64encode_write(struct archive_write_filter *f, const void *buff,
    size_t length)
{
	struct b64encode *b64encode = f->data;
	const unsigned char *p = buff;
	int ret = ARCHIVE_OK;

	if (length == 0)
		return (ret);

	if (b64encode->hold_len) {
		while (b64encode->hold_len < LBYTES && length > 0) {
			b64encode->hold[b64encode->hold_len++] = *p++;
			length--;
		}
		if (b64encode->hold_len < LBYTES)
			return (ret);
		la_b64_encode(&b64encode->encoded_buff, b64encode->hold, LBYTES);
		b64encode->hold_len = 0;
	}

	for (; length >= LBYTES; length -= LBYTES, p += LBYTES)
		la_b64_encode(&b64encode->encoded_buff, p, LBYTES);

	/* Save remaining bytes. */
	if (length > 0) {
		memcpy(b64encode->hold, p, length);
		b64encode->hold_len = length;
	}
	while (archive_strlen(&b64encode->encoded_buff) >= b64encode->bs) {
		ret = __archive_write_filter(f->next_filter,
		    b64encode->encoded_buff.s, b64encode->bs);
		memmove(b64encode->encoded_buff.s,
		    b64encode->encoded_buff.s + b64encode->bs,
		    b64encode->encoded_buff.length - b64encode->bs);
		b64encode->encoded_buff.length -= b64encode->bs;
	}

	return (ret);
}


/*
 * Finish the compression...
 */
static int
archive_filter_b64encode_close(struct archive_write_filter *f)
{
	struct b64encode *b64encode = f->data;

	/* Flush remaining bytes. */
	if (b64encode->hold_len != 0)
		la_b64_encode(&b64encode->encoded_buff, b64encode->hold,
		    b64encode->hold_len);
	archive_string_sprintf(&b64encode->encoded_buff, "====\n");
	/* Write the last block */
	archive_write_set_bytes_in_last_block(f->archive, 1);
	return __archive_write_filter(f->next_filter,
	    b64encode->encoded_buff.s, archive_strlen(&b64encode->encoded_buff));
}

static int
archive_filter_b64encode_free(struct archive_write_filter *f)
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
free_data(struct b64encode *b64encode)
{
	if (b64encode != NULL) {
		archive_string_free(&b64encode->name);
		archive_string_free(&b64encode->encoded_buff);
		free(b64encode);
	}
}
