/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
static char const copyright[] =
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mv.c	8.2 (Berkeley) 4/2/94";
#endif
static const char rcsid[] =
	"$Id: mv.c,v 1.19 1998/05/25 22:44:16 steve Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"

int fflg, iflg;

int	copy __P((char *, char *));
int	do_move __P((char *, char *));
int	fastcopy __P((char *, char *, struct stat *));
void	usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register int baselen, len, rval;
	register char *p, *endp;
	struct stat sb;
	int ch;
	char path[MAXPATHLEN];

	while ((ch = getopt(argc, argv, "fi")) != -1)
		switch (ch) {
		case 'i':
			iflg = 1;
			fflg = 0;
			break;
		case 'f':
			fflg = 1;
			iflg = 0;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 2)
		usage();

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
	if (strlen(argv[argc - 1]) > sizeof(path) - 1)
		errx(1, "%s: destination pathname too long", *argv);
	(void)strcpy(path, argv[argc - 1]);
	baselen = strlen(path);
	endp = &path[baselen];
	if (!baselen || *(endp - 1) != '/') {
		*endp++ = '/';
		++baselen;
	}
	for (rval = 0; --argc; ++argv) {
		/*
		 * Find the last component of the source pathname.  It
		 * may have trailing slashes.
		 */
		p = *argv + strlen(*argv);
		while (p != *argv && p[-1] == '/')
			--p;
		while (p != *argv && p[-1] != '/')
			--p;

		if ((baselen + (len = strlen(p))) >= MAXPATHLEN) {
			warnx("%s: destination pathname too long", *argv);
			rval = 1;
		} else {
			memmove(endp, p, len + 1);
			if (do_move(*argv, path))
				rval = 1;
		}
	}
	exit(rval);
}

int
do_move(from, to)
	char *from, *to;
{
	struct stat sb;
	int ask, ch, first;
	char modep[15];

	/*
	 * Check access.  If interactive and file exists, ask user if it
	 * should be replaced.  Otherwise if file exists but isn't writable
	 * make sure the user wants to clobber it.
	 */
	if (!fflg && !access(to, F_OK)) {

		/* prompt only if source exist */
	        if (lstat(from, &sb) == -1) {
			warn("%s", from);
			return (1);
		}

#define YESNO "(y/n [n]) "
		ask = 0;
		if (iflg) {
			(void)fprintf(stderr, "overwrite %s? %s", to, YESNO);
			ask = 1;
		} else if (access(to, W_OK) && !stat(to, &sb)) {
			strmode(sb.st_mode, modep);
			(void)fprintf(stderr, "override %s%s%s/%s for %s? %s",
			    modep + 1, modep[9] == ' ' ? "" : " ",
			    user_from_uid(sb.st_uid, 0),
			    group_from_gid(sb.st_gid, 0), to, YESNO);
			ask = 1;
		}
		if (ask) {
			first = ch = getchar();
			while (ch != '\n' && ch != EOF)
				ch = getchar();
			if (first != 'y' && first != 'Y') {
				(void)fprintf(stderr, "not overwritten\n");
				return (0);
			}
		}
	}
	if (!rename(from, to))
		return (0);

	if (errno == EXDEV) {
		struct statfs sfs;
		char path[MAXPATHLEN];

		/* Can't mv(1) a mount point. */
		if (realpath(from, path) == NULL) {
			warnx("cannot resolve %s: %s", from, path);
			return (1);
		}
		if (!statfs(path, &sfs) && !strcmp(path, sfs.f_mntonname)) {
			warnx("cannot rename a mount point");
			return (1);
		}
	} else {
		warn("rename %s to %s", from, to);
		return (1);
	}

	/*
	 * If rename fails because we're trying to cross devices, and
	 * it's a regular file, do the copy internally; otherwise, use
	 * cp and rm.
	 */
	if (stat(from, &sb)) {
		warn("%s", from);
		return (1);
	}
	return (S_ISREG(sb.st_mode) ?
	    fastcopy(from, to, &sb) : copy(from, to));
}

