/*
 * Copyright (c) 1980, 1986 The Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)setup.c	5.33 (Berkeley) 2/22/91";
static char rcsid[] = "$Header: /a/cvs/386BSD/src/sbin/fsck/setup.c,v 1.2 1993/07/22 16:52:00 jkh Exp $";
#endif /* not lint */

#define DKTYPENAMES
#include <sys/param.h>
#include <ufs/dinode.h>
#include <ufs/fs.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/file.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "fsck.h"

struct bufarea asblk;
#define altsblock (*asblk.b_un.b_fs)
#define POWEROF2(num)	(((num) & ((num) - 1)) == 0)

/*
 * The size of a cylinder group is calculated by CGSIZE. The maximum size
 * is limited by the fact that cylinder groups are at most one block.
 * Its size is derived from the size of the maps maintained in the 
 * cylinder group and the (struct cg) size.
 */
#define CGSIZE(fs) \
    /* base cg */	(sizeof(struct cg) + \
    /* blktot size */	(fs)->fs_cpg * sizeof(long) + \
    /* blks size */	(fs)->fs_cpg * (fs)->fs_nrpos * sizeof(short) + \
    /* inode map */	howmany((fs)->fs_ipg, NBBY) + \
    /* block map */	howmany((fs)->fs_cpg * (fs)->fs_spc / NSPF(fs), NBBY))

char	*index();
struct	disklabel *getdisklabel();

