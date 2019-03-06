/*
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

#if 0
#ifndef lint
static const char sccsid[] = "@(#)setup.c	8.10 (Berkeley) 5/9/95";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
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

#include "fsck.h"

struct bufarea asblk;
#define altsblock (*asblk.b_un.b_fs)
#define POWEROF2(num)	(((num) & ((num) - 1)) == 0)

static int calcsb(char *dev, int devfd, struct fs *fs);
static void saverecovery(int readfd, int writefd);
static int chkrecovery(int devfd);

/*
 * Read in a superblock finding an alternate if necessary.
 * Return 1 if successful, 0 if unsuccessful, -1 if file system
 * is already clean (ckclean and preen mode only).
 */
int
setup(char *dev)
{
	long cg, asked, i, j;
	long bmapsize;
	struct stat statb;
	struct fs proto;
	size_t size;

	havesb = 0;
	fswritefd = -1;
	cursnapshot = 0;
	if (stat(dev, &statb) < 0) {
		printf("Can't stat %s: %s\n", dev, strerror(errno));
		if (bkgrdflag) {
			unlink(snapname);
			bkgrdflag = 0;
		}
		return (0);
	}
	if ((statb.st_mode & S_IFMT) != S_IFCHR &&
	    (statb.st_mode & S_IFMT) != S_IFBLK) {
		if (bkgrdflag != 0 && (statb.st_flags & SF_SNAPSHOT) == 0) {
			unlink(snapname);
			printf("background fsck lacks a snapshot\n");
			exit(EEXIT);
		}
		if ((statb.st_flags & SF_SNAPSHOT) != 0 && cvtlevel == 0) {
			cursnapshot = statb.st_ino;
		} else {
			if (cvtlevel == 0 ||
			    (statb.st_flags & SF_SNAPSHOT) == 0) {
				if (preen && bkgrdflag) {
					unlink(snapname);
					bkgrdflag = 0;
				}
				pfatal("%s is not a disk device", dev);
				if (reply("CONTINUE") == 0) {
					if (bkgrdflag) {
						unlink(snapname);
						bkgrdflag = 0;
					}
					return (0);
				}
			} else {
				if (bkgrdflag) {
					unlink(snapname);
					bkgrdflag = 0;
				}
				pfatal("cannot convert a snapshot");
				exit(EEXIT);
			}
		}
	}
	if ((fsreadfd = open(dev, O_RDONLY)) < 0) {
		if (bkgrdflag) {
			unlink(snapname);
			bkgrdflag = 0;
		}
		printf("Can't open %s: %s\n", dev, strerror(errno));
		return (0);
	}
	if (bkgrdflag) {
		unlink(snapname);
		size = MIBSIZE;
		if (sysctlnametomib("vfs.ffs.adjrefcnt", adjrefcnt, &size) < 0||
		    sysctlnametomib("vfs.ffs.adjblkcnt", adjblkcnt, &size) < 0||
		    sysctlnametomib("vfs.ffs.setsize", setsize, &size) < 0 ||
		    sysctlnametomib("vfs.ffs.freefiles", freefiles, &size) < 0||
		    sysctlnametomib("vfs.ffs.freedirs", freedirs, &size) < 0 ||
		    sysctlnametomib("vfs.ffs.freeblks", freeblks, &size) < 0) {
			pfatal("kernel lacks background fsck support\n");
			exit(EEXIT);
		}
		/*
		 * When kernel is lack of runtime bgfsck superblock summary
		 * adjustment functionality, it does not mean we can not
		 * continue, as old kernels will recompute the summary at
		 * mount time.  However, it will be an unexpected softupdates
		 * inconsistency if it turns out that the summary is still
		 * incorrect.  Set a flag so subsequent operation can know
		 * this.
		 */
		bkgrdsumadj = 1;
		if (sysctlnametomib("vfs.ffs.adjndir", adjndir, &size) < 0 ||
		    sysctlnametomib("vfs.ffs.adjnbfree", adjnbfree, &size) < 0 ||
		    sysctlnametomib("vfs.ffs.adjnifree", adjnifree, &size) < 0 ||
		    sysctlnametomib("vfs.ffs.adjnffree", adjnffree, &size) < 0 ||
		    sysctlnametomib("vfs.ffs.adjnumclusters", adjnumclusters, &size) < 0) {
			bkgrdsumadj = 0;
			pwarn("kernel lacks runtime superblock summary adjustment support");
		}
		cmd.version = FFS_CMD_VERSION;
		cmd.handle = fsreadfd;
		fswritefd = -1;
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
	/*
	 * Read in the superblock, looking for alternates if necessary
	 */
	if (readsb(1) == 0) {
		skipclean = 0;
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
				"INFORMATION; SEE fsck_ffs(8).");
			bflag = 0;
			return(0);
		}
		pwarn("USING ALTERNATE SUPERBLOCK AT %jd\n", bflag);
		bflag = 0;
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
	if (asblk.b_dirty && !bflag) {
		memmove(&altsblock, &sblock, (size_t)sblock.fs_sbsize);
		flush(fswritefd, &asblk);
	}
	if (preen == 0 && yflag == 0 && sblock.fs_magic == FS_UFS2_MAGIC &&
	    fswritefd != -1 && chkrecovery(fsreadfd) == 0 &&
	    reply("SAVE DATA TO FIND ALTERNATE SUPERBLOCKS") != 0)
		saverecovery(fsreadfd, fswritefd);
	/*
	 * read in the summary info.
	 */
	asked = 0;
	sblock.fs_csp = Calloc(1, sblock.fs_cssize);
	if (sblock.fs_csp == NULL) {
		printf("cannot alloc %u bytes for cg summary info\n",
		    (unsigned)sblock.fs_cssize);
		goto badsb;
	}
	for (i = 0, j = 0; i < sblock.fs_cssize; i += sblock.fs_bsize, j++) {
		size = MIN(sblock.fs_cssize - i, sblock.fs_bsize);
		readcnt[sblk.b_type]++;
		if (blread(fsreadfd, (char *)sblock.fs_csp + i,
		    fsbtodb(&sblock, sblock.fs_csaddr + j * sblock.fs_frag),
		    size) != 0 && !asked) {
			pfatal("BAD SUMMARY INFORMATION");
			if (reply("CONTINUE") == 0) {
				ckfini(0);
				exit(EEXIT);
			}
			asked++;
		}
	}
	/*
	 * allocate and initialize the necessary maps
	 */
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
	numdirs = MAX(sblock.fs_cstotal.cs_ndir, 128);
	dirhash = numdirs;
	inplast = 0;
	listmax = numdirs + 10;
	inpsort = (struct inoinfo **)Calloc(listmax, sizeof(struct inoinfo *));
	inphead = (struct inoinfo **)Calloc(numdirs, sizeof(struct inoinfo *));
	if (inpsort == NULL || inphead == NULL) {
		printf("cannot alloc %ju bytes for inphead\n",
		    (uintmax_t)numdirs * sizeof(struct inoinfo *));
		goto badsb;
	}
	bufinit();
	if (sblock.fs_flags & FS_DOSOFTDEP)
		usedsoftdep = 1;
	else
		usedsoftdep = 0;
	return (1);

badsb:
	ckfini(0);
	return (0);
}

