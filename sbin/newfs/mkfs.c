/*
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Marshall
 * Kirk McKusick and Network Associates Laboratories, the Security
 * Research Division of Network Associates, Inc. under DARPA/SPAWAR
 * contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA CHATS
 * research program
 *
 * Copyright (c) 1982, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * Copyright (c) 1980, 1989, 1993
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
#if 0
static char sccsid[] = "@(#)mkfs.c	8.11 (Berkeley) 5/3/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <err.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>
#include <sys/disklabel.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "newfs.h"

/*
 * make filesystem for cylinder-group style filesystems
 */
#define UMASK		0755
#define POWEROF2(num)	(((num) & ((num) - 1)) == 0)

static union {
	struct fs fs;
	char pad[SBLOCKSIZE];
} fsun;
#define	sblock	fsun.fs
static struct	csum *fscs;

static union {
	struct cg cg;
	char pad[MAXBSIZE];
} cgun;
#define	acg	cgun.cg

union dinode {
	struct ufs1_dinode dp1;
	struct ufs2_dinode dp2;
};
#define DIP(dp, field) \
	((sblock.fs_magic == FS_UFS1_MAGIC) ? \
	(dp)->dp1.field : (dp)->dp2.field)

static int randinit;
static caddr_t iobuf;
static long iobufsize;
static ufs2_daddr_t alloc(int size, int mode);
static int charsperline(void);
static void clrblock(struct fs *, unsigned char *, int);
static void fsinit(time_t);
static int ilog2(int);
static void initcg(int, time_t);
static int isblock(struct fs *, unsigned char *, int);
static void iput(union dinode *, ino_t);
static int makedir(struct direct *, int);
static void rdfs(ufs2_daddr_t, int, char *);
static void setblock(struct fs *, unsigned char *, int);
static void wtfs(ufs2_daddr_t, int, char *);
static void wtfsflush(void);

