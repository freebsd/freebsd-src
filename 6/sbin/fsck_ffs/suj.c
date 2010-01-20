/*-
 * Copyright (c) 2009 Jeffrey W. Roberson <jeff@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <libufs.h>
#include <strings.h>
#include <err.h>
#include <assert.h>

#include "fsck.h"

static void	ino_decr(ino_t);

#define	SUJ_HASHSIZE	128
#define	SUJ_HASHMASK	(SUJ_HASHSIZE - 1)
#define	SUJ_HASH(x)	((x * 2654435761) & SUJ_HASHMASK)

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
	struct srechd		si_movs;
	ino_t			si_ino;
	int			si_nlinkadj;
	int			si_skipparent;
	int			si_linkadj;
	int			si_hasrecs;
	int			si_blkadj;
};
LIST_HEAD(inohd, suj_ino);

struct suj_blk {
	LIST_ENTRY(suj_blk)	sb_next;
	struct srechd		sb_recs;
	ufs2_daddr_t		sb_blk;
};
LIST_HEAD(blkhd, suj_blk);

struct data_blk {
	LIST_ENTRY(data_blk)	db_next;
	uint8_t			*db_buf;
	ufs2_daddr_t		db_blk;
	int			db_size;
};

struct ino_blk {
	LIST_ENTRY(ino_blk)	ib_next;
	uint8_t			*ib_buf;
	int			ib_dirty;
	ufs2_daddr_t		ib_blk;
};
LIST_HEAD(iblkhd, ino_blk);

struct suj_cg {
	LIST_ENTRY(suj_cg)	sc_next;
	struct blkhd		sc_blkhash[SUJ_HASHSIZE];
	struct inohd		sc_inohash[SUJ_HASHSIZE];
	struct iblkhd		sc_iblkhash[SUJ_HASHSIZE];
	struct ino_blk		*sc_lastiblk;
	uint8_t			*sc_cgbuf;
	struct cg		*sc_cgp;
	int			sc_dirty;
	int			sc_cgx;
};

LIST_HEAD(cghd, suj_cg) cghash[SUJ_HASHSIZE];
LIST_HEAD(dblkhd, data_blk) dbhash[SUJ_HASHSIZE];

TAILQ_HEAD(seghd, suj_seg) allsegs;
uint64_t oldseq;
static struct uufsd *disk = NULL;
static struct fs *fs = NULL;

/*
 * Summary statistics.
 */
uint64_t freefrags;
uint64_t freeblocks;
uint64_t freeinos;
uint64_t freedir;
uint64_t jbytes;
uint64_t jrecs;

typedef void (*ino_visitor)(ino_t, ufs_lbn_t, ufs2_daddr_t, int);

static void *
errmalloc(size_t n)
{
	void *a;

	a = malloc(n);
	if (a == NULL)
		errx(1, "malloc(%zu)", n);
	return (a);
}

/*
 * Open the given provider, load superblock.
 */
static void
opendisk(const char *devnam)
{
	if (disk != NULL)
		return;
	disk = malloc(sizeof(*disk));
	if (disk == NULL)
		errx(1, "malloc(%zu)", sizeof(*disk));
	if (ufs_disk_fillout(disk, devnam) == -1) {
		err(1, "ufs_disk_fillout(%s) failed: %s", devnam,
		    disk->d_error);
	}
	fs = &disk->d_fs;
	/*
	 * Setup a few things so reply() can work.
	 */
	bcopy(fs, &sblock, sizeof(sblock));
	fsreadfd = disk->d_fd;
	fswritefd = disk->d_fd;
}

/*
 * Mark file system as clean, write the super-block back, close the disk.
 */
