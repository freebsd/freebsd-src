/*-
 * Copyright (c) 1993, 1994
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
"@(#) Copyright (c) 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mount_ufs.c	8.4 (Berkeley) 4/26/95";
#else
static const char rcsid[] =
	"$Id: mount_ufs.c,v 1.10 1997/08/25 21:14:22 bde Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ufs/ufs/ufsmount.h>

#include "extern.h"
#include "mntopts.h"

static void ufs_usage __P((void));

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_ASYNC,
	MOPT_FORCE,
	MOPT_SYNC,
	MOPT_UPDATE,
	{ NULL }
};

int
mount_ufs(argc, argv)
	int argc;
	char * const argv[];
{
	extern int optreset;
	struct ufs_args args;
	int ch, mntflags;
	char *fs_name;
	struct vfsconf vfc;
	int error = 0;
#ifdef ROOTSLICE_HUNT
	int slice, part, result;
	char devbuf[MAXPATHLEN], devpfx[MAXPATHLEN];
#endif

	mntflags = 0;
	optind = optreset = 1;		/* Reset for parse of new argv. */
	while ((ch = getopt(argc, argv, "o:")) != -1)
		switch (ch) {
		case 'o':
			getmntopts(optarg, mopts, &mntflags, 0);
			break;
		case '?':
		default:
			ufs_usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		ufs_usage();

        args.fspec = argv[0];		/* The name of the device file. */
	fs_name = argv[1];		/* The mount point. */

#define DEFAULT_ROOTUID	-2
	args.export.ex_root = DEFAULT_ROOTUID;
	if (mntflags & MNT_RDONLY)
		args.export.ex_flags = MNT_EXRDONLY;
	else
		args.export.ex_flags = 0;

	error = getvfsbyname("ufs", &vfc);
	if (error && vfsisloadable("ufs")) {
		if (vfsload("ufs")) {
			warn("vfsload(ufs)");
			return (1);
		}
		endvfsent(); /* flush old table */
		error = getvfsbyname("ufs", &vfc);
	}
	if (error) {
		warnx("ufs filesystem is not available");
		return (1);
	}
#ifdef ROOTSLICE_HUNT
	result = -1;
	/*
	 * If we are mounting root, and we have a mount of something that
	 * might be the compatability slice, try mounting other slices
	 * first.  If the kernel has done the right thing and mounted
	 * the slice because the disk is really sliced, this will find
	 * the real root filesystem.  If not, we'll try what was supplied.
	 */
	if (!strcmp(fs_name, "/") &&
	    (sscanf(args.fspec, "%[^0-9]%d%c", devpfx, &unit, &part) == 3) &&
	    (part >= 'a') &&
	    (part <= 'h')) {
		for (slice = 1; (slice < 32) && (result < 0); slice++) {
			sprintf(devbuf, "%s%ds%d%c",
				devpfx, unit, slice, part);
			args.fspec = devbuf;
			result = mount(vfc.vfc_name, fs_name, mntflags, &args);
		}
		args.fspec = argv[0];
	}
	if (result == 0)
		warnx("*** update /etc/fstab entry for %s to use %s ***",
			fs_name, devbuf);
	/* Try the mount as originally requested */
	if ((result  < 0) &&
	    (mount(vfc.vfc_name, fs_name, mntflags, &args < 0)) {
#else
	if (mount(vfc.vfc_name, fs_name, mntflags, &args) < 0) {
#endif
		(void)fprintf(stderr, "%s on %s: ", args.fspec, fs_name);
		switch (errno) {
		case EMFILE:
			(void)fprintf(stderr, "mount table full.\n");
			break;
		case EINVAL:
			if (mntflags & MNT_UPDATE)
				(void)fprintf(stderr,
		    "Specified device does not match mounted device.\n");
			else
				(void)fprintf(stderr,
				    "Incorrect super block.\n");
			break;
		default:
			(void)fprintf(stderr, "%s\n", strerror(errno));
			break;
		}
		return (1);
	}
	return (0);
}

static void
ufs_usage()
{
	(void)fprintf(stderr, "usage: mount_ufs [-o options] special node\n");
	exit(1);
}
