/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */
     
/*
 * Copyright (c) 1983, 1984 1985, 1986, 1987, 1988, Sun Microsystems, Inc.
 * All Rights Reserved.
 */
  
/*	from OpenSolaris "lookup.c	1.5	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)lookup.c	1.5 (gritter) 9/18/05
 */

#include "e.h"
#include "y.tab.h"
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#define	TBLSIZE	100

tbl	*keytbl[TBLSIZE];	/* key words */
tbl	*restbl[TBLSIZE];	/* reserved words */
tbl	*deftbl[TBLSIZE];	/* user-defined names */

struct {
	char	*key;
	int	keyval;
} keyword[]	={
	{ "sub", 	SUB },
	{ "sup", 	SUP },
	{ ".EN", 	EOF },
	{ "from", 	FROM },
	{ "to", 	TO },
	{ "sum", 	SUM },
	{ "hat", 	HAT },
	{ "vec", VEC },
	{ "dyad", DYAD },
	{ "dot", 	DOT },
	{ "dotdot", 	DOTDOT },
	{ "bar", 	BAR },
	{ "tilde", 	TILDE },
	{ "under", 	UNDER },
	{ "prod", 	PROD },
	{ "int", 	INT },
	{ "integral", 	INT },
	{ "union", 	UNION },
	{ "inter", 	INTER },
	{ "pile", 	PILE },
	{ "lpile", 	LPILE },
	{ "cpile", 	CPILE },
	{ "rpile", 	RPILE },
	{ "over", 	OVER },
	{ "sqrt", 	SQRT },
	{ "above", 	ABOVE },
	{ "size", 	SIZE },
	{ "font", 	FONT },
	{ "fat", FAT },
	{ "roman", 	ROMAN },
	{ "italic", 	ITALIC },
	{ "bold", 	BOLD },
	{ "left", 	LEFT },
	{ "right", 	RIGHT },
	{ "delim", 	DELIM },
	{ "define", 	DEFINE },

#ifdef	NEQN	/* make ndefine synonym for define, tdefine a no-op */

	{ "tdefine",	TDEFINE },
	{ "ndefine",	DEFINE },

#else		/* tdefine = define, ndefine = no-op */

	{ "tdefine", 	DEFINE },
	{ "ndefine", 	NDEFINE },

#endif

	{ "gsize", 	GSIZE },
	{ ".gsize", 	GSIZE },
	{ "gfont", 	GFONT },
	{ "include", 	INCLUDE },
	{ "up", 	UP },
	{ "down", 	DOWN },
	{ "fwd", 	FWD },
	{ "back", 	BACK },
	{ "mark", 	MARK },
	{ "lineup", 	LINEUP },
	{ "matrix", 	MATRIX },
	{ "col", 	COL },
	{ "lcol", 	LCOL },
	{ "ccol", 	CCOL },
	{ "rcol", 	RCOL },
	{ NULL, 	0 }
};

