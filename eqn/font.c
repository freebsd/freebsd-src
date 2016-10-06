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

/*	from OpenSolaris "font.c	1.4	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)font.c	1.5 (gritter) 1/13/08
 */

/*
 * Changes Copyright (c) 2014 Carsten Kunze (carsten.kunze at arcor.de)
 */

# include "e.h"
#include "y.tab.h"

extern YYSTYPE yyval;

void
setfont(char ch1) {
	/* use number '1', '2', '3' for roman, italic, bold */
	yyval.token = ft;
	if (ch1 == 'r' || ch1 == 'R')
		ft = ROM;
	else if (ch1 == 'i' || ch1 == 'I')
		ft = ITAL;
	else if (ch1 == 'b' || ch1 == 'B')
		ft = BLD;
	else
		ft = ch1;
	printf(".ft %c\n", ft);
#ifndef NEQN
	if(dbg)printf(".\tsetfont %c %c\n", ch1, ft);
#else /* NEQN */
	if(dbg)printf(".\tsetfont %c\n", ft);
#endif /* NEQN */
}

void
font(int p1, int p2) {
		/* old font in p1, new in ft */
	yyval.token = p2;
	lfont[yyval.token] = rfont[yyval.token] = ital(ft) ? ITAL : ROM;
#ifndef	NEQN
	if(dbg)printf(".\tb:fb: S%d <- \\f%c S%d \\f%c b=%g,h=%g,lf=%c,rf=%c\n", 
		yyval.token, ft, p2, p1, ebase[yyval.token], eht[yyval.token], lfont[yyval.token], rfont[yyval.token]);
#else	/* NEQN */
	if(dbg)printf(".\tb:fb: S%d <- \\f%c S%d \\f%c b=%d,h=%d,lf=%c,rf=%c\n", 
		yyval.token, ft, p2, p1, ebase[yyval.token], eht[yyval.token], lfont[yyval.token], rfont[yyval.token]);
#endif	/* NEQN */
	printf(".ds %d \\f%c\\*(%d\\f%c\n", 
		yyval.token, ft, p2, p1);
	ft = p1;
	printf(".ft %c\n", ft);
}

void
fatbox(int p) {
	yyval.token = p;
	nrwid(p, ps, p);
	printf(".ds %d \\*(%d\\h'-\\n(%du+0.05m'\\*(%d\n", p, p, p, p);
	if(dbg)printf(".\tfat %d, sh=0.05m\n", p);
}

void
globfont(void) {
	char temp[20];

	getstr(temp, 20);
	yyval.token = eqnreg = 0;
	gfont = temp[0];
	switch (gfont) {
	case 'r': case 'R':
		gfont = '1';
		break;
	case 'i': case 'I':
		gfont = '2';
		break;
	case 'b': case 'B':
		gfont = '3';
		break;
	}
	printf(".ft %c\n", gfont);
	ft = gfont;
}
