/*
 * Copyright (c) 2003 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Marshall
 * Kirk McKusick and Network Associates Laboratories, the Security
 * Research Division of Network Associates, Inc. under DARPA/SPAWAR
 * contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA CHATS
 * research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <ufs/ufs/ufsmount.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

void usage(void);

int
main(int argc, char **argv)
{
	const char *dir;
	struct ufs_args args;
	struct group *grp;
	struct stat stbuf;
	int fd;

	if (argc != 3)
		usage();

	dir = argv[1];
	args.fspec = argv[2];

	if ((grp = getgrnam("operator")) == NULL)
		errx(1, "Cannot retrieve operator gid");
	if (mount("ffs", dir, MNT_UPDATE | MNT_SNAPSHOT, &args) < 0)
		err(1, "Cannot create %s", args.fspec);
	if ((fd = open(args.fspec, O_RDONLY)) < 0)
		err(1, "Cannot open %s", args.fspec);
	if (fstat(fd, &stbuf) != 0)
		err(1, "Cannot stat %s", args.fspec);
	if ((stbuf.st_flags & SF_SNAPSHOT) == 0)
		errx(1, "File %s is not a snapshot", args.fspec);
	if (fchown(fd, -1, grp->gr_gid) != 0)
		err(1, "Cannot chown %s", args.fspec);
	if (fchmod(fd, S_IRUSR | S_IRGRP) != 0)
		err(1, "Cannot chmod %s", args.fspec);

	exit(EXIT_SUCCESS);
}

void
usage()
{

	fprintf(stderr, "usage: mksnap_ffs mountpoint snapshot_name\n");
	exit(EX_USAGE);
}
