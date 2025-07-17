/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2009, 2010 Jeffrey W. Roberson <jeff@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>

#include <assert.h>
#include <err.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sysexits.h>
#include <time.h>

#include "fsck.h"

#define	DOTDOT_OFFSET	DIRECTSIZ(1)

struct suj_seg {
	TAILQ_ENTRY(suj_seg) ss_next;
	struct jsegrec	ss_rec;
	uint8_t		*ss_blk;
};

struct suj_rec {
	TAILQ_ENTRY(suj_rec) sr_next;
	union jrec	*sr_rec;
};
TAILQ_HEAD(srechd, suj_rec);

struct suj_ino {
	LIST_ENTRY(suj_ino)	si_next;
	struct srechd		si_recs;
	struct srechd		si_newrecs;
	struct srechd		si_movs;
	struct jtrncrec		*si_trunc;
	ino_t			si_ino;
	char			si_skipparent;
	char			si_hasrecs;
	char			si_blkadj;
	char			si_linkadj;
	int			si_mode;
	nlink_t			si_nlinkadj;
	nlink_t			si_nlink;
	nlink_t			si_dotlinks;
};
LIST_HEAD(inohd, suj_ino);

struct suj_blk {
	LIST_ENTRY(suj_blk)	sb_next;
	struct srechd		sb_recs;
	ufs2_daddr_t		sb_blk;
};
LIST_HEAD(blkhd, suj_blk);

struct suj_cg {
	LIST_ENTRY(suj_cg)	sc_next;
	struct blkhd		sc_blkhash[HASHSIZE];
	struct inohd		sc_inohash[HASHSIZE];
	struct ino_blk		*sc_lastiblk;
	struct suj_ino		*sc_lastino;
	struct suj_blk		*sc_lastblk;
	struct bufarea		*sc_cgbp;
	struct cg		*sc_cgp;
	int			sc_cgx;
};

static LIST_HEAD(cghd, suj_cg) cghash[HASHSIZE];
static struct suj_cg *lastcg;

static TAILQ_HEAD(seghd, suj_seg) allsegs;
static uint64_t oldseq;
static struct fs *fs = NULL;
static ino_t sujino;
static char *joptype[JOP_NUMJOPTYPES] = JOP_NAMES;

/*
 * Summary statistics.
 */
static uint64_t freefrags;
static uint64_t freeblocks;
static uint64_t freeinos;
static uint64_t freedir;
static uint64_t jbytes;
static uint64_t jrecs;

static jmp_buf	jmpbuf;

typedef void (*ino_visitor)(ino_t, ufs_lbn_t, ufs2_daddr_t, int);
static void err_suj(const char *, ...) __dead2;
static void ino_trunc(ino_t, off_t);
static void ino_decr(ino_t);
static void ino_adjust(struct suj_ino *);
static void ino_build(struct suj_ino *);
static int blk_isfree(ufs2_daddr_t);
static void initsuj(void);

static void *
errmalloc(size_t n)
{
	void *a;

	a = Malloc(n);
	if (a == NULL)
		err(EX_OSERR, "malloc(%zu)", n);
	return (a);
}

/*
 * When hit a fatal error in journalling check, print out
 * the error and then offer to fallback to normal fsck.
 */
static void
err_suj(const char * restrict fmt, ...)
{
	va_list ap;

	if (preen)
		(void)fprintf(stdout, "%s: ", cdevname);

	va_start(ap, fmt);
	(void)vfprintf(stdout, fmt, ap);
	va_end(ap);

	longjmp(jmpbuf, -1);
}

/*
 * Lookup a cg by number in the hash so we can keep track of which cgs
 * need stats rebuilt.
 */
static struct suj_cg *
cg_lookup(int cgx)
{
	struct cghd *hd;
	struct suj_cg *sc;
	struct bufarea *cgbp;

	if (cgx < 0 || cgx >= fs->fs_ncg)
		err_suj("Bad cg number %d\n", cgx);
	if (lastcg && lastcg->sc_cgx == cgx)
		return (lastcg);
	cgbp = cglookup(cgx);
	if (!check_cgmagic(cgx, cgbp))
		err_suj("UNABLE TO REBUILD CYLINDER GROUP %d", cgx);
	hd = &cghash[HASH(cgx)];
	LIST_FOREACH(sc, hd, sc_next)
		if (sc->sc_cgx == cgx) {
			sc->sc_cgbp = cgbp;
			sc->sc_cgp = sc->sc_cgbp->b_un.b_cg;
			lastcg = sc;
			return (sc);
		}
	sc = errmalloc(sizeof(*sc));
	bzero(sc, sizeof(*sc));
	sc->sc_cgbp = cgbp;
	sc->sc_cgp = sc->sc_cgbp->b_un.b_cg;
	sc->sc_cgx = cgx;
	LIST_INSERT_HEAD(hd, sc, sc_next);
	return (sc);
}

/*
 * Lookup an inode number in the hash and allocate a suj_ino if it does
 * not exist.
 */
static struct suj_ino *
ino_lookup(ino_t ino, int creat)
{
	struct suj_ino *sino;
	struct inohd *hd;
	struct suj_cg *sc;

	sc = cg_lookup(ino_to_cg(fs, ino));
	if (sc->sc_lastino && sc->sc_lastino->si_ino == ino)
		return (sc->sc_lastino);
	hd = &sc->sc_inohash[HASH(ino)];
	LIST_FOREACH(sino, hd, si_next)
		if (sino->si_ino == ino)
			return (sino);
	if (creat == 0)
		return (NULL);
	sino = errmalloc(sizeof(*sino));
	bzero(sino, sizeof(*sino));
	sino->si_ino = ino;
	TAILQ_INIT(&sino->si_recs);
	TAILQ_INIT(&sino->si_newrecs);
	TAILQ_INIT(&sino->si_movs);
	LIST_INSERT_HEAD(hd, sino, si_next);

	return (sino);
}

/*
 * Lookup a block number in the hash and allocate a suj_blk if it does
 * not exist.
 */
static struct suj_blk *
blk_lookup(ufs2_daddr_t blk, int creat)
{
	struct suj_blk *sblk;
	struct suj_cg *sc;
	struct blkhd *hd;

	sc = cg_lookup(dtog(fs, blk));
	if (sc->sc_lastblk && sc->sc_lastblk->sb_blk == blk)
		return (sc->sc_lastblk);
	hd = &sc->sc_blkhash[HASH(fragstoblks(fs, blk))];
	LIST_FOREACH(sblk, hd, sb_next)
		if (sblk->sb_blk == blk)
			return (sblk);
	if (creat == 0)
		return (NULL);
	sblk = errmalloc(sizeof(*sblk));
	bzero(sblk, sizeof(*sblk));
	sblk->sb_blk = blk;
	TAILQ_INIT(&sblk->sb_recs);
	LIST_INSERT_HEAD(hd, sblk, sb_next);

	return (sblk);
}

static int
blk_overlaps(struct jblkrec *brec, ufs2_daddr_t start, int frags)
{
	ufs2_daddr_t bstart;
	ufs2_daddr_t bend;
	ufs2_daddr_t end;

	end = start + frags;
	bstart = brec->jb_blkno + brec->jb_oldfrags;
	bend = bstart + brec->jb_frags;
	if (start < bend && end > bstart)
		return (1);
	return (0);
}

static int
blk_equals(struct jblkrec *brec, ino_t ino, ufs_lbn_t lbn, ufs2_daddr_t start,
    int frags)
{

	if (brec->jb_ino != ino || brec->jb_lbn != lbn)
		return (0);
	if (brec->jb_blkno + brec->jb_oldfrags != start)
		return (0);
	if (brec->jb_frags < frags)
		return (0);
	return (1);
}

static void
blk_setmask(struct jblkrec *brec, int *mask)
{
	int i;

	for (i = brec->jb_oldfrags; i < brec->jb_oldfrags + brec->jb_frags; i++)
		*mask |= 1 << i;
}

/*
 * Determine whether a given block has been reallocated to a new location.
 * Returns a mask of overlapping bits if any frags have been reused or
 * zero if the block has not been re-used and the contents can be trusted.
 *
 * This is used to ensure that an orphaned pointer due to truncate is safe
 * to be freed.  The mask value can be used to free partial blocks.
 */
static int
blk_freemask(ufs2_daddr_t blk, ino_t ino, ufs_lbn_t lbn, int frags)
{
	struct suj_blk *sblk;
	struct suj_rec *srec;
	struct jblkrec *brec;
	int mask;
	int off;

	/*
	 * To be certain we're not freeing a reallocated block we lookup
	 * this block in the blk hash and see if there is an allocation
	 * journal record that overlaps with any fragments in the block
	 * we're concerned with.  If any fragments have been reallocated
	 * the block has already been freed and re-used for another purpose.
	 */
	mask = 0;
	sblk = blk_lookup(blknum(fs, blk), 0);
	if (sblk == NULL)
		return (0);
	off = blk - sblk->sb_blk;
	TAILQ_FOREACH(srec, &sblk->sb_recs, sr_next) {
		brec = (struct jblkrec *)srec->sr_rec;
		/*
		 * If the block overlaps but does not match
		 * exactly this record refers to the current
		 * location.
		 */
		if (blk_overlaps(brec, blk, frags) == 0)
			continue;
		if (blk_equals(brec, ino, lbn, blk, frags) == 1)
			mask = 0;
		else
			blk_setmask(brec, &mask);
	}
	if (debug)
		printf("blk_freemask: blk %jd sblk %jd off %d mask 0x%X\n",
		    blk, sblk->sb_blk, off, mask);
	return (mask >> off);
}

/*
 * Determine whether it is safe to follow an indirect.  It is not safe
 * if any part of the indirect has been reallocated or the last journal
 * entry was an allocation.  Just allocated indirects may not have valid
 * pointers yet and all of their children will have their own records.
 * It is also not safe to follow an indirect if the cg bitmap has been
 * cleared as a new allocation may write to the block prior to the journal
 * being written.
 *
 * Returns 1 if it's safe to follow the indirect and 0 otherwise.
 */
static int
blk_isindir(ufs2_daddr_t blk, ino_t ino, ufs_lbn_t lbn)
{
	struct suj_blk *sblk;
	struct jblkrec *brec;

	sblk = blk_lookup(blk, 0);
	if (sblk == NULL)
		return (1);
	if (TAILQ_EMPTY(&sblk->sb_recs))
		return (1);
	brec = (struct jblkrec *)TAILQ_LAST(&sblk->sb_recs, srechd)->sr_rec;
	if (blk_equals(brec, ino, lbn, blk, fs->fs_frag))
		if (brec->jb_op == JOP_FREEBLK)
			return (!blk_isfree(blk));
	return (0);
}

