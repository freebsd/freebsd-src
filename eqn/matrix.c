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
  
/*	from OpenSolaris "matrix.c	1.3	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)matrix.c	1.4 (gritter) 10/29/05
 */

/*
 * Changes Copyright (c) 2014 Carsten Kunze (carsten.kunze at arcor.de)
 */

#include "e.h"
#include "y.tab.h"

extern YYSTYPE yyval;

void
column(int type, int p1) {
	int i;

	lp[p1] = ct - p1 - 1;
	if( dbg ){
		printf(".\t%d column of", type);
		for( i=p1+1; i<ct; i++ )
			printf(" S%d", lp[i]);
		printf(", rows=%d\n",lp[p1]);
	}
	lp[ct++] = type;
}

void
matrix(int p1) {
#ifndef	NEQN
	float hb, b;
#else	/* NEQN */
	int hb, b;
#endif	/* NEQN */
	int nrow, ncol, i, j, k, val[100];
	char *space;

	space = "\\ \\ ";
	nrow = lp[p1];	/* disaster if rows inconsistent */
	ncol = 0;
	for( i=p1; i<ct; i += lp[i]+2 ){
		ncol++;
		if(dbg)printf(".\tcolct=%d\n",lp[i]);
	}
	for( k=1; k<=nrow; k++ ) {
		hb = b = 0;
		j = p1 + k;
		for( i=0; i<ncol; i++ ) {
			hb = max(hb, eht[lp[j]]-ebase[lp[j]]);
			b = max(b, ebase[lp[j]]);
			j += nrow + 2;
		}
#ifndef	NEQN
		if(dbg)printf(".\trow %d: b=%g, hb=%g\n", k, b, hb);
#else	/* NEQN */
		if(dbg)printf(".\trow %d: b=%d, hb=%d\n", k, b, hb);
#endif	/* NEQN */
		j = p1 + k;
		for( i=0; i<ncol; i++ ) {
			ebase[lp[j]] = b;
			eht[lp[j]] = b + hb;
			j += nrow + 2;
		}
	}
	j = p1;
	for( i=0; i<ncol; i++ ) {
		lpile(lp[j+lp[j]+1], j+1, j+lp[j]+1);
		val[i] = yyval.token;
		j += nrow + 2;
	}
	yyval.token = oalloc();
	eht[yyval.token] = eht[val[0]];
	ebase[yyval.token] = ebase[val[0]];
	lfont[yyval.token] = rfont[yyval.token] = 0;
#ifndef	NEQN
	if(dbg)printf(".\tmatrix S%d: r=%d, c=%d, h=%g, b=%g\n",
		yyval.token,nrow,ncol,eht[yyval.token],ebase[yyval.token]);
#else	/* NEQN */
	if(dbg)printf(".\tmatrix S%d: r=%d, c=%d, h=%d, b=%d\n",
		yyval.token,nrow,ncol,eht[yyval.token],ebase[yyval.token]);
#endif	/* NEQN */
	printf(".ds %d \"", yyval.token);
	for( i=0; i<ncol; i++ )  {
		printf("\\*(%d%s", val[i], i==ncol-1 ? "" : space);
		ofree(val[i]);
	}
	printf("\n");
	ct = p1;
}
