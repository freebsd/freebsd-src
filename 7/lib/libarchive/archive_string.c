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

/*
 * Basic resizable string support, to simplify manipulating arbitrary-sized
 * strings while minimizing heap activity.
 */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif

#include "archive_private.h"
#include "archive_string.h"

struct archive_string *
__archive_string_append(struct archive_string *as, const char *p, size_t s)
{
	if (__archive_string_ensure(as, as->length + s + 1) == NULL)
		__archive_errx(1, "Out of memory");
	memcpy(as->s + as->length, p, s);
	as->s[as->length + s] = 0;
	as->length += s;
	return (as);
}

void
__archive_string_copy(struct archive_string *dest, struct archive_string *src)
{
	if (src->length == 0)
		dest->length = 0;
	else {
		if (__archive_string_ensure(dest, src->length + 1) == NULL)
			__archive_errx(1, "Out of memory");
		memcpy(dest->s, src->s, src->length);
		dest->length = src->length;
		dest->s[dest->length] = 0;
	}
}

void
__archive_string_concat(struct archive_string *dest, struct archive_string *src)
{
	if (src->length > 0) {
		if (__archive_string_ensure(dest, dest->length + src->length + 1) == NULL)
			__archive_errx(1, "Out of memory");
		memcpy(dest->s + dest->length, src->s, src->length);
		dest->length += src->length;
		dest->s[dest->length] = 0;
	}
}

void
__archive_string_free(struct archive_string *as)
{
	as->length = 0;
	as->buffer_length = 0;
	if (as->s != NULL) {
		free(as->s);
		as->s = NULL;
	}
}

/* Returns NULL on any allocation failure. */
struct archive_string *
__archive_string_ensure(struct archive_string *as, size_t s)
{
	/* If buffer is already big enough, don't reallocate. */
	if (as->s && (s <= as->buffer_length))
		return (as);

	/*
	 * Growing the buffer at least exponentially ensures that
	 * append operations are always linear in the number of
	 * characters appended.  Using a smaller growth rate for
	 * larger buffers reduces memory waste somewhat at the cost of
	 * a larger constant factor.
	 */
	if (as->buffer_length < 32)
		/* Start with a minimum 32-character buffer. */
		as->buffer_length = 32;
	else if (as->buffer_length < 8192)
		/* Buffers under 8k are doubled for speed. */
		as->buffer_length *= 2;
	else {
		/* Buffers 8k and over grow by at least 25% each time. */
		size_t old_length = as->buffer_length;
		as->buffer_length = (as->buffer_length * 5) / 4;
		/* Be safe: If size wraps, release buffer and return NULL. */
		if (as->buffer_length < old_length) {
			free(as->s);
			as->s = NULL;
			return (NULL);
		}
	}
	/*
	 * The computation above is a lower limit to how much we'll
	 * grow the buffer.  In any case, we have to grow it enough to
	 * hold the request.
	 */
	if (as->buffer_length < s)
		as->buffer_length = s;
	/* Now we can reallocate the buffer. */
	as->s = (char *)realloc(as->s, as->buffer_length);
	if (as->s == NULL)
		return (NULL);
	return (as);
}

struct archive_string *
__archive_strncat(struct archive_string *as, const char *p, size_t n)
{
	size_t s;
	const char *pp;

	/* Like strlen(p), except won't examine positions beyond p[n]. */
	s = 0;
	pp = p;
	while (*pp && s < n) {
		pp++;
		s++;
	}
	return (__archive_string_append(as, p, s));
}

struct archive_string *
__archive_strappend_char(struct archive_string *as, char c)
{
	return (__archive_string_append(as, &c, 1));
}

struct archive_string *
__archive_strappend_int(struct archive_string *as, int d, int base)
{
	static const char *digits = "0123456789abcdef";

	if (d < 0) {
		__archive_strappend_char(as, '-');
		d = -d;
	}
	if (d >= base)
		__archive_strappend_int(as, d/base, base);
	__archive_strappend_char(as, digits[d % base]);
	return (as);
}

/*
 * Home-grown wctomb for UTF-8.
 */
static int
my_wctomb_utf8(char *p, wchar_t wc)
{
	if (p == NULL)
		/* UTF-8 doesn't use shift states. */
		return (0);
	if (wc <= 0x7f) {
		p[0] = (char)wc;
		return (1);
	}
	if (wc <= 0x7ff) {
		p[0] = 0xc0 | ((wc >> 6) & 0x1f);
		p[1] = 0x80 | (wc & 0x3f);
		return (2);
	}
	if (wc <= 0xffff) {
		p[0] = 0xe0 | ((wc >> 12) & 0x0f);
		p[1] = 0x80 | ((wc >> 6) & 0x3f);
		p[2] = 0x80 | (wc & 0x3f);
		return (3);
	}
	if (wc <= 0x1fffff) {
		p[0] = 0xf0 | ((wc >> 18) & 0x07);
		p[1] = 0x80 | ((wc >> 12) & 0x3f);
		p[2] = 0x80 | ((wc >> 6) & 0x3f);
		p[3] = 0x80 | (wc & 0x3f);
		return (4);
	}
	/* Unicode has no codes larger than 0x1fffff. */
	/*
	 * Awkward point:  UTF-8 <-> wchar_t conversions
	 * can actually fail.
	 */
	return (-1);
}

