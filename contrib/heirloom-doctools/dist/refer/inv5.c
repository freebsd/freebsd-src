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

/*	from OpenSolaris "inv5.c	1.4	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)inv5.c	1.5 (gritter) 12/25/06
 */

#include <stdio.h>
#include <locale.h>
#include <stdlib.h>
#include "refer..c"

int
recopy (FILE *ft, FILE *fb, FILE *fa, int nhash)
{
	/* copy fb (old hash items/pointers) to ft (new ones) */
	int n, i, iflong;
	int *hpt_s = 0;
	int (*getfun)(FILE *);
	long *hpt_l = 0;
	long k, lp;
	if (fa==NULL)
	{
		err("No old pointers",0);
		return 0;
	}
	fread(&n, sizeof(n), 1, fa);
	fread(&iflong, sizeof(iflong), 1, fa);
	if (iflong)
	{
		hpt_l = calloc(sizeof(*hpt_l), n+1);
		n =fread(hpt_l, sizeof(*hpt_l), n, fa);
	}
	else
	{
		hpt_s = calloc(sizeof(*hpt_s), n+1);
		n =fread(hpt_s, sizeof(*hpt_s), n, fa);
	}
	if (n!= nhash)
		fprintf(stderr, "Changing hash value to old %d\n",n);
	fclose(fa);
	if (iflong)
		getfun = (int(*)(FILE *))getl;
	else
#ifdef	EUC
		getfun = getw;
#else
		getfun = fgetc;
#endif
	for(i=0; i<n; i++)
	{
		if (iflong)
			lp = hpt_l[i];
		else
			lp = hpt_s[i];
		fseek(fb, lp, SEEK_SET);
		while ( (k= (*getfun)(fb) ) != -1)
			fprintf(ft, "%04d %06ld\n",i,k);
	}
	fclose(fb);
	return(n);
}
