/* $Header: /src/pub/tcsh/tc.str.c,v 3.9 1996/04/26 19:21:42 christos Exp $ */
/*
 * tc.str.c: Short string package
 * 	     This has been a lesson of how to write buggy code!
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "sh.h"

RCSID("$Id: tc.str.c,v 3.9 1996/04/26 19:21:42 christos Exp $")

#define MALLOC_INCR	128

#ifdef SHORT_STRINGS
Char  **
blk2short(src)
    register char **src;
{
    size_t     n;
    register Char **sdst, **dst;

    /*
     * Count
     */
    for (n = 0; src[n] != NULL; n++)
	continue;
    sdst = dst = (Char **) xmalloc((size_t) ((n + 1) * sizeof(Char *)));

    for (; *src != NULL; src++)
	*dst++ = SAVE(*src);
    *dst = NULL;
    return (sdst);
}

char  **
short2blk(src)
    register Char **src;
{
    size_t     n;
    register char **sdst, **dst;

    /*
     * Count
     */
    for (n = 0; src[n] != NULL; n++)
	continue;
    sdst = dst = (char **) xmalloc((size_t) ((n + 1) * sizeof(char *)));

    for (; *src != NULL; src++)
	*dst++ = strsave(short2str(*src));
    *dst = NULL;
    return (sdst);
}

Char   *
str2short(src)
    register const char *src;
{
    static Char *sdst;
    static size_t dstsize = 0;
    register Char *dst, *edst;

    if (src == NULL)
	return (NULL);

    if (sdst == (NULL)) {
	dstsize = MALLOC_INCR;
	sdst = (Char *) xmalloc((size_t) (dstsize * sizeof(Char)));
    }

    dst = sdst;
    edst = &dst[dstsize];
    while ((unsigned char) *src) {
	*dst++ = (Char) ((unsigned char) *src++);
	if (dst == edst) {
	    dstsize += MALLOC_INCR;
	    sdst = (Char *) xrealloc((ptr_t) sdst,
				     (size_t) (dstsize * sizeof(Char)));
	    edst = &sdst[dstsize];
	    dst = &edst[-MALLOC_INCR];
	}
    }
    *dst = 0;
    return (sdst);
}

char   *
short2str(src)
    register const Char *src;
{
    static char *sdst = NULL;
    static size_t dstsize = 0;
    register char *dst, *edst;

    if (src == NULL)
	return (NULL);

    if (sdst == NULL) {
	dstsize = MALLOC_INCR;
	sdst = (char *) xmalloc((size_t) (dstsize * sizeof(char)));
    }
    dst = sdst;
    edst = &dst[dstsize];
    while (*src) {
	*dst++ = (char) *src++;
	if (dst == edst) {
	    dstsize += MALLOC_INCR;
	    sdst = (char *) xrealloc((ptr_t) sdst,
				     (size_t) (dstsize * sizeof(char)));
	    edst = &sdst[dstsize];
	    dst = &edst[-MALLOC_INCR];
	}
    }
    *dst = 0;
    return (sdst);
}

Char   *
s_strcpy(dst, src)
    register Char *dst;
    register const Char *src;
{
    register Char *sdst;

    sdst = dst;
    while ((*dst++ = *src++) != '\0')
	continue;
    return (sdst);
}

Char   *
s_strncpy(dst, src, n)
    register Char *dst;
    register const Char *src;
    register size_t n;
{
    register Char *sdst;

    if (n == 0)
	return(dst);

    sdst = dst;
    do 
	if ((*dst++ = *src++) == '\0') {
	    while (--n != 0)
		*dst++ = '\0';
	    return(sdst);
	}
    while (--n != 0);
    return (sdst);
}

Char   *
s_strcat(dst, src)
    register Char *dst;
    register const Char *src;
{
    register short *sdst;

    sdst = dst;
    while (*dst++)
	continue;
    --dst;
    while ((*dst++ = *src++) != '\0')
	continue;
    return (sdst);
}

#ifdef NOTUSED
Char   *
s_strncat(dst, src, n)
    register Char *dst;
    register const Char *src;
    register size_t n;
{
    register Char *sdst;

    if (n == 0) 
	return (dst);

    sdst = dst;

    while (*dst++)
	continue;
    --dst;

    do 
	if ((*dst++ = *src++) == '\0')
	    return(sdst);
    while (--n != 0)
	continue;

    *dst = '\0';
    return (sdst);
}

#endif

Char   *
s_strchr(str, ch)
    register const Char *str;
    int ch;
{
    do
	if (*str == ch)
	    return ((Char *) str);
    while (*str++);
    return (NULL);
}

