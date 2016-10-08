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


/*	from OpenSolaris "glue1.c	1.5	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)glue1.c	1.4 (gritter) 9/7/08
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "refer..c"
#define unopen(fil) {if (fil!=NULL) {fclose(fil); fil=NULL;}}

extern char refdir[];
int lmaster = 1000;
int reached = 0;
FILE *fd = 0;
int *hfreq, hfrflg;
int colevel = 0;
static union firetruck {
	unsigned *a; 
	long *b;
} master;
int iflong;
extern char *fgnames[], **fgnamp;
int prfreqs = 0;
int typeindex = 0;
char usedir[PATH_MAX];
static int full = 1000;
static int tags = 0;
char *sinput, *soutput, *tagout;
long indexdate = 0;
int soutlen = 1000;
int taglen = 1000;

void
huntmain(int argc,char **argv)
{
	/* read query from stdin, expect name of indexes in argv[1] */
	static FILE *fa, *fb, *fc;
	char indexname[PATH_MAX], *qitem[100], *rprog = 0;
	char grepquery[200];
	static char oldname[30] ;
	static int nhash = 0;
	static int maxhash = 0;
	int falseflg = 0, nitem, nfound, frtbl;
	static long *hpt = 0;
	unsigned *masterp;

# if D1
	fprintf(stderr, "in glue1 argc %d argv %o %o\n", argc, argv[0],argv[1]);
# endif
	savedir();
	while (argv[1][0] == '-')
	{
# if D1
		fprintf(stderr, "argv.1 is %s\n",argv[1]);
# endif
		switch(argv[1][1])
		{
		case 'a': /* all output, incl. false drops */
			falseflg = 1; 
			break;
		case 'r':
			argc--; 
			argv++;
			rprog = argv[1];
			break;
		case 'F': /* put out full text */
			full = setfrom(argv[1][2]);
			break;
		case 'T': /* put out tags */
			tags = setfrom(argv[1][2]);
			break;
		case 'i': /* input in argument string */
			argc--; 
			argv++;
			sinput = argv[1];
			break;
		case 's': /*text output to string */
		case 'o':
			argc--; 
			argv++;
			soutput = argv[1];
			if ((intptr_t) argv[2]<16000)
			{
				soutlen = (intptr_t) argv[2];
				argc--; 
				argv++;
			}
			break;
		case 't': /*tag output to string */
			argc--; 
			argv++;
			tagout = argv[1];
			if ((intptr_t)argv[2]<16000)
			{
				taglen = (intptr_t)argv[2];
				argc--; 
				argv++;
			}
			break;
		case 'l': /* specify length of lists */
			argc--; 
			argv++;
			lmaster = atoi(argv[1]);
# if D1
			fprintf(stderr, "lmaster now %d\n",lmaster);
# endif
			break;
		case 'C': 
			argc--; 
			argv++;
			colevel = atoi(argv[1]);
			break;
		}
		argc--; 
		argv++;
	}
	n_strcpy (indexname, todir(argv[1]), sizeof(indexname));
# if D1
	fprintf(stderr, "in huntmain indexname %s typeindex %d\n", indexname, typeindex);