void
mkfs(struct partition *pp, char *fsys)
{
	int fragsperinode, optimalfpg, origdensity, minfpg, lastminfpg;
	long i, j, cylno, csfrags;
	time_t utime;
	quad_t sizepb;
	int width;
	char tmpbuf[100];	/* XXX this will break in about 2,500 years */

	if (Rflag)
		utime = 1000000000;
	else 
		time(&utime);
	if (!Rflag && !randinit) {
		randinit = 1;
		srandomdev();
	}
	/*
	 * allocate space for superblock, cylinder group map, and
	 * two sets of inode blocks.
	 */
	if (bsize < SBLOCKSIZE)
		iobufsize = SBLOCKSIZE + 3 * bsize;
	else
		iobufsize = 4 * bsize;
	if ((iobuf = malloc(iobufsize)) == 0) {
		printf("Cannot allocate I/O buffer\n");
		exit(38);
	}
	bzero(iobuf, iobufsize);
	sblock.fs_flags = 0;
	if (Uflag)
		sblock.fs_flags |= FS_DOSOFTDEP;
	/*
	 * Validate the given filesystem size.
	 * Verify that its last block can actually be accessed.
	 * Convert to filesystem fragment sized units.
	 */
	if (fssize <= 0) {
		printf("preposterous size %jd\n", (intmax_t)fssize);
		exit(13);
	}
	wtfs(fssize - (realsectorsize / DEV_BSIZE), realsectorsize,
	    (char *)&sblock);
	/*
	 * collect and verify the filesystem density info
	 */
	sblock.fs_avgfilesize = avgfilesize;
	sblock.fs_avgfpdir = avgfilesperdir;
	if (sblock.fs_avgfilesize <= 0)
		printf("illegal expected average file size %d\n",
		    sblock.fs_avgfilesize), exit(14);
	if (sblock.fs_avgfpdir <= 0)
		printf("illegal expected number of files per directory %d\n",
		    sblock.fs_avgfpdir), exit(15);
	/*
	 * collect and verify the block and fragment sizes
	 */
	sblock.fs_bsize = bsize;
	sblock.fs_fsize = fsize;
	if (!POWEROF2(sblock.fs_bsize)) {
		printf("block size must be a power of 2, not %d\n",
		    sblock.fs_bsize);
		exit(16);
	}
	if (!POWEROF2(sblock.fs_fsize)) {
		printf("fragment size must be a power of 2, not %d\n",
		    sblock.fs_fsize);
		exit(17);
	}
	if (sblock.fs_fsize < sectorsize) {
		printf("increasing fragment size from %d to sector size (%d)\n",
		    sblock.fs_fsize, sectorsize);
		sblock.fs_fsize = sectorsize;
	}
	if (sblock.fs_bsize < MINBSIZE) {
		printf("increasing block size from %d to minimum (%d)\n",
		    sblock.fs_bsize, MINBSIZE);
		sblock.fs_bsize = MINBSIZE;
	}
	if (sblock.fs_bsize < sblock.fs_fsize) {
		printf("increasing block size from %d to fragment size (%d)\n",
		    sblock.fs_bsize, sblock.fs_fsize);
		sblock.fs_bsize = sblock.fs_fsize;
	}
	if (sblock.fs_fsize * MAXFRAG < sblock.fs_bsize) {
		printf(
		"increasing fragment size from %d to block size / %d (%d)\n",
		    sblock.fs_fsize, MAXFRAG, sblock.fs_bsize / MAXFRAG);
		sblock.fs_fsize = sblock.fs_bsize / MAXFRAG;
	}
	if (maxbsize < bsize || !POWEROF2(maxbsize)) {
		sblock.fs_maxbsize = sblock.fs_bsize;
		printf("Extent size set to %d\n", sblock.fs_maxbsize);
	} else if (sblock.fs_maxbsize > FS_MAXCONTIG * sblock.fs_bsize) {
		sblock.fs_maxbsize = FS_MAXCONTIG * sblock.fs_bsize;
		printf("Extent size reduced to %d\n", sblock.fs_maxbsize);
	} else {
		sblock.fs_maxbsize = maxbsize;
	}
	sblock.fs_maxcontig = maxcontig;
	if (sblock.fs_maxcontig < sblock.fs_maxbsize / sblock.fs_bsize) {
		sblock.fs_maxcontig = sblock.fs_maxbsize / sblock.fs_bsize;
		printf("Maxcontig raised to %d\n", sblock.fs_maxbsize);
	}
	if (sblock.fs_maxcontig > 1)
		sblock.fs_contigsumsize = MIN(sblock.fs_maxcontig,FS_MAXCONTIG);
	sblock.fs_bmask = ~(sblock.fs_bsize - 1);
	sblock.fs_fmask = ~(sblock.fs_fsize - 1);
	sblock.fs_qbmask = ~sblock.fs_bmask;
	sblock.fs_qfmask = ~sblock.fs_fmask;
	sblock.fs_bshift = ilog2(sblock.fs_bsize);
	sblock.fs_fshift = ilog2(sblock.fs_fsize);
	sblock.fs_frag = numfrags(&sblock, sblock.fs_bsize);
	sblock.fs_fragshift = ilog2(sblock.fs_frag);
	if (sblock.fs_frag > MAXFRAG) {
		printf("fragment size %d is still too small (can't happen)\n",
		    sblock.fs_bsize / MAXFRAG);
		exit(21);
	}
	sblock.fs_fsbtodb = ilog2(sblock.fs_fsize / sectorsize);
	sblock.fs_size = fssize = dbtofsb(&sblock, fssize);
	if (Oflag == 1) {
		sblock.fs_magic = FS_UFS1_MAGIC;
		sblock.fs_sblockloc = numfrags(&sblock, SBLOCK_UFS1);
		sblock.fs_nindir = sblock.fs_bsize / sizeof(ufs1_daddr_t);
		sblock.fs_inopb = sblock.fs_bsize / sizeof(struct ufs1_dinode);
		sblock.fs_maxsymlinklen = ((NDADDR + NIADDR) *
		    sizeof(ufs1_daddr_t));
		sblock.fs_old_inodefmt = FS_44INODEFMT;
		sblock.fs_old_cgoffset = 0;
		sblock.fs_old_cgmask = 0xffffffff;
		sblock.fs_old_size = sblock.fs_size;
		sblock.fs_old_rotdelay = 0;
		sblock.fs_old_rps = 60;
		sblock.fs_old_nspf = sblock.fs_fsize / sectorsize;
		sblock.fs_old_cpg = 1;
		sblock.fs_old_interleave = 1;
		sblock.fs_old_trackskew = 0;
		sblock.fs_old_cpc = 0;
		sblock.fs_old_postblformat = 1;
		sblock.fs_old_nrpos = 1;
	} else {
		sblock.fs_magic = FS_UFS2_MAGIC;
		sblock.fs_sblockloc = numfrags(&sblock, SBLOCK_UFS2);
		sblock.fs_nindir = sblock.fs_bsize / sizeof(ufs2_daddr_t);
		sblock.fs_inopb = sblock.fs_bsize / sizeof(struct ufs2_dinode);
		sblock.fs_maxsymlinklen = ((NDADDR + NIADDR) *
		    sizeof(ufs2_daddr_t));
	}
	sblock.fs_sblkno =
	    roundup(howmany(lfragtosize(&sblock, sblock.fs_sblockloc) +
		SBLOCKSIZE, sblock.fs_fsize), sblock.fs_frag);
	sblock.fs_cblkno = sblock.fs_sblkno +
	    roundup(howmany(SBLOCKSIZE, sblock.fs_fsize), sblock.fs_frag);
	sblock.fs_iblkno = sblock.fs_cblkno + sblock.fs_frag;
	sblock.fs_maxfilesize = sblock.fs_bsize * NDADDR - 1;
	for (sizepb = sblock.fs_bsize, i = 0; i < NIADDR; i++) {
		sizepb *= NINDIR(&sblock);
		sblock.fs_maxfilesize += sizepb;
	}
	/*
	 * Calculate the number of blocks to put into each cylinder group.
	 *
	 * This algorithm selects the number of blocks per cylinder
	 * group. The first goal is to have at least enough data blocks
	 * in each cylinder group to meet the density requirement. Once
	 * this goal is achieved we try to expand to have at least
	 * MINCYLGRPS cylinder groups. Once this goal is achieved, we
	 * pack as many blocks into each cylinder group map as will fit.
	 *
	 * We start by calculating the smallest number of blocks that we
	 * can put into each cylinder group. If this is too big, we reduce
	 * the density until it fits.
	 */
	origdensity = density;
	for (;;) {
		fragsperinode = numfrags(&sblock, density);
		minfpg = fragsperinode * INOPB(&sblock);
		if (minfpg > sblock.fs_size)
			minfpg = sblock.fs_size;
		sblock.fs_ipg = INOPB(&sblock);
		sblock.fs_fpg = roundup(sblock.fs_iblkno +
		    sblock.fs_ipg / INOPF(&sblock), sblock.fs_frag);
		if (sblock.fs_fpg < minfpg)
			sblock.fs_fpg = minfpg;
		sblock.fs_ipg = roundup(howmany(sblock.fs_fpg, fragsperinode),
		    INOPB(&sblock));
		sblock.fs_fpg = roundup(sblock.fs_iblkno +
		    sblock.fs_ipg / INOPF(&sblock), sblock.fs_frag);
		if (sblock.fs_fpg < minfpg)
			sblock.fs_fpg = minfpg;
		sblock.fs_ipg = roundup(howmany(sblock.fs_fpg, fragsperinode),
		    INOPB(&sblock));
		if (CGSIZE(&sblock) < (unsigned long)sblock.fs_bsize)
			break;
		density -= sblock.fs_fsize;
	}
	if (density != origdensity)
		printf("density reduced from %d to %d\n", origdensity, density);
	/*
	 * Start packing more blocks into the cylinder group until
	 * it cannot grow any larger, the number of cylinder groups
	 * drops below MINCYLGRPS, or we reach the size requested.
	 */
	for ( ; sblock.fs_fpg < maxblkspercg; sblock.fs_fpg += sblock.fs_frag) {
		sblock.fs_ipg = roundup(howmany(sblock.fs_fpg, fragsperinode),
		    INOPB(&sblock));
		if (sblock.fs_size / sblock.fs_fpg < MINCYLGRPS)
			break;
		if (CGSIZE(&sblock) < (unsigned long)sblock.fs_bsize)
			continue;
		if (CGSIZE(&sblock) == (unsigned long)sblock.fs_bsize)
			break;
		sblock.fs_fpg -= sblock.fs_frag;
		sblock.fs_ipg = roundup(howmany(sblock.fs_fpg, fragsperinode),
		    INOPB(&sblock));
		break;
	}
	/*
	 * Check to be sure that the last cylinder group has enough blocks
	 * to be viable. If it is too small, reduce the number of blocks
	 * per cylinder group which will have the effect of moving more
	 * blocks into the last cylinder group.
	 */
	optimalfpg = sblock.fs_fpg;
	for (;;) {
		sblock.fs_ncg = howmany(sblock.fs_size, sblock.fs_fpg);
		lastminfpg = roundup(sblock.fs_iblkno +
		    sblock.fs_ipg / INOPF(&sblock), sblock.fs_frag);
		if (sblock.fs_size < lastminfpg) {
			printf("Filesystem size %jd < minimum size of %d\n",
			    (intmax_t)sblock.fs_size, lastminfpg);
			exit(28);
		}
		if (sblock.fs_size % sblock.fs_fpg >= lastminfpg ||
		    sblock.fs_size % sblock.fs_fpg == 0)
			break;
		sblock.fs_fpg -= sblock.fs_frag;
		sblock.fs_ipg = roundup(howmany(sblock.fs_fpg, fragsperinode),
		    INOPB(&sblock));
	}
	if (optimalfpg != sblock.fs_fpg)
		printf("Reduced frags per cylinder group from %d to %d %s\n",
		   optimalfpg, sblock.fs_fpg, "to enlarge last cyl group");
	sblock.fs_cgsize = fragroundup(&sblock, CGSIZE(&sblock));
	sblock.fs_dblkno = sblock.fs_iblkno + sblock.fs_ipg / INOPF(&sblock);
	if (Oflag == 1) {
		sblock.fs_old_spc = sblock.fs_fpg * sblock.fs_old_nspf;
		sblock.fs_old_nsect = sblock.fs_old_spc;
		sblock.fs_old_npsect = sblock.fs_old_spc;
		sblock.fs_old_ncyl = sblock.fs_ncg;
	}
	/*
	 * fill in remaining fields of the super block
	 */
	sblock.fs_csaddr = cgdmin(&sblock, 0);
	sblock.fs_cssize =
	    fragroundup(&sblock, sblock.fs_ncg * sizeof(struct csum));
	fscs = (struct csum *)calloc(1, sblock.fs_cssize);
	if (fscs == NULL)
		errx(31, "calloc failed");
	sblock.fs_sbsize = fragroundup(&sblock, sizeof(struct fs));
	sblock.fs_minfree = minfree;
	sblock.fs_maxbpg = maxbpg;
	sblock.fs_optim = opt;
	sblock.fs_cgrotor = 0;
	sblock.fs_pendingblocks = 0;
	sblock.fs_pendinginodes = 0;
	sblock.fs_fmod = 0;
	sblock.fs_ronly = 0;
	sblock.fs_state = 0;
	sblock.fs_clean = 1;
	sblock.fs_id[0] = (long)utime;
	sblock.fs_id[1] = random();
	sblock.fs_fsmnt[0] = '\0';
	csfrags = howmany(sblock.fs_cssize, sblock.fs_fsize);
	sblock.fs_dsize = sblock.fs_size - sblock.fs_sblkno -
	    sblock.fs_ncg * (sblock.fs_dblkno - sblock.fs_sblkno);
	sblock.fs_cstotal.cs_nbfree =
	    fragstoblks(&sblock, sblock.fs_dsize) -
	    howmany(csfrags, sblock.fs_frag);
	sblock.fs_cstotal.cs_nffree =
	    fragnum(&sblock, sblock.fs_size) +
	    (csfrags > 0 ? sblock.fs_frag - csfrags : 0);
	sblock.fs_cstotal.cs_nifree = sblock.fs_ncg * sblock.fs_ipg - ROOTINO;
	sblock.fs_cstotal.cs_ndir = 0;
	sblock.fs_dsize -= csfrags;
	sblock.fs_time = utime;
	if (Oflag == 1) {
		sblock.fs_old_time = utime;
		sblock.fs_old_dsize = sblock.fs_dsize;
		sblock.fs_old_csaddr = sblock.fs_csaddr;
		sblock.fs_old_cstotal.cs_ndir = sblock.fs_cstotal.cs_ndir;
		sblock.fs_old_cstotal.cs_nbfree = sblock.fs_cstotal.cs_nbfree;
		sblock.fs_old_cstotal.cs_nifree = sblock.fs_cstotal.cs_nifree;
		sblock.fs_old_cstotal.cs_nffree = sblock.fs_cstotal.cs_nffree;
	}

	/*
	 * Dump out summary information about filesystem.
	 */
#	define B2MBFACTOR (1 / (1024.0 * 1024.0))
	printf("%s: %.1fMB (%jd sectors) block size %d, fragment size %d\n",
	    fsys, (float)sblock.fs_size * sblock.fs_fsize * B2MBFACTOR,
	    (intmax_t)fsbtodb(&sblock, sblock.fs_size), sblock.fs_bsize,
	    sblock.fs_fsize);
	printf("\tusing %d cylinder groups of %.2fMB, %d blks, %d inodes.\n",
	    sblock.fs_ncg, (float)sblock.fs_fpg * sblock.fs_fsize * B2MBFACTOR,
	    sblock.fs_fpg / sblock.fs_frag, sblock.fs_ipg);
	if (sblock.fs_flags & FS_DOSOFTDEP)
		printf("\twith soft updates\n");
#	undef B2MBFACTOR
	/*
	 * Now build the cylinders group blocks and
	 * then print out indices of cylinder groups.
	 */
	printf("super-block backups (for fsck -b #) at:\n");
	i = 0;
	width = charsperline();
	/*
	 * Make a copy of the superblock into the buffer that we will be
	 * writing out in each cylinder group.
	 */
	bcopy((char *)&sblock, iobuf, SBLOCKSIZE);
	for (cylno = 0; cylno < sblock.fs_ncg; cylno++) {
		initcg(cylno, utime);
		j = snprintf(tmpbuf, sizeof(tmpbuf), " %jd%s",
		    (intmax_t)fsbtodb(&sblock, cgsblock(&sblock, cylno)),
		    cylno < (sblock.fs_ncg-1) ? "," : "");
		if (j < 0)
			tmpbuf[j = 0] = '\0';
		if (i + j >= width) {
			printf("\n");
			i = 0;
		}
		i += j;
		printf("%s", tmpbuf);
		fflush(stdout);
	}
	printf("\n");
	if (Nflag)
		exit(0);
	/*
	 * Now construct the initial filesystem,
	 * then write out the super-block.
	 */
	fsinit(utime);
	if (Oflag == 1) {
		sblock.fs_old_cstotal.cs_ndir = sblock.fs_cstotal.cs_ndir;
		sblock.fs_old_cstotal.cs_nbfree = sblock.fs_cstotal.cs_nbfree;
		sblock.fs_old_cstotal.cs_nifree = sblock.fs_cstotal.cs_nifree;
		sblock.fs_old_cstotal.cs_nffree = sblock.fs_cstotal.cs_nffree;
	}
	wtfs(lfragtosize(&sblock, sblock.fs_sblockloc) / sectorsize,
	    SBLOCKSIZE, (char *)&sblock);
	for (i = 0; i < sblock.fs_cssize; i += sblock.fs_bsize)
		wtfs(fsbtodb(&sblock, sblock.fs_csaddr + numfrags(&sblock, i)),
			sblock.fs_cssize - i < sblock.fs_bsize ?
			sblock.fs_cssize - i : sblock.fs_bsize,
			((char *)fscs) + i);
	wtfsflush();
	/*
	 * Update information about this partion in pack
	 * label, to that it may be updated on disk.
	 */
	if (pp != NULL) {
		pp->p_fstype = FS_BSDFFS;
		pp->p_fsize = sblock.fs_fsize;
		pp->p_frag = sblock.fs_frag;
		pp->p_cpg = sblock.fs_fpg;
	}
}

