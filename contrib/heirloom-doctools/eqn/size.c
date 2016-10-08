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
  
/*	from OpenSolaris "size.c	1.3	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)size.c	1.5 (gritter) 10/19/06
 */

/*
 * Changes Copyright (c) 2014 Carsten Kunze (carsten.kunze at arcor.de)
 */

# include "e.h"
# include <stdlib.h>
#include "y.tab.h"

extern YYSTYPE yyval;

void
setsize(char *p)	/* set size as found in p */
{
	if (*p == '+')
		ps += atof(p+1);
	else if (*p == '-')
		ps -= atof(p+1);
	else
		ps = atof(p);
	if(dbg)printf(".\tsetsize %s; ps = %g\n", p, ps);
}

void
size(float p1, int p2) {
		/* old size in p1, new in ps */
	float effps, effp1;

	yyval.token = p2;
#ifndef	NEQN
	if(dbg)printf(".\tb:sb: S%d <- \\s%s S%d \\s%s; b=%g, h=%g\n", 
		yyval.token, tsize(ps), p2, tsize(p1), ebase[yyval.token], eht[yyval.token]);
#else	/* NEQN */
	if(dbg)printf(".\tb:sb: S%d <- \\s%s S%d \\s%s; b=%d, h=%d\n", 
		yyval.token, tsize(ps), p2, tsize(p1), ebase[yyval.token], eht[yyval.token]);
#endif	/* NEQN */
	effps = EFFPS(ps);
	effp1 = EFFPS(p1);
	printf(".ds %d \\s%s\\*(%d\\s%s\n", 
		yyval.token, tsize(effps), p2, tsize(effp1));
	ps = p1;
}

void
globsize(void) {
	char temp[20];

	getstr(temp, 20);
	if (temp[0] == '+')
		gsize += atof(temp+1);
	else if (temp[0] == '-')
		gsize -= atof(temp+1);
	else
		gsize = atof(temp);
	yyval.token = eqnreg = 0;
	setps(gsize);
	ps = gsize;
	if (gsize >= 12)	/* sub and sup size change */
		deltaps = gsize / 4;
	else
		deltaps = gsize / 3;
	if (gsize == (int)gsize)
		deltaps = (int)deltaps;
}

char *
tsize(float s)
{
	static char	b[5][20];
	static int	t;
	int	i;

	t = (t + 1) % 5;
	if ((i = s) == s) {
		if (i < 40)
			snprintf(b[t], sizeof(b[t]), "%d", i);
		else if (i < 100)
			snprintf(b[t], sizeof(b[t]), "(%d", i);
		else
			snprintf(b[t], sizeof(b[t]), "[%d]", i);
	} else {
		snprintf(b[t], sizeof(b[t]), "[%g]", s);
	}
	return b[t];
}
