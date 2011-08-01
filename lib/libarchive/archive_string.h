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
 *
 * $FreeBSD$
 *
 */

#ifndef __LIBARCHIVE_BUILD
#error This header is only to be used internally to libarchive.
#endif

#ifndef ARCHIVE_STRING_H_INCLUDED
#define	ARCHIVE_STRING_H_INCLUDED

#include <stdarg.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>  /* required for wchar_t on some systems */
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif

/*
 * Basic resizable/reusable string support a la Java's "StringBuffer."
 *
 * Unlike sbuf(9), the buffers here are fully reusable and track the
 * length throughout.
 *
 * Note that all visible symbols here begin with "__archive" as they
 * are internal symbols not intended for anyone outside of this library
 * to see or use.
 */

struct archive_string {
	char	*s;  /* Pointer to the storage */
	size_t	 length; /* Length of 's' */
	size_t	 buffer_length; /* Length of malloc-ed storage */
};

/* Initialize an archive_string object on the stack or elsewhere. */
#define	archive_string_init(a)	\
	do { (a)->s = NULL; (a)->length = 0; (a)->buffer_length = 0; } while(0)

/* Append a C char to an archive_string, resizing as necessary. */
struct archive_string *
__archive_strappend_char(struct archive_string *, char);
#define	archive_strappend_char __archive_strappend_char

/* Convert a wide-char string to UTF-8 and append the result. */
struct archive_string *
__archive_strappend_w_utf8(struct archive_string *, const wchar_t *);
#define	archive_strappend_w_utf8	__archive_strappend_w_utf8

/* Convert a wide-char string to current locale and append the result. */
/* Returns NULL if conversion fails. */
struct archive_string *
__archive_strappend_w_mbs(struct archive_string *, const wchar_t *);
#define	archive_strappend_w_mbs	__archive_strappend_w_mbs

/* Basic append operation. */
struct archive_string *
__archive_string_append(struct archive_string *as, const char *p, size_t s);

/* Copy one archive_string to another */
void
__archive_string_copy(struct archive_string *dest, struct archive_string *src);
#define archive_string_copy(dest, src) \
	__archive_string_copy(dest, src)

/* Concatenate one archive_string to another */
void
__archive_string_concat(struct archive_string *dest, struct archive_string *src);
#define archive_string_concat(dest, src) \
	__archive_string_concat(dest, src)

/* Ensure that the underlying buffer is at least as large as the request. */
struct archive_string *
__archive_string_ensure(struct archive_string *, size_t);
#define	archive_string_ensure __archive_string_ensure

/* Append C string, which may lack trailing \0. */
/* The source is declared void * here because this gets used with
 * "signed char *", "unsigned char *" and "char *" arguments.
 * Declaring it "char *" as with some of the other functions just
 * leads to a lot of extra casts. */
struct archive_string *
__archive_strncat(struct archive_string *, const void *, size_t);
#define	archive_strncat  __archive_strncat

/* Append a C string to an archive_string, resizing as necessary. */
#define	archive_strcat(as,p) __archive_string_append((as),(p),strlen(p))

/* Copy a C string to an archive_string, resizing as necessary. */
#define	archive_strcpy(as,p) \
	((as)->length = 0, __archive_string_append((as), (p), p == NULL ? 0 : strlen(p)))

/* Copy a C string to an archive_string with limit, resizing as necessary. */
#define	archive_strncpy(as,p,l) \
	((as)->length=0, archive_strncat((as), (p), (l)))

/* Return length of string. */
#define	archive_strlen(a) ((a)->length)

/* Set string length to zero. */
#define	archive_string_empty(a) ((a)->length = 0)

/* Release any allocated storage resources. */
void	__archive_string_free(struct archive_string *);
#define	archive_string_free  __archive_string_free

/* Like 'vsprintf', but resizes the underlying string as necessary. */
void	__archive_string_vsprintf(struct archive_string *, const char *,
	    va_list);
#define	archive_string_vsprintf	__archive_string_vsprintf

void	__archive_string_sprintf(struct archive_string *, const char *, ...);
#define	archive_string_sprintf	__archive_string_sprintf

/* Allocates a fresh buffer and converts as (assumed to be UTF-8) into it.
 * Returns NULL if conversion failed in any way. */
wchar_t *__archive_string_utf8_w(struct archive_string *as);


#endif
