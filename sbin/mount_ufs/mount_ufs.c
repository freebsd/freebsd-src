/*
 * Copyright (c) 1992, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2002 Scott Long
 *
 * This code is derived from software contributed to Berkeley
 * by Pace Willisson (pace@blitz.com).  The Rock Ridge Extension
 * Support code is derived from software contributed to Berkeley
 * by Atsushi Murai (amurai@spec.co.jp).
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
 *
 * $FreeBSD$
 */

#include <sys/cdio.h>
#include <sys/file.h>
#include <sys/iconv.h>
#include <sys/param.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/uio.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "mntopts.h"

struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_ASYNC,
	MOPT_FORCE,
	MOPT_SYNC,
	MOPT_UPDATE,
	MOPT_SNAPSHOT,
	{ NULL }
};

void	usage(void);

int
main(int argc, char **argv)
{
	struct iovec *iov;
	int iovlen;
	struct export_args export;
	int ch, i, mntflags, opts, ufs_flags;
	char *dev, *dir, mntpath[MAXPATHLEN];
	char *cs_disk, *cs_local;
	int verbose;

	i = mntflags = opts = ufs_flags = verbose = 0;
	cs_disk = cs_local = NULL;
	while ((ch = getopt(argc, argv, "o:v")) != -1)
		switch (ch) {
		case 'o':
			getmntopts(optarg, mopts, &mntflags, &opts);
			break;
		case 'v':
			verbose++;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	dev = argv[0];
	dir = argv[1];

#define DEFAULT_ROOTUID	-2
	export.ex_root = DEFAULT_ROOTUID;
	if (mntflags & MNT_RDONLY)
		export.ex_flags = MNT_EXRDONLY;
	else
		export.ex_flags = 0;

	/*
	 * Resolve the mountpoint with realpath(3) and remove unnecessary
	 * slashes from the devicename if there are any.
	 */
	(void)checkpath(dir, mntpath);
	(void)rmslashes(dev, dev);

	iov = NULL;
	iovlen = 0;
	build_iovec(&iov, &iovlen, "fstype", "ufs", -1);
	build_iovec(&iov, &iovlen, "fspath", mntpath, -1);
	build_iovec(&iov, &iovlen, "from", dev, -1);
	build_iovec(&iov, &iovlen, "flags", &ufs_flags, sizeof ufs_flags);
	build_iovec(&iov, &iovlen, "export", &export, sizeof export);
	if (nmount(iov, iovlen, mntflags) < 0)
		err(1, "%s", dev);
	exit(0);
}

void
usage(void)
{
	(void)fprintf(stderr,
		"usage: mount_ufs [-v] [-o options] special node\n");
	exit(EX_USAGE);
}
