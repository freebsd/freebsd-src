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
static const char sccsid[] = "@(#)inode.c	8.8 (Berkeley) 4/28/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/sysctl.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <pwd.h>
#include <string.h>

#include "fsck.h"

static ino_t startinum;

static int iblock __P((struct inodesc *, long ilevel, quad_t isize));

int
ckinode(dp, idesc)
	struct dinode *dp;
	register struct inodesc *idesc;
{
	ufs_daddr_t *ap;
	int ret;
	long n, ndb, offset;
	struct dinode dino;
	quad_t remsize, sizepb;
	mode_t mode;
	char pathbuf[MAXPATHLEN + 1];

	if (idesc->id_fix != IGNORE)
		idesc->id_fix = DONTKNOW;
	idesc->id_lbn = -1;
	idesc->id_entryno = 0;
	idesc->id_filesize = dp->di_size;
	mode = dp->di_mode & IFMT;
	if (mode == IFBLK || mode == IFCHR || (mode == IFLNK &&
	    dp->di_size < (unsigned)sblock.fs_maxsymlinklen))
		return (KEEPON);
	dino = *dp;
	ndb = howmany(dino.di_size, sblock.fs_bsize);
	for (ap = &dino.di_db[0]; ap < &dino.di_db[NDADDR]; ap++) {
		idesc->id_lbn++;
		if (--ndb == 0 && (offset = blkoff(&sblock, dino.di_size)) != 0)
			idesc->id_numfrags =
				numfrags(&sblock, fragroundup(&sblock, offset));
		else
			idesc->id_numfrags = sblock.fs_frag;
		if (*ap == 0) {
			if (idesc->id_type == DATA && ndb >= 0) {
				/* An empty block in a directory XXX */
				getpathname(pathbuf, idesc->id_number,
						idesc->id_number);
                        	pfatal("DIRECTORY %s: CONTAINS EMPTY BLOCKS",
					pathbuf);
                        	if (reply("ADJUST LENGTH") == 1) {
					dp = ginode(idesc->id_number);
                                	dp->di_size = (ap - &dino.di_db[0]) *
					    sblock.fs_bsize;
					printf(
					    "YOU MUST RERUN FSCK AFTERWARDS\n");
					rerun = 1;
                                	inodirty();
					
                        	}
			}
			continue;
		}
		idesc->id_blkno = *ap;
		if (idesc->id_type != DATA)
			ret = (*idesc->id_func)(idesc);
		else
			ret = dirscan(idesc);
		if (ret & STOP)
			return (ret);
	}
	idesc->id_numfrags = sblock.fs_frag;
	remsize = dino.di_size - sblock.fs_bsize * NDADDR;
	sizepb = sblock.fs_bsize;
	for (ap = &dino.di_ib[0], n = 1; n <= NIADDR; ap++, n++) {
		sizepb *= NINDIR(&sblock);
		if (*ap) {
			idesc->id_blkno = *ap;
			ret = iblock(idesc, n, remsize);
			if (ret & STOP)
				return (ret);
		} else {
			idesc->id_lbn += sizepb / sblock.fs_bsize;
			if (idesc->id_type == DATA && remsize > 0) {
				/* An empty block in a directory XXX */
				getpathname(pathbuf, idesc->id_number,
						idesc->id_number);
                        	pfatal("DIRECTORY %s: CONTAINS EMPTY BLOCKS",
					pathbuf);
                        	if (reply("ADJUST LENGTH") == 1) {
					dp = ginode(idesc->id_number);
                                	dp->di_size -= remsize;
					remsize = 0;
					printf(
					    "YOU MUST RERUN FSCK AFTERWARDS\n");
					rerun = 1;
                                	inodirty();
					break;
                        	}
			}
		}
		remsize -= sizepb;
	}
	return (KEEPON);
}

