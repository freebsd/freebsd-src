/*	$NetBSD: make_malloc.c,v 1.23 2020/10/05 19:27:47 rillig Exp $	*/

/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>

#include "make.h"

MAKE_RCSID("$NetBSD: make_malloc.c,v 1.23 2020/10/05 19:27:47 rillig Exp $");

#ifndef USE_EMALLOC

/* die when out of memory. */
static MAKE_ATTR_DEAD void
enomem(void)
{
	(void)fprintf(stderr, "%s: %s.\n", progname, strerror(ENOMEM));
	exit(2);
}

/* malloc, but die on error. */
void *
bmake_malloc(size_t len)
{
	void *p;

	if ((p = malloc(len)) == NULL)
		enomem();
	return p;
}

/* strdup, but die on error. */
char *
bmake_strdup(const char *str)
{
	size_t len;
	char *p;

	len = strlen(str) + 1;
	if ((p = malloc(len)) == NULL)
		enomem();
	return memcpy(p, str, len);
}

/* Allocate a string starting from str with exactly len characters. */
char *
bmake_strldup(const char *str, size_t len)
{
	char *p = bmake_malloc(len + 1);
	memcpy(p, str, len);
	p[len] = '\0';
	return p;
}

/* realloc, but die on error. */
void *
bmake_realloc(void *ptr, size_t size)
{
	if ((ptr = realloc(ptr, size)) == NULL)
		enomem();
	return ptr;
}
#endif

/* Allocate a string from start up to but excluding end. */
char *
bmake_strsedup(const char *start, const char *end)
{
	return bmake_strldup(start, (size_t)(end - start));
}
