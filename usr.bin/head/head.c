/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
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

#include <sys/capsicum.h>
#include <sys/types.h>

#include <capsicum_helpers.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libutil.h>

#include <libcasper.h>
#include <casper/cap_fileargs.h>

/*
 * head - give the first few lines of a stream or of each of a set of files
 *
 * Bill Joy UCB August 24, 1977
 */

static void head(FILE *, intmax_t);
static void head_bytes(FILE *, off_t);
static void obsolete(char *[]);
static void usage(void) __dead2;

static const struct option long_opts[] =
{
	{"bytes",	required_argument,	NULL, 'c'},
	{"lines",	required_argument,	NULL, 'n'},
	{"quiet",	no_argument,		NULL, 'q'},
	{"silent",	no_argument,		NULL, 'q'},
	{"verbose",	no_argument,		NULL, 'v'},
	{NULL,		no_argument,		NULL, 0}
};

int
main(int argc, char *argv[])
{
	FILE *fp;
	off_t bytecnt;
	intmax_t linecnt;
	int ch, first, eval;
	fileargs_t *fa;
	cap_rights_t rights;
	int qflag = 0;
	int vflag = 0;

	linecnt = -1;
	eval = 0;
	bytecnt = -1;

	obsolete(argv);
	while ((ch = getopt_long(argc, argv, "+n:c:qv", long_opts, NULL)) != -1) {
		switch(ch) {
		case 'c':
			if (expand_number(optarg, &bytecnt) || bytecnt <= 0)
				errx(1, "illegal byte count -- %s", optarg);
			break;
		case 'n':
			if (expand_number(optarg, &linecnt) || linecnt <= 0)
				errx(1, "illegal line count -- %s", optarg);
			break;
		case 'q':
			qflag = 1;
			vflag = 0;
			break;
		case 'v':
			qflag = 0;
			vflag = 1;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	fa = fileargs_init(argc, argv, O_RDONLY, 0,
	    cap_rights_init(&rights, CAP_READ, CAP_FSTAT, CAP_FCNTL), FA_OPEN);
	if (fa == NULL)
		err(1, "unable to init casper");

	caph_cache_catpages();
	if (caph_limit_stdio() < 0 || caph_enter_casper() < 0)
		err(1, "unable to enter capability mode");

	if (linecnt != -1 && bytecnt != -1)
		errx(1, "can't combine line and byte counts");
	if (linecnt == -1)
		linecnt = 10;
	if (*argv != NULL) {
		for (first = 1; *argv != NULL; ++argv) {
			if ((fp = fileargs_fopen(fa, *argv, "r")) == NULL) {
				warn("%s", *argv);
				eval = 1;
				continue;
			}
			if (vflag || (qflag == 0 && argc > 1)) {
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

	fileargs_free(fa);
	exit(eval);
}

static void
head(FILE *fp, intmax_t cnt)
{
	char *cp = NULL;
	size_t error, bufsize = 0;
	ssize_t readlen;

	while (cnt != 0 && (readlen = getline(&cp, &bufsize, fp)) >= 0) {
		error = fwrite(cp, sizeof(char), readlen, stdout);
		if ((ssize_t)error != readlen)
			err(1, "stdout");
		cnt--;
	}
}

static void
head_bytes(FILE *fp, off_t cnt)
{
	char buf[4096];
	size_t readlen;

	while (cnt) {
		if ((uintmax_t)cnt < sizeof(buf))
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

static void
obsolete(char *argv[])
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

static void
usage(void)
{

	(void)fprintf(stderr, "usage: head [-n lines | -c bytes] [file ...]\n");
	exit(1);
}
