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

/*	from OpenSolaris "refer5.c	1.5	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)refer5.c	1.3 (gritter) 10/22/05
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "refer..c"
#define SAME 0
#define NFLAB 3000		/* number of bytes to record all labels */
#define NLABC 1000		/* max number of labels */

static char sig[MXSIG];
static char bflab[NFLAB];
static char *labtab[NLABC];
static char *lbp = bflab;
static char labc[NLABC];
static char stbuff[50];
static int  prevsig;

void
putsig (int nf, char **flds, int nref, char *nstline,
		char *endline, int toindex)   /* choose signal style */
{
	char t[100], t1[MXSIG], t2[100], format[10], *sd, *stline;
	int addon = 0, another = 0;
	static FILE *fhide = 0;

	if (labels) {
		if (nf == 0)	/* old */
			snprintf(t, sizeof(t), "%s%c", labtab[nref],
			    labc[nref]);
		else {
			*t = 0;
			if (keywant)
				fpar(nf, flds, t, sizeof(t), keywant, 1, 0);
			if (science && t[0] == 0) {
				if (fpar(nf, flds, t, sizeof(t), 'A', 1, 0)
				    != 0) {
					if (fpar(nf, flds, t2, sizeof(t2),
					    'D', 1, 0) != 0) {
						n_strcat(t, ", ", sizeof(t));
						n_strcat(t, t2, sizeof(t));
					}
				}
			} else if (t[0] == 0) {
				if (nmlen > 0) {
					snprintf(format, sizeof(format),
						"%%.%ds%%s", nmlen);
				} else {
					snprintf(format, sizeof(format),
						"%%s%%s");
				}
				/* format is %s%s for default labels */
				/* or %.3s%s eg if wanted */
				if (fpar(nf, flds, t2, sizeof(t2), 'D', 1, 0)) {
					sd = t2;
					if (dtlen > 0) {
						int n = strlen(sd) - dtlen;
						if (n > 0)
							sd += n;
					}
				} else {
					sd = "";
				}
				t1[0] = 0;
				fpar(nf, flds, t1, sizeof(t1), 'A', 1, 0);
				snprintf(t, sizeof(t), format, t1, sd);
			}
			if (keywant) {
				addon = 0;
				for (sd = t; *sd; sd++)
					;
				if (*--sd == '-') {
					addon = 1;
					*sd = 0;
				}
			}
			if ((!keywant || addon) && !science) {
			    addch(t, keylet(t, nref));
			}
			else {
			    tokeytab (t,nref);
			}
		}
	}
	else {
		if (sort)
			snprintf(t, sizeof(t), "%c%d%c", FLAG, nref, FLAG);
		else
			snprintf(t, sizeof(t), "%d", nref);
	}
	another = (sd = lookat()) ? prefix(".[", sd) : 0;
	if (another && (strcmp(".[\n", sd) != SAME))
		fprintf(stderr, (char *)"File %s line %d: punctuation ignored from: %s",
			Ifile, Iline, sd);
	if ((strlen(sig) + strlen(t)) > MXSIG)
		err("sig overflow (%d)", MXSIG);
	n_strcat(sig, t, sizeof(sig));
#if EBUG
	fprintf(stderr, "sig is now %s leng %d\n",sig,strlen(sig));
#endif
	trimnl(nstline);
	trimnl(endline);
	stline = stbuff;
	if (prevsig == 0) {
		n_strcpy (stline, nstline, sizeof(stbuff));
		prevsig=1;
	}
	if (stline[2] || endline[2]) {
		stline += 2;
		endline += 2;
	}
	else {
		stline  = "\\*([.";
		endline = "\\*(.]";
	}
	if (science) {
		stline = " (";
		endline = ")";
	}
	if (bare == 0) {
		if (!another) {
			snprintf(t1, sizeof(t1), "%s%s%s\n", stline, sig,
			    endline);
			if (strlen(t1) > MXSIG)
				err("t1 overflow (%d)", MXSIG);
			append(t1);
			flout();
			sig[0] = 0;
			prevsig = 0;
			if (fo == fhide) {
				int ch;
				fclose(fhide); 
				fhide = fopen(hidenam, "r");
				fo = ftemp;
				while ((ch = getc(fhide)) != EOF)
					putc(ch, fo);
				fclose(fhide);
				unlink(hidenam);
			}
		}
		else {
			if (labels) {
				n_strcat(sig, ",\\|", sizeof(sig));
			} else {
				/*
				 * Seperate reference numbers with AFLAG
				 * for later sorting and condensing.
				 */
				t1[0] = AFLAG;
				t1[1] = '\0';
				n_strcat(sig, t1, sizeof(sig));
			}
			if (fo == ftemp) {	/* hide if need be */
				snprintf(hidenam, NTFILE,
				    "/tmp/rj%dc", (int)getpid());
#if EBUG
				fprintf(stderr, "hiding in %s\n", hidenam);
#endif
				fhide = fopen(hidenam, "w");
				if (fhide == NULL)
					err("Can't get scratch file %s",
						hidenam);
				fo = fhide;
			}
		}
	}
	if (bare < 2)
		if (nf > 0 && toindex)
			fprintf(fo,".ds [F %s%c",t,sep);
	if (bare > 0)
		flout();
#if EBUG
	fprintf(stderr, "sig is now %s\n",sig);
#endif
}

