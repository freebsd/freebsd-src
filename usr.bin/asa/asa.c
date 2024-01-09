/*	$NetBSD: asa.c,v 1.17 2016/09/05 00:40:28 sevan Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 1993,94 Winning Strategies, Inc.
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
 *      This product includes software developed by Winning Strategies, Inc.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void asa(FILE *);
static void usage(void);

int
main(int argc, char *argv[])
{
	FILE *fp;
	int ch, exval;

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	exval = 0;
	if (*argv == NULL) {
		asa(stdin);
	} else {
		do {
			if (strcmp(*argv, "-") == 0) {
				asa(stdin);
			} else if ((fp = fopen(*argv, "r")) == NULL) {
				warn("%s", *argv);
				exval = 1;
			} else {
				asa(fp);
				fclose(fp);
			}
		} while (*++argv != NULL);
	}

	if (fflush(stdout) != 0)
		err(1, "stdout");

	exit(exval);
}

static void
usage(void)
{
	fprintf(stderr, "usage: asa [file ...]\n");
	exit(1);
}

static void
asa(FILE *f)
{
	char *buf;
	size_t len;
	bool eol = false;

	while ((buf = fgetln(f, &len)) != NULL) {
		/* in all cases but '+', terminate previous line, if any */
		if (buf[0] != '+' && eol)
			putchar('\n');
		/* examine and translate the control character */
		switch (buf[0]) {
		default:
			/*
			 * “It is suggested that implementations treat
			 * characters other than 0, 1, and '+' as <space>
			 * in the absence of any compelling reason to do
			 * otherwise” (POSIX.1-2017)
			 */
		case ' ':
			/* nothing */
			break;
		case '0':
			putchar('\n');
			break;
		case '1':
			putchar('\f');
			break;
		case '+':
			/*
			 * “If the '+' is the first character in the
			 * input, it shall be equivalent to <space>.”
			 * (POSIX.1-2017)
			 */
			if (eol)
				putchar('\r');
			break;
		}
		/* trim newline if there is one */
		if ((eol = (buf[len - 1] == '\n')))
			--len;
		/* print the rest of the input line */
		if (len > 1 && buf[0] && buf[1])
			fwrite(buf + 1, 1, len - 1, stdout);
	}
	/* terminate the last line, if any */
	if (eol)
		putchar('\n');
	/* check for output errors */
	if (ferror(stdout) != 0)
		err(1, "stdout");
}
