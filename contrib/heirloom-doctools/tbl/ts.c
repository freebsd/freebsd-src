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
  
/*	from OpenSolaris "ts.c	1.3	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)ts.c	1.3 (gritter) 7/23/05
 */

#include "t..c"

 /* ts.c: minor string processing subroutines */

/* returns: 1 for match, 0 else */
int
cprefix(const char *ctl, const char *line)
{
	char c;

	if (*line++ != '.') return 0;
	while (*line == ' ' || *line == '\t') line++;
	while ((c = *ctl++) == *line++)
		if (!c) return 1;
	return !c;
}

int
letter(int ch)
{
	if (ch >= 'a' && ch <= 'z')
		return(1);
	if (ch >= 'A' && ch <= 'Z')
		return(1);
	return(0);
}

void
tcopy(char *s, char *t)
{
	while ((*s++ = *t++));
}
