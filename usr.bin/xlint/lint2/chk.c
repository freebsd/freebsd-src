/*	$NetBSD: chk.c,v 1.2 1995/07/03 21:24:42 cgd Exp $	*/

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
static char rcsid[] = "$NetBSD: chk.c,v 1.2 1995/07/03 21:24:42 cgd Exp $";
#endif

#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <err.h>

#include "lint2.h"

/* various type information */
ttab_t	ttab[NTSPEC];


static	void	chkund __P((hte_t *));
static	void	chkdnu __P((hte_t *));
static	void	chkdnud __P((hte_t *));
static	void	chkmd __P((hte_t *));
static	void	chkvtui __P((hte_t *, sym_t *, sym_t *));
static	void	chkvtdi __P((hte_t *, sym_t *, sym_t *));
static	void	chkfaui __P((hte_t *, sym_t *, sym_t *));
static	void	chkau __P((hte_t *, int, sym_t *, sym_t *, pos_t *,
			   fcall_t *, fcall_t *, type_t *, type_t *));
static	void	chkrvu __P((hte_t *, sym_t *));
static	void	chkadecl __P((hte_t *, sym_t *, sym_t *));
static	void	printflike __P((hte_t *,fcall_t *, int,
				const char *, type_t **));
static	void	scanflike __P((hte_t *, fcall_t *, int,
			       const char *, type_t **));
static	void	badfmt __P((hte_t *, fcall_t *));
static	void	inconarg __P((hte_t *, fcall_t *, int));
static	void	tofewarg __P((hte_t *, fcall_t *));
static	void	tomanyarg __P((hte_t *, fcall_t *));
static	int	eqtype __P((type_t *, type_t *, int, int, int, int *));
static	int	eqargs __P((type_t *, type_t *, int *));
static	int	mnoarg __P((type_t *, int *));


void
inittyp()
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

	for (i = 0; i < sizeof (ittab) / sizeof (ittab[0]); i++)
		STRUCT_ASSIGN(ttab[ittab[i].it_tspec], ittab[i].it_ttab);
	if (!pflag) {
		for (i = 0; i < NTSPEC; i++)
			ttab[i].tt_psz = ttab[i].tt_sz;
	}
}


/*
 * If there is a symbol named "main", mark it as used.
 */
void
mainused()
{
	hte_t	*hte;

	if ((hte = hsearch("main", 0)) != NULL)
		hte->h_used = 1;
}

/*
 * Performs all tests for a single name
 */
void
chkname(hte)
	hte_t	*hte;
{
	sym_t	*sym, *def, *pdecl, *decl;

	if (uflag) {
		chkund(hte);
		chkdnu(hte);
		if (xflag)
			chkdnud(hte);
	}
	chkmd(hte);

	/* Get definition, prototype declaration and declaration */
	def = pdecl = decl = NULL;
	for (sym = hte->h_syms; sym != NULL; sym = sym->s_nxt) {
		if (def == NULL && (sym->s_def == DEF || sym->s_def == TDEF))
			def = sym;
		if (pdecl == NULL && sym->s_def == DECL &&
		    TP(sym->s_type)->t_tspec == FUNC &&
		    TP(sym->s_type)->t_proto) {
			pdecl = sym;
		}
		if (decl == NULL && sym->s_def == DECL)
			decl = sym;
	}

	/* A prototype is better than an old style declaration. */
	if (pdecl != NULL)
		decl = pdecl;

	chkvtui(hte, def, decl);

	chkvtdi(hte, def, decl);

	chkfaui(hte, def, decl);

	chkrvu(hte, def);

	chkadecl(hte, def, decl);
}

/*
 * Print a warning if the name has been used, but not defined.
 */
static void
chkund(hte)
	hte_t	*hte;
{
	fcall_t	*fcall;
	usym_t	*usym;

	if (!hte->h_used || hte->h_def)
		return;

	if ((fcall = hte->h_calls) != NULL) {
		/* %s used( %s ), but not defined */
		msg(0, hte->h_name, mkpos(&fcall->f_pos));
	} else if ((usym = hte->h_usyms) != NULL) {
		/* %s used( %s ), but not defined */
		msg(0, hte->h_name, mkpos(&usym->u_pos));
	}
}

/*
 * Print a warning if the name has been defined, but never used.
 */
static void
chkdnu(hte)
	hte_t	*hte;
{
	sym_t	*sym;

	if (!hte->h_def || hte->h_used)
		return;

	for (sym = hte->h_syms; sym != NULL; sym = sym->s_nxt) {
		if (sym->s_def == DEF || sym->s_def == TDEF) {
			/* %s defined( %s ), but never used */
			msg(1, hte->h_name, mkpos(&sym->s_pos));
			break;
		}
	}
}

/*
 * Print a warning if the name has been declared, but is not used
 * or defined.
 */
