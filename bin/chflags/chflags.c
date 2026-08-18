/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1992, 1993, 1994
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

#include <sys/types.h>
#include <sys/capsicum.h>
#include <sys/stat.h>

#include <capsicum_helpers.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t siginfo;

#define	OPT_DEREF_UNSAFE	(CHAR_MAX + 1)

static void usage(void) __dead2;

static void
siginfo_handler(int sig __unused)
{

	siginfo = 1;
}

/*
 * A path needs its own pre-opened directory descriptor unless it is a
 * single path component that can be resolved directly relative to the
 * base directory descriptor.  Anything containing a '/' (an absolute
 * path, or a relative path with a directory component) has its parent
 * directory opened separately, so that in capability mode the final
 * component is always reached relative to its immediate parent.
 */
static bool
needs_own_fd(const char *path)
{

	return (strchr(path, '/') != NULL);
}

/*
 * Open a directory descriptor for the parent of "path", and return in
 * "*base" a pointer to the final path component (relative to that
 * descriptor).  "*base" points into the storage of "path".
 */
static int
open_base(char *path, char **base)
{
	char *dir, *bn, *pathcopy;
	int fd;

	/*
	 * dirname() and basename() may modify their argument and may
	 * return a pointer to internal storage, so operate on copies and
	 * duplicate basename()'s result for the caller.
	 */
	if ((pathcopy = strdup(path)) == NULL)
		err(1, "strdup");
	dir = dirname(pathcopy);
	fd = open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	free(pathcopy);

	if ((pathcopy = strdup(path)) == NULL)
		err(1, "strdup");
	bn = basename(pathcopy);
	if ((*base = strdup(bn)) == NULL)
		err(1, "strdup");
	free(pathcopy);

	return (fd);
}

static int
chflags_fts(int dirfd, char **paths, int fts_options, u_long set, u_long clear,
    int oct, int Rflag, int fflag, int vflag)
{
	FTS *ftsp;
	FTSENT *p;
	u_long newflags;
	int e, rval;

	if ((ftsp = fts_openat(dirfd, paths, fts_options, NULL)) == NULL)
		err(1, NULL);

	for (rval = 0; errno = 0, (p = fts_read(ftsp)) != NULL;) {
		int atflag;

		if ((fts_options & FTS_LOGICAL) ||
		    ((fts_options & FTS_COMFOLLOW) &&
		    p->fts_level == FTS_ROOTLEVEL))
			atflag = 0;
		else
			atflag = AT_SYMLINK_NOFOLLOW;

		switch (p->fts_info) {
		case FTS_D:		/* Change it at FTS_DP if we're recursive. */
			if (!Rflag)
				fts_set(ftsp, p, FTS_SKIP);
			continue;
		case FTS_DNR:			/* Warn, chflags. */
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			break;
		case FTS_ERR:			/* Warn, continue. */
		case FTS_NS:
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			continue;
		default:
			break;
		}
		if (oct)
			newflags = set;
		else
			newflags = (p->fts_statp->st_flags | set) & clear;
		if (newflags == p->fts_statp->st_flags)
			continue;
		if (chflagsat(p->fts_parent->fts_dirfd, p->fts_name, newflags,
		    atflag) == -1) {
			e = errno;
			if (!fflag) {
				warnc(e, "%s", p->fts_path);
				rval = 1;
			}
			if (siginfo) {
				(void)printf("%s: %s\n", p->fts_path,
				    strerror(e));
				siginfo = 0;
			}
		} else if (vflag || siginfo) {
			(void)printf("%s", p->fts_path);
			if (vflag > 1 || siginfo)
				(void)printf(": 0%lo -> 0%lo",
				    (u_long)p->fts_statp->st_flags,
				    newflags);
			(void)printf("\n");
			siginfo = 0;
		}
	}
	if (errno)
		err(1, "fts_read");
	(void)fts_close(ftsp);
	return (rval);
}