int
fastcopy(from, to, sbp)
	char *from, *to;
	struct stat *sbp;
{
	struct timeval tval[2];
	static u_int blen;
	static char *bp;
	mode_t oldmode;
	register int nread, from_fd, to_fd;

	if ((from_fd = open(from, O_RDONLY, 0)) < 0) {
		warn("%s", from);
		return (1);
	}
	if (blen < sbp->st_blksize) {
		if (bp != NULL)
			free(bp);
		if ((bp = malloc(sbp->st_blksize)) == NULL) {
			blen = 0;
			warnx("malloc failed");
			return (1);
		}
		blen = sbp->st_blksize;
	}
	while ((to_fd =
	    open(to, O_CREAT | O_EXCL | O_TRUNC | O_WRONLY, 0)) < 0) {
		if (errno == EEXIST && unlink(to) == 0)
			continue;
		warn("%s", to);
		(void)close(from_fd);
		return (1);
	}
	while ((nread = read(from_fd, bp, blen)) > 0)
		if (write(to_fd, bp, nread) != nread) {
			warn("%s", to);
			goto err;
		}
	if (nread < 0) {
		warn("%s", from);
err:		if (unlink(to))
			warn("%s: remove", to);
		(void)close(from_fd);
		(void)close(to_fd);
		return (1);
	}
	(void)close(from_fd);

	oldmode = sbp->st_mode & ALLPERMS;
	if (fchown(to_fd, sbp->st_uid, sbp->st_gid)) {
		warn("%s: set owner/group (was: %u/%u)", to, sbp->st_uid,
		    sbp->st_gid);
		if (oldmode & (S_ISUID | S_ISGID)) {
			warnx(
"%s: owner/group changed; clearing suid/sgid (mode was 0%03o)",
			    to, oldmode);
			sbp->st_mode &= ~(S_ISUID | S_ISGID);
		}
	}
	if (fchmod(to_fd, sbp->st_mode))
		warn("%s: set mode (was: 0%03o)", to, oldmode);

	tval[0].tv_sec = sbp->st_atime;
	tval[1].tv_sec = sbp->st_mtime;
	tval[0].tv_usec = tval[1].tv_usec = 0;
	if (utimes(to, tval))
		warn("%s: set times", to);

	if (close(to_fd)) {
		warn("%s", to);
		return (1);
	}

	if (unlink(from)) {
		warn("%s: remove", from);
		return (1);
	}
	return (0);
}

int
copy(from, to)
	char *from, *to;
{
	int pid, status;

	if ((pid = vfork()) == 0) {
		execl(_PATH_CP, "mv", "-PRp", from, to, NULL);
		warn("%s", _PATH_CP);
		_exit(1);
	}
	if (waitpid(pid, &status, 0) == -1) {
		warn("%s: waitpid", _PATH_CP);
		return (1);
	}
	if (!WIFEXITED(status)) {
		warn("%s: did not terminate normally", _PATH_CP);
		return (1);
	}
	if (WEXITSTATUS(status)) {
		warn("%s: terminated with %d (non-zero) status",
		    _PATH_CP, WEXITSTATUS(status));
		return (1);
	}
	if (!(pid = vfork())) {
		execl(_PATH_RM, "mv", "-rf", from, NULL);
		warn("%s", _PATH_RM);
		_exit(1);
	}
	if (waitpid(pid, &status, 0) == -1) {
		warn("%s: waitpid", _PATH_RM);
		return (1);
	}
	if (!WIFEXITED(status)) {
		warn("%s: did not terminate normally", _PATH_RM);
		return (1);
	}
	if (WEXITSTATUS(status)) {
		warn("%s: terminated with %d (non-zero) status",
		    _PATH_RM, WEXITSTATUS(status));
		return (1);
	}
	return (0);
}

void
usage()
{
	(void)fprintf(stderr, "%s\n%s\n",
		      "usage: mv [-f | -i] source target",
		      "       mv [-f | -i] source ... directory");
	exit(1);
}