setup(dev)
	char *dev;
{
	long cg, size, asked, i, j;
	long bmapsize;
	struct disklabel *lp;
	struct stat statb;
	struct fs proto;

	havesb = 0;
	if (stat(dev, &statb) < 0) {
		printf("Can't stat %s: %s\n", dev, strerror(errno));
		return (0);
	}
	if ((statb.st_mode & S_IFMT) != S_IFCHR) {
		pfatal("%s is not a character device", dev);
		if (reply("CONTINUE") == 0)
			return (0);
	}
	if ((fsreadfd = open(dev, O_RDONLY)) < 0) {
		printf("Can't open %s: %s\n", dev, strerror(errno));
		return (0);
	}
	if (preen == 0)
		printf("** %s", dev);
	if (nflag || (fswritefd = open(dev, O_WRONLY)) < 0) {
		fswritefd = -1;
		if (preen)
			pfatal("NO WRITE ACCESS");
		printf(" (NO WRITE)");
	}
	if (preen == 0)
		printf("\n");
	fsmodified = 0;
	lfdir = 0;
	initbarea(&sblk);
	initbarea(&asblk);
	sblk.b_un.b_buf = malloc(SBSIZE);
	asblk.b_un.b_buf = malloc(SBSIZE);
	if (sblk.b_un.b_buf == NULL || asblk.b_un.b_buf == NULL)
		errexit("cannot allocate space for superblock\n");
	if (lp = getdisklabel((char *)NULL, fsreadfd))
		dev_bsize = secsize = lp->d_secsize;
	else
		dev_bsize = secsize = DEV_BSIZE;
#ifdef tahoe
	/*
	 * On the tahoe, the disk label and the disk driver disagree.
	 * The label knows that sectors are 512 bytes, but the disk
	 * drivers will only transfer in 1024 sized pieces.
	 */
	secsize = 1024;
#endif
	/*
	 * Read in the superblock, looking for alternates if necessary
	 */
	if (readsb(1) == 0) {
		if (bflag || preen || calcsb(dev, fsreadfd, &proto) == 0)
			return(0);
		if (reply("LOOK FOR ALTERNATE SUPERBLOCKS") == 0)
			return (0);
		for (cg = 0; cg < proto.fs_ncg; cg++) {
			bflag = fsbtodb(&proto, cgsblock(&proto, cg));
			if (readsb(0) != 0)
				break;
		}
		if (cg >= proto.fs_ncg) {
			printf("%s %s\n%s %s\n%s %s\n",
				"SEARCH FOR ALTERNATE SUPER-BLOCK",
				"FAILED. YOU MUST USE THE",
				"-b OPTION TO FSCK TO SPECIFY THE",
				"LOCATION OF AN ALTERNATE",
				"SUPER-BLOCK TO SUPPLY NEEDED",
				"INFORMATION; SEE fsck(8).");
			return(0);
		}
		pwarn("USING ALTERNATE SUPERBLOCK AT %d\n", bflag);
	}
	maxfsblock = sblock.fs_size;
	maxino = sblock.fs_ncg * sblock.fs_ipg;
	/*
	 * Check and potentially fix certain fields in the super block.
	 */
	if (sblock.fs_optim != FS_OPTTIME && sblock.fs_optim != FS_OPTSPACE) {
		pfatal("UNDEFINED OPTIMIZATION IN SUPERBLOCK");
		if (reply("SET TO DEFAULT") == 1) {
			sblock.fs_optim = FS_OPTTIME;
			sbdirty();
		}
	}
	if ((sblock.fs_minfree < 0 || sblock.fs_minfree > 99)) {
		pfatal("IMPOSSIBLE MINFREE=%d IN SUPERBLOCK",
			sblock.fs_minfree);
		if (reply("SET TO DEFAULT") == 1) {
			sblock.fs_minfree = 10;
			sbdirty();
		}
	}
	if (sblock.fs_interleave < 1 || 
	    sblock.fs_interleave > sblock.fs_nsect) {
		pwarn("IMPOSSIBLE INTERLEAVE=%d IN SUPERBLOCK",
			sblock.fs_interleave);
		sblock.fs_interleave = 1;
		if (preen)
			printf(" (FIXED)\n");
		if (preen || reply("SET TO DEFAULT") == 1) {
			sbdirty();
			dirty(&asblk);
		}
	}
	if (sblock.fs_npsect < sblock.fs_nsect || 
	    sblock.fs_npsect > sblock.fs_nsect*2) {
		pwarn("IMPOSSIBLE NPSECT=%d IN SUPERBLOCK",
			sblock.fs_npsect);
		sblock.fs_npsect = sblock.fs_nsect;
		if (preen)
			printf(" (FIXED)\n");
		if (preen || reply("SET TO DEFAULT") == 1) {
			sbdirty();
			dirty(&asblk);
		}
	}
	if (cvtflag) {
		if (sblock.fs_postblformat == FS_42POSTBLFMT) {
			/*
			 * Requested to convert from old format to new format
			 */
			if (preen)
				pwarn("CONVERTING TO NEW FILE SYSTEM FORMAT\n");
			else if (!reply("CONVERT TO NEW FILE SYSTEM FORMAT"))
				return(0);
			sblock.fs_postblformat = FS_DYNAMICPOSTBLFMT;
			sblock.fs_nrpos = 8;
			sblock.fs_postbloff =
			    (char *)(&sblock.fs_opostbl[0][0]) -
			    (char *)(&sblock.fs_link);
			sblock.fs_rotbloff = &sblock.fs_space[0] -
			    (u_char *)(&sblock.fs_link);
			sblock.fs_cgsize =
				fragroundup(&sblock, CGSIZE(&sblock));
			/*
			 * Planning now for future expansion.
			 */
#			if (BYTE_ORDER == BIG_ENDIAN)
				sblock.fs_qbmask.val[0] = 0;
				sblock.fs_qbmask.val[1] = ~sblock.fs_bmask;
				sblock.fs_qfmask.val[0] = 0;
				sblock.fs_qfmask.val[1] = ~sblock.fs_fmask;
#			endif /* BIG_ENDIAN */
#			if (BYTE_ORDER == LITTLE_ENDIAN)
				sblock.fs_qbmask.val[0] = ~sblock.fs_bmask;
				sblock.fs_qbmask.val[1] = 0;
				sblock.fs_qfmask.val[0] = ~sblock.fs_fmask;
				sblock.fs_qfmask.val[1] = 0;
#			endif /* LITTLE_ENDIAN */
			sbdirty();
			dirty(&asblk);
		} else if (sblock.fs_postblformat == FS_DYNAMICPOSTBLFMT) {
			/*
			 * Requested to convert from new format to old format
			 */
			if (sblock.fs_nrpos != 8 || sblock.fs_ipg > 2048 ||
			    sblock.fs_cpg > 32 || sblock.fs_cpc > 16) {
				printf(
				"PARAMETERS OF CURRENT FILE SYSTEM DO NOT\n\t");
				errexit(
				"ALLOW CONVERSION TO OLD FILE SYSTEM FORMAT\n");
			}
			if (preen)
				pwarn("CONVERTING TO OLD FILE SYSTEM FORMAT\n");
			else if (!reply("CONVERT TO OLD FILE SYSTEM FORMAT"))
				return(0);
			sblock.fs_postblformat = FS_42POSTBLFMT;
			sblock.fs_cgsize = fragroundup(&sblock,
			    sizeof(struct ocg) + howmany(sblock.fs_fpg, NBBY));
			sbdirty();
			dirty(&asblk);
		} else {
			errexit("UNKNOWN FILE SYSTEM FORMAT\n");
		}
	}
	if (asblk.b_dirty) {
		bcopy((char *)&sblock, (char *)&altsblock,
			(size_t)sblock.fs_sbsize);
		flush(fswritefd, &asblk);
	}
	/*
	 * read in the summary info.
	 */
	asked = 0;
	for (i = 0, j = 0; i < sblock.fs_cssize; i += sblock.fs_bsize, j++) {
		size = sblock.fs_cssize - i < sblock.fs_bsize ?
		    sblock.fs_cssize - i : sblock.fs_bsize;
		sblock.fs_csp[j] = (struct csum *)calloc(1, (unsigned)size);
		if (bread(fsreadfd, (char *)sblock.fs_csp[j],
		    fsbtodb(&sblock, sblock.fs_csaddr + j * sblock.fs_frag),
		    size) != 0 && !asked) {
			pfatal("BAD SUMMARY INFORMATION");
			if (reply("CONTINUE") == 0)
				errexit("");
			asked++;
		}
	}
	/*
	 * allocate and initialize the necessary maps
	 */
	bmapsize = roundup(howmany(maxfsblock, NBBY), sizeof(short));
	blockmap = calloc((unsigned)bmapsize, sizeof (char));
	if (blockmap == NULL) {
		printf("cannot alloc %u bytes for blockmap\n",
		    (unsigned)bmapsize);
		goto badsb;
	}
	statemap = calloc((unsigned)(maxino + 1), sizeof(char));
	if (statemap == NULL) {
		printf("cannot alloc %u bytes for statemap\n",
		    (unsigned)(maxino + 1));
		goto badsb;
	}
	lncntp = (short *)calloc((unsigned)(maxino + 1), sizeof(short));
	if (lncntp == NULL) {
		printf("cannot alloc %u bytes for lncntp\n", 
		    (unsigned)(maxino + 1) * sizeof(short));
		goto badsb;
	}
	numdirs = sblock.fs_cstotal.cs_ndir;
	inplast = 0;
	listmax = numdirs + 10;
	inpsort = (struct inoinfo **)calloc((unsigned)listmax,
	    sizeof(struct inoinfo *));
	inphead = (struct inoinfo **)calloc((unsigned)numdirs,
	    sizeof(struct inoinfo *));
	if (inpsort == NULL || inphead == NULL) {
		printf("cannot alloc %u bytes for inphead\n", 
		    (unsigned)numdirs * sizeof(struct inoinfo *));
		goto badsb;
	}
	bufinit();
	return (1);

badsb:
	ckfini();
	return (0);
}

