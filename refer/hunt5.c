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

/*	from OpenSolaris "hunt5.c	1.4	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)hunt5.c	1.3 (gritter) 10/22/05
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

extern char *soutput, *tagout, usedir[];
union ptr {
	unsigned *a; 
	long *b;
};

void
result(unsigned *mptr, int nf, FILE *fc)
{
	int i, c;
	char *s;
	long lp;
	extern int iflong;
	char res[100];
	union ptr master;

	if (iflong) {
		master.b = (long *) mptr;
	}
	else {
		master.a = mptr;
	}

	for(i=0; i<nf; i++)
	{
		lp = iflong ? master.b[i] : master.a[i];
		fseek(fc,lp, SEEK_SET);
		fgets(res, sizeof res, fc);
		for(s=res; (c = *s); s++)
			if (c== ';')
			{
				*s=0;
				break;
			}
		if (tagout !=0)
		{
			if (res[0]=='/' || usedir[0]==0)
				sprintf(tagout, "%s", res);
			else
				sprintf(tagout, "%s/%s", usedir, res);
			while (*tagout) tagout++;
		}
		else
		{
			if (res[0]!='/' || usedir[0]==0)
				printf("%s/", usedir);
			printf("%s\n", res);
		}
	}
}

long
gdate(FILE *f)
{
	struct stat sb;
	fstat (fileno(f), &sb);
	return  (sb . st_mtime);
}