Char   *
s_strrchr(str, ch)
    register const Char *str;
    int ch;
{
    register const Char *rstr;

    rstr = NULL;
    do
	if (*str == ch)
	    rstr = str;
    while (*str++);
    return ((Char *) rstr);
}

size_t
s_strlen(str)
    register const Char *str;
{
    register size_t n;

    for (n = 0; *str++; n++)
	continue;
    return (n);
}

int
s_strcmp(str1, str2)
    register const Char *str1, *str2;
{
    for (; *str1 && *str1 == *str2; str1++, str2++)
	continue;
    /*
     * The following case analysis is necessary so that characters which look
     * negative collate low against normal characters but high against the
     * end-of-string NUL.
     */
    if (*str1 == '\0' && *str2 == '\0')
	return (0);
    else if (*str1 == '\0')
	return (-1);
    else if (*str2 == '\0')
	return (1);
    else
	return (*str1 - *str2);
}

int
s_strncmp(str1, str2, n)
    register const Char *str1, *str2;
    register size_t n;
{
    if (n == 0)
	return (0);
    do {
	if (*str1 != *str2) {
	    /*
	     * The following case analysis is necessary so that characters 
	     * which look negative collate low against normal characters
	     * but high against the end-of-string NUL.
	     */
	    if (*str1 == '\0')
		return (-1);
	    else if (*str2 == '\0')
		return (1);
	    else
		return (*str1 - *str2);
	}
        if (*str1 == '\0')
	    return(0);
	str1++, str2++;
    } while (--n != 0);
    return(0);
}

Char   *
s_strsave(s)
    register const Char *s;
{
    Char   *n;
    register Char *p;

    if (s == 0)
	s = STRNULL;
    for (p = (Char *) s; *p++;)
	continue;
    n = p = (Char *) xmalloc((size_t) 
			     ((((const Char *) p) - s) * sizeof(Char)));
    while ((*p++ = *s++) != '\0')
	continue;
    return (n);
}

Char   *
s_strspl(cp, dp)
    const Char   *cp, *dp;
{
    Char   *ep;
    register Char *p, *q;

    if (!cp)
	cp = STRNULL;
    if (!dp)
	dp = STRNULL;
    for (p = (Char *) cp; *p++;)
	continue;
    for (q = (Char *) dp; *q++;)
	continue;
    ep = (Char *) xmalloc((size_t)
			  (((((const Char *) p) - cp) + 
			    (((const Char *) q) - dp) - 1) * sizeof(Char)));
    for (p = ep, q = (Char*) cp; (*p++ = *q++) != '\0';)
	continue;
    for (p--, q = (Char *) dp; (*p++ = *q++) != '\0';)
	continue;
    return (ep);
}

Char   *
s_strend(cp)
    register const Char *cp;
{
    if (!cp)
	return ((Char *) cp);
    while (*cp)
	cp++;
    return ((Char *) cp);
}

Char   *
s_strstr(s, t)
    register const Char *s, *t;
{
    do {
	register const Char *ss = s;
	register const Char *tt = t;

	do
	    if (*tt == '\0')
		return ((Char *) s);
	while (*ss++ == *tt++);
    } while (*s++ != '\0');
    return (NULL);
}

#endif				/* SHORT_STRINGS */

char   *
short2qstr(src)
    register const Char *src;
{
    static char *sdst = NULL;
    static size_t dstsize = 0;
    register char *dst, *edst;

    if (src == NULL)
	return (NULL);

    if (sdst == NULL) {
	dstsize = MALLOC_INCR;
	sdst = (char *) xmalloc((size_t) (dstsize * sizeof(char)));
    }
    dst = sdst;
    edst = &dst[dstsize];
    while (*src) {
	if (*src & QUOTE) {
	    *dst++ = '\\';
	    if (dst == edst) {
		dstsize += MALLOC_INCR;
		sdst = (char *) xrealloc((ptr_t) sdst,
					 (size_t) (dstsize * sizeof(char)));
		edst = &sdst[dstsize];
		dst = &edst[-MALLOC_INCR];
	    }
	}
	*dst++ = (char) *src++;
	if (dst == edst) {
	    dstsize += MALLOC_INCR;
	    sdst = (char *) xrealloc((ptr_t) sdst,
				     (size_t) (dstsize * sizeof(char)));
	    edst = &sdst[dstsize];
	    dst = &edst[-MALLOC_INCR];
	}
    }
    *dst = 0;
    return (sdst);
}