static void
chkdnud(hte)
	hte_t	*hte;
{
	sym_t	*sym;

	if (hte->h_syms == NULL || hte->h_used || hte->h_def)
		return;
	
	if ((sym = hte->h_syms) != NULL) {
		if (sym->s_def != DECL)
			errx(1, "internal error: chkdnud() 1");
		/* %s declared( %s ), but never used or defined */
		msg(2, hte->h_name, mkpos(&sym->s_pos));
	}
}

/*
 * Print a warning if there is more then one definition for
 * this name.
 */
static void
chkmd(hte)
	hte_t	*hte;
{
	sym_t	*sym, *def1;
	char	*pos1;

	if (!hte->h_def)
		return;

	def1 = NULL;
	for (sym = hte->h_syms; sym != NULL; sym = sym->s_nxt) {
		/*
		 * ANSI C allows tentative definitions of the same name in
		 * only one compilation unit.
		 */
		if (sym->s_def != DEF && (!sflag || sym->s_def != TDEF))
			continue;
		if (def1 == NULL) {
			def1 = sym;
			continue;
		}
		pos1 = xstrdup(mkpos(&def1->s_pos));
		/* %s multiply defined\t%s  ::  %s */
		msg(3, hte->h_name, pos1, mkpos(&sym->s_pos));
		free(pos1);
	}
}

/*
 * Print a warning if the return value assumed for a function call
 * differs from the return value of the function definition or
 * function declaration.
 *
 * If no definition/declaration can be found, the assumed return values
 * are always int. So there is no need to compare with another function
 * call as it's done for function arguments.
 */
static void
chkvtui(hte, def, decl)
	hte_t	*hte;
	sym_t	*def, *decl;
{
	fcall_t	*call;
	char	*pos1;
	type_t	*tp1, *tp2;
	/* LINTED (automatic hides external declaration: warn) */
	int	warn, eq;
	tspec_t	t1;

	if (hte->h_calls == NULL)
		return;

	if (def == NULL)
		def = decl;
	if (def == NULL)
		return;

	t1 = (tp1 = TP(def->s_type)->t_subt)->t_tspec;
	for (call = hte->h_calls; call != NULL; call = call->f_nxt) {
		tp2 = TP(call->f_type)->t_subt;
		eq = eqtype(tp1, tp2, 1, 0, 0, (warn = 0, &warn));
		if (!call->f_rused) {
			/* no return value used */
			if ((t1 == STRUCT || t1 == UNION) && !eq) {
				/*
				 * If a function returns a struct or union it
				 * must be declared to return a struct or
				 * union, also if the return value is ignored.
				 * This is necessary because the caller must
				 * allocate stack space for the return value.
				 * If it does not, the return value would over-
				 * write other data.
				 * XXX Following massage may be confusing
				 * because it appears also if the return value
				 * was declared inconsistently. But this
				 * behaviour matches pcc based lint, so it is
				 * accepted for now.
				 */
				pos1 = xstrdup(mkpos(&def->s_pos));
				/* %s value must be decl. before use %s :: %s */
				msg(17, hte->h_name,
				    pos1, mkpos(&call->f_pos));
				free(pos1);
			}
			continue;
		}
		if (!eq || (sflag && warn)) {
			pos1 = xstrdup(mkpos(&def->s_pos));
			/* %s value used inconsistenty\t%s  ::  %s */
			msg(4, hte->h_name, pos1, mkpos(&call->f_pos));
			free(pos1);
		}
	}
}

/*
 * Print a warning if a definition/declaration does not match another
 * definition/declaration of the same name. For functions, only the
 * types of return values are tested.
 */
static void
chkvtdi(hte, def, decl)
	hte_t	*hte;
	sym_t	*def, *decl;
{
	sym_t	*sym;
	type_t	*tp1, *tp2;
	/* LINTED (automatic hides external declaration: warn) */
	int	eq, warn;
	char	*pos1;

	if (def == NULL)
		def = decl;
	if (def == NULL)
		return;

	tp1 = TP(def->s_type);
	for (sym = hte->h_syms; sym != NULL; sym = sym->s_nxt) {
		if (sym == def)
			continue;
		tp2 = TP(sym->s_type);
		warn = 0;
		if (tp1->t_tspec == FUNC && tp2->t_tspec == FUNC) {
			eq = eqtype(tp1->t_subt, tp2->t_subt, 1, 0, 0, &warn);
		} else {
			eq = eqtype(tp1, tp2, 0, 0, 0, &warn);
		}
		if (!eq || (sflag && warn)) {
			pos1 = xstrdup(mkpos(&def->s_pos));
			/* %s value declared inconsistently\t%s  ::  %s */
			msg(5, hte->h_name, pos1, mkpos(&sym->s_pos));
			free(pos1);
		}
	}
}

/*
 * Print a warning if a function is called with arguments which does
 * not match the function definition, declaration or another call
 * of the same function.
 */
