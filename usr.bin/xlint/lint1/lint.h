/*	$NetBSD: lint.h,v 1.2 1995/07/03 21:24:18 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
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
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>

#include "param.h"

/*
 * Type specifiers, used in type structures (type_t) and otherwere.
 */
typedef enum {
	NOTSPEC,
	SIGNED,		/* keyword "signed", only used in the parser */
	UNSIGN,		/* keyword "unsigned", only used in the parser */
	CHAR,		/* char */
	SCHAR,		/* signed char */
	UCHAR,		/* unsigned char */
	SHORT,		/* (signed) short */
	USHORT,		/* unsigned short */
	INT,		/* (signed) int */
	UINT,		/* unsigned int */
	LONG,		/* (signed) long */
	ULONG,		/* unsigned long */
	QUAD,		/* (signed) long long */
	UQUAD,		/* unsigned long long */
	FLOAT,		/* float */
	DOUBLE,		/* double or, with tflag, long float */
	LDOUBLE,	/* long double */
	VOID,		/* void */
	STRUCT,		/* structure tag */
	UNION,		/* union tag */
	ENUM,		/* enum tag */
	PTR,		/* pointer */
	ARRAY,		/* array */
	FUNC		/* function */
#define	NTSPEC		((int)FUNC + 1)
} tspec_t;

/*
 * size of types, name and classification
 */
typedef	struct {
	int	tt_sz;			/* size in bits */
	int	tt_psz;			/* size, different from tt_sz
					   if pflag is set */
	tspec_t	tt_styp;		/* signed counterpart */
	tspec_t	tt_utyp;		/* unsigned counterpart */
	u_int	tt_isityp : 1;		/* 1 if integer type */
	u_int	tt_isutyp : 1;		/* 1 if unsigned integer type */
	u_int	tt_isftyp : 1;		/* 1 if floating point type */
	u_int	tt_isatyp : 1;		/* 1 if arithmetic type */
	u_int	tt_issclt : 1;		/* 1 if scalar type */
	char	*tt_name;		/* Bezeichnung des Typs */
} ttab_t;

#define size(t)		(ttab[t].tt_sz)
#define psize(t)	(ttab[t].tt_psz)
#define styp(t)		(ttab[t].tt_styp)
#define utyp(t)		(ttab[t].tt_utyp)
#define isityp(t)	(ttab[t].tt_isityp)
#define isutyp(t)	(ttab[t].tt_isutyp)
#define isftyp(t)	(ttab[t].tt_isftyp)
#define isatyp(t)	(ttab[t].tt_isatyp)
#define issclt(t)	(ttab[t].tt_issclt)

extern	ttab_t	ttab[];


typedef	enum {
	NODECL,			/* until now not declared */
	DECL,			/* declared */
	TDEF,			/* tentative defined */
	DEF			/* defined */
} def_t;

/*
 * Following structure contains some data used for the output buffer.
 */
typedef	struct	ob {
	char	*o_buf;		/* buffer */
	char	*o_end;		/* first byte after buffer */
	size_t	o_len;		/* length of buffer */
	char	*o_nxt;		/* next free byte in buffer */
} ob_t;

#include "externs.h"
