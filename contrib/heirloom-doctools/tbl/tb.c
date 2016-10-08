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
  
/*	from OpenSolaris "tb.c	1.5	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)tb.c	1.7 (gritter) 9/8/06
 */

 /* tb.c: check which entries exist, also storage allocation */
# include "t..c"
# include <stdlib.h>
# include <string.h>
# include <inttypes.h>
void
checkuse(void)
{
int i,c, k;
for(c=0; c<ncol; c++)
	{
	used[c]=lused[c]=rused[c]=0;
	for(i=0; i<nlin; i++)
		{
		if (instead[i] || fullbot[i]) continue;
		k = ctype(i,c);
		if (k== '-' || k == '=') continue;
		if ((k=='n'||k=='a'))
			{
			rused[c]|= real(table[i][c].rcol);
			if( !real(table[i][c].rcol))
			used[c] |= real(table[i][c].col);
			if (table[i][c].rcol)
			lused[c] |= real(table[i][c].col);
			}
		else
			used[c] |= real(table[i][c].col);
		}
	}
}
int 
real(char *s)
{
if (s==0) return(0);
if (!point((intptr_t)s)) return(1);
if (*s==0) return(0);
return(1);
}
int spcount = 0;
int maxvec = 0;
char **spvecs;

char *
chspace(void) {
	char *pp;
	int n;
	if (spcount>=maxvec) {
		n = maxvec + 20;
		spvecs = realloc(spvecs, n * sizeof *spvecs);
		if (spvecs == NULL) {
			error("Too many characters in table");
			return NULL;
		}
		do
			spvecs[maxvec] = 0;
		while (++maxvec < n);
	}
	if (spvecs[spcount])
		return(spvecs[spcount++]);
	spvecs[spcount++]= pp = calloc(MAXCHS+MAXSTR,1);
	if (pp == 0) {
		error("no space for characters");
		return NULL;
	}
	return(pp);
}

void
updspace(char *old, char *new, int area)
{
int i, j, d = new - old;
for (i = 0; i < spcount; i++)
	if (spvecs[i] == old) {
		spvecs[i] = new;
		for (i = 0; i < nlin; i++) {
			if (instead[i] >= old && instead[i] < &old[area])
				instead[i] += d;
			if (table[i])
				for (j = 0; j <= ncol; j++)
					if (table[i][j].col >= old &&
					    table[i][j].col < &old[area])
						table[i][j].col += d;
		}
		if (leftover >= old && leftover < &old[area])
			leftover += d;
		break;
	}
}

static int MAXPC;
char *thisvec;
int tpcount = -1;
char **tpvecs;

struct colstr *
alocv(int n) {
	int *tp, *q;
	if (tpcount<0 || thisvec+n > tpvecs[tpcount]+MAXCHS) {
		tpcount++;
		if (tpcount >= MAXPC) {
			tpvecs = realloc(tpvecs, (tpcount+1) * sizeof *tpvecs);
			if (tpvecs == 0) goto nospace;
			memset(&tpvecs[MAXPC], 0, (tpcount+1-MAXPC) * sizeof *tpvecs);
			MAXPC = tpcount+1;
		}
		if (tpvecs[tpcount]==0) {
			tpvecs[tpcount] = calloc(MAXCHS,1);
		}
		thisvec = tpvecs[tpcount];
		if (thisvec == 0) {
nospace:
			error("no space for vectors");
			return (struct colstr *)-1;
		}
	}
	tp=(int *)thisvec;
	thisvec+=n;
	for(q=tp; q<(int *)thisvec; q++)
		*q=0;
	return (struct colstr *)tp;
}

void
release(void)
{
extern char *exstore;
/* give back unwanted space in some vectors */
spcount=0;
tpcount= -1;
exstore=0;
}