static void
chkfaui(hte, def, decl)
	hte_t	*hte;
	sym_t	*def, *decl;
{
	type_t	*tp1, *tp2, **ap1, **ap2;
	pos_t	*pos1p;
	fcall_t	*calls, *call, *call1;
	int	n, as;
	char	*pos1;
	arginf_t *ai;

	if ((calls = hte->h_calls) == NULL)
		return;

	/*
	 * If we find a function definition, we use this for comparision,
	 * otherwise the first prototype we can find. If there is no
	 * definition or prototype declaration, the first function call
	 * is used.
	 */
	tp1 = NULL;
	call1 = NULL;
	if (def != NULL) {
		if ((tp1 = TP(def->s_type))->t_tspec != FUNC)
			return;
		pos1p = &def->s_pos;
	} else if (decl != NULL && TP(decl->s_type)->t_proto) {
		if ((tp1 = TP(decl->s_type))->t_tspec != FUNC)
			return;
		pos1p = &decl->s_pos;
	}
	if (tp1 == NULL) {
		call1 = calls;
		calls = calls->f_nxt;
		if ((tp1 = TP(call1->f_type))->t_tspec != FUNC)
			return;
		pos1p = &call1->f_pos;
	}

	n = 1;
	for (call = calls; call != NULL; call = call->f_nxt) {
		if ((tp2 = TP(call->f_type))->t_tspec != FUNC)
			continue;
		ap1 = tp1->t_args;
		ap2 = tp2->t_args;
		n = 0;
		while (*ap1 != NULL && *ap2 != NULL) {
			if (def != NULL && def->s_va && n >= def->s_nva)
				break;
			n++;
			chkau(hte, n, def, decl, pos1p, call1, call,
			      *ap1, *ap2);
			ap1++;
			ap2++;
		}
		if (*ap1 == *ap2) {
			/* equal # of arguments */
		} else if (def != NULL && def->s_va && n >= def->s_nva) {
			/*
			 * function definition with VARARGS; The # of
			 * arguments of the call must be at least as large
			 * as the parameter of VARARGS.
			 */
		} else if (*ap2 != NULL && tp1->t_proto && tp1->t_vararg) {
			/*
			 * prototype with ... and function call with
			 * at least the same # of arguments as declared
			 * in the prototype.
			 */
		} else {
			pos1 = xstrdup(mkpos(pos1p));
			/* %s: variable # of args\t%s  ::  %s */
			msg(7, hte->h_name, pos1, mkpos(&call->f_pos));
			free(pos1);
			continue;
		}

		/* perform SCANFLIKE/PRINTFLIKE tests */
		if (def == NULL || (!def->s_prfl && !def->s_scfl))
			continue;
		as = def->s_prfl ? def->s_nprfl : def->s_nscfl;
		for (ai = call->f_args; ai != NULL; ai = ai->a_nxt) {
			if (ai->a_num == as)
				break;
		}
		if (ai == NULL || !ai->a_fmt)
			continue;
		if (def->s_prfl) {
			printflike(hte, call, n, ai->a_fstrg, ap2);
		} else {
			scanflike(hte, call, n, ai->a_fstrg, ap2);
		}
	}
}

/*
 * Check a single argument in a function call.
 *
 *  hte		a pointer to the hash table entry of the function
 *  n		the number of the argument (1..)
 *  def		the function definition or NULL
 *  decl	prototype declaration, old style declaration or NULL
 *  pos1p	position of definition, declaration of first call
 *  call1	first call, if both def and decl are old style def/decl
 *  call	checked call
 *  arg1	currently checked argument of def/decl/call1
 *  arg2	currently checked argument of call
 *
 */
