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
static char copyright[] =
"@(#) Copyright (c) 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mount_ufs.c	8.2 (Berkeley) 3/27/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mntopts.h"

void	ufs_usage __P((void));

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_ASYNC,
	MOPT_SYNC,
	MOPT_FORCE,
	MOPT_UPDATE,
	MOPT_UQUOTA,
	MOPT_GQUOTA,
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
	struct vfsconf *vfc;

	mntflags = 0;
	optind = optreset = 1;		/* Reset for parse of new argv. */
	while ((ch = getopt(argc, argv, "o:")) !=  -1)
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

	setvfsent(0);
	if(!(vfc = getvfsbyname("ufs"))) {
		if(vfsisloadable("ufs")) {
			if(vfsload("ufs")) {
				warn("vfsload(\"ufs\")");
				return 1;
			}
			endvfsent(); /* flush old table */
			vfc = getvfsbyname("ufs");
		} else {
			/*warnx("ufs: filesystem not found");*/
		}
	}

	if (mount(vfc ? vfc->vfc_index : MOUNT_UFS, fs_name, mntflags, &args) < 0) {
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

void
ufs_usage()
{
	(void)fprintf(stderr, "usage: mount_ufs [-o options] special node\n");
	exit(1);
}
