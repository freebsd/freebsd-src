/*-
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

#if 0
#ifndef lint
static char const copyright[] =
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mv.c	8.2 (Berkeley) 4/2/94";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/acl.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

int fflg, iflg, nflg, vflg;

int	copy(char *, char *);
int	do_move(char *, char *);
int	fastcopy(char *, char *, struct stat *);
void	usage(void);

int
main(int argc, char *argv[])
{
	size_t baselen, len;
	int rval;
	char *p, *endp;
	struct stat sb;
	int ch;
	char path[PATH_MAX];

	while ((ch = getopt(argc, argv, "finv")) != -1)
		switch (ch) {
		case 'i':
			iflg = 1;
			fflg = nflg = 0;
			break;
		case 'f':
			fflg = 1;
			iflg = nflg = 0;
			break;
		case 'n':
			nflg = 1;
			fflg = iflg = 0;
			break;
		case 'v':
			vflg = 1;
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

		if ((baselen + (len = strlen(p))) >= PATH_MAX) {
			warnx("%s: destination pathname too long", *argv);
			rval = 1;
		} else {
			memmove(endp, p, (size_t)len + 1);
			if (do_move(*argv, path))
				rval = 1;
		}
	}
	exit(rval);
}

int
do_move(char *from, char *to)
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
		if (nflg) {
			if (vflg)
				printf("%s not overwritten\n", to);
			return (0);
		} else if (iflg) {
			(void)fprintf(stderr, "overwrite %s? %s", to, YESNO);
			ask = 1;
		} else if (access(to, W_OK) && !stat(to, &sb)) {
			strmode(sb.st_mode, modep);
			(void)fprintf(stderr, "override %s%s%s/%s for %s? %s",
			    modep + 1, modep[9] == ' ' ? "" : " ",
			    user_from_uid((unsigned long)sb.st_uid, 0),
			    group_from_gid((unsigned long)sb.st_gid, 0), to, YESNO);
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
	if (!rename(from, to)) {
		if (vflg)
			printf("%s -> %s\n", from, to);
		return (0);
	}

	if (errno == EXDEV) {
		struct statfs sfs;
		char path[PATH_MAX];

		/*
		 * If the source is a symbolic link and is on another
		 * filesystem, it can be recreated at the destination.
		 */
		if (lstat(from, &sb) == -1) {
			warn("%s", from);
			return (1);
		}
		if (!S_ISLNK(sb.st_mode)) {
			/* Can't mv(1) a mount point. */
			if (realpath(from, path) == NULL) {
				warnx("cannot resolve %s: %s", from, path);
				return (1);
			}
			if (!statfs(path, &sfs) &&
			    !strcmp(path, sfs.f_mntonname)) {
				warnx("cannot rename a mount point");
				return (1);
			}
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
	if (lstat(from, &sb)) {
		warn("%s", from);
		return (1);
	}
	return (S_ISREG(sb.st_mode) ?
	    fastcopy(from, to, &sb) : copy(from, to));
}

