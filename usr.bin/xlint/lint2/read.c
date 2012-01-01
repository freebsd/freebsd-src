/* $NetBSD: read.c,v 1.12 2002/01/21 19:49:52 tv Exp $ */

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All Rights Reserved.
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

#include <sys/cdefs.h>
#if defined(__RCSID) && !defined(lint)
__RCSID("$NetBSD: read.c,v 1.12 2002/01/21 19:49:52 tv Exp $");
#endif
__FBSDID("$FreeBSD$");

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lint2.h"


/* index of current (included) source file */
static	int	srcfile;

/*
 * The array pointed to by inpfns maps the file name indices of input files
 * to the file name indices used in lint2
 */
static	short	*inpfns;
static	size_t	ninpfns;

/*
 * The array pointed to by *fnames maps file name indizes to file names.
 * Indices of type short are used instead of pointers to save memory.
 */
const	char **fnames;
static	size_t	nfnames;

/*
 * Types are shared (to save memory for the types itself) and accessed
 * via indices (to save memory for references to types (indices are short)).
 * To share types, an equal type must be located fast. This is done by a
 * hash table. Access by indices is done via an array of pointers to the
 * types.
 */
typedef struct thtab {
	const	char *th_name;
	u_short	th_idx;
	struct	thtab *th_nxt;
} thtab_t;
static	thtab_t	**thtab;		/* hash table */
type_t	**tlst;				/* array for indexed access */
static	size_t	tlstlen;		/* length of tlst */

static	hte_t **renametab;

/* index of current C source file (as spezified at the command line) */
static	int	csrcfile;


static	void	inperr(void);
static	void	setsrc(const char *);
static	void	setfnid(int, const char *);
static	void	funccall(pos_t *, const char *);
static	void	decldef(pos_t *, const char *);
static	void	usedsym(pos_t *, const char *);
static	u_short	inptype(const char *, const char **);
static	int	gettlen(const char *, const char **);
static	u_short	findtype(const char *, size_t, int);
static	u_short	storetyp(type_t *, const char *, size_t, int);
static	int	thash(const char *, size_t);
static	char	*inpqstrg(const char *, const char **);
static	const	char *inpname(const char *, const char **);
static	int	getfnidx(const char *);

void
readfile(const char *name)
{
	FILE	*inp;
	size_t	len;
	const	char *cp;
	char	*line, *eptr, rt = '\0';
	int	cline, isrc, iline;
	pos_t	pos;

	if (inpfns == NULL)
		if ((inpfns = calloc(ninpfns = 128, sizeof (short))) == NULL)
			nomem();
	if (fnames == NULL)
		if ((fnames = calloc(nfnames = 256, sizeof (char *))) == NULL)
			nomem();
	if (tlstlen == 0)
		if ((tlst = calloc(tlstlen = 256, sizeof (type_t *))) == NULL)
			nomem();
	if (thtab == NULL)
		if ((thtab = calloc(THSHSIZ2, sizeof (thtab_t))) == NULL)
			nomem();

	_inithash(&renametab);

	srcfile = getfnidx(name);

	if ((inp = fopen(name, "r")) == NULL)
		err(1, "cannot open %s", name);

	while ((line = fgetln(inp, &len)) != NULL) {

		if (len == 0 || line[len - 1] != '\n')
			inperr();
		line[len - 1] = '\0';
		cp = line;

		/* line number in csrcfile */
		cline = (int)strtol(cp, &eptr, 10);
		if (cp == eptr) {
		        cline = -1;
		} else {
			cp = eptr;
		}

		/* record type */
		if (*cp != '\0') {
			rt = *cp++;
		} else {
			inperr();
		}

		if (rt == 'S') {
			setsrc(cp);
			continue;
		} else if (rt == 's') {
			setfnid(cline, cp);
			continue;
		}

		/*
		 * Index of (included) source file. If this index is
		 * different from csrcfile, it refers to an included
		 * file.
		 */
		isrc = (int)strtol(cp, &eptr, 10);
		if (cp == eptr)
			inperr();
		cp = eptr;
		isrc = inpfns[isrc];

		/* line number in isrc */
		if (*cp++ != '.')
			inperr();
		iline = (int)strtol(cp, &eptr, 10);
		if (cp == eptr)
			inperr();
		cp = eptr;

		pos.p_src = (u_short)csrcfile;
		pos.p_line = (u_short)cline;
		pos.p_isrc = (u_short)isrc;
		pos.p_iline = (u_short)iline;

		/* process rest of this record */
		switch (rt) {
		case 'c':
			funccall(&pos, cp);
			break;
		case 'd':
			decldef(&pos, cp);
			break;
		case 'u':
			usedsym(&pos, cp);
			break;
		default:
			inperr();
		}

	}

	_destroyhash(renametab);

	if (ferror(inp))
		err(1, "read error on %s", name);

	(void)fclose(inp);
}


