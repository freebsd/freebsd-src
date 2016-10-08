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
  
/*	from OpenSolaris "tm.c	1.5	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)tm.c	1.5 (gritter) 9/15/05
 */

 /* tm.c: split numerical fields */
# include "t..c"

char *
maknew(char *str)
{
	/* make two numerical fields */
	int c;
	char *dpoint, *p, *q, *ba;
	p = str;
	for (ba= 0; (c = *str); str++)
		if (c == '\\' && *(str+1)== '&')
			ba=str;
	str=p;
	if (ba==0) {
		for (dpoint=0; *str; str++) {
			if ((*str&0377)==decimalpoint && !ineqn(str,p) &&
				((str>p && digit(*(str-1))) ||
				digit(*(str+1))))
					dpoint=str;
		}
		if (dpoint==0)
			for(; str>p; str--) {
			if (digit( * (str-1) ) && !ineqn(str, p))
				break;
			}
		if (!dpoint && p==str) /* not numerical, don't split */
			return NULL;
		if (dpoint) str=dpoint;
	}
	else
		str = ba;
	p =str;
	if (exstore ==0 || exstore >exlim) {
		if (!(exstore = chspace()))
			return (char *)-1;
		exlim= exstore+MAXCHS;
	}
	q = exstore;
	ba = exstore + MAXSTR;
	do {
		if (exstore > ba) {
			error("numeric field too big");
			return (char *)-1;
		}
	} while ((*exstore++ = *str++));
	*p = 0;
	return(q);
}

int 
ineqn(char *s, char *p)
{
/* true if s is in a eqn within p */
int ineq = 0, c;
while ((c = *p))
	{
	if (s == p)
		return(ineq);
	p++;
	if ((ineq == 0) && (c == delim1))
		ineq = 1;
	else
	if ((ineq == 1) && (c == delim2))
		ineq = 0;
	}
return(0);
}