/*
 * Initialize a cylinder group.
 */
void
initcg(int cylno, time_t utime)
{
	long i, j, d, dlower, dupper, blkno, start;
	ufs2_daddr_t cbase, dmax;
	struct ufs1_dinode *dp1;
	struct ufs2_dinode *dp2;
	struct csum *cs;

	/*
	 * Determine block bounds for cylinder group.
	 * Allow space for super block summary information in first
	 * cylinder group.
	 */
	cbase = cgbase(&sblock, cylno);
	dmax = cbase + sblock.fs_fpg;
	if (dmax > sblock.fs_size)
		dmax = sblock.fs_size;
	dlower = cgsblock(&sblock, cylno) - cbase;
	dupper = cgdmin(&sblock, cylno) - cbase;
	if (cylno == 0)
		dupper += howmany(sblock.fs_cssize, sblock.fs_fsize);
	cs = &fscs[cylno];
	memset(&acg, 0, sblock.fs_cgsize);
	acg.cg_time = utime;
	acg.cg_magic = CG_MAGIC;
	acg.cg_cgx = cylno;
	acg.cg_niblk = sblock.fs_ipg;
	acg.cg_initediblk = sblock.fs_ipg < 2 * INOPB(&sblock) ?
	    sblock.fs_ipg : 2 * INOPB(&sblock);
	acg.cg_ndblk = dmax - cbase;
	if (sblock.fs_contigsumsize > 0)
		acg.cg_nclusterblks = acg.cg_ndblk / sblock.fs_frag;
	start = &acg.cg_space[0] - (u_char *)(&acg.cg_firstfield);
	if (Oflag == 2) {
		acg.cg_iusedoff = start;
	} else {
		acg.cg_old_ncyl = sblock.fs_old_cpg;
		acg.cg_old_time = acg.cg_time;
		acg.cg_time = 0;
		acg.cg_old_niblk = acg.cg_niblk;
		acg.cg_niblk = 0;
		acg.cg_initediblk = 0;
		acg.cg_old_btotoff = start;
		acg.cg_old_boff = acg.cg_old_btotoff +
		    sblock.fs_old_cpg * sizeof(int32_t);
		acg.cg_iusedoff = acg.cg_old_boff +
		    sblock.fs_old_cpg * sizeof(u_int16_t);
	}
	acg.cg_freeoff = acg.cg_iusedoff + howmany(sblock.fs_ipg, NBBY);
	acg.cg_nextfreeoff = acg.cg_freeoff + howmany(sblock.fs_fpg, NBBY);
	if (sblock.fs_contigsumsize > 0) {
		acg.cg_clustersumoff =
		    roundup(acg.cg_nextfreeoff, sizeof(u_int32_t));
		acg.cg_clustersumoff -= sizeof(u_int32_t);
		acg.cg_clusteroff = acg.cg_clustersumoff +
		    (sblock.fs_contigsumsize + 1) * sizeof(u_int32_t);
		acg.cg_nextfreeoff = acg.cg_clusteroff +
		    howmany(fragstoblks(&sblock, sblock.fs_fpg), NBBY);
	}
	if (acg.cg_nextfreeoff > sblock.fs_cgsize) {
		printf("Panic: cylinder group too big\n");
		exit(37);
	}
	acg.cg_cs.cs_nifree += sblock.fs_ipg;
	if (cylno == 0)
		for (i = 0; i < (long)ROOTINO; i++) {
			setbit(cg_inosused(&acg), i);
			acg.cg_cs.cs_nifree--;
		}
	if (cylno > 0) {
		/*
		 * In cylno 0, beginning space is reserved
		 * for boot and super blocks.
		 */
		for (d = 0; d < dlower; d += sblock.fs_frag) {
			blkno = d / sblock.fs_frag;
			setblock(&sblock, cg_blksfree(&acg), blkno);
			if (sblock.fs_contigsumsize > 0)
				setbit(cg_clustersfree(&acg), blkno);
			acg.cg_cs.cs_nbfree++;
		}
	}
	if ((i = dupper % sblock.fs_frag)) {
		acg.cg_frsum[sblock.fs_frag - i]++;
		for (d = dupper + sblock.fs_frag - i; dupper < d; dupper++) {
			setbit(cg_blksfree(&acg), dupper);
			acg.cg_cs.cs_nffree++;
		}
	}
	for (d = dupper; d + sblock.fs_frag <= acg.cg_ndblk;
	     d += sblock.fs_frag) {
		blkno = d / sblock.fs_frag;
		setblock(&sblock, cg_blksfree(&acg), blkno);
		if (sblock.fs_contigsumsize > 0)
			setbit(cg_clustersfree(&acg), blkno);
		acg.cg_cs.cs_nbfree++;
	}
	if (d < acg.cg_ndblk) {
		acg.cg_frsum[acg.cg_ndblk - d]++;
		for (; d < acg.cg_ndblk; d++) {
			setbit(cg_blksfree(&acg), d);
			acg.cg_cs.cs_nffree++;
		}
	}
	if (sblock.fs_contigsumsize > 0) {
		int32_t *sump = cg_clustersum(&acg);
		u_char *mapp = cg_clustersfree(&acg);
		int map = *mapp++;
		int bit = 1;
		int run = 0;

		for (i = 0; i < acg.cg_nclusterblks; i++) {
			if ((map & bit) != 0)
				run++;
			else if (run != 0) {
				if (run > sblock.fs_contigsumsize)
					run = sblock.fs_contigsumsize;
				sump[run]++;
				run = 0;
			}
			if ((i & (NBBY - 1)) != NBBY - 1)
				bit <<= 1;
			else {
				map = *mapp++;
				bit = 1;
			}
		}
		if (run != 0) {
			if (run > sblock.fs_contigsumsize)
				run = sblock.fs_contigsumsize;
			sump[run]++;
		}
	}
	*cs = acg.cg_cs;
	/*
	 * Write out the duplicate super block, the cylinder group map
	 * and two blocks worth of inodes in a single write.
	 */
	start = sblock.fs_bsize > SBLOCKSIZE ? sblock.fs_bsize : SBLOCKSIZE;
	bcopy((char *)&acg, &iobuf[start], sblock.fs_cgsize);
	start += sblock.fs_bsize;
	dp1 = (struct ufs1_dinode *)(&iobuf[start]);
	dp2 = (struct ufs2_dinode *)(&iobuf[start]);
	for (i = 0; i < acg.cg_initediblk; i++) {
		if (sblock.fs_magic == FS_UFS1_MAGIC) {
			dp1->di_gen = random();
			dp1++;
		} else {
			dp2->di_gen = random();
			dp2++;
		}
	}
	wtfs(fsbtodb(&sblock, cgsblock(&sblock, cylno)), iobufsize, iobuf);
	/*
	 * For the old filesystem, we have to initialize all the inodes.
	 */
	if (Oflag == 1) {
		for (i = 2 * sblock.fs_frag;
		     i < sblock.fs_ipg / INOPF(&sblock);
		     i += sblock.fs_frag) {
			dp1 = (struct ufs1_dinode *)(&iobuf[start]);
			for (j = 0; j < INOPB(&sblock); j++) {
				dp1->di_gen = random();
				dp1++;
			}
			wtfs(fsbtodb(&sblock, cgimin(&sblock, cylno) + i),
			    sblock.fs_bsize, &iobuf[start]);
		}
	}
}

