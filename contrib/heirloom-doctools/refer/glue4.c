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

/*	from OpenSolaris "glue4.c	1.3	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)glue4.c	1.4 (gritter) 9/7/08
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include "refer..c"

extern char gfile[];
extern char usedir[];

int
grepcall (char *in, char *out, char *arg)
{
	char line[200], *s, argig[100], *cv[50];
	char *inp, inb[500];
	FILE *gf;
	int c, alph = 0, nv = 0;
	int sv0, sv1;
	n_strcpy (argig, arg, sizeof(argig)); 
	n_strcat(argig, ".ig", sizeof(argig));
	n_strcpy (inp=inb, in, sizeof(inb));
	if (gfile[0]==0)
		sprintf(gfile, "/tmp/rj%dg", (int)getpid());
# if D1
	fprintf(stderr, "in grepcall, gfile %s in %o out %o\n", gfile,in,out);
# endif
	for(cv[nv++] = "fgrep"; (c = *inp); inp++)
	{
		if (c== ' ')
			c = *inp = 0;
		else if (isupper(c))
			*inp = tolower(c);
		alph = (c==0) ? 0 : alph+1;
		if (alph == 1)
			cv[nv++] = inp;
		if (alph > 6)
			*inp = 0;
	}
# if D1
	fprintf(stderr, "%d args set up\n", nv);
# endif
	{
		sv0 = dup(0);
		close(0);
		if (open (argig, O_RDONLY) != 0)
			err("Can't read fgrep index %s", argig);
		sv1 = dup(1);
		close(1);
		if (creat(gfile, 0666) != 1)
			err("Can't write fgrep output %s", gfile);
		fgrep(nv, cv);
# if D1
		fprintf(stderr, "fgrep returned, output is..\n");
# endif
		close (0); 
		dup(sv0); 
		close(sv0);
		close (1); 
		dup(sv1); 
		close(sv1);
	}

# if D1
	fprintf(stderr, "back from fgrep\n");
# endif
	gf = fopen(gfile, "r");
	if (gf==NULL)
		err("can't read fgrep output %s", gfile);
	while (fgets(line, 100, gf) == line)
	{
		line[100]=0;
# if D1
		fprintf(stderr, "read line as //%s//\n",line);
# endif
		for(s=line; *s && (*s != '\t'); s++);
		if (*s == '\t')
		{
			*s++ = '\n';
			*s++ = 0;
		}
		if (line[0]) {
			if (usedir[0]) {
				strcat(out, usedir);
				strcat(out, "/");
			}
			strcat(out, line);
		}
# if D1
		fprintf(stderr, "out now /%s/\n",out);
# endif
		while (*s) s++;
# if D1
		fprintf(stderr, "line %o s %o s-1 %o\n",line,s,s[-1]);
# endif
		if (s[-1]!= '\n')
			while (!feof(gf) && getc(gf)!= '\n') ;
	}
	fclose(gf);
# if D1
	fprintf(stderr, "back from reading %, out %s\n",out);
# else
	unlink (gfile);
# endif
	return(0);
}

void
clfgrep(void)
{
	if (gfile[0])
		unlink(gfile);
}
