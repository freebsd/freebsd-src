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
/*
static char sccsid[] = "@(#)mount_cd9660.c	8.4 (Berkeley) 3/27/94";
*/
static const char rcsid[] =
	"$Id: mount_cd9660.c,v 1.7 1996/05/13 17:42:55 wollman Exp $";
#endif /* not lint */

#include <sys/cdio.h>
#include <sys/file.h>
#include <sys/param.h>
#define CD9660
#include <sys/mount.h>

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "mntopts.h"

struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_UPDATE,
	{ "extatt", 0, ISOFSMNT_EXTATT, 1 },
	{ "gens", 0, ISOFSMNT_GENS, 1 },
	{ "rrip", 1, ISOFSMNT_NORRIP, 1 },
	{ NULL }
};

int	get_ssector(const char *dev);
void	usage(void);

int
main(int argc, char **argv)
{
	struct iso_args args;
	int ch, mntflags, opts;
	char *dev, *dir;
	struct vfsconf *vfc;
	int verbose;
  
	mntflags = opts = verbose = 0;
	memset(&args, 0, sizeof args);
	args.ssector = -1;
	while ((ch = getopt(argc, argv, "ego:rs:v")) != -1)
		switch (ch) {
		case 'e':
			opts |= ISOFSMNT_EXTATT;
			break;
		case 'g':
			opts |= ISOFSMNT_GENS;
			break;
		case 'o':
			getmntopts(optarg, mopts, &mntflags, &opts);
			break;
		case 'r':
			opts |= ISOFSMNT_NORRIP;
			break;
		case 's':
			args.ssector = atoi(optarg);
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
	args.fspec = dev;
	args.export.ex_root = DEFAULT_ROOTUID;

	/*
	 * ISO 9660 filesystems are not writeable.
	 */
	mntflags |= MNT_RDONLY;
	args.export.ex_flags = MNT_EXRDONLY;

	args.flags = opts;

	if (args.ssector == -1) {
		/*
		 * The start of the session has not been specified on
		 * the command line.  If we can successfully read the
		 * TOC of a CD-ROM, use the last data track we find.
		 * Otherwise, just use 0, in order to mount the very
		 * first session.  This is compatible with the
		 * historic behaviour of mount_cd9660(8).  If the user
		 * has specified -s <ssector> above, we don't get here
		 * and leave the user's will.
		 */
		if ((args.ssector = get_ssector(dev)) == -1) {
			if (verbose)
				printf("could not determine starting sector, "
				       "using very first session\n");
			args.ssector = 0;
		} else if (verbose)
			printf("using starting sector %d\n", args.ssector);
	}

	vfc = getvfsbyname("cd9660");
	if(!vfc && vfsisloadable("cd9660")) {
		if(vfsload("cd9660")) {
			err(EX_OSERR, "vfsload(cd9660)");
		}
		endvfsent();	/* flush cache */
		vfc = getvfsbyname("cd9660");
	}
	if (!vfc)
		errx(EX_OSERR, "cd9660 filesystem not available");

	if (mount(vfc->vfc_index, dir, mntflags, &args) < 0)
		err(EX_OSERR, "%s", dev);
	exit(0);
}

void
usage(void)
{
	(void)fprintf(stderr,
		"usage: mount_cd9660 [-egrv] [-o options] [-s startsector] special node\n");
	exit(EX_USAGE);
}

int
get_ssector(const char *dev)
{
	struct ioc_toc_header h;
	struct ioc_read_toc_entry t;
	struct cd_toc_entry toc_buffer[100];
	int fd, ntocentries, i;

	if ((fd = open(dev, O_RDONLY)) == -1)
		return -1;
	if (ioctl(fd, CDIOREADTOCHEADER, &h) == -1) {
		close(fd);
		return -1;
	}

	ntocentries = h.ending_track - h.starting_track + 1;
	if (ntocentries > 100) {
		/* unreasonable, only 100 allowed */
		close(fd);
		return -1;
	}
	t.address_format = CD_LBA_FORMAT;
	t.starting_track = 0;
	t.data_len = ntocentries * sizeof(struct cd_toc_entry);
	t.data = toc_buffer;

	if (ioctl(fd, CDIOREADTOCENTRYS, (char *) &t) == -1) {
		close(fd);
		return -1;
	}
	close(fd);
	
	for (i = ntocentries - 1; i >= 0; i--)
		if ((toc_buffer[i].control & 4) != 0)
			/* found a data track */
			break;
	if (i < 0)
		return -1;

	return ntohl(toc_buffer[i].addr.lba);
}
