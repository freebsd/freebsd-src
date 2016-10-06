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
  
/*	from OpenSolaris "pile.c	1.4	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)pile.c	1.4 (gritter) 10/29/05
 */

/*
 * Changes Copyright (c) 2014 Carsten Kunze (carsten.kunze at arcor.de)
 */

# include "e.h"
#include "y.tab.h"

extern YYSTYPE yyval;

void
lpile(int type, int p1, int p2) {
	int i, nlist, nlist2, mid;
#ifndef	NEQN
	float h, b, bi, hi, gap;
#else	/* NEQN */
	int h, b, bi, hi, gap;
#endif	/* NEQN */
	yyval.token = oalloc();
#ifndef NEQN
	gap = VERT(EM(0.4, ps)); /* 4/10 m between blocks */
#else /* NEQN */
	gap = VERT(1);
#endif /* NEQN */
	if( type=='-' ) gap = 0;
	nlist = p2 - p1;
	nlist2 = (nlist+1)/2;
	mid = p1 + nlist2 -1;
	h = 0;
	for( i=p1; i<p2; i++ )
		h += eht[lp[i]];
	eht[yyval.token] = h + (nlist-1)*gap;
	b = 0;
	for( i=p2-1; i>mid; i-- )
		b += eht[lp[i]] + gap;
	ebase[yyval.token] = (nlist%2) ? b + ebase[lp[mid]]
#ifndef NEQN
			: b - VERT(EM(0.5, ps)) - gap;
#else /* NEQN */
			: b - VERT(1) - gap;
#endif /* NEQN */
	if(dbg) {
		printf(".\tS%d <- %c pile of:", yyval.token, type);
		for( i=p1; i<p2; i++)
			printf(" S%d", lp[i]);
#ifndef	NEQN
		printf(";h=%g b=%g\n", eht[yyval.token], ebase[yyval.token]);
#else	/* NEQN */
		printf(";h=%d b=%d\n", eht[yyval.token], ebase[yyval.token]);
#endif	/* NEQN */
	}
	nrwid(lp[p1], ps, lp[p1]);
	printf(".nr %d \\n(%d\n", yyval.token, lp[p1]);
	for( i = p1+1; i<p2; i++ ) {
		nrwid(lp[i], ps, lp[i]);
		printf(".if \\n(%d>\\n(%d .nr %d \\n(%d\n", 
			lp[i], yyval.token, yyval.token, lp[i]);
	}
#ifndef	NEQN
	printf(".ds %d \\v'%gp'\\h'%du*\\n(%du'\\\n", yyval.token, ebase[yyval.token], 
		type=='R' ? 1 : 0, yyval.token);
#else	/* NEQN */
	printf(".ds %d \\v'%du'\\h'%du*\\n(%du'\\\n", yyval.token, ebase[yyval.token], 
		type=='R' ? 1 : 0, yyval.token);
#endif	/* NEQN */
	for(i = p2-1; i >=p1; i--) {
		hi = eht[lp[i]]; 
		bi = ebase[lp[i]];
	switch(type) {

	case 'L':
#ifndef	NEQN
		printf("\\v'%gp'\\*(%d\\h'-\\n(%du'\\v'0-%gp'\\\n", 
			-bi, lp[i], lp[i], hi-bi+gap);
#else	/* NEQN */
		printf("\\v'%du'\\*(%d\\h'-\\n(%du'\\v'0-%du'\\\n", 
			-bi, lp[i], lp[i], hi-bi+gap);
#endif	/* NEQN */
		continue;
	case 'R':
#ifndef	NEQN
		printf("\\v'%gp'\\h'-\\n(%du'\\*(%d\\v'0-%gp'\\\n", 
			-bi, lp[i], lp[i], hi-bi+gap);
#else	/* NEQN */
		printf("\\v'%du'\\h'-\\n(%du'\\*(%d\\v'0-%du'\\\n", 
			-bi, lp[i], lp[i], hi-bi+gap);
#endif	/* NEQN */
		continue;
	case 'C':
	case '-':
#ifndef	NEQN
		printf("\\v'%gp'\\h'\\n(%du-\\n(%du/2u'\\*(%d", 
			-bi, yyval.token, lp[i], lp[i]);
		printf("\\h'-\\n(%du-\\n(%du/2u'\\v'0-%gp'\\\n", 
			yyval.token, lp[i], hi-bi+gap);
#else	/* NEQN */
		printf("\\v'%du'\\h'\\n(%du-\\n(%du/2u'\\*(%d", 
			-bi, yyval.token, lp[i], lp[i]);
		printf("\\h'-\\n(%du-\\n(%du/2u'\\v'0-%du'\\\n", 
			yyval.token, lp[i], hi-bi+gap);
#endif	/* NEQN */
		continue;
		}
	}
#ifndef	NEQN
	printf("\\v'%gp'\\h'%du*\\n(%du'\n", eht[yyval.token]-ebase[yyval.token]+gap, 
		type!='R' ? 1 : 0, yyval.token);
#else	/* NEQN */
	printf("\\v'%du'\\h'%du*\\n(%du'\n", eht[yyval.token]-ebase[yyval.token]+gap, 
		type!='R' ? 1 : 0, yyval.token);
#endif	/* NEQN */
	for( i=p1; i<p2; i++ )
		ofree(lp[i]);
	lfont[yyval.token] = rfont[yyval.token] = 0;
}