static void
inperr(void)
{

	errx(1, "input file error: %s", fnames[srcfile]);
}

/*
 * Set the name of the C source file of the .ln file which is
 * currently read.
 */
static void
setsrc(const char *cp)
{

	csrcfile = getfnidx(cp);
}

/*
 * setfnid() gets as input an index as used in an input file and the
 * associated file name. If necessary, it creates a new lint2 file
 * name index for this file name and creates the mapping of the index
 * as used in the input file to the index used in lint2.
 */
static void
setfnid(int fid, const char *cp)
{

	if (fid == -1)
		inperr();

	if (fid >= ninpfns) {
		if ((inpfns = realloc(inpfns, (ninpfns * 2) * sizeof (short)))
		    == NULL)
			nomem();
		(void)memset(inpfns + ninpfns, 0, ninpfns * sizeof (short));
		ninpfns *= 2;
	}
	/*
	 * Should always be true because indices written in the output
	 * file by lint1 are always the previous index + 1.
	 */
	if (fid >= ninpfns)
		errx(1, "internal error: setfnid()");
	inpfns[fid] = (u_short)getfnidx(cp);
}

/*
 * Process a function call record (c-record).
 */
static void
funccall(pos_t *posp, const char *cp)
{
	arginf_t *ai, **lai;
	char	c, *eptr;
	int	rused, rdisc;
	hte_t	*hte;
	fcall_t	*fcall;
	const char *name;

	fcall = xalloc(sizeof (fcall_t));
	STRUCT_ASSIGN(fcall->f_pos, *posp);

	/* read flags */
	rused = rdisc = 0;
	lai = &fcall->f_args;
	while ((c = *cp) == 'u' || c == 'i' || c == 'd' ||
	       c == 'z' || c == 'p' || c == 'n' || c == 's') {
		cp++;
		switch (c) {
		case 'u':
			if (rused || rdisc)
				inperr();
			rused = 1;
			break;
		case 'i':
			if (rused || rdisc)
				inperr();
			break;
		case 'd':
			if (rused || rdisc)
				inperr();
			rdisc = 1;
			break;
		case 'z':
		case 'p':
		case 'n':
		case 's':
			ai = xalloc(sizeof (arginf_t));
			ai->a_num = (int)strtol(cp, &eptr, 10);
			if (cp == eptr)
				inperr();
			cp = eptr;
			if (c == 'z') {
				ai->a_pcon = ai->a_zero = 1;
			} else if (c == 'p') {
				ai->a_pcon = 1;
			} else if (c == 'n') {
				ai->a_ncon = 1;
			} else {
				ai->a_fmt = 1;
				ai->a_fstrg = inpqstrg(cp, &cp);
			}
			*lai = ai;
			lai = &ai->a_nxt;
			break;
		}
	}
	fcall->f_rused = rused;
	fcall->f_rdisc = rdisc;

	/* read name of function */
	name = inpname(cp, &cp);

	/* first look it up in the renaming table, then in the normal table */
	hte = _hsearch(renametab, name, 0);
	if (hte != NULL)
		hte = hte->h_hte;
	else
		hte = hsearch(name, 1);
	hte->h_used = 1;

	fcall->f_type = inptype(cp, &cp);

	*hte->h_lcall = fcall;
	hte->h_lcall = &fcall->f_nxt;

	if (*cp != '\0')
		inperr();
}

