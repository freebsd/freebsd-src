/*	$NetBSD: decl.c,v 1.11 1995/10/02 17:34:16 jpo Exp $	*/

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

#ifndef lint
static char rcsid[] = "$NetBSD: decl.c,v 1.11 1995/10/02 17:34:16 jpo Exp $";
#endif

#include <sys/param.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "lint1.h"

const	char *unnamed = "<unnamed>";

/* contains various information and classification on types */
ttab_t	ttab[NTSPEC];

/* shared type structures for arithmtic types and void */
static	type_t	*typetab;

/* value of next enumerator during declaration of enum types */
int	enumval;

/*
 * pointer to top element of a stack which contains informations local
 * to nested declarations
 */
dinfo_t	*dcs;

static	type_t	*tdeferr __P((type_t *, tspec_t));
static	void	settdsym __P((type_t *, sym_t *));
static	tspec_t	mrgtspec __P((tspec_t, tspec_t));
static	void	align __P((int, int));
static	sym_t	*newtag __P((sym_t *, scl_t, int, int));
static	int	eqargs __P((type_t *, type_t *, int *));
static	int	mnoarg __P((type_t *, int *));
static	int	chkosdef __P((sym_t *, sym_t *));
static	int	chkptdecl __P((sym_t *, sym_t *));
static	sym_t	*nsfunc __P((sym_t *, sym_t *));
static	void	osfunc __P((sym_t *, sym_t *));
static	void	ledecl __P((sym_t *));
static	int	chkinit __P((sym_t *));
static	void	chkausg __P((int, sym_t *));
static	void	chkvusg __P((int, sym_t *));
static	void	chklusg __P((sym_t *));
static	void	chktusg __P((sym_t *));
static	void	chkglvar __P((sym_t *));
static	void	glchksz __P((sym_t *));

/*
 * initializes all global vars used in declarations
 */
void
initdecl()
{
	int	i;
	static	struct {
		tspec_t	it_tspec;
		ttab_t	it_ttab;
	} ittab[] = {
		{ SIGNED,   { 0, 0,
			      SIGNED, UNSIGN,
			      0, 0, 0, 0, 0, "signed" } },
		{ UNSIGN,   { 0, 0,
			      SIGNED, UNSIGN,
			      0, 0, 0, 0, 0, "unsigned" } },
		{ CHAR,	    { CHAR_BIT, CHAR_BIT,
			      SCHAR, UCHAR,
			      1, 0, 0, 1, 1, "char" } },
		{ SCHAR,    { CHAR_BIT, CHAR_BIT,
			      SCHAR, UCHAR,
			      1, 0, 0, 1, 1, "signed char" } },
		{ UCHAR,    { CHAR_BIT, CHAR_BIT,
			      SCHAR, UCHAR,
			      1, 1, 0, 1, 1, "unsigned char" } },
		{ SHORT,    { sizeof (short) * CHAR_BIT, 2 * CHAR_BIT,
			      SHORT, USHORT,
			      1, 0, 0, 1, 1, "short" } },
		{ USHORT,   { sizeof (u_short) * CHAR_BIT, 2 * CHAR_BIT,
			      SHORT, USHORT,
			      1, 1, 0, 1, 1, "unsigned short" } },
		{ INT,      { sizeof (int) * CHAR_BIT, 3 * CHAR_BIT,
			      INT, UINT,
			      1, 0, 0, 1, 1, "int" } },
		{ UINT,     { sizeof (u_int) * CHAR_BIT, 3 * CHAR_BIT,
			      INT, UINT,
			      1, 1, 0, 1, 1, "unsigned int" } },
		{ LONG,     { sizeof (long) * CHAR_BIT, 4 * CHAR_BIT,
			      LONG, ULONG,
			      1, 0, 0, 1, 1, "long" } },
		{ ULONG,    { sizeof (u_long) * CHAR_BIT, 4 * CHAR_BIT,
			      LONG, ULONG,
			      1, 1, 0, 1, 1, "unsigned long" } },
		{ QUAD,     { sizeof (quad_t) * CHAR_BIT, 8 * CHAR_BIT,
			      QUAD, UQUAD,
			      1, 0, 0, 1, 1, "long long" } },
		{ UQUAD,    { sizeof (u_quad_t) * CHAR_BIT, 8 * CHAR_BIT,
			      QUAD, UQUAD,
			      1, 1, 0, 1, 1, "unsigned long long" } },
		{ FLOAT,    { sizeof (float) * CHAR_BIT, 4 * CHAR_BIT,
			      FLOAT, FLOAT,
			      0, 0, 1, 1, 1, "float" } },
		{ DOUBLE,   { sizeof (double) * CHAR_BIT, 8 * CHAR_BIT,
			      DOUBLE, DOUBLE,
			      0, 0, 1, 1, 1, "double" } },
		{ LDOUBLE,  { sizeof (ldbl_t) * CHAR_BIT, 10 * CHAR_BIT,
			      LDOUBLE, LDOUBLE,
			      0, 0, 1, 1, 1, "long double" } },
		{ VOID,     { -1, -1,
			      VOID, VOID,
			      0, 0, 0, 0, 0, "void" } },
		{ STRUCT,   { -1, -1,
			      STRUCT, STRUCT,
			      0, 0, 0, 0, 0, "struct" } },
		{ UNION,    { -1, -1,
			      UNION, UNION,
			      0, 0, 0, 0, 0, "union" } },
		{ ENUM,     { sizeof (int) * CHAR_BIT, 3 * CHAR_BIT,
			      ENUM, ENUM,
			      1, 0, 0, 1, 1, "enum" } },
		{ PTR,      { sizeof (void *) * CHAR_BIT, 4 * CHAR_BIT,
			      PTR, PTR,
			      0, 1, 0, 0, 1, "pointer" } },
		{ ARRAY,    { -1, -1,
			      ARRAY, ARRAY,
			      0, 0, 0, 0, 0, "array" } },
		{ FUNC,     { -1, -1,
			      FUNC, FUNC,
			      0, 0, 0, 0, 0, "function" } },
	};

	/* declaration stack */
	dcs = xcalloc(1, sizeof (dinfo_t));
	dcs->d_ctx = EXTERN;
	dcs->d_ldlsym = &dcs->d_dlsyms;

	/* type information and classification */
	for (i = 0; i < sizeof (ittab) / sizeof (ittab[0]); i++)
		STRUCT_ASSIGN(ttab[ittab[i].it_tspec], ittab[i].it_ttab);
	if (!pflag) {
		for (i = 0; i < NTSPEC; i++)
			ttab[i].tt_psz = ttab[i].tt_sz;
	}
	
	/* shared type structures */
	typetab = xcalloc(NTSPEC, sizeof (type_t));
	for (i = 0; i < NTSPEC; i++)
		typetab[i].t_tspec = NOTSPEC;
	typetab[CHAR].t_tspec = CHAR;
	typetab[SCHAR].t_tspec = SCHAR;
	typetab[UCHAR].t_tspec = UCHAR;
	typetab[SHORT].t_tspec = SHORT;
	typetab[USHORT].t_tspec = USHORT;
	typetab[INT].t_tspec = INT;
	typetab[UINT].t_tspec = UINT;
	typetab[LONG].t_tspec = LONG;
	typetab[ULONG].t_tspec = ULONG;
	typetab[QUAD].t_tspec = QUAD;
	typetab[UQUAD].t_tspec = UQUAD;
	typetab[FLOAT].t_tspec = FLOAT;
	typetab[DOUBLE].t_tspec = DOUBLE;
	typetab[LDOUBLE].t_tspec = LDOUBLE;
	typetab[VOID].t_tspec = VOID;
	/*
	 * Next two are not real types. They are only used by the parser
	 * to return keywords "signed" and "unsigned"
	 */
	typetab[SIGNED].t_tspec = SIGNED;
	typetab[UNSIGN].t_tspec = UNSIGN;
}

/*
 * Returns a shared type structure vor arithmetic types and void.
 *
 * It's important do duplicate this structure (using duptyp() or tdupdyp())
 * if it is to be modified (adding qualifiers or anything else).
 */
type_t *
gettyp(t)
	tspec_t	t;
{
	return (&typetab[t]);
}

type_t *
duptyp(tp)
	const	type_t *tp;
{
	type_t	*ntp;

	ntp = getblk(sizeof (type_t));
	STRUCT_ASSIGN(*ntp, *tp);
	return (ntp);
}

/*
 * Use tduptyp() instead of duptyp() inside expressions (if the
 * allocated memory should be freed after the expr).
 */
type_t *
tduptyp(tp)
	const	type_t *tp;
{
	type_t	*ntp;

	ntp = tgetblk(sizeof (type_t));
	STRUCT_ASSIGN(*ntp, *tp);
	return (ntp);
}

/*
 * Returns 1 if the argument is void or an incomplete array,
 * struct, union or enum type.
 */
int
incompl(tp)
	type_t	*tp;
{
	tspec_t	t;

	if ((t = tp->t_tspec) == VOID) {
		return (1);
	} else if (t == ARRAY) {
		return (tp->t_aincompl);
	} else if (t == STRUCT || t == UNION) {
		return (tp->t_str->sincompl);
	} else if (t == ENUM) {
		return (tp->t_enum->eincompl);
	}
	return (0);
}

/*
 * Set the flag for (in)complete array, struct, union or enum
 * types.
 */
void
setcompl(tp, ic)
	type_t	*tp;
	int	ic;
{
	tspec_t	t;

	if ((t = tp->t_tspec) == ARRAY) {
		tp->t_aincompl = ic;
	} else if (t == STRUCT || t == UNION) {
		tp->t_str->sincompl = ic;
	} else {
		if (t != ENUM)
			lerror("setcompl() 1");
		tp->t_enum->eincompl = ic;
	}
}

/*
 * Remember the storage class of the current declaration in dcs->d_scl
 * (the top element of the declaration stack) and detect multiple
 * storage classes.
 */
void
addscl(sc)
	scl_t	sc;
{
	if (sc == INLINE) {
		if (dcs->d_inline)
			/* duplicate '%s' */
			warning(10, "inline");
		dcs->d_inline = 1;
		return;
	}
	if (dcs->d_type != NULL || dcs->d_atyp != NOTSPEC ||
	    dcs->d_smod != NOTSPEC || dcs->d_lmod != NOTSPEC) {
		/* storage class after type is obsolescent */
		warning(83);
	}
	if (dcs->d_scl == NOSCL) {
		dcs->d_scl = sc;
	} else {
		/*
		 * multiple storage classes. An error will be reported in
		 * deftyp().
		 */
		dcs->d_mscl = 1;
	}
}

/*
 * Remember the type, modifier or typedef name returned by the parser
 * in *dcs (top element of decl stack). This information is used in
 * deftyp() to build the type used for all declarators in this
 * declaration.
 *
 * Is tp->t_typedef 1, the type comes from a previously defined typename.
 * Otherwise it comes from a type specifier (int, long, ...) or a
 * struct/union/enum tag.
 */