int
main(int argc, char *argv[])
{
	static const struct option longopts[] = {
		{ "dereference-links-unsafely", no_argument, NULL,
		    OPT_DEREF_UNSAFE },
		{ NULL, 0, NULL, 0 }
	};
	u_long clear, set;
	long val;
	int Hflag, Lflag, Rflag, fflag, hflag, vflag, xflag, unsafe;
	int ch, fts_options, oct, rval;
	int cwd_fd, i, nrel, nown;
	int *ownfd;
	char **ownbase;
	char *flags, *ep;
	char **relpaths, *twopath[2];

	Hflag = Lflag = Rflag = fflag = hflag = vflag = xflag = unsafe = 0;
	while ((ch = getopt_long(argc, argv, "HLPRfhvx", longopts,
	    NULL)) != -1)
		switch (ch) {
		case 'H':
			Hflag = 1;
			Lflag = 0;
			break;
		case 'L':
			Lflag = 1;
			Hflag = 0;
			break;
		case 'P':
			Hflag = Lflag = 0;
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'f':
			fflag = 1;
			break;
		case 'h':
			hflag = 1;
			break;
		case 'v':
			vflag++;
			break;
		case 'x':
			xflag = 1;
			break;
		case OPT_DEREF_UNSAFE:
			unsafe = 1;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (argc < 2)
		usage();

	(void)signal(SIGINFO, siginfo_handler);

	if (Rflag) {
		if (hflag)
			errx(1, "the -R and -h options may not be "
			    "specified together.");
		if (Lflag) {
			fts_options = FTS_LOGICAL;
		} else {
			fts_options = FTS_PHYSICAL;

			if (Hflag) {
				fts_options |= FTS_COMFOLLOW;
			}
		}
	} else if (hflag) {
		fts_options = FTS_PHYSICAL;
	} else {
		fts_options = FTS_LOGICAL;
	}
	if (xflag)
		fts_options |= FTS_XDEV;

	flags = *argv;
	if (*flags >= '0' && *flags <= '7') {
		errno = 0;
		val = strtol(flags, &ep, 8);
		if (val < 0)
			errno = ERANGE;
		if (errno)
			err(1, "invalid flags: %s", flags);
		if (*ep)
			errx(1, "invalid flags: %s", flags);
		set = val;
		oct = 1;
	} else {
		if (strtofflags(&flags, &set, &clear))
			errx(1, "invalid flag: %s", flags);
		clear = ~clear;
		oct = 0;
	}

	argv++;
	argc--;

	/*
	 * Pre-open a directory descriptor for every path argument, so the
	 * traversal runs through fd-relative operations.  Plain relative
	 * arguments share a descriptor for the current directory; absolute
	 * paths and paths containing ".." cannot be resolved relative to
	 * another descriptor in capability mode, so each gets its own
	 * parent descriptor.
	 */
	if ((cwd_fd = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC)) < 0)
		err(1, ".");

	relpaths = calloc(argc + 1, sizeof(*relpaths));
	ownfd = calloc(argc, sizeof(*ownfd));
	ownbase = calloc(argc, sizeof(*ownbase));
	if (relpaths == NULL || ownfd == NULL || ownbase == NULL)
		err(1, "calloc");
	nrel = 0;
	nown = 0;
	rval = 0;

	for (i = 0; i < argc; i++) {
		if (needs_own_fd(argv[i])) {
			int fd = open_base(argv[i], &ownbase[nown]);
			if (fd < 0) {
				warn("%s", argv[i]);
				rval = 1;
				continue;
			}
			ownfd[nown] = fd;
			nown++;
		} else {
			relpaths[nrel++] = argv[i];
		}
	}
	relpaths[nrel] = NULL;

	if (caph_limit_stdio() < 0)
		err(1, "caph_limit_stdio");
	/*
	 * With --dereference-links-unsafely the traversal may follow a
	 * symlink to a file outside the hierarchy named on the command
	 * line, which capability mode would block, so skip cap_enter() in
	 * that case.
	 */
	if (!unsafe && caph_enter() < 0)
		err(1, "cap_enter");

	/* Process all plain relative paths together under cwd_fd. */
	if (nrel > 0)
		rval |= chflags_fts(cwd_fd, relpaths, fts_options, set, clear,
		    oct, Rflag, fflag, vflag);

	/* Process each absolute / ".."-containing path under its own fd. */
	for (i = 0; i < nown; i++) {
		twopath[0] = ownbase[i];
		twopath[1] = NULL;
		rval |= chflags_fts(ownfd[i], twopath, fts_options, set,
		    clear, oct, Rflag, fflag, vflag);
	}

	for (i = 0; i < nown; i++)
		free(ownbase[i]);
	free(relpaths);
	free(ownfd);
	free(ownbase);

	exit(rval);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: chflags [-fhvx] [-R [-H | -L | -P]] "
	    "[--dereference-links-unsafely] flags file ...\n");
	exit(1);
}
