/*
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Marshall
 * Kirk McKusick and Network Associates Laboratories, the Security
 * Research Division of Network Associates, Inc. under DARPA/SPAWAR
 * contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA CHATS
 * research program.
 *
 * Copyright (c) 1983, 1989, 1993, 1994
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
"@(#) Copyright (c) 1983, 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)newfs.c	8.13 (Berkeley) 5/1/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * newfs: friendly front end to mkfs
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/file.h>
#include <sys/mount.h>

#include <ufs/ufs/dir.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#include <ufs/ufs/ufsmount.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "newfs.h"

/*
 * The following two constants set the default block and fragment sizes.
 * Both constants must be a power of 2 and meet the following constraints:
 *	MINBSIZE <= DESBLKSIZE <= MAXBSIZE
 *	sectorsize <= DESFRAGSIZE <= DESBLKSIZE
 *	DESBLKSIZE / DESFRAGSIZE <= 8
 */
#define	DFL_FRAGSIZE	2048
#define	DFL_BLKSIZE	16384

/*
 * Cylinder groups may have up to MAXBLKSPERCG blocks. The actual
 * number used depends upon how much information can be stored
 * in a cylinder group map which must fit in a single file system
 * block. The default is to use as many as possible blocks per group.
 */
#define	MAXBLKSPERCG	0x7fffffff	/* desired fs_fpg ("infinity") */

/*
 * MAXBLKPG determines the maximum number of data blocks which are
 * placed in a single cylinder group. The default is one indirect
 * block worth of data blocks.
 */
#define MAXBLKPG(bsize)	((bsize) / sizeof(ufs2_daddr_t))

/*
 * Each file system has a number of inodes statically allocated.
 * We allocate one inode slot per NFPI fragments, expecting this
 * to be far more than we will ever need.
 */
#define	NFPI		4

int	Nflag;			/* run without writing file system */
int	Oflag = 1;		/* file system format (1 => UFS1, 2 => UFS2) */
int	Rflag;			/* regression test */
int	Uflag;			/* enable soft updates for file system */
quad_t	fssize;			/* file system size */
int	sectorsize;		/* bytes/sector */
int	realsectorsize;		/* bytes/sector in hardware */
int	fsize = 0;		/* fragment size */
int	bsize = 0;		/* block size */
int	maxbsize = 0;		/* maximum clustering */
int	maxblkspercg = MAXBLKSPERCG; /* maximum blocks per cylinder group */
int	minfree = MINFREE;	/* free space threshold */
int	opt = DEFAULTOPT;	/* optimization preference (space or time) */
int	density;		/* number of bytes per inode */
int	maxcontig = 0;		/* max contiguous blocks to allocate */
int	maxbpg;			/* maximum blocks per file in a cyl group */
int	avgfilesize = AVFILESIZ;/* expected average file size */
int	avgfilesperdir = AFPDIR;/* expected number of files per directory */
int	fso;			/* filedescriptor to device */

static char	device[MAXPATHLEN];
static char	*disktype;
static int	unlabeled;

static struct disklabel *getdisklabel(char *s);
static void rewritelabel(char *s, struct disklabel *lp);
static void usage(void);