/*
 * initialize the filesystem
 */
#define PREDEFDIR 2

struct direct root_dir[] = {
	{ ROOTINO, sizeof(struct direct), DT_DIR, 1, "." },
	{ ROOTINO, sizeof(struct direct), DT_DIR, 2, ".." },
};

void
fsinit(time_t utime)
{
	union dinode node;

	memset(&node, 0, sizeof node);
	if (sblock.fs_magic == FS_UFS1_MAGIC) {
		/*
		 * initialize the node
		 */
		node.dp1.di_atime = utime;
		node.dp1.di_mtime = utime;
		node.dp1.di_ctime = utime;
		/*
		 * create the root directory
		 */
		node.dp1.di_mode = IFDIR | UMASK;
		node.dp1.di_nlink = PREDEFDIR;
		node.dp1.di_size = makedir(root_dir, PREDEFDIR);
		node.dp1.di_db[0] = alloc(sblock.fs_fsize, node.dp1.di_mode);
		node.dp1.di_blocks =
		    btodb(fragroundup(&sblock, node.dp1.di_size));
		wtfs(fsbtodb(&sblock, node.dp1.di_db[0]), sblock.fs_fsize,
		    iobuf);
	} else {
		/*
		 * initialize the node
		 */
		node.dp2.di_atime = utime;
		node.dp2.di_mtime = utime;
		node.dp2.di_ctime = utime;
		node.dp2.di_birthtime = utime;
		/*
		 * create the root directory
		 */
		node.dp2.di_mode = IFDIR | UMASK;
		node.dp2.di_nlink = PREDEFDIR;
		node.dp2.di_size = makedir(root_dir, PREDEFDIR);
		node.dp2.di_db[0] = alloc(sblock.fs_fsize, node.dp2.di_mode);
		node.dp2.di_blocks =
		    btodb(fragroundup(&sblock, node.dp2.di_size));
		wtfs(fsbtodb(&sblock, node.dp2.di_db[0]), sblock.fs_fsize,
		    iobuf);
	}
	iput(&node, ROOTINO);
}