/*
 * Check to see if the requested block is available.
 * We can just check in the cylinder-group maps as
 * they will only have usable blocks in them.
 */
ufs2_daddr_t
suj_checkblkavail(ufs2_daddr_t blkno, long frags)
{
	struct bufarea *cgbp;
	struct cg *cgp;
	ufs2_daddr_t j, k, baseblk;
	long cg;

	if ((u_int64_t)blkno > sblock.fs_size)
		return (0);
	cg = dtog(&sblock, blkno);
	cgbp = cglookup(cg);
	cgp = cgbp->b_un.b_cg;
	if (!check_cgmagic(cg, cgbp))
		return (-((cg + 1) * sblock.fs_fpg - sblock.fs_frag));
	baseblk = dtogd(&sblock, blkno);
	for (j = 0; j <= sblock.fs_frag - frags; j++) {
		if (!isset(cg_blksfree(cgp), baseblk + j))
			continue;
		for (k = 1; k < frags; k++)
			if (!isset(cg_blksfree(cgp), baseblk + j + k))
				break;
		if (k < frags) {
			j += k;
			continue;
		}
		for (k = 0; k < frags; k++)
			clrbit(cg_blksfree(cgp), baseblk + j + k);
		n_blks += frags;
		if (frags == sblock.fs_frag)
			cgp->cg_cs.cs_nbfree--;
		else
			cgp->cg_cs.cs_nffree -= frags;
		cgdirty(cgbp);
		return ((cg * sblock.fs_fpg) + baseblk + j);
	}
	return (0);
}

/*
 * Clear an inode from the cg bitmap.  If the inode was already clear return
 * 0 so the caller knows it does not have to check the inode contents.
 */
static int
ino_free(ino_t ino, int mode)
{
	struct suj_cg *sc;
	uint8_t *inosused;
	struct cg *cgp;
	int cg;

	cg = ino_to_cg(fs, ino);
	ino = ino % fs->fs_ipg;
	sc = cg_lookup(cg);
	cgp = sc->sc_cgp;
	inosused = cg_inosused(cgp);
	/*
	 * The bitmap may never have made it to the disk so we have to
	 * conditionally clear.  We can avoid writing the cg in this case.
	 */
	if (isclr(inosused, ino))
		return (0);
	freeinos++;
	clrbit(inosused, ino);
	if (ino < cgp->cg_irotor)
		cgp->cg_irotor = ino;
	cgp->cg_cs.cs_nifree++;
	if ((mode & IFMT) == IFDIR) {
		freedir++;
		cgp->cg_cs.cs_ndir--;
	}
	cgdirty(sc->sc_cgbp);

	return (1);
}

/*
 * Free 'frags' frags starting at filesystem block 'bno' skipping any frags
 * set in the mask.
 */
static void
blk_free(ino_t ino, ufs2_daddr_t bno, int mask, int frags)
{
	ufs1_daddr_t fragno, cgbno;
	struct suj_cg *sc;
	struct cg *cgp;
	int i, cg;
	uint8_t *blksfree;

	if (debug)
		printf("Freeing %d frags at blk %jd mask 0x%x\n",
		    frags, bno, mask);
	/*
	 * Check to see if the block needs to be claimed by a snapshot.
	 * If wanted, the snapshot references it. Otherwise we free it.
	 */
	if (snapblkfree(fs, bno, lfragtosize(fs, frags), ino,
	    suj_checkblkavail))
		return;
	cg = dtog(fs, bno);
	sc = cg_lookup(cg);
	cgp = sc->sc_cgp;
	cgbno = dtogd(fs, bno);
	blksfree = cg_blksfree(cgp);

	/*
	 * If it's not allocated we only wrote the journal entry
	 * and never the bitmaps.  Here we unconditionally clear and
	 * resolve the cg summary later.
	 */
	if (frags == fs->fs_frag && mask == 0) {
		fragno = fragstoblks(fs, cgbno);
		ffs_setblock(fs, blksfree, fragno);
		freeblocks++;
	} else {
		/*
		 * deallocate the fragment
		 */
		for (i = 0; i < frags; i++)
			if ((mask & (1 << i)) == 0 &&
			    isclr(blksfree, cgbno +i)) {
				freefrags++;
				setbit(blksfree, cgbno + i);
			}
	}
	cgdirty(sc->sc_cgbp);
}

/*
 * Returns 1 if the whole block starting at 'bno' is marked free and 0
 * otherwise.
 */
static int
blk_isfree(ufs2_daddr_t bno)
{
	struct suj_cg *sc;

	sc = cg_lookup(dtog(fs, bno));
	return ffs_isblock(fs, cg_blksfree(sc->sc_cgp), dtogd(fs, bno));
}

/*
 * Determine whether a block exists at a particular lbn in an inode.
 * Returns 1 if found, 0 if not.  lbn may be negative for indirects
 * or ext blocks.
 */
static int
blk_isat(ino_t ino, ufs_lbn_t lbn, ufs2_daddr_t blk, int *frags)
{
	struct inode ip;
	union dinode *dp;
	ufs2_daddr_t nblk;

	ginode(ino, &ip);
	dp = ip.i_dp;
	if (DIP(dp, di_nlink) == 0 || DIP(dp, di_mode) == 0) {
		irelse(&ip);
		return (0);
	}
	nblk = ino_blkatoff(dp, ino, lbn, frags, NULL);
	irelse(&ip);
	return (nblk == blk);
}

/*
 * Clear the directory entry at diroff that should point to child.  Minimal
 * checking is done and it is assumed that this path was verified with isat.
 */
static void
ino_clrat(ino_t parent, off_t diroff, ino_t child)
{
	union dinode *dip;
	struct direct *dp;
	struct inode ip;
	ufs2_daddr_t blk;
	struct bufarea *bp;
	ufs_lbn_t lbn;
	int blksize;
	int frags;
	int doff;

	if (debug)
		printf("Clearing inode %ju from parent %ju at offset %jd\n",
		    (uintmax_t)child, (uintmax_t)parent, diroff);

	lbn = lblkno(fs, diroff);
	doff = blkoff(fs, diroff);
	ginode(parent, &ip);
	dip = ip.i_dp;
	blk = ino_blkatoff(dip, parent, lbn, &frags, NULL);
	blksize = sblksize(fs, DIP(dip, di_size), lbn);
	irelse(&ip);
	bp = getdatablk(blk, blksize, BT_DIRDATA);
	if (bp->b_errs != 0)
		err_suj("ino_clrat: UNRECOVERABLE I/O ERROR");
	dp = (struct direct *)&bp->b_un.b_buf[doff];
	if (dp->d_ino != child)
		errx(1, "Inode %ju does not exist in %ju at %jd",
		    (uintmax_t)child, (uintmax_t)parent, diroff);
	dp->d_ino = 0;
	dirty(bp);
	brelse(bp);
	/*
	 * The actual .. reference count will already have been removed
	 * from the parent by the .. remref record.
	 */
}

/*
 * Determines whether a pointer to an inode exists within a directory
 * at a specified offset.  Returns the mode of the found entry.
 */
static int
ino_isat(ino_t parent, off_t diroff, ino_t child, int *mode, int *isdot)
{
	struct inode ip;
	union dinode *dip;
	struct bufarea *bp;
	struct direct *dp;
	ufs2_daddr_t blk;
	ufs_lbn_t lbn;
	int blksize;
	int frags;
	int dpoff;
	int doff;

	*isdot = 0;
	ginode(parent, &ip);
	dip = ip.i_dp;
	*mode = DIP(dip, di_mode);
	if ((*mode & IFMT) != IFDIR) {
		if (debug) {
			/*
			 * This can happen if the parent inode
			 * was reallocated.
			 */
			if (*mode != 0)
				printf("Directory %ju has bad mode %o\n",
				    (uintmax_t)parent, *mode);
			else
				printf("Directory %ju has zero mode\n",
				    (uintmax_t)parent);
		}
		irelse(&ip);
		return (0);
	}
	lbn = lblkno(fs, diroff);
	doff = blkoff(fs, diroff);
	blksize = sblksize(fs, DIP(dip, di_size), lbn);
	if (diroff + DIRECTSIZ(1) > DIP(dip, di_size) || doff >= blksize) {
		if (debug)
			printf("ino %ju absent from %ju due to offset %jd"
			    " exceeding size %jd\n",
			    (uintmax_t)child, (uintmax_t)parent, diroff,
			    DIP(dip, di_size));
		irelse(&ip);
		return (0);
	}
	blk = ino_blkatoff(dip, parent, lbn, &frags, NULL);
	irelse(&ip);
	if (blk <= 0) {
		if (debug)
			printf("Sparse directory %ju", (uintmax_t)parent);
		return (0);
	}
	bp = getdatablk(blk, blksize, BT_DIRDATA);
	if (bp->b_errs != 0)
		err_suj("ino_isat: UNRECOVERABLE I/O ERROR");
	/*
	 * Walk through the records from the start of the block to be
	 * certain we hit a valid record and not some junk in the middle
	 * of a file name.  Stop when we reach or pass the expected offset.
	 */
	dpoff = rounddown(doff, DIRBLKSIZ);
	do {
		dp = (struct direct *)&bp->b_un.b_buf[dpoff];
		if (dpoff == doff)
			break;
		if (dp->d_reclen == 0)
			break;
		dpoff += dp->d_reclen;
	} while (dpoff <= doff);
	if (dpoff > fs->fs_bsize)
		err_suj("Corrupt directory block in dir ino %ju\n",
		    (uintmax_t)parent);
	/* Not found. */
	if (dpoff != doff) {
		if (debug)
			printf("ino %ju not found in %ju, lbn %jd, dpoff %d\n",
			    (uintmax_t)child, (uintmax_t)parent, lbn, dpoff);
		brelse(bp);
		return (0);
	}
	/*
	 * We found the item in question.  Record the mode and whether it's
	 * a . or .. link for the caller.
	 */
	if (dp->d_ino == child) {
		if (child == parent)
			*isdot = 1;
		else if (dp->d_namlen == 2 &&
		    dp->d_name[0] == '.' && dp->d_name[1] == '.')
			*isdot = 1;
		*mode = DTTOIF(dp->d_type);
		brelse(bp);
		return (1);
	}
	if (debug)
		printf("ino %ju doesn't match dirent ino %ju in parent %ju\n",
		    (uintmax_t)child, (uintmax_t)dp->d_ino, (uintmax_t)parent);
	brelse(bp);
	return (0);
}

#define	VISIT_INDIR	0x0001
#define	VISIT_EXT	0x0002
#define	VISIT_ROOT	0x0004	/* Operation came via root & valid pointers. */

