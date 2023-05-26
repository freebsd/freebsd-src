/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1980, 1986, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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

#if 0
#ifndef lint
static const char sccsid[] = "@(#)setup.c	8.10 (Berkeley) 5/9/95";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/disk.h>
#include <sys/stat.h>
#define FSTYPENAMES
#include <sys/disklabel.h>
#include <sys/file.h>
#include <sys/sysctl.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <libufs.h>

#include "fsck.h"

struct inohash *inphash;	       /* hash list of directory inode info */
struct inoinfo **inpsort;	       /* disk order list of directory inodes */
struct inode snaplist[FSMAXSNAP + 1];  /* list of active snapshots */
int snapcnt;			       /* number of active snapshots */
char *copybuf;			       /* buffer to copy snapshot blocks */

static int sbhashfailed;
#define POWEROF2(num)	(((num) & ((num) - 1)) == 0)

static int calcsb(char *dev, int devfd, struct fs *fs);
static void saverecovery(int readfd, int writefd);
static int chkrecovery(int devfd);
static int getlbnblkno(struct inodesc *);
static int checksnapinfo(struct inode *);

/*
 * Read in a superblock finding an alternate if necessary.
 * Return 1 if successful, 0 if unsuccessful, -1 if file system
 * is already clean (ckclean and preen mode only).
 */
