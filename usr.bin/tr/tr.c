/*
 * Copyright (c) 1988, 1993
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
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#ifndef lint
static const char sccsid[] = "@(#)tr.c	8.2 (Berkeley) 5/4/95";
#endif

#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

/*
 * For -C option: determine whether a byte is a valid character in the
 * current character set (as defined by LC_CTYPE).
 */
#define ISCHAR(c) (iscntrl(c) || isprint(c))

static int string1[NCHARS] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,		/* ASCII */
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
}, string2[NCHARS];

STR s1 = { STRING1, NORMAL, 0, OOBCH, { 0, OOBCH }, NULL, NULL };
STR s2 = { STRING2, NORMAL, 0, OOBCH, { 0, OOBCH }, NULL, NULL };

static void setup(int *, char *, STR *, int, int);
static void usage(void);

int
main(int argc, char **argv)
{
	static int carray[NCHARS];
	int ch, cnt, n, lastch, *p;
	int Cflag, cflag, dflag, sflag, isstring2;

	(void)setlocale(LC_ALL, "");

	Cflag = cflag = dflag = sflag = 0;
	while ((ch = getopt(argc, argv, "Ccdsu")) != -1)
		switch((char)ch) {
		case 'C':
			Cflag = 1;
			cflag = 0;
			break;
		case 'c':
			cflag = 1;
			Cflag = 0;
			break;
		case 'd':
			dflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 'u':
			setbuf(stdout, (char *)NULL);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	switch(argc) {
	case 0:
	default:
		usage();
		/* NOTREACHED */
	case 1:
		isstring2 = 0;
		break;
	case 2:
		isstring2 = 1;
		break;
	}

	/*
	 * tr -ds [-Cc] string1 string2
	 * Delete all characters (or complemented characters) in string1.
	 * Squeeze all characters in string2.
	 */
	if (dflag && sflag) {
		if (!isstring2)
			usage();

		setup(string1, argv[0], &s1, cflag, Cflag);
		setup(string2, argv[1], &s2, 0, 0);

		for (lastch = OOBCH; (ch = getchar()) != EOF;)
			if (!string1[ch] && (!string2[ch] || lastch != ch)) {
				lastch = ch;
				(void)putchar(ch);
			}
		exit(0);
	}

	/*
	 * tr -d [-Cc] string1
	 * Delete all characters (or complemented characters) in string1.
	 */
	if (dflag) {
		if (isstring2)
			usage();

		setup(string1, argv[0], &s1, cflag, Cflag);

		while ((ch = getchar()) != EOF)
			if (!string1[ch])
				(void)putchar(ch);
		exit(0);
	}

	/*
	 * tr -s [-Cc] string1
	 * Squeeze all characters (or complemented characters) in string1.
	 */
	if (sflag && !isstring2) {
		setup(string1, argv[0], &s1, cflag, Cflag);

		for (lastch = OOBCH; (ch = getchar()) != EOF;)
			if (!string1[ch] || lastch != ch) {
				lastch = ch;
				(void)putchar(ch);
			}
		exit(0);
	}

	/*
	 * tr [-Ccs] string1 string2
	 * Replace all characters (or complemented characters) in string1 with
	 * the character in the same position in string2.  If the -s option is
	 * specified, squeeze all the characters in string2.
	 */
	if (!isstring2)
		usage();

	s1.str = argv[0];
	if ((s2.str = strdup(argv[1])) == NULL)
		errx(2, "strdup(argv[1])");

	if (cflag || Cflag)
		for (cnt = NCHARS, p = string1; cnt--;)
			*p++ = OOBCH;

	if (!next(&s2))
		errx(1, "empty string2");

	/*
	 * For -s result will contain only those characters defined
	 * as the second characters in each of the toupper or tolower
	 * pairs.
	 */

	/* If string2 runs out of characters, use the last one specified. */
	while (next(&s1)) {
	again:
		if (s1.state == SET_LOWER &&
		    s2.state == SET_UPPER &&
		    s1.cnt == 1 && s2.cnt == 1) {
			do {
				string1[s1.lastch] = ch = toupper(s1.lastch);
				if (sflag && isupper(ch))
					string2[ch] = 1;
				if (!next(&s1))
					goto endloop;
			} while (s1.state == SET_LOWER && s1.cnt > 1);
			/* skip upper set */
			do {
				if (!next(&s2))
					break;
			} while (s2.state == SET_UPPER && s2.cnt > 1);
			goto again;
		} else if (s1.state == SET_UPPER &&
			   s2.state == SET_LOWER &&
			   s1.cnt == 1 && s2.cnt == 1) {
			do {
				string1[s1.lastch] = ch = tolower(s1.lastch);
				if (sflag && islower(ch))
					string2[ch] = 1;
				if (!next(&s1))
					goto endloop;
			} while (s1.state == SET_UPPER && s1.cnt > 1);
			/* skip lower set */
			do {
				if (!next(&s2))
					break;
			} while (s2.state == SET_LOWER && s2.cnt > 1);
			goto again;
		} else {
			string1[s1.lastch] = s2.lastch;
			if (sflag)
				string2[s2.lastch] = 1;
		}
		(void)next(&s2);
	}
endloop:
	if (cflag || Cflag) {
		for (p = carray, cnt = 0; cnt < NCHARS; cnt++) {
			if (string1[cnt] == OOBCH && (!Cflag || ISCHAR(cnt)))
				*p++ = cnt;
			else
				string1[cnt] = cnt;
		}
		n = p - carray;
		if (Cflag && n > 1)
			(void)mergesort(carray, n, sizeof(*carray), charcoll);

		s2.str = argv[1];
		s2.state = NORMAL;
		for (cnt = 0; cnt < n; cnt++) {
			(void)next(&s2);
			string1[carray[cnt]] = s2.lastch;
		}
	}

	if (sflag)
		for (lastch = OOBCH; (ch = getchar()) != EOF;) {
			ch = string1[ch];
			if (!string2[ch] || lastch != ch) {
				lastch = ch;
				(void)putchar(ch);
			}
		}
	else
		while ((ch = getchar()) != EOF)
			(void)putchar(string1[ch]);
	exit (0);
}

static void
setup(int *string, char *arg, STR *str, int cflag, int Cflag)
{
	int cnt, *p;

	str->str = arg;
	bzero(string, NCHARS * sizeof(int));
	while (next(str))
		string[str->lastch] = 1;
	if (cflag)
		for (p = string, cnt = NCHARS; cnt--; ++p)
			*p = !*p;
	else if (Cflag)
		for (cnt = 0; cnt < NCHARS; cnt++)
			string[cnt] = !string[cnt] && ISCHAR(cnt);
}

int
charcoll(const void *a, const void *b)
{
	static char sa[2], sb[2];

	sa[0] = *(const int *)a;
	sb[0] = *(const int *)b;
	return (strcoll(sa, sb));
}

static void
usage(void)
{
	(void)fprintf(stderr, "%s\n%s\n%s\n%s\n",
		"usage: tr [-Ccsu] string1 string2",
		"       tr [-Ccu] -d string1",
		"       tr [-Ccu] -s string1",
		"       tr [-Ccu] -ds string1 string2");
	exit(1);
}
