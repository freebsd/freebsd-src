/*-
 * Copyright (c) 1980, 1991, 1993
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
 *	@(#)char.h	8.1 (Berkeley) 5/31/93
 *	$FreeBSD$
 */

#include <ctype.h>

extern unsigned short _cmap[];

#ifndef NLS
extern unsigned char _cmap_lower[], _cmap_upper[];

#endif

#define	_QF	0x0001		/* '" (Forward quotes) */
#define	_QB	0x0002		/* ` (Backquote) */
#define	_SP	0x0004		/* space and tab */
#define	_NL	0x0008		/* \n */
#define	_META	0x0010		/* lex meta characters, sp #'`";&<>()|\t\n */
#define	_GLOB	0x0020		/* glob characters, *?{[` */
#define	_ESC	0x0040		/* \ */
#define	_DOL	0x0080		/* $ */
#define	_DIG  	0x0100		/* 0-9 */
#define	_LET  	0x0200		/* a-z, A-Z, _ */
#define	_UP   	0x0400		/* A-Z */
#define	_LOW  	0x0800		/* a-z */
#define	_XD 	0x1000		/* 0-9, a-f, A-F */
#define	_CMD	0x2000		/* lex end of command chars, ;&(|` */
#define _CTR	0x4000		/* control */

#define cmap(c, bits)	\
	(((c) & QUOTE) ? 0 : (_cmap[(unsigned char)(c)] & (bits)))

#define isglob(c)	cmap(c, _GLOB)
#define isspc(c)	cmap(c, _SP)
#define ismeta(c)	cmap(c, _META)
#define iscmdmeta(c)	cmap(c, _CMD)
#define letter(c)	(((c) & QUOTE) ? 0 : \
			 (isalpha((unsigned char) (c)) || (c) == '_'))
#define alnum(c)	(((c) & QUOTE) ? 0 : \
		         (isalnum((unsigned char) (c)) || (c) == '_'))
#ifdef NLS
#define Isspace(c)	(((c) & QUOTE) ? 0 : isspace((unsigned char) (c)))
#define Isdigit(c)	(((c) & QUOTE) ? 0 : isdigit((unsigned char) (c)))
#define Isalpha(c)	(((c) & QUOTE) ? 0 : isalpha((unsigned char) (c)))
#define Islower(c)	(((c) & QUOTE) ? 0 : islower((unsigned char) (c)))
#define Isupper(c)	(((c) & QUOTE) ? 0 : isupper((unsigned char) (c)))
#define Tolower(c) 	(((c) & QUOTE) ? 0 : tolower((unsigned char) (c)))
#define Toupper(c) 	(((c) & QUOTE) ? 0 : toupper((unsigned char) (c)))
#define Isxdigit(c)	(((c) & QUOTE) ? 0 : isxdigit((unsigned char) (c)))
#define Isalnum(c)	(((c) & QUOTE) ? 0 : isalnum((unsigned char) (c)))
#define Iscntrl(c) 	(((c) & QUOTE) ? 0 : iscntrl((unsigned char) (c)))
#define Isprint(c) 	(((c) & QUOTE) ? 0 : isprint((unsigned char) (c)))
#else
#define Isspace(c)	cmap(c, _SP|_NL)
#define Isdigit(c)	cmap(c, _DIG)
#define Isalpha(c)	(cmap(c,_LET) && !(((c) & META) && AsciiOnly))
#define Islower(c)	(cmap(c,_LOW) && !(((c) & META) && AsciiOnly))
#define Isupper(c)	(cmap(c, _UP) && !(((c) & META) && AsciiOnly))
#define Tolower(c)  (_cmap_lower[(unsigned char)(c)])
#define Toupper(c)  (_cmap_upper[(unsigned char)(c)])
#define Isxdigit(c)	cmap(c, _XD)
#define Isalnum(c)	(cmap(c, _DIG|_LET) && !(((c) & META) && AsciiOnly))
#define Iscntrl(c)  (cmap(c,_CTR) && !(((c) & META) && AsciiOnly))
#define Isprint(c)  (!cmap(c,_CTR) && !(((c) & META) && AsciiOnly))
#endif