static int
iblock(idesc, ilevel, isize)
	struct inodesc *idesc;
	long ilevel;
	quad_t isize;
{
	ufs_daddr_t *ap;
	ufs_daddr_t *aplim;
	struct bufarea *bp;
	int i, n, (*func)(), nif;
	quad_t sizepb;
	char buf[BUFSIZ];
	char pathbuf[MAXPATHLEN + 1];
	struct dinode *dp;

	if (idesc->id_type != DATA) {
		func = idesc->id_func;
		if (((n = (*func)(idesc)) & KEEPON) == 0)
			return (n);
	} else
		func = dirscan;
	if (chkrange(idesc->id_blkno, idesc->id_numfrags))
		return (SKIP);
	bp = getdatablk(idesc->id_blkno, sblock.fs_bsize);
	ilevel--;
	for (sizepb = sblock.fs_bsize, i = 0; i < ilevel; i++)
		sizepb *= NINDIR(&sblock);
	nif = howmany(isize , sizepb);
	if (nif > NINDIR(&sblock))
		nif = NINDIR(&sblock);
	if (idesc->id_func == pass1check && nif < NINDIR(&sblock)) {
		aplim = &bp->b_un.b_indir[NINDIR(&sblock)];
		for (ap = &bp->b_un.b_indir[nif]; ap < aplim; ap++) {
			if (*ap == 0)
				continue;
			(void)sprintf(buf, "PARTIALLY TRUNCATED INODE I=%lu",
			    (u_long)idesc->id_number);
			if (preen) {
				pfatal("%s", buf);
			} else if (dofix(idesc, buf)) {
				*ap = 0;
				dirty(bp);
			}
		}
		flush(fswritefd, bp);
	}
	aplim = &bp->b_un.b_indir[nif];
	for (ap = bp->b_un.b_indir; ap < aplim; ap++) {
		if (ilevel == 0)
			idesc->id_lbn++;
		if (*ap) {
			idesc->id_blkno = *ap;
			if (ilevel == 0)
				n = (*func)(idesc);
			else
				n = iblock(idesc, ilevel, isize);
			if (n & STOP) {
				bp->b_flags &= ~B_INUSE;
				return (n);
			}
		} else {
			if (idesc->id_type == DATA && isize > 0) {
				/* An empty block in a directory XXX */
				getpathname(pathbuf, idesc->id_number,
						idesc->id_number);
                        	pfatal("DIRECTORY %s: CONTAINS EMPTY BLOCKS",
					pathbuf);
                        	if (reply("ADJUST LENGTH") == 1) {
					dp = ginode(idesc->id_number);
                                	dp->di_size -= isize;
					isize = 0;
					printf(
					    "YOU MUST RERUN FSCK AFTERWARDS\n");
					rerun = 1;
                                	inodirty();
					bp->b_flags &= ~B_INUSE;
					return(STOP);
                        	}
			}
		}
		isize -= sizepb;
	}
	bp->b_flags &= ~B_INUSE;
	return (KEEPON);
}

/*
 * Check that a block in a legal block number.
 * Return 0 if in range, 1 if out of range.
 */
int
chkrange(blk, cnt)
	ufs_daddr_t blk;
	int cnt;
{
	register int c;

	if (cnt <= 0 || blk <= 0 || blk > maxfsblock ||
	    cnt - 1 > maxfsblock - blk)
		return (1);
	if (cnt > sblock.fs_frag ||
	    fragnum(&sblock, blk) + cnt > sblock.fs_frag) {
		if (debug)
			printf("bad size: blk %ld, offset %i, size %d\n",
			    (long)blk, (int)fragnum(&sblock, blk), cnt);
		return (1);
	}
	c = dtog(&sblock, blk);
	if (blk < cgdmin(&sblock, c)) {
		if ((blk + cnt) > cgsblock(&sblock, c)) {
			if (debug) {
				printf("blk %ld < cgdmin %ld;",
				    (long)blk, (long)cgdmin(&sblock, c));
				printf(" blk + cnt %ld > cgsbase %ld\n",
				    (long)(blk + cnt),
				    (long)cgsblock(&sblock, c));
			}
			return (1);
		}
	} else {
		if ((blk + cnt) > cgbase(&sblock, c+1)) {
			if (debug)  {
				printf("blk %ld >= cgdmin %ld;",
				    (long)blk, (long)cgdmin(&sblock, c));
				printf(" blk + cnt %ld > sblock.fs_fpg %ld\n",
				    (long)(blk + cnt), (long)sblock.fs_fpg);
			}
			return (1);
		}
	}
	return (0);
}

