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

/*	from OpenSolaris "deliv2.c	1.4	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)deliv2.c	1.3 (gritter) 10/22/05
 */


#include <stdio.h>
#include <locale.h>
#include <stdarg.h>
#include <stdlib.h>
#include "refer..c"

int
hash (const char *s)
{
	int c, n;
	for(n=0; (c= *s); s++)
		n += (c*n+ (c << (unsigned)n%4));
	return(n>0 ? n : -n);
}

void
err (const char *s, ...)
{
	va_list	ap;
	fprintf(stderr, "Error: ");
	va_start(ap, s);
	vfprintf(stderr, s, ap);
	va_end(ap);
	putc('\n', stderr);
	exit(1);
}

int
prefix(const char *t, const char *s)
{
	int c;

	while ((c= *t++) == *s++)
		if (c==0) return(1);
	return(c==0 ? 1: 0);
}

char *
mindex(const char *s, int c)
{
	register const char *p;
	for( p=s; *p; p++)
		if (*p ==c)
			return((char *)p);
	return(0);
}

void *
zalloc(int m,int n)
{
	void *
# if D1
	fprintf(stderr, "calling calloc for %d*%d bytes\n",m,n);
# endif
	t = calloc(m,n);
# if D1
	fprintf(stderr, "calloc returned %p\n", t);
# endif
	return(t);
}
