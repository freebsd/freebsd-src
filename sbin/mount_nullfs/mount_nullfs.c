/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mount_null.c	8.6 (Berkeley) 4/26/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "mntopts.h"

static void	usage(void) __dead2;

static int
stat_realpath(const char *path, char *resolved, struct stat *sbp)
{
	if (realpath(path, resolved) == NULL || stat(resolved, sbp) != 0)
		return (1);
	return (0);
}

int
main(int argc, char *argv[])
{
	struct iovec *iov;
	char *p, *val;
	char mountpoint[MAXPATHLEN];
	char target[MAXPATHLEN];
	char errmsg[255];
	int ch, iovlen;
	char nullfs[] = "nullfs";
	struct stat target_stat;
	struct stat mountpoint_stat;

	iov = NULL;
	iovlen = 0;
	errmsg[0] = '\0';
	while ((ch = getopt(argc, argv, "o:")) != -1)
		switch(ch) {
		case 'o':
			val = strdup("");
			p = strchr(optarg, '=');
			if (p != NULL) {
				free(val);
				*p = '\0';
				val = p + 1;
			}
			build_iovec(&iov, &iovlen, optarg, val, (size_t)-1);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	/* resolve target and mountpoint with realpath(3) */
	if (stat_realpath(argv[0], target, &target_stat) != 0)
		err(EX_USAGE, "%s", target);
	if (stat_realpath(argv[1], mountpoint, &mountpoint_stat) != 0)
		err(EX_USAGE, "%s", mountpoint);
	if (!S_ISDIR(target_stat.st_mode) && !S_ISREG(target_stat.st_mode))
		errx(EX_USAGE, "%s: must be either a file or directory",
		    target);
	if ((target_stat.st_mode & S_IFMT) !=
	    (mountpoint_stat.st_mode & S_IFMT))
		errx(EX_USAGE,
		    "%s: must be same type as %s (file or directory)",
		    mountpoint, target);

	build_iovec(&iov, &iovlen, "fstype", nullfs, (size_t)-1);
	build_iovec(&iov, &iovlen, "fspath", mountpoint, (size_t)-1);
	build_iovec(&iov, &iovlen, "target", target, (size_t)-1);
	build_iovec(&iov, &iovlen, "errmsg", errmsg, sizeof(errmsg));
	if (nmount(iov, iovlen, 0) < 0) {
		if (errmsg[0] != 0)
			err(1, "%s: %s", mountpoint, errmsg);
		else
			err(1, "%s", mountpoint);
	}
	exit(0);
}

static void
usage(void)
{
	(void)fprintf(stderr,
		"usage: mount_nullfs [-o options] target mount-point\n");
	exit(1);
}
