/*
 * Copyright (c) 1987 Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1987 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)xinstall.c	5.24 (Berkeley) 7/1/90";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <ctype.h>
#include <paths.h>
#include "pathnames.h"

static struct passwd *pp;
static struct group *gp;
static int docopy, dostrip, mode = 0755;
static char *group, *owner, pathbuf[MAXPATHLEN];

main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int optind;
	struct stat from_sb, to_sb;
	mode_t *set, *setmode();
	int ch, no_target;
	char *to_name;

	while ((ch = getopt(argc, argv, "cg:m:o:s")) != EOF)
		switch((char)ch) {
		case 'c':
			docopy = 1;
			break;
		case 'g':
			group = optarg;
			break;
		case 'm':
			if (!(set = setmode(optarg))) {
				(void)fprintf(stderr,
				    "install: invalid file mode.\n");
				exit(1);
			}
			mode = getmode(set, 0);
			break;
		case 'o':
			owner = optarg;
			break;
		case 's':
			dostrip = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc < 2)
		usage();

	/* get group and owner id's */
	if (group && !(gp = getgrnam(group))) {
		fprintf(stderr, "install: unknown group %s.\n", group);
		exit(1);
	}
	if (owner && !(pp = getpwnam(owner))) {
		fprintf(stderr, "install: unknown user %s.\n", owner);
		exit(1);
	}

	no_target = stat(to_name = argv[argc - 1], &to_sb);
	if (!no_target && (to_sb.st_mode & S_IFMT) == S_IFDIR) {
		for (; *argv != to_name; ++argv)
			install(*argv, to_name, 1);
		exit(0);
	}

	/* can't do file1 file2 directory/file */
	if (argc != 2)
		usage();

	if (!no_target) {
		if (stat(*argv, &from_sb)) {
			fprintf(stderr, "install: can't find %s.\n", *argv);
			exit(1);
		}
		if ((to_sb.st_mode & S_IFMT) != S_IFREG) {
			fprintf(stderr, "install: %s isn't a regular file.\n", to_name);
			exit(1);
		}
		if (to_sb.st_dev == from_sb.st_dev && to_sb.st_ino == from_sb.st_ino) {
			fprintf(stderr, "install: %s and %s are the same file.\n", *argv, to_name);
			exit(1);
		}
		/* unlink now... avoid ETXTBSY errors later */
		(void)unlink(to_name);
	}
	install(*argv, to_name, 0);
	exit(0);
}

/*
 * install --
 *	build a path name and install the file
 */
install(from_name, to_name, isdir)
	char *from_name, *to_name;
	int isdir;
{
	struct stat from_sb;
	int devnull, from_fd, to_fd;
	char *C, *rindex();

	/* if try to install NULL file to a directory, fails */
	if (isdir || strcmp(from_name, _PATH_DEVNULL)) {
		if (stat(from_name, &from_sb)) {
			fprintf(stderr, "install: can't find %s.\n", from_name);
			exit(1);
		}
		if ((from_sb.st_mode & S_IFMT) != S_IFREG) {
			fprintf(stderr, "install: %s isn't a regular file.\n", from_name);
			exit(1);
		}
		/* build the target path */
		if (isdir) {
			(void)sprintf(pathbuf, "%s/%s", to_name, (C = rindex(from_name, '/')) ? ++C : from_name);
			to_name = pathbuf;
		}
		devnull = 0;
	} else
		devnull = 1;

	/* unlink now... avoid ETXTBSY errors later */
	(void)unlink(to_name);

	/* create target */
	if ((to_fd = open(to_name, O_CREAT|O_WRONLY|O_TRUNC, 0600)) < 0) {
		error(to_name);
		exit(1);
	}
	if (!devnull) {
		if ((from_fd = open(from_name, O_RDONLY, 0)) < 0) {
			(void)unlink(to_name);
			error(from_name);
			exit(1);
		}
		copy(from_fd, from_name, to_fd, to_name);
		(void)close(from_fd);
	}
	if (dostrip)
		strip(to_name);
	/*
	 * set owner, group, mode for target; do the chown first,
	 * chown may lose the setuid bits.
	 */
	if ((group || owner) &&
	    fchown(to_fd, owner ? pp->pw_uid : -1, group ? gp->gr_gid : -1) ||
	    fchmod(to_fd, mode)) {
		error(to_name);
		bad(to_name);
	}
	(void)close(to_fd);
	if (!docopy && !devnull && unlink(from_name)) {
		error(from_name);
		exit(1);
	}
}

/*
 * copy --
 *	copy from one file to another
 */
copy(from_fd, from_name, to_fd, to_name)
	register int from_fd, to_fd;
	char *from_name, *to_name;
{
	register int n;
	char buf[MAXBSIZE];

	while ((n = read(from_fd, buf, sizeof(buf))) > 0)
		if (write(to_fd, buf, n) != n) {
			error(to_name);
			bad(to_name);
		}
	if (n == -1) {
		error(from_name);
		bad(to_name);
	}
}

/*
 * strip --
 *	use strip(1) to strip the target file
 */
strip(to_name)
	char *to_name;
{
	int status;

	switch (vfork()) {
	case -1:
		error("fork");
		bad(to_name);
	case 0:
		execl(_PATH_STRIP, "strip", to_name, (char *)NULL);
		error(_PATH_STRIP);
		_exit(1);
	default:
		if (wait(&status) == -1 || status)
			bad(to_name);
	}
}

/*
 * error --
 *	print out an error message
 */
error(s)
	char *s;
{
	extern int errno;
	char *strerror();

	(void)fprintf(stderr, "install: %s: %s\n", s, strerror(errno));
}

/*
 * bad --
 *	remove created target and die
 */
bad(fname)
	char *fname;
{
	(void)unlink(fname);
	exit(1);
}

/*
 * usage --
 *	print a usage message and die
 */
usage()
{
	(void)fprintf(stderr,
"usage: install [-cs] [-g group] [-m mode] [-o owner] file1 file2;\n\tor file1 ... fileN directory\n");
	exit(1);
}
