/*-
 * Copyright (c) 1989, 1992, 1993
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
"@(#) Copyright (c) 1989, 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)newfs.c	8.3 (Berkeley) 4/22/94";
#endif /* not lint */

/*
 * newfs: friendly front end to mkfs
 */
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/file.h>
#include <sys/mount.h>

#include <ufs/ufs/dir.h>
#include <ufs/ufs/dinode.h>

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <paths.h>
#include "config.h"
#include "extern.h"

#define	COMPAT			/* allow non-labeled disks */

int	mfs;			/* run as the memory based filesystem */
int	Nflag;			/* run without writing file system */
int	fssize;			/* file system size */
int	ntracks;		/* # tracks/cylinder */
int	nsectors;		/* # sectors/track */
int	nphyssectors;		/* # sectors/track including spares */
int	secpercyl;		/* sectors per cylinder */
int	trackspares = -1;	/* spare sectors per track */
int	cylspares = -1;		/* spare sectors per cylinder */
int	sectorsize;		/* bytes/sector */
#ifdef tahoe
int	realsectorsize;		/* bytes/sector in hardware */
#endif
int	rpm;			/* revolutions/minute of drive */
int	interleave;		/* hardware sector interleave */
int	trackskew = -1;		/* sector 0 skew, per track */
int	headswitch;		/* head switch time, usec */
int	trackseek;		/* track-to-track seek, usec */
int	fsize = 0;		/* fragment size */
int	bsize = 0;		/* block size */
int	cpg = DESCPG;		/* cylinders/cylinder group */
int	cpgflg;			/* cylinders/cylinder group flag was given */
int	minfree = MINFREE;	/* free space threshold */
int	opt = DEFAULTOPT;	/* optimization preference (space or time) */
int	density;		/* number of bytes per inode */
int	maxcontig = MAXCONTIG;	/* max contiguous blocks to allocate */
int	rotdelay = ROTDELAY;	/* rotational delay between blocks */
int	maxbpg;			/* maximum blocks per file in a cyl group */
int	nrpos = NRPOS;		/* # of distinguished rotational positions */
int	bbsize = BBSIZE;	/* boot block size */
int	sbsize = SBSIZE;	/* superblock size */
int	mntflags;		/* flags to be passed to mount */
u_long	memleft;		/* virtual memory available */
caddr_t	membase;		/* start address of memory based filesystem */
#ifdef COMPAT
char	*disktype;
int	unlabeled;
#endif

char	device[MAXPATHLEN];
char	*progname, *special;