int
main(int argc, char *argv[])
{
	struct partition *pp;
	struct disklabel *lp;
	struct partition oldpartition;
	struct stat st;
	char *cp, *special;
	int ch;
	off_t mediasize;

	while ((ch = getopt(argc, argv,
	    "NO:RS:T:Ua:b:c:d:e:f:g:h:i:m:o:s:")) != -1)
		switch (ch) {
		case 'N':
			Nflag = 1;
			break;
		case 'O':
			if ((Oflag = atoi(optarg)) < 1 || Oflag > 2)
				errx(1, "%s: bad file system format value",
				    optarg);
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'S':
			if ((sectorsize = atoi(optarg)) <= 0)
				errx(1, "%s: bad sector size", optarg);
			break;
		case 'T':
			disktype = optarg;
			break;
		case 'U':
			Uflag = 1;
			break;
		case 'a':
			if ((maxcontig = atoi(optarg)) <= 0)
				errx(1, "%s: bad maximum contiguous blocks",
				    optarg);
			break;
		case 'b':
			if ((bsize = atoi(optarg)) < MINBSIZE)
				errx(1, "%s: block size too small, min is %d",
				    optarg, MINBSIZE);
			if (bsize > MAXBSIZE)
				errx(1, "%s: block size too large, max is %d",
				    optarg, MAXBSIZE);
			break;
		case 'c':
			if ((maxblkspercg = atoi(optarg)) <= 0)
				errx(1, "%s: bad blocks per cylinder group",
				    optarg);
			break;
		case 'd':
			if ((maxbsize = atoi(optarg)) < MINBSIZE)
				errx(1, "%s: bad extent block size", optarg);
			break;
		case 'e':
			if ((maxbpg = atoi(optarg)) <= 0)
			  errx(1, "%s: bad blocks per file in a cylinder group",
				    optarg);
			break;
		case 'f':
			if ((fsize = atoi(optarg)) <= 0)
				errx(1, "%s: bad fragment size", optarg);
			break;
		case 'g':
			if ((avgfilesize = atoi(optarg)) <= 0)
				errx(1, "%s: bad average file size", optarg);
			break;
		case 'h':
			if ((avgfilesperdir = atoi(optarg)) <= 0)
			       errx(1, "%s: bad average files per dir", optarg);
			break;
		case 'i':
			if ((density = atoi(optarg)) <= 0)
				errx(1, "%s: bad bytes per inode", optarg);
			break;
		case 'm':
			if ((minfree = atoi(optarg)) < 0 || minfree > 99)
				errx(1, "%s: bad free space %%", optarg);
			break;
		case 'o':
			if (strcmp(optarg, "space") == 0)
				opt = FS_OPTSPACE;
			else if (strcmp(optarg, "time") == 0)
				opt = FS_OPTTIME;
			else
				errx(1, 
		"%s: unknown optimization preference: use `space' or `time'",
				    optarg);
			break;
		case 's':
			if ((fssize = atoi(optarg)) <= 0)
				errx(1, "%s: bad file system size", optarg);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	special = argv[0];
	cp = strrchr(special, '/');
	if (cp == 0) {
		/*
		 * No path prefix; try prefixing _PATH_DEV.
		 */
		snprintf(device, sizeof(device), "%s%s", _PATH_DEV, special);
		special = device;
	}

	fso = open(special, Nflag ? O_RDONLY : O_RDWR);
	if (fso < 0)
		err(1, "%s", special);
	if (fstat(fso, &st) < 0)
		err(1, "%s", special);
	if ((st.st_mode & S_IFMT) != S_IFCHR)
		errx(1, "%s: not a character-special device", special);

	if (sectorsize == 0) 
		ioctl(fso, DIOCGSECTORSIZE, &sectorsize);
	if (sectorsize && !ioctl(fso, DIOCGMEDIASIZE, &mediasize)) {
		if (fssize == 0)
			fssize = mediasize / sectorsize;
		else if (fssize > mediasize / sectorsize)
			errx(1, "%s: maximum file system size is %u",
			    special, (u_int)(mediasize / sectorsize));
	}
	pp = NULL;
	lp = getdisklabel(special);
	if (lp != NULL) {
		cp = strchr(special, '\0');
		cp--;
		if ((*cp < 'a' || *cp > 'h') && !isdigit(*cp))
			errx(1, "%s: can't figure out file system partition",
			    special);
		if (isdigit(*cp))
			pp = &lp->d_partitions[RAW_PART];
		else
			pp = &lp->d_partitions[*cp - 'a'];
		oldpartition = *pp;
		if (pp->p_size == 0)
			errx(1, "%s: `%c' partition is unavailable",
			    special, *cp);
		if (pp->p_fstype == FS_BOOT)
			errx(1, "%s: `%c' partition overlaps boot program",
			    special, *cp);
		if (fssize == 0)
			fssize = pp->p_size;
		if (fssize > pp->p_size)
			errx(1, 
		    "%s: maximum file system size %d", special, pp->p_size);
		if (sectorsize == 0)
			sectorsize = lp->d_secsize;
		if (fsize == 0)
			fsize = pp->p_fsize;
		if (bsize == 0)
			bsize = pp->p_frag * pp->p_fsize;
	}
	if (sectorsize <= 0)
		errx(1, "%s: no default sector size", special);
	if (fsize <= 0)
		fsize = MAX(DFL_FRAGSIZE, sectorsize);
	if (bsize <= 0)
		bsize = MIN(DFL_BLKSIZE, 8 * fsize);
	if (maxbsize == 0)
		maxbsize = bsize;
	/*
	 * Maxcontig sets the default for the maximum number of blocks
	 * that may be allocated sequentially. With file system clustering
	 * it is possible to allocate contiguous blocks up to the maximum
	 * transfer size permitted by the controller or buffering.
	 */
	if (maxcontig == 0)
		maxcontig = MAX(1, MAXPHYS / bsize);
	if (density == 0)
		density = NFPI * fsize;
	if (minfree < MINFREE && opt != FS_OPTSPACE) {
		fprintf(stderr, "Warning: changing optimization to space ");
		fprintf(stderr, "because minfree is less than %d%%\n", MINFREE);
		opt = FS_OPTSPACE;
	}
	if (maxbpg == 0)
		maxbpg = MAXBLKPG(bsize);
	realsectorsize = sectorsize;
	if (sectorsize != DEV_BSIZE) {		/* XXX */
		int secperblk = sectorsize / DEV_BSIZE;

		sectorsize = DEV_BSIZE;
		fssize *= secperblk;
		if (pp != NULL)
			pp->p_size *= secperblk;
	}
	mkfs(pp, special);
	if (!unlabeled) {
		if (realsectorsize != DEV_BSIZE)
			pp->p_size /= realsectorsize / DEV_BSIZE;
		if (!Nflag && bcmp(pp, &oldpartition, sizeof(oldpartition)))
			rewritelabel(special, lp);
	}
	close(fso);
	exit(0);
}

struct disklabel *
getdisklabel(char *s)
{
	static struct disklabel lab;
	struct disklabel *lp;

	if (!ioctl(fso, DIOCGDINFO, (char *)&lab))
		return (&lab);
	unlabeled++;
	if (disktype) {
		lp = getdiskbyname(disktype);
		if (lp != NULL)
			return (lp);
	}
	return (NULL);
}

void
rewritelabel(char *s, struct disklabel *lp)
{
	if (unlabeled)
		return;
	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);
	if (ioctl(fso, DIOCWDINFO, (char *)lp) < 0)
		warn("ioctl (WDINFO): %s: can't rewrite disk label", s);
}

