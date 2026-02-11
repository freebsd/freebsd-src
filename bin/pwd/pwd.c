/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2026 Dag-Erling Smørgrav
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

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static char *
getcwd_logical(void)
{
	struct stat log, phy;
	char *pwd, *p, *q;

	/* $PWD is set and absolute */
	if ((pwd = getenv("PWD")) == NULL || *pwd != '/')
		return (NULL);
	/* $PWD does not contain /./ or /../ */
	for (p = pwd; *p; p = q) {
		for (q = ++p; *q && *q != '/'; q++)
			/* nothing */;
		if ((*p == '.' && q == ++p) ||
		    (*p == '.' && q == ++p))
			return (NULL);
	}
	/* $PWD refers to the current directory */
	if (stat(pwd, &log) != 0 || stat(".", &phy) != 0 ||
	    log.st_dev != phy.st_dev || log.st_ino != phy.st_ino)
		return (NULL);
	return (pwd);
}

static char *
getcwd_physical(void)
{
	static char pwd[MAXPATHLEN];

	return (getcwd(pwd, sizeof(pwd)));
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: pwd [-L | -P]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *pwd;
	int opt;
	bool logical;

	logical = true;
	while ((opt = getopt(argc, argv, "LP")) != -1) {
		switch (opt) {
		case 'L':
			logical = true;
			break;
		case 'P':
			logical = false;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 0)
		usage();

	/*
	 * If we're trying to find the logical current directory and that
	 * fails, behave as if -P was specified.
	 */
	if ((logical && (pwd = getcwd_logical()) != NULL) ||
	    (pwd = getcwd_physical()) != NULL)
		printf("%s\n", pwd);
	else
		err(1, ".");
	if (fflush(stdout) != 0)
		err(1, "stdout");
	exit(0);
}
