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

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1987, 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#ifndef lint
static const char sccsid[] = "@(#)wc.c	8.1 (Berkeley) 6/6/93";
#endif

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

u_quad_t tlinect, twordct, tcharct;
int doline, doword, dochar;

int cnt __P((const char *));
void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, errors, total;

	(void) setlocale(LC_CTYPE, "");

	while ((ch = getopt(argc, argv, "lwc")) != -1)
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

	errors = 0;
	total = 0;
	if (!*argv) {
		if (cnt((char *)NULL) != 0)
			++errors;
		else
			(void)printf("\n");
	}
	else do {
		if (cnt(*argv) != 0)
			++errors;
		else
			(void)printf(" %s\n", *argv);
		++total;
	} while(*++argv);

	if (total > 1) {
		if (doline)
			(void)printf(" %7qu", tlinect);
		if (doword)
			(void)printf(" %7qu", twordct);
		if (dochar)
			(void)printf(" %7qu", tcharct);
		(void)printf(" total\n");
	}
	exit(errors == 0 ? 0 : 1);
}

int
cnt(file)
	const char *file;
{
	struct stat sb;
	u_quad_t linect, wordct, charct;
	int fd, len;
	short gotsp;
	u_char *p;
	u_char buf[MAXBSIZE], ch;

	linect = wordct = charct = 0;
	if (file == NULL) {
		file = "stdin";
		fd = STDIN_FILENO;
	} else {
		if ((fd = open(file, O_RDONLY, 0)) < 0) {
			warn("%s: open", file);
			return (1);
		}
		if (doword)
			goto word;
		/*
		 * Line counting is split out because it's a lot faster to get
		 * lines than to get words, since the word count requires some
		 * logic.
		 */
		if (doline) {
			while ((len = read(fd, buf, MAXBSIZE))) {
				if (len == -1) {
					warn("%s: read", file);
					(void)close(fd);
					return (1);
				}
				charct += len;
				for (p = buf; len--; ++p)
					if (*p == '\n')
						++linect;
			}
			tlinect += linect;
			(void)printf(" %7qu", linect);
			if (dochar) {
				tcharct += charct;
				(void)printf(" %7qu", charct);
			}
			(void)close(fd);
			return (0);
		}
		/*
		 * If all we need is the number of characters and it's a
		 * regular or linked file, just stat the puppy.
		 */
		if (dochar) {
			if (fstat(fd, &sb)) {
				warn("%s: fstat", file);
				(void)close(fd);
				return (1);
			}
			if (S_ISREG(sb.st_mode) || S_ISLNK(sb.st_mode)) {
				(void)printf(" %7lld", (long long)sb.st_size);
				tcharct += sb.st_size;
				(void)close(fd);
				return (0);
			}
		}
	}

	/* Do it the hard way... */
word:	for (gotsp = 1; (len = read(fd, buf, MAXBSIZE));) {
		if (len == -1) {
			warn("%s: read", file);
			(void)close(fd);
			return (1);
		}
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
		(void)printf(" %7qu", linect);
	}
	if (doword) {
		twordct += wordct;
		(void)printf(" %7qu", wordct);
	}
	if (dochar) {
		tcharct += charct;
		(void)printf(" %7qu", charct);
	}
	(void)close(fd);
	return (0);
}

void
usage()
{
	(void)fprintf(stderr, "usage: wc [-clw] [file ...]\n");
	exit(1);
}
