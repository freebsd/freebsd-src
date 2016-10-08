/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	from OpenSolaris "checkeq.c	1.6	05/06/02 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 */
#if __GNUC__ >= 3 && __GNUC_MINOR__ >= 4 || __GNUC__ >= 4
#define	USED	__attribute__ ((used))
#elif defined __GNUC__
#define	USED	__attribute__ ((unused))
#else
#define	USED
#endif
static const char sccsid[] USED = "@(#)/usr/ucb/checkeq.sl	4.1 (gritter) 9/15/05";

#include <stdio.h>
#include <stdlib.h>

static void check(FILE *);
static char *fgetline(char **, size_t *, FILE *);

static	FILE	*fin;
static	int	delim	= '$';

int
main(int argc, char **argv)
{
	if (argc <= 1)
		check(stdin);
	else
		while (--argc > 0) {
			if ((fin = fopen(*++argv, "r")) == NULL) {
				perror(*argv);
				exit(1);
			}
			printf("%s:\n", *argv);
			check(fin);
			fclose(fin);
		}
	return (0);
}

static void
check(FILE *f)
{
	int start, line, eq, ndel, totdel;
	char *in = NULL, *p;
	size_t insize = 0;

	start = eq = line = ndel = totdel = 0;
	while (fgetline(&in, &insize, f) != NULL) {
		line++;
		ndel = 0;
		for (p = in; *p; p++)
			if (*p == delim)
				ndel++;
		if (*in == '.' && *(in+1) == 'E' && *(in+2) == 'Q') {
			if (eq++)
				printf("   Spurious EQ, line %d\n", line);
			if (totdel)
				printf("   EQ in %c%c, line %d\n",
				    delim, delim, line);
		} else if (*in == '.' && *(in+1) == 'E' && *(in+2) == 'N') {
			if (eq == 0)
				printf("   Spurious EN, line %d\n", line);
			else
				eq = 0;
			if (totdel > 0)
				printf("   EN in %c%c, line %d\n",
				    delim, delim, line);
			start = 0;
		} else if (eq && *in == 'd' && *(in+1) == 'e' &&
		    *(in+2) == 'l' && *(in+3) == 'i' && *(in+4) == 'm') {
			for (p = in+5; *p; p++)
				if (*p != ' ') {
					if (*p == 'o' && *(p+1) == 'f')
						delim = 0;
					else
						delim = *p;
					break;
				}
			if (delim == 0)
				printf("   Delim off, line %d\n", line);
			else
				printf("   New delims %c%c, line %d\n",
				    delim, delim, line);
		}
		if (ndel > 0 && eq > 0)
			printf("   %c%c in EQ, line %d\n", delim,
			    delim, line);
		if (ndel == 0)
			continue;
		totdel += ndel;
		if (totdel%2) {
			if (start == 0)
				start = line;
			else {
				printf("   %d line %c%c, lines %d-%d\n",
				    line-start+1, delim, delim, start, line);
				start = line;
			}
		} else {
			if (start > 0) {
				printf("   %d line %c%c, lines %d-%d\n",
				    line-start+1, delim, delim, start, line);
				start = 0;
			}
			totdel = 0;
		}
	}
	if (totdel)
		printf("   Unfinished %c%c\n", delim, delim);
	if (eq)
		printf("   Unfinished EQ\n");
}

static char *
fgetline(char **lp, size_t *zp, FILE *fp)
{
	size_t	n = 0;
	int	c;

	while ((c = getc(fp)) != EOF) {
		if (n >= *zp)
			*lp = realloc(*lp, *zp += 600);
		(*lp)[n++] = c;
		if (c == '\n')
			break;
	}
	if (n >= *zp)
		*lp = realloc(*lp, *zp += 600);
	(*lp)[n] = 0;
	return c != EOF ? *lp : NULL;
}