void
addtype(tp)
	type_t	*tp;
{
	tspec_t	t;

	if (tp->t_typedef) {
		if (dcs->d_type != NULL || dcs->d_atyp != NOTSPEC ||
		    dcs->d_lmod != NOTSPEC || dcs->d_smod != NOTSPEC) {
			/*
			 * something like "typedef int a; int a b;"
			 * This should not happen with current grammar.
			 */
			lerror("addtype()");
		}
		dcs->d_type = tp;
		return;
	}

	t = tp->t_tspec;

	if (t == STRUCT || t == UNION || t == ENUM) {
		/*
		 * something like "int struct a ..."
		 * struct/union/enum with anything else is not allowed
		 */
		if (dcs->d_type != NULL || dcs->d_atyp != NOTSPEC ||
		    dcs->d_lmod != NOTSPEC || dcs->d_smod != NOTSPEC) {
			/*
			 * remember that an error must be reported in
			 * deftyp().
			 */
			dcs->d_terr = 1;
			dcs->d_atyp = dcs->d_lmod = dcs->d_smod = NOTSPEC;
		}
		dcs->d_type = tp;
		return;
	}

	if (dcs->d_type != NULL && !dcs->d_type->t_typedef) {
		/*
		 * something like "struct a int"
		 * struct/union/enum with anything else is not allowed
		 */
		dcs->d_terr = 1;
		return;
	}

	if (t == LONG && dcs->d_lmod == LONG) {
		/* "long long" or "long ... long" */
		t = QUAD;
		dcs->d_lmod = NOTSPEC;
		if (!quadflg)
			/* %s C does not support 'long long' */
			(void)gnuism(265, tflag ? "traditional" : "ANSI");
	}

	if (dcs->d_type != NULL && dcs->d_type->t_typedef) {
		/* something like "typedef int a; a long ..." */
		dcs->d_type = tdeferr(dcs->d_type, t);
		return;
	}

	/* now it can be only a combination of arithmetic types and void */
	if (t == SIGNED || t == UNSIGN) {
		/* remeber specifiers "signed" and "unsigned" in dcs->d_smod */
		if (dcs->d_smod != NOTSPEC)
			/*
			 * more then one "signed" and/or "unsigned"; print
			 * an error in deftyp()
			 */
			dcs->d_terr = 1;
		dcs->d_smod = t;
	} else if (t == SHORT || t == LONG || t == QUAD) {
		/*
		 * remember specifiers "short", "long" and "long long" in
		 * dcs->d_lmod
		 */
		if (dcs->d_lmod != NOTSPEC)
			/* more than one, print error in deftyp() */
			dcs->d_terr = 1;
		dcs->d_lmod = t;
	} else {
		/*
		 * remember specifiers "void", "char", "int", "float" or
		 * "double" int dcs->d_atyp 
		 */
		if (dcs->d_atyp != NOTSPEC)
			/* more than one, print error in deftyp() */
			dcs->d_terr = 1;
		dcs->d_atyp = t;
	}
}

/*
 * called if a list of declaration specifiers contains a typedef name
 * and other specifiers (except struct, union, enum, typedef name)
 */
static type_t *
tdeferr(td, t)
	type_t	*td;
	tspec_t	t;
{
	tspec_t	t2;

	t2 = td->t_tspec;

	switch (t) {
	case SIGNED:
	case UNSIGN:
		if (t2 == CHAR || t2 == SHORT || t2 == INT || t2 == LONG ||
		    t2 == QUAD) {
			if (!tflag)
				/* modifying typedef with ... */
				warning(5, ttab[t].tt_name);
			td = duptyp(gettyp(mrgtspec(t2, t)));
			td->t_typedef = 1;
			return (td);
		}
		break;
	case SHORT:
		if (t2 == INT || t2 == UINT) {
			/* modifying typedef with ... */
			warning(5, "short");
			td = duptyp(gettyp(t2 == INT ? SHORT : USHORT));
			td->t_typedef = 1;
			return (td);
		}
		break;
	case LONG:
		if (t2 == INT || t2 == UINT || t2 == LONG || t2 == ULONG ||
		    t2 == FLOAT || t2 == DOUBLE) {
			/* modifying typedef with ... */
			warning(5, "long");
			if (t2 == INT) {
				td = gettyp(LONG);
			} else if (t2 == UINT) {
				td = gettyp(ULONG);
			} else if (t2 == LONG) {
				td = gettyp(QUAD);
			} else if (t2 == ULONG) {
				td = gettyp(UQUAD);
			} else if (t2 == FLOAT) {
				td = gettyp(DOUBLE);
			} else if (t2 == DOUBLE) {
				td = gettyp(LDOUBLE);
			}
			td = duptyp(td);
			td->t_typedef = 1;
			return (td);
		}
		break;
		/* LINTED (enumeration values not handled in switch) */
	}

	/* Anything other is not accepted. */

	dcs->d_terr = 1;
	return (td);
}

/*
 * Remember the symbol of a typedef name (2nd arg) in a struct, union
 * or enum tag if the typedef name is the first defined for this tag.
 *
 * If the tag is unnamed, the typdef name is used for identification
 * of this tag in lint2. Although its possible that more then one typedef
 * name is defined for one tag, the first name defined should be unique
 * if the tag is unnamed.
 */
static void
settdsym(tp, sym)
	type_t	*tp;
	sym_t	*sym;
{
	tspec_t	t;

	if ((t = tp->t_tspec) == STRUCT || t == UNION) {
		if (tp->t_str->stdef == NULL)
			tp->t_str->stdef = sym;
	} else if (t == ENUM) {
		if (tp->t_enum->etdef == NULL)
			tp->t_enum->etdef = sym;
	}
}

/*
 * Remember a qualifier which is part of the declaration specifiers
 * (and not the declarator) in the top element of the declaration stack.
 * Also detect multiple qualifiers of the same kind.

 * The rememberd qualifier is used by deftyp() to construct the type
 * for all declarators.
 */
void
addqual(q)
	tqual_t	q;
{
	if (q == CONST) {
		if (dcs->d_const) {
			/* duplicate "%s" */
			warning(10, "const");
		}
		dcs->d_const = 1;
	} else {
		if (q != VOLATILE)
			lerror("addqual() 1");
		if (dcs->d_volatile) {
			/* duplicate "%s" */
			warning(10, "volatile");
		}
		dcs->d_volatile = 1;
	}
}

/*
 * Go to the next declaration level (structs, nested structs, blocks,
 * argument declaration lists ...)
 */
void
pushdecl(sc)
	scl_t	sc;
{
	dinfo_t	*di;

	if (dflag)
		(void)printf("pushdecl(%d)\n", (int)sc);

	/* put a new element on the declaration stack */
	di = xcalloc(1, sizeof (dinfo_t));
	di->d_nxt = dcs;
	dcs = di;
	di->d_ctx = sc;
	di->d_ldlsym = &di->d_dlsyms;
}

/*
 * Go back to previous declaration level
 */
void
popdecl()
{
	dinfo_t	*di;

	if (dflag)
		(void)printf("popdecl(%d)\n", (int)dcs->d_ctx);

	if (dcs->d_nxt == NULL)
		lerror("popdecl() 1");
	di = dcs;
	dcs = di->d_nxt;
	switch (di->d_ctx) {
	case EXTERN:
		/* there is nothing after external declarations */
		lerror("popdecl() 2");
		/* NOTREACHED */
	case MOS:
	case MOU:
	case ENUMCON:
		/*
		 * Symbols declared in (nested) structs or enums are
		 * part of the next level (they are removed from the
		 * symbol table if the symbols of the outher level are
		 * removed)
		 */
		if ((*dcs->d_ldlsym = di->d_dlsyms) != NULL)
			dcs->d_ldlsym = di->d_ldlsym;
		break;
	case ARG:
		/*
		 * All symbols in dcs->d_dlsyms are introduced in old style
		 * argument declarations (it's not clean, but possible).
		 * They are appended to the list of symbols declared in
		 * an old style argument identifier list or a new style
		 * parameter type list.
		 */
		if (di->d_dlsyms != NULL) {
			*di->d_ldlsym = dcs->d_fpsyms;
			dcs->d_fpsyms = di->d_dlsyms;
		}
		break;
	case ABSTRACT:
		/*
		 * casts and sizeof
		 * Append all symbols declared in the abstract declaration
		 * to the list of symbols declared in the surounding decl.
		 * or block.
		 * XXX I'm not sure whether they should be removed from the
		 * symbol table now or later.
		 */
		if ((*dcs->d_ldlsym = di->d_dlsyms) != NULL)
			dcs->d_ldlsym = di->d_ldlsym;
		break;
	case AUTO:
		/* check usage of local vars */
		chkusage(di);
		/* FALLTHROUGH */
	case PARG:
		/* usage of arguments will be checked by funcend() */
		rmsyms(di->d_dlsyms);
		break;
	default:
		lerror("popdecl() 3");
	}
	free(di);
}

/*
 * Set flag d_asm in all declaration stack elements up to the
 * outermost one.
 *
 * This is used to mark compound statements which have, possibly in
 * nested compound statements, asm statements. For these compound
 * statements no warnings about unused or unitialized variables are
 * printed.
 *
 * There is no need to clear d_asm in dinfo structs with context AUTO,
 * because these structs are freed at the end of the compound statement.
 * But it must be cleard in the outermost dinfo struct, which has
 * context EXTERN. This could be done in clrtyp() and would work for
 * C, but not for C++ (due to mixed statements and declarations). Thus
 * we clear it in glclup(), which is used to do some cleanup after
 * global declarations/definitions.
 */
void
setasm()
{
	dinfo_t	*di;

	for (di = dcs; di != NULL; di = di->d_nxt)
		di->d_asm = 1;
}

/*
 * Clean all elements of the top element of declaration stack which
 * will be used by the next declaration
 */
void
clrtyp()
{
	dcs->d_atyp = dcs->d_smod = dcs->d_lmod = NOTSPEC;
	dcs->d_scl = NOSCL;
	dcs->d_type = NULL;
	dcs->d_const = dcs->d_volatile = 0;
	dcs->d_inline = 0;
	dcs->d_mscl = dcs->d_terr = 0;
	dcs->d_nedecl = 0;
	dcs->d_notyp = 0;
}

/*
 * Create a type structure from the informations gathered in
 * the declaration stack.
 * Complain about storage classes which are not possible in current
 * context.
 */
