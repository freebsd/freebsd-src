/*	$NetBSD: vis.c,v 1.4 2003/08/07 09:15:32 agc Exp $	*/

/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
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


#if 1
#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: vis.c 21005 2007-06-08 01:54:35Z lha $");
#endif
#include "roken.h"
#ifndef _DIAGASSERT
#define _DIAGASSERT(X)
#endif
#else
#include <sys/cdefs.h>
#if !defined(lint)
__RCSID("$NetBSD: vis.c,v 1.4 2003/08/07 09:15:32 agc Exp $");
#endif /* not lint */
#endif

#if 0
#include "namespace.h"
#endif
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <vis.h>

#if 0
#ifdef __weak_alias
__weak_alias(strsvis,_strsvis)
__weak_alias(strsvisx,_strsvisx)
__weak_alias(strvis,_strvis)
__weak_alias(strvisx,_strvisx)
__weak_alias(svis,_svis)
__weak_alias(vis,_vis)
#endif
#endif

#undef BELL
#if defined(__STDC__)
#define BELL '\a'
#else
#define BELL '\007'
#endif

char ROKEN_LIB_FUNCTION
	*rk_vis (char *, int, int, int);
char ROKEN_LIB_FUNCTION
	*rk_svis (char *, int, int, int, const char *);
int ROKEN_LIB_FUNCTION
	rk_strvis (char *, const char *, int);
int ROKEN_LIB_FUNCTION
	rk_strsvis (char *, const char *, int, const char *);
int ROKEN_LIB_FUNCTION
	rk_strvisx (char *, const char *, size_t, int);
int ROKEN_LIB_FUNCTION
	rk_strsvisx (char *, const char *, size_t, int, const char *);


#define isoctal(c)	(((u_char)(c)) >= '0' && ((u_char)(c)) <= '7')
#define iswhite(c)	(c == ' ' || c == '\t' || c == '\n')
#define issafe(c)	(c == '\b' || c == BELL || c == '\r')

#define MAXEXTRAS       5


#define MAKEEXTRALIST(flag, extra)					      \
do {									      \
	char *pextra = extra;						      \
	if (flag & VIS_SP) *pextra++ = ' ';				      \
	if (flag & VIS_TAB) *pextra++ = '\t';				      \
	if (flag & VIS_NL) *pextra++ = '\n';				      \
	if ((flag & VIS_NOSLASH) == 0) *pextra++ = '\\';		      \
	*pextra = '\0';							      \
} while (/*CONSTCOND*/0)

/*
 * This is SVIS, the central macro of vis.
 * dst:	      Pointer to the destination buffer
 * c:	      Character to encode
 * flag:      Flag word
 * nextc:     The character following 'c'
 * extra:     Pointer to the list of extra characters to be
 *	      backslash-protected.
 */
#define SVIS(dst, c, flag, nextc, extra)				   \
do {									   \
	int isextra, isc;						   \
	isextra = strchr(extra, c) != NULL;				   \
	if (!isextra &&							   \
	    isascii((unsigned char)c) &&				   \
	    (isgraph((unsigned char)c) || iswhite(c) ||			   \
	    ((flag & VIS_SAFE) && issafe(c)))) {			   \
		*dst++ = c;						   \
		break;							   \
	}								   \
	isc = 0;							   \
	if (flag & VIS_CSTYLE) {					   \
		switch (c) {						   \
		case '\n':						   \
			isc = 1; *dst++ = '\\'; *dst++ = 'n';		   \
			break;						   \
		case '\r':						   \
			isc = 1; *dst++ = '\\'; *dst++ = 'r';		   \
			break;						   \
		case '\b':						   \
			isc = 1; *dst++ = '\\'; *dst++ = 'b';		   \
			break;						   \
		case BELL:						   \
			isc = 1; *dst++ = '\\'; *dst++ = 'a';		   \
			break;						   \
		case '\v':						   \
			isc = 1; *dst++ = '\\'; *dst++ = 'v';		   \
			break;						   \
		case '\t':						   \
			isc = 1; *dst++ = '\\'; *dst++ = 't';		   \
			break;						   \
		case '\f':						   \
			isc = 1; *dst++ = '\\'; *dst++ = 'f';		   \
			break;						   \
		case ' ':						   \
			isc = 1; *dst++ = '\\'; *dst++ = 's';		   \
			break;						   \
		case '\0':						   \
			isc = 1; *dst++ = '\\'; *dst++ = '0';		   \
			if (isoctal(nextc)) {				   \
				*dst++ = '0';				   \
				*dst++ = '0';				   \
			}						   \
		}							   \
	}								   \
	if (isc) break;							   \
	if (isextra || ((c & 0177) == ' ') || (flag & VIS_OCTAL)) {	   \
		*dst++ = '\\';						   \
		*dst++ = (u_char)(((unsigned)(u_char)c >> 6) & 03) + '0';  \
		*dst++ = (u_char)(((unsigned)(u_char)c >> 3) & 07) + '0';  \
		*dst++ =			     (c	      & 07) + '0'; \
	} else {							   \
		if ((flag & VIS_NOSLASH) == 0) *dst++ = '\\';		   \
		if (c & 0200) {						   \
			c &= 0177; *dst++ = 'M';			   \
		}							   \
		if (iscntrl((unsigned char)c)) {			   \
			*dst++ = '^';					   \
			if (c == 0177)					   \
				*dst++ = '?';				   \
			else						   \
				*dst++ = c + '@';			   \
		} else {						   \
			*dst++ = '-'; *dst++ = c;			   \
		}							   \
	}								   \
} while (/*CONSTCOND*/0)


