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

/*	from OpenSolaris "hunt6.c	1.4	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)hunt6.c	1.4 (gritter) 10/2/07
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "refer..c"
#define TXTLEN 1000

char *outbuf = 0;
extern char *soutput;
extern int soutlen, iflong;
extern long indexdate;
union ptr {
	unsigned *a; 
	long *b; 
};

int
baddrop(unsigned *mptr, int nf, FILE *fc, int nitem, char **qitem,
		char *rprog, int full)
{
	/* checks list of drops for real bad drops; finds items with "deliv" */
	int i, g, j, need, na, len = 0;
	long lp;
	char res[100], *ar[50], output[TXTLEN], *mput;
	union ptr master;
	extern int colevel, reached;

	if (iflong) {
		master.b = (long *) mptr;
	}
	else {
		master.a = mptr;
	}

# if D1
	if (iflong)
		fprintf(stderr,"in baddrop, nf %d master %ld %ld %ld\n",
			nf, master.b[0], master.b[1], master.b[2]);
	else
		fprintf(stderr,"in baddrop, nf %d master %d %d %d\n",
			nf, master.a[0], master.a[1], master.a[2]);
# endif
	for (i=g=0; i<nf; i++)
	{
		lp = iflong ? master.b[i] : master.a[i];
# if D1
		if (iflong)
			fprintf(stderr, "i %d master %lo lp %lo\n",
				i, master.b[i], lp);
		else
			fprintf(stderr, "i %d master %o lp %lo\n",
				i, master.a[i], lp);
# endif
		fseek (fc, lp, SEEK_SET);
		fgets( res, sizeof res, fc);
# if D1
		fprintf(stderr, "tag %s", res);
# endif
		if (!auxil(res,output))
		{
			char *s; 
			int c;
# if D1
			fprintf(stderr, "not auxil try rprog %c\n",
				rprog? 'y': 'n');
# endif
			for(s=res; (c= *s); s++)
				if (c == ';' || c == '\n')
				{
					*s=0; 
					break;
				}

			if (rprog)
				len = corout(res, output, rprog, 0, TXTLEN);
			else
			{
				len = findline(res, &mput, TXTLEN, indexdate);
				if (len > 0) /* copy and free */
				{
					strncpy(output, mput, TXTLEN);
					free(mput);
				}
				else /* insufficient memory or other... */
					len = 0;
			}
		}
# if D1
		assert (len <TXTLEN);
		fprintf(stderr,"item %d of %d, tag %s len %d output\n%s\n..\n",
			i, nf, res, len, output);
# endif
		if (len==0)
			continue;
		need = colevel ? reached : nitem;
		na=0;
		ar[na++] = "fgrep";
		ar[na++] = "-r";
		ar[na++] = "-n";
		ar[na++] = (char *)(intptr_t) need;
		ar[na++] = "-i";
		ar[na++] = output;
		ar[na++] = (char *)(intptr_t) len;
		for(j=0; j<nitem; j++)
			ar[na++] = qitem[j];
# ifdef D1
		fprintf(stderr, "calling fgrep len %d ar[4] %s %o %d \n",
			len,ar[4],ar[5],ar[6]);
# endif
		if (fgrep(na, ar)==0)
		{
# ifdef D1
			fprintf(stderr, "fgrep found it\n");
# endif
			if (iflong)
				master.b[g++] = master.b[i];
			else
				master.a[g++] = master.a[i];
			if (full >= g)
			{
				if (soutput==0)
					fputs(output, stdout);
				else
					strcpy (soutput, output);
			}
		}
# ifdef D1
		fprintf(stderr, "after fgrep\n");
# endif
	}
	return(g);
}

int
auxil(char * res, char *output)
{
	extern FILE *fd;
	long lp, c; 
	int len;
	if (fd==0)return(0);
	while ((c = *res++))
	{
		if (c == ';')
		{
			sscanf(res, "%ld,%d", &lp, &len);
			fseek (fd, lp, SEEK_SET);
			fgets(output, len, fd);
			return(1);
		}
	}
	return(0);
}
