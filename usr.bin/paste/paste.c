/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam S. Moskowitz of Menlo Consulting.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/types.h>
#include <sys/capsicum.h>
#include <sys/queue.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

typedef struct _list {
	STAILQ_ENTRY(_list) entries;
	FILE *fp;
	int cnt;
	int err;
	char *name;
} LIST;

static STAILQ_HEAD(head, _list) lh;

static wchar_t *delim;
static int delimcnt;
static int filecnt;

static int parallel(void);
static int sequential(void);
static int tr(wchar_t *);
static void usage(void) __dead2;

static wchar_t tab[] = L"\t";

int
main(int argc, char *argv[])
{
	LIST *lp;
	int ch, failed, rval, seq;
	wchar_t *warg;
	const char *arg;
	size_t len;
	cap_rights_t rights;

	STAILQ_INIT(&lh);
	setlocale(LC_CTYPE, "");

	seq = 0;
	while ((ch = getopt(argc, argv, "d:s")) != -1)
		switch(ch) {
		case 'd':
			arg = optarg;
			len = mbsrtowcs(NULL, &arg, 0, NULL);
			if (len == (size_t)-1)
				err(EXIT_FAILURE, "delimiters");
			warg = malloc((len + 1) * sizeof(*warg));
			if (warg == NULL)
				err(EXIT_FAILURE, NULL);
			arg = optarg;
			len = mbsrtowcs(warg, &arg, len + 1, NULL);
			if (len == (size_t)-1)
				err(EXIT_FAILURE, "delimiters");
			delimcnt = tr(delim = warg);
			break;
		case 's':
			seq = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (*argv == NULL)
		usage();
	if (!delim) {
		delimcnt = 1;
		delim = tab;
	}

	for (filecnt = 0; *argv; ++argv, ++filecnt) {
		failed = 0;
		if ((lp = malloc(sizeof(LIST))) == NULL)
			err(EXIT_FAILURE, NULL);
		if ((*argv)[0] == '-' && !(*argv)[1])
			lp->fp = stdin;
		else if (!(lp->fp = fopen(*argv, "r"))) {
			if (!seq)
				err(EXIT_FAILURE, "%s", *argv);
			else
				failed = errno;
		}

		if (cap_rights_limit(fileno(lp->fp),
		    cap_rights_init(&rights, CAP_READ)) < 0 &&
		    errno != ENOSYS)
			err(EXIT_FAILURE, "unable to limit rights for %s", *argv);

		lp->cnt = filecnt;
		lp->err = failed;
		lp->name = *argv;

		STAILQ_INSERT_TAIL(&lh, lp, entries);
	}

	if (cap_enter() < 0 && errno != ENOSYS)
		err(EXIT_FAILURE, "failed to enter capability mode");

	rval = seq ? sequential() : parallel();

	exit(rval);
}

static int
parallel(void)
{
	LIST *lp;
	wint_t ich;
	wchar_t ch;
	int opencnt, output;

	for (opencnt = filecnt; opencnt;) {
		output = 0;
		STAILQ_FOREACH(lp, &lh, entries) {
			if (!lp->fp) {
				if (output && lp->cnt &&
				    (ch = delim[(lp->cnt - 1) % delimcnt]))
					putwchar(ch);
				continue;
			}
			if ((ich = getwc(lp->fp)) == WEOF) {
				if (!--opencnt)
					break;
				lp->fp = NULL;
				if (output && lp->cnt &&
				    (ch = delim[(lp->cnt - 1) % delimcnt]))
					putwchar(ch);
				continue;
			}
			/*
			 * make sure that we don't print any delimiters
			 * unless there's a non-empty file.
			 */
			if (!output) {
				output = 1;
				for (int cnt = 0; cnt < lp->cnt; ++cnt)
					if ((ch = delim[cnt % delimcnt]))
						putwchar(ch);
			} else if ((ch = delim[(lp->cnt - 1) % delimcnt]))
				putwchar(ch);
			if (ich == '\n')
				continue;
			do {
				putwchar(ich);
			} while ((ich = getwc(lp->fp)) != WEOF && ich != '\n');
		}
		if (output)
			putwchar('\n');
	}

	return (0);
}

static int
sequential(void)
{
	LIST *lp;
	int cnt, failed, needdelim;
	wint_t ch;

	failed = 0;
	STAILQ_FOREACH(lp, &lh, entries) {
		cnt = needdelim = 0;
		if (lp->err) {
			errno = failed = lp->err;
			warn("%s", lp->name);
			continue;
		}
		while ((ch = getwc(lp->fp)) != WEOF) {
			if (needdelim) {
				needdelim = 0;
				if (delim[cnt] != '\0')
					putwchar(delim[cnt]);
				if (++cnt == delimcnt)
					cnt = 0;
			}
			if (ch != '\n')
				putwchar(ch);
			else
				needdelim = 1;
		}
		if (needdelim)
			putwchar('\n');
		if (lp->fp != stdin)
			(void)fclose(lp->fp);
	}

	return (failed != EXIT_SUCCESS);
}

static int
tr(wchar_t *arg)
{
	int cnt;
	wchar_t ch, *p;

	for (p = arg, cnt = 0; (ch = *p++); ++arg, ++cnt)
		if (ch == '\\')
			switch(ch = *p++) {
			case 'n':
				*arg = '\n';
				break;
			case 't':
				*arg = '\t';
				break;
			case '0':
				*arg = '\0';
				break;
			default:
				*arg = ch;
				break;
			}
		else
			*arg = ch;

	if (!cnt)
		errx(EXIT_FAILURE, "no delimiters specified");
	return(cnt);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: paste [-s] [-d delimiters] file ...\n");
	exit(EXIT_FAILURE);
}
