/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Case Larsen.
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
static char copyright[] =
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)comm.c	8.3 (Berkeley) 4/2/94";
#endif /* not lint */

#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	MAXLINELEN	(LINE_MAX + 1)

char *tabs[] = { "", "\t", "\t\t" };

FILE   *file __P((char *));
void	show __P((FILE *, char *, char *));
void	usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int comp, file1done, file2done, read1, read2;
	int ch, flag1, flag2, flag3;
	FILE *fp1, *fp2;
	char *col1, *col2, *col3;
	char **p, line1[MAXLINELEN], line2[MAXLINELEN];

	flag1 = flag2 = flag3 = 1;
	while ((ch = getopt(argc, argv, "-123")) != EOF)
		switch(ch) {
		case '-':
			--optind;
			goto done;
		case '1':
			flag1 = 0;
			break;
		case '2':
			flag2 = 0;
			break;
		case '3':
			flag3 = 0;
			break;
		case '?':
		default:
			usage();
		}
done:	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	fp1 = file(argv[0]);
	fp2 = file(argv[1]);

	/* for each column printed, add another tab offset */
	p = tabs;
	col1 = col2 = col3 = NULL;
	if (flag1)
		col1 = *p++;
	if (flag2)
		col2 = *p++;
	if (flag3)
		col3 = *p;

	for (read1 = read2 = 1;;) {
		/* read next line, check for EOF */
		if (read1)
			file1done = !fgets(line1, MAXLINELEN, fp1);
		if (read2)
			file2done = !fgets(line2, MAXLINELEN, fp2);

		/* if one file done, display the rest of the other file */
		if (file1done) {
			if (!file2done && col2)
				show(fp2, col2, line2);
			break;
		}
		if (file2done) {
			if (!file1done && col1)
				show(fp1, col1, line1);
			break;
		}

		/* lines are the same */
		if (!(comp = strcmp(line1, line2))) {
			read1 = read2 = 1;
			if (col3)
				(void)printf("%s%s", col3, line1);
			continue;
		}

		/* lines are different */
		if (comp < 0) {
			read1 = 1;
			read2 = 0;
			if (col1)
				(void)printf("%s%s", col1, line1);
		} else {
			read1 = 0;
			read2 = 1;
			if (col2)
				(void)printf("%s%s", col2, line2);
		}
	}
	exit(0);
}

void
show(fp, offset, buf)
	FILE *fp;
	char *offset, *buf;
{

	do {
		(void)printf("%s%s", offset, buf);
	} while (fgets(buf, MAXLINELEN, fp));
}

FILE *
file(name)
	char *name;
{
	FILE *fp;

	if (!strcmp(name, "-"))
		return (stdin);
	if ((fp = fopen(name, "r")) == NULL) {
		(void)fprintf(stderr, "comm: %s: %s\n", name, strerror(errno));
		exit(1);
	}
	return (fp);
}

void
usage()
{

	(void)fprintf(stderr, "usage: comm [-123] file1 file2\n");
	exit(1);
}