static struct disklabel *getdisklabel __P((char *, int));
static struct disklabel *debug_readlabel __P((int));
static void rewritelabel __P((char *, int, struct disklabel *));
static void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register int ch;
	register struct partition *pp;
	register struct disklabel *lp;
	struct partition oldpartition;
	struct stat st;
	int debug, lfs, fsi, fso, segsize;
	char *cp, *opstring;

	if (progname = rindex(*argv, '/'))
		++progname;
	else
		progname = *argv;

	if (strstr(progname, "mfs")) {
		mfs = 1;
		Nflag++;
	}

	/* -F is mfs only and MUST come first! */
	opstring = "F:B:DLNS:T:a:b:c:d:e:f:i:k:l:m:n:o:p:r:s:t:u:x:";
	if (!mfs)
		opstring += 2;

	debug = lfs = segsize = 0;
	while ((ch = getopt(argc, argv, opstring)) !=  -1)
		switch(ch) {
		case 'B':	/* LFS segment size */
			if ((segsize = atoi(optarg)) < LFS_MINSEGSIZE)
				fatal("%s: bad segment size", optarg);
			break;
		case 'D':
			debug = 1;
			break;
		case 'F':
			if ((mntflags = atoi(optarg)) == 0)
				fatal("%s: bad mount flags", optarg);
			break;
		case 'L':	/* Create lfs */
			lfs = 1;
			break;
		case 'N':
			Nflag++;
			break;
		case 'S':
			if ((sectorsize = atoi(optarg)) <= 0)
				fatal("%s: bad sector size", optarg);
			break;
#ifdef COMPAT
		case 'T':
			disktype = optarg;
			break;
#endif
		case 'a':
			if ((maxcontig = atoi(optarg)) <= 0)
				fatal("%s: bad max contiguous blocks\n",
				    optarg);
			break;
		case 'b':	/* used for LFS */
			if ((bsize = atoi(optarg)) < LFS_MINBLOCKSIZE)
				fatal("%s: bad block size", optarg);
			break;
		case 'c':
			if ((cpg = atoi(optarg)) <= 0)
				fatal("%s: bad cylinders/group", optarg);
			cpgflg++;
			break;
		case 'd':
			if ((rotdelay = atoi(optarg)) < 0)
				fatal("%s: bad rotational delay\n", optarg);
			break;
		case 'e':
			if ((maxbpg = atoi(optarg)) <= 0)
				fatal("%s: bad blocks per file in a cyl group\n",
				    optarg);
			break;
		case 'f':
			if ((fsize = atoi(optarg)) <= 0)
				fatal("%s: bad frag size", optarg);
			break;
		case 'i':
			if ((density = atoi(optarg)) <= 0)
				fatal("%s: bad bytes per inode\n", optarg);
			break;
		case 'k':
			if ((trackskew = atoi(optarg)) < 0)
				fatal("%s: bad track skew", optarg);
			break;
		case 'l':
			if ((interleave = atoi(optarg)) <= 0)
				fatal("%s: bad interleave", optarg);
			break;
		case 'm':		/* used for LFS */
			if ((minfree = atoi(optarg)) < 0 || minfree > 99)
				fatal("%s: bad free space %%\n", optarg);
			break;
		case 'n':
			if ((nrpos = atoi(optarg)) <= 0)
				fatal("%s: bad rotational layout count\n",
				    optarg);
			break;
		case 'o':
			if (strcmp(optarg, "space") == 0)
				opt = FS_OPTSPACE;
			else if (strcmp(optarg, "time") == 0)
				opt = FS_OPTTIME;
			else
				fatal("%s: bad optimization preference %s",
				    optarg, "(options are `space' or `time')");
			break;
		case 'p':
			if ((trackspares = atoi(optarg)) < 0)
				fatal("%s: bad spare sectors per track",
				    optarg);
			break;
		case 'r':
			if ((rpm = atoi(optarg)) <= 0)
				fatal("%s: bad revs/minute\n", optarg);
			break;
		case 's':	/* used for LFS */
			if ((fssize = atoi(optarg)) <= 0)
				fatal("%s: bad file system size", optarg);
			break;
		case 't':
			if ((ntracks = atoi(optarg)) <= 0)
				fatal("%s: bad total tracks", optarg);
			break;
		case 'u':
			if ((nsectors = atoi(optarg)) <= 0)
				fatal("%s: bad sectors/track", optarg);
			break;
		case 'x':
			if ((cylspares = atoi(optarg)) < 0)
				fatal("%s: bad spare sectors per cylinder",
				    optarg);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2 && (mfs || argc != 1))
		usage();

	/*
	 * If the -N flag isn't specified, open the output file.  If no path
	 * prefix, try /dev/r%s and then /dev/%s.
	 */
	special = argv[0];
	if (index(special, '/') == NULL) {
		(void)sprintf(device, "%sr%s", _PATH_DEV, special);
		if (stat(device, &st) == -1)
			(void)sprintf(device, "%s%s", _PATH_DEV, special);
		special = device;
	}
	if (!Nflag) {
		fso = open(special,
		    (debug ? O_CREAT : 0) | O_WRONLY, DEFFILEMODE);
		if (fso < 0)
			fatal("%s: %s", special, strerror(errno));
	} else
		fso = -1;

	/* Open the input file. */
	fsi = open(special, O_RDONLY);
	if (fsi < 0)
		fatal("%s: %s", special, strerror(errno));
	if (fstat(fsi, &st) < 0)
		fatal("%s: %s", special, strerror(errno));

	if (!debug && !mfs && !S_ISCHR(st.st_mode))
		(void)printf("%s: %s: not a character-special device\n",
		    progname, special);
	cp = index(argv[0], '\0') - 1;
	if (!debug && (cp == 0 || (*cp < 'a' || *cp > 'h') && !isdigit(*cp)))
		fatal("%s: can't figure out file system partition", argv[0]);

#ifdef COMPAT
	if (!mfs && disktype == NULL)
		disktype = argv[1];
#endif
	if (debug)
		lp = debug_readlabel(fsi);
	else
		lp = getdisklabel(special, fsi);

	if (isdigit(*cp))
		pp = &lp->d_partitions[0];
	else
		pp = &lp->d_partitions[*cp - 'a'];
	if (pp->p_size == 0)
		fatal("%s: `%c' partition is unavailable", argv[0], *cp);
        if (fsize == 0) {
			fsize = pp->p_fsize;
			if (fsize <= 0)
				fsize = MAX(DFL_FRAGSIZE, lp->d_secsize);
	}

	if (bsize == 0) {
			bsize = pp->p_frag * fsize;
			if (bsize <= 0)
				bsize = MIN(DFL_LFSBLOCK, 8 * fsize);
	}

	if (segsize == 0) {
			segsize = pp->p_cpg * bsize;
			if (segsize <= 0)
				segsize = DFL_LFSSEG;
	}

	/* If we're making a LFS, we break out here */
	exit(make_lfs(fso, lp, pp, minfree, bsize, segsize));
}

#ifdef COMPAT
char lmsg[] = "%s: can't read disk label; disk type must be specified";
#else
char lmsg[] = "%s: can't read disk label";
#endif