# endif
	if (typeindex == 0 || strcmp (oldname, indexname) !=0)
	{
		n_strcpy (oldname, indexname, sizeof(oldname));
		unopen(fa); 
		unopen(fb); 
		unopen(fc);

		if (ckexist(indexname, ".ib"))
		{
# if D1
			fprintf(stderr, "found old index\n");
# endif
			fa = iopen(indexname, ".ia");
			fb = iopen(indexname, ".ib");
			fc = iopen(indexname, ".ic");
			typeindex =1;
# if D1
			fprintf(stderr, "opened f's as %o %o %o\n",fa,fb,fc);
# endif
			indexdate = gdate(fb);
			fread (&nhash, sizeof(nhash), 1, fa);
			fread (&iflong, sizeof(iflong), 1, fa);
			if (nhash > maxhash)
			{
				if (hpt)
					free (hpt);
				hpt=0;
				if (hfreq)
					free(hfreq);
				hfreq=0;
				maxhash=nhash;
# if D1
				fprintf(stderr, "Freed if needed maxhash %d\n",maxhash);
# endif
			}
			if (hpt==0)
				hpt = zalloc(nhash, sizeof(*hpt));
# if D1
			fprintf(stderr, "hpt now %o\n",hpt);
# endif
			if (hpt == NULL)
				err("No space for hash list (%d)", nhash);
			fread( hpt, sizeof(*hpt), nhash, fa);
			if (hfreq==0)
				hfreq=zalloc(nhash, sizeof(*hfreq));
			if (hfreq==NULL)
				err("No space for hash frequencies (%d)",
				    nhash);
			frtbl = fread(hfreq, sizeof(*hfreq), nhash, fa);
			hfrflg = (frtbl == nhash);
# if D1
			fprintf(stderr,"Read pointer files\n");
# endif
			if (master.a == NULL)
			{
				if (iflong)
					master.b = zalloc(lmaster, sizeof(long));
				else
					master.a = zalloc(lmaster, sizeof(int));
			}
			if (master.a == NULL)
				err("no space for answer list", 0);
		}
		else
			if (makefgrep(indexname))
				typeindex=2;
			else
			{
				err("No files %s\n", indexname);
				exit(1);
			}
	}

	if (iflong) 
		masterp = (unsigned *) master.b;
	else
		masterp = master.a;

# if D1
	fprintf(stderr, "typeindex now %d\n",typeindex);
# endif
	tagout[0]=0;
	if (typeindex==2)
	{
		grepcall(sinput, tagout, indexname);
# if D1
		fprintf(stderr, " back from grepcall\n");
# endif
		restodir();
		return;
	}
	nitem = getq(qitem);
# if D1
	fprintf(stderr, "approaching doquery fb %o\n", fb);
# endif
	nfound = doquery(hpt, nhash, fb, nitem, qitem, masterp);
# ifdef D1
	fprintf(stderr, "return from doquery with nfound %d\n", nfound);
# endif
	if (falseflg == 0)
		nfound = baddrop(masterp, nfound, fc, nitem, qitem, rprog, full);
# ifdef D1
	fprintf(stderr, "after baddrop with nfound %d\n",nfound);
	fprintf(stderr, "tagout is /%s/, sout /%s/\n",tagout, soutput);
# endif
	if (tags)
		result (masterp, nfound >tags ? tags : nfound, fc);
# if D1
	fprintf(stderr, "done with huntmain\n");
	fprintf(stderr, "tagout is /%s/\n", tagout);
	fprintf(stderr, "string out is /%s/\n", soutput);
# endif
	if (fgnamp>fgnames)
	{
		char **fgp;
		int k;
# if D1
		fprintf(stderr, "were %d bad files\n", fgnamp-fgnames);
# endif
		grepquery[0]=0;
		for(k=0; k<nitem; k++)
		{
			n_strcat(grepquery, " ", sizeof(grepquery));
			n_strcat(grepquery, qitem[k], sizeof(grepquery));
		}
		for(fgp=fgnames; fgp<fgnamp; fgp++)
		{
# if D1
			fprintf(stderr, "Now on %s query /%s/\n", *fgp, grepquery);
# endif
			makefgrep(*fgp);
			grepcall(grepquery, tagout, *fgp);
# if D1
			fprintf(stderr, "tagout now /%s/\n", tagout);
# endif
		}
	}
	restodir();
}

char *
todir(char *t)
{
	char *s;

	usedir[0] = 0;
	s=t;
	while (*s) s++;
	while (s>=t && *s != '/') s--;
	if (s<t) return(t);
	*s++ = 0;
	t = (*t ? t : "/");
	chdir (t);
	n_strcpy (usedir,t,sizeof(usedir));
	return(s);
}

int
setfrom(int c)
{
	switch(c)
	{
	case 'y': 
	case '\0':
	default:
		return(1000);
	case '1':
	case '2': 
	case '3': 
	case '4': 
	case '5':
	case '6': 
	case '7': 
	case '8': 
	case '9':
		return(c-'0');
	case 'n': 
	case '0':
		return(0);
	}
}