/*
 * Read an indirect level which may or may not be linked into an inode.
 */
static void
indir_visit(ino_t ino, ufs_lbn_t lbn, ufs2_daddr_t blk, uint64_t *frags,
    ino_visitor visitor, int flags)
{
	struct bufarea *bp;
	ufs_lbn_t lbnadd;
	ufs2_daddr_t nblk;
	ufs_lbn_t nlbn;
	int level;
	int i;

	/*
	 * Don't visit indirect blocks with contents we can't trust.  This
	 * should only happen when indir_visit() is called to complete a
	 * truncate that never finished and not when a pointer is found via
	 * an inode.
	 */
	if (blk == 0)
		return;
	level = lbn_level(lbn);
	if (level == -1)
		err_suj("Invalid level for lbn %jd\n", lbn);
	if ((flags & VISIT_ROOT) == 0 && blk_isindir(blk, ino, lbn) == 0) {
		if (debug)
			printf("blk %jd ino %ju lbn %jd(%d) is not indir.\n",
			    blk, (uintmax_t)ino, lbn, level);
		goto out;
	}
	lbnadd = 1;
	for (i = level; i > 0; i--)
		lbnadd *= NINDIR(fs);
	bp = getdatablk(blk, fs->fs_bsize, BT_LEVEL1 + level);
	if (bp->b_errs != 0)
		err_suj("indir_visit: UNRECOVERABLE I/O ERROR\n");
	for (i = 0; i < NINDIR(fs); i++) {
		if ((nblk = IBLK(bp, i)) == 0)
			continue;
		if (level == 0) {
			nlbn = -lbn + i * lbnadd;
			(*frags) += fs->fs_frag;
			visitor(ino, nlbn, nblk, fs->fs_frag);
		} else {
			nlbn = (lbn + 1) - (i * lbnadd);
			indir_visit(ino, nlbn, nblk, frags, visitor, flags);
		}
	}
	brelse(bp);
out:
	if (flags & VISIT_INDIR) {
		(*frags) += fs->fs_frag;
		visitor(ino, lbn, blk, fs->fs_frag);
	}
}

/*
 * Visit each block in an inode as specified by 'flags' and call a
 * callback function.  The callback may inspect or free blocks.  The
 * count of frags found according to the size in the file is returned.
 * This is not valid for sparse files but may be used to determine
 * the correct di_blocks for a file.
 */
static uint64_t
ino_visit(union dinode *dp, ino_t ino, ino_visitor visitor, int flags)
{
	ufs_lbn_t nextlbn;
	ufs_lbn_t tmpval;
	ufs_lbn_t lbn;
	uint64_t size;
	uint64_t fragcnt;
	int mode;
	int frags;
	int i;

	size = DIP(dp, di_size);
	mode = DIP(dp, di_mode) & IFMT;
	fragcnt = 0;
	if ((flags & VISIT_EXT) &&
	    fs->fs_magic == FS_UFS2_MAGIC && dp->dp2.di_extsize) {
		for (i = 0; i < UFS_NXADDR; i++) {
			if (dp->dp2.di_extb[i] == 0)
				continue;
			frags = sblksize(fs, dp->dp2.di_extsize, i);
			frags = numfrags(fs, frags);
			fragcnt += frags;
			visitor(ino, -1 - i, dp->dp2.di_extb[i], frags);
		}
	}
	/* Skip datablocks for short links and devices. */
	if (mode == IFBLK || mode == IFCHR ||
	    (mode == IFLNK && size < fs->fs_maxsymlinklen))
		return (fragcnt);
	for (i = 0; i < UFS_NDADDR; i++) {
		if (DIP(dp, di_db[i]) == 0)
			continue;
		frags = sblksize(fs, size, i);
		frags = numfrags(fs, frags);
		fragcnt += frags;
		visitor(ino, i, DIP(dp, di_db[i]), frags);
	}
	/*
	 * We know the following indirects are real as we're following
	 * real pointers to them.
	 */
	flags |= VISIT_ROOT;
	for (i = 0, tmpval = NINDIR(fs), lbn = UFS_NDADDR; i < UFS_NIADDR; i++,
	    lbn = nextlbn) {
		nextlbn = lbn + tmpval;
		tmpval *= NINDIR(fs);
		if (DIP(dp, di_ib[i]) == 0)
			continue;
		indir_visit(ino, -lbn - i, DIP(dp, di_ib[i]), &fragcnt, visitor,
		    flags);
	}
	return (fragcnt);
}

/*
 * Null visitor function used when we just want to count blocks and
 * record the lbn.
 */
ufs_lbn_t visitlbn;
static void
null_visit(ino_t ino, ufs_lbn_t lbn, ufs2_daddr_t blk, int frags)
{
	if (lbn > 0)
		visitlbn = lbn;
}

/*
 * Recalculate di_blocks when we discover that a block allocation or
 * free was not successfully completed.  The kernel does not roll this back
 * because it would be too expensive to compute which indirects were
 * reachable at the time the inode was written.
 */
static void
ino_adjblks(struct suj_ino *sino)
{
	struct inode ip;
	union dinode *dp;
	uint64_t blocks;
	uint64_t frags;
	off_t isize;
	off_t size;
	ino_t ino;

	ino = sino->si_ino;
	ginode(ino, &ip);
	dp = ip.i_dp;
	/* No need to adjust zero'd inodes. */
	if (DIP(dp, di_mode) == 0) {
		irelse(&ip);
		return;
	}
	/*
	 * Visit all blocks and count them as well as recording the last
	 * valid lbn in the file.  If the file size doesn't agree with the
	 * last lbn we need to truncate to fix it.  Otherwise just adjust
	 * the blocks count.
	 */
	visitlbn = 0;
	frags = ino_visit(dp, ino, null_visit, VISIT_INDIR | VISIT_EXT);
	blocks = fsbtodb(fs, frags);
	/*
	 * We assume the size and direct block list is kept coherent by
	 * softdep.  For files that have extended into indirects we truncate
	 * to the size in the inode or the maximum size permitted by
	 * populated indirects.
	 */
	if (visitlbn >= UFS_NDADDR) {
		isize = DIP(dp, di_size);
		size = lblktosize(fs, visitlbn + 1);
		if (isize > size)
			isize = size;
		/* Always truncate to free any unpopulated indirects. */
		ino_trunc(ino, isize);
		irelse(&ip);
		return;
	}
	if (blocks == DIP(dp, di_blocks)) {
		irelse(&ip);
		return;
	}
	if (debug)
		printf("ino %ju adjusting block count from %jd to %jd\n",
		    (uintmax_t)ino, DIP(dp, di_blocks), blocks);
	DIP_SET(dp, di_blocks, blocks);
	inodirty(&ip);
	irelse(&ip);
}

static void
blk_free_visit(ino_t ino, ufs_lbn_t lbn, ufs2_daddr_t blk, int frags)
{

	blk_free(ino, blk, blk_freemask(blk, ino, lbn, frags), frags);
}

/*
 * Free a block or tree of blocks that was previously rooted in ino at
 * the given lbn.  If the lbn is an indirect all children are freed
 * recursively.
 */
static void
blk_free_lbn(ufs2_daddr_t blk, ino_t ino, ufs_lbn_t lbn, int frags, int follow)
{
	uint64_t resid;
	int mask;

	mask = blk_freemask(blk, ino, lbn, frags);
	resid = 0;
	if (lbn <= -UFS_NDADDR && follow && mask == 0)
		indir_visit(ino, lbn, blk, &resid, blk_free_visit, VISIT_INDIR);
	else
		blk_free(ino, blk, mask, frags);
}

static void
ino_setskip(struct suj_ino *sino, ino_t parent)
{
	int isdot;
	int mode;

	if (ino_isat(sino->si_ino, DOTDOT_OFFSET, parent, &mode, &isdot))
		sino->si_skipparent = 1;
}

static void
ino_remref(ino_t parent, ino_t child, uint64_t diroff, int isdotdot)
{
	struct suj_ino *sino;
	struct suj_rec *srec;
	struct jrefrec *rrec;

	/*
	 * Lookup this inode to see if we have a record for it.
	 */
	sino = ino_lookup(child, 0);
	/*
	 * Tell any child directories we've already removed their
	 * parent link cnt.  Don't try to adjust our link down again.
	 */
	if (sino != NULL && isdotdot == 0)
		ino_setskip(sino, parent);
	/*
	 * No valid record for this inode.  Just drop the on-disk
	 * link by one.
	 */
	if (sino == NULL || sino->si_hasrecs == 0) {
		ino_decr(child);
		return;
	}
	/*
	 * Use ino_adjust() if ino_check() has already processed this
	 * child.  If we lose the last non-dot reference to a
	 * directory it will be discarded.
	 */
	if (sino->si_linkadj) {
		if (sino->si_nlink == 0)
			err_suj("ino_remref: ino %ld mode 0%o about to go "
			    "negative\n", sino->si_ino, sino->si_mode);
		sino->si_nlink--;
		if (isdotdot)
			sino->si_dotlinks--;
		ino_adjust(sino);
		return;
	}
	/*
	 * If we haven't yet processed this inode we need to make
	 * sure we will successfully discover the lost path.  If not
	 * use nlinkadj to remember.
	 */
	TAILQ_FOREACH(srec, &sino->si_recs, sr_next) {
		rrec = (struct jrefrec *)srec->sr_rec;
		if (rrec->jr_parent == parent &&
		    rrec->jr_diroff == diroff)
			return;
	}
	sino->si_nlinkadj++;
}

/*
 * Free the children of a directory when the directory is discarded.
 */
static void
ino_free_children(ino_t ino, ufs_lbn_t lbn, ufs2_daddr_t blk, int frags)
{
	struct suj_ino *sino;
	struct bufarea *bp;
	struct direct *dp;
	off_t diroff;
	int skipparent;
	int isdotdot;
	int dpoff;
	int size;

	sino = ino_lookup(ino, 0);
	if (sino)
		skipparent = sino->si_skipparent;
	else
		skipparent = 0;
	size = lfragtosize(fs, frags);
	bp = getdatablk(blk, size, BT_DIRDATA);
	if (bp->b_errs != 0)
		err_suj("ino_free_children: UNRECOVERABLE I/O ERROR");
	dp = (struct direct *)&bp->b_un.b_buf[0];
	for (dpoff = 0; dpoff < size && dp->d_reclen; dpoff += dp->d_reclen) {
		dp = (struct direct *)&bp->b_un.b_buf[dpoff];
		if (dp->d_ino == 0 || dp->d_ino == UFS_WINO)
			continue;
		if (dp->d_namlen == 1 && dp->d_name[0] == '.')
			continue;
		isdotdot = dp->d_namlen == 2 && dp->d_name[0] == '.' &&
		    dp->d_name[1] == '.';
		if (isdotdot && skipparent == 1)
			continue;
		if (debug)
			printf("Directory %ju removing ino %ju name %s\n",
			    (uintmax_t)ino, (uintmax_t)dp->d_ino, dp->d_name);
		diroff = lblktosize(fs, lbn) + dpoff;
		ino_remref(ino, dp->d_ino, diroff, isdotdot);
	}
	brelse(bp);
}