/*
 * Read in the super block and its summary info.
 */
readsb(listerr)
	int listerr;
{
	daddr_t super = bflag ? bflag : SBOFF / dev_bsize;

	if (bread(fsreadfd, (char *)&sblock, super, (long)SBSIZE) != 0)
		return (0);
	sblk.b_bno = super;
	sblk.b_size = SBSIZE;
	/*
	 * run a few consistency checks of the super block
	 */
	if (sblock.fs_magic != FS_MAGIC)
		{ badsb(listerr, "MAGIC NUMBER WRONG"); return (0); }
	if (sblock.fs_ncg < 1)
		{ badsb(listerr, "NCG OUT OF RANGE"); return (0); }
	if (sblock.fs_cpg < 1)
		{ badsb(listerr, "CPG OUT OF RANGE"); return (0); }
	if (sblock.fs_ncg * sblock.fs_cpg < sblock.fs_ncyl ||
	    (sblock.fs_ncg - 1) * sblock.fs_cpg >= sblock.fs_ncyl)
		{ badsb(listerr, "NCYL LESS THAN NCG*CPG"); return (0); }
	if (sblock.fs_sbsize > SBSIZE)
		{ badsb(listerr, "SIZE PREPOSTEROUSLY LARGE"); return (0); }
	/*
	 * Compute block size that the filesystem is based on,
	 * according to fsbtodb, and adjust superblock block number
	 * so we can tell if this is an alternate later.
	 */
	super *= dev_bsize;
	dev_bsize = sblock.fs_fsize / fsbtodb(&sblock, 1);
	sblk.b_bno = super / dev_bsize;
	/*
	 * Set all possible fields that could differ, then do check
	 * of whole super block against an alternate super block.
	 * When an alternate super-block is specified this check is skipped.
	 */
	getblk(&asblk, cgsblock(&sblock, sblock.fs_ncg - 1), sblock.fs_sbsize);
	if (asblk.b_errs)
		return (0);
	if (bflag) {
		havesb = 1;
		return (1);
	}
	altsblock.fs_link = sblock.fs_link;
	altsblock.fs_rlink = sblock.fs_rlink;
	altsblock.fs_time = sblock.fs_time;
	altsblock.fs_cstotal = sblock.fs_cstotal;
	altsblock.fs_cgrotor = sblock.fs_cgrotor;
	altsblock.fs_fmod = sblock.fs_fmod;
	altsblock.fs_clean = sblock.fs_clean;
	altsblock.fs_ronly = sblock.fs_ronly;
	altsblock.fs_flags = sblock.fs_flags;
	altsblock.fs_maxcontig = sblock.fs_maxcontig;
	altsblock.fs_minfree = sblock.fs_minfree;
	altsblock.fs_optim = sblock.fs_optim;
	altsblock.fs_rotdelay = sblock.fs_rotdelay;
	altsblock.fs_maxbpg = sblock.fs_maxbpg;
	bcopy((char *)sblock.fs_csp, (char *)altsblock.fs_csp,
		sizeof sblock.fs_csp);
	bcopy((char *)sblock.fs_fsmnt, (char *)altsblock.fs_fsmnt,
		sizeof sblock.fs_fsmnt);
	bcopy((char *)sblock.fs_sparecon, (char *)altsblock.fs_sparecon,
		sizeof sblock.fs_sparecon);
	/*
	 * The following should not have to be copied.
	 */
	altsblock.fs_fsbtodb = sblock.fs_fsbtodb;
	altsblock.fs_interleave = sblock.fs_interleave;
	altsblock.fs_npsect = sblock.fs_npsect;
	altsblock.fs_nrpos = sblock.fs_nrpos;
	if (bcmp((char *)&sblock, (char *)&altsblock, (int)sblock.fs_sbsize)) {
		badsb(listerr,
		"VALUES IN SUPER BLOCK DISAGREE WITH THOSE IN FIRST ALTERNATE");
		return (0);
	}
	havesb = 1;
	return (1);
}