static void
chkau(hte, n, def, decl, pos1p, call1, call, arg1, arg2)
	hte_t	*hte;
	int	n;
	sym_t	*def, *decl;
	pos_t	*pos1p;
	fcall_t	*call1, *call;
	type_t	*arg1, *arg2;
{
	/* LINTED (automatic hides external declaration: warn) */
	int	promote, asgn, warn;
	tspec_t	t1, t2;
	arginf_t *ai, *ai1;
	char	*pos1;

	/*
	 * If a function definition is available (def != NULL), we compair the
	 * function call (call) with the definition. Otherwise, if a function
	 * definition is available and it is not an old style definition
	 * (decl != NULL && TP(decl->s_type)->t_proto), we compair the call
	 * with this declaration. Otherwise we compair it with the first
	 * call we have found (call1).
	 */

	/* arg1 must be promoted if it stems from an old style definition */
	promote = def != NULL && def->s_osdef;

	/*
	 * If we compair with a definition or declaration, we must perform
	 * the same checks for qualifiers in indirected types as in
	 * assignments.
	 */
	asgn = def != NULL || (decl != NULL && TP(decl->s_type)->t_proto);

	warn = 0;
	if (eqtype(arg1, arg2, 1, promote, asgn, &warn) && (!sflag || !warn))
		return;

	/*
	 * Other lint implementations print warnings as soon as the type
	 * of an argument does not match exactly the expected type. The
	 * result are lots of warnings which are really not neccessary.
	 * We print a warning only if
	 *   (0) at least one type is not an interger type and types differ
	 *   (1) hflag is set and types differ
	 *   (2) types differ, except in signedness
	 * If the argument is an integer constant whose msb is not set,
	 * signedness is ignored (e.g. 0 matches both signed and unsigned
	 * int). This is with and without hflag.
	 * If the argument is an integer constant with value 0 and the
	 * expected argument is of type pointer and the width of the
	 * interger constant is the same as the width of the pointer,
	 * no warning is printed.
	 */
	t1 = arg1->t_tspec;
	t2 = arg2->t_tspec;
	if (isityp(t1) && isityp(t2) && !arg1->t_isenum && !arg2->t_isenum) {
		if (promote) {
			/*
			 * XXX Here is a problem: Althrough it is possible to
			 * pass an int where a char/short it expected, there
			 * may be loss in significant digits. We should first
			 * check for const arguments if they can be converted
			 * into the original parameter type.
			 */
			if (t1 == FLOAT) {
				t1 = DOUBLE;
			} else if (t1 == CHAR || t1 == SCHAR) {
				t1 = INT;
			} else if (t1 == UCHAR) {
				t1 = tflag ? UINT : INT;
			} else if (t1 == SHORT) {
				t1 = INT;
			} else if (t1 == USHORT) {
				/* CONSTCOND */
				t1 = INT_MAX < USHRT_MAX || tflag ? UINT : INT;
			}
		}

		if (styp(t1) == styp(t2)) {

			/*
			 * types differ only in signedness; get information
			 * about arguments
			 */

			/*
			 * treat a definition like a call with variable
			 * arguments
			 */
			ai1 = call1 != NULL ? call1->f_args : NULL;

			/*
			 * if two calls are compared, ai1 is set to the
			 * information for the n-th argument, if this was
			 * a constant, otherwise to NULL
			 */
			for ( ; ai1 != NULL; ai1 = ai1->a_nxt) {
				if (ai1->a_num == n)
					break;
			}
			/*
			 * ai is set to the information of the n-th arg
			 * of the (second) call, if this was a constant,
			 * otherwise to NULL
			 */
			for (ai = call->f_args; ai != NULL; ai = ai->a_nxt) {
				if (ai->a_num == n)
					break;
			}

			if (ai1 == NULL && ai == NULL) {
				/* no constant at all */
				if (!hflag)
					return;
			} else if (ai1 == NULL || ai == NULL) {
				/* one constant */
				if (ai == NULL)
					ai = ai1;
				if (ai->a_zero || ai->a_pcon)
					/* same value in signed and unsigned */
					return;
				/* value (not representation) differently */
			} else {
				/*
				 * two constants, one signed, one unsigned;
				 * if the msb of one of the constants is set,
				 * the argument is used inconsistently.
				 */
				if (!ai1->a_ncon && !ai->a_ncon)
					return;
			}
		}

	} else if (t1 == PTR && isityp(t2) && psize(t1) == psize(t2)) {
		for (ai = call->f_args; ai != NULL; ai = ai->a_nxt) {
			if (ai->a_num == n)
				break;
		}
		if (ai != NULL && ai->a_zero)
			return;
	}

	pos1 = xstrdup(mkpos(pos1p));
	/* %s, arg %d used inconsistently\t%s  ::  %s */
	msg(6, hte->h_name, n, pos1, mkpos(&call->f_pos));
	free(pos1);
}

/*
 * Compare the types in the NULL-terminated array ap with the format
 * string fmt.
 */