char *
fpar (int nf, char **flds, char *out, size_t outsiz, int c, int seq,
    int prepend)
{
	char *p, *s;
	int i, fnd = 0;

	for(i = 0; i < nf; i++)
		if (flds[i][1] == c && ++fnd >= seq) {
			/* for titles use first word otherwise last */
			if (c == 'T' || c == 'J') {
				p = flds[i]+3;
				if (prefix("A ", p))
					p += 2;
				if (prefix("An ", p))
					p += 3;
				if (prefix("The ", p))
					p += 4;
				mycpy2(out, p, 20);
				return(out);
			}
			/* if its not 'L' then use just the last word */
			s = p = flds[i]+2;
			if (c != 'L') {
			    for(; *p; p++);
			    while (p > s && *p != ' ')
				    p--;
			}
			/* special wart for authors */
			if (c == 'A' && (p[-1] == ',' || p[1] =='(')) {
				p--;
				while (p > s && *p != ' ')
					p--;
				mycpy(out, p+1);
			}
			else
				n_strcpy(out, p+1, outsiz);
			if (c == 'A' && prepend)
				initadd(out, flds[i]+2, p);
			return(out);
		}
	return(0);
}

void
putkey(int nf, char **flds, int nref, char *keystr)
{
	char t1[50], *sf;
	int ctype, i, count;

	fprintf(fo, ".\\\"");
	if (nf <= 0)
		fprintf(fo, "%s%c%c", labtab[nref], labc[nref], sep);
	else {
		while ((ctype = *keystr++)) {
			count = atoi(keystr);
			if (*keystr=='+')
				count=999;
			if (count <= 0)
				count = 1;
			for(i = 1; i <= count; i++) {
				sf = fpar(nf, flds, t1, sizeof(t1), ctype, i, 1);
				if (sf == 0)
					break;
				sf = artskp(sf);
				fprintf(fo, "%s%c", sf, '-');
			}
		}
		fprintf(fo, "%c%d%c%c", FLAG, nref, FLAG, sep);
	}
}


void
tokeytab (const char *t, int nref)
{
	n_strcpy(labtab[nref]=lbp, t, sizeof(bflab) - (lbp - bflab));
	while (*lbp++)
		;
}

int
keylet(char *t, int nref)
{
	int i;
	int x = 'a' - 1;

	for(i = 1; i < nref; i++) {
		if (strcmp(labtab[i], t) == 0)
			x = labc[i];
	}
	tokeytab (t, nref);
	if (lbp-bflab > NFLAB)
		err("bflab overflow (%d)", NFLAB);
	if (nref > NLABC)
		err("nref in labc overflow (%d)", NLABC);
#if EBUG
	fprintf(stderr, "lbp up to %d of %d\n", lbp-bflab, NFLAB);
#endif
	return(labc[nref] = x+1);
}

void
mycpy(char *s, const char *t)
{
	while (*t && *t != ',' && *t != ' ')
		*s++ = *t++;
	*s = 0;
}

void
mycpy2(char *s, const char *t, int n)
{
	int c;

	while (n-- && (c= *t++) > 0) {
		if (c == ' ')
			c = '-';
		*s++ = c;
	}
	*s = 0;
}

void
initadd(char *to, const char *from, const char *stop)
{
	int c, nalph = 1;

	while (*to)
		to++;
	while (from < stop) {
		c = *from++;
		if (!isalpha(c)) {
			if (nalph)
				*to++ = '.';
			nalph = 0;
			continue;
		}
		if (nalph++ == 0)
			*to++ = c;
	}
	*to = 0;
}

static char *articles[] = {
	"the ", "an ", "a ", 0
};

char *
artskp(char *s)	/* skips over initial "a ", "an ", "the " in s */
{

	char **p, *r1, *r2;

	for (p = articles; *p; p++) {
		r2 = s;
		for (r1 = *p; ((*r1 ^ *r2) & ~040 ) == 0; r1++)
			r2++;
		if (*r1 == 0 && *r2 != 0)
			return(r2);
	}
	return(s);
}