/*
 * Reclaim an inode, freeing all blocks and decrementing all children's
 * link counts.  Free the inode back to the cg.
 */
static void
ino_reclaim(struct inode *ip, ino_t ino, int mode)
{
	union dinode *dp;
	uint32_t gen;

	dp = ip->i_dp;
	if (ino == UFS_ROOTINO)
		err_suj("Attempting to free UFS_ROOTINO\n");
	if (debug)
		printf("Truncating and freeing ino %ju, nlink %d, mode %o\n",
		    (uintmax_t)ino, DIP(dp, di_nlink), DIP(dp, di_mode));

	/* We are freeing an inode or directory. */
	if ((DIP(dp, di_mode) & IFMT) == IFDIR)
		ino_visit(dp, ino, ino_free_children, 0);
	DIP_SET(dp, di_nlink, 0);
	if ((DIP(dp, di_flags) & SF_SNAPSHOT) != 0)
		snapremove(ino);
	ino_visit(dp, ino, blk_free_visit, VISIT_EXT | VISIT_INDIR);
	/* Here we have to clear the inode and release any blocks it holds. */
	gen = DIP(dp, di_gen);
	if (fs->fs_magic == FS_UFS1_MAGIC)
		bzero(dp, sizeof(struct ufs1_dinode));
	else
		bzero(dp, sizeof(struct ufs2_dinode));
	DIP_SET(dp, di_gen, gen);
	inodirty(ip);
	ino_free(ino, mode);
	return;
}

/*
 * Adjust an inode's link count down by one when a directory goes away.
 */
static void
ino_decr(ino_t ino)
{
	struct inode ip;
	union dinode *dp;
	int reqlink;
	int nlink;
	int mode;

	ginode(ino, &ip);
	dp = ip.i_dp;
	nlink = DIP(dp, di_nlink);
	mode = DIP(dp, di_mode);
	if (nlink < 1)
		err_suj("Inode %d link count %d invalid\n", ino, nlink);
	if (mode == 0)
		err_suj("Inode %d has a link of %d with 0 mode\n", ino, nlink);
	nlink--;
	if ((mode & IFMT) == IFDIR)
		reqlink = 2;
	else
		reqlink = 1;
	if (nlink < reqlink) {
		if (debug)
			printf("ino %ju not enough links to live %d < %d\n",
			    (uintmax_t)ino, nlink, reqlink);
		ino_reclaim(&ip, ino, mode);
		irelse(&ip);
		return;
	}
	DIP_SET(dp, di_nlink, nlink);
	inodirty(&ip);
	irelse(&ip);
}

/*
 * Adjust the inode link count to 'nlink'.  If the count reaches zero
 * free it.
 */
static void
ino_adjust(struct suj_ino *sino)
{
	struct jrefrec *rrec;
	struct suj_rec *srec;
	struct suj_ino *stmp;
	union dinode *dp;
	struct inode ip;
	nlink_t nlink;
	nlink_t reqlink;
	int recmode;
	int isdot;
	int mode;
	ino_t ino;

	nlink = sino->si_nlink;
	ino = sino->si_ino;
	mode = sino->si_mode & IFMT;
	/*
	 * If it's a directory with no dot links, it was truncated before
	 * the name was cleared.  We need to clear the dirent that
	 * points at it.
	 */
	if (mode == IFDIR && nlink == 1 && sino->si_dotlinks == 0) {
		sino->si_nlink = nlink = 0;
		TAILQ_FOREACH(srec, &sino->si_recs, sr_next) {
			rrec = (struct jrefrec *)srec->sr_rec;
			if (ino_isat(rrec->jr_parent, rrec->jr_diroff, ino,
			    &recmode, &isdot) == 0)
				continue;
			ino_clrat(rrec->jr_parent, rrec->jr_diroff, ino);
			break;
		}
		if (srec == NULL)
			errx(1, "Directory %ju name not found", (uintmax_t)ino);
	}
	/*
	 * If it's a directory with no real names pointing to it go ahead
	 * and truncate it.  This will free any children.
	 */
	if (mode == IFDIR && nlink - sino->si_dotlinks == 0) {
		sino->si_nlink = nlink = 0;
		/*
		 * Mark any .. links so they know not to free this inode
		 * when they are removed.
		 */
		TAILQ_FOREACH(srec, &sino->si_recs, sr_next) {
			rrec = (struct jrefrec *)srec->sr_rec;
			if (rrec->jr_diroff == DOTDOT_OFFSET) {
				stmp = ino_lookup(rrec->jr_parent, 0);
				if (stmp)
					ino_setskip(stmp, ino);
			}
		}
	}
	ginode(ino, &ip);
	dp = ip.i_dp;
	mode = DIP(dp, di_mode) & IFMT;
	if (nlink > UFS_LINK_MAX)
		err_suj("ino %ju nlink manipulation error, new %ju, old %d\n",
		    (uintmax_t)ino, (uintmax_t)nlink, DIP(dp, di_nlink));
	if (debug)
	       printf("Adjusting ino %ju, nlink %ju, old link %d lastmode %o\n",
		    (uintmax_t)ino, (uintmax_t)nlink, DIP(dp, di_nlink),
		    sino->si_mode);
	if (mode == 0) {
		if (debug)
			printf("ino %ju, zero inode freeing bitmap\n",
			    (uintmax_t)ino);
		ino_free(ino, sino->si_mode);
		irelse(&ip);
		return;
	}
	/* XXX Should be an assert? */
	if (mode != sino->si_mode && debug)
		printf("ino %ju, mode %o != %o\n",
		    (uintmax_t)ino, mode, sino->si_mode);
	if ((mode & IFMT) == IFDIR)
		reqlink = 2;
	else
		reqlink = 1;
	/* If the inode doesn't have enough links to live, free it. */
	if (nlink < reqlink) {
		if (debug)
			printf("ino %ju not enough links to live %ju < %ju\n",
			    (uintmax_t)ino, (uintmax_t)nlink,
			    (uintmax_t)reqlink);
		ino_reclaim(&ip, ino, mode);
		irelse(&ip);
		return;
	}
	/* If required write the updated link count. */
	if (DIP(dp, di_nlink) == nlink) {
		if (debug)
			printf("ino %ju, link matches, skipping.\n",
			    (uintmax_t)ino);
		irelse(&ip);
		return;
	}
	DIP_SET(dp, di_nlink, nlink);
	inodirty(&ip);
	irelse(&ip);
}

/*
 * Truncate some or all blocks in an indirect, freeing any that are required
 * and zeroing the indirect.
 */
static void
indir_trunc(ino_t ino, ufs_lbn_t lbn, ufs2_daddr_t blk, ufs_lbn_t lastlbn,
	union dinode *dp)
{
	struct bufarea *bp;
	ufs_lbn_t lbnadd;
	ufs2_daddr_t nblk;
	ufs_lbn_t next;
	ufs_lbn_t nlbn;
	int isdirty;
	int level;
	int i;

	if (blk == 0)
		return;
	isdirty = 0;
	level = lbn_level(lbn);
	if (level == -1)
		err_suj("Invalid level for lbn %jd\n", lbn);
	lbnadd = 1;
	for (i = level; i > 0; i--)
		lbnadd *= NINDIR(fs);
	bp = getdatablk(blk, fs->fs_bsize, BT_LEVEL1 + level);
	if (bp->b_errs != 0)
		err_suj("indir_trunc: UNRECOVERABLE I/O ERROR");
	for (i = 0; i < NINDIR(fs); i++) {
		if ((nblk = IBLK(bp, i)) == 0)
			continue;
		if (level != 0) {
			nlbn = (lbn + 1) - (i * lbnadd);
			/*
			 * Calculate the lbn of the next indirect to
			 * determine if any of this indirect must be
			 * reclaimed.
			 */
			next = -(lbn + level) + ((i+1) * lbnadd);
			if (next <= lastlbn)
				continue;
			indir_trunc(ino, nlbn, nblk, lastlbn, dp);
			/* If all of this indirect was reclaimed, free it. */
			nlbn = next - lbnadd;
			if (nlbn < lastlbn)
				continue;
		} else {
			nlbn = -lbn + i * lbnadd;
			if (nlbn < lastlbn)
				continue;
		}
		isdirty = 1;
		blk_free(ino, nblk, 0, fs->fs_frag);
		IBLK_SET(bp, i, 0);
	}
	if (isdirty)
		dirty(bp);
	brelse(bp);
}

/*
 * Truncate an inode to the minimum of the given size or the last populated
 * block after any over size have been discarded.  The kernel would allocate
 * the last block in the file but fsck does not and neither do we.  This
 * code never extends files, only shrinks them.
 */