static void
printflike(hte, call, n, fmt, ap)
	hte_t	*hte;
	fcall_t	*call;
	int	n;
	const	char *fmt;
	type_t	**ap;
{
	const	char *fp;
	int	fc;
	int	fwidth, prec, left, sign, space, alt, zero;
	tspec_t	sz, t1, t2;
	type_t	*tp;

	fp = fmt;
	fc = *fp++;

	for ( ; ; ) {
		if (fc == '\0') {
			if (*ap != NULL)
				tomanyarg(hte, call);
			break;
		}
		if (fc != '%') {
			badfmt(hte, call);
			break;
		}
		fc = *fp++;
		fwidth = prec = left = sign = space = alt = zero = 0;
		sz = NOTSPEC;

		/* Flags */
		for ( ; ; ) {
			if (fc == '-') {
				if (left)
					break;
				left = 1;
			} else if (fc == '+') {
				if (sign)
					break;
				sign = 1;
			} else if (fc == ' ') {
				if (space)
					break;
				space = 1;
			} else if (fc == '#') {
				if (alt)
					break;
				alt = 1;
			} else if (fc == '0') {
				if (zero)
					break;
				zero = 1;
			} else {
				break;
			}
			fc = *fp++;
		}

		/* field width */
		if (isdigit(fc)) {
			fwidth = 1;
			do { fc = *fp++; } while (isdigit(fc)) ;
		} else if (fc == '*') {
			fwidth = 1;
			fc = *fp++;
			if ((tp = *ap++) == NULL) {
				tofewarg(hte, call);
				break;
			}
			n++;
			if ((t1 = tp->t_tspec) != INT && (hflag || t1 != UINT))
				inconarg(hte, call, n);
		}

		/* precision */
		if (fc == '.') {
			fc = *fp++;
			prec = 1;
			if (isdigit(fc)) {
				do { fc = *fp++; } while (isdigit(fc));
			} else if (fc == '*') {
				fc = *fp++;
				if ((tp = *ap++) == NULL) {
					tofewarg(hte, call);
					break;
				}
				n++;
				if (tp->t_tspec != INT)
					inconarg(hte, call, n);
			} else {
				badfmt(hte, call);
				break;
			}
		}

		if (fc == 'h') {
			sz = SHORT;
		} else if (fc == 'l') {
			sz = LONG;
		} else if (fc == 'q') {
			sz = QUAD;
		} else if (fc == 'L') {
			sz = LDOUBLE;
		}
		if (sz != NOTSPEC)
			fc = *fp++;

		if (fc == '%') {
			if (sz != NOTSPEC || left || sign || space ||
			    alt || zero || prec || fwidth) {
				badfmt(hte, call);
			}
			fc = *fp++;
			continue;
		}

		if (fc == '\0') {
			badfmt(hte, call);
			break;
		}

		if ((tp = *ap++) == NULL) {
			tofewarg(hte, call);
			break;
		}
		n++;
		if ((t1 = tp->t_tspec) == PTR)
			t2 = tp->t_subt->t_tspec;

		if (fc == 'd' || fc == 'i') {
			if (alt || sz == LDOUBLE) {
				badfmt(hte, call);
				break;
			}
		int_conv:
			if (sz == LONG) {
				if (t1 != LONG && (hflag || t1 != ULONG))
					inconarg(hte, call, n);
			} else if (sz == QUAD) {
				if (t1 != QUAD && (hflag || t1 != UQUAD))
					inconarg(hte, call, n);
			} else {
				/*
				 * SHORT is always promoted to INT, USHORT
				 * to INT or UINT.
				 */
				if (t1 != INT && (hflag || t1 != UINT))
					inconarg(hte, call, n);
			}
		} else if (fc == 'o' || fc == 'u' || fc == 'x' || fc == 'X') {
			if ((alt && fc == 'u') || sz == LDOUBLE)
				badfmt(hte, call);
		uint_conv:
			if (sz == LONG) {
				if (t1 != ULONG && (hflag || t1 != LONG))
					inconarg(hte, call, n);
			} else if (sz == QUAD) {
				if (t1 != UQUAD && (hflag || t1 != QUAD))
					inconarg(hte, call, n);
			} else if (sz == SHORT) {
				/* USHORT was promoted to INT or UINT */
				if (t1 != UINT && t1 != INT)
					inconarg(hte, call, n);
			} else {
				if (t1 != UINT && (hflag || t1 != INT))
					inconarg(hte, call, n);
			}
		} else if (fc == 'D' || fc == 'O' || fc == 'U') {
			if ((alt && fc != 'O') || sz != NOTSPEC || !tflag)
				badfmt(hte, call);
			sz = LONG;
			if (fc == 'D') {
				goto int_conv;
			} else {
				goto uint_conv;
			}
		} else if (fc == 'f' || fc == 'e' || fc == 'E' ||
			   fc == 'g' || fc == 'G') {
			if (sz == NOTSPEC)
				sz = DOUBLE;
			if (sz != DOUBLE && sz != LDOUBLE)
				badfmt(hte, call);
			if (t1 != sz)
				inconarg(hte, call, n);
		} else if (fc == 'c') {
			if (sz != NOTSPEC || alt || zero)
				badfmt(hte, call);
			if (t1 != INT)
				inconarg(hte, call, n);
		} else if (fc == 's') {
			if (sz != NOTSPEC || alt || zero)
				badfmt(hte, call);
			if (t1 != PTR ||
			    (t2 != CHAR && t2 != UCHAR && t2 != SCHAR)) {
				inconarg(hte, call, n);
			}
		} else if (fc == 'p') {
			if (fwidth || prec || sz != NOTSPEC || alt || zero)
				badfmt(hte, call);
			if (t1 != PTR || (hflag && t2 != VOID))
				inconarg(hte, call, n);
		} else if (fc == 'n') {
			if (fwidth || prec || alt || zero || sz == LDOUBLE)
				badfmt(hte, call);
			if (t1 != PTR) {
				inconarg(hte, call, n);
			} else if (sz == LONG) {
				if (t2 != LONG && t2 != ULONG)
					inconarg(hte, call, n);
			} else if (sz == SHORT) {
				if (t2 != SHORT && t2 != USHORT)
					inconarg(hte, call, n);
			} else {
				if (t2 != INT && t2 != UINT)
					inconarg(hte, call, n);
			}
		} else {
			badfmt(hte, call);
			break;
		}

		fc = *fp++;
	}
}