/*
 * construct a set of directory entries in "iobuf".
 * return size of directory.
 */
int
makedir(struct direct *protodir, int entries)
{
	char *cp;
	int i, spcleft;

	spcleft = DIRBLKSIZ;
	memset(iobuf, 0, DIRBLKSIZ);
	for (cp = iobuf, i = 0; i < entries - 1; i++) {
		protodir[i].d_reclen = DIRSIZ(0, &protodir[i]);
		memmove(cp, &protodir[i], protodir[i].d_reclen);
		cp += protodir[i].d_reclen;
		spcleft -= protodir[i].d_reclen;
	}
	protodir[i].d_reclen = spcleft;
	memmove(cp, &protodir[i], DIRSIZ(0, &protodir[i]));
	return (DIRBLKSIZ);
}

/*
 * allocate a block or frag
 */
ufs2_daddr_t
alloc(int size, int mode)
{
	int i, d, blkno, frag;

	rdfs(fsbtodb(&sblock, cgtod(&sblock, 0)), sblock.fs_cgsize,
	    (char *)&acg);
	if (acg.cg_magic != CG_MAGIC) {
		printf("cg 0: bad magic number\n");
		return (0);
	}
	if (acg.cg_cs.cs_nbfree == 0) {
		printf("first cylinder group ran out of space\n");
		return (0);
	}
	for (d = 0; d < acg.cg_ndblk; d += sblock.fs_frag)
		if (isblock(&sblock, cg_blksfree(&acg), d / sblock.fs_frag))
			goto goth;
	printf("internal error: can't find block in cyl 0\n");
	return (0);
goth:
	blkno = fragstoblks(&sblock, d);
	clrblock(&sblock, cg_blksfree(&acg), blkno);
	if (sblock.fs_contigsumsize > 0)
		clrbit(cg_clustersfree(&acg), blkno);
	acg.cg_cs.cs_nbfree--;
	sblock.fs_cstotal.cs_nbfree--;
	fscs[0].cs_nbfree--;
	if (mode & IFDIR) {
		acg.cg_cs.cs_ndir++;
		sblock.fs_cstotal.cs_ndir++;
		fscs[0].cs_ndir++;
	}
	if (size != sblock.fs_bsize) {
		frag = howmany(size, sblock.fs_fsize);
		fscs[0].cs_nffree += sblock.fs_frag - frag;
		sblock.fs_cstotal.cs_nffree += sblock.fs_frag - frag;
		acg.cg_cs.cs_nffree += sblock.fs_frag - frag;
		acg.cg_frsum[sblock.fs_frag - frag]++;
		for (i = frag; i < sblock.fs_frag; i++)
			setbit(cg_blksfree(&acg), d + i);
	}
	wtfs(fsbtodb(&sblock, cgtod(&sblock, 0)), sblock.fs_cgsize,
	    (char *)&acg);
	return ((ufs2_daddr_t)d);
}

