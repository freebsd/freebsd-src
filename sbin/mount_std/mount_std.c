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
static const char rcsid[] =
  "$FreeBSD: src/sbin/mount_std/mount_std.c,v 1.10 1999/10/09 11:54:12 phk Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "mntopts.h"

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	{ NULL }
};

static void	usage __P((void)) __dead2;
static const char *fsname;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, mntflags;
	char mntpath[MAXPATHLEN];
	struct vfsconf vfc;
	int error;

	/*
	 * XXX
	 * mount(8) calls the mount programs with an argv[0] which is
	 * /just/ the filesystem name.  So, if there is no underscore
	 * in argv[0], we assume that we are being called from mount(8)
	 * and that argv[0] is thus the name of the filesystem type.
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

	error = getvfsbyname(fsname, &vfc);
	if (error && vfsisloadable(fsname)) {
		if(vfsload(fsname))
			err(EX_OSERR, "vfsload(%s)", fsname);
		endvfsent();
		error = getvfsbyname(fsname, &vfc);
	}
	if (error)
		errx(EX_OSERR, "%s filesystem not available", fsname);

	/* resolve the mountpoint with realpath(3) */
	(void)checkpath(argv[1], mntpath);

	if (mount(vfc.vfc_name, mntpath, mntflags, NULL))
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
