/*-
 * Copyright (c) 1991, 1993, 1994
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
"@(#) Copyright (c) 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)dirname.c	8.4 (Berkeley) 5/4/95";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void usage __P((void));

int
main(argc, argv)
	int argc;
	char **argv;
{
	char *p;
	int ch;

	while ((ch = getopt(argc, argv, "")) != -1)
		switch(ch) {
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	/*
	 * (1) If string is //, skip steps (2) through (5).
	 * (2) If string consists entirely of slash characters, string
	 *     shall be set to a single slash character.  In this case,
	 *     skip steps (3) through (8).
	 */
	for (p = *argv;; ++p) {
		if (!*p) {
			if (p > *argv)
				(void)printf("/\n");
			else
				(void)printf(".\n");
			exit(0);
		}
		if (*p != '/')
			break;
	}

	/*
	 * (3) If there are any trailing slash characters in string, they
	 *     shall be removed.
	 */
	for (; *p; ++p);
	while (*--p == '/')
		continue;
	*++p = '\0';

	/*
	 * (4) If there are no slash characters remaining in string,
	 *     string shall be set to a single period character.  In this
	 *     case skip steps (5) through (8).
	 *
	 * (5) If there are any trailing nonslash characters in string,
	 *     they shall be removed.
	 */
	while (--p >= *argv)
		if (*p == '/')
			break;
	++p;
	if (p == *argv) {
		(void)printf(".\n");
		exit(0);
	}

	/*
	 * (6) If the remaining string is //, it is implementation defined
	 *     whether steps (7) and (8) are skipped or processed.
	 *
	 * This case has already been handled, as part of steps (1) and (2).
	 */

	/*
	 * (7) If there are any trailing slash characters in string, they
	 *     shall be removed.
	 */
	while (--p >= *argv)
		if (*p != '/')
			break;
	++p;

	/*
	 * (8) If the remaining string is empty, string shall be set to
	 *     a single slash character.
	 */
	*p = '\0';
	(void)printf("%s\n", p == *argv ? "/" : *argv);
	exit(0);
}

void
usage()
{

	(void)fprintf(stderr, "usage: dirname path\n");
	exit(1);
}