struct {
	char	*res;
	char	*resval;
} resword[]	={
	{ ">=",	"\\(>=" },
	{ "<=",	"\\(<=" },
	{ "==",	"\\(==" },
	{ "!=",	"\\(!=" },
	{ "+-",	"\\(+-" },
	{ "->",	"\\(->" },
	{ "<-",	"\\(<-" },
	{ "inf",	"\\(if" },
	{ "infinity",	"\\(if" },
	{ "partial",	"\\(pd" },
	{ "half",	"\\f1\\(12\\fP" },
	{ "prime",	"\\f1\\(fm\\fP" },
	{ "dollar",	"\\f1$\\fP" },
	{ "nothing",	"" },
	{ "times",	"\\(mu" },
	{ "del",	"\\(gr" },
	{ "grad",	"\\(gr" },
#ifdef	NEQN
	{ "<<",	"<<" },
	{ ">>",	">>" },
	{ "approx",	"~\b\\d~\\u" },
	{ "cdot",	"\\v'-.5'.\\v'.5'" },
	{ "...",	"..." },
	{ ",...,",	",...," },
#else
	{ "<<",	"<\\h'-.3m'<" },
	{ ">>",	">\\h'-.3m'>" },
	{ "approx",	"\\v'-.2m'\\z\\(ap\\v'.25m'\\(ap\\v'-.05m'" },
	{ "cdot",	"\\v'-.3m'.\\v'.3m'" },
	{ "...",	"\\v'-.3m'\\ .\\ .\\ .\\ \\v'.3m'" },
	{ ",...,",	",\\ .\\ .\\ .\\ ,\\|" },
#endif

	{ "alpha",	"\\(*a" },
	{ "ALPHA",	"\\(*A" },
	{ "beta",	"\\(*b" },
	{ "BETA",	"\\(*B" },
	{ "gamma",	"\\(*g" },
	{ "GAMMA",	"\\(*G" },
	{ "delta",	"\\(*d" },
	{ "DELTA",	"\\(*D" },
	{ "epsilon",	"\\(*e" },
	{ "EPSILON",	"\\(*E" },
	{ "omega",	"\\(*w" },
	{ "OMEGA",	"\\(*W" },
	{ "lambda",	"\\(*l" },
	{ "LAMBDA",	"\\(*L" },
	{ "mu",	"\\(*m" },
	{ "MU",	"\\(*M" },
	{ "nu",	"\\(*n" },
	{ "NU",	"\\(*N" },
	{ "theta",	"\\(*h" },
	{ "THETA",	"\\(*H" },
	{ "phi",	"\\(*f" },
	{ "PHI",	"\\(*F" },
	{ "pi",	"\\(*p" },
	{ "PI",	"\\(*P" },
	{ "sigma",	"\\(*s" },
	{ "SIGMA",	"\\(*S" },
	{ "xi",	"\\(*c" },
	{ "XI",	"\\(*C" },
	{ "zeta",	"\\(*z" },
	{ "ZETA",	"\\(*Z" },
	{ "iota",	"\\(*i" },
	{ "IOTA",	"\\(*I" },
	{ "eta",	"\\(*y" },
	{ "ETA",	"\\(*Y" },
	{ "kappa",	"\\(*k" },
	{ "KAPPA",	"\\(*K" },
	{ "rho",	"\\(*r" },
	{ "RHO",	"\\(*R" },
	{ "tau",	"\\(*t" },
	{ "TAU",	"\\(*T" },
	{ "omicron",	"\\(*o" },
	{ "OMICRON",	"\\(*O" },
	{ "upsilon",	"\\(*u" },
	{ "UPSILON",	"\\(*U" },
	{ "psi",	"\\(*q" },
	{ "PSI",	"\\(*Q" },
	{ "chi",	"\\(*x" },
	{ "CHI",	"\\(*X" },
	{ "and",	"\\f1and\\fP" },
	{ "for",	"\\f1for\\fP" },
	{ "if",	"\\f1if\\fP" },
	{ "Re",	"\\f1Re\\fP" },
	{ "Im",	"\\f1Im\\fP" },
	{ "sin",	"\\f1sin\\fP" },
	{ "cos",	"\\f1cos\\fP" },
	{ "tan",	"\\f1tan\\fP" },
	{ "sec",  "\\f1sec\\fP" },
	{ "csc",  "\\f1csc\\fP" },
	{ "arc",	"\\f1arc\\fP" },
	{ "asin", "\\f1asin\\fP" },
	{ "acos", "\\f1acos\\fP" },
	{ "atan", "\\f1atan\\fP" },
	{ "asec", "\\f1asec\\fP" },
	{ "acsc", "\\f1acsc\\fP" },
	{ "sinh",	"\\f1sinh\\fP" },
	{ "coth",	"\\f1coth\\fP" },
	{ "tanh",	"\\f1tanh\\fP" },
	{ "cosh",	"\\f1cosh\\fP" },
	{ "lim",	"\\f1lim\\fP" },
	{ "log",	"\\f1log\\fP" },
	{ "max",	"\\f1max\\fP" },
	{ "min",	"\\f1min\\fP" },
	{ "ln",	"\\f1ln\\fP" },
	{ "exp",	"\\f1exp\\fP" },
	{ "det",	"\\f1det\\fP" },
	{ NULL,	NULL }
};

tbl *
lookup(tbl **tblp, char *name, char *defn)	/* find name in tbl. if defn non-null, install */
{
	register tbl *p;
	register int h;
	register unsigned char *s = (unsigned char *)name;

	for (h = 0; *s != '\0'; )
		h += *s++;
	h %= TBLSIZE;

	for (p = tblp[h]; p != NULL; p = p->next)
		if (strcmp(name, p->name) == 0) {	/* found it */
			if (defn != NULL)
				p->defn = defn;
			return(p);
		}
	/* didn't find it */
	if (defn == NULL)
		return(NULL);
	p = (tbl *) malloc(sizeof (tbl));
	if (p == NULL)
		error(FATAL, "out of space in lookup");
	p->name = name;
	p->defn = defn;
	p->next = tblp[h];
	tblp[h] = p;
	return(p);
}

void
init_tbl(void)	/* initialize all tables */
{
	int i;

	for (i = 0; keyword[i].key != NULL; i++)
		lookup(keytbl, keyword[i].key, (char *)(intptr_t)keyword[i].keyval);
	for (i = 0; resword[i].res != NULL; i++)
		lookup(restbl, resword[i].res, resword[i].resval);
}
