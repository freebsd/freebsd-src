/*
 * Copyright (c) 1983, 1992, 1993
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
 *
 *	$Id: mkdir.c,v 1.9 1997/03/28 15:24:25 imp Exp $
 */

#ifndef lint
static char const copyright[] =
"@(#) Copyright (c) 1983, 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char const sccsid[] = "@(#)mkdir.c	8.2 (Berkeley) 1/25/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int	build __P((char *, mode_t));
void	usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, exitval, omode, pflag;
	mode_t *set = (mode_t *)NULL;
	char *mode;

	omode = pflag = 0;
	mode = NULL;
	while ((ch = getopt(argc, argv, "m:p")) != -1)
		switch(ch) {
		case 'p':
			pflag = 1;
			break;
		case 'm':
			mode = optarg;
			break;
		case '?':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;
	if (argv[0] == NULL)
		usage();

	if (mode == NULL) {
		omode = S_IRWXU | S_IRWXG | S_IRWXO;
	} else {
		if ((set = setmode(mode)) == NULL)
			errx(1, "invalid file mode: %s", mode);
		omode = getmode(set, S_IRWXU | S_IRWXG | S_IRWXO);
	}

	for (exitval = 0; *argv != NULL; ++argv) {
		if (pflag) {
			if (build(*argv, omode))
				exitval = 1;
			continue;
		}
		if (mkdir(*argv, omode) < 0) {
			warn("%s", *argv);
			exitval = 1;
		}
	}
	exit(exitval);
}

int
build(path, omode)
	char *path;
	mode_t omode;
{
	struct stat sb;
	mode_t numask, oumask;
	int first, last, retval;
	char *p;

	p = path;
	oumask = 0;
	retval = 0;
	if (p[0] == '/')		/* Skip leading '/'. */
		++p;
	for (first = 1, last = 0; !last ; ++p) {
		if (p[0] == '\0')
			last = 1;
		else if (p[0] != '/')
			continue;
		*p = '\0';
		if (p[1] == '\0')
			last = 1;
		if (first) {
			/*
			 * POSIX 1003.2:
			 * For each dir operand that does not name an existing
			 * directory, effects equivalent to those cased by the
			 * following command shall occcur:
			 *
			 * mkdir -p -m $(umask -S),u+wx $(dirname dir) &&
			 *    mkdir [-m mode] dir
			 *
			 * We change the user's umask and then restore it,
			 * instead of doing chmod's.
			 */
			oumask = umask(0);
			numask = oumask & ~(S_IWUSR | S_IXUSR);
			(void)umask(numask);
			first = 0;
		}
		if (last)
			(void)umask(oumask);
		if (stat(path, &sb)) {
			if (errno != ENOENT ||
			    mkdir(path, last ? omode : 
				  S_IRWXU | S_IRWXG | S_IRWXO) < 0) {
				warn("%s", path);
				retval = 1;
				break;
			}
		}
		else if ((sb.st_mode & S_IFMT) != S_IFDIR) {
			if (last)
				errno = EEXIST;
			else
				errno = ENOTDIR;
			warn("%s", path);
			retval = 1;
			break;
		}
		*p = '/';
	}
	if (!first && !last)
		(void)umask(oumask);
	return (retval);
}

void
usage()
{
	(void)fprintf(stderr, "usage: mkdir [-p] [-m mode] directory ...\n");
	exit (1);
}
