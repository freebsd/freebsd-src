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

/*	from OpenSolaris "hunt1.c	1.6	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)hunt1.c	1.4 (gritter) 9/7/08
 */

# include <locale.h>
# include <stdio.h>
# include <assert.h>
# include <inttypes.h>
# include <stdlib.h>
# include <unistd.h>
# include <string.h>
# include <limits.h>
# include "refer..c"
extern char refdir[];
extern int keepold;
extern char *fgnames[];
extern char **fgnamp;
FILE *fd =NULL;
int lmaster =500;
int *hfreq, hfrflg;
int colevel =0;
int measure=0;
int soutlen =1000;
int reached =0;
int iflong =0;
int prfreqs =0;
char usedir[PATH_MAX];
char gfile[50];
static int full =1000;
static int tags =0;
char *sinput, *soutput, *tagout;
long indexdate =0;

int
main(int argc,char **argv)
{
	/* read query from stdin, expect name of indexes in argv[1] */
	static FILE *fa, *fb, *fc;
	char nma[PATH_MAX], nmb[PATH_MAX], nmc[PATH_MAX],
	     *qitem[100], *rprog = NULL;
	char nmd[PATH_MAX], grepquery[256];
	static char oldname[30] ;
	static int was =0;
	/* these pointers are unions of pointer to int and pointer to long */
	long *hpt = 0;
	unsigned *master =0;
	int falseflg, nhash, nitem, nfound = 0, frtbl, kk;

	/* special wart for refpart: default is tags only */

	falseflg = 0;

	while (argc > 1 && argv[1][0] == '-')
	{
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
				soutlen = (intptr_t)argv[2];
				argc--; 
				argv++;
			}
			break;
		case 't': /*tag output to string */
			argc--; 
			argv++;
			tagout = argv[1];
			break;
		case 'l': /* length of internal lists */
			argc--; 
			argv++;
			lmaster = atoi(argv[1]);
			break;
		case 'g': /* suppress fgrep search on old files */
			keepold = 0;
			break;
		case 'C': /* coordination level */
			colevel = atoi(argv[1]+2);
# if D1
			fprintf(stderr, "colevel set to %d\n",colevel);
# endif
			break;
		case 'P': /* print term freqs */
			prfreqs=1; 
			break;
		case 'm':
			measure=1; 
			break;
		}
		argc--; 
		argv++;
	}
	if(argc < 2)
		exit(1);
	n_strcpy (nma, todir(argv[1]), sizeof(nma));
	if (was == 0 || strcmp (oldname, nma) !=0)
	{
		n_strcpy (oldname,nma, sizeof(oldname));
		n_strcpy (nmb, nma, sizeof(nmb)); 
		n_strcpy (nmc, nmb, sizeof(nmc)); 
		n_strcpy(nmd,nma, sizeof(nmd));
		n_strcat (nma, ".ia", sizeof(nma));
		n_strcat (nmb, ".ib", sizeof(nmb));
		n_strcat (nmc, ".ic", sizeof(nmc));
		n_strcat (nmd, ".id", sizeof(nmd));
		if (was)
		{
			fclose(fa); 
			fclose(fb); 
			fclose(fc);
		}

		fa = fopen(nma, "r");
		if (fa==NULL)
		{
			size_t s = strlen(oldname)+2;
			n_strcpy(*fgnamp++ = calloc(s,1), oldname, s);
			fb=NULL;
			goto search;
		}
		fb = fopen(nmb, "r");
		fc = fopen(nmc, "r");
		was =1;
		if (fb== NULL || fc ==NULL)
		{
			err("Index incomplete %s", nmb);
			exit(1);
		}
		indexdate = gdate(fb);
		fd = fopen(nmd, "r");
	}
	fseek (fa, 0, SEEK_SET);
	fread (&nhash, sizeof(nhash), 1, fa);
	fread (&iflong, sizeof(iflong), 1, fa);
	if(master==0)
		master = calloc (lmaster, iflong? sizeof(long): sizeof(unsigned));
	hpt = calloc(nhash, sizeof(*hpt));
	kk=fread( hpt, sizeof(*hpt), nhash, fa);
# if D1
	fprintf(stderr,"read %d hashes, iflong %d, nhash %d\n", kk, iflong, nhash);
# endif
	assert (kk==nhash);
	hfreq = calloc(nhash, sizeof(*hfreq));
	assert (hfreq != NULL);
	frtbl = fread(hfreq, sizeof(*hfreq), nhash, fa);
	hfrflg = (frtbl == nhash);
# if D1
	fprintf(stderr, "read freqs %d\n", frtbl);
# endif

search:
	while (1)
	{
		nitem = getq(qitem);
		if (measure) tick();
		if (nitem==0) continue;
		if (nitem < 0) break;
		if (tagout) tagout[0]=0;
		if (fb!=NULL)
		{
			nfound = doquery(hpt, nhash, fb, nitem, qitem, master);
# if D1
			fprintf(stderr,"after doquery nfound %d\n", nfound);
# endif
			fgnamp=fgnames;
			if (falseflg == 0)
				nfound = baddrop(master, nfound, fc, nitem, qitem, rprog, full);
# if D1
			fprintf(stderr,"after baddrop nfound %d\n", nfound);
# endif
		}
		if (fgnamp>fgnames)
		{
			char **fgp, tgbuff[100];
			int k;
# if D1
			fprintf(stderr, "were %d bad files\n", fgnamp-fgnames);
# endif
			memset(tgbuff, 0, sizeof (tgbuff));
			grepquery[0]=0;
			for(k=0; k<nitem; k++)
			{
				n_strcat(grepquery, " ", sizeof(grepquery));
				n_strcat(grepquery, qitem[k],
				    sizeof(grepquery));
			}
# if D1
			fprintf(stderr, "grepquery %s\n",grepquery);
# endif
			for(fgp=fgnames; fgp<fgnamp; fgp++)
			{
# if D1
				fprintf(stderr, "Now on %s query /%s/\n", *fgp, grepquery);
# endif
				makefgrep(*fgp);
# if D1
				fprintf(stderr, "grepmade\n");
# endif
				if (tagout==0)
					tagout=tgbuff;
				grepcall(grepquery, tagout, *fgp);
# if D1
				fprintf(stderr, "tagout now /%s/\n", tagout);
# endif
				if (full)
				{
					int nout;
					char *bout;
					char *tagp;
					char *oldtagp;
					tagp = tagout;
					while (*tagp) {
						oldtagp = tagp;
						while (*tagp && (*tagp != '\n')) 
							tagp++;
						if (*tagp) 
							tagp++;
				                nout = findline(oldtagp, &bout, 1000, 0L);
						if (nout > 0)
						{
							fputs(bout, stdout);
							free(bout); 
						}
					}
				}
			}
		}
		if (tags)
			result (master, nfound >tags ? tags: nfound, fc);
		if (measure) tock();
	}
	/* NOTREACHED */
	return 0;
}

char *
todir(char *t)
{
	char *s;
	s=t;
	while (*s) s++;
	while (s>=t && *s != '/') s--;
	if (s<t) return(t);
	*s++ = 0;
	t = (*t ? t : "/");
	chdir (t);
	n_strcpy (usedir,t, sizeof(usedir));
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