/*
 * General purpose interface for reading inodes.
 */
struct dinode *
ginode(inumber)
	ino_t inumber;
{
	ufs_daddr_t iblk;

	if (inumber < ROOTINO || inumber > maxino)
		errx(EEXIT, "bad inode number %d to ginode", inumber);
	if (startinum == 0 ||
	    inumber < startinum || inumber >= startinum + INOPB(&sblock)) {
		iblk = ino_to_fsba(&sblock, inumber);
		if (pbp != 0)
			pbp->b_flags &= ~B_INUSE;
		pbp = getdatablk(iblk, sblock.fs_bsize);
		startinum = (inumber / INOPB(&sblock)) * INOPB(&sblock);
	}
	return (&pbp->b_un.b_dinode[inumber % INOPB(&sblock)]);
}

/*
 * Special purpose version of ginode used to optimize first pass
 * over all the inodes in numerical order.
 */
ino_t nextino, lastinum, lastvalidinum;
long readcnt, readpercg, fullcnt, inobufsize, partialcnt, partialsize;
struct dinode *inodebuf;

struct dinode *
getnextinode(inumber)
	ino_t inumber;
{
	long size;
	ufs_daddr_t dblk;
	static struct dinode *dp;

	if (inumber != nextino++ || inumber > lastvalidinum)
		errx(EEXIT, "bad inode number %d to nextinode", inumber);
	if (inumber >= lastinum) {
		readcnt++;
		dblk = fsbtodb(&sblock, ino_to_fsba(&sblock, lastinum));
		if (readcnt % readpercg == 0) {
			size = partialsize;
			lastinum += partialcnt;
		} else {
			size = inobufsize;
			lastinum += fullcnt;
		}
		/*
		 * If bread returns an error, it will already have zeroed
		 * out the buffer, so we do not need to do so here.
		 */
		(void)bread(fsreadfd, (char *)inodebuf, dblk, size);
		dp = inodebuf;
	}
	return (dp++);
}

void
setinodebuf(inum)
	ino_t inum;
{

	if (inum % sblock.fs_ipg != 0)
		errx(EEXIT, "bad inode number %d to setinodebuf", inum);
	lastvalidinum = inum + sblock.fs_ipg - 1;
	startinum = 0;
	nextino = inum;
	lastinum = inum;
	readcnt = 0;
	if (inodebuf != NULL)
		return;
	inobufsize = blkroundup(&sblock, INOBUFSIZE);
	fullcnt = inobufsize / sizeof(struct dinode);
	readpercg = sblock.fs_ipg / fullcnt;
	partialcnt = sblock.fs_ipg % fullcnt;
	partialsize = partialcnt * sizeof(struct dinode);
	if (partialcnt != 0) {
		readpercg++;
	} else {
		partialcnt = fullcnt;
		partialsize = inobufsize;
	}
	if ((inodebuf = (struct dinode *)malloc((unsigned)inobufsize)) == NULL)
		errx(EEXIT, "cannot allocate space for inode buffer");
}

void
freeinodebuf()
{

	if (inodebuf != NULL)
		free((char *)inodebuf);
	inodebuf = NULL;
}

/*
 * Routines to maintain information about directory inodes.
 * This is built during the first pass and used during the
 * second and third passes.
 *
 * Enter inodes into the cache.
 */