/*
 * Allocate an inode on the disk
 */
void
iput(union dinode *ip, ino_t ino)
{
	ufs2_daddr_t d;
	int c;

	c = ino_to_cg(&sblock, ino);
	rdfs(fsbtodb(&sblock, cgtod(&sblock, 0)), sblock.fs_cgsize,
	    (char *)&acg);
	if (acg.cg_magic != CG_MAGIC) {
		printf("cg 0: bad magic number\n");
		exit(31);
	}
	acg.cg_cs.cs_nifree--;
	setbit(cg_inosused(&acg), ino);
	wtfs(fsbtodb(&sblock, cgtod(&sblock, 0)), sblock.fs_cgsize,
	    (char *)&acg);
	sblock.fs_cstotal.cs_nifree--;
	fscs[0].cs_nifree--;
	if (ino >= (unsigned long)sblock.fs_ipg * sblock.fs_ncg) {
		printf("fsinit: inode value out of range (%d).\n", ino);
		exit(32);
	}
	d = fsbtodb(&sblock, ino_to_fsba(&sblock, ino));
	rdfs(d, sblock.fs_bsize, (char *)iobuf);
	if (sblock.fs_magic == FS_UFS1_MAGIC)
		((struct ufs1_dinode *)iobuf)[ino_to_fsbo(&sblock, ino)] =
		    ip->dp1;
	else
		((struct ufs2_dinode *)iobuf)[ino_to_fsbo(&sblock, ino)] =
		    ip->dp2;
	wtfs(d, sblock.fs_bsize, (char *)iobuf);
}

