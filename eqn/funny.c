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
  
/*	from OpenSolaris "funny.c	1.6	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)funny.c	1.6 (gritter) 10/19/06
 */

/*
 * Changes Copyright (c) 2014 Carsten Kunze (carsten.kunze at arcor.de)
 */

#include "e.h"
#include "y.tab.h"

extern YYSTYPE yyval;

void
funny(int n) {
	char *f = NULL;

	yyval.token = oalloc();
	switch(n) {
	case SUM:
		f = "\\(*S"; break;
	case UNION:
		f = "\\(cu"; break;
	case INTER:	/* intersection */
		f = "\\(ca"; break;
	case PROD:
		f = "\\(*P"; break;
	default:
		error(FATAL, "funny type %d in funny", n);
	}
#ifndef NEQN
	printf(".ds %d \\s%s\\v'.3m'\\s+5%s\\s-5\\v'-.3m'\\s%s\n", yyval.token, tsize(ps), f, tsize(ps));
	eht[yyval.token] = VERT(EM(1.0, ps+5) - EM(0.2, ps));
	ebase[yyval.token] = VERT(EM(0.3, ps));
	if(dbg)printf(".\tfunny: S%d <- %s; h=%g b=%g\n", 
		yyval.token, f, eht[yyval.token], ebase[yyval.token]);
#else /* NEQN */
	printf(".ds %d %s\n", yyval.token, f);
	eht[yyval.token] = VERT(2);
	ebase[yyval.token] = 0;
	if(dbg)printf(".\tfunny: S%d <- %s; h=%d b=%d\n", 
		yyval.token, f, eht[yyval.token], ebase[yyval.token]);
#endif /* NEQN */
	lfont[yyval.token] = rfont[yyval.token] = ROM;
}
