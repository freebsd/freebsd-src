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

/*	from OpenSolaris "inv2.c	1.3	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)inv2.c	1.3 (gritter) 10/22/05
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "refer..c"
#define LINESIZ 1250

int
newkeys (FILE *outf, FILE *inf, FILE *recf, int nhash, FILE *fd, int *iflong)
{
	/* reads key lines from inf; hashes and writes on outf;
	 * writes orig key on recf, records pointer on outf too.
	 * format of outf is : hash code space record pointer
	 */

	long lp;
	long ld = 0; 
	int ll = 0, lt = 0;
	char line[LINESIZ];
	char key[30], bkeys[40];
	char *p, *s;
	char *keyv[500];
	int i, nk, ndoc = 0, more = 0, c;

	lp = ftell (recf);
	while (fgets(line, LINESIZ, inf))
	{
		p = line;
		while (*p != '\t') p++;
		*p++ =0;
		fputs(line, recf);
		if (fd)
		{
			snprintf(bkeys, sizeof(bkeys), ";%ld", ld);
			ll = strlen(p);
			lt = strlen(bkeys);
			fputs(bkeys, recf);
			snprintf(bkeys, sizeof(bkeys), ",%d", ll);
			lt += strlen(bkeys);
			fputs(bkeys, recf);
			ld += ll;
			fputs(p, fd);
		}
		putc('\n',recf);
		for(s=p; *s; s++);
		if (*--s == '\n')
		{
			more=0;
			*s=0;
		}
		else
			more=1;
		assert (fd==0 || more==0);
		nk = getargs(p, keyv);
		if (more)
			nk--;
		for(i=0; i<nk; i++)
			fprintf(outf,"%04d %06ld\n",hash(keyv[i])%nhash, lp);
# if D1
		for(i=0; i<nk; i++)
			printf("key %s hash %d\n",keyv[i],hash(keyv[i])%nhash);
# endif
		if (more) /* allow more than LINESIZ keys */
		{
			n_strcpy(key, keyv[nk], sizeof(key));
			for(s=key; *s; s++);
			while ( (c=getc(inf)) != '\n')
			{
				if (c != ' ')
				{
					*s++ = c;
					continue;
				}
				*s=0;
				if (s>key)
					fprintf(outf, "%04d %06ld\n",hash(key)%nhash, lp);
				s = key;
			}
		}
		lp += (strlen(line)+lt+1);
		ndoc++;
	}
	*iflong = (lp>=65536L);
	if (sizeof(int)>2) *iflong=1; /* force long on VAX */
	fclose(recf);
	return(ndoc);
}

char *
trimnl(char *p)
{
	while (*p) p++;
	p--;
	if (*p == '\n') *p=0;
	return p;
}
