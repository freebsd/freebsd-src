/*
 * Copyright (c) 1980, 1987, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static const char copyright[] =
"@(#) Copyright (c) 1980, 1987, 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)head.c	8.2 (Berkeley) 5/4/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * head - give the first few lines of a stream or of each of a set of files
 *
 * Bill Joy UCB August 24, 1977
 */

void head(FILE *, int);
void head_bytes(FILE *, int);
void obsolete(char *[]);
void usage(void);

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register int ch;
	FILE *fp;
	int first, linecnt = -1, bytecnt = -1, eval = 0;
	char *ep;

	obsolete(argv);
	while ((ch = getopt(argc, argv, "n:c:")) != -1)
		switch(ch) {
		case 'c':
			bytecnt = strtol(optarg, &ep, 10);
			if (*ep || bytecnt <= 0)
				errx(1, "illegal byte count -- %s", optarg);
			break;
		case 'n':
			linecnt = strtol(optarg, &ep, 10);
			if (*ep || linecnt <= 0)
				errx(1, "illegal line count -- %s", optarg);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (linecnt != -1 && bytecnt != -1)
		errx(1, "can't combine line and byte counts");
	if (linecnt == -1 )
		linecnt = 10;
	if (*argv) {
		for (first = 1; *argv; ++argv) {
			if ((fp = fopen(*argv, "r")) == NULL) {
				warn("%s", *argv);
				eval = 1;
				continue;
			}
			if (argc > 1) {
				(void)printf("%s==> %s <==\n",
				    first ? "" : "\n", *argv);
				first = 0;
			}
			if (bytecnt == -1)
				head(fp, linecnt);
			else
				head_bytes(fp, bytecnt);
			(void)fclose(fp);
		}
	} else if (bytecnt == -1)
		head(stdin, linecnt);
	else
		head_bytes(stdin, bytecnt);

	exit(eval);
}

void
head(fp, cnt)
	FILE *fp;
	register int cnt;
{
	char *cp;
	int error, readlen;

	while (cnt && (cp = fgetln(fp, &readlen)) != NULL) {
		error = fwrite(cp, sizeof(char), readlen, stdout);
		if (error != readlen)
			err(1, "stdout");
		cnt--;
	}
}

void
head_bytes(fp, cnt)
	FILE *fp;
	register int cnt;
{
	char buf[4096];
	register int readlen;

	while (cnt) {
		if (cnt < sizeof(buf))
			readlen = cnt;
		else
			readlen = sizeof(buf);
		readlen = fread(buf, sizeof(char), readlen, fp);
		if (readlen == 0)
			break;
		if (fwrite(buf, sizeof(char), readlen, stdout) != readlen)
			err(1, "stdout");
		cnt -= readlen;
	}
}

void
obsolete(argv)
	char *argv[];
{
	char *ap;

	while ((ap = *++argv)) {
		/* Return if "--" or not "-[0-9]*". */
		if (ap[0] != '-' || ap[1] == '-' || !isdigit(ap[1]))
			return;
		if ((ap = malloc(strlen(*argv) + 2)) == NULL)
			err(1, NULL);
		ap[0] = '-';
		ap[1] = 'n';
		(void)strcpy(ap + 2, *argv + 1);
		*argv = ap;
	}
}

void
usage()
{

	(void)fprintf(stderr, "usage: head [-n lines | -c bytes] [file ...]\n");
	exit(1);
}
