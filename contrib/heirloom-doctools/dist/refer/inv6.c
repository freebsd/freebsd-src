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

/*	from OpenSolaris "inv6.c	1.3	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)inv6.c	1.3 (gritter) 10/22/05
 */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "refer..c"

void
whash(FILE *ft, FILE *fa, FILE *fb, int nhash, int iflong, long *ptotct, int *phused)
{
	char line[100];
	int hash = 0, hused = 0;
	long totct = 0L;
	int ct = 0;
	long point;
	long opoint = -1;
	int k; 
	long lp;
	long *hpt;
	int *hfreq;

	hpt = calloc (nhash+1, sizeof(*hpt));
	assert (hpt != NULL);
	hfreq = calloc (nhash, sizeof(*hfreq));
	assert (hfreq != NULL);
	hpt[0] = 0;
	lp= 0;
	while (fgets(line, 100, ft))
	{
		totct++;
		sscanf(line, "%d %ld", &k, &point);
		if (hash < k)
		{
			hused++;
			if (iflong) putl(-1L, fb); 
			else putw(-1, fb);
			hfreq[hash]=ct;
			while (hash<k)
			{
				hpt[++hash] = lp;
				hfreq[hash] = 0;
			}
			hpt[hash] = lp += iflong? sizeof(long) : sizeof(int);
			opoint= -1;
			ct=0;
		}
		if (point!=opoint)
		{
			if (iflong)
				putl(opoint=point, fb);
			else
				putw( (int)(opoint=point), fb);
			lp += iflong? sizeof(long) : sizeof(int);
			ct++;
		}
	}
	if (iflong) putl(-1L, fb); 
	else putw(-1,fb);
	while (hash<nhash)
		hpt[++hash]=lp;
	fwrite(&nhash, sizeof(nhash), 1, fa);
	fwrite(&iflong, sizeof(iflong), 1, fa);
	fwrite(hpt, sizeof(*hpt), nhash, fa);
	free(hpt);
	fwrite (hfreq, sizeof(*hfreq), nhash, fa);
	free(hfreq);
	*ptotct = totct;
	*phused = hused;
}

void
putl(long ll, FILE *f)
{
	putw(ll, f);
}

long
getl(FILE *f)
{
	return(getw(f));
}