static void
closedisk(const char *devnam)
{
	struct csum *cgsum;
	int i;

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
	/* XXX Don't set clean for now, we don't trust the journal. */
	/* fs->fs_clean = 1; */
	fs->fs_time = time(NULL);
	fs->fs_mtime = time(NULL);
	if (sbwrite(disk, 0) == -1)
		err(1, "sbwrite(%s)", devnam);
	if (ufs_disk_close(disk) == -1)
		err(1, "ufs_disk_close(%s)", devnam);
	free(disk);
	disk = NULL;
	fs = NULL;
	fsreadfd = -1;
	fswritefd = -1;
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

	if (cgx < 0 || cgx >= fs->fs_ncg) {
		abort();
		errx(1, "Bad cg number %d", cgx);
	}
	hd = &cghash[SUJ_HASH(cgx)];
	LIST_FOREACH(sc, hd, sc_next)
		if (sc->sc_cgx == cgx)
			return (sc);
	sc = errmalloc(sizeof(*sc));
	bzero(sc, sizeof(*sc));
	sc->sc_cgbuf = errmalloc(fs->fs_bsize);
	sc->sc_cgp = (struct cg *)sc->sc_cgbuf;
	sc->sc_cgx = cgx;
	LIST_INSERT_HEAD(hd, sc, sc_next);
	if (bread(disk, fsbtodb(fs, cgtod(fs, sc->sc_cgx)), sc->sc_cgbuf,
	    fs->fs_bsize) == -1)
		err(1, "Unable to read cylinder group %d", sc->sc_cgx);

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
	hd = &sc->sc_inohash[SUJ_HASH(ino)];
	LIST_FOREACH(sino, hd, si_next)
		if (sino->si_ino == ino)
			return (sino);
	if (creat == 0)
		return (NULL);
	sino = errmalloc(sizeof(*sino));
	bzero(sino, sizeof(*sino));
	sino->si_ino = ino;
	sino->si_nlinkadj = 0;
	TAILQ_INIT(&sino->si_recs);
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
	hd = &sc->sc_blkhash[SUJ_HASH(blk)];
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

static uint8_t *
dblk_read(ufs2_daddr_t blk, int size)
{
	struct data_blk *dblk;
	struct dblkhd *hd;

	hd = &dbhash[SUJ_HASH(blk)];
	LIST_FOREACH(dblk, hd, db_next)
		if (dblk->db_blk == blk)
			goto found;
	/*
	 * The inode block wasn't located, allocate a new one.
	 */
	dblk = errmalloc(sizeof(*dblk));
	bzero(dblk, sizeof(*dblk));
	LIST_INSERT_HEAD(hd, dblk, db_next);
	dblk->db_blk = blk;
found:
	/*
	 * I doubt size mismatches can happen in practice but it is trivial
	 * to handle.
	 */
	if (size != dblk->db_size) {
		if (dblk->db_buf)
			free(dblk->db_buf);
		dblk->db_buf = errmalloc(size);
		dblk->db_size = size;
		if (bread(disk, fsbtodb(fs, blk), dblk->db_buf, size) == -1)
			err(1, "Failed to read data block %jd", blk);
	}
	return (dblk->db_buf);
}

static union dinode *
ino_read(ino_t ino)
{
	struct ino_blk *iblk;
	struct iblkhd *hd;
	struct suj_cg *sc;
	ufs2_daddr_t blk;
	int off;

	blk = ino_to_fsba(fs, ino);
	sc = cg_lookup(ino_to_cg(fs, ino));
	hd = &sc->sc_iblkhash[SUJ_HASH(blk)];
	LIST_FOREACH(iblk, hd, ib_next)
		if (iblk->ib_blk == blk)
			goto found;
	/*
	 * The inode block wasn't located, allocate a new one.
	 */
	iblk = errmalloc(sizeof(*iblk));
	bzero(iblk, sizeof(*iblk));
	iblk->ib_buf = errmalloc(fs->fs_bsize);
	iblk->ib_blk = blk;
	LIST_INSERT_HEAD(hd, iblk, ib_next);
	if (bread(disk, fsbtodb(fs, blk), iblk->ib_buf, fs->fs_bsize) == -1)
		err(1, "Failed to read inode block %jd", blk);
found:
	sc->sc_lastiblk = iblk;
	off = ino_to_fsbo(fs, ino);
	if (fs->fs_magic == FS_UFS1_MAGIC)
		return (union dinode *)&((struct ufs1_dinode *)iblk->ib_buf)[off];
	else
		return (union dinode *)&((struct ufs2_dinode *)iblk->ib_buf)[off];
}

static void
ino_dirty(ino_t ino)
{
	struct ino_blk *iblk;
	struct iblkhd *hd;
	struct suj_cg *sc;
	ufs2_daddr_t blk;

	blk = ino_to_fsba(fs, ino);
	sc = cg_lookup(ino_to_cg(fs, ino));
	iblk = sc->sc_lastiblk;
	if (iblk && iblk->ib_blk == blk) {
		iblk->ib_dirty = 1;
		return;
	}
	hd = &sc->sc_iblkhash[SUJ_HASH(blk)];
	LIST_FOREACH(iblk, hd, ib_next) {
		if (iblk->ib_blk == blk) {
			iblk->ib_dirty = 1;
			return;
		}
	}
	ino_read(ino);
	ino_dirty(ino);
}

static void
iblk_write(struct ino_blk *iblk)
{

	if (iblk->ib_dirty == 0)
		return;
	if (bwrite(disk, fsbtodb(fs, iblk->ib_blk), iblk->ib_buf,
	    fs->fs_bsize) == -1)
		err(1, "Failed to write inode block %jd", iblk->ib_blk);
}

/*
 * Return 1 if the inode was free and 0 if it is allocated.
 */
static int
ino_isfree(ino_t ino)
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
	return isclr(inosused, ino);
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
	if (brec->jb_frags != frags)
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
blk_isfree(ufs2_daddr_t blk, ino_t ino, ufs_lbn_t lbn, int frags)
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
	 * we're concerned with.  If any fragments have ben reallocated
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
		 * exactly it's a new allocation.  If it matches
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
		printf("blk_isfree: blk %jd sblk %jd off %d mask 0x%X\n",
		    blk, sblk->sb_blk, off, mask);
	return (mask >> off);
}

/*
 * Determine whether it is safe to follow an indirect.  It is not safe
 * if any part of the indirect has been reallocated or the last journal
 * entry was an allocation.  Just allocated indirects may not have valid
 * pointers yet and all of their children will have their own records.
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
			return (1);
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
	sc->sc_dirty = 1;

	return (1);
}

/*
 * Free 'frags' frags starting at filesystem block 'bno' skipping any frags
 * set in the mask.
 */
static void
blk_free(ufs2_daddr_t bno, int mask, int frags)
{
	ufs1_daddr_t fragno, cgbno;
	struct suj_cg *sc;
	struct cg *cgp;
	int i, cg;
	uint8_t *blksfree;

	if (debug)
		printf("Freeing %d frags at blk %jd\n", frags, bno);
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
			if ((mask & (1 << i)) == 0 && isclr(blksfree, cgbno +i)) {
				freefrags++;
				setbit(blksfree, cgbno + i);
			}
	}
	sc->sc_dirty = 1;
}

/*
 * Fetch an indirect block to find the block at a given lbn.  The lbn
 * may be negative to fetch a specific indirect block pointer or positive
 * to fetch a specific block.
 */
static ufs2_daddr_t
indir_blkatoff(ufs2_daddr_t blk, ino_t ino, ufs_lbn_t cur, ufs_lbn_t lbn, int level)
{
	ufs2_daddr_t *bap2;
	ufs2_daddr_t *bap1;
	ufs_lbn_t lbnadd;
	ufs_lbn_t base;
	int i;

	if (blk == 0)
		return (0);
	if (cur == lbn)
		return (blk);
	if (level == 0 && lbn < 0) {
		abort();
		errx(1, "Invalid lbn %jd", lbn);
	}
	bap2 = (void *)dblk_read(blk, fs->fs_bsize);
	bap1 = (void *)bap2;
	lbnadd = 1;
	base = -(cur + level);
	for (i = level; i > 0; i--)
		lbnadd *= NINDIR(fs);
	if (lbn > 0) 
		i = (lbn - base) / lbnadd;
	else
		i = (-lbn - base) / lbnadd;
	if (i < 0 || i >= NINDIR(fs)) {
		abort();
		errx(1, "Invalid indirect index %d produced by lbn %jd",
		    i, lbn);
	}
	if (level == 0)
		cur = base + (i * lbnadd);
	else
		cur = -(base + (i * lbnadd)) - (level - 1);
	if (fs->fs_magic == FS_UFS1_MAGIC)
		blk = bap1[i];
	else
		blk = bap2[i];
	if (cur == lbn)
		return (blk);
	if (level == 0) {
		abort();
		errx(1, "Invalid lbn %jd at level 0", lbn);
	}
	return indir_blkatoff(blk, ino, cur, lbn, level - 1);
}

/*
 * Finds the disk block address at the specified lbn within the inode
 * specified by ip.  This follows the whole tree and honors di_size and
 * di_extsize so it is a true test of reachability.  The lbn may be
 * negative if an extattr or indirect block is requested.
 */