/*
 * Process a declaration or definition (d-record).
 */
static void
decldef(pos_t *posp, const char *cp)
{
	sym_t	*symp, sym;
	char	c, *ep, *pos1;
	int	used, renamed;
	hte_t	*hte, *renamehte = NULL;
	const char *name, *rename;

	(void)memset(&sym, 0, sizeof (sym));
	STRUCT_ASSIGN(sym.s_pos, *posp);
	sym.s_def = NODECL;

	used = 0;

	while ((c = *cp) == 't' || c == 'd' || c == 'e' || c == 'u' ||
	       c == 'r' || c == 'o' || c == 's' || c == 'v' ||
	       c == 'P' || c == 'S') {
		cp++;
		switch (c) {
		case 't':
			if (sym.s_def != NODECL)
				inperr();
			sym.s_def = TDEF;
			break;
		case 'd':
			if (sym.s_def != NODECL)
				inperr();
			sym.s_def = DEF;
			break;
		case 'e':
			if (sym.s_def != NODECL)
				inperr();
			sym.s_def = DECL;
			break;
		case 'u':
			if (used)
				inperr();
			used = 1;
			break;
		case 'r':
			if (sym.s_rval)
				inperr();
			sym.s_rval = 1;
			break;
		case 'o':
			if (sym.s_osdef)
				inperr();
			sym.s_osdef = 1;
			break;
		case 's':
			if (sym.s_static)
				inperr();
			sym.s_static = 1;
			break;
		case 'v':
			if (sym.s_va)
				inperr();
			sym.s_va = 1;
			sym.s_nva = (short)strtol(cp, &ep, 10);
			if (cp == ep)
				inperr();
			cp = ep;
			break;
		case 'P':
			if (sym.s_prfl)
				inperr();
			sym.s_prfl = 1;
			sym.s_nprfl = (short)strtol(cp, &ep, 10);
			if (cp == ep)
				inperr();
			cp = ep;
			break;
		case 'S':
			if (sym.s_scfl)
				inperr();
			sym.s_scfl = 1;
			sym.s_nscfl = (short)strtol(cp, &ep, 10);
			if (cp == ep)
				inperr();
			cp = ep;
			break;
		}
	}

	/* read symbol name, doing renaming if necessary */
	name = inpname(cp, &cp);
	renamed = 0;
	if (*cp == 'r') {
		cp++;
		name = xstrdup(name);
		rename = inpname(cp, &cp);

		/* enter it and see if it's already been renamed */
		renamehte = _hsearch(renametab, name, 1);
		if (renamehte->h_hte == NULL) {
			hte = hsearch(rename, 1);
			renamehte->h_hte = hte;
			renamed = 1;
		} else if (strcmp((hte = renamehte->h_hte)->h_name, rename)) {
			pos1 = xstrdup(mkpos(&renamehte->h_syms->s_pos));
			/* %s renamed multiple times\t%s  ::  %s */
			msg(18, name, pos1, mkpos(&sym.s_pos));
			free(pos1);
		}
		free((char *)name);
	} else {
		/* it might be a previously-done rename */
		hte = _hsearch(renametab, name, 0);
		if (hte != NULL)
			hte = hte->h_hte;
		else
			hte = hsearch(name, 1);
	}
	hte->h_used |= used;
	if (sym.s_def == DEF || sym.s_def == TDEF)
		hte->h_def = 1;

	sym.s_type = inptype(cp, &cp);

	/*
	 * Allocate memory for this symbol only if it was not already
	 * declared or tentatively defined at the same location with
	 * the same type. Works only for symbols with external linkage,
	 * because static symbols, tentatively defined at the same location
	 * but in different translation units are really different symbols.
	 */
	for (symp = hte->h_syms; symp != NULL; symp = symp->s_nxt) {
		if (symp->s_pos.p_isrc == sym.s_pos.p_isrc &&
		    symp->s_pos.p_iline == sym.s_pos.p_iline &&
		    symp->s_type == sym.s_type &&
		    ((symp->s_def == DECL && sym.s_def == DECL) ||
		     (!sflag && symp->s_def == TDEF && sym.s_def == TDEF)) &&
		    !symp->s_static && !sym.s_static) {
			break;
		}
	}

	if (symp == NULL) {
		/* allocsym reserviert keinen Platz fuer s_nva */
		if (sym.s_va || sym.s_prfl || sym.s_scfl) {
			symp = xalloc(sizeof (sym_t));
			STRUCT_ASSIGN(*symp, sym);
		} else {
			symp = xalloc(sizeof (symp->s_s));
			STRUCT_ASSIGN(symp->s_s, sym.s_s);
		}
		*hte->h_lsym = symp;
		hte->h_lsym = &symp->s_nxt;

		/* XXX hack so we can remember where a symbol was renamed */
		if (renamed)
			renamehte->h_syms = symp;
	}

	if (*cp != '\0')
		inperr();
}

