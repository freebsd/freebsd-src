/*-
 * Copyright (c) 1992, 1993
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
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)pig.c	8.2 (Berkeley) 5/4/95";
#endif
static const char rcsid[] =
 "$FreeBSD: src/games/pig/pig.c,v 1.7 1999/11/30 03:49:08 billf Exp $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void pigout __P((char *, int));
void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int len;
	int ch;
	char buf[1024];

	while ((ch = getopt(argc, argv, "")) != -1)
		switch(ch) {
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	for (len = 0; (ch = getchar()) != EOF;) {
		if (isalpha(ch)) {
			if (len >= sizeof(buf)) {
				(void)fprintf(stderr, "pig: ate too much!\n");
				exit(1);
			}
			buf[len++] = ch;
			continue;
		}
		if (len != 0) {
			pigout(buf, len);
			len = 0;
		}
		(void)putchar(ch);
	}
	exit(0);
}

void
pigout(buf, len)
	char *buf;
	int len;
{
	int ch, start;
	int olen;

	/*
	 * If the word starts with a vowel, append "way".  Don't treat 'y'
	 * as a vowel if it appears first.
	 */
	if (index("aeiouAEIOU", buf[0]) != NULL) {
		(void)printf("%.*sway", len, buf);
		return;
	}

	/*
	 * Copy leading consonants to the end of the word.  The unit "qu"
	 * isn't treated as a vowel.
	 */
	for (start = 0, olen = len;
	    !index("aeiouyAEIOUY", buf[start]) && start < olen;) {
		ch = buf[len++] = buf[start++];
		if ((ch == 'q' || ch == 'Q') && start < olen &&
		    (buf[start] == 'u' || buf[start] == 'U'))
			buf[len++] = buf[start++];
	}
	(void)printf("%.*say", olen, buf + start);
}

void
usage()
{
	(void)fprintf(stderr, "usage: pig\n");
	exit(1);
}
