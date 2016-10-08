/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*
 * Copyright (c) 1983-1988 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

/*	from OpenSolaris "sortbib.c	1.9	05/06/02 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)sortbib.c	1.3 (gritter) 10/22/05
 */

#include <locale.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "global.h"

#define	BUF BUFSIZ
#define	MXFILES 16

char tempfile[32];		/* temporary file for sorting keys */
int tmpfd = -1;
char *keystr = "AD";		/* default sorting on author and date */
int multauth = 0;		/* by default sort on senior author only */
int oneauth;			/* has there been author in the record? */

static void sortbib(FILE *, FILE *, int);
static void deliver(FILE **, FILE *);
static void parse(char *, char [][BUF]);
static int article(const char *);
static void eval(char *);
static void error(const char *);
static void onintr(int);
static int endcomma(char *);

int
main(int argc, char **argv)	/* sortbib: sort bibliographic database in place */
{
	FILE *fp[MXFILES], *tfp;
	int i;

	if (argc == 1)		/* can't use stdin for seeking anyway */
	{
		puts("Usage:  sortbib [-sKEYS] database [...]\n\
\t-s: sort by fields in KEYS (default is AD)");
		exit(1);
	}
	if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 's')
	{
		/* if a key is specified use it, otherwise use default key */
		if (argv[1][2] != '\0')
			keystr = argv[1] + 2;
		eval(keystr);		/* evaluate A+ for multiple authors */
		argv++; argc--;
	}
	if (argc > MXFILES+1)	/* too many open file streams */
	{
		fprintf(stderr,
		"sortbib: More than %d databases specified\n",
		    MXFILES);
		exit(1);
	}
	for (i = 1; i < argc; i++)		/* open files in arg list */
		if ((fp[i-1] = fopen(argv[i], "r")) == NULL)
			error(argv[i]);
	/* tempfile for sorting keys */
	n_strcpy(tempfile, "/tmp/SbibXXXXXX", sizeof(tempfile));
	if ((tmpfd = mkstemp(tempfile)) == -1)
		error(tempfile);

	close(tmpfd);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)	/* remove if interrupted */
		signal(SIGINT, onintr);
	if ((tfp = fopen(tempfile, "w")) == NULL) {
		unlink(tempfile);
		error(tempfile);
	}
	for (i = 0; i < argc-1; i++)		/* read keys from bib files */
		sortbib(fp[i], tfp, i);
	fclose(tfp);
	deliver(fp, tfp);	/* do disk seeks and read from biblio files */
	unlink(tempfile);
	return 0;
}

static int rsmode = 0;		/* record separator: 1 = null line, 2 = bracket */

static void
sortbib(FILE *fp, FILE *tfp, int i)	/* read records, prepare list for sorting */
{
	long offset, lastoffset = 0;		/* byte offsets in file */
	int length, newrec = 0, recno = 0;	/* reclen, new rec'd?, number */
	char line[BUF], fld[4][BUF];		/* one line, the sort fields */

	/* measure byte offset, then get new line */
	while (offset = ftell(fp), fgets(line, BUF, fp))
	{
		if (recno == 0)		/* accept record w/o initial newline */
			newrec = 1;
		if (line[0] == '\n')	/* accept null line record separator */
		{
			if (!rsmode)
				rsmode = 1;	/* null line mode */
			if (rsmode == 1)
				newrec = 1;
		}
		if (line[0] == '.' && line[1] == '[')	/* also accept .[ .] */
		{
			if (!rsmode)
				rsmode = 2;	/* bracket pair mode */
			if (rsmode == 2)
				newrec = 1;
		}
		if (newrec)		/* by whatever means above */
		{
			newrec = 0;
			length = offset - lastoffset;	/* measure rec len */
			if (length > BUF*8) {
				fprintf(stderr,
				"sortbib: record %d longer than %d (%d)\n",
					recno, BUF*8, length);
				unlink(tempfile);
				exit(1);
			}
			if (recno++)			/* info for sorting */
			{
				fprintf(tfp, "%d %ld %d : %s %s %s %s\n",
					i, lastoffset, length,
					fld[0], fld[1], fld[2], fld[3]);
				if (ferror(tfp)) {
					unlink(tempfile);
					error(tempfile);
				}
			}
			*fld[0] = *fld[1] = *fld[2] = *fld[3] = 0;
			oneauth = 0;		/* reset number of authors */
			lastoffset = offset;	/* save for next time */
		}
		if (line[0] == '%')	/* parse out fields to be sorted */
			parse(line, fld);
	}
	offset = ftell(fp);		/* measure byte offset at EOF */
	length = offset - lastoffset;	/* measure final record length */
	if (length > BUF*8)
	{
		fprintf(stderr,
		    "sortbib: record %d longer than %d (%d)\n",
		    recno, BUF*8, length);
		unlink(tempfile);
		exit(1);
	}
	if (line[0] != '\n')		/* ignore null line just before EOF */
	{
		fprintf(tfp, "%d %ld %d : %s %s %s %s\n",
			i, lastoffset, length,
			fld[0], fld[1], fld[2], fld[3]);
		if (ferror(tfp)) {
			unlink(tempfile);
			error(tempfile);	/* disk error in /tmp */
		}
	}
}

