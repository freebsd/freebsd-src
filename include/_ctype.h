/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
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
 *
 *	@(#)ctype.h	8.4 (Berkeley) 1/21/94
 *      $FreeBSD$
 */

#ifndef _CTYPE_H_
#define	_CTYPE_H_

/*
 * XXX <runetype.h> brings massive namespace pollution (rune_t and struct
 * member names).
 */
#include <runetype.h>

#define	_CTYPE_A	0x00000100L		/* Alpha */
#define	_CTYPE_C	0x00000200L		/* Control */
#define	_CTYPE_D	0x00000400L		/* Digit */
#define	_CTYPE_G	0x00000800L		/* Graph */
#define	_CTYPE_L	0x00001000L		/* Lower */
#define	_CTYPE_P	0x00002000L		/* Punct */
#define	_CTYPE_S	0x00004000L		/* Space */
#define	_CTYPE_U	0x00008000L		/* Upper */
#define	_CTYPE_X	0x00010000L		/* X digit */
#define	_CTYPE_B	0x00020000L		/* Blank */
#define	_CTYPE_R	0x00040000L		/* Print */
#define	_CTYPE_I	0x00080000L		/* Ideogram */
#define	_CTYPE_T	0x00100000L		/* Special */
#define	_CTYPE_Q	0x00200000L		/* Phonogram */

__BEGIN_DECLS
int	isalnum(int);
int	isalpha(int);
int	iscntrl(int);
int	isdigit(int);
int	isgraph(int);
int	islower(int);
int	isprint(int);
int	ispunct(int);
int	isspace(int);
int	isupper(int);
int	isxdigit(int);
int	tolower(int);
int	toupper(int);

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
int	digittoint(int);
int	isascii(int);
int	isblank(int);
int	ishexnumber(int);
int	isideogram(int);
int	isnumber(int);
int	isphonogram(int);
int	isrune(int);
int	isspecial(int);
int	toascii(int);
#endif
__END_DECLS

#define	isalnum(c)	__istype((c), _CTYPE_A|_CTYPE_D)
#define	isalpha(c)	__istype((c), _CTYPE_A)
#define	iscntrl(c)	__istype((c), _CTYPE_C)
#define	isdigit(c)	__isctype((c), _CTYPE_D) /* ANSI -- locale independent */
#define	isgraph(c)	__istype((c), _CTYPE_G)
#define	islower(c)	__istype((c), _CTYPE_L)
#define	isprint(c)	__istype((c), _CTYPE_R)
#define	ispunct(c)	__istype((c), _CTYPE_P)
#define	isspace(c)	__istype((c), _CTYPE_S)
#define	isupper(c)	__istype((c), _CTYPE_U)
#define	isxdigit(c)	__isctype((c), _CTYPE_X) /* ANSI -- locale independent */
#define	tolower(c)	__tolower(c)
#define	toupper(c)	__toupper(c)

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
#define	digittoint(c)	__maskrune((c), 0xFF)
#define	isascii(c)	(((c) & ~0x7F) == 0)
#define	isblank(c)	__istype((c), _CTYPE_B)
#define	ishexnumber(c)	__istype((c), _CTYPE_X)
#define	isideogram(c)	__istype((c), _CTYPE_I)
#define	isnumber(c)	__istype((c), _CTYPE_D)
#define	isphonogram(c)	__istype((c), _CTYPE_Q)
#define	isrune(c)	__istype((c), 0xFFFFFF00L)
#define	isspecial(c)	__istype((c), _CTYPE_T)
#define	toascii(c)	((c) & 0x7F)
#endif

/* See comments in <machine/ansi.h> about _BSD_CT_RUNE_T_. */
__BEGIN_DECLS
unsigned long	___runetype(_BSD_CT_RUNE_T_);
_BSD_CT_RUNE_T_	___tolower(_BSD_CT_RUNE_T_);
_BSD_CT_RUNE_T_	___toupper(_BSD_CT_RUNE_T_);
__END_DECLS

/*
 * _EXTERNALIZE_CTYPE_INLINES_ is defined in locale/nomacros.c to tell us
 * to generate code for extern versions of all our inline functions.
 */
#ifdef _EXTERNALIZE_CTYPE_INLINES_
#define	_USE_CTYPE_INLINE_
#define	static
#define	__inline
#endif

/*
 * Use inline functions if we are allowed to and the compiler supports them.
 */
#if !defined(_DONT_USE_CTYPE_INLINE_) && \
    (defined(_USE_CTYPE_INLINE_) || defined(__GNUC__) || defined(__cplusplus))
static __inline int
__maskrune(_BSD_CT_RUNE_T_ _c, unsigned long _f)
{
	return ((_c < 0 || _c >= _CACHED_RUNES) ? ___runetype(_c) :
		_CurrentRuneLocale->runetype[_c]) & _f;
}

static __inline int
__istype(_BSD_CT_RUNE_T_ _c, unsigned long _f)
{
	return (!!__maskrune(_c, _f));
}

static __inline int
__isctype(_BSD_CT_RUNE_T_ _c, unsigned long _f)
{
	return (_c < 0 || _c >= _CACHED_RUNES) ? 0 :
	       !!(_DefaultRuneLocale.runetype[_c] & _f);
}

static __inline _BSD_CT_RUNE_T_
__toupper(_BSD_CT_RUNE_T_ _c)
{
	return (_c < 0 || _c >= _CACHED_RUNES) ? ___toupper(_c) :
	       _CurrentRuneLocale->mapupper[_c];
}

static __inline _BSD_CT_RUNE_T_
__tolower(_BSD_CT_RUNE_T_ _c)
{
	return (_c < 0 || _c >= _CACHED_RUNES) ? ___tolower(_c) :
	       _CurrentRuneLocale->maplower[_c];
}

#else /* not using inlines */

__BEGIN_DECLS
int		__maskrune(_BSD_CT_RUNE_T_, unsigned long);
int		__istype(_BSD_CT_RUNE_T_, unsigned long);
int		__isctype(_BSD_CT_RUNE_T_, unsigned long);
_BSD_CT_RUNE_T_	__toupper(_BSD_CT_RUNE_T_);
_BSD_CT_RUNE_T_	__tolower(_BSD_CT_RUNE_T_);
__END_DECLS
#endif /* using inlines */

#endif /* !_CTYPE_H_ */
