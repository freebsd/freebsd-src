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

/*	from OpenSolaris "refer8.c	1.3	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)refer8.c	1.3 (gritter) 10/22/05
 */

#include <string.h>
#include "refer..c"

static char ahead[1024];
static int peeked = 0;
static char *noteof = (char *) 1;

char *
input(char *s, size_t l)
{
	if (peeked) {
		peeked = 0;
		if (noteof == 0)
			return(0);
		if (s != ahead) n_strcpy(s, ahead, l);
		return(s);
	}
	return(fgets(s, 1000, in));
}

char *
lookat(void)
{
	if (peeked)
		return(ahead);
	noteof = input(ahead, sizeof(ahead));
	peeked = 1;
	return(noteof);
}

void
addch(char *s, int c)
{
	while (*s)
		s++;
	*s++ = c;
	*s = 0;
}
