/*
 * Copyright (c) 1987, 1993, 1994
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
"@(#) Copyright (c) 1987, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)ln.c	8.2 (Berkeley) 3/31/94";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int	fflag;				/* Unlink existing files. */
int	iflag;				/* Interactive mode. */
int	sflag;				/* Symbolic, not hard, link. */
int	vflag;				/* Verbose output. */
					/* System link call. */
int (*linkf) __P((const char *, const char *));
char	linkch;

int	linkit __P((char *, char *, int));
void	usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct stat sb;
	int ch, exitval;
	char *p, *sourcedir;

	/*
	 * Test for the special case where the utility is called as
	 * "link", for which the functionality provided is greatly
	 * simplified.
	 */
	if ((p = rindex(argv[0], '/')) == NULL)
		p = argv[0];
	else
		++p;
	if (strcmp(p, "link") == 0) {
		if (argc == 3) {
			linkf = link;
			exit(linkit(argv[1], argv[2], 0));
		} else
			usage();
	}

	fflag = iflag = sflag = vflag = 0;

	while ((ch = getopt(argc, argv, "fisv")) != -1)
		switch (ch) {
		case 'f':
			fflag = 1;
			iflag = 0;	/* -f overrides iflag */
			break;
		case 'i':
			iflag = 1;
			fflag = 0;	/* -i overrides fflag */
			break;
		case 's':
			sflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		case '?':
		default:
			usage();
		}

	argv += optind;
	argc -= optind;

	linkf = sflag ? symlink : link;
	linkch = sflag ? '-' : '=';

	switch(argc) {
	case 0:
		usage();
	case 1:				/* ln target */
		exit(linkit(argv[0], ".", 1));
	case 2:				/* ln target source */
		exit(linkit(argv[0], argv[1], 0));
	}
					/* ln target1 target2 directory */
	sourcedir = argv[argc - 1];
	if (stat(sourcedir, &sb))
		err(1, "%s", sourcedir);
	if (!S_ISDIR(sb.st_mode))
		usage();
	for (exitval = 0; *argv != sourcedir; ++argv)
		exitval |= linkit(*argv, sourcedir, 1);
	exit(exitval);
}

int
linkit(target, source, isdir)
	char *target, *source;
	int isdir;
{
	struct stat sb;
	int exists, ch, first;
	char *p, path[MAXPATHLEN];

	if (!sflag) {
		/* If target doesn't exist, quit now. */
		if (stat(target, &sb)) {
			warn("%s", target);
			return (1);
		}
		/* Only symbolic links to directories. */
		if (S_ISDIR(sb.st_mode)) {
			errno = EISDIR;
			warn("%s", target);
			return (1);
		}
	}

	/* If the source is a directory, append the target's name. */
	if (isdir || ((exists = !stat(source, &sb)) && S_ISDIR(sb.st_mode))) {
		if ((p = strrchr(target, '/')) == NULL)
			p = target;
		else
			++p;
		(void)snprintf(path, sizeof(path), "%s/%s", source, p);
		source = path;
		exists = !lstat(source, &sb);
	} else
		exists = !lstat(source, &sb);

	/*
	 * If the file exists, and -f was specified, unlink it.
	 * Attempt the link.
	 */
	if (fflag && exists && unlink(source)) {
		warn("%s", source);
		return (1);
	} else if (iflag && exists) {
		fprintf(stderr, "replace %s? ", source);
		fflush(stderr);

		first = ch = getchar();
		while(ch != '\n' && ch != EOF)
			ch = getchar();

		if ((first == 'y' || first == 'Y') && unlink(source)) {
			warn("%s", source);
			return (1);
		}
	}
	if ((*linkf)(target, source)) {
		warn("%s", source);
		return (1);
	}
	if (vflag)
		(void)printf("%s %c> %s\n", source, linkch, target);
	return (0);
}

void
usage()
{
	(void)fprintf(stderr, "%s\n%s\n%s\n",
	    "usage: ln [-fisv] file1 file2",
	    "       ln [-fisv] file ... directory",
	    "       link file1 file2");
	exit(1);
}
