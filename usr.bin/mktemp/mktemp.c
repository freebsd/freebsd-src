/*-
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
 * $FreeBSD$
 */

/*
 * This program was originally written long ago, originally for a non
 * BSD-like OS without mkstemp().  It's been modified over the years
 * to use mkstemp() rather than the original O_CREAT|O_EXCL/fstat/lstat
 * etc style hacks.
 * A cleanup, misc options and mkdtemp() calls were added to try and work
 * more like the OpenBSD version - which was first to publish the interface.
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <paths.h>
#include <err.h>
#include <string.h>

int
main(int argc, char **argv)
{
	int c, fd, ret;
	char *usage = "[-d] [-q] [-t prefix] [-u] [template ...]";
	char *tmpdir, *prefix;
	char *prog;
	char *name;
	int dflag, qflag, tflag, uflag;

	ret = dflag = qflag = tflag = uflag = 0;
	name = NULL;
	prog = argv[0];		/* XXX basename(argv[0]) */

	while ((c = getopt(argc, argv, "dqt:u")) != -1)
		switch (c) {
		case 'd':
			dflag++;
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
			fprintf(stderr, "Usage: %s %s\n", prog, usage);
			return (1);
		}

	argc -= optind;
	argv += optind;

	if (tflag) {
		tmpdir = getenv("TMPDIR");
		if (prefix == NULL)
			prefix = "mktemp";	/* shouldn't happen, but.. */
		if (tmpdir == NULL)
			asprintf(&name, "%s%s.XXXXXXXX", _PATH_TMP, prefix);
		else
			asprintf(&name, "%s/%s.XXXXXXXX", tmpdir, prefix);
		/* if this fails, the program is in big trouble already */
		if (name == NULL) {
			if (qflag)
				return (1);
			else
				err(1, "cannot generate template");
		}
	} else if (argc < 1) {
		fprintf(stderr, "Usage: %s %s\n", prog, usage);
		return (1);
	}
		
	/* generate all requested files */
	while (name != NULL || argc > 0) {
		if (name == NULL) {
			name = strdup(argv[0]);
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