void
deftyp()
{
	tspec_t	t, s, l;
	type_t	*tp;
	scl_t	scl;

	t = dcs->d_atyp;		/* CHAR, INT, FLOAT, DOUBLE, VOID */
	s = dcs->d_smod;		/* SIGNED, UNSIGNED */
	l = dcs->d_lmod;		/* SHORT, LONG, QUAD */
	tp = dcs->d_type;
	scl = dcs->d_scl;

	if (t == NOTSPEC && s == NOTSPEC && l == NOTSPEC && tp == NULL)
		dcs->d_notyp = 1;

	if (tp != NULL && (t != NOTSPEC || s != NOTSPEC || l != NOTSPEC)) {
		/* should never happen */
		lerror("deftyp() 1");
	}

	if (tp == NULL) {
		switch (t) {
		case NOTSPEC:
			t = INT;
			/* FALLTHROUGH */
		case INT:
			if (s == NOTSPEC)
				s = SIGNED;
			break;
		case CHAR:
			if (l != NOTSPEC) {
				dcs->d_terr = 1;
				l = NOTSPEC;
			}
			break;
		case FLOAT:
			if (l == LONG) {
				l = NOTSPEC;
				t = DOUBLE;
				if (!tflag)
					/* use 'double' instead of ...  */
					warning(6);
			}
			break;
		case DOUBLE:
			if (l == LONG) {
				l = NOTSPEC;
				t = LDOUBLE;
				if (tflag)
					/* 'long double' is illegal in ... */
					warning(266);
			}
			break;
		case VOID:
			break;
		default:
			lerror("deftyp() 2");
		}
		if (t != INT && t != CHAR && (s != NOTSPEC || l != NOTSPEC)) {
			dcs->d_terr = 1;
			l = s = NOTSPEC;
		}
		if (l != NOTSPEC)
			t = l;
		dcs->d_type = gettyp(mrgtspec(t, s));
	}

	if (dcs->d_mscl) {
		/* only one storage class allowed */
		error(7);
	}
	if (dcs->d_terr) {
		/* illegal type combination */
		error(4);
	}

	if (dcs->d_ctx == EXTERN) {
		if (scl == REG || scl == AUTO) {
			/* illegal storage class */
			error(8);
			scl = NOSCL;
		}
	} else if (dcs->d_ctx == ARG || dcs->d_ctx == PARG) {
		if (scl != NOSCL && scl != REG) {
			/* only "register" valid ... */
			error(9);
			scl = NOSCL;
		}
	}

	dcs->d_scl = scl;

	if (dcs->d_const && dcs->d_type->t_const) {
		if (!dcs->d_type->t_typedef)
			lerror("deftyp() 3");
		/* typedef already qualified with "%s" */
		warning(68, "const");
	}
	if (dcs->d_volatile && dcs->d_type->t_volatile) {
		if (!dcs->d_type->t_typedef)
			lerror("deftyp() 4");
		/* typedef already qualified with "%s" */
		warning(68, "volatile");
	}

	if (dcs->d_const || dcs->d_volatile) {
		dcs->d_type = duptyp(dcs->d_type);
		dcs->d_type->t_const |= dcs->d_const;
		dcs->d_type->t_volatile |= dcs->d_volatile;
	}
}

/*
 * Merge type specifiers (char, ..., long long, signed, unsigned).
 */
static tspec_t
mrgtspec(t, s)
	tspec_t	t, s;
{
	if (s == SIGNED || s == UNSIGN) {
		if (t == CHAR) {
			t = s == SIGNED ? SCHAR : UCHAR;
		} else if (t == SHORT) {
			t = s == SIGNED ? SHORT : USHORT;
		} else if (t == INT) {
			t = s == SIGNED ? INT : UINT;
		} else if (t == LONG) {
			t = s == SIGNED ? LONG : ULONG;
		} else if (t == QUAD) {
			t = s == SIGNED ? QUAD : UQUAD;
		}
	}

	return (t);
}

/*
 * Return the length of a type in bit.
 *
 * Printing a message if the outhermost dimension of an array is 0 must
 * be done by the caller. All other problems are reported by length()
 * if name is not NULL.
 */
int
length(tp, name)
	type_t	*tp;
	const	char *name;
{
	int	elem, elsz;

	elem = 1;
	while (tp->t_tspec == ARRAY) {
		elem *= tp->t_dim;
		tp = tp->t_subt;
	}
	switch (tp->t_tspec) {
	case FUNC:
		/* compiler takes size of function */
		lerror(msgs[12]);
		/* NOTREACHED */
	case STRUCT:
	case UNION:
		if (incompl(tp) && name != NULL) {
			/* incomplete structure or union %s: %s */
			error(31, tp->t_str->stag->s_name, name);
		}
		elsz = tp->t_str->size;
		break;
	case ENUM:
		if (incompl(tp) && name != NULL) {
			/* incomplete enum type: %s */
			warning(13, name);
		}
		/* FALLTHROUGH */
	default:
		elsz = size(tp->t_tspec);
		if (elsz <= 0)
			lerror("length()");
		break;
	}
	return (elem * elsz);
}

/*
 * Get the alignment of the given Type in bits.
 */
int
getbound(tp)
	type_t	*tp;
{
	int	a;
	tspec_t	t;

	while (tp->t_tspec == ARRAY)
		tp = tp->t_subt;

	if ((t = tp->t_tspec) == STRUCT || t == UNION) {
		a = tp->t_str->align;
	} else if (t == FUNC) {
		/* compiler takes alignment of function */
		error(14);
		a = ALIGN(1) * CHAR_BIT;
	} else {
		if ((a = size(t)) == 0) {
			a = CHAR_BIT;
		} else if (a > ALIGN(1) * CHAR_BIT) {
			a = ALIGN(1) * CHAR_BIT;
		}
	}
	if (a < CHAR_BIT || a > ALIGN(1) * CHAR_BIT)
		lerror("getbound() 1");
	return (a);
}

/*
 * Concatenate two lists of symbols by s_nxt. Used by declarations of
 * struct/union/enum elements and parameters.
 */
sym_t *
lnklst(l1, l2)
	sym_t	*l1, *l2;
{
	sym_t	*l;

	if ((l = l1) == NULL)
		return (l2);
	while (l1->s_nxt != NULL)
		l1 = l1->s_nxt;
	l1->s_nxt = l2;
	return (l);
}

/*
 * Check if the type of the given symbol is valid and print an error
 * message if it is not.
 *
 * Invalid types are:
 * - arrays of incomlete types or functions
 * - functions returning arrays or functions
 * - void types other than type of function or pointer
 */
void
chktyp(sym)
	sym_t	*sym;
{
	tspec_t	to, t;
	type_t	**tpp, *tp;

	tpp = &sym->s_type;
	to = NOTSPEC;
	while ((tp = *tpp) != NULL) {
		t = tp->t_tspec;
		/*
		 * If this is the type of an old style function definition,
		 * a better warning is printed in funcdef().
		 */
		if (t == FUNC && !tp->t_proto &&
		    !(to == NOTSPEC && sym->s_osdef)) {
			if (sflag && hflag)
				/* function declaration is not a prototype */
				warning(287);
		}
		if (to == FUNC) {
			if (t == FUNC || t == ARRAY) {
				/* function returns illegal type */
				error(15);
				if (t == FUNC) {
					*tpp = incref(*tpp, PTR);
				} else {
					*tpp = incref((*tpp)->t_subt, PTR);
				}
				return;
			} else if (tp->t_const || tp->t_volatile) {
				if (sflag) {	/* XXX oder better !tflag ? */
					/* function cannot return const... */
					warning(228);
				}
			}
		} if (to == ARRAY) {
			if (t == FUNC) {
				/* array of function is illegal */
				error(16);
				*tpp = gettyp(INT);
				return;
			} else if (t == ARRAY && tp->t_dim == 0) {
				/* null dimension */
				error(17);
				return;
			} else if (t == VOID) {
				/* illegal use of void */
				error(18);
				*tpp = gettyp(INT);
#if 0	/* errors are produced by length() */
			} else if (incompl(tp)) {
				/* array of incomplete type */
				if (sflag) {
					error(301);
				} else {
					warning(301);
				}
#endif
			}
		} else if (to == NOTSPEC && t == VOID) {
			if (dcs->d_ctx == PARG) {
				if (sym->s_scl != ABSTRACT) {
					if (sym->s_name == unnamed)
						lerror("chktyp()");
					/* void param cannot have name: %s */
					error(61, sym->s_name);
					*tpp = gettyp(INT);
				}
			} else if (dcs->d_ctx == ABSTRACT) {
				/* ok */
			} else if (sym->s_scl != TYPEDEF) {
				/* void type for %s */
				error(19, sym->s_name);
				*tpp = gettyp(INT);
			}
		}
		if (t == VOID && to != PTR) {
			if (tp->t_const || tp->t_volatile) {
				/* inappropriate qualifiers with "void" */
				warning(69);
				tp->t_const = tp->t_volatile = 0;
			}
		}
		tpp = &tp->t_subt;
		to = t;
	}
}

/*
 * Process the declarator of a struct/union element.
 */
sym_t *
decl1str(dsym)
	sym_t	*dsym;
{
	type_t	*tp;
	tspec_t	t;
	int	sz, o, len;
	scl_t	sc;

	if ((sc = dsym->s_scl) != MOS && sc != MOU)
		lerror("decl1str() 1");

	if (dcs->d_rdcsym != NULL) {
		if ((sc = dcs->d_rdcsym->s_scl) != MOS && sc != MOU)
			/* should be ensured by storesym() */
			lerror("decl1str() 2");
		if (dsym->s_styp == dcs->d_rdcsym->s_styp) {
			/* duplicate member name: %s */
			error(33, dsym->s_name);
			rmsym(dcs->d_rdcsym);
		}
	}

	chktyp(dsym);

	t = (tp = dsym->s_type)->t_tspec;

	if (dsym->s_field) {
		/*
		 * bit field
		 *
		 * only unsigned und signed int are protable bit-field types
		 *(at least in ANSI C, in traditional C only unsigned int)
		 */
		if (t == CHAR || t == UCHAR || t == SCHAR ||
		    t == SHORT || t == USHORT || t == ENUM) {
			if (sflag) {
				/* bit-field type '%s' invalid in ANSI C */
				warning(273, tyname(tp));
			} else if (pflag) {
				/* nonportable bit-field type */
				warning(34);
			}
		} else if (t == INT && dcs->d_smod == NOTSPEC) {
			if (pflag) {
				/* nonportable bit-field type */
				warning(34);
			}
		} else if (t != INT && t != UINT) {
			/* illegal bit-field type */
			error(35);
			sz = tp->t_flen;
			dsym->s_type = tp = duptyp(gettyp(t = INT));
			if ((tp->t_flen = sz) > size(t))
				tp->t_flen = size(t);
		}
		if ((len = tp->t_flen) < 0 || len > size(t)) {
			/* illegal bit-field size */
			error(36);
			tp->t_flen = size(t);
		} else if (len == 0 && dsym->s_name != unnamed) {
			/* zero size bit-field */
			error(37);
			tp->t_flen = size(t);
		}
		if (dsym->s_scl == MOU) {
			/* illegal use of bit-field */
			error(41);
			dsym->s_type->t_isfield = 0;
			dsym->s_field = 0;
		}
	} else if (t == FUNC) {
		/* function illegal in structure or union */
		error(38);
		dsym->s_type = tp = incref(tp, t = PTR);
	}

	/*
	 * bit-fields of length 0 are not warned about because length()
	 * does not return the length of the bit-field but the length
	 * of the type the bit-field is packed in (its ok)
	 */
	if ((sz = length(dsym->s_type, dsym->s_name)) == 0) {
		if (t == ARRAY && dsym->s_type->t_dim == 0) {
			/* illegal zero sized structure member: %s */
			warning(39, dsym->s_name);
		}
	}

	if (dcs->d_ctx == MOU) {
		o = dcs->d_offset;
		dcs->d_offset = 0;
	}
	if (dsym->s_field) {
		align(getbound(tp), tp->t_flen);
		dsym->s_value.v_quad = (dcs->d_offset / size(t)) * size(t);
		tp->t_foffs = dcs->d_offset - (int)dsym->s_value.v_quad;
		dcs->d_offset += tp->t_flen;
	} else {
		align(getbound(tp), 0);
		dsym->s_value.v_quad = dcs->d_offset;
		dcs->d_offset += sz;
	}
	if (dcs->d_ctx == MOU) {
		if (o > dcs->d_offset)
			dcs->d_offset = o;
	}

	chkfdef(dsym, 0);

	return (dsym);
}

