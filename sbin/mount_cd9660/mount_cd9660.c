/*
 * Copyright (c) 1992, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
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
 *
 *      @(#)mount_cd9660.c	8.4 (Berkeley) 3/27/94
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1992, 1993, 1994\n\
        The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mount_cd9660.c	8.4 (Berkeley) 3/27/94";
#endif /* not lint */

#include <sys/param.h>
#define CD9660
#include <sys/mount.h>

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mntopts.h"

struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_UPDATE,
	{ NULL }
};

void	usage __P((void));

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct iso_args args;
	int ch, mntflags, opts;
	char *dev, *dir, *options;

	options = NULL;
	mntflags = opts = 0;
	while ((ch = getopt(argc, argv, "ego:r")) != EOF)
		switch (ch) {
		case 'e':
			opts |= ISOFSMNT_EXTATT;
			break;
		case 'g':
			opts |= ISOFSMNT_GENS;
			break;
		case 'o':
			getmntopts(options, mopts, &mntflags);
			break;
		case 'r':
			opts |= ISOFSMNT_NORRIP;
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
	args.fspec = dev;
	args.export.ex_root = DEFAULT_ROOTUID;

	if (mntflags & MNT_RDONLY)
		args.export.ex_flags = MNT_EXRDONLY;
	else
		args.export.ex_flags = 0;
	args.flags = opts;

	if (mount(MOUNT_CD9660, dir, mntflags, &args) < 0)
		err(1, NULL);
	exit(0);
}

void
usage()
{
	(void)fprintf(stderr,
		"usage: mount_cd9660 [-egrt] [-o options] special node\n");
	exit(1);
}
