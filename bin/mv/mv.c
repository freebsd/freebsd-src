/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ken Smith of The State University of New York at Buffalo.
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
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)mv.c	5.11 (Berkeley) 4/3/91";*/
static char rcsid[] = "$Id: mv.c,v 1.2 1993/11/22 23:54:24 jtc Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pathnames.h"

int fflg, iflg;
int stdin_ok;

main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int optind;
	register int baselen, exitval, len;
	register char *p, *endp;
	struct stat sb;
	int ch;
	char path[MAXPATHLEN + 1];

	while (((ch = getopt(argc, argv, "if")) != -1))
		switch((char)ch) {
		case 'i':
			fflg = 0;
			iflg = 1;
			break;
		case 'f':
			iflg = 0;
			fflg = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 2)
		usage();

	stdin_ok = isatty(STDIN_FILENO);

	/*
	 * If the stat on the target fails or the target isn't a directory,
	 * try the move.  More than 2 arguments is an error in this case.
	 */
	if (stat(argv[argc - 1], &sb) || !S_ISDIR(sb.st_mode)) {
		if (argc > 2)
			usage();
		exit(do_move(argv[0], argv[1]));
	}

	/* It's a directory, move each file into it. */
	(void)strcpy(path, argv[argc - 1]);
	baselen = strlen(path);
	endp = &path[baselen];
	*endp++ = '/';
	++baselen;
	for (exitval = 0; --argc; ++argv) {
		if ((p = rindex(*argv, '/')) == NULL)
			p = *argv;
		else
			++p;
		if ((baselen + (len = strlen(p))) >= MAXPATHLEN)
			(void)fprintf(stderr,
			    "mv: %s: destination pathname too long\n", *argv);
		else {
			bcopy(p, endp, len + 1);
			exitval |= do_move(*argv, path);
		}
	}
	exit(exitval);
}

do_move(from, to)
	char *from, *to;
{
	struct stat sb;

	/* (1)	If the destination path exists, the -f option is not specified
	 *	and either of the following conditions are true:
	 *
	 *	(a) The perimissions of the destination path do not permit
	 *	    writing and the standard input is a terminal.
	 *	(b) The -i option is specified.
	 *
	 *	the mv utility shall write a prompt to standard error and 
	 *	read a line from standard input.  If the response is not
	 *	affirmative, mv shall do nothing more with the current
	 *	source file...
	 */
	if (!fflg && !access(to, F_OK)) {
		int ask = 0;
		int ch;

		if (iflg) {
			(void)fprintf(stderr, "overwrite %s? ", to);
			ask = 1;
		} else if (stdin_ok && access(to, W_OK) && !stat(to, &sb)) {
			(void)fprintf(stderr, "override mode %o on %s? ",
			    sb.st_mode & 07777, to);
			ask = 1;
		}
		if (ask) {
			if ((ch = getchar()) != EOF && ch != '\n')
				while (getchar() != '\n');
			if (ch != 'y' && ch != 'Y')
				return(0);
		}
	}


	/* (2)	If rename() succeeds, mv shall do nothing more with the 
	 *	current source file.  If it fails for any other reason than
	 *	EXDEV, mv shall write a diagnostic message to the standard
	 *	error and do nothing more with the current source file.
	 *
	 * (3)	If the destination path exists, and it is a file of type
	 *	directory and source_file is not a file of type directory,
	 *	or it is a file not of type directory, and source file is
	 *	a file of type directory, mv shall write a diagnostic 
	 *	message to standard error, and do nothing more with the
	 *	current source file...
	 */
	if (!rename(from, to))
		return(0);

	if (errno != EXDEV) {
		(void)fprintf(stderr,
		    "mv: rename %s to %s: %s\n", from, to, strerror(errno));
		return(1);
	}


	/* (4)	If the destination path exists, mv shall attempt to remove it.
	 *	If this fails for any reason, mv shall write a diagnostic 
	 *	message to the standard error and do nothing more with the
	 *	current source file...
	 */
	if (!stat(to, &sb)) {
		if ((S_ISDIR(sb.st_mode)) ? rmdir(to) : unlink(to)) {
			(void) fprintf(stderr,
			    "mv: can't remove %s: %s\n", to, strerror(errno));
			return (1);
		}
	}


	/* (5)	The file hierarchy rooted in source_file shall be duplicated
	 *	as a file hiearchy rooted in the destination path...
	 */
	if (stat(from, &sb)) {
		(void)fprintf(stderr, "mv: %s: %s\n", from, strerror(errno));
		return(1);
	}
	return(S_ISREG(sb.st_mode) ?
	    fastcopy(from, to, &sb) : copy(from, to));
}

fastcopy(from, to, sbp)
	char *from, *to;
	struct stat *sbp;
{
	struct timeval tval[2];
	static u_int blen;
	static char *bp;
	register int nread, from_fd, to_fd;

	if ((from_fd = open(from, O_RDONLY, 0)) < 0) {
		error(from);
		return(1);
	}
	if ((to_fd = open(to, O_CREAT|O_TRUNC|O_WRONLY, sbp->st_mode)) < 0) {
		error(to);
		(void)close(from_fd);
		return(1);
	}
	if (!blen && !(bp = malloc(blen = sbp->st_blksize))) {
		error(NULL);
		return(1);
	}
	while ((nread = read(from_fd, bp, blen)) > 0)
		if (write(to_fd, bp, nread) != nread) {
			error(to);
			goto err;
		}
	if (nread < 0) {
		error(from);
err:		(void)unlink(to);
		(void)close(from_fd);
		(void)close(to_fd);
		return(1);
	}
	(void)fchown(to_fd, sbp->st_uid, sbp->st_gid);
	(void)fchmod(to_fd, sbp->st_mode);

	(void)close(from_fd);
	(void)close(to_fd);

	tval[0].tv_sec = sbp->st_atime;
	tval[1].tv_sec = sbp->st_mtime;
	tval[0].tv_usec = tval[1].tv_usec = 0;
	(void)utimes(to, tval);
	(void)unlink(from);
	return(0);
}

copy(from, to)
	char *from, *to;
{
	int pid, status;

	if (!(pid = vfork())) {
		execl(_PATH_CP, "mv", "-pr", from, to, NULL);
		error(_PATH_CP);
		_exit(1);
	}
	(void)waitpid(pid, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status))
		return(1);
	if (!(pid = vfork())) {
		execl(_PATH_RM, "mv", "-rf", from, NULL);
		error(_PATH_RM);
		_exit(1);
	}
	(void)waitpid(pid, &status, 0);
	return(!WIFEXITED(status) || WEXITSTATUS(status));
}

error(s)
	char *s;
{
	if (s)
		(void)fprintf(stderr, "mv: %s: %s\n", s, strerror(errno));
	else
		(void)fprintf(stderr, "mv: %s\n", strerror(errno));
}

usage()
{
	(void)fprintf(stderr, 
		"usage: mv [-fi] source_file target_file\n"
		"       mv [-fi] source_file ... target_dir\n");
	exit(1);
}