/*
 * Aligns next structure element as required.
 *
 * al contains the required alignment, len the length of a bit-field.
 */
static void
align(al, len)
	int	al, len;
{
	int	no;

	/*
	 * The alignment of the current element becomes the alignment of
	 * the struct/union if it is larger than the current alignment
	 * of the struct/union.
	 */
	if (al > dcs->d_stralign)
		dcs->d_stralign = al;
	
	no = (dcs->d_offset + (al - 1)) & ~(al - 1);
	if (len == 0 || dcs->d_offset + len > no)
		dcs->d_offset = no;
}

/*
 * Remember the width of the field in its type structure.
 */
sym_t *
bitfield(dsym, len)
	sym_t	*dsym;
	int	len;
{
	if (dsym == NULL) {
		dsym = getblk(sizeof (sym_t));
		dsym->s_name = unnamed;
		dsym->s_kind = FMOS;
		dsym->s_scl = MOS;
		dsym->s_type = gettyp(INT);
		dsym->s_blklev = -1;
	}
	dsym->s_type = duptyp(dsym->s_type);
	dsym->s_type->t_isfield = 1;
	dsym->s_type->t_flen = len;
	dsym->s_field = 1;
	return (dsym);
}

/*
 * Collect informations about a sequence of asterisks and qualifiers
 * in a list of type pqinf_t.
 * Qualifiers refer always to the left asterisk. The rightmost asterisk
 * will be at the top of the list.
 */
pqinf_t *
mergepq(p1, p2)
	pqinf_t	*p1, *p2;
{
	pqinf_t	*p;

	if (p2->p_pcnt != 0) {
		/* left '*' at the end of the list */
		for (p = p2; p->p_nxt != NULL; p = p->p_nxt) ;
		p->p_nxt = p1;
		return (p2);
	} else {
		if (p2->p_const) {
			if (p1->p_const) {
				/* duplicate %s */
				warning(10, "const");
			}
			p1->p_const = 1;
		}
		if (p2->p_volatile) {
			if (p1->p_volatile) {
				/* duplicate %s */
				warning(10, "volatile");
			}
			p1->p_volatile = 1;
		}
		free(p2);
		return (p1);
	}
}

/*
 * Followint 3 functions extend the type of a declarator with
 * pointer, function and array types.
 *
 * The current type is the Type built by deftyp() (dcs->d_type) and
 * pointer, function and array types already added for this
 * declarator. The new type extension is inserted between both.
 */
sym_t *
addptr(decl, pi)
	sym_t	*decl;
	pqinf_t	*pi;
{
	type_t	**tpp, *tp;
	pqinf_t	*npi;

	tpp = &decl->s_type;
	while (*tpp != dcs->d_type)
		tpp = &(*tpp)->t_subt;

	while (pi != NULL) {
		*tpp = tp = getblk(sizeof (type_t));
		tp->t_tspec = PTR;
		tp->t_const = pi->p_const;
		tp->t_volatile = pi->p_volatile;
		*(tpp = &tp->t_subt) = dcs->d_type;
		npi = pi->p_nxt;
		free(pi);
		pi = npi;
	}
	return (decl);
}

/*
 * If a dimension was specified, dim is 1, otherwise 0
 * n is the specified dimension
 */
sym_t *
addarray(decl, dim, n)
	sym_t	*decl;
	int	dim, n;
{
	type_t	**tpp, *tp;

	tpp = &decl->s_type;
	while (*tpp != dcs->d_type)
		tpp = &(*tpp)->t_subt;

	*tpp = tp = getblk(sizeof (type_t));
	tp->t_tspec = ARRAY;
	tp->t_subt = dcs->d_type;
	tp->t_dim = n;

	if (n < 0) {
		/* zero or negative array dimension */
		error(20);
		n = 0;
	} else if (n == 0 && dim) {
		/* zero or negative array dimension */
		warning(20);
	} else if (n == 0 && !dim) {
		/* is incomplete type */
		setcompl(tp, 1);
	}

	return (decl);
}

sym_t *
addfunc(decl, args)
	sym_t	*decl, *args;
{
	type_t	**tpp, *tp;

	if (dcs->d_proto) {
		if (tflag)
			/* function prototypes are illegal in traditional C */
			warning(270);
		args = nsfunc(decl, args);
	} else {
		osfunc(decl, args);
	}

	/*
	 * The symbols are removed from the symbol table by popdecl() after
	 * addfunc(). To be able to restore them if this is a function
	 * definition, a pointer to the list of all symbols is stored in
	 * dcs->d_nxt->d_fpsyms. Also a list of the arguments (concatenated
	 * by s_nxt) is stored in dcs->d_nxt->d_fargs.
	 * (dcs->d_nxt must be used because *dcs is the declaration stack
	 * element created for the list of params and is removed after
	 * addfunc())
	 */
	if (dcs->d_nxt->d_ctx == EXTERN &&
	    decl->s_type == dcs->d_nxt->d_type) {
		dcs->d_nxt->d_fpsyms = dcs->d_dlsyms;
		dcs->d_nxt->d_fargs = args;
	}

	tpp = &decl->s_type;
	while (*tpp != dcs->d_nxt->d_type)
		tpp = &(*tpp)->t_subt;

	*tpp = tp = getblk(sizeof (type_t));
	tp->t_tspec = FUNC;
	tp->t_subt = dcs->d_nxt->d_type;
	if ((tp->t_proto = dcs->d_proto) != 0)
		tp->t_args = args;
	tp->t_vararg = dcs->d_vararg;

	return (decl);
}

/*
 * Called for new style function declarations.
 */
/* ARGSUSED */
static sym_t *
nsfunc(decl, args)
	sym_t	*decl, *args;
{
	sym_t	*arg, *sym;
	scl_t	sc;
	int	n;

	/*
	 * Declarations of structs/unions/enums in param lists are legal,
	 * but senseless.
	 */
	for (sym = dcs->d_dlsyms; sym != NULL; sym = sym->s_dlnxt) {
		sc = sym->s_scl;
		if (sc == STRTAG || sc == UNIONTAG || sc == ENUMTAG) {
			/* dubious tag declaration: %s %s */
			warning(85, scltoa(sc), sym->s_name);
		}
	}

	n = 1;
	for (arg = args; arg != NULL; arg = arg->s_nxt) {
		if (arg->s_type->t_tspec == VOID) {
			if (n > 1 || arg->s_nxt != NULL) {
				/* "void" must be sole parameter */
				error(60);
				arg->s_type = gettyp(INT);
			}
		}
		n++;
	}

	/* return NULL if first param is VOID */
	return (args != NULL && args->s_type->t_tspec != VOID ? args : NULL);
}

/*
 * Called for old style function declarations.
 */
static void
osfunc(decl, args)
	sym_t	*decl, *args;
{
	/*
	 * Remember list of params only if this is really seams to be
	 * a function definition.
	 */
	if (dcs->d_nxt->d_ctx == EXTERN &&
	    decl->s_type == dcs->d_nxt->d_type) {
		/*
		 * We assume that this becomes a function definition. If
		 * we are wrong, its corrected in chkfdef(). 
		 */
		if (args != NULL) {
			decl->s_osdef = 1;
			decl->s_args = args;
		}
	} else {
		if (args != NULL)
			/* function prototype parameters must have types */
			warning(62);
	}
}

/*
 * Lists of Identifiers in functions declarations are allowed only if
 * its also a function definition. If this is not the case, print a
 * error message.
 */
void
chkfdef(sym, msg)
	sym_t	*sym;
	int	msg;
{
	if (sym->s_osdef) {
		if (msg) {
			/* incomplete or misplaced function definition */
			error(22);
		}
		sym->s_osdef = 0;
		sym->s_args = NULL;
	}
}

/*
 * Process the name in a declarator.
 * If the symbol does already exists, a new one is created.
 * The symbol becomes one of the storage classes EXTERN, STATIC, AUTO or
 * TYPEDEF.
 * s_def and s_reg are valid after dname().
 */
sym_t *
dname(sym)
	sym_t	*sym;
{
	scl_t	sc;

	if (sym->s_scl == NOSCL) {
		dcs->d_rdcsym = NULL;
	} else if (sym->s_defarg) {
		sym->s_defarg = 0;
		dcs->d_rdcsym = NULL;
	} else {
		dcs->d_rdcsym = sym;
		sym = pushdown(sym);
	}

	switch (dcs->d_ctx) {
	case MOS:
	case MOU:
		/* Parent setzen */
		sym->s_styp = dcs->d_tagtyp->t_str;
		sym->s_def = DEF;
		sym->s_value.v_tspec = INT;
		sc = dcs->d_ctx;
		break;
	case EXTERN:
		/*
		 * static and external symbols without "extern" are
		 * considered to be tentative defined, external
		 * symbols with "extern" are declared, and typedef names
		 * are defined. Tentative defined and declared symbols
		 * may become defined if an initializer is present or
		 * this is a function definition.
		 */
		if ((sc = dcs->d_scl) == NOSCL) {
			sc = EXTERN;
			sym->s_def = TDEF;
		} else if (sc == STATIC) {
			sym->s_def = TDEF;
		} else if (sc == TYPEDEF) {
			sym->s_def = DEF;
		} else if (sc == EXTERN) {
			sym->s_def = DECL;
		} else {
			lerror("dname() 1");
		}
		break;
	case PARG:
		sym->s_arg = 1;
		/* FALLTHROUGH */
	case ARG:
		if ((sc = dcs->d_scl) == NOSCL) {
			sc = AUTO;
		} else if (sc == REG) {
			sym->s_reg = 1;
			sc = AUTO;
		} else {
			lerror("dname() 2");
		}
		sym->s_def = DEF;
		break;
	case AUTO:
		if ((sc = dcs->d_scl) == NOSCL) {
			/*
			 * XXX somewhat ugly because we dont know whether
			 * this is AUTO or EXTERN (functions). If we are
			 * wrong it must be corrected in decl1loc(), where
			 * we have the neccessary type information.
			 */
			sc = AUTO;
			sym->s_def = DEF;
		} else if (sc == AUTO || sc == STATIC || sc == TYPEDEF) {
			sym->s_def = DEF;
		} else if (sc == REG) {
			sym->s_reg = 1;
			sc = AUTO;
			sym->s_def = DEF;
		} else if (sc == EXTERN) {
			sym->s_def = DECL;
		} else {
			lerror("dname() 3");
		}
		break;
	default:
		lerror("dname() 4");
	}
	sym->s_scl = sc;

	sym->s_type = dcs->d_type;

	dcs->d_fpsyms = NULL;

	return (sym);
}

