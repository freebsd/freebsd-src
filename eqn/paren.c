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
  
/*	from OpenSolaris "paren.c	1.5	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)paren.c	1.4 (gritter) 10/29/05
 */

/*
 * Changes Copyright (c) 2014 Carsten Kunze (carsten.kunze at arcor.de)
 */

# include "e.h"
#include "y.tab.h"

extern YYSTYPE yyval;

void
paren(int leftc, int p1, int rightc) {
	int n, m, j;
#ifndef	NEQN
	float v, h1, b1;
#else	/* NEQN */
	int v, h1, b1;
#endif	/* NEQN */
	h1 = eht[p1]; b1 = ebase[p1];
	yyval.token = p1;
#ifndef NEQN
	lfont[yyval.token] = rfont[yyval.token] = 0;
	n = (h1 + EM(1.0, EFFPS(ps)) - 1) / EM(1.0, EFFPS(ps));
#else /* NEQN */
	n = max(b1+VERT(1), h1-b1-VERT(1)) / VERT(1);
#endif /* NEQN */
	if( n<2 ) n = 1;
	m = n-2;
	if (leftc=='{' || rightc == '}') {
		n = n%2 ? n : n+1;
		if( n<3 ) n=3;
		m = n-3;
	}
#ifndef NEQN
	eht[yyval.token] = VERT(EM(n, ps));
	ebase[yyval.token] = b1 + (eht[yyval.token]-h1)/2;
	v = b1 - h1/2 + VERT(EM(0.4, ps));
	printf(".ds %d \\|\\v'%gp'", yyval.token, v);
#else /* NEQN */
	eht[yyval.token] = VERT(2 * n);
	ebase[yyval.token] = (n)/2 * VERT(2);
	if (n%2 == 0)
		ebase[yyval.token] -= VERT(1);
	v = b1 - h1/2 + VERT(1);
	printf(".ds %d \\|\\v'%du'", yyval.token, v);
#endif /* NEQN */
	switch( leftc ) {
		case 'n':	/* nothing */
		case '\0':
			break;
		case 'f':	/* floor */
			if (n <= 1)
				printf("\\(lf");
			else
				brack(m, "\\(bv", "\\(bv", "\\(lf");
			break;
		case 'c':	/* ceiling */
			if (n <= 1)
				printf("\\(lc");
			else
				brack(m, "\\(lc", "\\(bv", "\\(bv");
			break;
		case '{':
			printf("\\b'\\(lt");
			for(j = 0; j < m; j += 2) printf("\\(bv");
			printf("\\(lk");
			for(j = 0; j < m; j += 2) printf("\\(bv");
			printf("\\(lb'");
			break;
		case '(':
			brack(m, "\\(lt", "\\(bv", "\\(lb");
			break;
		case '[':
			brack(m, "\\(lc", "\\(bv", "\\(lf");
			break;
		case '|':
			brack(m, "\\(bv", "\\(bv", "\\(bv");
			break;
		default:
			brack(m, (char *) &leftc, (char *) &leftc, (char *) &leftc);
			break;
		}
#ifndef	NEQN
	printf("\\v'%gp'\\*(%d", -v, p1);
#else	/* NEQN */
	printf("\\v'%du'\\*(%d", -v, p1);
#endif	/* NEQN */
	if( rightc ) {
#ifndef	NEQN
		printf("\\|\\v'%gp'", v);
#else	/* NEQN */
		printf("\\|\\v'%du'", v);
#endif	/* NEQN */
		switch( rightc ) {
			case 'f':	/* floor */
				if (n <= 1)
					printf("\\(rf");
				else
					brack(m, "\\(bv", "\\(bv", "\\(rf");
				break;
			case 'c':	/* ceiling */
				if (n <= 1)
					printf("\\(rc");
				else
					brack(m, "\\(rc", "\\(bv", "\\(bv");
				break;
			case '}':
				printf("\\b'\\(rt");
				for(j = 0; j< m; j += 2)printf("\\(bv");
				printf("\\(rk");
				for(j = 0; j< m; j += 2) printf("\\(bv");
				printf("\\(rb'");
				break;
			case ']':
				brack(m, "\\(rc", "\\(bv", "\\(rf");
				break;
			case ')':
				brack(m, "\\(rt", "\\(bv", "\\(rb");
				break;
			case '|':
				brack(m, "\\(bv", "\\(bv", "\\(bv");
				break;
			default:
				brack(m, (char *) &rightc, (char *) &rightc, (char *) &rightc);
				break;
		}
#ifndef	NEQN
		printf("\\v'%gp'", -v);
#else	/* NEQN */
		printf("\\v'%du'", -v);
#endif	/* NEQN */
	}
	printf("\n");
#ifndef	NEQN
	if(dbg)printf(".\tcurly: h=%g b=%g n=%d v=%g l=%c, r=%c\n", 
		eht[yyval.token], ebase[yyval.token], n, v, leftc, rightc);
#else	/* NEQN */
	if(dbg)printf(".\tcurly: h=%d b=%d n=%d v=%d l=%c, r=%c\n", 
		eht[yyval.token], ebase[yyval.token], n, v, leftc, rightc);
#endif	/* NEQN */
}

void
brack(int m, char *t, char *c, char *b) {
	int j;
	printf("\\b'%s", t);
	for( j=0; j<m; j++)
		printf("%s", c);
	printf("%s'", b);
}