static int
my_wcstombs(struct archive_string *as, const wchar_t *w,
    int (*func)(char *, wchar_t))
{
	int n;
	char *p;
	char buff[256];

	/* Clear the shift state before starting. */
	(*func)(NULL, L'\0');

	/*
	 * Convert one wide char at a time into 'buff', whenever that
	 * fills, append it to the string.
	 */
	p = buff;
	while (*w != L'\0') {
		/* Flush the buffer when we have <=16 bytes free. */
		/* (No encoding has a single character >16 bytes.) */
		if ((size_t)(p - buff) >= (size_t)(sizeof(buff) - 16)) {
			*p = '\0';
			archive_strcat(as, buff);
			p = buff;
		}
		n = (*func)(p, *w++);
		if (n == -1)
			return (-1);
		p += n;
	}
	*p = '\0';
	archive_strcat(as, buff);
	return (0);
}

/*
 * Translates a wide character string into UTF-8 and appends
 * to the archive_string.  Note: returns NULL if conversion fails.
 */
struct archive_string *
__archive_strappend_w_utf8(struct archive_string *as, const wchar_t *w)
{
	if (my_wcstombs(as, w, my_wctomb_utf8))
		return (NULL);
	return (as);
}

/*
 * Translates a wide character string into current locale character set
 * and appends to the archive_string.  Note: returns NULL if conversion
 * fails.
 */
struct archive_string *
__archive_strappend_w_mbs(struct archive_string *as, const wchar_t *w)
{
#if HAVE_WCTOMB
	if (my_wcstombs(as, w, wctomb))
		return (NULL);
#else
	/* TODO: Can we do better than this?  Are there platforms
	 * that have locale support but don't have wctomb()? */
	if (my_wcstombs(as, w, my_wctomb_utf8))
		return (NULL);
#endif
	return (as);
}


/*
 * Home-grown mbtowc for UTF-8.  Some systems lack UTF-8
 * (or even lack mbtowc()) and we need UTF-8 support for pax
 * format.  So please don't replace this with a call to the
 * standard mbtowc() function!
 */
static int
my_mbtowc_utf8(wchar_t *pwc, const char *s, size_t n)
{
        int ch;

	/* Standard behavior:  a NULL value for 's' just resets shift state. */
        if (s == NULL)
                return (0);
	/* If length argument is zero, don't look at the first character. */
	if (n <= 0)
		return (-1);

        /*
	 * Decode 1-4 bytes depending on the value of the first byte.
	 */
        ch = (unsigned char)*s;
	if (ch == 0) {
		return (0); /* Standard:  return 0 for end-of-string. */
	}
	if ((ch & 0x80) == 0) {
                *pwc = ch & 0x7f;
		return (1);
        }
	if ((ch & 0xe0) == 0xc0) {
		if (n < 2)
			return (-1);
		if ((s[1] & 0xc0) != 0x80) return (-1);
                *pwc = ((ch & 0x1f) << 6) | (s[1] & 0x3f);
		return (2);
        }
	if ((ch & 0xf0) == 0xe0) {
		if (n < 3)
			return (-1);
		if ((s[1] & 0xc0) != 0x80) return (-1);
		if ((s[2] & 0xc0) != 0x80) return (-1);
                *pwc = ((ch & 0x0f) << 12)
		    | ((s[1] & 0x3f) << 6)
		    | (s[2] & 0x3f);
		return (3);
        }
	if ((ch & 0xf8) == 0xf0) {
		if (n < 4)
			return (-1);
		if ((s[1] & 0xc0) != 0x80) return (-1);
		if ((s[2] & 0xc0) != 0x80) return (-1);
		if ((s[3] & 0xc0) != 0x80) return (-1);
                *pwc = ((ch & 0x07) << 18)
		    | ((s[1] & 0x3f) << 12)
		    | ((s[2] & 0x3f) << 6)
		    | (s[3] & 0x3f);
		return (4);
        }
	/* Invalid first byte. */
	return (-1);
}

/*
 * Return a wide-character string by converting this archive_string
 * from UTF-8.
 */
wchar_t *
__archive_string_utf8_w(struct archive_string *as)
{
	wchar_t *ws, *dest;
	const char *src;
	int n;
	int err;

	ws = (wchar_t *)malloc((as->length + 1) * sizeof(wchar_t));
	if (ws == NULL)
		__archive_errx(1, "Out of memory");
	err = 0;
	dest = ws;
	src = as->s;
	while (*src != '\0') {
		n = my_mbtowc_utf8(dest, src, 8);
		if (n == 0)
			break;
		if (n < 0) {
			free(ws);
			return (NULL);
		}
		dest++;
		src += n;
	}
	*dest++ = L'\0';
	return (ws);
}
