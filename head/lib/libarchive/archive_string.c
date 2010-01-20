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
#if defined(_WIN32) && !defined(__CYGWIN__)
#include <windows.h>
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
		as->buffer_length += as->buffer_length;
	else {
		/* Buffers 8k and over grow by at least 25% each time. */
		size_t old_length = as->buffer_length;
		as->buffer_length += as->buffer_length / 4;
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
__archive_strncat(struct archive_string *as, const void *_p, size_t n)
{
	size_t s;
	const char *p, *pp;

	p = (const char *)_p;

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

/*
 * Translates a wide character string into UTF-8 and appends
 * to the archive_string.  Note: returns NULL if conversion fails,
 * but still leaves a best-effort conversion in the argument as.
 */
struct archive_string *
__archive_strappend_w_utf8(struct archive_string *as, const wchar_t *w)
{
	char *p;
	unsigned wc;
	char buff[256];
	struct archive_string *return_val = as;

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
		wc = *w++;
		/* If this is a surrogate pair, assemble the full code point.*/
		/* Note: wc must not be wchar_t here, because the full code
		 * point can be more than 16 bits! */
		if (wc >= 0xD800 && wc <= 0xDBff
		    && *w >= 0xDC00 && *w <= 0xDFFF) {
			wc -= 0xD800;
			wc *= 0x400;
			wc += (*w - 0xDC00);
			wc += 0x10000;
			++w;
		}
		/* Translate code point to UTF8 */
		if (wc <= 0x7f) {
			*p++ = (char)wc;
		} else if (wc <= 0x7ff) {
			*p++ = 0xc0 | ((wc >> 6) & 0x1f);
			*p++ = 0x80 | (wc & 0x3f);
		} else if (wc <= 0xffff) {
			*p++ = 0xe0 | ((wc >> 12) & 0x0f);
			*p++ = 0x80 | ((wc >> 6) & 0x3f);
			*p++ = 0x80 | (wc & 0x3f);
		} else if (wc <= 0x1fffff) {
			*p++ = 0xf0 | ((wc >> 18) & 0x07);
			*p++ = 0x80 | ((wc >> 12) & 0x3f);
			*p++ = 0x80 | ((wc >> 6) & 0x3f);
			*p++ = 0x80 | (wc & 0x3f);
		} else {
			/* Unicode has no codes larger than 0x1fffff. */
			/* TODO: use \uXXXX escape here instead of ? */
			*p++ = '?';
			return_val = NULL;
		}
	}
	*p = '\0';
	archive_strcat(as, buff);
	return (return_val);
}

static int
utf8_to_unicode(int *pwc, const char *s, size_t n)
{
        int ch;

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
 * Return a wide-character Unicode string by converting this archive_string
 * from UTF-8.  We assume that systems with 16-bit wchar_t always use
 * UTF16 and systems with 32-bit wchar_t can accept UCS4.
 */
wchar_t *
__archive_string_utf8_w(struct archive_string *as)
{
	wchar_t *ws, *dest;
	int wc, wc2;/* Must be large enough for a 21-bit Unicode code point. */
	const char *src;
	int n;

	ws = (wchar_t *)malloc((as->length + 1) * sizeof(wchar_t));
	if (ws == NULL)
		__archive_errx(1, "Out of memory");
	dest = ws;
	src = as->s;
	while (*src != '\0') {
		n = utf8_to_unicode(&wc, src, 8);
		if (n == 0)
			break;
		if (n < 0) {
			free(ws);
			return (NULL);
		}
		src += n;
		if (wc >= 0xDC00 && wc <= 0xDBFF) {
			/* This is a leading surrogate; some idiot
			 * has translated UTF16 to UTF8 without combining
			 * surrogates; rebuild the full code point before
			 * continuing. */
			n = utf8_to_unicode(&wc2, src, 8);
			if (n < 0) {
				free(ws);
				return (NULL);
			}
			if (n == 0) /* Ignore the leading surrogate */
				break;
			if (wc2 < 0xDC00 || wc2 > 0xDFFF) {
				/* If the second character isn't a
				 * trailing surrogate, then someone
				 * has really screwed up and this is
				 * invalid. */
				free(ws);
				return (NULL);
			} else {
				src += n;
				wc -= 0xD800;
				wc *= 0x400;
				wc += wc2 - 0xDC00;
				wc += 0x10000;
			}
		}
		if ((sizeof(wchar_t) < 4) && (wc > 0xffff)) {
			/* We have a code point that won't fit into a
			 * wchar_t; convert it to a surrogate pair. */
			wc -= 0x10000;
			*dest++ = ((wc >> 10) & 0x3ff) + 0xD800;
			*dest++ = (wc & 0x3ff) + 0xDC00;
		} else
			*dest++ = wc;
	}
	*dest = L'\0';
	return (ws);
}

#if defined(_WIN32) && !defined(__CYGWIN__)

/*
 * Translates a wide character string into current locale character set
 * and appends to the archive_string.  Note: returns NULL if conversion
 * fails.
 *
 * Win32 builds use WideCharToMultiByte from the Windows API.
 * (Maybe Cygwin should too?  WideCharToMultiByte will know a
 * lot more about local character encodings than the wcrtomb()
 * wrapper is going to know.)
 */
struct archive_string *
__archive_strappend_w_mbs(struct archive_string *as, const wchar_t *w)
{
	char *p;
	int l, wl;
	BOOL useDefaultChar = FALSE;

	wl = (int)wcslen(w);
	l = wl * 4 + 4;
	p = malloc(l);
	if (p == NULL)
		__archive_errx(1, "Out of memory");
	/* To check a useDefaultChar is to simulate error handling of
	 * the my_wcstombs() which is running on non Windows system with
	 * wctomb().
	 * And to set NULL for last argument is necessary when a codepage
	 * is not CP_ACP(current locale).
	 */
	l = WideCharToMultiByte(CP_ACP, 0, w, wl, p, l, NULL, &useDefaultChar);
	if (l == 0) {
		free(p);
		return (NULL);
	}
	__archive_string_append(as, p, l);
	free(p);
	return (as);
}

#else

/*
 * Translates a wide character string into current locale character set
 * and appends to the archive_string.  Note: returns NULL if conversion
 * fails.
 *
 * Non-Windows uses ISO C wcrtomb() or wctomb() to perform the conversion
 * one character at a time.  If a non-Windows platform doesn't have
 * either of these, fall back to the built-in UTF8 conversion.
 */
struct archive_string *
__archive_strappend_w_mbs(struct archive_string *as, const wchar_t *w)
{
#if !defined(HAVE_WCTOMB) && !defined(HAVE_WCRTOMB)
	/* If there's no built-in locale support, fall back to UTF8 always. */
	return __archive_strappend_w_utf8(as, w);
#else
	/* We cannot use the standard wcstombs() here because it
	 * cannot tell us how big the output buffer should be.  So
	 * I've built a loop around wcrtomb() or wctomb() that
	 * converts a character at a time and resizes the string as
	 * needed.  We prefer wcrtomb() when it's available because
	 * it's thread-safe. */
	int n;
	char *p;
	char buff[256];
#if HAVE_WCRTOMB
	mbstate_t shift_state;

	memset(&shift_state, 0, sizeof(shift_state));
#else
	/* Clear the shift state before starting. */
	wctomb(NULL, L'\0');
#endif

	/*
	 * Convert one wide char at a time into 'buff', whenever that
	 * fills, append it to the string.
	 */
	p = buff;
	while (*w != L'\0') {
		/* Flush the buffer when we have <=16 bytes free. */
		/* (No encoding has a single character >16 bytes.) */
		if ((size_t)(p - buff) >= (size_t)(sizeof(buff) - MB_CUR_MAX)) {
			*p = '\0';
			archive_strcat(as, buff);
			p = buff;
		}
#if HAVE_WCRTOMB
		n = wcrtomb(p, *w++, &shift_state);
#else
		n = wctomb(p, *w++);
#endif
		if (n == -1)
			return (NULL);
		p += n;
	}
	*p = '\0';
	archive_strcat(as, buff);
	return (as);
#endif
}

#endif /* _WIN32 && ! __CYGWIN__ */
