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
/*
static char sccsid[] = "@(#)mount_lfs.c	8.3 (Berkeley) 3/27/94";
*/
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "mntopts.h"
#include "pathnames.h"

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_UPDATE,
	{ NULL }
};

static __dead void	usage __P((void)) __dead2;
static void	invoke_cleaner __P((char *, int, int));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct ufs_args args;
	int ch, mntflags, noclean;
	char *fs_name, *options;
	struct vfsconf *vfc;
	int short_rds, cleaner_debug;


	options = NULL;
	mntflags = noclean = short_rds = cleaner_debug = 0;
	while ((ch = getopt(argc, argv, "dno:s")) != EOF)
		switch (ch) {
		case 'd':
			cleaner_debug = 1;
			break;
		case 'n':
			noclean = 1;
			break;
		case 'o':
			getmntopts(optarg, mopts, &mntflags, 0);
			break;
		case 's':
			short_rds = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

        args.fspec = argv[0];	/* the name of the device file */
	fs_name = argv[1];	/* the mount point */

#define DEFAULT_ROOTUID	-2
	args.export.ex_root = DEFAULT_ROOTUID;
	if (mntflags & MNT_RDONLY)
		args.export.ex_flags = MNT_EXRDONLY;
	else
		args.export.ex_flags = 0;

	vfc = getvfsbyname("lfs");
	if(!vfc && vfsisloadable("lfs")) {
		if(vfsload("lfs"))
			err(EX_OSERR, "vfsload(lfs)");
		endvfsent();	/* flush cache */
		vfc = getvfsbyname("lfs");
	}
	if (!vfc)
		errx(EX_OSERR, "lfs filesystem is not available");

	if (mount(vfc ? vfc->vfc_index : MOUNT_LFS, fs_name, mntflags, &args))
		err(EX_OSERR, args.fspec);

	if (!noclean)
		invoke_cleaner(fs_name, short_rds, cleaner_debug);
		/* NOTREACHED */

	exit(0);
}

static void
invoke_cleaner(name, short_rds, cleaner_debug)
	char *name;
	int short_rds;
	int cleaner_debug;
{
	char *args[6], **ap = args;

	/* Build the argument list. */
	*ap++ = _PATH_LFS_CLEANERD;
	if (short_rds)
		*ap++ = "-s";
	if (cleaner_debug)
		*ap++ = "-d";
	*ap++ = name;
	*ap = NULL;

	execv(args[0], args);
	err(EX_OSERR, "exec %s", _PATH_LFS_CLEANERD);
}

static void
usage()
{
	(void)fprintf(stderr,
		"usage: mount_lfs [-dns] [-o options] special node\n");
	exit(EX_USAGE);
}