/*
 * read a block from the filesystem
 */
void
rdfs(ufs2_daddr_t bno, int size, char *bf)
{
	int n;

	wtfsflush();
	if (lseek(fso, (off_t)bno * sectorsize, 0) < 0) {
		printf("seek error: %ld\n", (long)bno);
		err(33, "rdfs");
	}
	n = read(fso, bf, size);
	if (n != size) {
		printf("read error: %ld\n", (long)bno);
		err(34, "rdfs");
	}
}

#define WCSIZE (128 * 1024)
ufs2_daddr_t wc_sect;		/* units of sectorsize */
int wc_end;			/* bytes */
static char wc[WCSIZE];		/* bytes */

/*
 * Flush dirty write behind buffer.
 */
static void
wtfsflush()
{
	int n;
	if (wc_end) {
		if (lseek(fso, (off_t)wc_sect * sectorsize, SEEK_SET) < 0) {
			printf("seek error: %ld\n", (long)wc_sect);
			err(35, "wtfs - writecombine");
		}
		n = write(fso, wc, wc_end);
		if (n != wc_end) {
			printf("write error: %ld\n", (long)wc_sect);
			err(36, "wtfs - writecombine");
		}
		wc_end = 0;
	}
}

/*
 * write a block to the filesystem
 */
static void
wtfs(ufs2_daddr_t bno, int size, char *bf)
{
	int done, n;

	if (Nflag)
		return;
	done = 0;
	if (wc_end == 0 && size <= WCSIZE) {
		wc_sect = bno;
		bcopy(bf, wc, size);
		wc_end = size;
		if (wc_end < WCSIZE)
			return;
		done = 1;
	}
	if ((off_t)wc_sect * sectorsize + wc_end == (off_t)bno * sectorsize &&
	    wc_end + size <= WCSIZE) {
		bcopy(bf, wc + wc_end, size);
		wc_end += size;
		if (wc_end < WCSIZE)
			return;
		done = 1;
	}
	wtfsflush();
	if (done)
		return;
	if (lseek(fso, (off_t)bno * sectorsize, SEEK_SET) < 0) {
		printf("seek error: %ld\n", (long)bno);
		err(35, "wtfs");
	}
	n = write(fso, bf, size);
	if (n != size) {
		printf("write error: %ld\n", (long)bno);
		err(36, "wtfs");
	}
}