/*
 * Possible superblock locations ordered from most to least likely.
 */
static int sblock_try[] = SBLOCKSEARCH;

#define BAD_MAGIC_MSG \
"The previous newfs operation on this volume did not complete.\n" \
"You must complete newfs before mounting this volume.\n"

/*
 * Read in the super block and its summary info.
 */
int
readsb(int listerr)
{
	ufs2_daddr_t super;
	int i, bad;

	if (bflag) {
		super = bflag;
		readcnt[sblk.b_type]++;
		if ((blread(fsreadfd, (char *)&sblock, super, (long)SBLOCKSIZE)))
			return (0);
		if (sblock.fs_magic == FS_BAD_MAGIC) {
			fprintf(stderr, BAD_MAGIC_MSG);
			exit(11);
		}
		if (sblock.fs_magic != FS_UFS1_MAGIC &&
		    sblock.fs_magic != FS_UFS2_MAGIC) {
			fprintf(stderr, "%jd is not a file system superblock\n",
			    bflag);
			return (0);
		}
	} else {
		for (i = 0; sblock_try[i] != -1; i++) {
			super = sblock_try[i] / dev_bsize;
			readcnt[sblk.b_type]++;
			if ((blread(fsreadfd, (char *)&sblock, super,
			    (long)SBLOCKSIZE)))
				return (0);
			if (sblock.fs_magic == FS_BAD_MAGIC) {
				fprintf(stderr, BAD_MAGIC_MSG);
				exit(11);
			}
			if ((sblock.fs_magic == FS_UFS1_MAGIC ||
			     (sblock.fs_magic == FS_UFS2_MAGIC &&
			      sblock.fs_sblockloc == sblock_try[i])) &&
			    sblock.fs_ncg >= 1 &&
			    sblock.fs_bsize >= MINBSIZE &&
			    sblock.fs_sbsize >= roundup(sizeof(struct fs), dev_bsize))
				break;
		}
		if (sblock_try[i] == -1) {
			fprintf(stderr, "Cannot find file system superblock\n");
			return (0);
		}
	}
	/*
	 * Compute block size that the file system is based on,
	 * according to fsbtodb, and adjust superblock block number
	 * so we can tell if this is an alternate later.
	 */
	super *= dev_bsize;
	dev_bsize = sblock.fs_fsize / fsbtodb(&sblock, 1);
	sblk.b_bno = super / dev_bsize;
	sblk.b_size = SBLOCKSIZE;
	/*
	 * Compare all fields that should not differ in alternate super block.
	 * When an alternate super-block is specified this check is skipped.
	 */
	if (bflag)
		goto out;
	getblk(&asblk, cgsblock(&sblock, sblock.fs_ncg - 1), sblock.fs_sbsize);
	if (asblk.b_errs)
		return (0);
	bad = 0;
#define CHK(x, y)				\
	if (altsblock.x != sblock.x) {		\
		bad++;				\
		if (listerr && debug)		\
			printf("SUPER BLOCK VS ALTERNATE MISMATCH %s: " y " vs " y "\n", \
			    #x, (intmax_t)sblock.x, (intmax_t)altsblock.x); \
	}
	CHK(fs_sblkno, "%jd");
	CHK(fs_cblkno, "%jd");
	CHK(fs_iblkno, "%jd");
	CHK(fs_dblkno, "%jd");
	CHK(fs_ncg, "%jd");
	CHK(fs_bsize, "%jd");
	CHK(fs_fsize, "%jd");
	CHK(fs_frag, "%jd");
	CHK(fs_bmask, "%#jx");
	CHK(fs_fmask, "%#jx");
	CHK(fs_bshift, "%jd");
	CHK(fs_fshift, "%jd");
	CHK(fs_fragshift, "%jd");
	CHK(fs_fsbtodb, "%jd");
	CHK(fs_sbsize, "%jd");
	CHK(fs_nindir, "%jd");
	CHK(fs_inopb, "%jd");
	CHK(fs_cssize, "%jd");
	CHK(fs_ipg, "%jd");
	CHK(fs_fpg, "%jd");
	CHK(fs_magic, "%#jx");
#undef CHK
	if (bad) {
		if (listerr == 0)
			return (0);
		if (preen)
			printf("%s: ", cdevname);
		printf(
		    "VALUES IN SUPER BLOCK LSB=%jd DISAGREE WITH THOSE IN\n"
		    "LAST ALTERNATE LSB=%jd\n",
		    sblk.b_bno, asblk.b_bno);
		if (reply("IGNORE ALTERNATE SUPER BLOCK") == 0)
			return (0);
	}
out:
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

	fswritefd = -1;
	fsmodified = 0;
	lfdir = 0;
	initbarea(&sblk, BT_SUPERBLK);
	initbarea(&asblk, BT_SUPERBLK);
	sblk.b_un.b_buf = Malloc(SBLOCKSIZE);
	asblk.b_un.b_buf = Malloc(SBLOCKSIZE);
	if (sblk.b_un.b_buf == NULL || asblk.b_un.b_buf == NULL)
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
	struct fsrecovery fsr;

	/*
	 * We need fragments-per-group and the partition-size.
	 *
	 * Newfs stores these details at the end of the boot block area
	 * at the start of the filesystem partition. If they have been
	 * overwritten by a boot block, we fail. But usually they are
	 * there and we can use them.
	 */
	if (blread(devfd, (char *)&fsr,
	    (SBLOCK_UFS2 - sizeof(fsr)) / dev_bsize, sizeof(fsr)) ||
	    fsr.fsr_magic != FS_UFS2_MAGIC)
		return (0);
	memset(fs, 0, sizeof(struct fs));
	fs->fs_fpg = fsr.fsr_fpg;
	fs->fs_fsbtodb = fsr.fsr_fsbtodb;
	fs->fs_sblkno = fsr.fsr_sblkno;
	fs->fs_magic = fsr.fsr_magic;
	fs->fs_ncg = fsr.fsr_ncg;
	return (1);
}

/*
 * Check to see if recovery information exists.
 */
static int
chkrecovery(int devfd)
{
	struct fsrecovery fsr;

	if (blread(devfd, (char *)&fsr,
	    (SBLOCK_UFS2 - sizeof(fsr)) / dev_bsize, sizeof(fsr)) ||
	    fsr.fsr_magic != FS_UFS2_MAGIC)
		return (0);
	return (1);
}

/*
 * Read the last sector of the boot block, replace the last
 * 20 bytes with the recovery information, then write it back.
 * The recovery information only works for UFS2 filesystems.
 */
static void
saverecovery(int readfd, int writefd)
{
	struct fsrecovery fsr;

	if (sblock.fs_magic != FS_UFS2_MAGIC ||
	    blread(readfd, (char *)&fsr,
	    (SBLOCK_UFS2 - sizeof(fsr)) / dev_bsize, sizeof(fsr)))
		return;
	fsr.fsr_magic = sblock.fs_magic;
	fsr.fsr_fpg = sblock.fs_fpg;
	fsr.fsr_fsbtodb = sblock.fs_fsbtodb;
	fsr.fsr_sblkno = sblock.fs_sblkno;
	fsr.fsr_ncg = sblock.fs_ncg;
	blwrite(writefd, (char *)&fsr, (SBLOCK_UFS2 - sizeof(fsr)) / dev_bsize,
	    sizeof(fsr));
}
