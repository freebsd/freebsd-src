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

/*	from OpenSolaris "hunt8.c	1.6	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)hunt8.c	1.4 (gritter) 01/12/07
 */

#include <locale.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "refer..c"
#define unopen(fil) {if (fil!=NULL) {fclose(fil); fil=NULL;}}

extern long indexdate;

void
runbib (const char *s)
{
	/* make a file suitable for fgrep */
	char tmp[4096];
	snprintf(tmp, sizeof tmp, REFDIR "/mkey '%s' > '%s.ig'", s,s);
	system(tmp);
}

int
makefgrep(char *indexname)
{
	FILE *fa, *fb;
	if (ckexist(indexname, ".ig"))
	{
		/* existing gfrep -type index */
# if D1
		fprintf(stderr, "found fgrep\n");
# endif
		fa = iopen(indexname, ".ig");
		fb = iopen(indexname, "");
		if (gdate(fb)>gdate(fa))
		{
			if (fa!=NULL)
				fclose(fa);
			runbib(indexname);
			fa= iopen(indexname, ".ig");
		}
		indexdate = gdate(fa);
		unopen(fa); 
		unopen(fb);
	}
	else
		if (ckexist(indexname, ""))
		{
			time_t	t;
			/* make fgrep */
# if D1
			fprintf(stderr, "make fgrep\n");
# endif
			runbib(indexname);
			time(&t);
			indexdate = t;
		}
		else /* failure */
		return(0);
	return(1); /* success */
}

int
ckexist(const char *s, const char *t)
{
	char fnam[4096];
	snprintf(fnam, sizeof fnam, "%s%s", s, t);
	return (access(fnam, 04) != -1);
}

FILE *
iopen(const char *s, const char *t)
{
	char fnam[4096];
	FILE *f;
	snprintf(fnam, sizeof fnam, "%s%s", s, t);
	f = fopen (fnam, "r");
	if (f == NULL)
	{
		err("Missing expected file %s", fnam);
		exit(1);
	}
	return(f);
}