/*
 * Process a name in the list of formal params in an old style function
 * definition.
 */
sym_t *
iname(sym)
	sym_t	*sym;
{
	if (sym->s_scl != NOSCL) {
		if (blklev == sym->s_blklev) {
			/* redeclaration of formal parameter %s */
			error(21, sym->s_name);
			if (!sym->s_defarg)
				lerror("iname()");
		}
		sym = pushdown(sym);
	}
	sym->s_type = gettyp(INT);
	sym->s_scl = AUTO;
	sym->s_def = DEF;
	sym->s_defarg = sym->s_arg = 1;
	return (sym);
}

/*
 * Create the type of a tag.
 *
 * tag points to the symbol table entry of the tag
 * kind is the kind of the tag (STRUCT/UNION/ENUM)
 * decl is 1 if the type of the tag will be completed in this declaration
 * (the following token is T_LBRACE)
 * semi is 1 if the following token is T_SEMI
 */
type_t *
mktag(tag, kind, decl, semi)
	sym_t	*tag;
	tspec_t	kind;
	int	decl, semi;
{
	scl_t	scl;
	type_t	*tp;

	if (kind == STRUCT) {
		scl = STRTAG;
	} else if (kind == UNION) {
		scl = UNIONTAG;
	} else if (kind == ENUM) {
		scl = ENUMTAG;
	} else {
		lerror("mktag()");
	}

	if (tag != NULL) {
		if (tag->s_scl != NOSCL) {
			tag = newtag(tag, scl, decl, semi);
		} else {
			/* a new tag, no empty declaration */
			dcs->d_nxt->d_nedecl = 1;
			if (scl == ENUMTAG && !decl) {
				if (!tflag && (sflag || pflag))
					/* forward reference to enum type */
					warning(42);
			}
		}
		if (tag->s_scl == NOSCL) {
			tag->s_scl = scl;
			tag->s_type = tp = getblk(sizeof (type_t));
		} else {
			tp = tag->s_type;
		}
	} else {
		tag = getblk(sizeof (sym_t));
		tag->s_name = unnamed;
		STRUCT_ASSIGN(tag->s_dpos, curr_pos);
		tag->s_kind = FTAG;
		tag->s_scl = scl;
		tag->s_blklev = -1;
		tag->s_type = tp = getblk(sizeof (type_t));
		dcs->d_nxt->d_nedecl = 1;
	}

	if (tp->t_tspec == NOTSPEC) {
		tp->t_tspec = kind;
		if (kind != ENUM) {
			tp->t_str = getblk(sizeof (str_t));
			tp->t_str->align = CHAR_BIT;
			tp->t_str->stag = tag;
		} else {
			tp->t_isenum = 1;
			tp->t_enum = getblk(sizeof (enum_t));
			tp->t_enum->etag = tag;
		}
		/* ist unvollstaendiger Typ */
		setcompl(tp, 1);
	}

	return (tp);
}

/*
 * Checks all possible cases of tag redeclarations.
 * decl is 1 if T_LBRACE follows
 * semi is 1 if T_SEMI follows
 */
static sym_t *
newtag(tag, scl, decl, semi)
	sym_t	*tag;
	scl_t	scl;
	int	decl, semi;
{
	if (tag->s_blklev < blklev) {
		if (semi) {
			/* "struct a;" */
			if (!tflag) {
				if (!sflag)
					/* decl. introduces new type ... */
					warning(44, scltoa(scl), tag->s_name);
				tag = pushdown(tag);
			} else if (tag->s_scl != scl) {
				/* base type is really "%s %s" */
				warning(45, scltoa(tag->s_scl), tag->s_name);
			}
			dcs->d_nxt->d_nedecl = 1;
		} else if (decl) {
			/* "struct a { ..." */
			if (hflag)
				/* redefinition hides earlier one: %s */
				warning(43, tag->s_name);
			tag = pushdown(tag);
			dcs->d_nxt->d_nedecl = 1;
		} else if (tag->s_scl != scl) {
			/* base type is really "%s %s" */
			warning(45, scltoa(tag->s_scl), tag->s_name);
			/* declaration introduces new type in ANSI C: %s %s */
			if (!sflag)
				warning(44, scltoa(scl), tag->s_name);
			tag = pushdown(tag);
			dcs->d_nxt->d_nedecl = 1;
		}
	} else {
		if (tag->s_scl != scl) {
			/* (%s) tag redeclared */
			error(46, scltoa(tag->s_scl));
			prevdecl(-1, tag);
			tag = pushdown(tag);
			dcs->d_nxt->d_nedecl = 1;
		} else if (decl && !incompl(tag->s_type)) {
			/* (%s) tag redeclared */
			error(46, scltoa(tag->s_scl));
			prevdecl(-1, tag);
			tag = pushdown(tag);
			dcs->d_nxt->d_nedecl = 1;
		} else if (semi || decl) {
			dcs->d_nxt->d_nedecl = 1;
		}
	}
	return (tag);
}

const char *
scltoa(sc)
	scl_t	sc;
{
	const	char *s;

	switch (sc) {
	case EXTERN:	s = "extern";	break;
	case STATIC:	s = "static";	break;
	case AUTO:	s = "auto";	break;
	case REG:	s = "register";	break;
	case TYPEDEF:	s = "typedef";	break;
	case STRTAG:	s = "struct";	break;
	case UNIONTAG:	s = "union";	break;
	case ENUMTAG:	s = "enum";	break;
	default:	lerror("tagttoa()");
	}
	return (s);
}

/*
 * Completes the type of a tag in a struct/union/enum declaration.
 * tp points to the type of the, tag, fmem to the list of members/enums.
 */
type_t *
compltag(tp, fmem)
	type_t	*tp;
	sym_t	*fmem;
{
	tspec_t	t;
	str_t	*sp;
	int	n;
	sym_t	*mem;

	/* from now a complete type */
	setcompl(tp, 0);

	if ((t = tp->t_tspec) != ENUM) {
		align(dcs->d_stralign, 0);
		sp = tp->t_str;
		sp->align = dcs->d_stralign;
		sp->size = dcs->d_offset;
		sp->memb = fmem;
		if (sp->size == 0) {
			/* zero sized %s */
			(void)gnuism(47, ttab[t].tt_name);
		} else {
			n = 0;
			for (mem = fmem; mem != NULL; mem = mem->s_nxt) {
				if (mem->s_name != unnamed)
					n++;
			}
			if (n == 0) {
				/* %s has no named members */
				warning(65,
					t == STRUCT ? "structure" : "union");
			}
		}
	} else {
		tp->t_enum->elem = fmem;
	}
	return (tp);
}

/*
 * Processes the name of an enumerator in en enum declaration.
 *
 * sym points to the enumerator
 * val is the value of the enumerator
 * impl is 1 if the the value of the enumerator was not explicit specified.
 */
sym_t *
ename(sym, val, impl)
	sym_t	*sym;
	int	val, impl;
{
	if (sym->s_scl) {
		if (sym->s_blklev == blklev) {
			/* no hflag, because this is illegal!!! */
			if (sym->s_arg) {
				/* enumeration constant hides parameter: %s */
				warning(57, sym->s_name);
			} else {
				/* redeclaration of %s */
				error(27, sym->s_name);
				/*
				 * inside blocks it should not too complicated
				 * to find the position of the previous
				 * declaration
				 */
				if (blklev == 0)
					prevdecl(-1, sym);
			}
		} else {
			if (hflag)
				/* redefinition hides earlier one: %s */
				warning(43, sym->s_name);
		}
		sym = pushdown(sym);
	}
	sym->s_scl = ENUMCON;
	sym->s_type = dcs->d_tagtyp;
	sym->s_value.v_tspec = INT;
	sym->s_value.v_quad = val;
	if (impl && val - 1 == INT_MAX) {
		/* overflow in enumeration values: %s */
		warning(48, sym->s_name);
	}
	enumval = val + 1;
	return (sym);
}

/*
 * Process a single external declarator.
 */
void
decl1ext(dsym, initflg)
	sym_t	*dsym;
	int	initflg;
{
	int	warn, rval, redec;
	sym_t	*rdsym;

	chkfdef(dsym, 1);

	chktyp(dsym);

	if (initflg && !(initerr = chkinit(dsym)))
		dsym->s_def = DEF;

	/*
	 * Declarations of functions are marked as "tentative" in dname().
	 * This is wrong because there are no tentative function
	 * definitions.
	 */
	if (dsym->s_type->t_tspec == FUNC && dsym->s_def == TDEF)
		dsym->s_def = DECL;

	if (dcs->d_inline) {
		if (dsym->s_type->t_tspec == FUNC) {
			dsym->s_inline = 1;
		} else {
			/* variable declared inline: %s */
			warning(268, dsym->s_name);
		}
	}

	/* Write the declaration into the output file */
	if (plibflg && llibflg &&
	    dsym->s_type->t_tspec == FUNC && dsym->s_type->t_proto) {
		/*
		 * With both LINTLIBRARY and PROTOLIB the prototyp is
		 * written as a function definition to the output file.
		 */
		rval = dsym->s_type->t_subt->t_tspec != VOID;
		outfdef(dsym, &dsym->s_dpos, rval, 0, NULL);
	} else {
		outsym(dsym, dsym->s_scl, dsym->s_def);
	}

	if ((rdsym = dcs->d_rdcsym) != NULL) {

		/*
		 * If the old symbol stems from a old style function definition
		 * we have remembered the params in rdsmy->s_args and compare
		 * them with the params of the prototype.
		 */
		if (rdsym->s_osdef && dsym->s_type->t_proto) {
			redec = chkosdef(rdsym, dsym);
		} else {
			redec = 0;
		}

		if (!redec && !isredec(dsym, (warn = 0, &warn))) {
		
			if (warn) {
				/* redeclaration of %s */
				(*(sflag ? error : warning))(27, dsym->s_name);
				prevdecl(-1, rdsym);
			}

			/*
			 * Overtake the rememberd params if the new symbol
			 * is not a prototype.
			 */
			if (rdsym->s_osdef && !dsym->s_type->t_proto) {
				dsym->s_osdef = rdsym->s_osdef;
				dsym->s_args = rdsym->s_args;
				STRUCT_ASSIGN(dsym->s_dpos, rdsym->s_dpos);
			}

			/*
			 * Remember the position of the declaration if the
			 * old symbol was a prototype and the new is not.
			 * Also remember the position if the old symbol
			 * was defined and the new is not.
			 */
			if (rdsym->s_type->t_proto && !dsym->s_type->t_proto) {
				STRUCT_ASSIGN(dsym->s_dpos, rdsym->s_dpos);
			} else if (rdsym->s_def == DEF && dsym->s_def != DEF) {
				STRUCT_ASSIGN(dsym->s_dpos, rdsym->s_dpos);
			}

			/*
			 * Copy informations about usage of the name into
			 * the new symbol.
			 */
			cpuinfo(dsym, rdsym);

			/* Once a name is defined, it remains defined. */
			if (rdsym->s_def == DEF)
				dsym->s_def = DEF;

			/* once a function is inline, it remains inline */
			if (rdsym->s_inline)
				dsym->s_inline = 1;

			compltyp(dsym, rdsym);

		}
		
		rmsym(rdsym);
	}

	if (dsym->s_scl == TYPEDEF) {
		dsym->s_type = duptyp(dsym->s_type);
		dsym->s_type->t_typedef = 1;
		settdsym(dsym->s_type, dsym);
	}

}