static void
ino_trunc(ino_t ino, off_t size)
{
	struct inode ip;
	union dinode *dp;
	struct bufarea *bp;
	ufs2_daddr_t bn;
	uint64_t totalfrags;
	ufs_lbn_t nextlbn;
	ufs_lbn_t lastlbn;
	ufs_lbn_t tmpval;
	ufs_lbn_t lbn;
	ufs_lbn_t i;
	int blksize, frags;
	off_t cursize;
	off_t off;
	int mode;

	ginode(ino, &ip);
	dp = ip.i_dp;
	mode = DIP(dp, di_mode) & IFMT;
	cursize = DIP(dp, di_size);
	/* If no size change, nothing to do */
	if (size == cursize) {
		irelse(&ip);
		return;
	}
	if (debug)
		printf("Truncating ino %ju, mode %o to size %jd from "
		    "size %jd\n", (uintmax_t)ino, mode, size, cursize);

	/* Skip datablocks for short links and devices. */
	if (mode == 0 || mode == IFBLK || mode == IFCHR ||
	    (mode == IFLNK && cursize < fs->fs_maxsymlinklen)) {
		irelse(&ip);
		return;
	}
	/* Don't extend. */
	if (size > cursize) {
		irelse(&ip);
		return;
	}
	if ((DIP(dp, di_flags) & SF_SNAPSHOT) != 0) {
		if (size > 0)
			err_suj("Partial truncation of ino %ju snapshot file\n",
			    (uintmax_t)ino);
		snapremove(ino);
	}
	lastlbn = lblkno(fs, blkroundup(fs, size));
	for (i = lastlbn; i < UFS_NDADDR; i++) {
		if ((bn = DIP(dp, di_db[i])) == 0)
			continue;
		blksize = sblksize(fs, cursize, i);
		blk_free(ino, bn, 0, numfrags(fs, blksize));
		DIP_SET(dp, di_db[i], 0);
	}
	/*
	 * Follow indirect blocks, freeing anything required.
	 */
	for (i = 0, tmpval = NINDIR(fs), lbn = UFS_NDADDR; i < UFS_NIADDR; i++,
	    lbn = nextlbn) {
		nextlbn = lbn + tmpval;
		tmpval *= NINDIR(fs);
		/* If we're not freeing any in this indirect range skip it. */
		if (lastlbn >= nextlbn)
			continue;
		if ((bn = DIP(dp, di_ib[i])) == 0)
  			continue;
		indir_trunc(ino, -lbn - i, bn, lastlbn, dp);
  		/* If we freed everything in this indirect free the indir. */
  		if (lastlbn > lbn)
  			continue;
		blk_free(ino, bn, 0, fs->fs_frag);
		DIP_SET(dp, di_ib[i], 0);
	}
	/*
	 * Now that we've freed any whole blocks that exceed the desired
	 * truncation size, figure out how many blocks remain and what the
	 * last populated lbn is.  We will set the size to this last lbn
	 * rather than worrying about allocating the final lbn as the kernel
	 * would've done.  This is consistent with normal fsck behavior.
	 */
	visitlbn = 0;
	totalfrags = ino_visit(dp, ino, null_visit, VISIT_INDIR | VISIT_EXT);
	if (size > lblktosize(fs, visitlbn + 1))
		size = lblktosize(fs, visitlbn + 1);
	/*
	 * If we're truncating direct blocks we have to adjust frags
	 * accordingly.
	 */
	if (visitlbn < UFS_NDADDR && totalfrags) {
		long oldspace, newspace;

		bn = DIP(dp, di_db[visitlbn]);
		if (bn == 0)
			err_suj("Bad blk at ino %ju lbn %jd\n",
			    (uintmax_t)ino, visitlbn);
		oldspace = sblksize(fs, cursize, visitlbn);
		newspace = sblksize(fs, size, visitlbn);
		if (oldspace != newspace) {
			bn += numfrags(fs, newspace);
			frags = numfrags(fs, oldspace - newspace);
			blk_free(ino, bn, 0, frags);
			totalfrags -= frags;
		}
	}
	DIP_SET(dp, di_blocks, fsbtodb(fs, totalfrags));
	DIP_SET(dp, di_size, size);
	inodirty(&ip);
	/*
	 * If we've truncated into the middle of a block or frag we have
	 * to zero it here.  Otherwise the file could extend into
	 * uninitialized space later.
	 */
	off = blkoff(fs, size);
	if (off && DIP(dp, di_mode) != IFDIR) {
		long clrsize;

		bn = ino_blkatoff(dp, ino, visitlbn, &frags, NULL);
		if (bn == 0)
			err_suj("Block missing from ino %ju at lbn %jd\n",
			    (uintmax_t)ino, visitlbn);
		clrsize = frags * fs->fs_fsize;
		bp = getdatablk(bn, clrsize, BT_DATA);
		if (bp->b_errs != 0)
			err_suj("ino_trunc: UNRECOVERABLE I/O ERROR");
		clrsize -= off;
		bzero(&bp->b_un.b_buf[off], clrsize);
		dirty(bp);
		brelse(bp);
	}
	irelse(&ip);
	return;
}

/*
 * Process records available for one inode and determine whether the
 * link count is correct or needs adjusting.
 */
static void
ino_check(struct suj_ino *sino)
{
	struct suj_rec *srec;
	struct jrefrec *rrec;
	nlink_t dotlinks;
	nlink_t newlinks;
	nlink_t removes;
	nlink_t nlink;
	ino_t ino;
	int isdot;
	int isat;
	int mode;

	if (sino->si_hasrecs == 0)
		return;
	ino = sino->si_ino;
	rrec = (struct jrefrec *)TAILQ_FIRST(&sino->si_recs)->sr_rec;
	nlink = rrec->jr_nlink;
	newlinks = 0;
	dotlinks = 0;
	removes = sino->si_nlinkadj;
	TAILQ_FOREACH(srec, &sino->si_recs, sr_next) {
		rrec = (struct jrefrec *)srec->sr_rec;
		isat = ino_isat(rrec->jr_parent, rrec->jr_diroff,
		    rrec->jr_ino, &mode, &isdot);
		if (isat && (mode & IFMT) != (rrec->jr_mode & IFMT))
			err_suj("Inode mode/directory type mismatch %o != %o\n",
			    mode, rrec->jr_mode);
		if (debug)
			printf("jrefrec: op %s ino %ju, nlink %ju, parent %ju, "
			    "diroff %jd, mode %o, isat %d, isdot %d\n",
			    JOP_OPTYPE(rrec->jr_op), (uintmax_t)rrec->jr_ino,
			    (uintmax_t)rrec->jr_nlink,
			    (uintmax_t)rrec->jr_parent,
			    (uintmax_t)rrec->jr_diroff,
			    rrec->jr_mode, isat, isdot);
		mode = rrec->jr_mode & IFMT;
		if (rrec->jr_op == JOP_REMREF)
			removes++;
		newlinks += isat;
		if (isdot)
			dotlinks += isat;
	}
	/*
	 * The number of links that remain are the starting link count
	 * subtracted by the total number of removes with the total
	 * links discovered back in.  An incomplete remove thus
	 * makes no change to the link count but an add increases
	 * by one.
	 */
	if (debug)
		printf(
		    "ino %ju nlink %ju newlinks %ju removes %ju dotlinks %ju\n",
		    (uintmax_t)ino, (uintmax_t)nlink, (uintmax_t)newlinks,
		    (uintmax_t)removes, (uintmax_t)dotlinks);
	nlink += newlinks;
	nlink -= removes;
	sino->si_linkadj = 1;
	sino->si_nlink = nlink;
	sino->si_dotlinks = dotlinks;
	sino->si_mode = mode;
	ino_adjust(sino);
}

/*
 * Process records available for one block and determine whether it is
 * still allocated and whether the owning inode needs to be updated or
 * a free completed.
 */
static void
blk_check(struct suj_blk *sblk)
{
	struct suj_rec *srec;
	struct jblkrec *brec;
	struct suj_ino *sino;
	ufs2_daddr_t blk;
	int mask;
	int frags;
	int isat;

	/*
	 * Each suj_blk actually contains records for any fragments in that
	 * block.  As a result we must evaluate each record individually.
	 */
	sino = NULL;
	TAILQ_FOREACH(srec, &sblk->sb_recs, sr_next) {
		brec = (struct jblkrec *)srec->sr_rec;
		frags = brec->jb_frags;
		blk = brec->jb_blkno + brec->jb_oldfrags;
		isat = blk_isat(brec->jb_ino, brec->jb_lbn, blk, &frags);
		if (sino == NULL || sino->si_ino != brec->jb_ino) {
			sino = ino_lookup(brec->jb_ino, 1);
			sino->si_blkadj = 1;
		}
		if (debug)
			printf("op %s blk %jd ino %ju lbn %jd frags %d isat %d "
			    "(%d)\n", JOP_OPTYPE(brec->jb_op), blk,
			    (uintmax_t)brec->jb_ino, brec->jb_lbn,
			    brec->jb_frags, isat, frags);
		/*
		 * If we found the block at this address we still have to
		 * determine if we need to free the tail end that was
		 * added by adding contiguous fragments from the same block.
		 */
		if (isat == 1) {
			if (frags == brec->jb_frags)
				continue;
			mask = blk_freemask(blk, brec->jb_ino, brec->jb_lbn,
			    brec->jb_frags);
			mask >>= frags;
			blk += frags;
			frags = brec->jb_frags - frags;
			blk_free(brec->jb_ino, blk, mask, frags);
			continue;
		}
		/*
	 	 * The block wasn't found, attempt to free it.  It won't be
		 * freed if it was actually reallocated.  If this was an
		 * allocation we don't want to follow indirects as they
		 * may not be written yet.  Any children of the indirect will
		 * have their own records.  If it's a free we need to
		 * recursively free children.
		 */
		blk_free_lbn(blk, brec->jb_ino, brec->jb_lbn, brec->jb_frags,
		    brec->jb_op == JOP_FREEBLK);
	}
}

/*
 * Walk the list of inode records for this cg and resolve moved and duplicate
 * inode references now that we have a complete picture.
 */
static void
cg_build(struct suj_cg *sc)
{
	struct suj_ino *sino;
	int i;

	for (i = 0; i < HASHSIZE; i++)
		LIST_FOREACH(sino, &sc->sc_inohash[i], si_next)
			ino_build(sino);
}

/*
 * Handle inodes requiring truncation.  This must be done prior to
 * looking up any inodes in directories.
 */
static void
cg_trunc(struct suj_cg *sc)
{
	struct suj_ino *sino;
	int i;

	for (i = 0; i < HASHSIZE; i++) {
		LIST_FOREACH(sino, &sc->sc_inohash[i], si_next) {
			if (sino->si_trunc) {
				ino_trunc(sino->si_ino,
				    sino->si_trunc->jt_size);
				sino->si_blkadj = 0;
				sino->si_trunc = NULL;
			}
			if (sino->si_blkadj)
				ino_adjblks(sino);
		}
	}
}

static void
cg_adj_blk(struct suj_cg *sc)
{
	struct suj_ino *sino;
	int i;

	for (i = 0; i < HASHSIZE; i++) {
		LIST_FOREACH(sino, &sc->sc_inohash[i], si_next) {
			if (sino->si_blkadj)
				ino_adjblks(sino);
		}
	}
}

/*
 * Free any partially allocated blocks and then resolve inode block
 * counts.
 */
static void
cg_check_blk(struct suj_cg *sc)
{
	struct suj_blk *sblk;
	int i;


	for (i = 0; i < HASHSIZE; i++)
		LIST_FOREACH(sblk, &sc->sc_blkhash[i], sb_next)
			blk_check(sblk);
}

/*
 * Walk the list of inode records for this cg, recovering any
 * changes which were not complete at the time of crash.
 */
static void
cg_check_ino(struct suj_cg *sc)
{
	struct suj_ino *sino;
	int i;

	for (i = 0; i < HASHSIZE; i++)
		LIST_FOREACH(sino, &sc->sc_inohash[i], si_next)
			ino_check(sino);
}