int
fastcopy(char *from, char *to, struct stat *sbp)
{
	struct timeval tval[2];
	static u_int blen;
	static char *bp;
	mode_t oldmode;
	int nread, from_fd, to_fd;
	acl_t acl;

	if ((from_fd = open(from, O_RDONLY, 0)) < 0) {
		warn("%s", from);
		return (1);
	}
	if (blen < sbp->st_blksize) {
		if (bp != NULL)
			free(bp);
		if ((bp = malloc((size_t)sbp->st_blksize)) == NULL) {
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
	while ((nread = read(from_fd, bp, (size_t)blen)) > 0)
		if (write(to_fd, bp, (size_t)nread) != nread) {
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

	oldmode = sbp->st_mode & ALLPERMS;
	if (fchown(to_fd, sbp->st_uid, sbp->st_gid)) {
		warn("%s: set owner/group (was: %lu/%lu)", to,
		    (u_long)sbp->st_uid, (u_long)sbp->st_gid);
		if (oldmode & (S_ISUID | S_ISGID)) {
			warnx(
"%s: owner/group changed; clearing suid/sgid (mode was 0%03o)",
			    to, oldmode);
			sbp->st_mode &= ~(S_ISUID | S_ISGID);
		}
	}
	/*
	 * POSIX 1003.2c states that if _POSIX_ACL_EXTENDED is in effect
	 * for dest_file, then it's ACLs shall reflect the ACLs of the
	 * source_file.
	 */
	if (fpathconf(to_fd, _PC_ACL_EXTENDED) == 1 &&
	    fpathconf(from_fd, _PC_ACL_EXTENDED) == 1) {
		acl = acl_get_fd(from_fd);
		if (acl == NULL)
			warn("failed to get acl entries while setting %s",
			    from);
		else if (acl_set_fd(to_fd, acl) < 0)
			warn("failed to set acl entries for %s", to);
	}
	(void)close(from_fd);
	if (fchmod(to_fd, sbp->st_mode))
		warn("%s: set mode (was: 0%03o)", to, oldmode);
	/*
	 * XXX
	 * NFS doesn't support chflags; ignore errors unless there's reason
	 * to believe we're losing bits.  (Note, this still won't be right
	 * if the server supports flags and we were trying to *remove* flags
	 * on a file that we copied, i.e., that we didn't create.)
	 */
	errno = 0;
	if (fchflags(to_fd, (u_long)sbp->st_flags))
		if (errno != EOPNOTSUPP || sbp->st_flags != 0)
			warn("%s: set flags (was: 0%07o)", to, sbp->st_flags);

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
	if (vflg)
		printf("%s -> %s\n", from, to);
	return (0);
}

int
copy(char *from, char *to)
{
	struct stat sb;
	enum clean {CLEAN_SOURCE, CLEAN_DEST, CLEAN_ODEST, CLEAN_MAX};
	char *cleanup[CLEAN_MAX];
	int pid, status;
	volatile int i, rval;

	rval = 0;
	for (i = 0; i < CLEAN_MAX; i++)
		cleanup[i] = NULL;
	/*
	 * If "to" exists and is a directory, get it out of the way.
	 * When the copy succeeds, delete it.
	 */
	if (stat(to, &sb) == 0 && S_ISDIR(sb.st_mode)) {
		if (asprintf(&cleanup[CLEAN_ODEST], "%s.XXXXXX", to) == -1) {
			warnx("asprintf failed");
			return (1);

		}
		if (rename(to, cleanup[CLEAN_ODEST]) < 0) {
			warn("rename of existing target from %s to %s failed",
			    to, cleanup[CLEAN_ODEST]);
			free(cleanup[CLEAN_ODEST]);
			return (1);
		}
	}
	/* Copy source to destination. */
	cleanup[CLEAN_DEST] = to;
	if ((pid = fork()) == 0) {
		execl(_PATH_CP, "mv", vflg ? "-PRpv" : "-PRp", "--", from, to,
		    (char *)NULL);
		warn("%s", _PATH_CP);
		_exit(1);
	}
	if (waitpid(pid, &status, 0) == -1) {
		warn("%s: waitpid", _PATH_CP);
		rval = 1;
		goto done;
	}
	if (!WIFEXITED(status)) {
		warnx("%s: did not terminate normally", _PATH_CP);
		rval = 1;
		goto done;
	}
	if (WEXITSTATUS(status)) {
		warnx("%s: terminated with %d (non-zero) status",
		    _PATH_CP, WEXITSTATUS(status));
		rval = 1;
		goto done;
	}
	/*
	 * The copy succeeded.  From now on the destination is where users
	 * will find their files.
	 */
	cleanup[CLEAN_DEST] = NULL;
	cleanup[CLEAN_SOURCE] = from;
done:
	/* Clean what needs to be cleaned. */
	for (i = 0; i < CLEAN_MAX; i++) {
		if (cleanup[i] == NULL)
			continue;
		if (!(pid = vfork())) {
			execl(_PATH_RM, "mv", "-rf", "--", cleanup[i],
			    (char *)NULL);
			_exit(EX_OSERR);
		}
		if (waitpid(pid, &status, 0) == -1) {
			warn("%s %s: waitpid", _PATH_RM, cleanup[i]);
			rval = 1;
			continue;
		}
		if (!WIFEXITED(status)) {
			warnx("%s %s: did not terminate normally",
			    _PATH_RM, cleanup[i]);
			rval = 1;
			continue;
		}
		switch (WEXITSTATUS(status)) {
		case 0:
			break;
		case EX_OSERR:
			warnx("Failed to exec %s %s", _PATH_RM, cleanup[i]);
			rval = 1;
			continue;
		default:
			warnx("%s %s: terminated with %d (non-zero) status",
			    _PATH_RM, cleanup[i], WEXITSTATUS(status));
			rval = 1;
			continue;
		}
		/*
		 * If the copy failed, and we just deleted the copy's trash,
		 * try to salvage the original destination,
		 */
		if (i == CLEAN_DEST && cleanup[CLEAN_ODEST]) {
			if (rename(cleanup[CLEAN_ODEST], to) < 0) {
				warn("rename back renamed existing target from %s to %s failed",
				    cleanup[CLEAN_ODEST], to);
				rval = 1;
			}
			free(cleanup[CLEAN_ODEST]);
			cleanup[CLEAN_ODEST] = NULL;
		}
	}
	if (cleanup[CLEAN_ODEST])
		free(cleanup[CLEAN_ODEST]);
	return (rval);
}

void
usage(void)
{

	(void)fprintf(stderr, "%s\n%s\n",
		      "usage: mv [-f | -i | -n] [-v] source target",
		      "       mv [-f | -i | -n] [-v] source ... directory");
	exit(EX_USAGE);
}