static void
deliver(FILE **fp, FILE *tfp)	/* deliver sorted entries out of database(s) */
{
	char str[BUF], buff[BUF*8];	/* for tempfile & databases */
	char cmd[80];			/* for using system sort command */
	long int offset;
	int i, length;

	/* when sorting, ignore case distinctions; tab char is ':' */
	snprintf(cmd, sizeof(cmd), "sort +4f +0n +1n %s -o %s", tempfile,
	    tempfile);
	if (system(cmd) == 127) {
		unlink(tempfile);
		error("sortbib");
	}
	tfp = fopen(tempfile, "r");
	while (fgets(str, sizeof (str), tfp))
	{
		/* get file pointer, record offset, and length */
		if (sscanf(str, "%d %ld %d :", &i, &offset, &length) != 3)
			error("sortbib: sorting error");
		/* seek to proper disk location in proper file */
		if (fseek(fp[i], offset, SEEK_SET) == -1) {
			unlink(tempfile);
			error("sortbib");
		}
		/* read exactly one record from bibliography */
		if (fread(buff, sizeof (*buff), length, fp[i]) == 0) {
			unlink(tempfile);
			error("sortbib");
		}
		/* add newline between unseparated records */
		if (buff[0] != '\n' && rsmode == 1)
			putchar('\n');
		/* write record buffer to standard output */
		if (fwrite(buff, sizeof (*buff), length, stdout) == 0) {
			unlink(tempfile);
			error("sortbib");
		}
	}
}

static void
parse(char *line, char fld[][BUF])	/* get fields out of line, prepare for sorting */
{
	char wd[8][BUF/4];
	int n, i, j;

	for (i = 0; i < 8; i++)		/* zap out old strings */
		*wd[i] = 0;
	n = sscanf(line, "%s %s %s %s %s %s %s %s",
		wd[0], wd[1], wd[2], wd[3], wd[4], wd[5], wd[6], wd[7]);
	for (i = 0; i < 4; i++)
	{
		if (wd[0][1] == keystr[i])
		{
			if (wd[0][1] == 'A')
			{
				if (oneauth && !multauth)	/* no repeat */
					break;
				else if (oneauth)		/* mult auths */
					n_strcat(fld[i], "~~", BUF);
				if (!endcomma(wd[n-2]))		/* surname */
					n_strcat(fld[i], wd[n-1], BUF);
				else {				/* jr. or ed. */
					n_strcat(fld[i], wd[n-2], BUF);
					n--;
				}
				n_strcat(fld[i], " ", BUF);
				for (j = 1; j < n-1; j++)
					n_strcat(fld[i], wd[j], BUF);
				oneauth = 1;
			} else if (wd[0][1] == 'D') {
				n_strcat(fld[i], wd[n-1], BUF);	/* year */
				if (n > 2)
					n_strcat(fld[i], wd[1], BUF);	/* month */
			} else if (wd[0][1] == 'T' || wd[0][1] == 'J') {
				j = 1;
				if (article(wd[1]))	/* skip article */
					j++;
				for (; j < n; j++)
					n_strcat(fld[i], wd[j], BUF);
			} else  /* any other field */
				for (j = 1; j < n; j++)
					n_strcat(fld[i], wd[j], BUF);
		}
		/* %Q quorporate or queer author - unreversed %A */
		else if (wd[0][1] == 'Q' && keystr[i] == 'A')
			for (j = 1; j < n; j++)
				n_strcat(fld[i], wd[j], BUF);
	}
}

static int
article(const char *str)		/* see if string contains an article */
{
	if (strcmp("The", str) == 0)	/* English */
		return (1);
	if (strcmp("A", str) == 0)
		return (1);
	if (strcmp("An", str) == 0)
		return (1);
	if (strcmp("Le", str) == 0)	/* French */
		return (1);
	if (strcmp("La", str) == 0)
		return (1);
	if (strcmp("Der", str) == 0)	/* German */
		return (1);
	if (strcmp("Die", str) == 0)
		return (1);
	if (strcmp("Das", str) == 0)
		return (1);
	if (strcmp("El", str) == 0)	/* Spanish */
		return (1);
	if (strcmp("Den", str) == 0)	/* Scandinavian */
		return (1);
	return (0);
}

static void
eval(char *keystr)		/* evaluate key string for A+ marking */
{
	int i, j;

	for (i = 0, j = 0; keystr[i]; i++, j++)
	{
		if (keystr[i] == '+')
		{
			multauth = 1;
			i++;
		}
		if (keystr[i] == 0)
			break;
		keystr[j] = keystr[i];
	}
	keystr[j] = 0;
}

static void
error(const char *s)		/* exit in case of various system errors */
{
	perror(s);
	exit(1);
}

static void
onintr(int unused)		/* remove tempfile in case of interrupt */
{
	fprintf(stderr, "\nInterrupt\n");
	unlink(tempfile);
	exit(1);
}

static int
endcomma(char *str)
{
	int n;

	n = strlen(str) - 1;
	if (str[n] == ',')
	{
		str[n] = '\0';
		return (1);
	}
	return (0);
}