static void
cg_apply(void (*apply)(struct suj_cg *))
{
	struct suj_cg *scg;
	int i;

	for (i = 0; i < HASHSIZE; i++)
		LIST_FOREACH(scg, &cghash[i], sc_next)
			apply(scg);
}

/*
 * Process the unlinked but referenced file list.  Freeing all inodes.
 */
static void
ino_unlinked(void)
{
	struct inode ip;
	union dinode *dp;
	uint16_t mode;
	ino_t inon;
	ino_t ino;

	ino = fs->fs_sujfree;
	fs->fs_sujfree = 0;
	while (ino != 0) {
		ginode(ino, &ip);
		dp = ip.i_dp;
		mode = DIP(dp, di_mode) & IFMT;
		inon = DIP(dp, di_freelink);
		DIP_SET(dp, di_freelink, 0);
		inodirty(&ip);
		/*
		 * XXX Should this be an errx?
		 */
		if (DIP(dp, di_nlink) == 0) {
			if (debug)
				printf("Freeing unlinked ino %ju mode %o\n",
				    (uintmax_t)ino, mode);
			ino_reclaim(&ip, ino, mode);
		} else if (debug)
			printf("Skipping ino %ju mode %o with link %d\n",
			    (uintmax_t)ino, mode, DIP(dp, di_nlink));
		ino = inon;
		irelse(&ip);
	}
}

/*
 * Append a new record to the list of records requiring processing.
 */
static void
ino_append(union jrec *rec)
{
	struct jrefrec *refrec;
	struct jmvrec *mvrec;
	struct suj_ino *sino;
	struct suj_rec *srec;

	mvrec = &rec->rec_jmvrec;
	refrec = &rec->rec_jrefrec;
	if (debug && mvrec->jm_op == JOP_MVREF)
		printf("ino move: ino %ju, parent %ju, "
		    "diroff %jd, oldoff %jd\n",
		    (uintmax_t)mvrec->jm_ino, (uintmax_t)mvrec->jm_parent,
		    (uintmax_t)mvrec->jm_newoff, (uintmax_t)mvrec->jm_oldoff);
	else if (debug &&
	    (refrec->jr_op == JOP_ADDREF || refrec->jr_op == JOP_REMREF))
		printf("ino ref: op %s, ino %ju, nlink %ju, "
		    "parent %ju, diroff %jd\n",
		    JOP_OPTYPE(refrec->jr_op), (uintmax_t)refrec->jr_ino,
		    (uintmax_t)refrec->jr_nlink,
		    (uintmax_t)refrec->jr_parent, (uintmax_t)refrec->jr_diroff);
	sino = ino_lookup(((struct jrefrec *)rec)->jr_ino, 1);
	sino->si_hasrecs = 1;
	srec = errmalloc(sizeof(*srec));
	srec->sr_rec = rec;
	TAILQ_INSERT_TAIL(&sino->si_newrecs, srec, sr_next);
}

/*
 * Add a reference adjustment to the sino list and eliminate dups.  The
 * primary loop in ino_build_ref() checks for dups but new ones may be
 * created as a result of offset adjustments.
 */
static void
ino_add_ref(struct suj_ino *sino, struct suj_rec *srec)
{
	struct jrefrec *refrec;
	struct suj_rec *srn;
	struct jrefrec *rrn;

	refrec = (struct jrefrec *)srec->sr_rec;
	/*
	 * We walk backwards so that the oldest link count is preserved.  If
	 * an add record conflicts with a remove keep the remove.  Redundant
	 * removes are eliminated in ino_build_ref.  Otherwise we keep the
	 * oldest record at a given location.
	 */
	for (srn = TAILQ_LAST(&sino->si_recs, srechd); srn;
	    srn = TAILQ_PREV(srn, srechd, sr_next)) {
		rrn = (struct jrefrec *)srn->sr_rec;
		if (rrn->jr_parent != refrec->jr_parent ||
		    rrn->jr_diroff != refrec->jr_diroff)
			continue;
		if (rrn->jr_op == JOP_REMREF || refrec->jr_op == JOP_ADDREF) {
			rrn->jr_mode = refrec->jr_mode;
			return;
		}
		/*
		 * Adding a remove.
		 *
		 * Replace the record in place with the old nlink in case
		 * we replace the head of the list.  Abandon srec as a dup.
		 */
		refrec->jr_nlink = rrn->jr_nlink;
		srn->sr_rec = srec->sr_rec;
		return;
	}
	TAILQ_INSERT_TAIL(&sino->si_recs, srec, sr_next);
}

/*
 * Create a duplicate of a reference at a previous location.
 */
static void
ino_dup_ref(struct suj_ino *sino, struct jrefrec *refrec, off_t diroff)
{
	struct jrefrec *rrn;
	struct suj_rec *srn;

	rrn = errmalloc(sizeof(*refrec));
	*rrn = *refrec;
	rrn->jr_op = JOP_ADDREF;
	rrn->jr_diroff = diroff;
	srn = errmalloc(sizeof(*srn));
	srn->sr_rec = (union jrec *)rrn;
	ino_add_ref(sino, srn);
}

/*
 * Add a reference to the list at all known locations.  We follow the offset
 * changes for a single instance and create duplicate add refs at each so
 * that we can tolerate any version of the directory block.  Eliminate
 * removes which collide with adds that are seen in the journal.  They should
 * not adjust the link count down.
 */
static void
ino_build_ref(struct suj_ino *sino, struct suj_rec *srec)
{
	struct jrefrec *refrec;
	struct jmvrec *mvrec;
	struct suj_rec *srp;
	struct suj_rec *srn;
	struct jrefrec *rrn;
	off_t diroff;

	refrec = (struct jrefrec *)srec->sr_rec;
	/*
	 * Search for a mvrec that matches this offset.  Whether it's an add
	 * or a remove we can delete the mvref after creating a dup record in
	 * the old location.
	 */
	if (!TAILQ_EMPTY(&sino->si_movs)) {
		diroff = refrec->jr_diroff;
		for (srn = TAILQ_LAST(&sino->si_movs, srechd); srn; srn = srp) {
			srp = TAILQ_PREV(srn, srechd, sr_next);
			mvrec = (struct jmvrec *)srn->sr_rec;
			if (mvrec->jm_parent != refrec->jr_parent ||
			    mvrec->jm_newoff != diroff)
				continue;
			diroff = mvrec->jm_oldoff;
			TAILQ_REMOVE(&sino->si_movs, srn, sr_next);
			free(srn);
			ino_dup_ref(sino, refrec, diroff);
		}
	}
	/*
	 * If a remove wasn't eliminated by an earlier add just append it to
	 * the list.
	 */
	if (refrec->jr_op == JOP_REMREF) {
		ino_add_ref(sino, srec);
		return;
	}
	/*
	 * Walk the list of records waiting to be added to the list.  We
	 * must check for moves that apply to our current offset and remove
	 * them from the list.  Remove any duplicates to eliminate removes
	 * with corresponding adds.
	 */
	TAILQ_FOREACH_SAFE(srn, &sino->si_newrecs, sr_next, srp) {
		switch (srn->sr_rec->rec_jrefrec.jr_op) {
		case JOP_ADDREF:
			/*
			 * This should actually be an error we should
			 * have a remove for every add journaled.
			 */
			rrn = (struct jrefrec *)srn->sr_rec;
			if (rrn->jr_parent != refrec->jr_parent ||
			    rrn->jr_diroff != refrec->jr_diroff)
				break;
			TAILQ_REMOVE(&sino->si_newrecs, srn, sr_next);
			break;
		case JOP_REMREF:
			/*
			 * Once we remove the current iteration of the
			 * record at this address we're done.
			 */
			rrn = (struct jrefrec *)srn->sr_rec;
			if (rrn->jr_parent != refrec->jr_parent ||
			    rrn->jr_diroff != refrec->jr_diroff)
				break;
			TAILQ_REMOVE(&sino->si_newrecs, srn, sr_next);
			ino_add_ref(sino, srec);
			return;
		case JOP_MVREF:
			/*
			 * Update our diroff based on any moves that match
			 * and remove the move.
			 */
			mvrec = (struct jmvrec *)srn->sr_rec;
			if (mvrec->jm_parent != refrec->jr_parent ||
			    mvrec->jm_oldoff != refrec->jr_diroff)
				break;
			ino_dup_ref(sino, refrec, mvrec->jm_oldoff);
			refrec->jr_diroff = mvrec->jm_newoff;
			TAILQ_REMOVE(&sino->si_newrecs, srn, sr_next);
			break;
		default:
			err_suj("ino_build_ref: Unknown op %s\n",
			    JOP_OPTYPE(srn->sr_rec->rec_jrefrec.jr_op));
		}
	}
	ino_add_ref(sino, srec);
}

/*
 * Walk the list of new records and add them in-order resolving any
 * dups and adjusted offsets.
 */
static void
ino_build(struct suj_ino *sino)
{
	struct suj_rec *srec;

	while ((srec = TAILQ_FIRST(&sino->si_newrecs)) != NULL) {
		TAILQ_REMOVE(&sino->si_newrecs, srec, sr_next);
		switch (srec->sr_rec->rec_jrefrec.jr_op) {
		case JOP_ADDREF:
		case JOP_REMREF:
			ino_build_ref(sino, srec);
			break;
		case JOP_MVREF:
			/*
			 * Add this mvrec to the queue of pending mvs.
			 */
			TAILQ_INSERT_TAIL(&sino->si_movs, srec, sr_next);
			break;
		default:
			err_suj("ino_build: Unknown op %s\n",
			    JOP_OPTYPE(srec->sr_rec->rec_jrefrec.jr_op));
		}
	}
	if (TAILQ_EMPTY(&sino->si_recs))
		sino->si_hasrecs = 0;
}

/*
 * Modify journal records so they refer to the base block number
 * and a start and end frag range.  This is to facilitate the discovery
 * of overlapping fragment allocations.
 */
