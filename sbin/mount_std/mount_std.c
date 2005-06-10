/*
 * Copyright (c) 1990, 1992 Jan-Simon Pendry
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/uio.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "mntopts.h"

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_END
};

static char *fsname;
static volatile sig_atomic_t caughtsig;

static void usage(void) __dead2;

static void
catchsig(int s)
{
	caughtsig = 1;
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, mntflags;
	char mntpath[MAXPATHLEN];
	struct iovec iov[4];
	int error;

	/*
	 * XXX
	 * mount(8) calls the mount programs with an argv[0] which is
	 * /just/ the file system name.  So, if there is no underscore
	 * in argv[0], we assume that we are being called from mount(8)
	 * and that argv[0] is thus the name of the file system type.
	 */
	fsname = strrchr(argv[0], '_');
	if (fsname) {
		if (strcmp(fsname, "_std") == 0)
			errx(EX_USAGE, "argv[0] must end in _fsname");
		fsname++;
	} else {
		fsname = argv[0];
	}

	mntflags = 0;
	while ((ch = getopt(argc, argv, "o:")) != -1)
		switch (ch) {
		case 'o':
			getmntopts(optarg, mopts, &mntflags, 0);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	/* resolve the mountpoint with realpath(3) */
	(void)checkpath(argv[1], mntpath);

	iov[0].iov_base = "fstype";
	iov[0].iov_len = sizeof("fstype");
	iov[1].iov_base = fsname;
	iov[1].iov_len = strlen(iov[1].iov_base) + 1;
	iov[2].iov_base = "fspath";
	iov[2].iov_len = sizeof("fspath");
	iov[3].iov_base = mntpath;
	iov[3].iov_len = strlen(mntpath) + 1;

	/*
	 * nmount(2) would kill us with SIGSYS if the kernel doesn't have it.
	 * This design bug is inconvenient.  We must catch the signal and not
	 * just ignore it because of a plain bug: nmount(2) would return
	 * EINVAL instead of the correct ENOSYS if the kernel doesn't have it
	 * and we don't let the signal kill us.  EINVAL is too ambiguous.
	 * This bug in 4.4BSD-Lite1 was fixed in 4.4BSD-Lite2 but is still in
	 * FreeBSD-5.0.
	 */
	signal(SIGSYS, catchsig);
	error = nmount(iov, 4, mntflags);
	signal(SIGSYS, SIG_DFL);

	/*
	 * Try with the old mount syscall in the case
	 * this file system has not been converted yet,
	 * or the user didn't recompile his kernel.
	 */
	if (error && (errno == EOPNOTSUPP || errno == ENOSYS || caughtsig))
		error = mount(fsname, mntpath, mntflags, NULL);

	if (error)
		err(EX_OSERR, NULL);
	exit(0);
}

void
usage()
{
	(void)fprintf(stderr,
		"usage: mount_%s [-o options] what_to_mount mount_point\n",
		      fsname);
	exit(EX_USAGE);
}