/*
 * Copies informations about usage into a new symbol table entry of
 * the same symbol.
 */
void
cpuinfo(sym, rdsym)
	sym_t	*sym, *rdsym;
{
	sym->s_spos = rdsym->s_spos;
	sym->s_upos = rdsym->s_upos;
	sym->s_set = rdsym->s_set;
	sym->s_used = rdsym->s_used;
}

/*
 * Prints an error and returns 1 if a symbol is redeclared/redefined.
 * Otherwise returns 0 and, in some cases of minor problems, prints
 * a warning.
 */
int
isredec(dsym, warn)
	sym_t	*dsym;
	int	*warn;
{
	sym_t	*rsym;

	if ((rsym = dcs->d_rdcsym)->s_scl == ENUMCON) {
		/* redeclaration of %s */
		error(27, dsym->s_name);
		prevdecl(-1, rsym);
		return (1);
	}
	if (rsym->s_scl == TYPEDEF) {
		/* typedef redeclared: %s */
		error(89, dsym->s_name);
		prevdecl(-1, rsym);
		return (1);
	}
	if (dsym->s_scl == TYPEDEF) {
		/* redeclaration of %s */
		error(27, dsym->s_name);
		prevdecl(-1, rsym);
		return (1);
	}
	if (rsym->s_def == DEF && dsym->s_def == DEF) {
		/* redefinition of %s */
		error(28, dsym->s_name);
		prevdecl(-1, rsym);
		return(1);
	}
	if (!eqtype(rsym->s_type, dsym->s_type, 0, 0, warn)) {
		/* redeclaration of %s */
		error(27, dsym->s_name);
		prevdecl(-1, rsym);
		return(1);
	}
	if (rsym->s_scl == EXTERN && dsym->s_scl == EXTERN)
		return(0);
	if (rsym->s_scl == STATIC && dsym->s_scl == STATIC)
		return(0);
	if (rsym->s_scl == STATIC && dsym->s_def == DECL)
		return(0);
	if (rsym->s_scl == EXTERN && rsym->s_def == DEF) {
		/*
		 * All cases except "int a = 1; static int a;" are catched
		 * above with or without a warning
		 */
		/* redeclaration of %s */
		error(27, dsym->s_name);
		prevdecl(-1, rsym);
		return(1);
	}
	if (rsym->s_scl == EXTERN) {
		/* previously declared extern, becomes static: %s */
		warning(29, dsym->s_name);
		prevdecl(-1, rsym);
		return(0);
	}
	/*
	 * Now its on of:
	 * "static a; int a;", "static a; int a = 1;", "static a = 1; int a;"
	 */
	/* redeclaration of %s; ANSI C requires "static" */
	if (sflag) {
		warning(30, dsym->s_name);
		prevdecl(-1, rsym);
	}
	dsym->s_scl = STATIC;
	return (0);
}

/*
 * Checks if two types are compatible. Returns 0 if not, otherwise 1.
 *
 * ignqual	ignore qualifiers of type; used for function params
 * promot	promote left type; used for comparision of params of
 *		old style function definitions with params of prototypes.
 * *warn	set to 1 if an old style function declaration is not
 *		compatible with a prototype
 */
int
eqtype(tp1, tp2, ignqual, promot, warn)
	type_t	*tp1, *tp2;
	int	ignqual, promot, *warn;
{
	tspec_t	t;

	while (tp1 != NULL && tp2 != NULL) {

		t = tp1->t_tspec;
		if (promot) {
			if (t == FLOAT) {
				t = DOUBLE;
			} else if (t == CHAR || t == SCHAR) {
				t = INT;
			} else if (t == UCHAR) {
				t = tflag ? UINT : INT;
			} else if (t == SHORT) {
				t = INT;
			} else if (t == USHORT) {
				/* CONSTCOND */
				t = INT_MAX < USHRT_MAX || tflag ? UINT : INT;
			}
		}

		if (t != tp2->t_tspec)
			return (0);

		if (tp1->t_const != tp2->t_const && !ignqual && !tflag)
			return (0);

		if (tp1->t_volatile != tp2->t_volatile && !ignqual && !tflag)
			return (0);

		if (t == STRUCT || t == UNION)
			return (tp1->t_str == tp2->t_str);

		if (t == ARRAY && tp1->t_dim != tp2->t_dim) {
			if (tp1->t_dim != 0 && tp2->t_dim != 0)
				return (0);
		}

		/* dont check prototypes for traditional */
		if (t == FUNC && !tflag) {
			if (tp1->t_proto && tp2->t_proto) {
				if (!eqargs(tp1, tp2, warn))
					return (0);
			} else if (tp1->t_proto) {
				if (!mnoarg(tp1, warn))
					return (0);
			} else if (tp2->t_proto) {
				if (!mnoarg(tp2, warn))
					return (0);
			}
		}

		tp1 = tp1->t_subt;
		tp2 = tp2->t_subt;
		ignqual = promot = 0;

	}

	return (tp1 == tp2);
}

/*
 * Compares the parameter types of two prototypes.
 */
static int
eqargs(tp1, tp2, warn)
	type_t	*tp1, *tp2;
	int	*warn;
{
	sym_t	*a1, *a2;

	if (tp1->t_vararg != tp2->t_vararg)
		return (0);

	a1 = tp1->t_args;
	a2 = tp2->t_args;

	while (a1 != NULL && a2 != NULL) {

		if (eqtype(a1->s_type, a2->s_type, 1, 0, warn) == 0)
			return (0);

		a1 = a1->s_nxt;
		a2 = a2->s_nxt;

	}

	return (a1 == a2);
}

/*
 * mnoarg() (matches functions with no argument type information)
 * returns 1 if all parameters of a prototype are compatible with
 * and old style function declaration.
 * This is the case if following conditions are met:
 *	1. the prototype must have a fixed number of parameters
 *	2. no parameter is of type float
 *	3. no parameter is converted to another type if integer promotion
 *	   is applied on it
 */
static int
mnoarg(tp, warn)
	type_t	*tp;
	int	*warn;
{
	sym_t	*arg;
	tspec_t	t;

	if (tp->t_vararg) {
		if (warn != NULL)
			*warn = 1;
	}
	for (arg = tp->t_args; arg != NULL; arg = arg->s_nxt) {
		if ((t = arg->s_type->t_tspec) == FLOAT ||
		    t == CHAR || t == SCHAR || t == UCHAR ||
		    t == SHORT || t == USHORT) {
			if (warn != NULL)
				*warn = 1;
		}
	}
	return (1);
}

/*
 * Compares a prototype declaration with the remembered arguments of
 * a previous old style function definition.
 */
static int
chkosdef(rdsym, dsym)
	sym_t	*rdsym, *dsym;
{
	sym_t	*args, *pargs, *arg, *parg;
	int	narg, nparg, n;
	int	warn, msg;

	args = rdsym->s_args;
	pargs = dsym->s_type->t_args;

	msg = 0;

	narg = nparg = 0;
	for (arg = args; arg != NULL; arg = arg->s_nxt)
		narg++;
	for (parg = pargs; parg != NULL; parg = parg->s_nxt)
		nparg++;
	if (narg != nparg) {
		/* prototype does not match old-style definition */
		error(63);
		msg = 1;
		goto end;
	}

	arg = args;
	parg = pargs;
	n = 1;
	while (narg--) {
		warn = 0;
		/*
		 * If it does not match due to promotion and sflag is
		 * not set we print only a warning.
		 */
		if (!eqtype(arg->s_type, parg->s_type, 1, 1, &warn) || warn) {
			/* prototype does not match old-style def., arg #%d */
			error(299, n);
			msg = 1;
		}
		arg = arg->s_nxt;
		parg = parg->s_nxt;
		n++;
	}

 end:
	if (msg)
		/* old style definition */
		prevdecl(300, rdsym);

	return (msg);
}

/*
 * Complets a type by copying the dimension and prototype information
 * from a second compatible type.
 *
 * Following lines are legal:
 *  "typedef a[]; a b; a b[10]; a c; a c[20];"
 *  "typedef ft(); ft f; f(int); ft g; g(long);"
 * This means that, if a type is completed, the type structure must
 * be duplicated.
 */
void
compltyp(dsym, ssym)
	sym_t	*dsym, *ssym;
{
	type_t	**dstp, *src;
	type_t	*dst;

	dstp = &dsym->s_type;
	src = ssym->s_type;

	while ((dst = *dstp) != NULL) {
		if (src == NULL || dst->t_tspec != src->t_tspec)
			lerror("compltyp() 1");
		if (dst->t_tspec == ARRAY) {
			if (dst->t_dim == 0 && src->t_dim != 0) {
				*dstp = dst = duptyp(dst);
				dst->t_dim = src->t_dim;
				/* now a complete Typ */
				setcompl(dst, 0);
			}
		} else if (dst->t_tspec == FUNC) {
			if (!dst->t_proto && src->t_proto) {
				*dstp = dst = duptyp(dst);
				dst->t_proto = 1;
				dst->t_args = src->t_args;
			}
		}
		dstp = &dst->t_subt;
		src = src->t_subt;
	}
}

/*
 * Completes the declaration of a single argument.
 */
sym_t *
decl1arg(sym, initflg)
	sym_t	*sym;
	int	initflg;
{
	tspec_t	t;

	chkfdef(sym, 1);

	chktyp(sym);

	if (dcs->d_rdcsym != NULL && dcs->d_rdcsym->s_blklev == blklev) {
		/* redeclaration of formal parameter %s */
		error(237, sym->s_name);
		rmsym(dcs->d_rdcsym);
		sym->s_arg = 1;
	}

	if (!sym->s_arg) {
		/* declared argument %s is missing */
		error(53, sym->s_name);
		sym->s_arg = 1;
	}

	if (initflg) {
		/* cannot initialize parameter: %s */
		error(52, sym->s_name);
		initerr = 1;
	}

	if ((t = sym->s_type->t_tspec) == ARRAY) {
		sym->s_type = incref(sym->s_type->t_subt, PTR);
	} else if (t == FUNC) {
		if (tflag)
			/* a function is declared as an argument: %s */
			warning(50, sym->s_name);
		sym->s_type = incref(sym->s_type, PTR);
	} else if (t == FLOAT) {
		if (tflag)
			sym->s_type = gettyp(DOUBLE);
	}

	if (dcs->d_inline)
		/* argument declared inline: %s */
		warning(269, sym->s_name);

	/*
	 * Arguments must have complete types. lengths() prints the needed
	 * error messages (null dimension is impossible because arrays are
	 * converted to pointers).
	 */
	if (sym->s_type->t_tspec != VOID)
		(void)length(sym->s_type, sym->s_name);

	setsflg(sym);

	return (sym);
}

/*
 * Does some checks for lint directives which apply to functions.
 * Processes arguments in old style function definitions which default
 * to int.
 * Checks compatiblility of old style function definition with previous
 * prototype.
 */