static ufs2_daddr_t
ino_blkatoff(union dinode *ip, ino_t ino, ufs_lbn_t lbn, int *frags)
{
	ufs_lbn_t tmpval;
	ufs_lbn_t cur;
	ufs_lbn_t next;
	int i;

	/*
	 * Handle extattr blocks first.
	 */
	if (lbn < 0 && lbn >= -NXADDR) {
		lbn = -1 - lbn;
		if (lbn > lblkno(fs, ip->dp2.di_extsize - 1))
			return (0);
		*frags = numfrags(fs, sblksize(fs, ip->dp2.di_extsize, lbn));
		return (ip->dp2.di_extb[lbn]);
	}
	/*
	 * And now direct and indirect.  Verify that the lbn does not
	 * exceed the size required to store the file by asking for
	 * the lbn of the last byte.  These blocks should be 0 anyway
	 * so this simply saves the traversal.
	 */
	if (lbn > 0 && lbn > lblkno(fs, DIP(ip, di_size) - 1))
		return (0);
	if (lbn < 0 && -lbn > lblkno(fs, DIP(ip, di_size) - 1))
		return (0);
	if (lbn >= 0 && lbn < NDADDR) {
		*frags = numfrags(fs, sblksize(fs, DIP(ip, di_size), lbn));
		return (DIP(ip, di_db[lbn]));
	}
	*frags = fs->fs_frag;

	for (i = 0, tmpval = NINDIR(fs), cur = NDADDR; i < NIADDR; i++,
	    tmpval *= NINDIR(fs), cur = next) {
		next = cur + tmpval;
		if (lbn == -cur)
			return (DIP(ip, di_ib[i]));
		/*
		 * Determine whether the lbn in question is within this tree.
		 */
		if (lbn < 0 && -lbn >= next)
			continue;
		if (lbn > 0 && lbn >= next)
			continue;

		return indir_blkatoff(DIP(ip, di_ib[i]), ino, -cur - i, lbn, i);
	}
	errx(1, "lbn %jd not in ino", lbn);
}

/*
 * Determine whether a block exists at a particular lbn in an inode.
 * Returns 1 if found, 0 if not.  lbn may be negative for indirects
 * or ext blocks.
 */
static int
blk_isat(ino_t ino, ufs_lbn_t lbn, ufs2_daddr_t blk, int *frags)
{
	union dinode *ip;
	ufs2_daddr_t nblk;

	ip = ino_read(ino);

	if (DIP(ip, di_nlink) == 0 || DIP(ip, di_mode) == 0)
		return (0);
	nblk = ino_blkatoff(ip, ino, lbn, frags);

	return (nblk == blk);
}

/*
 * Determines whether a pointer to an inode exists within a directory
 * at a specified offset.  Returns the mode of the found entry.
 */
static int
ino_isat(ino_t parent, off_t diroff, ino_t child, int *mode, int *isdot)
{
	union dinode *dip;
	struct direct *dp;
	ufs2_daddr_t blk;
	uint8_t *block;
	ufs_lbn_t lbn;
	int blksize;
	int frags;
	int dpoff;
	int doff;

	*isdot = 0;
	dip = ino_read(parent);
	*mode = DIP(dip, di_mode);
	if ((*mode & IFMT) != IFDIR) {
		if (debug) {
			/* This can happen if the parent inode was reallocated. */
			if (*mode != 0)
				printf("Directory %d has bad mode %o\n",
				    parent, *mode);
			else
				printf("Directory %d zero inode\n", parent);
		}
		return (0);
	}
	lbn = lblkno(fs, diroff);
	doff = blkoff(fs, diroff);
	blksize = sblksize(fs, DIP(dip, di_size), lbn);
	if (diroff + DIRECTSIZ(1) > DIP(dip, di_size) || doff >= blksize) {
		if (debug)
			printf("ino %d absent from %d due to offset %jd"
			    " exceeding size %jd\n",
			    child, parent, diroff, DIP(dip, di_size));
		return (0);
	}
	blk = ino_blkatoff(dip, parent, lbn, &frags);
	if (blk <= 0) {
		if (debug)
			printf("Sparse directory %d", parent);
		return (0);
	}
	block = dblk_read(blk, blksize);
	/*
	 * Walk through the records from the start of the block to be
	 * certain we hit a valid record and not some junk in the middle
	 * of a file name.  Stop when we reach or pass the expected offset.
	 */
	dpoff = 0;
	do {
		dp = (struct direct *)&block[dpoff];
		if (dpoff == doff)
			break;
		if (dp->d_reclen == 0)
			break;
		dpoff += dp->d_reclen;
	} while (dpoff <= doff);
	if (dpoff > fs->fs_bsize)
		errx(1, "Corrupt directory block in dir inode %d", parent);
	/* Not found. */
	if (dpoff != doff) {
		if (debug)
			printf("ino %d not found in %d, lbn %jd, dpoff %d\n",
			    child, parent, lbn, dpoff);
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
		return (1);
	}
	if (debug)
		printf("ino %d doesn't match dirent ino %d in parent %d\n",
		    child, dp->d_ino, parent);
	return (0);
}

#define	VISIT_INDIR	0x0001
#define	VISIT_EXT	0x0002

/*
 * Read an indirect level which may or may not be linked into an inode.
 */