static struct disklabel *
getdisklabel(s, fd)
	char *s;
	int fd;
{
	static struct disklabel lab;

	if (ioctl(fd, DIOCGDINFO, (char *)&lab) < 0) {
#ifdef COMPAT
		if (disktype) {
			struct disklabel *lp, *getdiskbyname();

			unlabeled++;
			lp = getdiskbyname(disktype);
			if (lp == NULL)
				fatal("%s: unknown disk type", disktype);
			return (lp);
		}
#endif
		(void)fprintf(stderr,
		    "%s: ioctl (GDINFO): %s\n", progname, strerror(errno));
		fatal(lmsg, s);
	}
	return (&lab);
}


static struct disklabel *
debug_readlabel(fd)
	int fd;
{
	static struct disklabel lab;
	int n;

	if ((n = read(fd, &lab, sizeof(struct disklabel))) < 0)
		fatal("unable to read disk label: %s", strerror(errno));
	else if (n < sizeof(struct disklabel))
		fatal("short read of disklabel: %d of %d bytes", n,
			sizeof(struct disklabel));
	return(&lab);
}

static void
rewritelabel(s, fd, lp)
	char *s;
	int fd;
	register struct disklabel *lp;
{
#ifdef COMPAT
	if (unlabeled)
		return;
#endif
	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);
	if (ioctl(fd, DIOCWDINFO, (char *)lp) < 0) {
		(void)fprintf(stderr,
		    "%s: ioctl (WDINFO): %s\n", progname, strerror(errno));
		fatal("%s: can't rewrite disk label", s);
	}
#if vax
	if (lp->d_type == DTYPE_SMD && lp->d_flags & D_BADSECT) {
		register i;
		int cfd;
		daddr_t alt;
		char specname[64];
		char blk[1024];
		char *cp;

		/*
		 * Make name for 'c' partition.
		 */
		strcpy(specname, s);
		cp = specname + strlen(specname) - 1;
		if (!isdigit(*cp))
			*cp = 'c';
		cfd = open(specname, O_WRONLY);
		if (cfd < 0)
			fatal("%s: %s", specname, strerror(errno));
		bzero(blk, sizeof(blk));
		*(struct disklabel *)(blk + LABELOFFSET) = *lp;
		alt = lp->d_ncylinders * lp->d_secpercyl - lp->d_nsectors;
		for (i = 1; i < 11 && i < lp->d_nsectors; i += 2) {
			if (lseek(cfd, (off_t)(alt + i) * lp->d_secsize,
			    L_SET) == -1)
				fatal("lseek to badsector area: %s",
				    strerror(errno));
			if (write(cfd, blk, lp->d_secsize) < lp->d_secsize)
				fprintf(stderr,
				    "%s: alternate label %d write: %s\n",
				    progname, i/2, strerror(errno));
		}
		close(cfd);
	}
#endif
}

void
usage()
{
	if (mfs) {
		fprintf(stderr,
		    "usage: mfs [ -fsoptions ] special-device mount-point\n");
	} else
		fprintf(stderr,
		    "usage: newlfs [ -fsoptions ] special-device%s\n",
#ifdef COMPAT
		    " [device-type]");
#else
		    "");
#endif
	fprintf(stderr, "where fsoptions are:\n");
	fprintf(stderr, "\t-B LFS segment size\n");
	fprintf(stderr, "\t-D debug\n");
	fprintf(stderr, "\t-F mount flags\n");
	fprintf(stderr, "\t-L create LFS file system\n");
	fprintf(stderr,
	    "\t-N do not create file system, just print out parameters\n");
	fprintf(stderr, "\t-S sector size\n");
#ifdef COMPAT
	fprintf(stderr, "\t-T disktype\n");
#endif
	fprintf(stderr, "\t-a maximum contiguous blocks\n");
	fprintf(stderr, "\t-b block size\n");
	fprintf(stderr, "\t-c cylinders/group\n");
	fprintf(stderr, "\t-d rotational delay between contiguous blocks\n");
	fprintf(stderr, "\t-e maximum blocks per file in a cylinder group\n");
	fprintf(stderr, "\t-f frag size\n");
	fprintf(stderr, "\t-i number of bytes per inode\n");
	fprintf(stderr, "\t-k sector 0 skew, per track\n");
	fprintf(stderr, "\t-l hardware sector interleave\n");
	fprintf(stderr, "\t-m minimum free space %%\n");
	fprintf(stderr, "\t-n number of distinguished rotational positions\n");
	fprintf(stderr, "\t-o optimization preference (`space' or `time')\n");
	fprintf(stderr, "\t-p spare sectors per track\n");
	fprintf(stderr, "\t-r revolutions/minute\n");
	fprintf(stderr, "\t-s file system size (sectors)\n");
	fprintf(stderr, "\t-t tracks/cylinder\n");
	fprintf(stderr, "\t-u sectors/track\n");
	fprintf(stderr, "\t-x spare sectors per cylinder\n");
	exit(1);
}