/*
 * Read an u-record (emitted by lint1 if a symbol was used).
 */
static void
usedsym(pos_t *posp, const char *cp)
{
	usym_t	*usym;
	hte_t	*hte;
	const char *name;

	usym = xalloc(sizeof (usym_t));
	STRUCT_ASSIGN(usym->u_pos, *posp);

	/* needed as delimiter between two numbers */
	if (*cp++ != 'x')
		inperr();

	name = inpname(cp, &cp);
	hte = _hsearch(renametab, name, 0);
	if (hte != NULL)
		hte = hte->h_hte;
	else
		hte = hsearch(name, 1);
	hte->h_used = 1;

	*hte->h_lusym = usym;
	hte->h_lusym = &usym->u_nxt;
}

/*
 * Read a type and return the index of this type.
 */
static u_short
inptype(const char *cp, const char **epp)
{
	char	c, s, *eptr;
	const	char *ep;
	type_t	*tp;
	int	narg, i, osdef = 0;
	size_t	tlen;
	u_short	tidx;
	int	h;

	/* If we have this type already, return it's index. */
	tlen = gettlen(cp, &ep);
	h = thash(cp, tlen);
	if ((tidx = findtype(cp, tlen, h)) != 0) {
		*epp = ep;
		return (tidx);
	}

	/* No, we must create a new type. */
	tp = xalloc(sizeof (type_t));

	tidx = storetyp(tp, cp, tlen, h);

	c = *cp++;

	while (c == 'c' || c == 'v') {
		if (c == 'c') {
			tp->t_const = 1;
		} else {
			tp->t_volatile = 1;
		}
		c = *cp++;
	}

	if (c == 's' || c == 'u' || c == 'l' || c == 'e') {
		s = c;
		c = *cp++;
	} else {
		s = '\0';
	}

	switch (c) {
	case 'C':
		tp->t_tspec = s == 's' ? SCHAR : (s == 'u' ? UCHAR : CHAR);
		break;
	case 'S':
		tp->t_tspec = s == 'u' ? USHORT : SHORT;
		break;
	case 'I':
		tp->t_tspec = s == 'u' ? UINT : INT;
		break;
	case 'L':
		tp->t_tspec = s == 'u' ? ULONG : LONG;
		break;
	case 'Q':
		tp->t_tspec = s == 'u' ? UQUAD : QUAD;
		break;
	case 'D':
		tp->t_tspec = s == 's' ? FLOAT : (s == 'l' ? LDOUBLE : DOUBLE);
		break;
	case 'V':
		tp->t_tspec = VOID;
		break;
	case 'P':
		tp->t_tspec = PTR;
		break;
	case 'A':
		tp->t_tspec = ARRAY;
		break;
	case 'F':
	case 'f':
		osdef = c == 'f';
		tp->t_tspec = FUNC;
		break;
	case 'T':
		tp->t_tspec = s == 'e' ? ENUM : (s == 's' ? STRUCT : UNION);
		break;
	}

	switch (tp->t_tspec) {
	case ARRAY:
		tp->t_dim = (int)strtol(cp, &eptr, 10);
		cp = eptr;
		tp->t_subt = TP(inptype(cp, &cp));
		break;
	case PTR:
		tp->t_subt = TP(inptype(cp, &cp));
		break;
	case FUNC:
		c = *cp;
		if (isdigit((u_char)c)) {
			if (!osdef)
				tp->t_proto = 1;
			narg = (int)strtol(cp, &eptr, 10);
			cp = eptr;
			if ((tp->t_args = calloc((size_t)(narg + 1),
			    sizeof (type_t *))) == NULL)
				nomem();
			for (i = 0; i < narg; i++) {
				if (i == narg - 1 && *cp == 'E') {
					tp->t_vararg = 1;
					cp++;
				} else {
					tp->t_args[i] = TP(inptype(cp, &cp));
				}
			}
		}
		tp->t_subt = TP(inptype(cp, &cp));
		break;
	case ENUM:
		tp->t_tspec = INT;
		tp->t_isenum = 1;
		/* FALLTHROUGH */
	case STRUCT:
	case UNION:
		switch (*cp++) {
		case '1':
			tp->t_istag = 1;
			tp->t_tag = hsearch(inpname(cp, &cp), 1);
			break;
		case '2':
			tp->t_istynam = 1;
			tp->t_tynam = hsearch(inpname(cp, &cp), 1);
			break;
		case '3':
			tp->t_isuniqpos = 1;
			tp->t_uniqpos.p_line = strtol(cp, &eptr, 10);
			cp = eptr;
			cp++;
			/* xlate to 'global' file name. */
			tp->t_uniqpos.p_file =
			    addoutfile(inpfns[strtol(cp, &eptr, 10)]);
			cp = eptr;
			cp++;
			tp->t_uniqpos.p_uniq = strtol(cp, &eptr, 10);
			cp = eptr;
			break;
		}
		break;
	case LONG:
	case VOID:
	case LDOUBLE:
	case DOUBLE:
	case FLOAT:
	case UQUAD:
	case QUAD:
	case ULONG:
	case UINT:
	case INT:
	case USHORT:
	case SHORT:
	case UCHAR:
	case SCHAR:
	case CHAR:
	case UNSIGN:
	case SIGNED:
	case NOTSPEC:
		break;
	}

	*epp = cp;
	return (tidx);
}

