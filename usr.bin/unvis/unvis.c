/*-
 * Copyright (c) 1989 The Regents of the University of California.
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
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)unvis.c	5.1 (Berkeley) 6/1/90";
#endif /* not lint */

#include <stdio.h>
#include <vis.h>

char	*Program;
#define usage()	fprintf(stderr, "usage: %s %s\n", Program, USAGE)
#define USAGE "[file...]"

main(argc, argv)
	char *argv[];
{
	FILE *fp;
	extern char *optarg;
	extern int optind;
	int ch;

	Program = argv[0];
	while ((ch = getopt(argc, argv, "")) != EOF)
		switch((char)ch) {
		case '?':
		default:
			usage();
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (*argv)
		while (*argv) {
			if ((fp=fopen(*argv, "r")) != NULL)
				process(fp, *argv);
			else
				syserror("%s", *argv);
			argv++;
		}
	else
		process(stdin, "<stdin>");
	exit(0);
}

process(fp, filename)
	FILE *fp;
	char *filename;
{
	register int offset = 0, c, ret;
	int state = 0;
	char outc;

	while ((c = getc(fp)) != EOF) {
		offset++;
	again:
		switch(ret = unvis(&outc, (char)c, &state, 0)) {
		case UNVIS_VALID:
			putchar(outc);
			break;
		case UNVIS_VALIDPUSH:
			putchar(outc);
			goto again;
		case UNVIS_SYNBAD:
			error("%s: offset: %d: can't decode", filename, offset);
			state = 0;
			break;
		case 0:
		case UNVIS_NOCHAR:
			break;
		default:
			error("bad return value (%d), can't happen", ret);
			exit(1);
		}
	}
	if (unvis(&outc, (char)0, &state, UNVIS_END) == UNVIS_VALID)
		putchar(outc);
}

#include <varargs.h>

error(va_alist)
	va_dcl
{
	char *fmt;
	va_list ap;
	extern errno;

	fprintf(stderr, "%s: ", Program);
	va_start(ap);
	fmt = va_arg(ap, char *);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

syserror(va_alist)
	va_dcl
{
	char *fmt;
	va_list ap;
	extern errno;

	fprintf(stderr, "%s: ", Program);
	va_start(ap);
	fmt = va_arg(ap, char *);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ": %s\n", strerror(errno));
}