static void
usage()
{
	fprintf(stderr,
	    "usage: %s [ -fsoptions ] special-device%s\n",
	    getprogname(),
	    " [device-type]");
	fprintf(stderr, "where fsoptions are:\n");
	fprintf(stderr,
	    "\t-N do not create file system, just print out parameters\n");
	fprintf(stderr, "\t-O file system format: 1 => UFS1, 2 => UFS2\n");
	fprintf(stderr, "\t-R regression test, supress random factors\n");
	fprintf(stderr, "\t-S sector size\n");
	fprintf(stderr, "\t-T disktype\n");
	fprintf(stderr, "\t-U enable soft updates\n");
	fprintf(stderr, "\t-a maximum contiguous blocks\n");
	fprintf(stderr, "\t-b block size\n");
	fprintf(stderr, "\t-c blocks per cylinders group\n");
	fprintf(stderr, "\t-d maximum extent size\n");
	fprintf(stderr, "\t-e maximum blocks per file in a cylinder group\n");
	fprintf(stderr, "\t-f frag size\n");
	fprintf(stderr, "\t-g average file size\n");
	fprintf(stderr, "\t-h average files per directory\n");
	fprintf(stderr, "\t-i number of bytes per inode\n");
	fprintf(stderr, "\t-m minimum free space %%\n");
	fprintf(stderr, "\t-o optimization preference (`space' or `time')\n");
	fprintf(stderr, "\t-s file systemsize (sectors)\n");
	exit(1);
}
