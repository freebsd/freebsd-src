/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1987, 1990, 1993, 1994
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1987, 1990, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#if 0
#ifndef lint
static char sccsid[] = "@(#)cmp.c	8.3 (Berkeley) 4/2/94";
#endif
#endif

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <capsicum_helpers.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <nl_types.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libutil.h>

#include "extern.h"

bool	bflag, lflag, sflag, xflag, zflag;

static const struct option long_opts[] =
{
	{"print-bytes",	no_argument,		NULL, 'b'},
	{"ignore-initial", required_argument,	NULL, 'i'},
	{"verbose",	no_argument,		NULL, 'l'},
	{"bytes",	required_argument,	NULL, 'n'},
	{"silent",	no_argument,		NULL, 's'},
	{"quiet",	no_argument,		NULL, 's'},
	{NULL,		no_argument,		NULL, 0}
};

#ifdef SIGINFO
volatile sig_atomic_t info;

static void
siginfo(int signo)
{
	info = signo;
}
#endif

static void usage(void) __dead2;

static bool
parse_iskipspec(char *spec, off_t *skip1, off_t *skip2)
{
	char *colon;

	colon = strchr(spec, ':');
	if (colon != NULL)
		*colon++ = '\0';

	if (expand_number(spec, skip1) < 0)
		return (false);

	if (colon != NULL)
		return (expand_number(colon, skip2) == 0);

	*skip2 = *skip1;
	return (true);
}

int
main(int argc, char *argv[])
{
	struct stat sb1, sb2;
	off_t skip1, skip2, limit;
	int ch, fd1, fd2, oflag;
	bool special;
	const char *file1, *file2;
	int ret;

	limit = skip1 = skip2 = ret = 0;
	oflag = O_RDONLY;
	while ((ch = getopt_long(argc, argv, "+bhi:ln:sxz", long_opts, NULL)) != -1)
		switch (ch) {
		case 'b':		/* Print bytes */
			bflag = true;
			break;
		case 'h':		/* Don't follow symlinks */
			oflag |= O_NOFOLLOW;
			break;
		case 'i':
			if (!parse_iskipspec(optarg, &skip1, &skip2)) {
				fprintf(stderr,
				    "Invalid --ignore-initial: %s\n",
				    optarg);
				usage();
			}
			break;
		case 'l':		/* print all differences */
			lflag = true;
			break;
		case 'n':		/* Limit */
			if (expand_number(optarg, &limit) < 0 || limit < 0) {
				fprintf(stderr, "Invalid --bytes: %s\n",
				    optarg);
				usage();
			}
			break;
		case 's':		/* silent run */
			sflag = true;
			break;
		case 'x':		/* hex output */
			lflag = true;
			xflag = true;
			break;
		case 'z':		/* compare size first */
			zflag = true;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (lflag && sflag)
		errx(ERR_EXIT, "specifying -s with -l or -x is not permitted");

	if (argc < 2 || argc > 4)
		usage();

	/* Don't limit rights on stdin since it may be one of the inputs. */
	if (caph_limit_stream(STDOUT_FILENO, CAPH_WRITE | CAPH_IGNORE_EBADF))
		err(ERR_EXIT, "unable to limit rights on stdout");
	if (caph_limit_stream(STDERR_FILENO, CAPH_WRITE | CAPH_IGNORE_EBADF))
		err(ERR_EXIT, "unable to limit rights on stderr");

	/* Backward compatibility -- handle "-" meaning stdin. */
	special = false;
	if (strcmp(file1 = argv[0], "-") == 0) {
		special = true;
		fd1 = STDIN_FILENO;
		file1 = "stdin";
	} else if ((fd1 = open(file1, oflag, 0)) < 0 && errno != EMLINK) {
		if (!sflag)
			err(ERR_EXIT, "%s", file1);
		else
			exit(ERR_EXIT);
	}
	if (strcmp(file2 = argv[1], "-") == 0) {
		if (special)
			errx(ERR_EXIT,
				"standard input may only be specified once");
		special = true;
		fd2 = STDIN_FILENO;
		file2 = "stdin";
	} else if ((fd2 = open(file2, oflag, 0)) < 0 && errno != EMLINK) {
		if (!sflag)
			err(ERR_EXIT, "%s", file2);
		else
			exit(ERR_EXIT);
	}

	if (argc > 2 && expand_number(argv[2], &skip1) < 0) {
		fprintf(stderr, "Invalid skip1: %s\n", argv[2]);
		usage();
	}

	if (argc == 4 && expand_number(argv[3], &skip2) < 0) {
		fprintf(stderr, "Invalid skip2: %s\n", argv[3]);
		usage();
	}

	if (sflag && skip1 == 0 && skip2 == 0)
		zflag = true;

	if (fd1 == -1) {
		if (fd2 == -1) {
			ret = c_link(file1, skip1, file2, skip2, limit);
			goto end;
		} else if (!sflag)
			errx(ERR_EXIT, "%s: Not a symbolic link", file2);
		else
			exit(ERR_EXIT);
	} else if (fd2 == -1) {
		if (!sflag)
			errx(ERR_EXIT, "%s: Not a symbolic link", file1);
		else
			exit(ERR_EXIT);
	}

	/* FD rights are limited in c_special() and c_regular(). */
	caph_cache_catpages();

	if (!special) {
		if (fstat(fd1, &sb1)) {
			if (!sflag)
				err(ERR_EXIT, "%s", file1);
			else
				exit(ERR_EXIT);
		}
		if (!S_ISREG(sb1.st_mode))
			special = true;
		else {
			if (fstat(fd2, &sb2)) {
				if (!sflag)
					err(ERR_EXIT, "%s", file2);
				else
					exit(ERR_EXIT);
			}
			if (!S_ISREG(sb2.st_mode))
				special = true;
		}
	}

#ifdef SIGINFO
	(void)signal(SIGINFO, siginfo);
#endif
	if (special) {
		ret = c_special(fd1, file1, skip1, fd2, file2, skip2, limit);
	} else {
		if (zflag && sb1.st_size != sb2.st_size) {
			if (!sflag)
				(void)printf("%s %s differ: size\n",
				    file1, file2);
			ret = DIFF_EXIT;
		} else {
			ret = c_regular(fd1, file1, skip1, sb1.st_size,
			    fd2, file2, skip2, sb2.st_size, limit);
		}
	}
end:
	if (!sflag && fflush(stdout) != 0)
		err(ERR_EXIT, "stdout");
	exit(ret);
}

static void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: cmp [-l | -s | -x] [-hz] file1 file2 [skip1 [skip2]]\n");
	exit(ERR_EXIT);
}