/*
 * svis - visually encode characters, also encoding the characters
 * 	  pointed to by `extra'
 */

char * ROKEN_LIB_FUNCTION
rk_svis(char *dst, int c, int flag, int nextc, const char *extra)
{
	_DIAGASSERT(dst != NULL);
	_DIAGASSERT(extra != NULL);

	SVIS(dst, c, flag, nextc, extra);
	*dst = '\0';
	return(dst);
}


/*
 * strsvis, strsvisx - visually encode characters from src into dst
 *
 *	Extra is a pointer to a \0-terminated list of characters to
 *	be encoded, too. These functions are useful e. g. to
 *	encode strings in such a way so that they are not interpreted
 *	by a shell.
 *	
 *	Dst must be 4 times the size of src to account for possible
 *	expansion.  The length of dst, not including the trailing NULL,
 *	is returned. 
 *
 *	Strsvisx encodes exactly len bytes from src into dst.
 *	This is useful for encoding a block of data.
 */

int ROKEN_LIB_FUNCTION
rk_strsvis(char *dst, const char *src, int flag, const char *extra)
{
	char c;
	char *start;

	_DIAGASSERT(dst != NULL);
	_DIAGASSERT(src != NULL);
	_DIAGASSERT(extra != NULL);

	for (start = dst; (c = *src++) != '\0'; /* empty */)
	    SVIS(dst, c, flag, *src, extra);
	*dst = '\0';
	return (dst - start);
}


int ROKEN_LIB_FUNCTION
rk_strsvisx(char *dst, const char *src, size_t len, int flag, const char *extra)
{
	char c;
	char *start;

	_DIAGASSERT(dst != NULL);
	_DIAGASSERT(src != NULL);
	_DIAGASSERT(extra != NULL);

	for (start = dst; len > 0; len--) {
		c = *src++;
		SVIS(dst, c, flag, len ? *src : '\0', extra);
	}
	*dst = '\0';
	return (dst - start);
}


/*
 * vis - visually encode characters
 */
char * ROKEN_LIB_FUNCTION
rk_vis(char *dst, int c, int flag, int nextc)
{
	char extra[MAXEXTRAS];

	_DIAGASSERT(dst != NULL);

	MAKEEXTRALIST(flag, extra);
	SVIS(dst, c, flag, nextc, extra);
	*dst = '\0';
	return (dst);
}


/*
 * strvis, strvisx - visually encode characters from src into dst
 *	
 *	Dst must be 4 times the size of src to account for possible
 *	expansion.  The length of dst, not including the trailing NULL,
 *	is returned. 
 *
 *	Strvisx encodes exactly len bytes from src into dst.
 *	This is useful for encoding a block of data.
 */

int ROKEN_LIB_FUNCTION
rk_strvis(char *dst, const char *src, int flag)
{
	char extra[MAXEXTRAS];

	MAKEEXTRALIST(flag, extra);
	return (rk_strsvis(dst, src, flag, extra));
}


int ROKEN_LIB_FUNCTION
rk_strvisx(char *dst, const char *src, size_t len, int flag)
{
	char extra[MAXEXTRAS];

	MAKEEXTRALIST(flag, extra);
	return (rk_strsvisx(dst, src, len, flag, extra));
}
