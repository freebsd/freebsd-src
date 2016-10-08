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
  
/*	from OpenSolaris "integral.c	1.4	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)integral.c	1.5 (gritter) 10/19/06
 */

/*
 * Changes Copyright (c) 2014 Carsten Kunze (carsten.kunze at arcor.de)
 */

#include "e.h"
#include "y.tab.h"

extern YYSTYPE yyval;

void
integral(int p, int p1, int p2) {
#ifndef	NEQN
	if (p1 != 0)
		printf(".ds %d \\h'-0.4m'\\v'0.4m'\\*(%d\\v'-0.4m'\n", p1, p1);
	if (p2 != 0)
		printf(".ds %d \\v'-0.3m'\\*(%d\\v'0.3m'\n", p2, p2);
#endif
	if (p1 != 0 && p2 != 0)
		shift2(p, p1, p2);
	else if (p1 != 0)
		bshiftb(p, SUB, p1);
	else if (p2 != 0)
		bshiftb(p, SUP, p2);
#ifndef	NEQN
	if(dbg)printf(".\tintegral: S%d; h=%g b=%g\n", 
		p, eht[p], ebase[p]);
#else	/* NEQN */
	if(dbg)printf(".\tintegral: S%d; h=%d b=%d\n", 
		p, eht[p], ebase[p]);
#endif	/* NEQN */
	lfont[p] = ROM;
}

void
setintegral(void) {
	char *f;

	yyval.token = oalloc();
	f = "\\(is";
#ifndef NEQN
	printf(".ds %d \\s%s\\v'.1m'\\s+4%s\\s-4\\v'-.1m'\\s%s\n", 
		yyval.token, tsize(ps), f, tsize(ps));
	eht[yyval.token] = VERT(EM(1.15, ps+4));
	ebase[yyval.token] = VERT(EM(0.3, ps));
#else /* NEQN */
	printf(".ds %d %s\n", yyval.token, f);
	eht[yyval.token] = VERT(2);
	ebase[yyval.token] = 0;
#endif /* NEQN */
	lfont[yyval.token] = rfont[yyval.token] = ROM;
}
