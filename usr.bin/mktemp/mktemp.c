/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1994, 1995, 1996, 1998 Peter Wemm <peter@netplex.com.au>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * This program was originally written long ago, originally for a non
 * BSD-like OS without mkstemp().  It's been modified over the years
 * to use mkstemp() rather than the original O_CREAT|O_EXCL/fstat/lstat
 * etc style hacks.
 * A cleanup, misc options and mkdtemp() calls were added to try and work
 * more like the OpenBSD version - which was first to publish the interface.
 */

#include <err.h>
#include <getopt.h>
#include <paths.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void) __dead2;

static const struct option long_opts[] = {
	{"directory",	no_argument,	NULL,	'd'},
	{"tmpdir",	optional_argument,	NULL,	'p'},
	{"quiet",	no_argument,	NULL,	'q'},
	{"dry-run",	no_argument,	NULL,	'u'},
	{NULL,		no_argument,	NULL,	0},
};

int
main(int argc, char **argv)
{
	int c, fd, ret;
	const char *prefix, *tmpdir;
	char *name;
	int dflag, qflag, tflag, uflag;
	bool prefer_tmpdir;

	ret = dflag = qflag = tflag = uflag = 0;
	prefer_tmpdir = true;
	prefix = "mktemp";
	name = NULL;
	tmpdir = NULL;

	while ((c = getopt_long(argc, argv, "dp:qt:u", long_opts, NULL)) != -1)
		switch (c) {
		case 'd':
			dflag++;
			break;

		case 'p':
			tmpdir = optarg;
			if (tmpdir == NULL || *tmpdir == '\0')
				tmpdir = getenv("TMPDIR");

			/*
			 * We've already done the necessary environment
			 * fallback, skip the later one.
			 */
			prefer_tmpdir = false;
			break;

		case 'q':
			qflag++;
			break;

		case 't':
			prefix = optarg;
			tflag++;
			break;

		case 'u':
			uflag++;
			break;

		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (!tflag && argc < 1) {
		tflag = 1;
		prefix = "tmp";

		/*
		 * For this implied -t mode, we actually want to swap the usual
		 * order of precedence: -p, then TMPDIR, then /tmp.
		 */
		prefer_tmpdir = false;
	}

	if (tflag) {
		const char *envtmp;
		size_t len;

		envtmp = NULL;

		/*
		 * $TMPDIR preferred over `-p` if specified, for compatibility.
		 */
		if (prefer_tmpdir || tmpdir == NULL)
			envtmp = getenv("TMPDIR");
		if (envtmp != NULL)
			tmpdir = envtmp;
		if (tmpdir == NULL)
			tmpdir = _PATH_TMP;
		len = strlen(tmpdir);
		if (len > 0 && tmpdir[len - 1] == '/')
			asprintf(&name, "%s%s.XXXXXXXXXX", tmpdir, prefix);
		else
			asprintf(&name, "%s/%s.XXXXXXXXXX", tmpdir, prefix);
		/* if this fails, the program is in big trouble already */
		if (name == NULL) {
			if (qflag)
				return (1);
			else
				errx(1, "cannot generate template");
		}
	}

	/* generate all requested files */
	while (name != NULL || argc > 0) {
		if (name == NULL) {
			if (!tflag && tmpdir != NULL)
				asprintf(&name, "%s/%s", tmpdir, argv[0]);
			else
				name = strdup(argv[0]);
			if (name == NULL)
				err(1, "%s", argv[0]);
			argv++;
			argc--;
		}

		if (dflag) {
			if (mkdtemp(name) == NULL) {
				ret = 1;
				if (!qflag)
					warn("mkdtemp failed on %s", name);
			} else {
				printf("%s\n", name);
				if (uflag)
					rmdir(name);
			}
		} else {
			fd = mkstemp(name);
			if (fd < 0) {
				ret = 1;
				if (!qflag)
					warn("mkstemp failed on %s", name);
			} else {
				close(fd);
				if (uflag)
					unlink(name);
				printf("%s\n", name);
			}
		}
		if (name)
			free(name);
		name = NULL;
	}
	return (ret);
}

static void
usage(void)
{
	fprintf(stderr,
		"usage: mktemp [-d] [-p tmpdir] [-q] [-t prefix] [-u] template "
		"...\n");
	fprintf(stderr,
		"       mktemp [-d] [-p tmpdir] [-q] [-u] -t prefix \n");
	exit (1);
}
