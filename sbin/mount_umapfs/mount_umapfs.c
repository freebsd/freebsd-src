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
char copyright[] =
"@(#) Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*
static char sccsid[] = "@(#)mount_umap.c	8.3 (Berkeley) 3/27/94";
*/
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <miscfs/umapfs/umap.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "mntopts.h"

#define ROOTUSER 0
/*
 * This define controls whether any user but the superuser can own and
 * write mapfiles.  If other users can, system security can be gravely
 * compromised.  If this is not a concern, undefine SECURITY.
 */
#define MAPSECURITY 1

/*
 * This routine provides the user interface to mounting a umap layer.
 * It takes 4 mandatory parameters.  The mandatory arguments are the place
 * where the next lower level is mounted, the place where the umap layer is to
 * be mounted, the name of the user mapfile, and the name of the group
 * mapfile.  The routine checks the ownerships and permissions on the
 * mapfiles, then opens and reads them.  Then it calls mount(), which
 * will, in turn, call the umap version of mount.
 */

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	{ NULL }
};

static __dead void	usage __P((void)) __dead2;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	static char not[] = "; not mounted.";
	struct stat statbuf;
	struct umap_args args;
        FILE *fp, *gfp;
        u_long gmapdata[GMAPFILEENTRIES][2], mapdata[MAPFILEENTRIES][2];
	int ch, count, gnentries, mntflags, nentries;
	char *gmapfile, *mapfile, *source, *target, buf[20];
	struct vfsconf *vfc;

	mntflags = 0;
	mapfile = gmapfile = NULL;
	while ((ch = getopt(argc, argv, "g:o:u:")) != EOF)
		switch (ch) {
		case 'g':
			gmapfile = optarg;
			break;
		case 'o':
			getmntopts(optarg, mopts, &mntflags, 0);
			break;
		case 'u':
			mapfile = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2 || mapfile == NULL || gmapfile == NULL)
		usage();

	source = argv[0];
	target = argv[1];

	/* Read in uid mapping data. */
	if ((fp = fopen(mapfile, "r")) == NULL)
		err(EX_NOINPUT, "%s%s", mapfile, not);

#ifdef MAPSECURITY
	/*
	 * Check that group and other don't have write permissions on
	 * this mapfile, and that the mapfile belongs to root.
	 */
	if (fstat(fileno(fp), &statbuf))
		err(EX_OSERR, "%s%s", mapfile, not);
	if (statbuf.st_mode & S_IWGRP || statbuf.st_mode & S_IWOTH) {
		strmode(statbuf.st_mode, buf);
		err(EX_NOPERM, "%s: improper write permissions (%s)%s",
		    mapfile, buf, not);
	}
	if (statbuf.st_uid != ROOTUSER)
		errx(EX_NOPERM, "%s does not belong to root%s", mapfile, not);
#endif /* MAPSECURITY */

	if ((fscanf(fp, "%d\n", &nentries)) != 1)
		errx(EX_DATAERR, "%s: nentries not found%s", mapfile, not);
	if (nentries > MAPFILEENTRIES)
		errx(EX_DATAERR,
		    "maximum number of entries is %d%s", MAPFILEENTRIES, not);
#if 0
	(void)printf("reading %d entries\n", nentries);
#endif
	for (count = 0; count < nentries; ++count) {
		if ((fscanf(fp, "%lu %lu\n",
		    &(mapdata[count][0]), &(mapdata[count][1]))) != 2) {
			if (ferror(fp))
				err(EX_OSERR, "%s%s", mapfile, not);
			if (feof(fp))
				errx(EX_DATAERR, "%s: unexpected end-of-file%s",
				    mapfile, not);
			errx(EX_DATAERR, "%s: illegal format (line %d)%s",
			    mapfile, count + 2, not);
		}
#if 0
		/* Fix a security hole. */
		if (mapdata[count][1] == 0)
			errx(1, "mapping id 0 not permitted (line %d)%s",
			    count + 2, not);
#endif
	}

	/* Read in gid mapping data. */
	if ((gfp = fopen(gmapfile, "r")) == NULL)
		err(EX_NOINPUT, "%s%s", gmapfile, not);

#ifdef MAPSECURITY
	/*
	 * Check that group and other don't have write permissions on
	 * this group mapfile, and that the file belongs to root.
	 */
	if (fstat(fileno(gfp), &statbuf))
		err(EX_OSERR, "%s%s", gmapfile, not);
	if (statbuf.st_mode & S_IWGRP || statbuf.st_mode & S_IWOTH) {
		strmode(statbuf.st_mode, buf);
		err(EX_NOPERM, "%s: improper write permissions (%s)%s",
		    gmapfile, buf, not);
	}
	if (statbuf.st_uid != ROOTUSER)
		errx(EX_NOPERM, "%s does not belong to root%s", gmapfile, not);
#endif /* MAPSECURITY */

	if ((fscanf(gfp, "%d\n", &gnentries)) != 1)
		errx(EX_DATAERR, "nentries not found%s", gmapfile, not);
	if (gnentries > MAPFILEENTRIES)
		errx(EX_DATAERR,
		    "maximum number of entries is %d%s", GMAPFILEENTRIES, not);
#if 0
	(void)printf("reading %d group entries\n", gnentries);
#endif

	for (count = 0; count < gnentries; ++count)
		if ((fscanf(gfp, "%lu %lu\n",
		    &(gmapdata[count][0]), &(gmapdata[count][1]))) != 2) {
			if (ferror(gfp))
				err(EX_OSERR, "%s%s", gmapfile, not);
			if (feof(gfp))
				errx(EX_DATAERR, "%s: unexpected end-of-file%s",
				    gmapfile, not);
			errx(EX_DATAERR, "%s: illegal format (line %d)%s",
			    gmapfile, count + 2, not);
		}


	/* Setup mount call args. */
	args.target = source;
	args.nentries = nentries;
	args.mapdata = mapdata;
	args.gnentries = gnentries;
	args.gmapdata = gmapdata;

	vfc = getvfsbyname("umap");
	if(!vfc && vfsisloadable("umap")) {
		if(vfsload("umap"))
			err(1, "vfsload(umap)");
		endvfsent();	/* flush cache */
		vfc = getvfsbyname("umap");
	}
	if (!vfc) {
		errx(1, "umap filesystem not available");
	}

	if (mount(vfc->vfc_index, argv[1], mntflags, &args))
		err(1, NULL);
	exit(0);
}

void
usage()
{
	(void)fprintf(stderr,
"usage: mount_umap [-o options] -u usermap -g groupmap target_fs mount_point\n");
	exit(EX_USAGE);
}
