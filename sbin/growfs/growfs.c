/*
 * Copyright (c) 2000 Christoph Herrmann, Thomas-Henning von Kamptz
 * Copyright (c) 1980, 1989, 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christoph Herrmann and Thomas-Henning von Kamptz, Munich and Frankfurt.
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
 *    must display the following acknowledgment:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors, as well as Christoph
 *      Herrmann and Thomas-Henning von Kamptz.
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
 * $TSHeader: src/sbin/growfs/growfs.c,v 1.5 2000/12/12 19:31:00 tomsoft Exp $
 *
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 2000 Christoph Herrmann, Thomas-Henning von Kamptz\n\
Copyright (c) 1980, 1989, 1993 The Regents of the University of California.\n\
All rights reserved.\n";
#endif /* not lint */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/disk.h>

#include <stdio.h>
#include <paths.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include "debug.h"

#ifdef FS_DEBUG
int	_dbg_lvl_ = (DL_INFO);	/* DL_TRC */
#endif /* FS_DEBUG */

static union {
	struct fs	fs;
	char	pad[SBLOCKSIZE];
} fsun1, fsun2;
#define	sblock	fsun1.fs	/* the new superblock */
#define	osblock	fsun2.fs	/* the old superblock */

/*
 * Possible superblock locations ordered from most to least likely.
 */
static int sblock_try[] = SBLOCKSEARCH;
static ufs2_daddr_t sblockloc;

static union {
	struct cg	cg;
	char	pad[MAXBSIZE];
} cgun1, cgun2;
#define	acg	cgun1.cg	/* a cylinder cgroup (new) */
#define	aocg	cgun2.cg	/* an old cylinder group */

static struct csum	*fscs;	/* cylinder summary */

static void	growfs(int, int, unsigned int);
static void	rdfs(ufs2_daddr_t, size_t, void *, int);
static void	wtfs(ufs2_daddr_t, size_t, void *, int, unsigned int);
static int	charsperline(void);
static void	usage(void);
static int	isblock(struct fs *, unsigned char *, int);
static void	clrblock(struct fs *, unsigned char *, int);
static void	setblock(struct fs *, unsigned char *, int);
static void	initcg(int, time_t, int, unsigned int);
static void	updjcg(int, time_t, int, int, unsigned int);
static void	updcsloc(time_t, int, int, unsigned int);
static void	frag_adjust(ufs2_daddr_t, int);
static void	updclst(int);
static void	get_dev_size(int, int *);

/*
 * Here we actually start growing the file system. We basically read the
 * cylinder summary from the first cylinder group as we want to update
 * this on the fly during our various operations. First we handle the
 * changes in the former last cylinder group. Afterwards we create all new
 * cylinder groups.  Now we handle the cylinder group containing the
 * cylinder summary which might result in a relocation of the whole
 * structure.  In the end we write back the updated cylinder summary, the
 * new superblock, and slightly patched versions of the super block
 * copies.
 */