void
cluparg()
{
	sym_t	*args, *arg, *pargs, *parg;
	int	narg, nparg, n, msg;
	tspec_t	t;

	args = funcsym->s_args;
	pargs = funcsym->s_type->t_args;

	/* check for illegal combinations of lint directives */
	if (prflstrg != -1 && scflstrg != -1) {
		/* can't be used together: ** PRINTFLIKE ** ** SCANFLIKE ** */
		warning(289);
		prflstrg = scflstrg = -1;
	}
	if (nvararg != -1 && (prflstrg != -1 || scflstrg != -1)) {
		/* dubious use of ** VARARGS ** with ** %s ** */
		warning(288, prflstrg != -1 ? "PRINTFLIKE" : "SCANFLIKE");
		nvararg = -1;
	}

	/*
	 * check if the argument of a lint directive is compatible with the
	 * number of arguments.
	 */
	narg = 0;
	for (arg = dcs->d_fargs; arg != NULL; arg = arg->s_nxt)
		narg++;
	if (nargusg > narg) {
		/* argument number mismatch with directive: ** %s ** */
		warning(283, "ARGSUSED");
		nargusg = 0;
	}
	if (nvararg > narg) {
		/* argument number mismatch with directive: ** %s ** */
		warning(283, "VARARGS");
		nvararg = 0;
	}
	if (prflstrg > narg) {
		/* argument number mismatch with directive: ** %s ** */
		warning(283, "PRINTFLIKE");
		prflstrg = -1;
	} else if (prflstrg == 0) {
		prflstrg = -1;
	}
	if (scflstrg > narg) {
		/* argument number mismatch with directive: ** %s ** */
		warning(283, "SCANFLIKE");
		scflstrg = -1;
	} else if (scflstrg == 0) {
		scflstrg = -1;
	}
	if (prflstrg != -1 || scflstrg != -1) {
		narg = prflstrg != -1 ? prflstrg : scflstrg;
		arg = dcs->d_fargs;
		for (n = 1; n < narg; n++)
			arg = arg->s_nxt;
		if (arg->s_type->t_tspec != PTR ||
		    ((t = arg->s_type->t_subt->t_tspec) != CHAR &&
		     t != UCHAR && t != SCHAR)) {
			/* arg. %d must be 'char *' for PRINTFLIKE/SCANFLIKE */
			warning(293, narg);
			prflstrg = scflstrg = -1;
		}
	}

	/*
	 * print a warning for each argument off an old style function
	 * definition which defaults to int
	 */
	for (arg = args; arg != NULL; arg = arg->s_nxt) {
		if (arg->s_defarg) {
			/* argument type defaults to int: %s */
			warning(32, arg->s_name);
			arg->s_defarg = 0;
			setsflg(arg);
		}
	}

	/*
	 * If this is an old style function definition and a prototyp
	 * exists, compare the types of arguments.
	 */
	if (funcsym->s_osdef && funcsym->s_type->t_proto) {
		/*
		 * If the number of arguments does not macht, we need not
		 * continue.
		 */
		narg = nparg = 0;
		msg = 0;
		for (parg = pargs; parg != NULL; parg = parg->s_nxt)
			nparg++;
		for (arg = args; arg != NULL; arg = arg->s_nxt)
			narg++;
		if (narg != nparg) {
			/* parameter mismatch: %d declared, %d defined */
			error(51, nparg, narg);
			msg = 1;
		} else {
			parg = pargs;
			arg = args;
			while (narg--) {
				msg |= chkptdecl(arg, parg);
				parg = parg->s_nxt;
				arg = arg->s_nxt;
			}
		}
		if (msg)
			/* prototype declaration */
			prevdecl(285, dcs->d_rdcsym);

		/* from now the prototype is valid */
		funcsym->s_osdef = 0;
		funcsym->s_args = NULL;
		
	}

}

/*
 * Checks compatibility of an old style function definition with a previous
 * prototype declaration.
 * Returns 1 if the position of the previous declaration should be reported.
 */
static int
chkptdecl(arg, parg)
	sym_t	*arg, *parg;
{
	type_t	*tp, *ptp;
	int	warn, msg;

	tp = arg->s_type;
	ptp = parg->s_type;

	msg = 0;
	warn = 0;

	if (!eqtype(tp, ptp, 1, 1, &warn)) {
		if (eqtype(tp, ptp, 1, 0, &warn)) {
			/* type does not match prototype: %s */
			msg = gnuism(58, arg->s_name);
		} else {
			/* type does not match prototype: %s */
			error(58, arg->s_name);
			msg = 1;
		}
	} else if (warn) {
		/* type does not match prototype: %s */
		(*(sflag ? error : warning))(58, arg->s_name);
		msg = 1;
	}

	return (msg);
}

/*
 * Completes a single local declaration/definition.
 */
void
decl1loc(dsym, initflg)
	sym_t	*dsym;
	int	initflg;
{
	/* Correct a mistake done in dname(). */
	if (dsym->s_type->t_tspec == FUNC) {
		dsym->s_def = DECL;
		if (dcs->d_scl == NOSCL)
			dsym->s_scl = EXTERN;
	}

	if (dsym->s_type->t_tspec == FUNC) {
		if (dsym->s_scl == STATIC) {
			/* dubious static function at block level: %s */
			warning(93, dsym->s_name);
			dsym->s_scl = EXTERN;
		} else if (dsym->s_scl != EXTERN && dsym->s_scl != TYPEDEF) {
			/* function has illegal storage class: %s */
			error(94, dsym->s_name);
			dsym->s_scl = EXTERN;
		}
	}

	/*
	 * functions may be declared inline at local scope, although
	 * this has no effect for a later definition of the same
	 * function.
	 * XXX it should have an effect if tflag is set. this would
	 * also be the way gcc behaves.
	 */
	if (dcs->d_inline) {
		if (dsym->s_type->t_tspec == FUNC) {
			dsym->s_inline = 1;
		} else {
			/* variable declared inline: %s */
			warning(268, dsym->s_name);
		}
	}

	chkfdef(dsym, 1);

	chktyp(dsym);

	if (dcs->d_rdcsym != NULL && dsym->s_scl == EXTERN)
		ledecl(dsym);

	if (dsym->s_scl == EXTERN) {
		/*
		 * XXX wenn die statische Variable auf Ebene 0 erst
		 * spaeter definiert wird, haben wir die Brille auf.
		 */
		if (dsym->s_xsym == NULL) {
			outsym(dsym, EXTERN, dsym->s_def);
		} else {
			outsym(dsym, dsym->s_xsym->s_scl, dsym->s_def);
		}
	}

	if (dcs->d_rdcsym != NULL) {

		if (dcs->d_rdcsym->s_blklev == 0) {

			switch (dsym->s_scl) {
			case AUTO:
				/* automatic hides external declaration: %s */
				if (hflag)
					warning(86, dsym->s_name);
				break;
			case STATIC:
				/* static hides external declaration: %s */
				if (hflag)
					warning(87, dsym->s_name);
				break;
			case TYPEDEF:
				/* typedef hides  external declaration: %s */
				if (hflag)
					warning(88, dsym->s_name);
				break;
			case EXTERN:
				/*
				 * Warnings and errors are printed in ledecl()
				 */
				break;
			default:
				lerror("decl1loc() 1");
			}

		} else if (dcs->d_rdcsym->s_blklev == blklev) {

			/* no hflag, because its illegal! */
			if (dcs->d_rdcsym->s_arg) {
				/*
				 * if !tflag, a "redeclaration of %s" error
				 * is produced below
				 */
				if (tflag) {
					if (hflag)
						/* decl. hides parameter: %s */
						warning(91, dsym->s_name);
					rmsym(dcs->d_rdcsym);
				}
			}

		} else if (dcs->d_rdcsym->s_blklev < blklev) {

			if (hflag)
				/* declaration hides earlier one: %s */
				warning(95, dsym->s_name);
			
		}

		if (dcs->d_rdcsym->s_blklev == blklev) {

			/* redeclaration of %s */
			error(27, dsym->s_name);
			rmsym(dcs->d_rdcsym);

		}

	}

	if (initflg && !(initerr = chkinit(dsym))) {
		dsym->s_def = DEF;
		setsflg(dsym);
	}

	if (dsym->s_scl == TYPEDEF) {
		dsym->s_type = duptyp(dsym->s_type);
		dsym->s_type->t_typedef = 1;
		settdsym(dsym->s_type, dsym);
	}

	/*
	 * Before we can check the size we must wait for a initialisation
	 * which may follow.
	 */
}

/*
 * Processes (re)declarations of external Symbols inside blocks.
 */
static void
ledecl(dsym)
	sym_t	*dsym;
{
	int	eqt, warn;
	sym_t	*esym;

	/* look for a symbol with the same name */
	esym = dcs->d_rdcsym;
	while (esym != NULL && esym->s_blklev != 0) {
		while ((esym = esym->s_link) != NULL) {
			if (esym->s_kind != FVFT)
				continue;
			if (strcmp(dsym->s_name, esym->s_name) == 0)
				break;
		}
	}
	if (esym == NULL)
		return;
	if (esym->s_scl != EXTERN && esym->s_scl != STATIC) {
		/* gcc accepts this without a warning, pcc prints an error. */
		/* redeclaration of %s */
		warning(27, dsym->s_name);
		prevdecl(-1, esym);
		return;
	}

	warn = 0;
	eqt = eqtype(esym->s_type, dsym->s_type, 0, 0, &warn);

	if (!eqt || warn) {
		if (esym->s_scl == EXTERN) {
			/* inconsistent redeclaration of extern: %s */
			warning(90, dsym->s_name);
			prevdecl(-1, esym);
		} else {
			/* inconsistent redeclaration of static: %s */
			warning(92, dsym->s_name);
			prevdecl(-1, esym);
		}
	}

	if (eqt) {
		/*
		 * Remember the external symbol so we can update usage
		 * information at the end of the block.
		 */
		dsym->s_xsym = esym;
	}
}

/*
 * Print an error or a warning if the symbol cant be initialized due
 * to type/storage class. Returnvalue is 1 if an error has been
 * detected.
 */
static int
chkinit(sym)
	sym_t	*sym;
{
	int	err;

	err = 0;

	if (sym->s_type->t_tspec == FUNC) {
		/* cannot initialize function: %s */
		error(24, sym->s_name);
		err = 1;
	} else if (sym->s_scl == TYPEDEF) {
		/* cannot initialize typedef: %s */
		error(25, sym->s_name);
		err = 1;
	} else if (sym->s_scl == EXTERN && sym->s_def == DECL) {
		/* cannot initialize "extern" declaration: %s */
		if (dcs->d_ctx == EXTERN) {
			warning(26, sym->s_name);
		} else {
			error(26, sym->s_name);
			err = 1;
		}
	}

	return (err);
}

/*
 * Create a symbole for an abstract declaration.
 */
sym_t *
aname()
{
	sym_t	*sym;

	if (dcs->d_ctx != ABSTRACT && dcs->d_ctx != PARG)
		lerror("aname()");

	sym = getblk(sizeof (sym_t));

	sym->s_name = unnamed;
	sym->s_def = DEF;
	sym->s_scl = ABSTRACT;
	sym->s_blklev = -1;

	if (dcs->d_ctx == PARG)
		sym->s_arg = 1;

	sym->s_type = dcs->d_type;
	dcs->d_rdcsym = NULL;
	dcs->d_vararg = 0;

	return (sym);
}