void
cacheino(dp, inumber)
	register struct dinode *dp;
	ino_t inumber;
{
	register struct inoinfo *inp;
	struct inoinfo **inpp;
	int blks;

	blks = howmany(dp->di_size, sblock.fs_bsize);
	if (blks > NDADDR)
		blks = NDADDR + NIADDR;
	inp = (struct inoinfo *)
		malloc(sizeof(*inp) + (blks - 1) * sizeof(ufs_daddr_t));
	if (inp == NULL)
		errx(EEXIT, "cannot increase directory list");
	inpp = &inphead[inumber % dirhash];
	inp->i_nexthash = *inpp;
	*inpp = inp;
	inp->i_parent = inumber == ROOTINO ? ROOTINO : (ino_t)0;
	inp->i_dotdot = (ino_t)0;
	inp->i_number = inumber;
	inp->i_isize = dp->di_size;
	inp->i_numblks = blks * sizeof(ufs_daddr_t);
	memmove(&inp->i_blks[0], &dp->di_db[0], (size_t)inp->i_numblks);
	if (inplast == listmax) {
		listmax += 100;
		inpsort = (struct inoinfo **)realloc((char *)inpsort,
		    (unsigned)listmax * sizeof(struct inoinfo *));
		if (inpsort == NULL)
			errx(EEXIT, "cannot increase directory list");
	}
	inpsort[inplast++] = inp;
}

/*
 * Look up an inode cache structure.
 */
struct inoinfo *
getinoinfo(inumber)
	ino_t inumber;
{
	register struct inoinfo *inp;

	for (inp = inphead[inumber % dirhash]; inp; inp = inp->i_nexthash) {
		if (inp->i_number != inumber)
			continue;
		return (inp);
	}
	errx(EEXIT, "cannot find inode %d", inumber);
	return ((struct inoinfo *)0);
}

/*
 * Clean up all the inode cache structure.
 */
void
inocleanup()
{
	register struct inoinfo **inpp;

	if (inphead == NULL)
		return;
	for (inpp = &inpsort[inplast - 1]; inpp >= inpsort; inpp--)
		free((char *)(*inpp));
	free((char *)inphead);
	free((char *)inpsort);
	inphead = inpsort = NULL;
}

void
inodirty()
{

	dirty(pbp);
}

void
clri(idesc, type, flag)
	register struct inodesc *idesc;
	char *type;
	int flag;
{
	register struct dinode *dp;

	dp = ginode(idesc->id_number);
	if (flag == 1) {
		pwarn("%s %s", type,
		    (dp->di_mode & IFMT) == IFDIR ? "DIR" : "FILE");
		pinode(idesc->id_number);
	}
	if (preen || reply("CLEAR") == 1) {
		if (preen)
			printf(" (CLEARED)\n");
		n_files--;
		if (bkgrdflag == 0) {
			(void)ckinode(dp, idesc);
			inoinfo(idesc->id_number)->ino_state = USTATE;
			clearinode(dp);
			inodirty();
		} else {
			cmd.value = idesc->id_number;
			cmd.size = -dp->di_nlink;
			if (debug)
				printf("adjrefcnt ino %ld amt %ld\n",
				    (long)cmd.value, cmd.size);
			if (sysctl(adjrefcnt, MIBSIZE, 0, 0,
			    &cmd, sizeof cmd) == -1)
				rwerror("ADJUST INODE", cmd.value);
		}
	}
}

int
findname(idesc)
	struct inodesc *idesc;
{
	register struct direct *dirp = idesc->id_dirp;

	if (dirp->d_ino != idesc->id_parent || idesc->id_entryno < 2) {
		idesc->id_entryno++;
		return (KEEPON);
	}
	memmove(idesc->id_name, dirp->d_name, (size_t)dirp->d_namlen + 1);
	return (STOP|FOUND);
}

int
findino(idesc)
	struct inodesc *idesc;
{
	register struct direct *dirp = idesc->id_dirp;

	if (dirp->d_ino == 0)
		return (KEEPON);
	if (strcmp(dirp->d_name, idesc->id_name) == 0 &&
	    dirp->d_ino >= ROOTINO && dirp->d_ino <= maxino) {
		idesc->id_parent = dirp->d_ino;
		return (STOP|FOUND);
	}
	return (KEEPON);
}

int
clearentry(idesc)
	struct inodesc *idesc;
{
	register struct direct *dirp = idesc->id_dirp;

	if (dirp->d_ino != idesc->id_parent || idesc->id_entryno < 2) {
		idesc->id_entryno++;
		return (KEEPON);
	}
	dirp->d_ino = 0;
	return (STOP|FOUND|ALTERED);
}