static void
blk_build(struct jblkrec *blkrec)
{
	struct suj_rec *srec;
	struct suj_blk *sblk;
	struct jblkrec *blkrn;
	ufs2_daddr_t blk;
	int frag;

	if (debug)
		printf("blk_build: op %s blkno %jd frags %d oldfrags %d "
		    "ino %ju lbn %jd\n",
		    JOP_OPTYPE(blkrec->jb_op), (uintmax_t)blkrec->jb_blkno,
		    blkrec->jb_frags, blkrec->jb_oldfrags,
		    (uintmax_t)blkrec->jb_ino, (uintmax_t)blkrec->jb_lbn);

	blk = blknum(fs, blkrec->jb_blkno);
	frag = fragnum(fs, blkrec->jb_blkno);
	if (blkrec->jb_blkno < 0 || blk + fs->fs_frag - frag > fs->fs_size)
		err_suj("Out-of-bounds journal block number %jd\n",
		    blkrec->jb_blkno);
	sblk = blk_lookup(blk, 1);
	/*
	 * Rewrite the record using oldfrags to indicate the offset into
	 * the block.  Leave jb_frags as the actual allocated count.
	 */
	blkrec->jb_blkno -= frag;
	blkrec->jb_oldfrags = frag;
	if (blkrec->jb_oldfrags + blkrec->jb_frags > fs->fs_frag)
		err_suj("Invalid fragment count %d oldfrags %d\n",
		    blkrec->jb_frags, frag);
	/*
	 * Detect dups.  If we detect a dup we always discard the oldest
	 * record as it is superseded by the new record.  This speeds up
	 * later stages but also eliminates free records which are used
	 * to indicate that the contents of indirects can be trusted.
	 */
	TAILQ_FOREACH(srec, &sblk->sb_recs, sr_next) {
		blkrn = (struct jblkrec *)srec->sr_rec;
		if (blkrn->jb_ino != blkrec->jb_ino ||
		    blkrn->jb_lbn != blkrec->jb_lbn ||
		    blkrn->jb_blkno != blkrec->jb_blkno ||
		    blkrn->jb_frags != blkrec->jb_frags ||
		    blkrn->jb_oldfrags != blkrec->jb_oldfrags)
			continue;
		if (debug)
			printf("Removed dup.\n");
		/* Discard the free which is a dup with an alloc. */
		if (blkrec->jb_op == JOP_FREEBLK)
			return;
		TAILQ_REMOVE(&sblk->sb_recs, srec, sr_next);
		free(srec);
		break;
	}
	srec = errmalloc(sizeof(*srec));
	srec->sr_rec = (union jrec *)blkrec;
	TAILQ_INSERT_TAIL(&sblk->sb_recs, srec, sr_next);
}

static void
ino_build_trunc(struct jtrncrec *rec)
{
	struct suj_ino *sino;

	if (debug)
		printf("ino_build_trunc: op %d ino %ju, size %jd\n",
		    rec->jt_op, (uintmax_t)rec->jt_ino,
		    (uintmax_t)rec->jt_size);
	if (chkfilesize(IFREG, rec->jt_size) == 0)
		err_suj("ino_build: truncation size too large %ju\n",
		    (intmax_t)rec->jt_size);
	sino = ino_lookup(rec->jt_ino, 1);
	if (rec->jt_op == JOP_SYNC) {
		sino->si_trunc = NULL;
		return;
	}
	if (sino->si_trunc == NULL || sino->si_trunc->jt_size > rec->jt_size)
		sino->si_trunc = rec;
}

/*
 * Build up tables of the operations we need to recover.
 */
static void
suj_build(void)
{
	struct suj_seg *seg;
	union jrec *rec;
	int off;
	int i;

	TAILQ_FOREACH(seg, &allsegs, ss_next) {
		if (debug)
			printf("seg %jd has %d records, oldseq %jd.\n",
			    seg->ss_rec.jsr_seq, seg->ss_rec.jsr_cnt,
			    seg->ss_rec.jsr_oldest);
		off = 0;
		rec = (union jrec *)seg->ss_blk;
		for (i = 0; i < seg->ss_rec.jsr_cnt; off += JREC_SIZE, rec++) {
			/* skip the segrec. */
			if ((off % real_dev_bsize) == 0)
				continue;
			switch (rec->rec_jrefrec.jr_op) {
			case JOP_ADDREF:
			case JOP_REMREF:
			case JOP_MVREF:
				ino_append(rec);
				break;
			case JOP_NEWBLK:
			case JOP_FREEBLK:
				blk_build((struct jblkrec *)rec);
				break;
			case JOP_TRUNC:
			case JOP_SYNC:
				ino_build_trunc((struct jtrncrec *)rec);
				break;
			default:
				err_suj("Unknown journal operation %s at %d\n",
				    JOP_OPTYPE(rec->rec_jrefrec.jr_op), off);
			}
			i++;
		}
	}
}

/*
 * Prune the journal segments to those we care about based on the
 * oldest sequence in the newest segment.  Order the segment list
 * based on sequence number.
 */
static void
suj_prune(void)
{
	struct suj_seg *seg;
	struct suj_seg *segn;
	uint64_t newseq;
	int discard;

	if (debug)
		printf("Pruning up to %jd\n", oldseq);
	/* First free the expired segments. */
	TAILQ_FOREACH_SAFE(seg, &allsegs, ss_next, segn) {
		if (seg->ss_rec.jsr_seq >= oldseq)
			continue;
		TAILQ_REMOVE(&allsegs, seg, ss_next);
		free(seg->ss_blk);
		free(seg);
	}
	/* Next ensure that segments are ordered properly. */
	seg = TAILQ_FIRST(&allsegs);
	if (seg == NULL) {
		if (debug)
			printf("Empty journal\n");
		return;
	}
	newseq = seg->ss_rec.jsr_seq;
	for (;;) {
		seg = TAILQ_LAST(&allsegs, seghd);
		if (seg->ss_rec.jsr_seq >= newseq)
			break;
		TAILQ_REMOVE(&allsegs, seg, ss_next);
		TAILQ_INSERT_HEAD(&allsegs, seg, ss_next);
		newseq = seg->ss_rec.jsr_seq;

	}
	if (newseq != oldseq) {
		TAILQ_FOREACH(seg, &allsegs, ss_next) {
			printf("%jd, ", seg->ss_rec.jsr_seq);
		}
		printf("\n");
		err_suj("Journal file sequence mismatch %jd != %jd\n",
		    newseq, oldseq);
	}
	/*
	 * The kernel may asynchronously write segments which can create
	 * gaps in the sequence space.  Throw away any segments after the
	 * gap as the kernel guarantees only those that are contiguously
	 * reachable are marked as completed.
	 */
	discard = 0;
	TAILQ_FOREACH_SAFE(seg, &allsegs, ss_next, segn) {
		if (!discard && newseq++ == seg->ss_rec.jsr_seq) {
			jrecs += seg->ss_rec.jsr_cnt;
			jbytes += seg->ss_rec.jsr_blocks * real_dev_bsize;
			continue;
		}
		discard = 1;
		if (debug)
			printf("Journal order mismatch %jd != %jd pruning\n",
			    newseq-1, seg->ss_rec.jsr_seq);
		TAILQ_REMOVE(&allsegs, seg, ss_next);
		free(seg->ss_blk);
		free(seg);
	}
	if (debug)
		printf("Processing journal segments from %jd to %jd\n",
		    oldseq, newseq-1);
}

/*
 * Verify the journal inode before attempting to read records.
 */
static int
suj_verifyino(union dinode *dp)
{

	if (DIP(dp, di_nlink) != 1) {
		printf("Invalid link count %d for journal inode %ju\n",
		    DIP(dp, di_nlink), (uintmax_t)sujino);
		return (-1);
	}

	if ((DIP(dp, di_flags) & (SF_IMMUTABLE | SF_NOUNLINK)) !=
	    (SF_IMMUTABLE | SF_NOUNLINK)) {
		printf("Invalid flags 0x%X for journal inode %ju\n",
		    DIP(dp, di_flags), (uintmax_t)sujino);
		return (-1);
	}

	if (DIP(dp, di_mode) != (IFREG | IREAD)) {
		printf("Invalid mode %o for journal inode %ju\n",
		    DIP(dp, di_mode), (uintmax_t)sujino);
		return (-1);
	}

	if (DIP(dp, di_size) < SUJ_MIN) {
		printf("Invalid size %jd for journal inode %ju\n",
		    DIP(dp, di_size), (uintmax_t)sujino);
		return (-1);
	}

	if (DIP(dp, di_modrev) != fs->fs_mtime) {
		if (!bkgrdcheck || debug)
			printf("Journal timestamp does not match "
			    "fs mount time\n");
		return (-1);
	}

	return (0);
}

struct jblocks {
	struct jextent *jb_extent;	/* Extent array. */
	int		jb_avail;	/* Available extents. */
	int		jb_used;	/* Last used extent. */
	int		jb_head;	/* Allocator head. */
	int		jb_off;		/* Allocator extent offset. */
};
struct jextent {
	ufs2_daddr_t	je_daddr;	/* Disk block address. */
	int		je_blocks;	/* Disk block count. */
};

static struct jblocks *suj_jblocks;

static struct jblocks *
jblocks_create(void)
{
	struct jblocks *jblocks;
	int size;

	jblocks = errmalloc(sizeof(*jblocks));
	jblocks->jb_avail = 10;
	jblocks->jb_used = 0;
	jblocks->jb_head = 0;
	jblocks->jb_off = 0;
	size = sizeof(struct jextent) * jblocks->jb_avail;
	jblocks->jb_extent = errmalloc(size);
	bzero(jblocks->jb_extent, size);

	return (jblocks);
}

/*
 * Return the next available disk block and the amount of contiguous
 * free space it contains.
 */
static ufs2_daddr_t
jblocks_next(struct jblocks *jblocks, int bytes, int *actual)
{
	struct jextent *jext;
	ufs2_daddr_t daddr;
	int freecnt;
	int blocks;

	blocks = btodb(bytes);
	jext = &jblocks->jb_extent[jblocks->jb_head];
	freecnt = jext->je_blocks - jblocks->jb_off;
	if (freecnt == 0) {
		jblocks->jb_off = 0;
		if (++jblocks->jb_head > jblocks->jb_used)
			return (0);
		jext = &jblocks->jb_extent[jblocks->jb_head];
		freecnt = jext->je_blocks;
	}
	if (freecnt > blocks)
		freecnt = blocks;
	*actual = dbtob(freecnt);
	daddr = jext->je_daddr + jblocks->jb_off;

	return (daddr);
}

/*
 * Advance the allocation head by a specified number of bytes, consuming
 * one journal segment.
 */
static void
jblocks_advance(struct jblocks *jblocks, int bytes)
{

	jblocks->jb_off += btodb(bytes);
}

static void
jblocks_destroy(struct jblocks *jblocks)
{

	free(jblocks->jb_extent);
	free(jblocks);
}

