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

/*	from OpenSolaris "refer7.c	1.4	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)refer7.c	1.3 (gritter) 10/22/05
 */

#include "refer..c"
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int newr[250];

int
chkdup(const char *tag)
{
	int i;

	for(i = 1; i <= refnum; i++) {
		if (reftable[i] && strcmp(reftable[i], tag)==0)
			return(i);
	}
	reftable[refnum+1] = rtp;
	if (refnum >= NRFTBL)
		err("too many references (%d) for table", refnum);
	n_strcpy(rtp, tag, NRFTXT - (rtp - reftext));
	while (*rtp++);
	if (rtp > reftext + NRFTXT)
		err("reference pointers too long (%d)", rtp-reftext);
	return(0);
}

void
dumpold(void)
{
	FILE *fi;
	int c, g1 = 0, nr = 1;

	if (!endpush)
		return;
	fclose(fo);
	fo = NULL;
	if (sort) {
		char comm[100];
		snprintf(comm, sizeof(comm), "sort -f %s -o %s", tfile, tfile);
		system(comm);
	}
	fi = fopen(tfile, "r");
	if (fi == NULL)
		return;
	flout();
	fprintf(ftemp, ".]<\n");
	while ((c = getc(fi)) > 0) {
		if (c == '\n') {
			nr++;
			g1 = 0;
		}
		if (c == sep)
			c = '\n';
		if (c == FLAG) {
			/* make old-new ref number table */
			char tb[20];
			char *s = tb;
			while ((c = getc(fi)) != FLAG)
				*s++ = c;
			*s = 0;
			if (g1++ == 0)
				newr[atoi(tb)] = nr;
#if EBUG
			fprintf(stderr,
				"nr %d assigned to atoi(tb) %d\n",nr,atoi(tb));
# endif
			fprintf(ftemp,"%d", nr);
			continue;
		}
		putc(c, ftemp);
	}
	fclose(fi);
#ifndef TF
	unlink(tfile);
#endif
	fprintf(ftemp, ".]>\n");
}

void
recopy1 (char *fnam)
{
	int c;
	int *wref = NULL;
	int wcnt = 0;
	int wsize = 50;
	int finalrn;
	char sig[MXSIG];

	wref = calloc((unsigned)wsize, (unsigned)sizeof(int));
	fclose(ftemp);
	ftemp = fopen(fnam, "r");
	if (ftemp == NULL) {
		fprintf(stderr, "Can't reopen %s\n", fnam);
		exit(1);
	}
	while ((c = getc(ftemp)) != EOF) {
		if (c == FLAG) {
			char tb[10];
			char *s = tb;
			while ((c = getc(ftemp)) != FLAG)
				*s++ = c;
			*s = 0;
			/*
			 * If sort was done, permute the reference number
			 * to obtain the final reference number, finalrn.
			 */
			if (sort)
				finalrn = newr[atoi(tb)];
			else
				finalrn = atoi(tb);
			if ((++wcnt > wsize) && 
			 ((wref=realloc(wref,(wsize+=50)*sizeof(int)))==NULL)){
				fprintf(stderr, "Ref condense out of memory.");
				exit(1);
			}
			wref[wcnt-1] = finalrn;
			if ((c = getc(ftemp)) == AFLAG) 
				continue;
			wref[wcnt] = 0;
			condense(wref,wcnt,sig);
			wcnt = 0;
			printf("%s", sig);
		}
		putchar(c);
	}
	fclose(ftemp);
	unlink(fnam);
}

/*
 * sort and condense reference signals when they are placed in
 * the text. Viz, the signal 1,2,3,4 is condensed to 1-4 and signals
 * of the form 5,2,9 are converted to 2,5,9
 */
void
condense(int *wref, int wcnt, char *sig)
{
	register int i = 0;
	char wt[4];

	qsort(wref, wcnt, sizeof(int), wswap);
	sig[0] = 0;
	while (i < wcnt) {
		snprintf(wt, sizeof(wt), "%d",wref[i]);
		n_strcat(sig,wt, MXSIG);
		if ((i+2 < wcnt) && (wref[i] == (wref[i+2] - 2))) {
			while (wref[i] == (wref[i+1] - 1))
				i++;
			n_strcat(sig, "-", MXSIG);
		} else if (++i < wcnt)
			n_strcat(sig,",\\|", MXSIG);
	}
}

int
wswap(register const void *iw1, register const void *iw2)
{
	return(*(const int *)iw1 - *(const int *)iw2);
}
