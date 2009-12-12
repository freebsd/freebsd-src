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
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#if 0
#ifndef lint
static char sccsid[] = "From: @(#)comm.c	8.4 (Berkeley) 5/4/95";
#endif
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <err.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#define	MAXLINELEN	(LINE_MAX + 1)

const wchar_t *tabs[] = { L"", L"\t", L"\t\t" };

FILE   *file(const char *);
wchar_t	*getline(wchar_t *, size_t *, FILE *);
void	show(FILE *, const char *, const wchar_t *, wchar_t *, size_t *);
int     wcsicoll(const wchar_t *, const wchar_t *);
static void	usage(void);

int
main(int argc, char *argv[])
{
	int comp, read1, read2;
	int ch, flag1, flag2, flag3, iflag;
	FILE *fp1, *fp2;
	const wchar_t *col1, *col2, *col3;
	size_t line1len, line2len;
	wchar_t *line1, *line2;
	const wchar_t **p;

	flag1 = flag2 = flag3 = 1;
	iflag = 0;

 	line1len = MAXLINELEN;
 	line2len = MAXLINELEN;
 	line1 = malloc(line1len * sizeof(*line1));
 	line2 = malloc(line2len * sizeof(*line2));
	if (line1 == NULL || line2 == NULL)
		err(1, "malloc");

	(void) setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "123i")) != -1)
		switch(ch) {
		case '1':
			flag1 = 0;
			break;
		case '2':
			flag2 = 0;
			break;
		case '3':
			flag3 = 0;
			break;
		case 'i':
			iflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
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
		if (read1) {
			line1 = getline(line1, &line1len, fp1);
			if (line1 == NULL && ferror(fp1))
				err(1, "%s", argv[0]);
		}
		if (read2) {
			line2 = getline(line2, &line2len, fp2);
			if (line2 == NULL && ferror(fp2))
				err(1, "%s", argv[1]);
		}

		/* if one file done, display the rest of the other file */
		if (line1 == NULL) {
			if (line2 != NULL && col2 != NULL)
				show(fp2, argv[1], col2, line2, &line2len);
			break;
		}
		if (line2 == NULL) {
			if (line1 != NULL && col1 != NULL)
				show(fp1, argv[0], col1, line1, &line1len);
			break;
		}

		/* lines are the same */
		if(iflag)
			comp = wcsicoll(line1, line2);
		else
			comp = wcscoll(line1, line2);

		if (!comp) {
			read1 = read2 = 1;
			if (col3 != NULL)
				(void)printf("%ls%ls\n", col3, line1);
			continue;
		}

		/* lines are different */
		if (comp < 0) {
			read1 = 1;
			read2 = 0;
			if (col1 != NULL)
				(void)printf("%ls%ls\n", col1, line1);
		} else {
			read1 = 0;
			read2 = 1;
			if (col2 != NULL)
				(void)printf("%ls%ls\n", col2, line2);
		}
	}
	exit(0);
}

wchar_t *
getline(wchar_t *buf, size_t *buflen, FILE *fp)
{
	size_t bufpos;
	wint_t ch;

	bufpos = 0;
	while ((ch = getwc(fp)) != WEOF && ch != '\n') {
		if (bufpos + 1 >= *buflen) {
			*buflen = *buflen * 2;
			buf = reallocf(buf, *buflen * sizeof(*buf));
			if (buf == NULL)
				return (NULL);
		}
		buf[bufpos++] = ch;
	}
	buf[bufpos] = '\0';

	return (bufpos != 0 || ch == '\n' ? buf : NULL);
}

void
show(FILE *fp, const char *fn, const wchar_t *offset, wchar_t *buf, size_t *buflen)
{

	do {
		(void)printf("%ls%ls\n", offset, buf);
	} while ((buf = getline(buf, buflen, fp)) != NULL);
	if (ferror(fp))
		err(1, "%s", fn);
}

FILE *
file(const char *name)
{
	FILE *fp;

	if (!strcmp(name, "-"))
		return (stdin);
	if ((fp = fopen(name, "r")) == NULL) {
		err(1, "%s", name);
	}
	return (fp);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: comm [-123i] file1 file2\n");
	exit(1);
}

static size_t wcsicoll_l1_buflen = 0, wcsicoll_l2_buflen = 0;
static wchar_t *wcsicoll_l1_buf = NULL, *wcsicoll_l2_buf = NULL;

int
wcsicoll(const wchar_t *s1, const wchar_t *s2)
{
	wchar_t *p;
	size_t l1, l2;
	size_t new_l1_buflen, new_l2_buflen;

	l1 = wcslen(s1) + 1;
	l2 = wcslen(s2) + 1;
	new_l1_buflen = wcsicoll_l1_buflen;
	new_l2_buflen = wcsicoll_l2_buflen;
	while (new_l1_buflen < l1) {
		if (new_l1_buflen == 0)
			new_l1_buflen = MAXLINELEN;
		else
			new_l1_buflen *= 2;
	}
	while (new_l2_buflen < l2) {
		if (new_l2_buflen == 0)
			new_l2_buflen = MAXLINELEN;
		else
			new_l2_buflen *= 2;
	}
	if (new_l1_buflen > wcsicoll_l1_buflen) {
		wcsicoll_l1_buf = reallocf(wcsicoll_l1_buf, new_l1_buflen * sizeof(*wcsicoll_l1_buf));
		if (wcsicoll_l1_buf == NULL)
                	err(1, "reallocf");
		wcsicoll_l1_buflen = new_l1_buflen;
	}
	if (new_l2_buflen > wcsicoll_l2_buflen) {
		wcsicoll_l2_buf = reallocf(wcsicoll_l2_buf, new_l2_buflen * sizeof(*wcsicoll_l2_buf));
		if (wcsicoll_l2_buf == NULL)
                	err(1, "reallocf");
		wcsicoll_l2_buflen = new_l2_buflen;
	}

	for (p = wcsicoll_l1_buf; *s1; s1++)
		*p++ = towlower(*s1);
	*p = '\0';
	for (p = wcsicoll_l2_buf; *s2; s2++)
		*p++ = towlower(*s2);
	*p = '\0';

	return (wcscoll(wcsicoll_l1_buf, wcsicoll_l2_buf));
}
