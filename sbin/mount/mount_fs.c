/*
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
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
static const char copyright[] =
"@(#) Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mount_fs.c	8.6 (Berkeley) 4/26/95";
#endif
static const char rcsid[] =
	"$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>

#include <err.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"
#include "mntopts.h"

struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_END
};

static void
usage(void)
{
	(void)fprintf(stderr,
		"usage: mount [-t fstype] [-o options] target_fs mount_point\n");
	exit(1);
}

int
mount_fs(const char *vfstype, int argc, char *argv[])
{
	struct iovec *iov;
	int iovlen;
	int mntflags = 0;
	int ch;
	char *dev, *dir, mntpath[MAXPATHLEN];
	char fstype[32];
	char *p, *val;
	int ret;

	strncpy(fstype, vfstype, sizeof(fstype));

	getmnt_silent = 1;
	iov = NULL;
	iovlen = 0;

	optind = optreset = 1;		/* Reset for parse of new argv. */
	while ((ch = getopt(argc, argv, "o:")) != -1) {
		switch(ch) {
		case 'o':
			getmntopts(optarg, mopts, &mntflags, 0);
			p = strchr(optarg, '=');
			val = NULL;
			if (p != NULL) {
				*p = '\0';
				val = p + 1;
			}
			build_iovec(&iov, &iovlen, optarg, val, (size_t)-1);
			break;
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc != 2)
		usage();

	dev = argv[0];
	dir = argv[1];

	(void)checkpath(dir, mntpath);
	(void)rmslashes(dev, dev);

	build_iovec(&iov, &iovlen, "fstype", fstype, (size_t)-1);
	build_iovec(&iov, &iovlen, "fspath", mntpath, (size_t)-1);
	build_iovec(&iov, &iovlen, "from", dev, (size_t)-1);
	
	ret = nmount(iov, iovlen, mntflags);
	if (ret < 0)
		err(1, "%s", dev);

	return (ret);
}