badsb(listerr, s)
	int listerr;
	char *s;
{

	if (!listerr)
		return;
	if (preen)
		printf("%s: ", devname);
	pfatal("BAD SUPER BLOCK: %s\n", s);
}

/*
 * Calculate a prototype superblock based on information in the disk label.
 * When done the cgsblock macro can be calculated and the fs_ncg field
 * can be used. Do NOT attempt to use other macros without verifying that
 * their needed information is available!
 */
calcsb(dev, devfd, fs)
	char *dev;
	int devfd;
	register struct fs *fs;
{
	register struct disklabel *lp;
	register struct partition *pp;
	register char *cp;
	int i;

	cp = index(dev, '\0') - 1;
	if (cp == (char *)-1 || (*cp < 'a' || *cp > 'h') && !isdigit(*cp)) {
		pfatal("%s: CANNOT FIGURE OUT FILE SYSTEM PARTITION\n", dev);
		return (0);
	}
	lp = getdisklabel(dev, devfd);
	if (isdigit(*cp))
		pp = &lp->d_partitions[0];
	else
		pp = &lp->d_partitions[*cp - 'a'];
	if (pp->p_fstype != FS_BSDFFS) {
		pfatal("%s: NOT LABELED AS A BSD FILE SYSTEM (%s)\n",
			dev, pp->p_fstype < FSMAXTYPES ?
			fstypenames[pp->p_fstype] : "unknown");
		return (0);
	}
	bzero((char *)fs, sizeof(struct fs));
	fs->fs_fsize = pp->p_fsize;
	fs->fs_frag = pp->p_frag;
	fs->fs_cpg = pp->p_cpg;
	fs->fs_size = pp->p_size;
	fs->fs_ntrak = lp->d_ntracks;
	fs->fs_nsect = lp->d_nsectors;
	fs->fs_spc = lp->d_secpercyl;
	fs->fs_nspf = fs->fs_fsize / lp->d_secsize;
	fs->fs_sblkno = roundup(
		howmany(lp->d_bbsize + lp->d_sbsize, fs->fs_fsize),
		fs->fs_frag);
	fs->fs_cgmask = 0xffffffff;
	for (i = fs->fs_ntrak; i > 1; i >>= 1)
		fs->fs_cgmask <<= 1;
	if (!POWEROF2(fs->fs_ntrak))
		fs->fs_cgmask <<= 1;
	fs->fs_cgoffset = roundup(
		howmany(fs->fs_nsect, NSPF(fs)), fs->fs_frag);
	fs->fs_fpg = (fs->fs_cpg * fs->fs_spc) / NSPF(fs);
	fs->fs_ncg = howmany(fs->fs_size / fs->fs_spc, fs->fs_cpg);
	for (fs->fs_fsbtodb = 0, i = NSPF(fs); i > 1; i >>= 1)
		fs->fs_fsbtodb++;
	dev_bsize = lp->d_secsize;
	return (1);
}

struct disklabel *
getdisklabel(s, fd)
	char *s;
	int	fd;
{
	static struct disklabel lab;

	if (ioctl(fd, DIOCGDINFO, (char *)&lab) < 0) {
		if (s == NULL)
			return ((struct disklabel *)NULL);
		pwarn("ioctl (GCINFO): %s\n", strerror(errno));
		errexit("%s: can't read disk label\n", s);
	}
	return (&lab);
}
