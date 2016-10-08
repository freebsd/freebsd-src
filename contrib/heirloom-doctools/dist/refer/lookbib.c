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

/*	from OpenSolaris "lookbib.c	1.6	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)lookbib.c	1.3 (gritter) 10/22/05
 */

#include <stdio.h>
#include <ctype.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static void map_lower(char *);
static void instruct(void);

int
main(int argc, char **argv)	/* look in biblio for record matching keywords */
{
	FILE *hfp;
	char s[BUFSIZ], hunt[BUFSIZ];

	if (argc == 1 || argc > 2) {
		fputs("Usage:  lookbib database\n\
\tfinds citations specified on standard input\n", stderr);
		exit(1);
	}
	snprintf(s, sizeof s, "%s.ia", argv[1]);
	if (access(s, 0) == -1) {
		snprintf (s, sizeof(s), "%s", argv[1]);
		if (access(s, 0) == -1) {
			perror(s);
			fprintf(stderr, "\tNeither index file %s.ia \
nor reference file %s found\n", s, s);
			exit(1);
		}
	}
	snprintf(hunt, sizeof hunt, REFDIR "/hunt %s", argv[1]);
	if (isatty(fileno(stdin))) {
		fprintf(stderr, "Instructions? ");
		fgets(s, BUFSIZ, stdin);
		if (*s == 'y')
			instruct();
	}
   again:
	fprintf(stderr, "> ");
	if (fgets(s, BUFSIZ, stdin)) {
		if (*s == '\n')
			goto again;
		if (strlen(s) <= 3)
			goto again;
		if ((hfp = popen(hunt, "w")) == NULL) {
			perror("lookbib: " REFDIR "/hunt");
			exit(1);
		}
		map_lower(s);
		fputs(s, hfp);
		pclose(hfp);
		goto again;
	}
	fprintf(stderr, "EOT\n");
	return 0;
}

static void
map_lower(char *s)		/* map string s to lower case */
{
	for ( ; *s; ++s)
		if (isupper((int)*s))
			*s = tolower((int)*s);
}

static void
instruct(void)
{
	fputs(
"\nType keywords (such as author and date) after the > prompt.\n\
References with those keywords are printed if they exist;\n\
\tif nothing matches you are given another prompt.\n\
To quit lookbib, press CTRL-d after the > prompt.\n\n", stderr);

}