void
pinode(ino)
	ino_t ino;
{
	register struct dinode *dp;
	register char *p;
	struct passwd *pw;
	time_t t;

	printf(" I=%lu ", (u_long)ino);
	if (ino < ROOTINO || ino > maxino)
		return;
	dp = ginode(ino);
	printf(" OWNER=");
	if ((pw = getpwuid((int)dp->di_uid)) != 0)
		printf("%s ", pw->pw_name);
	else
		printf("%u ", (unsigned)dp->di_uid);
	printf("MODE=%o\n", dp->di_mode);
	if (preen)
		printf("%s: ", cdevname);
	printf("SIZE=%qu ", dp->di_size);
	t = dp->di_mtime;
	p = ctime(&t);
	printf("MTIME=%12.12s %4.4s ", &p[4], &p[20]);
}

void
blkerror(ino, type, blk)
	ino_t ino;
	char *type;
	ufs_daddr_t blk;
{

	pfatal("%ld %s I=%lu", (long)blk, type, (u_long)ino);
	printf("\n");
	switch (inoinfo(ino)->ino_state) {

	case FSTATE:
		inoinfo(ino)->ino_state = FCLEAR;
		return;

	case DSTATE:
		inoinfo(ino)->ino_state = DCLEAR;
		return;

	case FCLEAR:
	case DCLEAR:
		return;

	default:
		errx(EEXIT, "BAD STATE %d TO BLKERR", inoinfo(ino)->ino_state);
		/* NOTREACHED */
	}
}

/*
 * allocate an unused inode
 */
ino_t
allocino(request, type)
	ino_t request;
	int type;
{
	register ino_t ino;
	register struct dinode *dp;
	struct cg *cgp = &cgrp;
	int cg;

	if (request == 0)
		request = ROOTINO;
	else if (inoinfo(request)->ino_state != USTATE)
		return (0);
	for (ino = request; ino < maxino; ino++)
		if (inoinfo(ino)->ino_state == USTATE)
			break;
	if (ino == maxino)
		return (0);
	cg = ino_to_cg(&sblock, ino);
	getblk(&cgblk, cgtod(&sblock, cg), sblock.fs_cgsize);
	if (!cg_chkmagic(cgp))
		pfatal("CG %d: BAD MAGIC NUMBER\n", cg);
	setbit(cg_inosused(cgp), ino % sblock.fs_ipg);
	cgp->cg_cs.cs_nifree--;
	switch (type & IFMT) {
	case IFDIR:
		inoinfo(ino)->ino_state = DSTATE;
		cgp->cg_cs.cs_ndir++;
		break;
	case IFREG:
	case IFLNK:
		inoinfo(ino)->ino_state = FSTATE;
		break;
	default:
		return (0);
	}
	cgdirty();
	dp = ginode(ino);
	dp->di_db[0] = allocblk((long)1);
	if (dp->di_db[0] == 0) {
		inoinfo(ino)->ino_state = USTATE;
		return (0);
	}
	dp->di_mode = type;
	dp->di_flags = 0;
	dp->di_atime = time(NULL);
	dp->di_mtime = dp->di_ctime = dp->di_atime;
	dp->di_mtimensec = dp->di_ctimensec = dp->di_atimensec = 0;
	dp->di_size = sblock.fs_fsize;
	dp->di_blocks = btodb(sblock.fs_fsize);
	n_files++;
	inodirty();
	if (newinofmt)
		inoinfo(ino)->ino_type = IFTODT(type);
	return (ino);
}

/*
 * deallocate an inode
 */
void
freeino(ino)
	ino_t ino;
{
	struct inodesc idesc;
	struct dinode *dp;

	memset(&idesc, 0, sizeof(struct inodesc));
	idesc.id_type = ADDR;
	idesc.id_func = pass4check;
	idesc.id_number = ino;
	dp = ginode(ino);
	(void)ckinode(dp, &idesc);
	clearinode(dp);
	inodirty();
	inoinfo(ino)->ino_state = USTATE;
	n_files--;
}
