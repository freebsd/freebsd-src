/*	$NetBSD: make_malloc.h,v 1.10 2020/08/29 16:47:45 rillig Exp $	*/

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

#ifndef USE_EMALLOC
void *bmake_malloc(size_t);
void *bmake_realloc(void *, size_t);
char *bmake_strdup(const char *);
char *bmake_strldup(const char *, size_t);
#else
#include <util.h>
#define bmake_malloc(x)         emalloc(x)
#define bmake_realloc(x,y)      erealloc(x,y)
#define bmake_strdup(x)         estrdup(x)
#define bmake_strldup(x,y)      estrndup(x,y)
#endif
char *bmake_strsedup(const char *, const char *);

/* Thin wrapper around free(3) to avoid the extra function call in case
 * p is NULL, which on x86_64 costs about 12 machine instructions.
 * Other platforms are similarly affected.
 *
 * The case of a NULL pointer happens especially often after Var_Value,
 * since only environment variables need to be freed, but not others. */
static inline void MAKE_ATTR_UNUSED
bmake_free(void *p)
{
    if (p != NULL)
	free(p);
}
