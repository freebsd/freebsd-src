/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)pac.c	5.5 (Berkeley) 6/1/90";
#endif /* not lint */

/*
 * Do Printer accounting summary.
 * Currently, usage is
 *	pac [-Pprinter] [-pprice] [-s] [-r] [-c] [-m] [user ...]
 * to print the usage information for the named people.
 */

#include <stdio.h>
#include "lp.local.h"

char	*printer;			/* printer name */
char	*acctfile;			/* accounting file (input data) */
char	*sumfile;			/* summary file */
float	price = 0.02;			/* cost per page (or what ever) */
int	allflag = 1;			/* Get stats on everybody */
int	sort;				/* Sort by cost */
int	summarize;			/* Compress accounting file */
int	reverse;			/* Reverse sort order */
int	hcount;				/* Count of hash entries */
int	errs;
int	mflag = 0;			/* disregard machine names */
int	pflag = 0;			/* 1 if -p on cmd line */
int	price100;			/* per-page cost in 100th of a cent */
char	*index();
int	pgetnum();

/*
 * Grossness follows:
 *  Names to be accumulated are hashed into the following
 *  table.
 */

#define	HSHSIZE	97			/* Number of hash buckets */

struct hent {
	struct	hent *h_link;		/* Forward hash link */
	char	*h_name;		/* Name of this user */
	float	h_feetpages;		/* Feet or pages of paper */
	int	h_count;		/* Number of runs */
};

struct	hent	*hashtab[HSHSIZE];	/* Hash table proper */
struct	hent	*enter();
struct	hent	*lookup();

#define	NIL	((struct hent *) 0)	/* The big zero */

double	atof();
char	*getenv();
char	*pgetstr();

main(argc, argv)
	char **argv;
{
	register FILE *acct;
	register char *cp;

	while (--argc) {
		cp = *++argv;
		if (*cp++ == '-') {
			switch(*cp++) {
			case 'P':
				/*
				 * Printer name.
				 */
				printer = cp;
				continue;

			case 'p':
				/*
				 * get the price.
				 */
				price = atof(cp);
				pflag = 1;
				continue;

			case 's':
				/*
				 * Summarize and compress accounting file.
				 */
				summarize++;
				continue;

			case 'c':
				/*
				 * Sort by cost.
				 */
				sort++;
				continue;

			case 'm':
				/*
				 * disregard machine names for each user
				 */
				mflag = 1;
				continue;

			case 'r':
				/*
				 * Reverse sorting order.
				 */
				reverse++;
				continue;

			default:
fprintf(stderr,
    "usage: pac [-Pprinter] [-pprice] [-s] [-c] [-r] [-m] [user ...]\n");
				exit(1);
			}
		}
		(void) enter(--cp);
		allflag = 0;
	}
	if (printer == NULL && (printer = getenv("PRINTER")) == NULL)
		printer = DEFLP;
	if (!chkprinter(printer)) {
		printf("pac: unknown printer %s\n", printer);
		exit(2);
	}

	if ((acct = fopen(acctfile, "r")) == NULL) {
		perror(acctfile);
		exit(1);
	}
	account(acct);
	fclose(acct);
	if ((acct = fopen(sumfile, "r")) != NULL) {
		account(acct);
		fclose(acct);
	}
	if (summarize)
		rewrite();
	else
		dumpit();
	exit(errs);
}

/*
 * Read the entire accounting file, accumulating statistics
 * for the users that we have in the hash table.  If allflag
 * is set, then just gather the facts on everyone.
 * Note that we must accomodate both the active and summary file
 * formats here.
 * Host names are ignored if the -m flag is present.
 */

account(acct)
	register FILE *acct;
{
	char linebuf[BUFSIZ];
	double t;
	register char *cp, *cp2;
	register struct hent *hp;
	register int ic;

	while (fgets(linebuf, BUFSIZ, acct) != NULL) {
		cp = linebuf;
		while (any(*cp, " t\t"))
			cp++;
		t = atof(cp);
		while (any(*cp, ".0123456789"))
			cp++;
		while (any(*cp, " \t"))
			cp++;
		for (cp2 = cp; !any(*cp2, " \t\n"); cp2++)
			;
		ic = atoi(cp2);
		*cp2 = '\0';
		if (mflag && index(cp, ':'))
		    cp = index(cp, ':') + 1;
		hp = lookup(cp);
		if (hp == NIL) {
			if (!allflag)
				continue;
			hp = enter(cp);
		}
		hp->h_feetpages += t;
		if (ic)
			hp->h_count += ic;
		else
			hp->h_count++;
	}
}

/*
 * Sort the hashed entries by name or footage
 * and print it all out.
 */

