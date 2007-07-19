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
	if (__archive_string_ensure(dest, src->length + 1) == NULL)
		__archive_errx(1, "Out of memory");
	memcpy(dest->s, src->s, src->length);
	dest->length = src->length;
	dest->s[dest->length] = 0;
}

void
__archive_string_free(struct archive_string *as)
{
	as->length = 0;
	as->buffer_length = 0;
	if (as->s != NULL)
		free(as->s);
}

/* Returns NULL on any allocation failure. */
struct archive_string *
__archive_string_ensure(struct archive_string *as, size_t s)
{
	if (as->s && (s <= as->buffer_length))
		return (as);

	if (as->buffer_length < 32)
		as->buffer_length = 32;
	while (as->buffer_length < s)
		as->buffer_length *= 2;
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