/*
 * Compare the types in the NULL-terminated array ap with the format
 * string fmt.
 */
static void
scanflike(hte, call, n, fmt, ap)
	hte_t	*hte;
	fcall_t	*call;
	int	n;
	const	char *fmt;
	type_t	**ap;
{
	const	char *fp;
	int	fc;
	int	noasgn, fwidth;
	tspec_t	sz, t1, t2;
	type_t	*tp;

	fp = fmt;
	fc = *fp++;

	for ( ; ; ) {
		if (fc == '\0') {
			if (*ap != NULL)
				tomanyarg(hte, call);
			break;
		}
		if (fc != '%') {
			badfmt(hte, call);
			break;
		}
		fc = *fp++;

		noasgn = fwidth = 0;
		sz = NOTSPEC;

		if (fc == '*') {
			noasgn = 1;
			fc = *fp++;
		}
		
		if (isdigit(fc)) {
			fwidth = 1;
			do { fc = *fp++; } while (isdigit(fc));
		}

		if (fc == 'h') {
			sz = SHORT;
		} else if (fc == 'l') {
			sz = LONG;
		} else if (fc == 'q') {
			sz = QUAD;
		} else if (fc == 'L') {
			sz = LDOUBLE;
		}
		if (sz != NOTSPEC)
			fc = *fp++;

		if (fc == '%') {
			if (sz != NOTSPEC || noasgn || fwidth)
				badfmt(hte, call);
			fc = *fp++;
			continue;
		}

		if (!noasgn) {
			if ((tp = *ap++) == NULL) {
				tofewarg(hte, call);
				break;
			}
			n++;
			if ((t1 = tp->t_tspec) == PTR)
				t2 = tp->t_subt->t_tspec;
		}

		if (fc == 'd' || fc == 'i' || fc == 'n') {
			if (sz == LDOUBLE)
				badfmt(hte, call);
			if (sz != SHORT && sz != LONG && sz != QUAD)
				sz = INT;
		conv:
			if (!noasgn) {
				if (t1 != PTR) {
					inconarg(hte, call, n);
				} else if (t2 != styp(sz)) {
					inconarg(hte, call, n);
				} else if (hflag && t2 != sz) {
					inconarg(hte, call, n);
				} else if (tp->t_subt->t_const) {
					inconarg(hte, call, n);
				}
			}
		} else if (fc == 'o' || fc == 'u' || fc == 'x') {
			if (sz == LDOUBLE)
				badfmt(hte, call);
			if (sz == SHORT) {
				sz = USHORT;
			} else if (sz == LONG) {
				sz = ULONG;
			} else if (sz == QUAD) {
				sz = UQUAD;
			} else {
				sz = UINT;
			}
			goto conv;
		} else if (fc == 'D') {
			if (sz != NOTSPEC || !tflag)
				badfmt(hte, call);
			sz = LONG;
			goto conv;
		} else if (fc == 'O') {
			if (sz != NOTSPEC || !tflag)
				badfmt(hte, call);
			sz = ULONG;
			goto conv;
		} else if (fc == 'X') {
			/*
			 * XXX valid in ANSI C, but in NetBSD's libc imple-
			 * mented as "lx". Thats why it should be avoided.
			 */
			if (sz != NOTSPEC || !tflag)
				badfmt(hte, call);
			sz = ULONG;
			goto conv;
		} else if (fc == 'E') {
			/*
			 * XXX valid in ANSI C, but in NetBSD's libc imple-
			 * mented as "lf". Thats why it should be avoided.
			 */
			if (sz != NOTSPEC || !tflag)
				badfmt(hte, call);
			sz = DOUBLE;
			goto conv;
		} else if (fc == 'F') {
			/* XXX only for backward compatibility */
			if (sz != NOTSPEC || !tflag)
				badfmt(hte, call);
			sz = DOUBLE;
			goto conv;
		} else if (fc == 'G') {
			/*
			 * XXX valid in ANSI C, but in NetBSD's libc not
			 * implemented
			 */
			if (sz != NOTSPEC && sz != LONG && sz != LDOUBLE)
				badfmt(hte, call);
			goto fconv;
		} else if (fc == 'e' || fc == 'f' || fc == 'g') {
		fconv:
			if (sz == NOTSPEC) {
				sz = FLOAT;
			} else if (sz == LONG) {
				sz = DOUBLE;
			} else if (sz != LDOUBLE) {
				badfmt(hte, call);
				sz = FLOAT;
			}
			goto conv;
		} else if (fc == 's' || fc == '[' || fc == 'c') {
			if (sz != NOTSPEC)
				badfmt(hte, call);
			if (fc == '[') {
				if ((fc = *fp++) == '-') {
					badfmt(hte, call);
					fc = *fp++;
				}
				if (fc != ']') {
					badfmt(hte, call);
					if (fc == '\0')
						break;
				}
			}
			if (!noasgn) {
				if (t1 != PTR) {
					inconarg(hte, call, n);
				} else if (t2 != CHAR && t2 != UCHAR &&
					   t2 != SCHAR) {
					inconarg(hte, call, n);
				}
			}
		} else if (fc == 'p') {
			if (sz != NOTSPEC)
				badfmt(hte, call);
			if (!noasgn) {
				if (t1 != PTR || t2 != PTR) {
					inconarg(hte, call, n);
				} else if (tp->t_subt->t_subt->t_tspec!=VOID) {
					if (hflag)
						inconarg(hte, call, n);
				}
			}
		} else {
			badfmt(hte, call);
			break;
		}

		fc = *fp++;
	}
}

