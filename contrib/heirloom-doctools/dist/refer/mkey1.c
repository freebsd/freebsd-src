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

/*	from OpenSolaris "mkey1.c	1.5	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)mkey1.c	1.3 (gritter) 10/22/05
 */

#include <stdio.h>
#include <locale.h>
#include <stdlib.h>
#include "refer..c"

extern char *comname;	/* "/usr/lib/refer/eign" */
int wholefile = 0;
int keycount = 100;
int labels = 1;
int minlen = 3;
extern int comcount;
char *iglist = "XYZ#";

int
main (int argc,char **argv)
{
	/* this program expects as its arguments a list of
	 * files and generates a set of lines of the form
	 *	filename:byte-add,length (tab) key1 key2 key3
	 * where the byte addresses give the position within
	 * the file and the keys are the strings off the lines
	 * which are alphabetic, first six characters only.
	 */

	int i;
	char *name, qn[200];
	char *inlist = 0;

	FILE *f, *ff;

	while (argc>1 && argv[1][0] == '-')
	{
		switch(argv[1][1])
		{
		case 'c':
			comname = argv[2];
			argv++; 
			argc--;
			break;
		case 'w':
			wholefile = 1;  
			break;
		case 'f':
			inlist = argv[2];
			argv++; 
			argc--;
			break;
		case 'i':
			iglist = argv[2];
			argv++; 
			argc--;
			break;
		case 'l':
			minlen = atoi(argv[1]+2);
			if (minlen<=0) minlen=3;
			break;
		case 'n': /* number of common words to use */
			comcount = atoi(argv[1]+2);
			break;
		case 'k': /* number  of keys per file max */
			keycount = atoi(argv[1]+2);
			break;
		case 's': /* suppress labels, search only */
			labels = 0;
			break;
		}
		argc--; 
		argv++;
	}
	if (inlist)
	{
		ff = fopen(inlist, "r");
		while (fgets(qn, 200, ff))
		{
			trimnl(qn);
			f = fopen (qn, "r");
			if (f!=NULL)
				dofile(f, qn);
			else
				fprintf(stderr, "Can't read %s\n",qn);
		}
	}
	else
		if (argc<=1)
			dofile(stdin, "");
		else
			for(i=1; i<argc; i++)
			{
				f = fopen(name=argv[i], "r");
				if (f==NULL)
					err("No file %s",name);
				else
					dofile(f, name);
			}
	return 0;
}
