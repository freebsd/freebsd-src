/*
 * Copyright (c) 1991, 1993, 1994
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
static char const copyright[] =
"@(#) Copyright (c) 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)pwd.c	8.3 (Berkeley) 4/1/94";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>

static char *getcwd_logical(void);
void usage(void);

int
main(int argc, char *argv[])
{
	int Lflag, Pflag;
	int ch;
	char *p;
	char buf[PATH_MAX];

	if (strcmp(getprogname(), "realpath") == 0) {
		if (argc != 2)
			usage();
		if ((p = realpath(argv[1], buf)) == NULL)
			err(1, "%s", argv[1]);
		(void)printf("%s\n", p);
		exit(0);
	}

	Lflag = Pflag = 0;
	while ((ch = getopt(argc, argv, "LP")) != -1)
		switch (ch) {
		case 'L':
			Lflag = 1;
			break;
		case 'P':
			Pflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 0 || (Lflag && Pflag))
		usage();

	p = Lflag ? getcwd_logical() : getcwd(NULL, 0);
	if (p == NULL)
		err(1, ".");
	(void)printf("%s\n", p);

	exit(0);
}

void
usage(void)
{

	if (strcmp(getprogname(), "realpath") == 0)
		(void)fprintf(stderr, "usage: realpath [path]\n");
	else
		(void)fprintf(stderr, "usage: pwd [-L | -P]\n");
  	exit(1);
}

static char *
getcwd_logical(void)
{
	struct stat log, phy;
	char *pwd;

	/*
	 * Check that $PWD is an absolute logical pathname referring to
	 * the current working directory.
	 */
	if ((pwd = getenv("PWD")) != NULL && *pwd == '/') {
		if (stat(pwd, &log) == -1 || stat(".", &phy) == -1)
			return (NULL);
		if (log.st_dev == phy.st_dev && log.st_ino == phy.st_ino)
			return (pwd);
	}

	errno = ENOENT;
	return (NULL);
}