/*
 * Get the length of a type string.
 */
static int
gettlen(const char *cp, const char **epp)
{
	const	char *cp1;
	char	c, s, *eptr;
	tspec_t	t;
	int	narg, i, cm, vm;

	cp1 = cp;

	c = *cp++;

	cm = vm = 0;

	while (c == 'c' || c == 'v') {
		if (c == 'c') {
			if (cm)
				inperr();
			cm = 1;
		} else {
			if (vm)
				inperr();
			vm = 1;
		}
		c = *cp++;
	}

	if (c == 's' || c == 'u' || c == 'l' || c == 'e') {
		s = c;
		c = *cp++;
	} else {
		s = '\0';
	}

	t = NOTSPEC;

	switch (c) {
	case 'C':
		if (s == 's') {
			t = SCHAR;
		} else if (s == 'u') {
			t = UCHAR;
		} else if (s == '\0') {
			t = CHAR;
		}
		break;
	case 'S':
		if (s == 'u') {
			t = USHORT;
		} else if (s == '\0') {
			t = SHORT;
		}
		break;
	case 'I':
		if (s == 'u') {
			t = UINT;
		} else if (s == '\0') {
			t = INT;
		}
		break;
	case 'L':
		if (s == 'u') {
			t = ULONG;
		} else if (s == '\0') {
			t = LONG;
		}
		break;
	case 'Q':
		if (s == 'u') {
			t = UQUAD;
		} else if (s == '\0') {
			t = QUAD;
		}
		break;
	case 'D':
		if (s == 's') {
			t = FLOAT;
		} else if (s == 'l') {
			t = LDOUBLE;
		} else if (s == '\0') {
			t = DOUBLE;
		}
		break;
	case 'V':
		if (s == '\0')
			t = VOID;
		break;
	case 'P':
		if (s == '\0')
			t = PTR;
		break;
	case 'A':
		if (s == '\0')
			t = ARRAY;
		break;
	case 'F':
	case 'f':
		if (s == '\0')
			t = FUNC;
		break;
	case 'T':
		if (s == 'e') {
			t = ENUM;
		} else if (s == 's') {
			t = STRUCT;
		} else if (s == 'u') {
			t = UNION;
		}
		break;
	default:
		inperr();
	}

	if (t == NOTSPEC)
		inperr();

	switch (t) {
	case ARRAY:
		(void)strtol(cp, &eptr, 10);
		if (cp == eptr)
			inperr();
		cp = eptr;
		(void)gettlen(cp, &cp);
		break;
	case PTR:
		(void)gettlen(cp, &cp);
		break;
	case FUNC:
		c = *cp;
		if (isdigit((u_char)c)) {
			narg = (int)strtol(cp, &eptr, 10);
			cp = eptr;
			for (i = 0; i < narg; i++) {
				if (i == narg - 1 && *cp == 'E') {
					cp++;
				} else {
					(void)gettlen(cp, &cp);
				}
			}
		}
		(void)gettlen(cp, &cp);
		break;
	case ENUM:
	case STRUCT:
	case UNION:
		switch (*cp++) {
		case '1':
			(void)inpname(cp, &cp);
			break;
		case '2':
			(void)inpname(cp, &cp);
			break;
		case '3':
			/* unique position: line.file.uniquifier */
			(void)strtol(cp, &eptr, 10);
			if (cp == eptr)
				inperr();
			cp = eptr;
			if (*cp++ != '.')
				inperr();
			(void)strtol(cp, &eptr, 10);
			if (cp == eptr)
				inperr();
			cp = eptr;
			if (*cp++ != '.')
				inperr();
			(void)strtol(cp, &eptr, 10);
			if (cp == eptr)
				inperr();
			cp = eptr;
			break;
		default:
			inperr();
		}
		break;
	case FLOAT:
	case USHORT:
	case SHORT:
	case UCHAR:
	case SCHAR:
	case CHAR:
	case UNSIGN:
	case SIGNED:
	case NOTSPEC:
	case INT:
	case UINT:
	case DOUBLE:
	case LDOUBLE:
	case VOID:
	case ULONG:
	case QUAD:
	case UQUAD:
	case LONG:
		break;
	}

	*epp = cp;
	return (cp - cp1);
}