/*
 * Removes anything which has nothing to do on global level.
 */
void
globclup()
{
	while (dcs->d_nxt != NULL)
		popdecl();

	cleanup();
	blklev = 0;
	mblklev = 0;

	/*
	 * remove all informations about pending lint directives without
	 * warnings.
	 */
	glclup(1);
}

/*
 * Process an abstract type declaration
 */
sym_t *
decl1abs(sym)
	sym_t	*sym;
{
	chkfdef(sym, 1);
	chktyp(sym);
	return (sym);
}

/*
 * Checks size after declarations of variables and their initialisation.
 */
void
chksz(dsym)
	sym_t	*dsym;
{
	/*
	 * check size only for symbols which are defined and no function and
	 * not typedef name
	 */
	if (dsym->s_def != DEF)
		return;
	if (dsym->s_scl == TYPEDEF)
		return;
	if (dsym->s_type->t_tspec == FUNC)
		return;

	if (length(dsym->s_type, dsym->s_name) == 0 &&
	    dsym->s_type->t_tspec == ARRAY && dsym->s_type->t_dim == 0) {
		/* empty array declaration: %s */
		if (tflag) {
			warning(190, dsym->s_name);
		} else {
			error(190, dsym->s_name);
		}
	}
}

/*
 * Mark an object as set if it is not already
 */
void
setsflg(sym)
	sym_t	*sym;
{
	if (!sym->s_set) {
		sym->s_set = 1;
		STRUCT_ASSIGN(sym->s_spos, curr_pos);
	}
}

/*
 * Mark an object as used if it is not already
 */
void
setuflg(sym, fcall, szof)
	sym_t	*sym;
	int	fcall, szof;
{
	if (!sym->s_used) {
		sym->s_used = 1;
		STRUCT_ASSIGN(sym->s_upos, curr_pos);
	}
	/*
	 * for function calls another record is written
	 *
	 * XXX Should symbols used in sizeof() treated as used or not?
	 * Probably not, because there is no sense to declare an
	 * external variable only to get their size.
	 */
	if (!fcall && !szof && sym->s_kind == FVFT && sym->s_scl == EXTERN)
		outusg(sym);
}

/*
 * Prints warnings for a list of variables and labels (concatenated
 * with s_dlnxt) if these are not used or only set.
 */
void
chkusage(di)
	dinfo_t	*di;
{
	sym_t	*sym;
	int	mknowarn;

	/* for this warnings LINTED has no effect */
	mknowarn = nowarn;
	nowarn = 0;

	for (sym = di->d_dlsyms; sym != NULL; sym = sym->s_dlnxt)
		chkusg1(di->d_asm, sym);

	nowarn = mknowarn;
}

/*
 * Prints a warning for a single variable or label if it is not used or
 * only set.
 */
void
chkusg1(novar, sym)
	int	novar;
	sym_t	*sym;
{
	pos_t	cpos;

	if (sym->s_blklev == -1)
		return;

	STRUCT_ASSIGN(cpos, curr_pos);

	if (sym->s_kind == FVFT) {
		if (sym->s_arg) {
			chkausg(novar, sym);
		} else {
			chkvusg(novar, sym);
		}
	} else if (sym->s_kind == FLAB) {
		chklusg(sym);
	} else if (sym->s_kind == FTAG) {
		chktusg(sym);
	}

	STRUCT_ASSIGN(curr_pos, cpos);
}

static void
chkausg(novar, arg)
	int	novar;
	sym_t	*arg;
{
	if (!arg->s_set)
		lerror("chkausg() 1");

	if (novar)
		return;

	if (!arg->s_used && vflag) {
		STRUCT_ASSIGN(curr_pos, arg->s_dpos);
		/* argument %s unused in function %s */
		warning(231, arg->s_name, funcsym->s_name);
	}
}

static void
chkvusg(novar, sym)
	int	novar;
	sym_t	*sym;
{
	scl_t	sc;
	sym_t	*xsym;

	if (blklev == 0 || sym->s_blklev == 0)
		lerror("chkvusg() 1");

	/* errors in expressions easily cause lots of these warnings */
	if (nerr != 0)
		return;

	/*
	 * XXX Only variables are checkd, although types should
	 * probably also be checked
	 */
	if ((sc = sym->s_scl) != EXTERN && sc != STATIC &&
	    sc != AUTO && sc != REG) {
		return;
	}

	if (novar)
		return;

	if (sc == EXTERN) {
		if (!sym->s_used && !sym->s_set) {
			STRUCT_ASSIGN(curr_pos, sym->s_dpos);
			/* %s unused in function %s */
			warning(192, sym->s_name, funcsym->s_name);
		}
	} else {
		if (sym->s_set && !sym->s_used) {
			STRUCT_ASSIGN(curr_pos, sym->s_spos);
			/* %s set but not used in function %s */
			warning(191, sym->s_name, funcsym->s_name);
		} else if (!sym->s_used) {
			STRUCT_ASSIGN(curr_pos, sym->s_dpos);
			/* %s unused in function %s */
			warning(192, sym->s_name, funcsym->s_name);
		}
	}

	if (sc == EXTERN) {
		/*
		 * information about usage is taken over into the symbol
		 * tabel entry at level 0 if the symbol was locally declared
		 * as an external symbol.
		 *
		 * XXX This is wrong for symbols declared static at level 0
		 * if the usage information stems from sizeof(). This is
		 * because symbols at level 0 only used in sizeof() are
		 * considered to not be used.
		 */
		if ((xsym = sym->s_xsym) != NULL) {
			if (sym->s_used && !xsym->s_used) {
				xsym->s_used = 1;
				STRUCT_ASSIGN(xsym->s_upos, sym->s_upos);
			}
			if (sym->s_set && !xsym->s_set) {
				xsym->s_set = 1;
				STRUCT_ASSIGN(xsym->s_spos, sym->s_spos);
			}
		}
	}
}

static void
chklusg(lab)
	sym_t	*lab;
{
	if (blklev != 1 || lab->s_blklev != 1)
		lerror("chklusg() 1");

	if (lab->s_set && !lab->s_used) {
		STRUCT_ASSIGN(curr_pos, lab->s_spos);
		/* label %s unused in function %s */
		warning(192, lab->s_name, funcsym->s_name);
	} else if (!lab->s_set) {
		STRUCT_ASSIGN(curr_pos, lab->s_upos);
		/* undefined label %s */
		warning(23, lab->s_name);
	}
}

static void
chktusg(sym)
	sym_t	*sym;
{
	if (!incompl(sym->s_type))
		return;

	/* complain alwasy about incomplet tags declared inside blocks */
	if (!zflag || dcs->d_ctx != EXTERN)
		return;

	STRUCT_ASSIGN(curr_pos, sym->s_dpos);
	switch (sym->s_type->t_tspec) {
	case STRUCT:
		/* struct %s never defined */
		warning(233, sym->s_name);
		break;
	case UNION:
		/* union %s never defined */
		warning(234, sym->s_name);
		break;
	case ENUM:
		/* enum %s never defined */
		warning(235, sym->s_name);
		break;
	default:
		lerror("chktusg() 1");
	}
}

/*
 * Called after the entire translation unit has been parsed.
 * Changes tentative definitions in definitions.
 * Performs some tests on global Symbols. Detected Problems are:
 * - defined variables of incomplete type
 * - constant variables which are not initialized
 * - static symbols which are never used
 */
void
chkglsyms()
{
	sym_t	*sym;
	pos_t	cpos;

	if (blklev != 0 || dcs->d_nxt != NULL)
		norecover();

	STRUCT_ASSIGN(cpos, curr_pos);

	for (sym = dcs->d_dlsyms; sym != NULL; sym = sym->s_dlnxt) {
		if (sym->s_blklev == -1)
			continue;
		if (sym->s_kind == FVFT) {
			chkglvar(sym);
		} else if (sym->s_kind == FTAG) {
			chktusg(sym);
		} else {
			if (sym->s_kind != FMOS)
				lerror("chkglsyms() 1");
		}
	}

	STRUCT_ASSIGN(curr_pos, cpos);
}

static void
chkglvar(sym)
	sym_t	*sym;
{
	if (sym->s_scl == TYPEDEF || sym->s_scl == ENUMCON)
		return;
	
	if (sym->s_scl != EXTERN && sym->s_scl != STATIC)
		lerror("chkglvar() 1");

	glchksz(sym);

	if (sym->s_scl == STATIC) {
		if (sym->s_type->t_tspec == FUNC) {
			if (sym->s_used && sym->s_def != DEF) {
				STRUCT_ASSIGN(curr_pos, sym->s_upos);
				/* static func. called but not def.. */
				error(225, sym->s_name);
			}
		}
		if (!sym->s_used) {
			STRUCT_ASSIGN(curr_pos, sym->s_dpos);
			if (sym->s_type->t_tspec == FUNC) {
				if (sym->s_def == DEF) {
					if (!sym->s_inline)
						/* static function %s unused */
						warning(236, sym->s_name);
				} else {
					/* static function %s decl. but ... */
					warning(290, sym->s_name);
				}
			} else if (!sym->s_set) {
				/* static variable %s unused */
				warning(226, sym->s_name);
			} else {
				/* static variable %s set but not used */
				warning(307, sym->s_name);
			}
		}
		if (!tflag && sym->s_def == TDEF && sym->s_type->t_const) {
			STRUCT_ASSIGN(curr_pos, sym->s_dpos);
			/* const object %s should have initializer */
			warning(227, sym->s_name);
		}
	}
}

static void
glchksz(sym)
	sym_t	*sym;
{
	if (sym->s_def == TDEF) {
		if (sym->s_type->t_tspec == FUNC)
			/*
			 * this can happen if an syntax error occured
			 * after a function declaration
			 */
			return;
		STRUCT_ASSIGN(curr_pos, sym->s_dpos);
		if (length(sym->s_type, sym->s_name) == 0 &&
		    sym->s_type->t_tspec == ARRAY && sym->s_type->t_dim == 0) {
			/* empty array declaration: %s */
			if (tflag || (sym->s_scl == EXTERN && !sflag)) {
				warning(190, sym->s_name);
			} else {
				error(190, sym->s_name);
			}
		}
	}
}

/*
 * Prints information about location of previous definition/declaration.
 */
void
prevdecl(msg, psym)
	int	msg;
	sym_t	*psym;
{
	pos_t	cpos;

	if (!rflag)
		return;

	STRUCT_ASSIGN(cpos, curr_pos);
	STRUCT_ASSIGN(curr_pos, psym->s_dpos);
	if (msg != -1) {
		message(msg, psym->s_name);
	} else if (psym->s_def == DEF || psym->s_def == TDEF) {
		/* previous definition of %s */
		message(261, psym->s_name);
	} else {
		/* previous declaration of %s */
		message(260, psym->s_name);
	}
	STRUCT_ASSIGN(curr_pos, cpos);
}