static void
badfmt(hte, call)
	hte_t	*hte;
	fcall_t	*call;
{
	/* %s: malformed format string\t%s */
	msg(13, hte->h_name, mkpos(&call->f_pos));
}

static void
inconarg(hte, call, n)
	hte_t	*hte;
	fcall_t	*call;
	int	n;
{
	/* %s, arg %d inconsistent with format\t%s(%d) */
	msg(14, hte->h_name, n, mkpos(&call->f_pos));
}

static void
tofewarg(hte, call)
	hte_t	*hte;
	fcall_t	*call;
{
	/* %s: too few args for format  \t%s */
	msg(15, hte->h_name, mkpos(&call->f_pos));
}

static void
tomanyarg(hte, call)
	hte_t	*hte;
	fcall_t	*call;
{
	/* %s: too many args for format  \t%s */
	msg(16, hte->h_name, mkpos(&call->f_pos));
}


/*
 * Print warnings for return values which are used, but not returned,
 * or return values which are always or sometimes ignored.
 */
static void
chkrvu(hte, def)
	hte_t	*hte;
	sym_t	*def;
{
	fcall_t	*call;
	int	used, ignored;

	if (def == NULL)
		/* don't know wheter or not the functions returns a value */
		return;

	if (hte->h_calls == NULL)
		return;

	if (def->s_rval) {
		/* function has return value */
		used = ignored = 0;
		for (call = hte->h_calls; call != NULL; call = call->f_nxt) {
			used |= call->f_rused;
			ignored |= !call->f_rused && !call->f_rdisc;
		}
		/*
		 * XXX as soon as we are able to disable single warnings
		 * the following dependencies from hflag should be removed.
		 * but for now I do'nt want to be botherd by this warnings
		 * which are almost always useless.
		 */
		if (!used && ignored) {
			if (hflag)
				/* %s returns value which is always ignored */
				msg(8, hte->h_name);
		} else if (used && ignored) {
			if (hflag)
				/* %s returns value which is sometimes ign. */
				msg(9, hte->h_name);
		}
	} else {
		/* function has no return value */
		for (call = hte->h_calls; call != NULL; call = call->f_nxt) {
			if (call->f_rused)
				/* %s value is used( %s ), but none ret. */
				msg(10, hte->h_name, mkpos(&call->f_pos));
		}
	}
}

/*
 * Print warnings for inconsistent argument declarations.
 */