/*
 * Search a type by it's type string.
 */
static u_short
findtype(const char *cp, size_t len, int h)
{
	thtab_t	*thte;

	for (thte = thtab[h]; thte != NULL; thte = thte->th_nxt) {
		if (strncmp(thte->th_name, cp, len) != 0)
			continue;
		if (thte->th_name[len] == '\0')
			return (thte->th_idx);
	}

	return (0);
}

/*
 * Store a type and it's type string so we can later share this type
 * if we read the same type string from the input file.
 */
static u_short
storetyp(type_t *tp, const char *cp, size_t len, int h)
{
	static	u_int	tidx = 1;	/* 0 is reserved */
	thtab_t	*thte;
	char	*name;

	if (tidx >= USHRT_MAX)
		errx(1, "sorry, too many types");

	if (tidx == tlstlen - 1) {
		if ((tlst = realloc(tlst, (tlstlen * 2) * sizeof (type_t *)))
		    == NULL)
			nomem();
		(void)memset(tlst + tlstlen, 0, tlstlen * sizeof (type_t *));
		tlstlen *= 2;
	}

	tlst[tidx] = tp;

	/* create a hash table entry */
	name = xalloc(len + 1);
	(void)memcpy(name, cp, len);
	name[len] = '\0';

	thte = xalloc(sizeof (thtab_t));
	thte->th_name = name;
	thte->th_idx = tidx;
	thte->th_nxt = thtab[h];
	thtab[h] = thte;

	return ((u_short)tidx++);
}