static void
indir_visit(ino_t ino, ufs_lbn_t lbn, ufs2_daddr_t blk, uint64_t *frags,
    ino_visitor visitor, int flags)
{
	ufs2_daddr_t *bap2;
	ufs1_daddr_t *bap1;
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
	if (blk_isindir(blk, ino, lbn) == 0) {
		if (debug)
			printf("blk %jd ino %d lbn %jd is not indir.\n",
			    blk, ino, lbn);
		goto out;
	}
	level = lbn_level(lbn);
	if (level == -1) {
		abort();
		errx(1, "Invalid level for lbn %jd", lbn);
	}
	lbnadd = 1;
	for (i = level; i > 0; i--)
		lbnadd *= NINDIR(fs);
	bap1 = (void *)dblk_read(blk, fs->fs_bsize);
	bap2 = (void *)bap1;
	for (i = 0; i < NINDIR(fs); i++) {
		if (fs->fs_magic == FS_UFS1_MAGIC)
			nblk = *bap1++;
		else
			nblk = *bap2++;
		if (nblk == 0)
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
ino_visit(union dinode *ip, ino_t ino, ino_visitor visitor, int flags)
{
	ufs_lbn_t tmpval;
	ufs_lbn_t lbn;
	uint64_t size;
	uint64_t fragcnt;
	int mode;
	int frags;
	int i;

	size = DIP(ip, di_size);
	mode = DIP(ip, di_mode) & IFMT;
	fragcnt = 0;
	if ((flags & VISIT_EXT) &&
	    fs->fs_magic == FS_UFS2_MAGIC && ip->dp2.di_extsize) {
		for (i = 0; i < NXADDR; i++) {
			if (ip->dp2.di_extb[i] == 0)
				continue;
			frags = sblksize(fs, ip->dp2.di_extsize, i);
			frags = numfrags(fs, frags);
			fragcnt += frags;
			visitor(ino, -1 - i, ip->dp2.di_extb[i], frags);
		}
	}
	/* Skip datablocks for short links and devices. */
	if (mode == IFBLK || mode == IFCHR ||
	    (mode == IFLNK && size < fs->fs_maxsymlinklen))
		return (fragcnt);
	for (i = 0; i < NDADDR; i++) {
		if (DIP(ip, di_db[i]) == 0)
			continue;
		frags = sblksize(fs, size, i);
		frags = numfrags(fs, frags);
		fragcnt += frags;
		visitor(ino, i, DIP(ip, di_db[i]), frags);
	}
	for (i = 0, tmpval = NINDIR(fs), lbn = NDADDR; i < NIADDR; i++,
	    tmpval *= NINDIR(fs), lbn += tmpval) {
		if (DIP(ip, di_ib[i]) == 0)
			continue;
		indir_visit(ino, -lbn - i, DIP(ip, di_ib[i]), &fragcnt, visitor,
		    flags);
	}
	return (fragcnt);
}

/*
 * Null visitor function used when we just want to count blocks.
 */
static void
null_visit(ino_t ino, ufs_lbn_t lbn, ufs2_daddr_t blk, int frags)
{
}

/*
 * Recalculate di_blocks when we discover that a block allocation or
 * free was not successfully completed.  The kernel does not roll this back
 * because it would be too expensive to compute which indirects were
 * reachable at the time the inode was written.
 */
static void
ino_adjblks(ino_t ino)
{
	struct suj_ino *sino;
	union dinode *ip;
	uint64_t blocks;
	uint64_t frags;

	sino = ino_lookup(ino, 1);
	if (sino->si_blkadj)
		return;
	sino->si_blkadj = 1;
	ip = ino_read(ino);
	/* No need to adjust zero'd inodes. */
	if (DIP(ip, di_mode) == 0)
		return;
	frags = ino_visit(ip, ino, null_visit, VISIT_INDIR | VISIT_EXT);
	blocks = fsbtodb(fs, frags);
	if (blocks == DIP(ip, di_blocks))
		return;
	if (debug)
		printf("ino %d adjusting block count from %jd to %jd\n",
		    ino, DIP(ip, di_blocks), blocks);
	DIP_SET(ip, di_blocks, blocks);
	ino_dirty(ino);
}

static void
blk_free_visit(ino_t ino, ufs_lbn_t lbn, ufs2_daddr_t blk, int frags)
{
	int mask;

	mask = blk_isfree(blk, ino, lbn, frags);
	if (debug)
		printf("blk %jd freemask 0x%X\n", blk, mask);
	blk_free(blk, mask, frags);
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

	mask = blk_isfree(blk, ino, lbn, frags);
	if (debug)
		printf("blk %jd freemask 0x%X\n", blk, mask);
	resid = 0;
	if (lbn <= -NDADDR && follow && mask == 0)
		indir_visit(ino, lbn, blk, &resid, blk_free_visit, VISIT_INDIR);
	else
		blk_free(blk, mask, frags);
}

static void
ino_free_children(ino_t ino, ufs_lbn_t lbn, ufs2_daddr_t blk, int frags)
{
	struct suj_ino *sino;
	struct suj_rec *srec;
	struct jrefrec *rrec;
	struct direct *dp;
	off_t diroff;
	uint8_t *block;
	int skipparent;
	int isparent;
	int dpoff;
	int size;

	sino = ino_lookup(ino, 0);
	if (sino)
		skipparent = sino->si_skipparent;
	else
		skipparent = 0;
	size = lfragtosize(fs, frags);
	block = dblk_read(blk, size);
	dp = (struct direct *)&block[0];
	for (dpoff = 0; dpoff < size && dp->d_reclen; dpoff += dp->d_reclen) {
		dp = (struct direct *)&block[dpoff];
		if (dp->d_ino == 0 || dp->d_ino == WINO)
			continue;
		if (dp->d_namlen == 1 && dp->d_name[0] == '.')
			continue;
		isparent = dp->d_namlen == 2 && dp->d_name[0] == '.' &&
		    dp->d_name[1] == '.';
		if (isparent && skipparent == 1)
			continue;
		if (debug)
			printf("Directory %d removing inode %d name %s\n",
			    ino, dp->d_ino, dp->d_name);
		/*
		 * Lookup this inode to see if we have a record for it.
		 * If not, we've already adjusted it assuming this path
		 * was valid and we have to adjust once more.
		 */
		sino = ino_lookup(dp->d_ino, 0);
		if (sino == NULL || sino->si_linkadj || sino->si_hasrecs == 0) {
			ino_decr(dp->d_ino);
			continue;
		}
		/*
		 * Tell any child directories we've already removed their
		 * parent.  Don't try to adjust our link down again.
		 */
		if (isparent == 0)
			sino->si_skipparent = 1;
		/*
		 * If we haven't yet processed this inode we need to make
		 * sure we will successfully discover the lost path.  If not
		 * use nlinkadj to remember.
		 */
		diroff = lblktosize(fs, lbn) + dpoff;
		TAILQ_FOREACH(srec, &sino->si_recs, sr_next) {
			rrec = (struct jrefrec *)srec->sr_rec;
			if (rrec->jr_parent == ino &&
			    rrec->jr_diroff == diroff)
				break;
		}
		if (srec == NULL)
			sino->si_nlinkadj--;
	}
}

/*
 * Truncate an inode, freeing all blocks and decrementing all children's
 * link counts.  Free the inode back to the cg.
 */
static void
ino_truncate(union dinode *ip, ino_t ino, int mode)
{
	uint32_t gen;

	if (ino == ROOTINO)
		errx(1, "Attempting to free ROOTINO");
	if (debug)
		printf("Truncating and freeing ino %d, nlink %d, mode %o\n",
		    ino, DIP(ip, di_nlink), DIP(ip, di_mode));

	/* We are freeing an inode or directory. */
	if ((DIP(ip, di_mode) & IFMT) == IFDIR)
		ino_visit(ip, ino, ino_free_children, 0);
	DIP_SET(ip, di_nlink, 0);
	ino_visit(ip, ino, blk_free_visit, VISIT_EXT | VISIT_INDIR);
	/* Here we have to clear the inode and release any blocks it holds. */
	gen = DIP(ip, di_gen);
	if (fs->fs_magic == FS_UFS1_MAGIC)
		bzero(ip, sizeof(struct ufs1_dinode));
	else
		bzero(ip, sizeof(struct ufs2_dinode));
	DIP_SET(ip, di_gen, gen);
	ino_dirty(ino);
	ino_free(ino, mode);
	return;
}

/*
 * Adjust an inode's link count down by one when a directory goes away.
 */
static void
ino_decr(ino_t ino)
{
	union dinode *ip;
	int reqlink;
	int nlink;
	int mode;

	ip = ino_read(ino);
	nlink = DIP(ip, di_nlink);
	mode = DIP(ip, di_mode);
	if (nlink < 1)
		errx(1, "Inode %d link count %d invalid", ino, nlink);
	if (mode == 0)
		errx(1, "Inode %d has a link of %d with 0 mode.", ino, nlink);
	nlink--;
	if ((mode & IFMT) == IFDIR)
		reqlink = 2;
	else
		reqlink = 1;
	if (nlink < reqlink) {
		if (debug)
			printf("ino %d not enough links to live %d < %d\n",
			    ino, nlink, reqlink);
		ino_truncate(ip, ino, mode);
		return;
	}
	DIP_SET(ip, di_nlink, nlink);
	ino_dirty(ino);
}

/*
 * Adjust the inode link count to 'nlink'.  If the count reaches zero
 * free it.
 */
static void
ino_adjust(ino_t ino, int lastmode, nlink_t nlink)
{
	union dinode *ip;
	int reqlink;
	int mode;

	ip = ino_read(ino);
	mode = DIP(ip, di_mode) & IFMT;
	if (nlink > LINK_MAX)
		errx(1,
		    "ino %d nlink manipulation error, new link %d, old link %d",
		    ino, nlink, DIP(ip, di_nlink));
	if (debug)
		printf("Adjusting ino %d, nlink %d, old link %d lastmode %o\n",
		    ino, nlink, DIP(ip, di_nlink), lastmode);
	if (mode == 0) {
		if (debug)
			printf("ino %d, zero inode freeing bitmap\n", ino);
		ino_free(ino, lastmode);
		return;
	}
	/* XXX Should be an assert? */
	if (mode != lastmode && debug)
		printf("ino %d, mode %o != %o\n", ino, mode, lastmode);
	if ((mode & IFMT) == IFDIR)
		reqlink = 2;
	else
		reqlink = 1;
	/* If the inode doesn't have enough links to live, free it. */
	if (nlink < reqlink) {
		if (debug)
			printf("ino %d not enough links to live %d < %d\n",
			    ino, nlink, reqlink);
		ino_truncate(ip, ino, mode);
		return;
	}
	/* If required write the updated link count. */
	if (DIP(ip, di_nlink) == nlink) {
		if (debug)
			printf("ino %d, link matches, skipping.\n", ino);
		return;
	}
	DIP_SET(ip, di_nlink, nlink);
	ino_dirty(ino);
}

#define	DOTDOT_OFFSET	DIRECTSIZ(1)

/*
 * Process records available for one inode and determine whether the
 * link count is correct or needs adjusting.
 *
 * XXX Failed to fix zero length directory.  Shouldn't .. have been mising?
 */
static void
ino_check(struct suj_ino *sino)
{
	struct suj_rec *srec;
	struct jrefrec *rrec;
	struct suj_ino *stmp;
	nlink_t dotlinks;
	int newlinks;
	int removes;
	int nlink;
	ino_t ino;
	int isdot;
	int isat;
	int mode;

	if (sino->si_hasrecs == 0)
		return;
	ino = sino->si_ino;
	/*
	 * XXX ino_isfree currently is skipping initialized inodes
	 * that are unreferenced.
	 */
	if (0 && ino_isfree(ino))
		return;
	rrec = (struct jrefrec *)TAILQ_FIRST(&sino->si_recs)->sr_rec;
	nlink = rrec->jr_nlink;
	newlinks = sino->si_nlinkadj;
	dotlinks = 0;
	removes = 0;
	TAILQ_FOREACH(srec, &sino->si_recs, sr_next) {
		rrec = (struct jrefrec *)srec->sr_rec;
		isat = ino_isat(rrec->jr_parent, rrec->jr_diroff, 
		    rrec->jr_ino, &mode, &isdot);
		if (isat && (mode & IFMT) != (rrec->jr_mode & IFMT))
			errx(1, "Inode mode/directory type mismatch %o != %o",
			    mode, rrec->jr_mode);
		if (debug)
			printf("jrefrec: op %d ino %d, nlink %d, parent %d, "
			    "diroff %jd, mode %o, isat %d, isdot %d\n",
			    rrec->jr_op, rrec->jr_ino, rrec->jr_nlink,
			    rrec->jr_parent, rrec->jr_diroff, rrec->jr_mode,
			    isat, isdot);
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
	nlink += newlinks;
	nlink -= removes;
	/*
	 * If it's a directory with no real names pointing to it go ahead
	 * and truncate it.  This will free any children.
	 */
	if ((mode & IFMT) == IFDIR && nlink - dotlinks == 0) {
		nlink = 0;
		/*
		 * Mark any .. links so they know not to free this inode
		 * when they are removed.
		 */
		TAILQ_FOREACH(srec, &sino->si_recs, sr_next) {
			rrec = (struct jrefrec *)srec->sr_rec;
			if (rrec->jr_diroff == DOTDOT_OFFSET) {
				stmp = ino_lookup(rrec->jr_parent, 0);
				if (stmp)
					stmp->si_skipparent = 1;
			}
		}
	}
	sino->si_linkadj = 1;
	ino_adjust(ino, mode, nlink);
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
	ufs2_daddr_t blk;
	int mask;
	int frags;
	int isat;

	/*
	 * Each suj_blk actually contains records for any fragments in that
	 * block.  As a result we must evaluate each record individually.
	 */
	TAILQ_FOREACH(srec, &sblk->sb_recs, sr_next) {
		brec = (struct jblkrec *)srec->sr_rec;
		frags = brec->jb_frags;
		blk = brec->jb_blkno + brec->jb_oldfrags;
		isat = blk_isat(brec->jb_ino, brec->jb_lbn, blk, &frags);
		if (debug)
			printf("op %d blk %jd ino %d lbn %jd frags %d isat %d (%d)\n",
			    brec->jb_op, blk, brec->jb_ino, brec->jb_lbn,
			    brec->jb_frags, isat, frags);
		/*
		 * If we found the block at this address we still have to
		 * determine if we need to free the tail end that was
		 * added by adding contiguous fragments from the same block.
		 */
		if (isat == 1) {
			if (frags == brec->jb_frags)
				continue;
			mask = blk_isfree(blk, brec->jb_ino, brec->jb_lbn,
			    brec->jb_frags);
			mask >>= frags;
			blk += frags;
			frags = brec->jb_frags - frags;
			blk_free(blk, mask, frags);
			ino_adjblks(brec->jb_ino);
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
		ino_adjblks(brec->jb_ino);
	}
}

/*
 * Walk the list of inode and block records for this cg, recovering any
 * changes which were not complete at the time of crash.
 */
static void
cg_check(struct suj_cg *sc)
{
	struct suj_blk *nextb;
	struct suj_ino *nexti;
	struct suj_ino *sino;
	struct suj_blk *sblk;
	int i;

	if (debug)
		printf("Recovering cg %d\n", sc->sc_cgx);

	for (i = 0; i < SUJ_HASHSIZE; i++)
		LIST_FOREACH_SAFE(sino, &sc->sc_inohash[i], si_next, nexti)
			ino_check(sino);

	for (i = 0; i < SUJ_HASHSIZE; i++)
		LIST_FOREACH_SAFE(sblk, &sc->sc_blkhash[i], sb_next, nextb)
			blk_check(sblk);
}

/*
 * Write a potentially dirty cg.  All inodes must be written before the
 * cg maps are so that an allocated inode is never marked free, even if
 * we crash during fsck.
 */
static void
cg_write(struct suj_cg *sc)
{
	struct ino_blk *iblk;
	ufs1_daddr_t fragno, cgbno, maxbno;
	u_int8_t *blksfree;
	struct cg *cgp;
	int blk;
	int i;

	for (i = 0; i < SUJ_HASHSIZE; i++)
		LIST_FOREACH(iblk, &sc->sc_iblkhash[i], ib_next)
			iblk_write(iblk);
	if (sc->sc_dirty == 0)
		return;
	/*
	 * Fix the frag and cluster summary.
	 */
	cgp = sc->sc_cgp;
	cgp->cg_cs.cs_nbfree = 0;
	cgp->cg_cs.cs_nffree = 0;
	bzero(&cgp->cg_frsum, sizeof(cgp->cg_frsum));
	maxbno = fragstoblks(fs, fs->fs_fpg);
	if (fs->fs_contigsumsize > 0) {
		for (i = 1; i <= fs->fs_contigsumsize; i++)
			cg_clustersum(cgp)[i] = 0;
		bzero(cg_clustersfree(cgp), howmany(maxbno, CHAR_BIT));
	}
	blksfree = cg_blksfree(cgp);
	for (cgbno = 0; cgbno < maxbno; cgbno++) {
		if (ffs_isfreeblock(fs, blksfree, cgbno))
			continue;
		if (ffs_isblock(fs, blksfree, cgbno)) {
			ffs_clusteracct(fs, cgp, cgbno, 1);
			cgp->cg_cs.cs_nbfree++;
			continue;
		}
		fragno = blkstofrags(fs, cgbno);
		blk = blkmap(fs, blksfree, fragno);
		ffs_fragacct(fs, blk, cgp->cg_frsum, 1);
		for (i = 0; i < fs->fs_frag; i++)
			if (isset(blksfree, fragno + i))
				cgp->cg_cs.cs_nffree++;
	}
	/*
	 * Update the superblock cg summary from our now correct values
	 * before writing the block.
	 */
	fs->fs_cs(fs, sc->sc_cgx) = cgp->cg_cs;
	if (bwrite(disk, fsbtodb(fs, cgtod(fs, sc->sc_cgx)), sc->sc_cgbuf,
	    fs->fs_bsize) == -1)
		err(1, "Unable to write cylinder group %d", sc->sc_cgx);
}

static void
cg_apply(void (*apply)(struct suj_cg *))
{
	struct suj_cg *scg;
	int i;

	for (i = 0; i < SUJ_HASHSIZE; i++)
		LIST_FOREACH(scg, &cghash[i], sc_next)
			apply(scg);
}

/*
 * Process the unlinked but referenced file list.  Freeing all inodes.
 */
static void
ino_unlinked(void)
{
	union dinode *ip;
	uint16_t mode;
	ino_t inon;
	ino_t ino;

	ino = fs->fs_sujfree;
	fs->fs_sujfree = 0;
	while (ino != 0) {
		ip = ino_read(ino);
		mode = DIP(ip, di_mode) & IFMT;
		inon = DIP(ip, di_freelink);
		DIP_SET(ip, di_freelink, 0);
		/*
		 * XXX Should this be an errx?
		 */
		if (DIP(ip, di_nlink) == 0) {
			if (debug)
				printf("Freeing unlinked ino %d mode %o\n",
				    ino, mode);
			ino_truncate(ip, ino, mode);
		} else if (debug)
			printf("Skipping ino %d mode %o with link %d\n",
			    ino, mode, DIP(ip, di_nlink));
		ino = inon;
	}
}

/*
 * If we see two ops for the same inode to the same parent at the same
 * offset we could miscount the link with ino_isat() returning twice.
 * Keep only the first record because it has the valid link count but keep
 * the mode from the final op as that should be the correct mode in case
 * it changed.
 */
static void
suj_build_ino(struct jrefrec *refrec)
{
	struct jmvrec *mvrec;
	struct suj_rec *srec;
	struct suj_ino *sino;
	struct suj_rec *srn;
	struct jrefrec *rrn;

	if (debug)
		printf("suj_build_ino: op %d, ino %d, nlink %d, parent %d, diroff %jd\n", 
		    refrec->jr_op, refrec->jr_ino, refrec->jr_nlink, refrec->jr_parent,
		    refrec->jr_diroff);
	sino = ino_lookup(refrec->jr_ino, 1);
	/*
	 * Search for a mvrec that matches this offset.  Whether it's an add
	 * or a remove we can delete the mvref.  It no longer applies to this
	 * location.
	 *
	 * For removes, we have to find the original offset so we can create
	 * a remove that matches the earlier add so it can be abandoned
	 * if necessary.  We create an add in the new location so we can
	 * tolerate the directory block as it existed before or after
	 * the move.
	 */
	if (!TAILQ_EMPTY(&sino->si_movs)) {
		for (srn = TAILQ_LAST(&sino->si_movs, srechd); srn;
		    srn = TAILQ_PREV(srn, srechd, sr_next)) {
			mvrec = (struct jmvrec *)srn->sr_rec;
			if (mvrec->jm_parent != refrec->jr_parent ||
			    mvrec->jm_newoff != refrec->jr_diroff)
				continue;
			TAILQ_REMOVE(&sino->si_movs, srn, sr_next);
			if (refrec->jr_op == JOP_REMREF) {
				rrn = errmalloc(sizeof(*refrec));
				*rrn = *refrec;
				rrn->jr_op = JOP_ADDREF;
				suj_build_ino(rrn);
				refrec->jr_diroff = mvrec->jm_oldoff;
			}
		}
	}
	/*
	 * We walk backwards so that adds and removes are evaluated in the
	 * correct order.
	 */
	for (srn = TAILQ_LAST(&sino->si_recs, srechd); srn;
	    srn = TAILQ_PREV(srn, srechd, sr_next)) {
		rrn = (struct jrefrec *)srn->sr_rec;
		if (rrn->jr_parent != refrec->jr_parent ||
		    rrn->jr_diroff != refrec->jr_diroff)
			continue;
		if (debug)
			printf("Discarding dup.\n");
		rrn->jr_mode = refrec->jr_mode;
		return;
	}
	sino->si_hasrecs = 1;
	srec = errmalloc(sizeof(*srec));
	srec->sr_rec = (union jrec *)refrec;
	TAILQ_INSERT_TAIL(&sino->si_recs, srec, sr_next);
}

/*
 * Apply a move record to an inode.  We must search for adds that preceed us
 * and add duplicates because we won't know which location to search first.
 * Then we add movs to a queue that is maintained until the moved location
 * is removed.  If a single record is moved multiple times we only maintain
 * one copy that contains the original and final diroffs.
 */
static void
suj_move_ino(struct jmvrec *mvrec)
{
	struct jrefrec *refrec;
	struct suj_ino *sino;
	struct suj_rec *srec;
	struct jmvrec *mvrn;
	struct suj_rec *srn;
	struct jrefrec *rrn;

	if (debug)
		printf("suj_move_ino: ino %d, parent %d, diroff %jd, oldoff %jd\n", 
		    mvrec->jm_ino, mvrec->jm_parent, mvrec->jm_newoff,
		    mvrec->jm_oldoff);
	sino = ino_lookup(mvrec->jm_ino, 0);
	if (sino == NULL)
		return;
	/*
	 * We walk backwards so we only evaluate the most recent record at
	 * this offset.
	 */
	for (srn = TAILQ_LAST(&sino->si_recs, srechd); srn;
	    srn = TAILQ_PREV(srn, srechd, sr_next)) {
		rrn = (struct jrefrec *)srn->sr_rec;
		if (rrn->jr_op != JOP_ADDREF)
			continue;
		if (rrn->jr_parent != mvrec->jm_parent ||
		    rrn->jr_diroff != mvrec->jm_oldoff)
			continue;
		/*
		 * When an entry is moved we don't know whether the write
		 * to move has completed yet.  To resolve this we create
		 * a new add dependency in the new location as if it were added
		 * twice.  Only one will succeed.
		 */
		refrec = errmalloc(sizeof(*refrec));
		refrec->jr_op = JOP_ADDREF;
		refrec->jr_ino = mvrec->jm_ino;
		refrec->jr_parent = mvrec->jm_parent;
		refrec->jr_diroff = mvrec->jm_newoff;
		refrec->jr_mode = rrn->jr_mode;
		refrec->jr_nlink = rrn->jr_nlink;
		suj_build_ino(refrec);
		break;
	}
	/*
	 * Add this mvrec to the queue of pending mvs.
	 */
	for (srn = TAILQ_LAST(&sino->si_movs, srechd); srn;
	    srn = TAILQ_PREV(srn, srechd, sr_next)) {
		mvrn = (struct jmvrec *)srn->sr_rec;
		if (mvrn->jm_parent != mvrec->jm_parent ||
		    mvrn->jm_newoff != mvrec->jm_oldoff)
			continue;
		mvrn->jm_newoff = mvrec->jm_newoff;
		return;
	}
	srec = errmalloc(sizeof(*srec));
	srec->sr_rec = (union jrec *)mvrec;
	TAILQ_INSERT_TAIL(&sino->si_movs, srec, sr_next);
}

/*
 * Modify journal records so they refer to the base block number
 * and a start and end frag range.  This is to facilitate the discovery
 * of overlapping fragment allocations.
 */
static void
suj_build_blk(struct jblkrec *blkrec)
{
	struct suj_rec *srec;
	struct suj_blk *sblk;
	struct jblkrec *blkrn;
	ufs2_daddr_t blk;
	int frag;

	if (debug)
		printf("suj_build_blk: op %d blkno %jd frags %d oldfrags %d "
		    "ino %d lbn %jd\n",
		    blkrec->jb_op, blkrec->jb_blkno, blkrec->jb_frags,
		    blkrec->jb_oldfrags, blkrec->jb_ino, blkrec->jb_lbn);
	blk = blknum(fs, blkrec->jb_blkno);
	frag = fragnum(fs, blkrec->jb_blkno);
	sblk = blk_lookup(blk, 1);
	/*
	 * Rewrite the record using oldfrags to indicate the offset into
	 * the block.  Leave jb_frags as the actual allocated count.
	 */
	blkrec->jb_blkno -= frag;
	blkrec->jb_oldfrags = frag;
	if (blkrec->jb_oldfrags + blkrec->jb_frags > fs->fs_frag)
		errx(1, "Invalid fragment count %d oldfrags %d",
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

/*
 * Build up tables of the operations we need to recover.
 */
static void
suj_build(void)
{
	struct suj_seg *seg;
	union jrec *rec;
	int i;

	TAILQ_FOREACH(seg, &allsegs, ss_next) {
		rec = (union jrec *)seg->ss_blk;
		rec++;	/* skip the segrec. */
		if (debug)
			printf("seg %jd has %d records, oldseq %jd.\n",
			    seg->ss_rec.jsr_seq, seg->ss_rec.jsr_cnt,
			    seg->ss_rec.jsr_oldest);
		for (i = 0; i < seg->ss_rec.jsr_cnt; i++, rec++) {
			switch (rec->rec_jrefrec.jr_op) {
			case JOP_ADDREF:
			case JOP_REMREF:
				suj_build_ino((struct jrefrec *)rec);
				break;
			case JOP_MVREF:
				suj_move_ino((struct jmvrec *)rec);
				break;
			case JOP_NEWBLK:
			case JOP_FREEBLK:
				suj_build_blk((struct jblkrec *)rec);
				break;
			default:
				errx(1, "Unknown journal operation %d (%d)",
				    rec->rec_jrefrec.jr_op, i);
			}
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
	if (newseq != oldseq)
		errx(1, "Journal file sequence mismatch %jd != %jd",
		    newseq, oldseq);
	/*
	 * The kernel may asynchronously write segments which can create
	 * gaps in the sequence space.  Throw away any segments after the
	 * gap as the kernel guarantees only those that are contiguously
	 * reachable are marked as completed.
	 */
	discard = 0;
	TAILQ_FOREACH_SAFE(seg, &allsegs, ss_next, segn) {
		if (!discard && newseq++ == seg->ss_rec.jsr_seq)
			continue;
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
static void
suj_verifyino(union dinode *ip)
{

	if (DIP(ip, di_nlink) != 1)
		errx(1, "Invalid link count %d for journal inode %d",
		    DIP(ip, di_nlink), fs->fs_sujournal);

	if (DIP(ip, di_mode) != IFREG)
		errx(1, "Invalid mode %d for journal inode %d",
		    DIP(ip, di_mode), fs->fs_sujournal);

	if (DIP(ip, di_size) < SUJ_MIN || DIP(ip, di_size) > SUJ_MAX)
		errx(1, "Invalid size %jd for journal inode %d",
		    DIP(ip, di_size), fs->fs_sujournal);

	if (DIP(ip, di_modrev) != fs->fs_mtime)
		errx(1, "Journal timestamp does not match fs mount time");
	/* XXX Add further checks. */
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

struct jblocks *suj_jblocks;

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

	blocks = bytes / DEV_BSIZE;
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
	*actual = freecnt * DEV_BSIZE;
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

	jblocks->jb_off += bytes / DEV_BSIZE;
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
 * buffer and segments may span mutliple contiguous blocks.
 */
static void
suj_add_block(ino_t ino, ufs_lbn_t lbn, ufs2_daddr_t blk, int frags)
{

	jblocks_add(suj_jblocks, fsbtodb(fs, blk), fsbtodb(fs, frags));
}

static void
suj_read(void)
{
	uint8_t block[1 * 1024 * 1024];
	struct suj_seg *seg;
	struct jsegrec *rec;
	ufs2_daddr_t blk;
	int recsize;
	int size;

	/*
	 * Read records until we exhaust the journal space.  If we find
	 * an invalid record we start searching for a valid segment header
	 * at the next block.  This is because we don't have a head/tail
	 * pointer and must recover the information indirectly.  At the gap
	 * between the head and tail we won't necessarily have a valid
	 * segment.
	 */
	for (;;) {
		size = sizeof(block);
		blk = jblocks_next(suj_jblocks, size, &size);
		if (blk == 0)
			return;
		/*
		 * Read 1MB at a time and scan for records within this block.
		 */
		if (bread(disk, blk, &block, size) == -1)
			err(1, "Error reading journal block %jd",
			    (intmax_t)blk);
		for (rec = (void *)block; size; size -= recsize,
		    rec = (struct jsegrec *)((uintptr_t)rec + recsize)) {
			recsize = DEV_BSIZE;
			if (rec->jsr_time != fs->fs_mtime) {
				if (debug)
					printf("Rec time %jd != fs mtime %jd\n",
					    rec->jsr_time, fs->fs_mtime);
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
			recsize = roundup2((rec->jsr_cnt + 1) * JREC_SIZE,
			    DEV_BSIZE);
			if (recsize > size) {
				/*
				 * We may just have run out of buffer, restart
				 * the loop to re-read from this spot.
				 */
				if (size < fs->fs_bsize && 
				    recsize <= fs->fs_bsize) {
					recsize = size;
					continue;
				}
				if (debug)
					printf("Found invalid segsize %d > %d\n",
					    recsize, size);
				recsize = DEV_BSIZE;
				jblocks_advance(suj_jblocks, recsize);
				continue;
			}
			seg = errmalloc(sizeof(*seg));
			seg->ss_blk = errmalloc(recsize);
			seg->ss_rec = *rec;
			bcopy((void *)rec, seg->ss_blk, recsize);
			if (rec->jsr_oldest > oldseq)
				oldseq = rec->jsr_oldest;
			TAILQ_INSERT_TAIL(&allsegs, seg, ss_next);
			jrecs += rec->jsr_cnt;
			jbytes += recsize;
			jblocks_advance(suj_jblocks, recsize);
		}
	}
}

/*
 * Orchestrate the verification of a filesystem via the softupdates journal.
 */
void
suj_check(const char *filesys)
{
	union dinode *jip;
	uint64_t blocks;

	opendisk(filesys);
	TAILQ_INIT(&allsegs);
	/*
	 * Fetch the journal inode and verify it.
	 */
	jip = ino_read(fs->fs_sujournal);
	printf("SU+J Checking %s\n", filesys);
	suj_verifyino(jip);
	/*
	 * Build a list of journal blocks in jblocks before parsing the
	 * available journal blocks in with suj_read().
	 */
	printf("Reading %jd byte journal from inode %d.\n",
	    DIP(jip, di_size), fs->fs_sujournal);
	suj_jblocks = jblocks_create();
	blocks = ino_visit(jip, fs->fs_sujournal, suj_add_block, 0);
	if (blocks != numfrags(fs, DIP(jip, di_size)))
		errx(1, "Sparse journal inode %d.\n", fs->fs_sujournal);
	suj_read();
	jblocks_destroy(suj_jblocks);
	suj_jblocks = NULL;
	if (reply("RECOVER")) {
		printf("Building recovery table.\n");
		suj_prune();
		suj_build();
		printf("Resolving unreferenced inode list.\n");
		ino_unlinked();
		printf("Processing journal entries.\n");
		cg_apply(cg_check);
	}
	if (reply("WRITE CHANGES"))
		cg_apply(cg_write);
	printf("%jd journal records in %jd bytes for %.2f%% utilization\n",
	    jrecs, jbytes, ((float)jrecs / (float)(jbytes / JREC_SIZE)) * 100);
	printf("Freed %jd inodes (%jd directories) %jd blocks and %jd frags.\n",
	    freeinos, freedir, freeblocks, freefrags);
	/* Write back superblock. */
	closedisk(filesys);
}