static void
chkadecl(hte, def, decl)
	hte_t	*hte;
	sym_t	*def, *decl;
{
	/* LINTED (automatic hides external declaration: warn) */
	int	osdef, eq, warn, n;
	sym_t	*sym1, *sym;
	type_t	**ap1, **ap2, *tp1, *tp2;
	char	*pos1;
	const	char *pos2;

	osdef = 0;
	if (def != NULL) {
		osdef = def->s_osdef;
		sym1 = def;
	} else if (decl != NULL && TP(decl->s_type)->t_proto) {
		sym1 = decl;
	} else {
		return;
	}
	if (TP(sym1->s_type)->t_tspec != FUNC)
		return;

	/*
	 * XXX Prototypes should also be compared with old style function
	 * declarations.
	 */

	for (sym = hte->h_syms; sym != NULL; sym = sym->s_nxt) {
		if (sym == sym1 || !TP(sym->s_type)->t_proto)
			continue;
		ap1 = TP(sym1->s_type)->t_args;
		ap2 = TP(sym->s_type)->t_args;
		n = 0;
		while (*ap1 != NULL && *ap2 != NULL) {
			warn = 0;
			eq = eqtype(*ap1, *ap2, 1, osdef, 0, &warn);
			if (!eq || warn) {
				pos1 = xstrdup(mkpos(&sym1->s_pos));
				pos2 = mkpos(&sym->s_pos);
				/* %s, arg %d declared inconsistently ... */
				msg(11, hte->h_name, n + 1, pos1, pos2);
				free(pos1);
			}
			n++;
			ap1++;
			ap2++;
		}
		if (*ap1 == *ap2) {
			tp1 = TP(sym1->s_type);
			tp2 = TP(sym->s_type);
			if (tp1->t_vararg == tp2->t_vararg)
				continue;
			if (tp2->t_vararg &&
			    sym1->s_va && sym1->s_nva == n && !sflag) {
				continue;
			}
		}
		/* %s: variable # of args declared\t%s  ::  %s */
		pos1 = xstrdup(mkpos(&sym1->s_pos));
		msg(12, hte->h_name, pos1, mkpos(&sym->s_pos));
		free(pos1);
	}
}


/*
 * Check compatibility of two types. Returns 1 if types are compatible,
 * otherwise 0.
 *
 * ignqual	if set, ignore qualifiers of outhermost type; used for
 *		function arguments
 * promote	if set, promote left type before comparision; used for
 *		comparisions of arguments with parameters of old style
 *		definitions
 * asgn		left indirected type must have at least the same qualifiers
 *		like right indirected type (for assignments and function
 *		arguments)
 * *warn	set to 1 if an old style declaration was compared with
 *		an incompatible prototype declaration
 */
static int
eqtype(tp1, tp2, ignqual, promot, asgn, warn)
	type_t	*tp1, *tp2;
	int	ignqual, promot, asgn, *warn;
{
	tspec_t	t, to;
	int	indir;

	to = NOTSPEC;
	indir = 0;

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

		if (asgn && to == PTR) {
			if (indir == 1 && (t == VOID || tp2->t_tspec == VOID))
				return (1);
		}
		
		if (t != tp2->t_tspec) {
			/*
			 * Give pointer to types which differ only in
			 * signedness a chance if not sflag and not hflag.
			 */
			if (sflag || hflag || to != PTR)
				return (0);
			if (styp(t) != styp(tp2->t_tspec))
				return (0);
		}

		if (tp1->t_isenum && tp2->t_isenum) {
			if (tp1->t_istag && tp2->t_istag) {
				return (tp1->t_tag == tp2->t_tag);
			} else if (tp1->t_istynam && tp2->t_istynam) {
				return (tp1->t_tynam == tp2->t_tynam);
			} else {
				return (0);
			}
		}

		/*
		 * XXX Handle combinations of enum and int if eflag is set.
		 * But note: enum and 0 should be allowed.
		 */

		if (asgn && indir == 1) {
			if (!tp1->t_const && tp2->t_const)
				return (0);
			if (!tp1->t_volatile && tp2->t_volatile)
				return (0);
		} else if (!ignqual && !tflag) {
			if (tp1->t_const != tp2->t_const)
				return (0);
			if (tp1->t_const != tp2->t_const)
				return (0);
		}

		if (t == STRUCT || t == UNION) {
			if (tp1->t_istag && tp2->t_istag) {
				return (tp1->t_tag == tp2->t_tag);
			} else if (tp1->t_istynam && tp2->t_istynam) {
				return (tp1->t_tynam == tp2->t_tynam);
			} else {
				return (0);
			}
		}

		if (t == ARRAY && tp1->t_dim != tp2->t_dim) {
			if (tp1->t_dim != 0 && tp2->t_dim != 0)
				return (0);
		}

		if (t == FUNC) {
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
		to = t;
		indir++;

	}

	return (tp1 == tp2);
}

/*
 * Compares arguments of two prototypes
 */
static int
eqargs(tp1, tp2, warn)
	type_t	*tp1, *tp2;
	int	*warn;
{
	type_t	**a1, **a2;

	if (tp1->t_vararg != tp2->t_vararg)
		return (0);

	a1 = tp1->t_args;
	a2 = tp2->t_args;

	while (*a1 != NULL && *a2 != NULL) {

		if (eqtype(*a1, *a2, 1, 0, 0, warn) == 0)
			return (0);

		a1++;
		a2++;

	}

	return (*a1 == *a2);
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
	type_t	**arg;
	tspec_t	t;

	if (tp->t_vararg && warn != NULL)
		*warn = 1;
	for (arg = tp->t_args; *arg != NULL; arg++) {
		if ((t = (*arg)->t_tspec) == FLOAT)
			return (0);
		if (t == CHAR || t == SCHAR || t == UCHAR)
			return (0);
		if (t == SHORT || t == USHORT)
			return (0);
	}
	return (1);
}