/*
 * Hash function for types
 */
static int
thash(const char *s, size_t len)
{
	u_int	v;

	v = 0;
	while (len-- != 0) {
		v = (v << sizeof (v)) + (u_char)*s++;
		v ^= v >> (sizeof (v) * CHAR_BIT - sizeof (v));
	}
	return (v % THSHSIZ2);
}

/*
 * Read a string enclosed by "". This string may contain quoted chars.
 */
static char *
inpqstrg(const char *src, const char **epp)
{
	char	*strg, *dst;
	size_t	slen;
	int	c;
	int	v;

	if ((dst = strg = malloc(slen = 32)) == NULL)
		nomem();

	if ((c = *src++) != '"')
		inperr();
	if ((c = *src++) == '\0')
		inperr();

	while (c != '"') {
		if (c == '\\') {
			if ((c = *src++) == '\0')
				inperr();
			switch (c) {
			case 'n':
				c = '\n';
				break;
			case 't':
				c = '\t';
				break;
			case 'v':
				c = '\v';
				break;
			case 'b':
				c = '\b';
				break;
			case 'r':
				c = '\r';
				break;
			case 'f':
				c = '\f';
				break;
			case 'a':
				c = '\a';
				break;
			case '\\':
				c = '\\';
				break;
			case '"':
				c = '"';
				break;
			case '\'':
				c = '\'';
				break;
			case '0': case '1': case '2': case '3':
				v = (c - '0') << 6;
				if ((c = *src++) < '0' || c > '7')
					inperr();
				v |= (c - '0') << 3;
				if ((c = *src++) < '0' || c > '7')
					inperr();
				v |= c - '0';
				c = (u_char)v;
				break;
			default:
				inperr();
			}
		}
		/* keep space for trailing '\0' */
		if (dst - strg == slen - 1) {
			if ((strg = realloc(strg, slen * 2)) == NULL)
				nomem();
			dst = strg + (slen - 1);
			slen *= 2;
		}
		*dst++ = (char)c;
		if ((c = *src++) == '\0')
			inperr();
	}
	*dst = '\0';

	*epp = src;
	return (strg);
}

/*
 * Read the name of a symbol in static memory.
 */
static const char *
inpname(const char *cp, const char **epp)
{
	static	char	*buf;
	static	size_t	blen = 0;
	size_t	len, i;
	char	*eptr, c;

	len = (int)strtol(cp, &eptr, 10);
	if (cp == eptr)
		inperr();
	cp = eptr;
	if (len + 1 > blen)
		if ((buf = realloc(buf, blen = len + 1)) == NULL)
			nomem();
	for (i = 0; i < len; i++) {
		c = *cp++;
		if (!isalnum((unsigned char)c) && c != '_')
			inperr();
		buf[i] = c;
	}
	buf[i] = '\0';

	*epp = cp;
	return (buf);
}

/*
 * Return the index of a file name. If the name cannot be found, create
 * a new entry and return the index of the newly created entry.
 */
static int
getfnidx(const char *fn)
{
	int	i;

	/* 0 ist reserved */
	for (i = 1; fnames[i] != NULL; i++) {
		if (strcmp(fnames[i], fn) == 0)
			break;
	}
	if (fnames[i] != NULL)
		return (i);

	if (i == nfnames - 1) {
		if ((fnames = realloc(fnames, (nfnames * 2) * sizeof (char *)))
		    == NULL)
			nomem();
		(void)memset(fnames + nfnames, 0, nfnames * sizeof (char *));
		nfnames *= 2;
	}

	if ((fnames[i] = strdup(fn)) == NULL)
		nomem();
	return (i);
}

/*
 * Separate symbols with static and external linkage.
 */
