/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2022 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2011-2015 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 * by Venkatesh Srinivas <vsrinivas@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/mount.h>
#include <fs/hammer2/hammer2_mount.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <sysexits.h>
#include <mntopts.h>

static void usage(const char *ctl, ...);

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_UPDATE,
	MOPT_END
};

/*
 * Usage: mount_hammer2 [volume] [mtpt]
 */
int
main(int ac, char *av[])
{
	struct iovec *iov;
	char mntpath[MAXPATHLEN];
	char fstype[] = "hammer2";
	char *mountpt, *devpath = NULL;
	int ch, iovlen = 0, mount_flags = 0;
	int hflags = HMNT2_LOCAL; /* force local, not optional */

	while ((ch = getopt(ac, av, "o:")) != -1) {
		switch(ch) {
		case 'o':
			getmntopts(optarg, mopts, &mount_flags, &hflags);
			break;
		default:
			usage("unknown option: -%c", ch);
			/* not reached */
		}
	}
	ac -= optind;
	av += optind;

	/*
	 * New mount
	 */
	if (ac != 2) {
		usage("missing parameter(s) (special[@label] node)");
		/* not reached */
	}

	devpath = strdup(av[0]);
	mountpt = av[1];

	if (devpath[0] == 0) {
		fprintf(stderr, "mount_hammer2: empty device path\n");
		exit(1);
	}

	/*
	 * Automatically add @DATA if no label specified.
	 */
	if (strchr(devpath, '@') == NULL)
		asprintf(&devpath, "%s@DATA", devpath);

	/*
	 * Try to mount it, prefix if necessary.
	 */
	if (!strchr(devpath, ':') && devpath[0] != '/' && devpath[0] != '@') {
		char *p2;
		asprintf(&p2, "/dev/%s", devpath);
		free(devpath);
		devpath = p2;
	}

	/*
	 * Resolve the mountpoint with realpath(3) and remove unnecessary
	 * slashes from the devicename if there are any.
	 */
	if (checkpath(mountpt, mntpath) != 0)
		err(EX_USAGE, "%s", mntpath);
	if (devpath)
		rmslashes(devpath, devpath);

	mount_flags |= MNT_RDONLY; /* currently write unsupported */
	build_iovec(&iov, &iovlen, "fstype", fstype, (size_t)-1);
	build_iovec(&iov, &iovlen, "fspath", mntpath, (size_t)-1);
	build_iovec(&iov, &iovlen, "from", devpath, (size_t)-1);
	build_iovec(&iov, &iovlen, "hflags", &hflags, sizeof(hflags));
	if (nmount(iov, iovlen, mount_flags) < 0)
		err(1, "%s", devpath);

	free(devpath);

	return (0);
}

static
void
usage(const char *ctl, ...)
{
	va_list va;

	va_start(va, ctl);
	fprintf(stderr, "mount_hammer2: ");
	vfprintf(stderr, ctl, va);
	va_end(va);
	fprintf(stderr, "\n");
	fprintf(stderr, " mount_hammer2 [-o options] special[@label] node\n");
	fprintf(stderr, " mount_hammer2 [-o options] @label node\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "options:\n"
			" <standard_mount_options>\n"
	);
	exit(1);
}
