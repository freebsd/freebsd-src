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

/*	from OpenSolaris "mkey2.c	1.3	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)mkey2.c	1.3 (gritter) 10/22/05
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "refer..c"
#define MAXLINE 500

static int eof = 0;
static long lp, lim;
static int alph, used, prevc;
static char *p, key[20];

void
dofile(FILE *f, char *name)
{
	/* read file f & spit out keys & ptrs */

	char line[MAXLINE], *s;
	extern int keycount;
	int c;
	extern int wholefile;
	alph=used=prevc=eof=0;

	lp=0;
	if (wholefile==0)
		while ((lim = grec(line,f)))
		{
# if D1
			fprintf(stderr, "line: /%s",line);
# endif
			used=alph=0;
			p = key;
			for(s=line; (c= *s) && (used<keycount); s++)
				chkey(c, name);
			lp += lim;
			if (used) putchar('\n');
		}
	else
	{
		p=key;
		used=alph=0;
		while ( (c=getc(f)) != EOF && used<keycount)
			chkey (c, name);
		if (used) putchar('\n');
	}
	fclose(f);
}

int
outkey(char *ky, int lead, int trail)
{
	int n;
	extern int minlen;
	n = strlen(ky);
	if (n<minlen) return (0);
	if (n<3)
	{
		if (trail == '.') return(0);
		if (mindex(".%,!#$%&'();+:*", lead)!=0) return(0);
	}
	if (isdigit((int)ky[0]))
		/* Allow years 1000 - 2099 */
		if (!(ky[0] == '1' || (ky[0] == '2' && ky[1] == '0')) || n != 4)
			return(0);
	if (common(ky))
		return(0);
	return(1);
}

long
grec (char *s, FILE *f)
{
	char tm[200];
	int curtype = 0;
	long len = 0L, tlen = 0L;
	extern int wholefile;
	extern char *iglist;
	if (eof) return(0);
	*s = 0;
	while (fgets(tm, sizeof tm, f))
	{
		tlen += strlen(tm);
		if (tm[0] == '%' || tm[0] == '.')
			curtype = tm[1];
		if (tlen < MAXLINE && mindex(iglist,curtype)==0)
			n_strcat(s, tm, MAXLINE);
		len = tlen;
		if (wholefile==0 && tm[0] == '\n')
			return(len);
		if (wholefile>0 && len >= MAXLINE)
		{
			fseek (f, 0, SEEK_END);
			return(ftell(f));
		}
	}
	eof=1;
	return(s[0] ? len : 0L);
}

char *
trimnl(char *ln)
{
	register char *p = ln;
	while (*p) p++;
	p--;
	if (*p == '\n') *p=0;
	return(ln);
}

void
chkey (int c, char *name)
{
	extern int labels;
	extern int wholefile;
	if (isalpha(c) || isdigit(c))
	{
		if (alph++ < 6)
			*p++ = c;
	}
	else
	{
		*p = 0;
		for(p=key; *p; p++)
			*p |= 040;
		if (outkey(p=key,prevc,c))
		{
			if (used==0)
			{
				if (labels)
				{
					if (wholefile==0)
						printf("%s:%ld,%ld\t", name, lp, lim);
					else
						printf("%s\t", name);
				}
			}
			else
				putchar(' ');
			fputs(key, stdout);
			used++;
		}
		prevc=c;
		alph=0;
	}
}