static void
jblocks_add(struct jblocks *jblocks, ufs2_daddr_t daddr, int blocks)
{
	struct jextent *jext;
	int size;

	jext = &jblocks->jb_extent[jblocks->jb_used];
	/* Adding the first block. */
	if (jext->je_daddr == 0) {
		jext->je_daddr = daddr;
		jext->je_blocks = blocks;
		return;
	}
	/* Extending the last extent. */
	if (jext->je_daddr + jext->je_blocks == daddr) {
		jext->je_blocks += blocks;
		return;
	}
	/* Adding a new extent. */
	if (++jblocks->jb_used == jblocks->jb_avail) {
		jblocks->jb_avail *= 2;
		size = sizeof(struct jextent) * jblocks->jb_avail;
		jext = errmalloc(size);
		bzero(jext, size);
		bcopy(jblocks->jb_extent, jext,
		    sizeof(struct jextent) * jblocks->jb_used);
		free(jblocks->jb_extent);
		jblocks->jb_extent = jext;
	}
	jext = &jblocks->jb_extent[jblocks->jb_used];
	jext->je_daddr = daddr;
	jext->je_blocks = blocks;

	return;
}

/*
 * Add a file block from the journal to the extent map.  We can't read
 * each file block individually because the kernel treats it as a circular
 * buffer and segments may span multiple contiguous blocks.
 */
static void
suj_add_block(ino_t ino, ufs_lbn_t lbn, ufs2_daddr_t blk, int frags)
{

	jblocks_add(suj_jblocks, fsbtodb(fs, blk), fsbtodb(fs, frags));
}

static void
suj_read(void)
{
	uint8_t block[1 * 1024 * 1024] __aligned(LIBUFS_BUFALIGN);
	struct suj_seg *seg;
	struct jsegrec *recn;
	struct jsegrec *rec;
	ufs2_daddr_t blk;
	int readsize;
	int blocks;
	int recsize;
	int size;
	int i;

	/*
	 * Read records until we exhaust the journal space.  If we find
	 * an invalid record we start searching for a valid segment header
	 * at the next block.  This is because we don't have a head/tail
	 * pointer and must recover the information indirectly.  At the gap
	 * between the head and tail we won't necessarily have a valid
	 * segment.
	 */
restart:
	for (;;) {
		size = sizeof(block);
		blk = jblocks_next(suj_jblocks, size, &readsize);
		if (blk == 0)
			return;
		size = readsize;
		/*
		 * Read 1MB at a time and scan for records within this block.
		 */
		if (pread(fsreadfd, &block, size, dbtob(blk)) != size) {
			err_suj("Error reading journal block %jd\n",
			    (intmax_t)blk);
		}
		for (rec = (void *)block; size; size -= recsize,
		    rec = (struct jsegrec *)((uintptr_t)rec + recsize)) {
			recsize = real_dev_bsize;
			if (rec->jsr_time != fs->fs_mtime) {
#ifdef notdef
				if (debug)
					printf("Rec time %jd != fs mtime %jd\n",
					    rec->jsr_time, fs->fs_mtime);
#endif
				jblocks_advance(suj_jblocks, recsize);
				continue;
			}
			if (rec->jsr_cnt == 0) {
				if (debug)
					printf("Found illegal count %d\n",
					    rec->jsr_cnt);
				jblocks_advance(suj_jblocks, recsize);
				continue;
			}
			blocks = rec->jsr_blocks;
			recsize = blocks * real_dev_bsize;
			if (recsize > size) {
				/*
				 * We may just have run out of buffer, restart
				 * the loop to re-read from this spot.
				 */
				if (size < fs->fs_bsize &&
				    size != readsize &&
				    recsize <= fs->fs_bsize)
					goto restart;
				if (debug)
					printf("Found invalid segsize "
					    "%d > %d\n", recsize, size);
				recsize = real_dev_bsize;
				jblocks_advance(suj_jblocks, recsize);
				continue;
			}
			/*
			 * Verify that all blocks in the segment are present.
			 */
			for (i = 1; i < blocks; i++) {
				recn = (void *)((uintptr_t)rec) + i *
				    real_dev_bsize;
				if (recn->jsr_seq == rec->jsr_seq &&
				    recn->jsr_time == rec->jsr_time)
					continue;
				if (debug)
					printf("Incomplete record %jd (%d)\n",
					    rec->jsr_seq, i);
				recsize = i * real_dev_bsize;
				jblocks_advance(suj_jblocks, recsize);
				goto restart;
			}
			seg = errmalloc(sizeof(*seg));
			seg->ss_blk = errmalloc(recsize);
			seg->ss_rec = *rec;
			bcopy((void *)rec, seg->ss_blk, recsize);
			if (rec->jsr_oldest > oldseq)
				oldseq = rec->jsr_oldest;
			TAILQ_INSERT_TAIL(&allsegs, seg, ss_next);
			jblocks_advance(suj_jblocks, recsize);
		}
	}
}

/*
 * Orchestrate the verification of a filesystem via the softupdates journal.
 */
int
suj_check(const char *filesys)
{
	struct inodesc idesc;
	struct csum *cgsum;
	union dinode *dp, *jip;
	struct inode ip;
	uint64_t blocks;
	int i, retval;
	struct suj_seg *seg;
	struct suj_seg *segn;

	initsuj();
	fs = &sblock;
	if (real_dev_bsize == 0 && ioctl(fsreadfd, DIOCGSECTORSIZE,
	    &real_dev_bsize) == -1)
		real_dev_bsize = secsize;
	if (debug)
		printf("dev_bsize %u\n", real_dev_bsize);

	/*
	 * Set an exit point when SUJ check failed
	 */
	retval = setjmp(jmpbuf);
	if (retval != 0) {
		pwarn("UNEXPECTED SU+J INCONSISTENCY\n");
		TAILQ_FOREACH_SAFE(seg, &allsegs, ss_next, segn) {
			TAILQ_REMOVE(&allsegs, seg, ss_next);
				free(seg->ss_blk);
				free(seg);
		}
		if (reply("FALLBACK TO FULL FSCK") == 0) {
			ckfini(0);
			exit(EEXIT);
		} else
			return (-1);
	}

	/*
	 * Search the root directory for the SUJ_FILE.
	 */
	idesc.id_type = DATA;
	idesc.id_fix = IGNORE;
	idesc.id_number = UFS_ROOTINO;
	idesc.id_func = findino;
	idesc.id_name = SUJ_FILE;
	ginode(UFS_ROOTINO, &ip);
	dp = ip.i_dp;
	if ((DIP(dp, di_mode) & IFMT) != IFDIR) {
		irelse(&ip);
		err_suj("root inode is not a directory\n");
	}
	if (DIP(dp, di_size) < 0 || DIP(dp, di_size) > MAXDIRSIZE) {
		irelse(&ip);
		err_suj("negative or oversized root directory %jd\n",
		    (uintmax_t)DIP(dp, di_size));
	}
	if ((ckinode(dp, &idesc) & FOUND) == FOUND) {
		sujino = idesc.id_parent;
		irelse(&ip);
	} else {
		if (!bkgrdcheck || debug)
			printf("Journal inode removed.  "
			    "Use tunefs to re-create.\n");
		sblock.fs_flags &= ~FS_SUJ;
		sblock.fs_sujfree = 0;
		irelse(&ip);
		return (-1);
	}
	/*
	 * Fetch the journal inode and verify it.
	 */
	ginode(sujino, &ip);
	jip = ip.i_dp;
	if (!bkgrdcheck || debug)
		printf("** SU+J Recovering %s\n", filesys);
	if (suj_verifyino(jip) != 0 || (!preen && !reply("USE JOURNAL"))) {
		irelse(&ip);
		return (-1);
	}
	/*
	 * Build a list of journal blocks in jblocks before parsing the
	 * available journal blocks in with suj_read().
	 */
	if (!bkgrdcheck || debug)
		printf("** Reading %jd byte journal from inode %ju.\n",
		    DIP(jip, di_size), (uintmax_t)sujino);
	suj_jblocks = jblocks_create();
	blocks = ino_visit(jip, sujino, suj_add_block, 0);
	if (blocks != numfrags(fs, DIP(jip, di_size))) {
		if (!bkgrdcheck || debug)
			printf("Sparse journal inode %ju.\n",
			    (uintmax_t)sujino);
		irelse(&ip);
		return (-1);
	}
	/* If journal is valid then do journal check rather than background */
	if (bkgrdcheck) {
		irelse(&ip);
		return (0);
	}
	irelse(&ip);
	suj_read();
	jblocks_destroy(suj_jblocks);
	suj_jblocks = NULL;
	if (preen || reply("RECOVER")) {
		printf("** Building recovery table.\n");
		suj_prune();
		suj_build();
		cg_apply(cg_build);
		printf("** Resolving unreferenced inode list.\n");
		ino_unlinked();
		printf("** Processing journal entries.\n");
		cg_apply(cg_trunc);
		cg_apply(cg_check_blk);
		cg_apply(cg_adj_blk);
		cg_apply(cg_check_ino);
	}
	if (preen == 0 && (jrecs > 0 || jbytes > 0) &&
	    reply("WRITE CHANGES") == 0)
		return (0);
	/*
	 * Check block counts of snapshot inodes and
	 * make copies of any needed snapshot blocks.
	 */
	for (i = 0; i < snapcnt; i++)
		check_blkcnt(&snaplist[i]);
	snapflush(suj_checkblkavail);
	/*
	 * Recompute the fs summary info from correct cs summaries.
	 */
	bzero(&fs->fs_cstotal, sizeof(struct csum_total));
	for (i = 0; i < fs->fs_ncg; i++) {
		cgsum = &fs->fs_cs(fs, i);
		fs->fs_cstotal.cs_nffree += cgsum->cs_nffree;
		fs->fs_cstotal.cs_nbfree += cgsum->cs_nbfree;
		fs->fs_cstotal.cs_nifree += cgsum->cs_nifree;
		fs->fs_cstotal.cs_ndir += cgsum->cs_ndir;
	}
	fs->fs_pendinginodes = 0;
	fs->fs_pendingblocks = 0;
	fs->fs_clean = 1;
	fs->fs_time = time(NULL);
	fs->fs_mtime = time(NULL);
	sbdirty();
	ckfini(1);
	if (jrecs > 0 || jbytes > 0) {
		printf("** %jd journal records in %jd bytes for %.2f%% "
		    "utilization\n", jrecs, jbytes,
		    ((float)jrecs / (float)(jbytes / JREC_SIZE)) * 100);
		printf("** Freed %jd inodes (%jd dirs) %jd blocks, and %jd "
		    "frags.\n", freeinos, freedir, freeblocks, freefrags);
	}

	return (0);
}

static void
initsuj(void)
{
	int i;

	for (i = 0; i < HASHSIZE; i++)
		LIST_INIT(&cghash[i]);
	lastcg = NULL;
	TAILQ_INIT(&allsegs);
	oldseq = 0;
	fs = NULL;
	sujino = 0;
	freefrags = 0;
	freeblocks = 0;
	freeinos = 0;
	freedir = 0;
	jbytes = 0;
	jrecs = 0;
	suj_jblocks = NULL;
}