dumpit()
{
	struct hent **base;
	register struct hent *hp, **ap;
	register int hno, c, runs;
	float feet;
	int qucmp();

	hp = hashtab[0];
	hno = 1;
	base = (struct hent **) calloc(sizeof hp, hcount);
	for (ap = base, c = hcount; c--; ap++) {
		while (hp == NIL)
			hp = hashtab[hno++];
		*ap = hp;
		hp = hp->h_link;
	}
	qsort(base, hcount, sizeof hp, qucmp);
	printf("  Login               pages/feet   runs    price\n");
	feet = 0.0;
	runs = 0;
	for (ap = base, c = hcount; c--; ap++) {
		hp = *ap;
		runs += hp->h_count;
		feet += hp->h_feetpages;
		printf("%-24s %7.2f %4d   $%6.2f\n", hp->h_name,
		    hp->h_feetpages, hp->h_count, hp->h_feetpages * price);
	}
	if (allflag) {
		printf("\n");
		printf("%-24s %7.2f %4d   $%6.2f\n", "total", feet, 
		    runs, feet * price);
	}
}

/*
 * Rewrite the summary file with the summary information we have accumulated.
 */

rewrite()
{
	register struct hent *hp;
	register int i;
	register FILE *acctf;

	if ((acctf = fopen(sumfile, "w")) == NULL) {
		perror(sumfile);
		errs++;
		return;
	}
	for (i = 0; i < HSHSIZE; i++) {
		hp = hashtab[i];
		while (hp != NULL) {
			fprintf(acctf, "%7.2f\t%s\t%d\n", hp->h_feetpages,
			    hp->h_name, hp->h_count);
			hp = hp->h_link;
		}
	}
	fflush(acctf);
	if (ferror(acctf)) {
		perror(sumfile);
		errs++;
	}
	fclose(acctf);
	if ((acctf = fopen(acctfile, "w")) == NULL)
		perror(acctfile);
	else
		fclose(acctf);
}

/*
 * Hashing routines.
 */

/*
 * Enter the name into the hash table and return the pointer allocated.
 */

struct hent *
enter(name)
	char name[];
{
	register struct hent *hp;
	register int h;

	if ((hp = lookup(name)) != NIL)
		return(hp);
	h = hash(name);
	hcount++;
	hp = (struct hent *) calloc(sizeof *hp, 1);
	hp->h_name = (char *) calloc(sizeof(char), strlen(name)+1);
	strcpy(hp->h_name, name);
	hp->h_feetpages = 0.0;
	hp->h_count = 0;
	hp->h_link = hashtab[h];
	hashtab[h] = hp;
	return(hp);
}

/*
 * Lookup a name in the hash table and return a pointer
 * to it.
 */

struct hent *
lookup(name)
	char name[];
{
	register int h;
	register struct hent *hp;

	h = hash(name);
	for (hp = hashtab[h]; hp != NIL; hp = hp->h_link)
		if (strcmp(hp->h_name, name) == 0)
			return(hp);
	return(NIL);
}

/*
 * Hash the passed name and return the index in
 * the hash table to begin the search.
 */

hash(name)
	char name[];
{
	register int h;
	register char *cp;

	for (cp = name, h = 0; *cp; h = (h << 2) + *cp++)
		;
	return((h & 0x7fffffff) % HSHSIZE);
}

/*
 * Other stuff
 */

any(ch, str)
	char str[];
{
	register int c = ch;
	register char *cp = str;

	while (*cp)
		if (*cp++ == c)
			return(1);
	return(0);
}

/*
 * The qsort comparison routine.
 * The comparison is ascii collating order
 * or by feet of typesetter film, according to sort.
 */

qucmp(left, right)
	struct hent **left, **right;
{
	register struct hent *h1, *h2;
	register int r;

	h1 = *left;
	h2 = *right;
	if (sort)
		r = h1->h_feetpages < h2->h_feetpages ? -1 : h1->h_feetpages > 
h2->h_feetpages;
	else
		r = strcmp(h1->h_name, h2->h_name);
	return(reverse ? -r : r);
}

/*
 * Perform lookup for printer name or abbreviation --
 */
chkprinter(s)
	register char *s;
{
	static char buf[BUFSIZ/2];
	char b[BUFSIZ];
	int stat;
	char *bp = buf;

	if ((stat = pgetent(b, s)) < 0) {
		printf("pac: can't open printer description file\n");
		exit(3);
	} else if (stat == 0)
		return(0);
	if ((acctfile = pgetstr("af", &bp)) == NULL) {
		printf("accounting not enabled for printer %s\n", printer);
		exit(2);
	}
	if (!pflag && (price100 = pgetnum("pc")) > 0)
		price = price100/10000.0;
	sumfile = (char *) calloc(sizeof(char), strlen(acctfile)+5);
	if (sumfile == NULL) {
		perror("pac");
		exit(1);
	}
	strcpy(sumfile, acctfile);
	strcat(sumfile, "_sum");
	return(1);
}
