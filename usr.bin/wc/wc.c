/*
 * Copyright (c) 1980, 1987, 1991, 1993
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
static char copyright[] =
"@(#) Copyright (c) 1980, 1987, 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)wc.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

u_long tlinect, twordct, tcharct;
int doline, doword, dochar;

void cnt __P((char *));
void err __P((const char *, ...));
void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register int ch;
	int total;

	while ((ch = getopt(argc, argv, "lwc")) != EOF)
		switch((char)ch) {
		case 'l':
			doline = 1;
			break;
		case 'w':
			doword = 1;
			break;
		case 'c':
			dochar = 1;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	/* Wc's flags are on by default. */
	if (doline + doword + dochar == 0)
		doline = doword = dochar = 1;

	total = 0;
	if (!*argv) {
		cnt(NULL);
		(void)printf("\n");
	}
	else do {
		cnt(*argv);
		(void)printf(" %s\n", *argv);
		++total;
	} while(*++argv);

	if (total > 1) {
		if (doline)
			(void)printf(" %7ld", tlinect);
		if (doword)
			(void)printf(" %7ld", twordct);
		if (dochar)
			(void)printf(" %7ld", tcharct);
		(void)printf(" total\n");
	}
	exit(0);
}

void
cnt(file)
	char *file;
{
	register u_char *p;
	register short gotsp;
	register int ch, len;
	register u_long linect, wordct, charct;
	struct stat sb;
	int fd;
	u_char buf[MAXBSIZE];

	fd = STDIN_FILENO;
	linect = wordct = charct = 0;
	if (file) {
		if ((fd = open(file, O_RDONLY, 0)) < 0)
			err("%s: %s", file, strerror(errno));
		if (doword)
			goto word;
		/*
		 * Line counting is split out because it's a lot faster to get
		 * lines than to get words, since the word count requires some
		 * logic.
		 */
		if (doline) {
			while (len = read(fd, buf, MAXBSIZE)) {
				if (len == -1)
					err("%s: %s", file, strerror(errno));
				charct += len;
				for (p = buf; len--; ++p)
					if (*p == '\n')
						++linect;
			}
			tlinect += linect;
			(void)printf(" %7lu", linect);
			if (dochar) {
				tcharct += charct;
				(void)printf(" %7lu", charct);
			}
			(void)close(fd);
			return;
		}
		/*
		 * If all we need is the number of characters and it's a
		 * regular or linked file, just stat the puppy.
		 */
		if (dochar) {
			if (fstat(fd, &sb))
				err("%s: %s", file, strerror(errno));
			if (S_ISREG(sb.st_mode) || S_ISLNK(sb.st_mode)) {
				(void)printf(" %7qu", sb.st_size);
				tcharct += sb.st_size;
				(void)close(fd);
				return;
			}
		}
	}

	/* Do it the hard way... */
word:	for (gotsp = 1; len = read(fd, buf, MAXBSIZE);) {
		if (len == -1)
			err("%s: %s", file, strerror(errno));
		/*
		 * This loses in the presence of multi-byte characters.
		 * To do it right would require a function to return a
		 * character while knowing how many bytes it consumed.
		 */
		charct += len;
		for (p = buf; len--;) {
			ch = *p++;
			if (ch == '\n')
				++linect;
			if (isspace(ch))
				gotsp = 1;
			else if (gotsp) {
				gotsp = 0;
				++wordct;
			}
		}
	}
	if (doline) {
		tlinect += linect;
		(void)printf(" %7lu", linect);
	}
	if (doword) {
		twordct += wordct;
		(void)printf(" %7lu", wordct);
	}
	if (dochar) {
		tcharct += charct;
		(void)printf(" %7lu", charct);
	}
	(void)close(fd);
}

void
usage()
{
	(void)fprintf(stderr, "usage: wc [-clw] [files]\n");
	exit(1);
}

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#if __STDC__
err(const char *fmt, ...)
#else
err(fmt, va_alist)
	char *fmt;
        va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)fprintf(stderr, "wc: ");
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	exit(1);
	/* NOTREACHED */
}