/*
 * check if a block is available
 */
static int
isblock(struct fs *fs, unsigned char *cp, int h)
{
	unsigned char mask;

	switch (fs->fs_frag) {
	case 8:
		return (cp[h] == 0xff);
	case 4:
		mask = 0x0f << ((h & 0x1) << 2);
		return ((cp[h >> 1] & mask) == mask);
	case 2:
		mask = 0x03 << ((h & 0x3) << 1);
		return ((cp[h >> 2] & mask) == mask);
	case 1:
		mask = 0x01 << (h & 0x7);
		return ((cp[h >> 3] & mask) == mask);
	default:
		fprintf(stderr, "isblock bad fs_frag %d\n", fs->fs_frag);
		return (0);
	}
}

/*
 * take a block out of the map
 */
static void
clrblock(struct fs *fs, unsigned char *cp, int h)
{
	switch ((fs)->fs_frag) {
	case 8:
		cp[h] = 0;
		return;
	case 4:
		cp[h >> 1] &= ~(0x0f << ((h & 0x1) << 2));
		return;
	case 2:
		cp[h >> 2] &= ~(0x03 << ((h & 0x3) << 1));
		return;
	case 1:
		cp[h >> 3] &= ~(0x01 << (h & 0x7));
		return;
	default:
		fprintf(stderr, "clrblock bad fs_frag %d\n", fs->fs_frag);
		return;
	}
}

/*
 * put a block into the map
 */
static void
setblock(struct fs *fs, unsigned char *cp, int h)
{
	switch (fs->fs_frag) {
	case 8:
		cp[h] = 0xff;
		return;
	case 4:
		cp[h >> 1] |= (0x0f << ((h & 0x1) << 2));
		return;
	case 2:
		cp[h >> 2] |= (0x03 << ((h & 0x3) << 1));
		return;
	case 1:
		cp[h >> 3] |= (0x01 << (h & 0x7));
		return;
	default:
		fprintf(stderr, "setblock bad fs_frag %d\n", fs->fs_frag);
		return;
	}
}

/*
 * Determine the number of characters in a
 * single line.
 */

static int
charsperline(void)
{
	int columns;
	char *cp;
	struct winsize ws;

	columns = 0;
	if (ioctl(0, TIOCGWINSZ, &ws) != -1)
		columns = ws.ws_col;
	if (columns == 0 && (cp = getenv("COLUMNS")))
		columns = atoi(cp);
	if (columns == 0)
		columns = 80;	/* last resort */
	return (columns);
}

static int
ilog2(int val)
{
	u_int n;

	for (n = 0; n < sizeof(n) * NBBY; n++)
		if (1 << n == val)
			return (n);
	errx(1, "ilog2: %d is not a power of 2\n", val);
}
