/*	$NetBSD: lint2.h,v 1.2 1995/07/03 21:24:49 cgd Exp $	*/

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

#include "lint.h"

/*
 * Types are described by structures of type type_t.
 */
typedef	struct type {
	tspec_t	t_tspec;	/* type specifier */
	u_int	t_const : 1;	/* constant */
	u_int	t_volatile : 1;	/* volatile */
	u_int	t_vararg : 1;	/* function has variable number of arguments */
	u_int	t_isenum : 1;	/* enum type */
	u_int	t_proto : 1;	/* this is a prototype */
	u_int	t_istag : 1;	/* tag with _t_tag valid */
	u_int	t_istynam : 1;	/* tag with _t_tynam valid */
	union {
		int	_t_dim;		/* if the type is an ARRAY than this
					   is the dimension of the array. */
		struct	hte *_t_tag;	/* hash table entry of tag if
					   t_isenum, STRUCT or UNION */
		struct	hte *_t_tynam;	/* hash table entry of typename if
					   t_isenum, STRUCT or UNION */
		struct	type **_t_args;	/* list of argument types if this
					   is a prototype */
	} t_u;
	struct	type *t_subt;	/* indirected type (array element, pointed to
				   type, type of return value) */
} type_t;

#define	t_dim	t_u._t_dim
#define	t_tag	t_u._t_tag
#define t_tynam	t_u._t_tynam
#define	t_args	t_u._t_args

/*
 * argument information
 *
 * Such a structure is created for each argument of a function call
 * which is an integer constant or a constant string.
 */
typedef	struct arginf {
	int	a_num;		/* # of argument (1..) */
	u_int	a_zero : 1;	/* argument is 0 */
	u_int	a_pcon : 1;	/* msb of argument is not set */
	u_int	a_ncon : 1;	/* msb of argument is set */
	u_int	a_fmt : 1;	/* a_fstrg points to format string */
	char	*a_fstrg;	/* format string */
	struct	arginf *a_nxt;	/* information for next const. argument */
} arginf_t;

/*
 * Keeps information about position in source file.
 */
typedef	struct {
	u_short	p_src;		/* index of name of translation unit
				   (the name which was specified at the
				   command line) */
	u_short	p_line;		/* line number in p_src */
	u_short	p_isrc;		/* index of (included) file */
	u_short p_iline;	/* line number in p_iline */
} pos_t;	

/*
 * Used for definitions and declarations
 *
 * To save memory, variable sized structures are used. If
 * all s_va, s_prfl and s_scfl are not set, the memory allocated
 * for a symbol is only large enough to keep the first member of
 * struct sym, s_s.
 */
typedef	struct sym {
	struct {
		pos_t	s_pos;		/* pos of def./decl. */
#ifndef lint
		u_int	s_def : 3;	/* DECL, TDEF or DEF */
#else
		def_t	s_def;
#endif		
		u_int	s_rval : 1;	/* function has return value */
		u_int	s_osdef : 1;	/* old style function definition */
		u_int	s_static : 1;	/* symbol is static */
		u_int	s_va : 1;	/* check only first s_nva arguments */
		u_int	s_prfl : 1;	/* printflike */
		u_int	s_scfl : 1;	/* scanflike */
		u_short	s_type;		/* type */
		struct	sym *s_nxt;	/* next symbol with same name */
	} s_s;
	short	s_nva;
	short	s_nprfl;
	short	s_nscfl;
} sym_t;

#define s_pos		s_s.s_pos
#define s_rval		s_s.s_rval
#define s_osdef		s_s.s_osdef
#define s_static	s_s.s_static
#define s_def		s_s.s_def
#define s_va		s_s.s_va
#define s_prfl		s_s.s_prfl
#define s_scfl		s_s.s_scfl
#define s_type		s_s.s_type
#define s_nxt		s_s.s_nxt

/*
 * Used to store informations about function calls.
 */
typedef	struct fcall {
	pos_t	f_pos;		/* position of call */
	u_int	f_rused : 1;	/* return value used */
	u_int	f_rdisc : 1;	/* return value discarded (casted to void) */
	u_short	f_type;		/* types of expected return value and args */
	arginf_t *f_args;	/* information about constant arguments */
	struct	fcall *f_nxt;	/* next call of same function */
} fcall_t;

/*
 * Used to store information about usage of symbols other
 * than for function calls.
 */
typedef	struct usym {
	pos_t	u_pos;		/* position */
	struct	usym *u_nxt;	/* next usage */
} usym_t;

/*
 * hash table entry
 */
typedef	struct hte {
	const	char *h_name;	/* name */
	u_int	h_used : 1;	/* symbol is used */
	u_int	h_def : 1;	/* symbol is defined */
	u_int	h_static : 1;	/* static symbol */
	sym_t	*h_syms;	/* declarations and definitions */
	sym_t	**h_lsym;	/* points to s_nxt of last decl./def. */
	fcall_t	*h_calls;	/* function calls */
	fcall_t	**h_lcall;	/* points to f_nxt of last call */
	usym_t	*h_usyms;	/* usage info */
	usym_t	**h_lusym;	/* points to u_nxt of last usage info */
	struct	hte *h_link;	/* next hte with same hash function */
} hte_t;

/* maps type indices into pointers to type structs */
#define TP(idx)		(tlst[idx])

#include "externs2.h"