static void
growfs(int fsi, int fso, unsigned int Nflag)
{
	DBG_FUNC("growfs")
	time_t modtime;
	uint cylno;
	int i, j, width;
	char tmpbuf[100];
#ifdef FSIRAND
	static int randinit=0;

	DBG_ENTER;

	if (!randinit) {
		randinit = 1;
		srandomdev();
	}
#else /* not FSIRAND */

	DBG_ENTER;

#endif /* FSIRAND */
	time(&modtime);

	/*
	 * Get the cylinder summary into the memory.
	 */
	fscs = (struct csum *)calloc((size_t)1, (size_t)sblock.fs_cssize);
	if (fscs == NULL)
		errx(1, "calloc failed");
	for (i = 0; i < osblock.fs_cssize; i += osblock.fs_bsize) {
		rdfs(fsbtodb(&osblock, osblock.fs_csaddr +
		    numfrags(&osblock, i)), (size_t)MIN(osblock.fs_cssize - i,
		    osblock.fs_bsize), (void *)(((char *)fscs) + i), fsi);
	}

#ifdef FS_DEBUG
	{
		struct csum *dbg_csp;
		int dbg_csc;
		char dbg_line[80];

		dbg_csp = fscs;

		for (dbg_csc = 0; dbg_csc < osblock.fs_ncg; dbg_csc++) {
			snprintf(dbg_line, sizeof(dbg_line),
			    "%d. old csum in old location", dbg_csc);
			DBG_DUMP_CSUM(&osblock, dbg_line, dbg_csp++);
		}
	}
#endif /* FS_DEBUG */
	DBG_PRINT0("fscs read\n");

	/*
	 * Do all needed changes in the former last cylinder group.
	 */
	updjcg(osblock.fs_ncg - 1, modtime, fsi, fso, Nflag);

	/*
	 * Dump out summary information about file system.
	 */
#	define B2MBFACTOR (1 / (1024.0 * 1024.0))
	printf("growfs: %.1fMB (%jd sectors) block size %d, fragment size %d\n",
	    (float)sblock.fs_size * sblock.fs_fsize * B2MBFACTOR,
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
	 * Iterate for only the new cylinder groups.
	 */
	for (cylno = osblock.fs_ncg; cylno < sblock.fs_ncg; cylno++) {
		initcg(cylno, modtime, fso, Nflag);
		j = sprintf(tmpbuf, " %jd%s",
		    (intmax_t)fsbtodb(&sblock, cgsblock(&sblock, cylno)),
		    cylno < (sblock.fs_ncg - 1) ? "," : "" );
		if (i + j >= width) {
			printf("\n");
			i = 0;
		}
		i += j;
		printf("%s", tmpbuf);
		fflush(stdout);
	}
	printf("\n");

	/*
	 * Do all needed changes in the first cylinder group.
	 * allocate blocks in new location
	 */
	updcsloc(modtime, fsi, fso, Nflag);

	/*
	 * Now write the cylinder summary back to disk.
	 */
	for (i = 0; i < sblock.fs_cssize; i += sblock.fs_bsize) {
		wtfs(fsbtodb(&sblock, sblock.fs_csaddr + numfrags(&sblock, i)),
		    (size_t)MIN(sblock.fs_cssize - i, sblock.fs_bsize),
		    (void *)(((char *)fscs) + i), fso, Nflag);
	}
	DBG_PRINT0("fscs written\n");

#ifdef FS_DEBUG
	{
		struct csum	*dbg_csp;
		int	dbg_csc;
		char	dbg_line[80];

		dbg_csp = fscs;
		for (dbg_csc = 0; dbg_csc < sblock.fs_ncg; dbg_csc++) {
			snprintf(dbg_line, sizeof(dbg_line),
			    "%d. new csum in new location", dbg_csc);
			DBG_DUMP_CSUM(&sblock, dbg_line, dbg_csp++);
		}
	}
#endif /* FS_DEBUG */

	/*
	 * Now write the new superblock back to disk.
	 */
	sblock.fs_time = modtime;
	wtfs(sblockloc, (size_t)SBLOCKSIZE, (void *)&sblock, fso, Nflag);
	DBG_PRINT0("sblock written\n");
	DBG_DUMP_FS(&sblock, "new initial sblock");

	/*
	 * Clean up the dynamic fields in our superblock copies.
	 */
	sblock.fs_fmod = 0;
	sblock.fs_clean = 1;
	sblock.fs_ronly = 0;
	sblock.fs_cgrotor = 0;
	sblock.fs_state = 0;
	memset((void *)&sblock.fs_fsmnt, 0, sizeof(sblock.fs_fsmnt));
	sblock.fs_flags &= FS_DOSOFTDEP;

	/*
	 * XXX
	 * The following fields are currently distributed from the superblock
	 * to the copies:
	 *     fs_minfree
	 *     fs_rotdelay
	 *     fs_maxcontig
	 *     fs_maxbpg
	 *     fs_minfree,
	 *     fs_optim
	 *     fs_flags regarding SOFTPDATES
	 *
	 * We probably should rather change the summary for the cylinder group
	 * statistics here to the value of what would be in there, if the file
	 * system were created initially with the new size. Therefor we still
	 * need to find an easy way of calculating that.
	 * Possibly we can try to read the first superblock copy and apply the
	 * "diffed" stats between the old and new superblock by still copying
	 * certain parameters onto that.
	 */

	/*
	 * Write out the duplicate super blocks.
	 */
	for (cylno = 0; cylno < sblock.fs_ncg; cylno++) {
		wtfs(fsbtodb(&sblock, cgsblock(&sblock, cylno)),
		    (size_t)SBLOCKSIZE, (void *)&sblock, fso, Nflag);
	}
	DBG_PRINT0("sblock copies written\n");
	DBG_DUMP_FS(&sblock, "new other sblocks");

	DBG_LEAVE;
	return;
}

/*
 * This creates a new cylinder group structure, for more details please see
 * the source of newfs(8), as this function is taken over almost unchanged.
 * As this is never called for the first cylinder group, the special
 * provisions for that case are removed here.
 */
static void
initcg(int cylno, time_t modtime, int fso, unsigned int Nflag)
{
	DBG_FUNC("initcg")
	static caddr_t iobuf;
	long blkno, start;
	ufs2_daddr_t i, cbase, dmax;
#ifdef FSIRAND
	struct ufs1_dinode *dp1;
#endif
	struct csum *cs;
	uint d, dupper, dlower;

	if (iobuf == NULL && (iobuf = malloc(sblock.fs_bsize * 3)) == NULL)
		errx(37, "panic: cannot allocate I/O buffer");

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
	if (cylno == 0)	/* XXX fscs may be relocated */
		dupper += howmany(sblock.fs_cssize, sblock.fs_fsize);
	cs = &fscs[cylno];
	memset(&acg, 0, sblock.fs_cgsize);
	acg.cg_time = modtime;
	acg.cg_magic = CG_MAGIC;
	acg.cg_cgx = cylno;
	acg.cg_niblk = sblock.fs_ipg;
	acg.cg_initediblk = sblock.fs_ipg < 2 * INOPB(&sblock) ?
	    sblock.fs_ipg : 2 * INOPB(&sblock);
	acg.cg_ndblk = dmax - cbase;
	if (sblock.fs_contigsumsize > 0)
		acg.cg_nclusterblks = acg.cg_ndblk / sblock.fs_frag;
	start = &acg.cg_space[0] - (u_char *)(&acg.cg_firstfield);
	if (sblock.fs_magic == FS_UFS2_MAGIC) {
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
	acg.cg_freeoff = acg.cg_iusedoff + howmany(sblock.fs_ipg, CHAR_BIT);
	acg.cg_nextfreeoff = acg.cg_freeoff + howmany(sblock.fs_fpg, CHAR_BIT);
	if (sblock.fs_contigsumsize > 0) {
		acg.cg_clustersumoff =
		    roundup(acg.cg_nextfreeoff, sizeof(u_int32_t));
		acg.cg_clustersumoff -= sizeof(u_int32_t);
		acg.cg_clusteroff = acg.cg_clustersumoff +
		    (sblock.fs_contigsumsize + 1) * sizeof(u_int32_t);
		acg.cg_nextfreeoff = acg.cg_clusteroff +
		    howmany(fragstoblks(&sblock, sblock.fs_fpg), CHAR_BIT);
	}
	if (acg.cg_nextfreeoff > (unsigned)sblock.fs_cgsize) {
		/*
		 * This should never happen as we would have had that panic
		 * already on file system creation
		 */
		errx(37, "panic: cylinder group too big");
	}
	acg.cg_cs.cs_nifree += sblock.fs_ipg;
	if (cylno == 0)
		for (i = 0; i < ROOTINO; i++) {
			setbit(cg_inosused(&acg), i);
			acg.cg_cs.cs_nifree--;
		}
	/*
	 * For the old file system, we have to initialize all the inodes.
	 */
	if (sblock.fs_magic == FS_UFS1_MAGIC) {
		bzero(iobuf, sblock.fs_bsize);
		for (i = 0; i < sblock.fs_ipg / INOPF(&sblock);
		    i += sblock.fs_frag) {
#ifdef FSIRAND
			dp1 = (struct ufs1_dinode *)(void *)iobuf;
			for (j = 0; j < INOPB(&sblock); j++) {
				dp1->di_gen = random();
				dp1++;
			}
#endif
			wtfs(fsbtodb(&sblock, cgimin(&sblock, cylno) + i),
			    sblock.fs_bsize, iobuf, fso, Nflag);
		}
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
		sblock.fs_dsize += dlower;
	}
	sblock.fs_dsize += acg.cg_ndblk - dupper;
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
			if ((i & (CHAR_BIT - 1)) != CHAR_BIT - 1)
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
	sblock.fs_cstotal.cs_ndir += acg.cg_cs.cs_ndir;
	sblock.fs_cstotal.cs_nffree += acg.cg_cs.cs_nffree;
	sblock.fs_cstotal.cs_nbfree += acg.cg_cs.cs_nbfree;
	sblock.fs_cstotal.cs_nifree += acg.cg_cs.cs_nifree;
	*cs = acg.cg_cs;

	memcpy(iobuf, &acg, sblock.fs_cgsize);
	memset(iobuf + sblock.fs_cgsize, '\0',
	    sblock.fs_bsize * 3 - sblock.fs_cgsize);

	wtfs(fsbtodb(&sblock, cgtod(&sblock, cylno)),
	    sblock.fs_bsize * 3, iobuf, fso, Nflag);
	DBG_DUMP_CG(&sblock, "new cg", &acg);

	DBG_LEAVE;
	return;
}

/*
 * Here we add or subtract (sign +1/-1) the available fragments in a given
 * block to or from the fragment statistics. By subtracting before and adding
 * after an operation on the free frag map we can easy update the fragment
 * statistic, which seems to be otherwise a rather complex operation.
 */
static void
frag_adjust(ufs2_daddr_t frag, int sign)
{
	DBG_FUNC("frag_adjust")
	int fragsize;
	int f;

	DBG_ENTER;

	fragsize=0;
	/*
	 * Here frag only needs to point to any fragment in the block we want
	 * to examine.
	 */
	for (f = rounddown(frag, sblock.fs_frag);
	    f < roundup(frag + 1, sblock.fs_frag); f++) {
		/*
		 * Count contiguous free fragments.
		 */
		if (isset(cg_blksfree(&acg), f)) {
			fragsize++;
		} else {
			if (fragsize && fragsize < sblock.fs_frag) {
				/*
				 * We found something in between.
				 */
				acg.cg_frsum[fragsize]+=sign;
				DBG_PRINT2("frag_adjust [%d]+=%d\n",
				    fragsize, sign);
			}
			fragsize = 0;
		}
	}
	if (fragsize && fragsize < sblock.fs_frag) {
		/*
		 * We found something.
		 */
		acg.cg_frsum[fragsize] += sign;
		DBG_PRINT2("frag_adjust [%d]+=%d\n", fragsize, sign);
	}
	DBG_PRINT2("frag_adjust [[%d]]+=%d\n", fragsize, sign);

	DBG_LEAVE;
	return;
}

/*
 * Here we do all needed work for the former last cylinder group. It has to be
 * changed in any case, even if the file system ended exactly on the end of
 * this group, as there is some slightly inconsistent handling of the number
 * of cylinders in the cylinder group. We start again by reading the cylinder
 * group from disk. If the last block was not fully available, we first handle
 * the missing fragments, then we handle all new full blocks in that file
 * system and finally we handle the new last fragmented block in the file
 * system.  We again have to handle the fragment statistics rotational layout
 * tables and cluster summary during all those operations.
 */
static void
updjcg(int cylno, time_t modtime, int fsi, int fso, unsigned int Nflag)
{
	DBG_FUNC("updjcg")
	ufs2_daddr_t cbase, dmax, dupper;
	struct csum *cs;
	int i, k;
	int j = 0;

	DBG_ENTER;

	/*
	 * Read the former last (joining) cylinder group from disk, and make
	 * a copy.
	 */
	rdfs(fsbtodb(&osblock, cgtod(&osblock, cylno)),
	    (size_t)osblock.fs_cgsize, (void *)&aocg, fsi);
	DBG_PRINT0("jcg read\n");
	DBG_DUMP_CG(&sblock, "old joining cg", &aocg);

	memcpy((void *)&cgun1, (void *)&cgun2, sizeof(cgun2));

	/*
	 * If the cylinder group had already its new final size almost
	 * nothing is to be done ... except:
	 * For some reason the value of cg_ncyl in the last cylinder group has
	 * to be zero instead of fs_cpg. As this is now no longer the last
	 * cylinder group we have to change that value now to fs_cpg.
	 */

	if (cgbase(&osblock, cylno + 1) == osblock.fs_size) {
		if (sblock.fs_magic == FS_UFS1_MAGIC)
			acg.cg_old_ncyl=sblock.fs_old_cpg;

		wtfs(fsbtodb(&sblock, cgtod(&sblock, cylno)),
		    (size_t)sblock.fs_cgsize, (void *)&acg, fso, Nflag);
		DBG_PRINT0("jcg written\n");
		DBG_DUMP_CG(&sblock, "new joining cg", &acg);

		DBG_LEAVE;
		return;
	}

	/*
	 * Set up some variables needed later.
	 */
	cbase = cgbase(&sblock, cylno);
	dmax = cbase + sblock.fs_fpg;
	if (dmax > sblock.fs_size)
		dmax = sblock.fs_size;
	dupper = cgdmin(&sblock, cylno) - cbase;
	if (cylno == 0) /* XXX fscs may be relocated */
		dupper += howmany(sblock.fs_cssize, sblock.fs_fsize);

	/*
	 * Set pointer to the cylinder summary for our cylinder group.
	 */
	cs = fscs + cylno;

	/*
	 * Touch the cylinder group, update all fields in the cylinder group as
	 * needed, update the free space in the superblock.
	 */
	acg.cg_time = modtime;
	if ((unsigned)cylno == sblock.fs_ncg - 1) {
		/*
		 * This is still the last cylinder group.
		 */
		if (sblock.fs_magic == FS_UFS1_MAGIC)
			acg.cg_old_ncyl =
			    sblock.fs_old_ncyl % sblock.fs_old_cpg;
	} else {
		acg.cg_old_ncyl = sblock.fs_old_cpg;
	}
	DBG_PRINT2("jcg dbg: %d %u", cylno, sblock.fs_ncg);
#ifdef FS_DEBUG
	if (sblock.fs_magic == FS_UFS1_MAGIC)
		DBG_PRINT2("%d %u", acg.cg_old_ncyl, sblock.fs_old_cpg);
#endif
	DBG_PRINT0("\n");
	acg.cg_ndblk = dmax - cbase;
	sblock.fs_dsize += acg.cg_ndblk - aocg.cg_ndblk;
	if (sblock.fs_contigsumsize > 0)
		acg.cg_nclusterblks = acg.cg_ndblk / sblock.fs_frag;

	/*
	 * Now we have to update the free fragment bitmap for our new free
	 * space.  There again we have to handle the fragmentation and also
	 * the rotational layout tables and the cluster summary.  This is
	 * also done per fragment for the first new block if the old file
	 * system end was not on a block boundary, per fragment for the new
	 * last block if the new file system end is not on a block boundary,
	 * and per block for all space in between.
	 *
	 * Handle the first new block here if it was partially available
	 * before.
	 */
	if (osblock.fs_size % sblock.fs_frag) {
		if (roundup(osblock.fs_size, sblock.fs_frag) <=
		    sblock.fs_size) {
			/*
			 * The new space is enough to fill at least this
			 * block
			 */
			j = 0;
			for (i = roundup(osblock.fs_size - cbase,
			    sblock.fs_frag) - 1; i >= osblock.fs_size - cbase;
			    i--) {
				setbit(cg_blksfree(&acg), i);
				acg.cg_cs.cs_nffree++;
				j++;
			}

			/*
			 * Check if the fragment just created could join an
			 * already existing fragment at the former end of the
			 * file system.
			 */
			if (isblock(&sblock, cg_blksfree(&acg),
			    ((osblock.fs_size - cgbase(&sblock, cylno)) /
			     sblock.fs_frag))) {
				/*
				 * The block is now completely available.
				 */
				DBG_PRINT0("block was\n");
				acg.cg_frsum[osblock.fs_size % sblock.fs_frag]--;
				acg.cg_cs.cs_nbfree++;
				acg.cg_cs.cs_nffree -= sblock.fs_frag;
				k = rounddown(osblock.fs_size - cbase,
				    sblock.fs_frag);
				updclst((osblock.fs_size - cbase) /
				    sblock.fs_frag);
			} else {
				/*
				 * Lets rejoin a possible partially growed
				 * fragment.
				 */
				k = 0;
				while (isset(cg_blksfree(&acg), i) &&
				    (i >= rounddown(osblock.fs_size - cbase,
				    sblock.fs_frag))) {
					i--;
					k++;
				}
				if (k)
					acg.cg_frsum[k]--;
				acg.cg_frsum[k + j]++;
			}
		} else {
			/*
			 * We only grow by some fragments within this last
			 * block.
			 */
			for (i = sblock.fs_size - cbase - 1;
			    i >= osblock.fs_size - cbase; i--) {
				setbit(cg_blksfree(&acg), i);
				acg.cg_cs.cs_nffree++;
				j++;
			}
			/*
			 * Lets rejoin a possible partially growed fragment.
			 */
			k = 0;
			while (isset(cg_blksfree(&acg), i) &&
			    (i >= rounddown(osblock.fs_size - cbase,
			    sblock.fs_frag))) {
				i--;
				k++;
			}
			if (k)
				acg.cg_frsum[k]--;
			acg.cg_frsum[k + j]++;
		}
	}

	/*
	 * Handle all new complete blocks here.
	 */
	for (i = roundup(osblock.fs_size - cbase, sblock.fs_frag);
	    i + sblock.fs_frag <= dmax - cbase;	/* XXX <= or only < ? */
	    i += sblock.fs_frag) {
		j = i / sblock.fs_frag;
		setblock(&sblock, cg_blksfree(&acg), j);
		updclst(j);
		acg.cg_cs.cs_nbfree++;
	}

	/*
	 * Handle the last new block if there are stll some new fragments left.
	 * Here we don't have to bother about the cluster summary or the even
	 * the rotational layout table.
	 */
	if (i < (dmax - cbase)) {
		acg.cg_frsum[dmax - cbase - i]++;
		for (; i < dmax - cbase; i++) {
			setbit(cg_blksfree(&acg), i);
			acg.cg_cs.cs_nffree++;
		}
	}

	sblock.fs_cstotal.cs_nffree +=
	    (acg.cg_cs.cs_nffree - aocg.cg_cs.cs_nffree);
	sblock.fs_cstotal.cs_nbfree +=
	    (acg.cg_cs.cs_nbfree - aocg.cg_cs.cs_nbfree);
	/*
	 * The following statistics are not changed here:
	 *     sblock.fs_cstotal.cs_ndir
	 *     sblock.fs_cstotal.cs_nifree
	 * As the statistics for this cylinder group are ready, copy it to
	 * the summary information array.
	 */
	*cs = acg.cg_cs;

	/*
	 * Write the updated "joining" cylinder group back to disk.
	 */
	wtfs(fsbtodb(&sblock, cgtod(&sblock, cylno)), (size_t)sblock.fs_cgsize,
	    (void *)&acg, fso, Nflag);
	DBG_PRINT0("jcg written\n");
	DBG_DUMP_CG(&sblock, "new joining cg", &acg);

	DBG_LEAVE;
	return;
}

/*
 * Here we update the location of the cylinder summary. We have two possible
 * ways of growing the cylinder summary.
 * (1)	We can try to grow the summary in the current location, and relocate
 *	possibly used blocks within the current cylinder group.
 * (2)	Alternatively we can relocate the whole cylinder summary to the first
 *	new completely empty cylinder group. Once the cylinder summary is no
 *	longer in the beginning of the first cylinder group you should never
 *	use a version of fsck which is not aware of the possibility to have
 *	this structure in a non standard place.
 * Option (2) is considered to be less intrusive to the structure of the file-
 * system, so that's the one being used.
 */
static void
updcsloc(time_t modtime, int fsi, int fso, unsigned int Nflag)
{
	DBG_FUNC("updcsloc")
	struct csum *cs;
	int ocscg, ncscg;
	int blocks;
	ufs2_daddr_t d;
	int lcs = 0;
	int block;

	DBG_ENTER;

	if (howmany(sblock.fs_cssize, sblock.fs_fsize) ==
	    howmany(osblock.fs_cssize, osblock.fs_fsize)) {
		/*
		 * No new fragment needed.
		 */
		DBG_LEAVE;
		return;
	}
	ocscg = dtog(&osblock, osblock.fs_csaddr);
	cs = fscs + ocscg;
	blocks = 1 + howmany(sblock.fs_cssize, sblock.fs_bsize) -
	    howmany(osblock.fs_cssize, osblock.fs_bsize);

	/*
	 * Read original cylinder group from disk, and make a copy.
	 * XXX	If Nflag is set in some very rare cases we now miss
	 *	some changes done in updjcg by reading the unmodified
	 *	block from disk.
	 */
	rdfs(fsbtodb(&osblock, cgtod(&osblock, ocscg)),
	    (size_t)osblock.fs_cgsize, (void *)&aocg, fsi);
	DBG_PRINT0("oscg read\n");
	DBG_DUMP_CG(&sblock, "old summary cg", &aocg);

	memcpy((void *)&cgun1, (void *)&cgun2, sizeof(cgun2));

	/*
	 * Touch the cylinder group, set up local variables needed later
	 * and update the superblock.
	 */
	acg.cg_time = modtime;

	/*
	 * XXX	In the case of having active snapshots we may need much more
	 *	blocks for the copy on write. We need each block twice, and
	 *	also up to 8*3 blocks for indirect blocks for all possible
	 *	references.
	 */
	/*
	 * There is not enough space in the old cylinder group to
	 * relocate all blocks as needed, so we relocate the whole
	 * cylinder group summary to a new group. We try to use the
	 * first complete new cylinder group just created. Within the
	 * cylinder group we align the area immediately after the
	 * cylinder group information location in order to be as
	 * close as possible to the original implementation of ffs.
	 *
	 * First we have to make sure we'll find enough space in the
	 * new cylinder group. If not, then we currently give up.
	 * We start with freeing everything which was used by the
	 * fragments of the old cylinder summary in the current group.
	 * Now we write back the group meta data, read in the needed
	 * meta data from the new cylinder group, and start allocating
	 * within that group. Here we can assume, the group to be
	 * completely empty. Which makes the handling of fragments and
	 * clusters a lot easier.
	 */
	DBG_TRC;
	if (sblock.fs_ncg - osblock.fs_ncg < 2)
		errx(2, "panic: not enough space");

	/*
	 * Point "d" to the first fragment not used by the cylinder
	 * summary.
	 */
	d = osblock.fs_csaddr + (osblock.fs_cssize / osblock.fs_fsize);

	/*
	 * Set up last cluster size ("lcs") already here. Calculate
	 * the size for the trailing cluster just behind where "d"
	 * points to.
	 */
	if (sblock.fs_contigsumsize > 0) {
		for (block = howmany(d % sblock.fs_fpg, sblock.fs_frag),
		    lcs = 0; lcs < sblock.fs_contigsumsize;
		    block++, lcs++) {
			if (isclr(cg_clustersfree(&acg), block))
				break;
		}
	}

	/*
	 * Point "d" to the last frag used by the cylinder summary.
	 */
	d--;

	DBG_PRINT1("d=%jd\n", (intmax_t)d);
	if ((d + 1) % sblock.fs_frag) {
		/*
		 * The end of the cylinder summary is not a complete
		 * block.
		 */
		DBG_TRC;
		frag_adjust(d % sblock.fs_fpg, -1);
		for (; (d + 1) % sblock.fs_frag; d--) {
			DBG_PRINT1("d=%jd\n", (intmax_t)d);
			setbit(cg_blksfree(&acg), d % sblock.fs_fpg);
			acg.cg_cs.cs_nffree++;
			sblock.fs_cstotal.cs_nffree++;
		}
		/*
		 * Point "d" to the last fragment of the last
		 * (incomplete) block of the cylinder summary.
		 */
		d++;
		frag_adjust(d%sblock.fs_fpg, 1);

		if (isblock(&sblock, cg_blksfree(&acg),
		    (d % sblock.fs_fpg) / sblock.fs_frag)) {
			DBG_PRINT1("d=%jd\n", (intmax_t)d);
			acg.cg_cs.cs_nffree -= sblock.fs_frag;
			acg.cg_cs.cs_nbfree++;
			sblock.fs_cstotal.cs_nffree -= sblock.fs_frag;
			sblock.fs_cstotal.cs_nbfree++;
			if (sblock.fs_contigsumsize > 0) {
				setbit(cg_clustersfree(&acg),
				    (d % sblock.fs_fpg) /
				    sblock.fs_frag);
				if (lcs < sblock.fs_contigsumsize) {
					if (lcs)
						cg_clustersum(&acg)[lcs]--;
					lcs++;
					cg_clustersum(&acg)[lcs]++;
				}
			}
		}
		/*
		 * Point "d" to the first fragment of the block before
		 * the last incomplete block.
		 */
		d--;
	}

	DBG_PRINT1("d=%jd\n", (intmax_t)d);
	for (d = rounddown(d, sblock.fs_frag); d >= osblock.fs_csaddr;
	    d -= sblock.fs_frag) {
		DBG_TRC;
		DBG_PRINT1("d=%jd\n", (intmax_t)d);
		setblock(&sblock, cg_blksfree(&acg),
		    (d % sblock.fs_fpg) / sblock.fs_frag);
		acg.cg_cs.cs_nbfree++;
		sblock.fs_cstotal.cs_nbfree++;
		if (sblock.fs_contigsumsize > 0) {
			setbit(cg_clustersfree(&acg),
			    (d % sblock.fs_fpg) / sblock.fs_frag);
			/*
			 * The last cluster size is already set up.
			 */
			if (lcs < sblock.fs_contigsumsize) {
				if (lcs)
					cg_clustersum(&acg)[lcs]--;
				lcs++;
				cg_clustersum(&acg)[lcs]++;
			}
		}
	}
	*cs = acg.cg_cs;

	/*
	 * Now write the former cylinder group containing the cylinder
	 * summary back to disk.
	 */
	wtfs(fsbtodb(&sblock, cgtod(&sblock, ocscg)),
	    (size_t)sblock.fs_cgsize, (void *)&acg, fso, Nflag);
	DBG_PRINT0("oscg written\n");
	DBG_DUMP_CG(&sblock, "old summary cg", &acg);

	/*
	 * Find the beginning of the new cylinder group containing the
	 * cylinder summary.
	 */
	sblock.fs_csaddr = cgdmin(&sblock, osblock.fs_ncg);
	ncscg = dtog(&sblock, sblock.fs_csaddr);
	cs = fscs + ncscg;

	/*
	 * If Nflag is specified, we would now read random data instead
	 * of an empty cg structure from disk. So we can't simulate that
	 * part for now.
	 */
	if (Nflag) {
		DBG_PRINT0("nscg update skipped\n");
		DBG_LEAVE;
		return;
	}

	/*
	 * Read the future cylinder group containing the cylinder
	 * summary from disk, and make a copy.
	 */
	rdfs(fsbtodb(&sblock, cgtod(&sblock, ncscg)),
	    (size_t)sblock.fs_cgsize, (void *)&aocg, fsi);
	DBG_PRINT0("nscg read\n");
	DBG_DUMP_CG(&sblock, "new summary cg", &aocg);

	memcpy((void *)&cgun1, (void *)&cgun2, sizeof(cgun2));

	/*
	 * Allocate all complete blocks used by the new cylinder
	 * summary.
	 */
	for (d = sblock.fs_csaddr; d + sblock.fs_frag <=
	    sblock.fs_csaddr + (sblock.fs_cssize / sblock.fs_fsize);
	    d += sblock.fs_frag) {
		clrblock(&sblock, cg_blksfree(&acg),
		    (d % sblock.fs_fpg) / sblock.fs_frag);
		acg.cg_cs.cs_nbfree--;
		sblock.fs_cstotal.cs_nbfree--;
		if (sblock.fs_contigsumsize > 0) {
			clrbit(cg_clustersfree(&acg),
			    (d % sblock.fs_fpg) / sblock.fs_frag);
		}
	}

	/*
	 * Allocate all fragments used by the cylinder summary in the
	 * last block.
	 */
	if (d <
	    sblock.fs_csaddr + (sblock.fs_cssize / sblock.fs_fsize)) {
		for (; d - sblock.fs_csaddr <
		    sblock.fs_cssize/sblock.fs_fsize; d++) {
			clrbit(cg_blksfree(&acg), d % sblock.fs_fpg);
			acg.cg_cs.cs_nffree--;
			sblock.fs_cstotal.cs_nffree--;
		}
		acg.cg_cs.cs_nbfree--;
		acg.cg_cs.cs_nffree += sblock.fs_frag;
		sblock.fs_cstotal.cs_nbfree--;
		sblock.fs_cstotal.cs_nffree += sblock.fs_frag;
		if (sblock.fs_contigsumsize > 0)
			clrbit(cg_clustersfree(&acg),
			    (d % sblock.fs_fpg) / sblock.fs_frag);

		frag_adjust(d % sblock.fs_fpg, 1);
	}
	/*
	 * XXX	Handle the cluster statistics here in the case this
	 *	cylinder group is now almost full, and the remaining
	 *	space is less then the maximum cluster size. This is
	 *	probably not needed, as you would hardly find a file
	 *	system which has only MAXCSBUFS+FS_MAXCONTIG of free
	 *	space right behind the cylinder group information in
	 *	any new cylinder group.
	 */

	/*
	 * Update our statistics in the cylinder summary.
	 */
	*cs = acg.cg_cs;

	/*
	 * Write the new cylinder group containing the cylinder summary
	 * back to disk.
	 */
	wtfs(fsbtodb(&sblock, cgtod(&sblock, ncscg)),
	    (size_t)sblock.fs_cgsize, (void *)&acg, fso, Nflag);
	DBG_PRINT0("nscg written\n");
	DBG_DUMP_CG(&sblock, "new summary cg", &acg);

	DBG_LEAVE;
	return;
}

/*
 * Here we read some block(s) from disk.
 */
static void
rdfs(ufs2_daddr_t bno, size_t size, void *bf, int fsi)
{
	DBG_FUNC("rdfs")
	ssize_t	n;

	DBG_ENTER;

	if (bno < 0)
		err(32, "rdfs: attempting to read negative block number");
	if (lseek(fsi, (off_t)bno * DEV_BSIZE, 0) < 0)
		err(33, "rdfs: seek error: %jd", (intmax_t)bno);
	n = read(fsi, bf, size);
	if (n != (ssize_t)size)
		err(34, "rdfs: read error: %jd", (intmax_t)bno);

	DBG_LEAVE;
	return;
}

/*
 * Here we write some block(s) to disk.
 */
static void
wtfs(ufs2_daddr_t bno, size_t size, void *bf, int fso, unsigned int Nflag)
{
	DBG_FUNC("wtfs")
	ssize_t	n;

	DBG_ENTER;

	if (Nflag) {
		DBG_LEAVE;
		return;
	}
	if (lseek(fso, (off_t)bno * DEV_BSIZE, SEEK_SET) < 0)
		err(35, "wtfs: seek error: %ld", (long)bno);
	n = write(fso, bf, size);
	if (n != (ssize_t)size)
		err(36, "wtfs: write error: %ld", (long)bno);

	DBG_LEAVE;
	return;
}

/*
 * Here we check if all frags of a block are free. For more details again
 * please see the source of newfs(8), as this function is taken over almost
 * unchanged.
 */
static int
isblock(struct fs *fs, unsigned char *cp, int h)
{
	DBG_FUNC("isblock")
	unsigned char mask;

	DBG_ENTER;

	switch (fs->fs_frag) {
	case 8:
		DBG_LEAVE;
		return (cp[h] == 0xff);
	case 4:
		mask = 0x0f << ((h & 0x1) << 2);
		DBG_LEAVE;
		return ((cp[h >> 1] & mask) == mask);
	case 2:
		mask = 0x03 << ((h & 0x3) << 1);
		DBG_LEAVE;
		return ((cp[h >> 2] & mask) == mask);
	case 1:
		mask = 0x01 << (h & 0x7);
		DBG_LEAVE;
		return ((cp[h >> 3] & mask) == mask);
	default:
		fprintf(stderr, "isblock bad fs_frag %d\n", fs->fs_frag);
		DBG_LEAVE;
		return (0);
	}
}

/*
 * Here we allocate a complete block in the block map. For more details again
 * please see the source of newfs(8), as this function is taken over almost
 * unchanged.
 */
static void
clrblock(struct fs *fs, unsigned char *cp, int h)
{
	DBG_FUNC("clrblock")

	DBG_ENTER;

	switch ((fs)->fs_frag) {
	case 8:
		cp[h] = 0;
		break;
	case 4:
		cp[h >> 1] &= ~(0x0f << ((h & 0x1) << 2));
		break;
	case 2:
		cp[h >> 2] &= ~(0x03 << ((h & 0x3) << 1));
		break;
	case 1:
		cp[h >> 3] &= ~(0x01 << (h & 0x7));
		break;
	default:
		warnx("clrblock bad fs_frag %d", fs->fs_frag);
		break;
	}

	DBG_LEAVE;
	return;
}

/*
 * Here we free a complete block in the free block map. For more details again
 * please see the source of newfs(8), as this function is taken over almost
 * unchanged.
 */
static void
setblock(struct fs *fs, unsigned char *cp, int h)
{
	DBG_FUNC("setblock")

	DBG_ENTER;

	switch (fs->fs_frag) {
	case 8:
		cp[h] = 0xff;
		break;
	case 4:
		cp[h >> 1] |= (0x0f << ((h & 0x1) << 2));
		break;
	case 2:
		cp[h >> 2] |= (0x03 << ((h & 0x3) << 1));
		break;
	case 1:
		cp[h >> 3] |= (0x01 << (h & 0x7));
		break;
	default:
		warnx("setblock bad fs_frag %d", fs->fs_frag);
		break;
	}

	DBG_LEAVE;
	return;
}

/*
 * Figure out how many lines our current terminal has. For more details again
 * please see the source of newfs(8), as this function is taken over almost
 * unchanged.
 */
static int
charsperline(void)
{
	DBG_FUNC("charsperline")
	int columns;
	char *cp;
	struct winsize ws;

	DBG_ENTER;

	columns = 0;
	if (ioctl(0, TIOCGWINSZ, &ws) != -1)
		columns = ws.ws_col;
	if (columns == 0 && (cp = getenv("COLUMNS")))
		columns = atoi(cp);
	if (columns == 0)
		columns = 80;	/* last resort */

	DBG_LEAVE;
	return columns;
}

/*
 * Get the size of the partition.
 */
static void
get_dev_size(int fd, int *size)
{
	int sectorsize;
	off_t mediasize;

	if (ioctl(fd, DIOCGSECTORSIZE, &sectorsize) == -1)
		err(1,"DIOCGSECTORSIZE");
	if (ioctl(fd, DIOCGMEDIASIZE, &mediasize) == -1)
		err(1,"DIOCGMEDIASIZE");

	if (sectorsize <= 0)
		errx(1, "bogus sectorsize: %d", sectorsize);

	*size = mediasize / sectorsize;
}

/*
 * growfs(8) is a utility which allows to increase the size of an existing
 * ufs file system. Currently this can only be done on unmounted file system.
 * It recognizes some command line options to specify the new desired size,
 * and it does some basic checkings. The old file system size is determined
 * and after some more checks like we can really access the new last block
 * on the disk etc. we calculate the new parameters for the superblock. After
 * having done this we just call growfs() which will do the work.
 * We still have to provide support for snapshots. Therefore we first have to
 * understand what data structures are always replicated in the snapshot on
 * creation, for all other blocks we touch during our procedure, we have to
 * keep the old blocks unchanged somewhere available for the snapshots. If we
 * are lucky, then we only have to handle our blocks to be relocated in that
 * way.
 * Also we have to consider in what order we actually update the critical
 * data structures of the file system to make sure, that in case of a disaster
 * fsck(8) is still able to restore any lost data.
 * The foreseen last step then will be to provide for growing even mounted
 * file systems. There we have to extend the mount() system call to provide
 * userland access to the file system locking facility.
 */
int
main(int argc, char **argv)
{
	DBG_FUNC("main")
	char *device, *special;
	int ch;
	unsigned int size = 0;
	size_t len;
	unsigned int Nflag = 0;
	int ExpertFlag = 0;
	struct stat st;
	int i, fsi, fso;
	u_int32_t p_size;
	char reply[5];
#ifdef FSMAXSNAP
	int j;
#endif /* FSMAXSNAP */

	DBG_ENTER;

	while ((ch = getopt(argc, argv, "Ns:vy")) != -1) {
		switch(ch) {
		case 'N':
			Nflag = 1;
			break;
		case 's':
			size = (size_t)atol(optarg);
			if (size < 1)
				usage();
			break;
		case 'v': /* for compatibility to newfs */
			break;
		case 'y':
			ExpertFlag = 1;
			break;
		case '?':
			/* FALLTHROUGH */
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	device = *argv;

	/*
	 * Now try to guess the (raw)device name.
	 */
	if (0 == strrchr(device, '/')) {
		/*
		 * No path prefix was given, so try in that order:
		 *     /dev/r%s
		 *     /dev/%s
		 *     /dev/vinum/r%s
		 *     /dev/vinum/%s.
		 *
		 * FreeBSD now doesn't distinguish between raw and block
		 * devices any longer, but it should still work this way.
		 */
		len = strlen(device) + strlen(_PATH_DEV) + 2 + strlen("vinum/");
		special = (char *)malloc(len);
		if (special == NULL)
			errx(1, "malloc failed");
		snprintf(special, len, "%sr%s", _PATH_DEV, device);
		if (stat(special, &st) == -1) {
			snprintf(special, len, "%s%s", _PATH_DEV, device);
			if (stat(special, &st) == -1) {
				snprintf(special, len, "%svinum/r%s",
				    _PATH_DEV, device);
				if (stat(special, &st) == -1) {
					/* For now this is the 'last resort' */
					snprintf(special, len, "%svinum/%s",
					    _PATH_DEV, device);
				}
			}
		}
		device = special;
	}

	/*
	 * Try to access our devices for writing ...
	 */
	if (Nflag) {
		fso = -1;
	} else {
		fso = open(device, O_WRONLY);
		if (fso < 0)
			err(1, "%s", device);
	}

	/*
	 * ... and reading.
	 */
	fsi = open(device, O_RDONLY);
	if (fsi < 0)
		err(1, "%s", device);

	/*
	 * Try to guess the slice if not specified. This code should guess
	 * the right thing and avoid to bother the user with the task
	 * of specifying the option -v on vinum volumes.
	 */
	get_dev_size(fsi, &p_size);

	/*
	 * Check if that partition is suitable for growing a file system.
	 */
	if (p_size < 1)
		errx(1, "partition is unavailable");

	/*
	 * Read the current superblock, and take a backup.
	 */
	for (i = 0; sblock_try[i] != -1; i++) {
		sblockloc = sblock_try[i] / DEV_BSIZE;
		rdfs(sblockloc, (size_t)SBLOCKSIZE, (void *)&(osblock), fsi);
		if ((osblock.fs_magic == FS_UFS1_MAGIC ||
		    (osblock.fs_magic == FS_UFS2_MAGIC &&
		    osblock.fs_sblockloc == sblock_try[i])) &&
		    osblock.fs_bsize <= MAXBSIZE &&
		    osblock.fs_bsize >= (int32_t) sizeof(struct fs))
			break;
	}
	if (sblock_try[i] == -1)
		errx(1, "superblock not recognized");
	memcpy((void *)&fsun1, (void *)&fsun2, sizeof(fsun2));

	DBG_OPEN("/tmp/growfs.debug"); /* already here we need a superblock */
	DBG_DUMP_FS(&sblock, "old sblock");

	/*
	 * Determine size to grow to. Default to the device size.
	 */
	sblock.fs_size = dbtofsb(&osblock, p_size);
	if (size != 0) {
		if (size > p_size)
			errx(1, "there is not enough space (%d < %d)",
			    p_size, size);
		sblock.fs_size = dbtofsb(&osblock, size);
	}

	/*
	 * Are we really growing ?
	 */
	if (osblock.fs_size >= sblock.fs_size) {
		errx(1, "we are not growing (%jd->%jd)",
		    (intmax_t)osblock.fs_size, (intmax_t)sblock.fs_size);
	}


#ifdef FSMAXSNAP
	/*
	 * Check if we find an active snapshot.
	 */
	if (ExpertFlag == 0) {
		for (j = 0; j < FSMAXSNAP; j++) {
			if (sblock.fs_snapinum[j]) {
				errx(1, "active snapshot found in file system; "
				    "please remove all snapshots before "
				    "using growfs");
			}
			if (!sblock.fs_snapinum[j]) /* list is dense */
				break;
		}
	}
#endif

	if (ExpertFlag == 0 && Nflag == 0) {
		printf("We strongly recommend you to make a backup "
		    "before growing the file system.\n"
		    "Did you backup your data (Yes/No)? ");
		fgets(reply, (int)sizeof(reply), stdin);
		if (strcmp(reply, "Yes\n")){
			printf("\nNothing done\n");
			exit (0);
		}
	}

	printf("New file system size is %jd frags\n", (intmax_t)sblock.fs_size);

	/*
	 * Try to access our new last block in the file system. Even if we
	 * later on realize we have to abort our operation, on that block
	 * there should be no data, so we can't destroy something yet.
	 */
	wtfs((ufs2_daddr_t)p_size - 1, (size_t)DEV_BSIZE, (void *)&sblock,
	    fso, Nflag);

	/*
	 * Now calculate new superblock values and check for reasonable
	 * bound for new file system size:
	 *     fs_size:    is derived from user input
	 *     fs_dsize:   should get updated in the routines creating or
	 *                 updating the cylinder groups on the fly
	 *     fs_cstotal: should get updated in the routines creating or
	 *                 updating the cylinder groups
	 */

	/*
	 * Update the number of cylinders and cylinder groups in the file system.
	 */
	if (sblock.fs_magic == FS_UFS1_MAGIC) {
		sblock.fs_old_ncyl =
		    sblock.fs_size * sblock.fs_old_nspf / sblock.fs_old_spc;
		if (sblock.fs_size * sblock.fs_old_nspf >
		    sblock.fs_old_ncyl * sblock.fs_old_spc)
			sblock.fs_old_ncyl++;
	}
	sblock.fs_ncg = howmany(sblock.fs_size, sblock.fs_fpg);

	if (sblock.fs_size % sblock.fs_fpg != 0 &&
	    sblock.fs_size % sblock.fs_fpg < cgdmin(&sblock, sblock.fs_ncg)) {
		/*
		 * The space in the new last cylinder group is too small,
		 * so revert back.
		 */
		sblock.fs_ncg--;
		if (sblock.fs_magic == FS_UFS1_MAGIC)
			sblock.fs_old_ncyl = sblock.fs_ncg * sblock.fs_old_cpg;
		printf("Warning: %jd sector(s) cannot be allocated.\n",
		    (intmax_t)fsbtodb(&sblock, sblock.fs_size % sblock.fs_fpg));
		sblock.fs_size = sblock.fs_ncg * sblock.fs_fpg;
	}

	/*
	 * Update the space for the cylinder group summary information in the
	 * respective cylinder group data area.
	 */
	sblock.fs_cssize =
	    fragroundup(&sblock, sblock.fs_ncg * sizeof(struct csum));

	if (osblock.fs_size >= sblock.fs_size)
		errx(1, "not enough new space");

	DBG_PRINT0("sblock calculated\n");

	/*
	 * Ok, everything prepared, so now let's do the tricks.
	 */
	growfs(fsi, fso, Nflag);

	close(fsi);
	if (fso > -1)
		close(fso);

	DBG_CLOSE;

	DBG_LEAVE;
	return 0;
}

/*
 * Dump a line of usage.
 */
static void
usage(void)
{
	DBG_FUNC("usage")

	DBG_ENTER;

	fprintf(stderr, "usage: growfs [-Ny] [-s size] special\n");

	DBG_LEAVE;
	exit(1);
}

/*
 * This updates most parameters and the bitmap related to cluster. We have to
 * assume that sblock, osblock, acg are set up.
 */
static void
updclst(int block)
{
	DBG_FUNC("updclst")
	static int lcs = 0;

	DBG_ENTER;

	if (sblock.fs_contigsumsize < 1) /* no clustering */
		return;
	/*
	 * update cluster allocation map
	 */
	setbit(cg_clustersfree(&acg), block);

	/*
	 * update cluster summary table
	 */
	if (!lcs) {
		/*
		 * calculate size for the trailing cluster
		 */
		for (block--; lcs < sblock.fs_contigsumsize; block--, lcs++ ) {
			if (isclr(cg_clustersfree(&acg), block))
				break;
		}
	}
	if (lcs < sblock.fs_contigsumsize) {
		if (lcs)
			cg_clustersum(&acg)[lcs]--;
		lcs++;
		cg_clustersum(&acg)[lcs]++;
	}

	DBG_LEAVE;
	return;
}
