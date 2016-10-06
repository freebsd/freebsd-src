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
  
/*	from OpenSolaris "eqnbox.c	1.3	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)eqnbox.c	1.7 (gritter) 1/13/08
 */

/*
 * Changes Copyright (c) 2014 Carsten Kunze (carsten.kunze at arcor.de)
 */

# include "e.h"
#include "y.tab.h"

extern YYSTYPE yyval;

void
eqnbox(int p1, int p2, int lu) {
#ifndef	NEQN
	float b, h;
#else	/* NEQN */
	int b, h;
#endif	/* NEQN */
	char *sh;

	yyval.token = p1;
	b = max(ebase[p1], ebase[p2]);
	eht[yyval.token] = h = b + max(eht[p1]-ebase[p1], 
		eht[p2]-ebase[p2]);
	ebase[yyval.token] = b;
#ifndef	NEQN
	if(dbg)printf(".\te:eb: S%d <- S%d S%d; b=%g, h=%g\n", 
		yyval.token, p1, p2, b, h);
#else	/* NEQN */
	if(dbg)printf(".\te:eb: S%d <- S%d S%d; b=%d, h=%d\n", 
		yyval.token, p1, p2, b, h);
#endif	/* NEQN */
	if (ital(rfont[p1]) && rom(lfont[p2])) {
		if (op(lfont[p2]))
			sh = "\\|";
		else
			sh = "\\^";
	} else
		sh = "";
	if (lu) {
		printf(".nr %d \\w'\\s%s\\*(%d%s'\n", p1, tsize(ps), p1, sh);
		printf(".ds %d \\h'|\\n(97u-\\n(%du'\\*(%d\n", p1, p1, p1);
	}
	printf(".as %d \"%s\\*(%d\n", yyval.token, sh, p2);
	rfont[p1] = rfont[p2];
	ofree(p2);
}
