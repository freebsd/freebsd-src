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

/*	from OpenSolaris "refer2.c	1.4	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)refer2.c	1.4 (gritter) 9/7/08
 */

#include "refer..c"
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#define NFLD 30
#define TLEN PATH_MAX

extern FILE *in;
char one[ANSLEN];
int onelen = ANSLEN;
static char dr [100] = "";

void
doref(char *line1)
{
	char buff[QLEN], dbuff[3*QLEN];
	char answer[ANSLEN], temp[TLEN], line[BUFSIZ];
	char *p, **sr, *flds[NFLD], *r;
	int nf, nr, query = 0, alph, digs;

   again:
	buff[0] = dbuff[0] = 0;
	if (biblio && Iline == 1 && line1[0] == '%')
		n_strcat(dbuff, line1, sizeof(dbuff));
	while (input(line, sizeof(line))) {		/* get query */
		Iline++;
		if (prefix(".]", line))
			break;
		if (biblio && line[0] == '\n')
			break;
		if (biblio && line[0] == '%' && line[1] == *convert)
			break;
		if (control(line[0]))
			query = 1;
		n_strcat(query ? dbuff : buff, line, query ?
		    sizeof(dbuff) : sizeof(buff));
		if (strlen(buff) > QLEN)
			err("query too long (%d)", strlen(buff));
		if (strlen(dbuff) > 3 * QLEN)
			err("record at line %d too long", Iline-1);
	}
	if (biblio && line[0] == '\n' && feof(in))
		return;
	if (strcmp(buff, "$LIST$\n")==0) {
		assert (dbuff[0] == 0);
		dumpold();
		return;
	}
	answer[0] = 0;
	for (p = buff; *p; p++) {
		if (isupper((int)*p))
			*p |= 040;
	}
	alph = digs = 0;
	for (p = buff; *p; p++) {
		if (isalpha((int)*p))
			alph++;
		else
			if (isdigit((int)*p))
				digs++;
			else {
				*p = 0;
				if ((alph+digs < 3) || common(p-alph)) {
					r = p-alph;
					while (r < p)
						*r++ = ' ';
				}
				if (alph == 0 && digs > 0) {
					r = p-digs;
					if (digs != 4 || atoi(r)/100 != 19) { 
						while (r < p)
							*r++ = ' ';
					}
				}
				*p = ' ';
				alph = digs = 0;
			}
	}
	one[0] = 0;
	if (buff[0]) {	/* do not search if no query */
		for (sr = rdata; sr < search; sr++) {
			temp[0] = 0;
			corout(buff, temp, "hunt", *sr, TLEN);
			assert(strlen(temp) < TLEN);
			if (strlen(temp)+strlen(answer) > BUFSIZ)
				err("Accumulated answers too large",0);
			n_strcat(answer, temp, sizeof(answer));
			if (strlen(answer)>BUFSIZ)
				err("answer too long (%d)", strlen(answer));
			if (newline(answer) > 0)
				break;
		}
	}
	assert(strlen(one) < ANSLEN);
	assert(strlen(answer) < ANSLEN);
	if (buff[0])
		switch (newline(answer)) {
		case 0:
			fprintf(stderr, "No such paper: %s\n", buff);
			return;
		default:
			fprintf(stderr, "Too many hits: %s\n", trimnl(buff));
			choices(answer);
			p = buff;
			while (*p != '\n')
				p++;
			*++p = 0;
		case 1:
			if (endpush)
				if ((nr = chkdup(answer))) {
					if (bare < 2) {
						nf = tabs(flds, one);
						nf += tabs(flds+nf, dbuff);
						assert(nf < NFLD);
						putsig(nf,flds,nr,line1,line,0);
					}
					return;
				}
			if (one[0] == 0)
				corout(answer, one, "deliv", dr, QLEN);
			break;
		}
	assert(strlen(buff) < QLEN);
	assert(strlen(one) < ANSLEN);
	nf = tabs(flds, one);
	nf += tabs(flds+nf, dbuff);
	assert(nf < NFLD);
	refnum++;
	if (sort)
		putkey(nf, flds, refnum, keystr);
	if (bare < 2)
		putsig(nf, flds, refnum, line1, line, 1);
	else
		flout();
	putref(nf, flds);
	if (biblio && line[0] == '\n')
		goto again;
	if (biblio && line[0] == '%' && line[1] == *convert)
		fprintf(fo, "%s%c%s", convert+1, sep, line+3);
}

int
newline(const char *s)
{
	int k = 0, c;

	while ((c = *s++))
		if (c == '\n')
		k++;
	return(k);
}

void
choices(char *buff)
{
	char ob[BUFSIZ], *p, *r, *q, *t;
	int nl;

	for (r = p = buff; *p; p++) {
		if (*p == '\n') {
			*p++ = 0;
		corout(r, ob, "deliv", dr, BUFSIZ);
			nl = 1;
			for (q = ob; *q; q++) {
				if (nl && (q[0]=='.'||q[0]=='%') && q[1]=='T') {
					q += 3;
					for (t = q; *t && *t != '\n'; t++)
						;
				*t = 0;
					fprintf(stderr, "%.70s\n", q);
					q = 0; 
				break;
			}
				nl = *q == '\n';
		}
			if (q)
			fprintf(stderr, "??? at %s\n",r);
			r=p;
		}
	}
}

int
control(int c)
{
	if (c == '.')
		return(1);
	if (c == '%')
		return(1);
	return(0);
}