int
setup(char *dev)
{
	long i, bmapsize;
	struct inode ip;

	/*
	 * We are expected to have an open file descriptor and a superblock.
	 */
	if (fsreadfd < 0 || havesb == 0) {
		if (debug) {
			if (fsreadfd < 0)
				printf("setup: missing fsreadfd\n");
			else
				printf("setup: missing superblock\n");
		}
		return (0);
	}
	if (preen == 0)
		printf("** %s", dev);
	if (bkgrdflag == 0 &&
	    (nflag || (fswritefd = open(dev, O_WRONLY)) < 0)) {
		fswritefd = -1;
		if (preen)
			pfatal("NO WRITE ACCESS");
		printf(" (NO WRITE)");
	}
	if (preen == 0)
		printf("\n");
	if (sbhashfailed != 0) {
		pwarn("SUPERBLOCK CHECK HASH FAILED");
		if (fswritefd == -1)
			pwarn("OPENED READONLY SO CANNOT CORRECT CHECK HASH\n");
		else if (preen || reply("CORRECT CHECK HASH") != 0) {
			if (preen)
				printf(" (CORRECTED)\n");
			sblock.fs_clean = 0;
			sbdirty();
		}
	}
	if (skipclean && ckclean && sblock.fs_clean) {
		pwarn("FILE SYSTEM CLEAN; SKIPPING CHECKS\n");
		return (-1);
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
	if (sblock.fs_magic == FS_UFS1_MAGIC &&
	    sblock.fs_old_inodefmt < FS_44INODEFMT) {
		pwarn("Format of file system is too old.\n");
		pwarn("Must update to modern format using a version of fsck\n");
		pfatal("from before 2002 with the command ``fsck -c 2''\n");
		exit(EEXIT);
	}
	if (preen == 0 && yflag == 0 && sblock.fs_magic == FS_UFS2_MAGIC &&
	    fswritefd != -1 && chkrecovery(fsreadfd) == 0 &&
	    reply("SAVE DATA TO FIND ALTERNATE SUPERBLOCKS") != 0)
		saverecovery(fsreadfd, fswritefd);
	/*
	 * allocate and initialize the necessary maps
	 */
	bufinit();
	bmapsize = roundup(howmany(maxfsblock, CHAR_BIT), sizeof(short));
	blockmap = Calloc((unsigned)bmapsize, sizeof (char));
	if (blockmap == NULL) {
		printf("cannot alloc %u bytes for blockmap\n",
		    (unsigned)bmapsize);
		goto badsb;
	}
	inostathead = Calloc(sblock.fs_ncg, sizeof(struct inostatlist));
	if (inostathead == NULL) {
		printf("cannot alloc %u bytes for inostathead\n",
		    (unsigned)(sizeof(struct inostatlist) * (sblock.fs_ncg)));
		goto badsb;
	}
	numdirs = sblock.fs_cstotal.cs_ndir;
	dirhash = MAX(numdirs / 2, 1);
	inplast = 0;
	listmax = numdirs + 10;
	inpsort = (struct inoinfo **)Calloc(listmax, sizeof(struct inoinfo *));
	inphash = (struct inohash *)Calloc(dirhash, sizeof(struct inohash));
	if (inpsort == NULL || inphash == NULL) {
		printf("cannot alloc %ju bytes for inphash\n",
		    (uintmax_t)numdirs * sizeof(struct inoinfo *));
		goto badsb;
	}
	if (sblock.fs_flags & FS_DOSOFTDEP)
		usedsoftdep = 1;
	else
		usedsoftdep = 0;
	/*
	 * Collect any snapshot inodes so that we can allow them to
	 * claim any blocks that we free. The code for doing this is
	 * imported here and into inode.c from sys/ufs/ffs/ffs_snapshot.c.
	 */
	for (snapcnt = 0; snapcnt < FSMAXSNAP; snapcnt++) {
		if (sblock.fs_snapinum[snapcnt] == 0)
			break;
		ginode(sblock.fs_snapinum[snapcnt], &ip);
		if ((DIP(ip.i_dp, di_mode) & IFMT) == IFREG &&
		    (DIP(ip.i_dp, di_flags) & SF_SNAPSHOT) != 0 &&
		    checksnapinfo(&ip)) {
			if (debug)
				printf("Load snapshot %jd\n",
				    (intmax_t)sblock.fs_snapinum[snapcnt]);
			snaplist[snapcnt] = ip;
			continue;
		}
		printf("Removing non-snapshot inode %ju from snapshot list\n",
		    (uintmax_t)sblock.fs_snapinum[snapcnt]);
		irelse(&ip);
		for (i = snapcnt + 1; i < FSMAXSNAP; i++) {
			if (sblock.fs_snapinum[i] == 0)
				break;
			sblock.fs_snapinum[i - 1] = sblock.fs_snapinum[i];
		}
		sblock.fs_snapinum[i - 1] = 0;
		snapcnt--;
		sbdirty();
	}
	if (snapcnt > 0 && copybuf == NULL) {
		copybuf = Malloc(sblock.fs_bsize);
		if (copybuf == NULL)
			errx(EEXIT, "cannot allocate space for snapshot "
			    "copy buffer");
	}
	return (1);

badsb:
	ckfini(0);
	return (0);
}

/*
 * Check for valid snapshot information.
 *
 * Each snapshot has a list of blocks that have been copied. This list
 * is consulted before checking the snapshot inode. Its purpose is to
 * speed checking of commonly checked blocks and to avoid recursive
 * checks of the snapshot inode. In particular, the list must contain
 * the superblock, the superblock summary information, and all the
 * cylinder group blocks. The list may contain other commonly checked
 * pointers such as those of the blocks that contain the snapshot inodes.
 * The list is sorted into block order to allow binary search lookup.
 *
 * The twelve direct direct block pointers of the snapshot are always
 * copied, so we test for them first before checking the list itself
 * (i.e., they are not in the list).
 *
 * The checksnapinfo() routine needs to ensure that the list contains at
 * least the super block, its summary information, and the cylinder groups.
 * Here we check the list first for the superblock, zero or more cylinder
 * groups up to the location of the superblock summary information, the
 * summary group information, and any remaining cylinder group maps that
 * follow it. We skip over any other entries in the list.
 */
#define CHKBLKINLIST(chkblk)						\
	/* All UFS_NDADDR blocks are copied */				\
	if ((chkblk) >= UFS_NDADDR) {					\
		/* Skip over blocks that are not of interest */		\
		while (*blkp < (chkblk) && blkp < lastblkp)		\
			blkp++;						\
		/* Fail if end of list and not all blocks found */	\
		if (blkp >= lastblkp) {					\
			pwarn("UFS%d snapshot inode %jd failed: "	\
			    "improper block list length (%jd)\n",	\
			    sblock.fs_magic == FS_UFS1_MAGIC ? 1 : 2,	\
			    (intmax_t)snapip->i_number,			\
			    (intmax_t)(lastblkp - &snapblklist[0]));	\
			status = 0;					\
		}							\
		/* Fail if block we seek is missing */			\
		else if (*blkp++ != (chkblk)) {				\
			pwarn("UFS%d snapshot inode %jd failed: "	\
			    "block list (%jd) != %s (%jd)\n",		\
			    sblock.fs_magic == FS_UFS1_MAGIC ? 1 : 2,	\
			    (intmax_t)snapip->i_number,			\
			    (intmax_t)blkp[-1],	#chkblk,		\
			    (intmax_t)chkblk);				\
			status = 0;					\
		}							\
	}

static int
checksnapinfo(struct inode *snapip)
{
	struct fs *fs;
	struct bufarea *bp;
	struct inodesc idesc;
	daddr_t *snapblklist, *blkp, *lastblkp, csblkno;
	int cg, loc, len, status;
	ufs_lbn_t lbn;
	size_t size;

	fs = &sblock;
	memset(&idesc, 0, sizeof(struct inodesc));
	idesc.id_type = ADDR;
	idesc.id_func = getlbnblkno;
	idesc.id_number = snapip->i_number;
	lbn = howmany(fs->fs_size, fs->fs_frag);
	idesc.id_parent = lbn;		/* sought after blkno */
	if ((ckinode(snapip->i_dp, &idesc) & FOUND) == 0)
		return (0);
	size = fragroundup(fs,
	    DIP(snapip->i_dp, di_size) - lblktosize(fs, lbn));
	bp = getdatablk(idesc.id_parent, size, BT_DATA);
	snapblklist = (daddr_t *)bp->b_un.b_buf;
	/*
	 * snapblklist[0] is the size of the list
	 * snapblklist[1] is the first element of the list
	 *
	 * We need to be careful to bound the size of the list and verify
	 * that we have not run off the end of it if it or its size has
	 * been corrupted.
	 */
	blkp = &snapblklist[1];
	lastblkp = &snapblklist[MAX(0,
	    MIN(snapblklist[0] + 1, size / sizeof(daddr_t)))];
	status = 1;
	/* Check that the superblock is listed. */
	CHKBLKINLIST(lblkno(fs, fs->fs_sblockloc));
	if (status == 0)
		goto out;
	/*
	 * Calculate where the summary information is located.
	 * Usually it is in the first cylinder group, but growfs
	 * may move it to the first cylinder group that it adds.
	 *
	 * Check all cylinder groups up to the summary information.
	 */
	csblkno = fragstoblks(fs, fs->fs_csaddr);
	for (cg = 0; cg < fs->fs_ncg; cg++) {
		if (fragstoblks(fs, cgtod(fs, cg)) > csblkno)
			break;
		CHKBLKINLIST(fragstoblks(fs, cgtod(fs, cg)));
		if (status == 0)
			goto out;
	}
	/* Check the summary information block(s). */
	len = howmany(fs->fs_cssize, fs->fs_bsize);
	for (loc = 0; loc < len; loc++) {
		CHKBLKINLIST(csblkno + loc);
		if (status == 0)
			goto out;
	}
	/* Check the remaining cylinder groups. */
	for (; cg < fs->fs_ncg; cg++) {
		CHKBLKINLIST(fragstoblks(fs, cgtod(fs, cg)));
		if (status == 0)
			goto out;
	}
out:
	brelse(bp);
	return (status);
}

/*
 * Return the block number associated with a specified inode lbn.
 * Requested lbn is in id_parent. If found, block is returned in
 * id_parent.
 */
static int
getlbnblkno(struct inodesc *idesc)
{

	if (idesc->id_lbn < idesc->id_parent)
		return (KEEPON);
	idesc->id_parent = idesc->id_blkno;
	return (STOP | FOUND);
}

/*
 * Open a device or file to be checked by fsck.
 */
int
openfilesys(char *dev)
{
	struct stat statb;
	int saved_fsreadfd;

	if (stat(dev, &statb) < 0)
		return (0);
	if ((statb.st_mode & S_IFMT) != S_IFCHR &&
	    (statb.st_mode & S_IFMT) != S_IFBLK) {
		if (bkgrdflag != 0 && (statb.st_flags & SF_SNAPSHOT) == 0) {
			pfatal("BACKGROUND FSCK LACKS A SNAPSHOT\n");
			exit(EEXIT);
		}
		if (bkgrdflag != 0) {
			cursnapshot = statb.st_ino;
		} else {
			pfatal("%s IS NOT A DISK DEVICE\n", dev);
			if (reply("CONTINUE") == 0)
				return (0);
		}
	}
	saved_fsreadfd = fsreadfd;
	if ((fsreadfd = open(dev, O_RDONLY)) < 0) {
		fsreadfd = saved_fsreadfd;
		return (0);
	}
	if (saved_fsreadfd != -1)
		close(saved_fsreadfd);
	return (1);
}

/*
 * Read in the super block and its summary info.
 */
int
readsb(void)
{
	struct fs *fs;

	sbhashfailed = 0;
	readcnt[sblk.b_type]++;
	/*
	 * If bflag is given, then check just that superblock.
	 */
	if (bflag) {
		switch (sbget(fsreadfd, &fs, bflag * dev_bsize, 0)) {
		case 0:
			goto goodsb;
		case EINTEGRITY:
			printf("Check hash failed for superblock at %jd\n",
			    bflag);
			return (0);
		case ENOENT:
			printf("%jd is not a file system superblock\n", bflag);
			return (0);
		case EIO:
		default:
			printf("I/O error reading %jd\n", bflag);
			return (0);
		}
	}
	/*
	 * Check for the standard superblock and use it if good.
	 */
	if (sbget(fsreadfd, &fs, UFS_STDSB, UFS_NOMSG) == 0)
		goto goodsb;
	/*
	 * Check if the only problem is a check-hash failure.
	 */
	skipclean = 0;
	if (sbget(fsreadfd, &fs, UFS_STDSB, UFS_NOMSG | UFS_NOHASHFAIL) == 0) {
		sbhashfailed = 1;
		goto goodsb;
	}
	/*
	 * Do an exhaustive search for a usable superblock.
	 */
	switch (sbsearch(fsreadfd, &fs, 0)) {
	case 0:
		goto goodsb;
	case ENOENT:
		printf("SEARCH FOR ALTERNATE SUPER-BLOCK FAILED. "
		    "YOU MUST USE THE\n-b OPTION TO FSCK TO SPECIFY "
		    "THE LOCATION OF AN ALTERNATE\nSUPER-BLOCK TO "
		    "SUPPLY NEEDED INFORMATION; SEE fsck_ffs(8).\n");
		return (0);
	case EIO:
	default:
		printf("I/O error reading a usable superblock\n");
		return (0);
	}

goodsb:
	memcpy(&sblock, fs, fs->fs_sbsize);
	free(fs);
	/*
	 * Compute block size that the file system is based on,
	 * according to fsbtodb, and adjust superblock block number
	 * so we can tell if this is an alternate later.
	 */
	dev_bsize = sblock.fs_fsize / fsbtodb(&sblock, 1);
	sblk.b_bno = sblock.fs_sblockactualloc / dev_bsize;
	sblk.b_size = SBLOCKSIZE;
	/*
	 * If not yet done, update UFS1 superblock with new wider fields.
	 */
	if (sblock.fs_magic == FS_UFS1_MAGIC &&
	    sblock.fs_maxbsize != sblock.fs_bsize) {
		sblock.fs_maxbsize = sblock.fs_bsize;
		sblock.fs_time = sblock.fs_old_time;
		sblock.fs_size = sblock.fs_old_size;
		sblock.fs_dsize = sblock.fs_old_dsize;
		sblock.fs_csaddr = sblock.fs_old_csaddr;
		sblock.fs_cstotal.cs_ndir = sblock.fs_old_cstotal.cs_ndir;
		sblock.fs_cstotal.cs_nbfree = sblock.fs_old_cstotal.cs_nbfree;
		sblock.fs_cstotal.cs_nifree = sblock.fs_old_cstotal.cs_nifree;
		sblock.fs_cstotal.cs_nffree = sblock.fs_old_cstotal.cs_nffree;
	}
	havesb = 1;
	return (1);
}

void
sblock_init(void)
{

	fsreadfd = -1;
	fswritefd = -1;
	fsmodified = 0;
	lfdir = 0;
	initbarea(&sblk, BT_SUPERBLK);
	sblk.b_un.b_buf = Malloc(SBLOCKSIZE);
	if (sblk.b_un.b_buf == NULL)
		errx(EEXIT, "cannot allocate space for superblock");
	dev_bsize = secsize = DEV_BSIZE;
}

/*
 * Calculate a prototype superblock based on information in the boot area.
 * When done the cgsblock macro can be calculated and the fs_ncg field
 * can be used. Do NOT attempt to use other macros without verifying that
 * their needed information is available!
 */
static int
calcsb(char *dev, int devfd, struct fs *fs)
{
	struct fsrecovery *fsr;
	char *fsrbuf;
	u_int secsize;

	/*
	 * We need fragments-per-group and the partition-size.
	 *
	 * Newfs stores these details at the end of the boot block area
	 * at the start of the filesystem partition. If they have been
	 * overwritten by a boot block, we fail. But usually they are
	 * there and we can use them.
	 */
	if (ioctl(devfd, DIOCGSECTORSIZE, &secsize) == -1)
		return (0);
	fsrbuf = Malloc(secsize);
	if (fsrbuf == NULL)
		errx(EEXIT, "calcsb: cannot allocate recovery buffer");
	if (blread(devfd, fsrbuf,
	    (SBLOCK_UFS2 - secsize) / dev_bsize, secsize) != 0) {
		free(fsrbuf);
		return (0);
	}
	fsr = (struct fsrecovery *)&fsrbuf[secsize - sizeof *fsr];
	if (fsr->fsr_magic != FS_UFS2_MAGIC) {
		free(fsrbuf);
		return (0);
	}
	memset(fs, 0, sizeof(struct fs));
	fs->fs_fpg = fsr->fsr_fpg;
	fs->fs_fsbtodb = fsr->fsr_fsbtodb;
	fs->fs_sblkno = fsr->fsr_sblkno;
	fs->fs_magic = fsr->fsr_magic;
	fs->fs_ncg = fsr->fsr_ncg;
	free(fsrbuf);
	return (1);
}

/*
 * Check to see if recovery information exists.
 * Return 1 if it exists or cannot be created.
 * Return 0 if it does not exist and can be created.
 */
static int
chkrecovery(int devfd)
{
	struct fsrecovery *fsr;
	char *fsrbuf;
	u_int secsize, rdsize;

	/*
	 * Could not determine if backup material exists, so do not
	 * offer to create it.
	 */
	fsrbuf = NULL;
	rdsize = sblock.fs_fsize;
	if (ioctl(devfd, DIOCGSECTORSIZE, &secsize) == -1 ||
	    rdsize % secsize != 0 ||
	    (fsrbuf = Malloc(rdsize)) == NULL ||
	    blread(devfd, fsrbuf, (SBLOCK_UFS2 - rdsize) / dev_bsize,
	      rdsize) != 0) {
		free(fsrbuf);
		return (1);
	}
	/*
	 * Recovery material has already been created, so do not
	 * need to create it again.
	 */
	fsr = (struct fsrecovery *)&fsrbuf[rdsize - sizeof *fsr];
	if (fsr->fsr_magic == FS_UFS2_MAGIC) {
		free(fsrbuf);
		return (1);
	}
	/*
	 * Recovery material has not been created and can be if desired.
	 */
	free(fsrbuf);
	return (0);
}

/*
 * Read the last filesystem-size piece of the boot block, replace the
 * last 20 bytes with the recovery information, then write it back.
 * The recovery information only works for UFS2 filesystems.
 */
static void
saverecovery(int readfd, int writefd)
{
	struct fsrecovery *fsr;
	char *fsrbuf;
	u_int secsize, rdsize;

	fsrbuf = NULL;
	rdsize = sblock.fs_fsize;
	if (sblock.fs_magic != FS_UFS2_MAGIC ||
	    ioctl(readfd, DIOCGSECTORSIZE, &secsize) == -1 ||
	    rdsize % secsize != 0 ||
	    (fsrbuf = Malloc(rdsize)) == NULL ||
	    blread(readfd, fsrbuf, (SBLOCK_UFS2 - rdsize) / dev_bsize,
	      rdsize) != 0) {
		printf("RECOVERY DATA COULD NOT BE CREATED\n");
		free(fsrbuf);
		return;
	}
	fsr = (struct fsrecovery *)&fsrbuf[rdsize - sizeof *fsr];
	fsr->fsr_magic = sblock.fs_magic;
	fsr->fsr_fpg = sblock.fs_fpg;
	fsr->fsr_fsbtodb = sblock.fs_fsbtodb;
	fsr->fsr_sblkno = sblock.fs_sblkno;
	fsr->fsr_ncg = sblock.fs_ncg;
	blwrite(writefd, fsrbuf, (SBLOCK_UFS2 - rdsize) / dev_bsize, rdsize);
	free(fsrbuf);
}
