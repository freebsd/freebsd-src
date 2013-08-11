/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char copyright[] =
"%Z% Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "$Id: dump.c,v 8.2 2011/07/14 00:05:25 zy Exp $";
#endif /* not lint */

#include <ctype.h>
#include <stdio.h>

static void
parse(fp)
	FILE *fp;
{
	int ch, s1, s2, s3;

#define	TESTD(s) {							\
	if ((s = getc(fp)) == EOF)					\
		return;							\
	if (!isdigit(s))						\
		continue;						\
}
#define	TESTP {								\
	if ((ch = getc(fp)) == EOF)					\
		return;							\
	if (ch != '|')							\
		continue;						\
}
#define	MOVEC(t) {							\
	do {								\
		if ((ch = getc(fp)) == EOF)				\
			return;						\
	} while (ch != (t));						\
}
	for (;;) {
		MOVEC('"');
		TESTD(s1);
		TESTD(s2);
		TESTD(s3);
		TESTP;
		putchar('"');
		putchar(s1);
		putchar(s2);
		putchar(s3);
		putchar('|');
		for (;;) {		/* dump to end quote. */
			if ((ch = getc(fp)) == EOF)
				return;
			putchar(ch);
			if (ch == '"')
				break;
			if (ch == '\\') {
				if ((ch = getc(fp)) == EOF)
					return;
				putchar(ch);
			}
		}
		putchar('\n');
	}
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	FILE *fp;

	for (; *argv != NULL; ++argv) {
		if ((fp = fopen(*argv, "r")) == NULL) {
			perror(*argv);
			return (1);
		}
		parse(fp);
		(void)fclose(fp);
	}
	return (0);
}
