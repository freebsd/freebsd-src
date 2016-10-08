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
  
/*	from OpenSolaris "mark.c	1.3	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)mark.c	1.3 (gritter) 8/12/05
 */

/*
 * Changes Copyright (c) 2014 Carsten Kunze (carsten.kunze at arcor.de)
 */

#include "e.h"
#include "y.tab.h"

extern YYSTYPE yyval;

void
mark(int p1) {
	markline = 1;
	printf(".ds %d \\k(97\\*(%d\n", p1, p1);
	yyval.token = p1;
	if(dbg)printf(".\tmark %d\n", p1);
}

void
lineup(int p1) {
	markline = 1;
	if (p1 == 0) {
		yyval.token = oalloc();
		printf(".ds %d \\h'|\\n(97u'\n", yyval.token);
	}
	if(dbg)printf(".\tlineup %d\n", p1);
}