void
mkstatic(hte_t *hte)
{
	sym_t	*sym1, **symp, *sym;
	fcall_t	**callp, *call;
	usym_t	**usymp, *usym;
	hte_t	*nhte;
	int	ofnd;

	/* Look for first static definition */
	for (sym1 = hte->h_syms; sym1 != NULL; sym1 = sym1->s_nxt) {
		if (sym1->s_static)
			break;
	}
	if (sym1 == NULL)
		return;

	/* Do nothing if this name is used only in one translation unit. */
	ofnd = 0;
	for (sym = hte->h_syms; sym != NULL && !ofnd; sym = sym->s_nxt) {
		if (sym->s_pos.p_src != sym1->s_pos.p_src)
			ofnd = 1;
	}
	for (call = hte->h_calls; call != NULL && !ofnd; call = call->f_nxt) {
		if (call->f_pos.p_src != sym1->s_pos.p_src)
			ofnd = 1;
	}
	for (usym = hte->h_usyms; usym != NULL && !ofnd; usym = usym->u_nxt) {
		if (usym->u_pos.p_src != sym1->s_pos.p_src)
			ofnd = 1;
	}
	if (!ofnd) {
		hte->h_used = 1;
		/* errors about undef. static symbols are printed in lint1 */
		hte->h_def = 1;
		hte->h_static = 1;
		return;
	}

	/*
	 * Create a new hash table entry
	 *
	 * XXX this entry should be put at the beginning of the list to
	 * avoid to process the same symbol twice.
	 */
	for (nhte = hte; nhte->h_link != NULL; nhte = nhte->h_link)
		continue;
	nhte->h_link = xmalloc(sizeof (hte_t));
	nhte = nhte->h_link;
	nhte->h_name = hte->h_name;
	nhte->h_used = 1;
	nhte->h_def = 1;	/* error in lint1 */
	nhte->h_static = 1;
	nhte->h_syms = NULL;
	nhte->h_lsym = &nhte->h_syms;
	nhte->h_calls = NULL;
	nhte->h_lcall = &nhte->h_calls;
	nhte->h_usyms = NULL;
	nhte->h_lusym = &nhte->h_usyms;
	nhte->h_link = NULL;
	nhte->h_hte = NULL;

	/*
	 * move all symbols used in this translation unit into the new
	 * hash table entry.
	 */
	for (symp = &hte->h_syms; (sym = *symp) != NULL; ) {
		if (sym->s_pos.p_src == sym1->s_pos.p_src) {
			sym->s_static = 1;
			(*symp) = sym->s_nxt;
			if (hte->h_lsym == &sym->s_nxt)
				hte->h_lsym = symp;
			sym->s_nxt = NULL;
			*nhte->h_lsym = sym;
			nhte->h_lsym = &sym->s_nxt;
		} else {
			symp = &sym->s_nxt;
		}
	}
	for (callp = &hte->h_calls; (call = *callp) != NULL; ) {
		if (call->f_pos.p_src == sym1->s_pos.p_src) {
			(*callp) = call->f_nxt;
			if (hte->h_lcall == &call->f_nxt)
				hte->h_lcall = callp;
			call->f_nxt = NULL;
			*nhte->h_lcall = call;
			nhte->h_lcall = &call->f_nxt;
		} else {
			callp = &call->f_nxt;
		}
	}
	for (usymp = &hte->h_usyms; (usym = *usymp) != NULL; ) {
		if (usym->u_pos.p_src == sym1->s_pos.p_src) {
			(*usymp) = usym->u_nxt;
			if (hte->h_lusym == &usym->u_nxt)
				hte->h_lusym = usymp;
			usym->u_nxt = NULL;
			*nhte->h_lusym = usym;
			nhte->h_lusym = &usym->u_nxt;
		} else {
			usymp = &usym->u_nxt;
		}
	}

	/* h_def must be recalculated for old hte */
	hte->h_def = nhte->h_def = 0;
	for (sym = hte->h_syms; sym != NULL; sym = sym->s_nxt) {
		if (sym->s_def == DEF || sym->s_def == TDEF) {
			hte->h_def = 1;
			break;
		}
	}

	mkstatic(hte);
}
