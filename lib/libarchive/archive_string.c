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

/*
 * Basic resizable string support, to simplify manipulating arbitrary-sized
 * strings while minimizing heap activity.
 */

#ifdef HAVE_DMALLOC
#include <dmalloc.h>
#endif
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "archive_string.h"

struct archive_string *
__archive_string_append(struct archive_string *as, const char *p, size_t s)
{
	__archive_string_ensure(as, as->length + s + 1);
	memcpy(as->s + as->length, p, s);
	as->s[as->length + s] = 0;
	as->length += s;
	return (as);
}

void
__archive_string_free(struct archive_string *as)
{
	as->length = 0;
	as->buffer_length = 0;
	if (as->s != NULL)
		free(as->s);
}

struct archive_string *
__archive_string_ensure(struct archive_string *as, size_t s)
{
	if (as->s && (s <= as->buffer_length))
		return (as);

	if (as->buffer_length < 32)
		as->buffer_length = 32;
	while (as->buffer_length < s)
		as->buffer_length *= 2;
	as->s = realloc(as->s, as->buffer_length);
	if (as->s == NULL)
		errx(1,"Out of memory");
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

#if 0
/* Append Unicode character to string using UTF8 encoding */
struct archive_string *
__archive_strappend_char_UTF8(struct archive_string *as, int c)
{
	char buff[6];

	if (c <= 0x7f) {
		buff[0] = c;
		return (__archive_string_append(as, buff, 1));
	} else if (c <= 0x7ff) {
		buff[0] = 0xc0 | (c >> 6);
		buff[1] = 0x80 | (c & 0x3f);
		return (__archive_string_append(as, buff, 2));
	} else if (c <= 0xffff) {
		buff[0] = 0xe0 | (c >> 12);
		buff[1] = 0x80 | ((c >> 6) & 0x3f);
		buff[2] = 0x80 | (c & 0x3f);
		return (__archive_string_append(as, buff, 3));
	} else if (c <= 0x1fffff) {
		buff[0] = 0xf0 | (c >> 18);
		buff[1] = 0x80 | ((c >> 12) & 0x3f);
		buff[2] = 0x80 | ((c >> 6) & 0x3f);
		buff[3] = 0x80 | (c & 0x3f);
		return (__archive_string_append(as, buff, 4));
	} else if (c <= 0x3ffffff) {
		buff[0] = 0xf8 | (c >> 24);
		buff[1] = 0x80 | ((c >> 18) & 0x3f);
		buff[2] = 0x80 | ((c >> 12) & 0x3f);
		buff[3] = 0x80 | ((c >> 6) & 0x3f);
		buff[4] = 0x80 | (c & 0x3f);
		return (__archive_string_append(as, buff, 5));
	} else if (c <= 0x7fffffff) {
		buff[0] = 0xfc | (c >> 30);
		buff[1] = 0x80 | ((c >> 24) & 0x3f);
		buff[1] = 0x80 | ((c >> 18) & 0x3f);
		buff[2] = 0x80 | ((c >> 12) & 0x3f);
		buff[3] = 0x80 | ((c >> 6) & 0x3f);
		buff[4] = 0x80 | (c & 0x3f);
		return (__archive_string_append(as, buff, 6));
	} else {
		/* TODO: Handle this error?? */
		return (as);
	}
}
#endif
