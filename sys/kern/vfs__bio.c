/*
 * Copyright (c) 1989, 1990, 1991, 1992 William F. Jolitz, TeleMuse
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
 *	This software is a component of "386BSD" developed by 
	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE THIS WORK.
 *
 * FOR USERS WHO WISH TO UNDERSTAND THE 386BSD SYSTEM DEVELOPED
 * BY WILLIAM F. JOLITZ, WE RECOMMEND THE USER STUDY WRITTEN 
 * REFERENCES SUCH AS THE  "PORTING UNIX TO THE 386" SERIES 
 * (BEGINNING JANUARY 1991 "DR. DOBBS JOURNAL", USA AND BEGINNING 
 * JUNE 1991 "UNIX MAGAZIN", GERMANY) BY WILLIAM F. JOLITZ AND 
 * LYNNE GREER JOLITZ, AS WELL AS OTHER BOOKS ON UNIX AND THE 
 * ON-LINE 386BSD USER MANUAL BEFORE USE. A BOOK DISCUSSING THE INTERNALS 
 * OF 386BSD ENTITLED "386BSD FROM THE INSIDE OUT" WILL BE AVAILABLE LATE 1992.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: vfs__bio.c,v 1.21 1994/05/29 18:06:31 ats Exp $
 */

#include "param.h"
#include "systm.h"
#include "kernel.h"
#include "proc.h"
#include "vnode.h"
#include "buf.h"
#include "specdev.h"
#include "mount.h"
#include "malloc.h"
#include "vm/vm.h"
#include "resourcevar.h"

/* From sys/buf.h */
struct	buf *buf;		/* the buffer pool itself */
char	*buffers;
int	nbuf;			/* number of buffer headers */
int	bufpages;		/* number of memory pages in the buffer pool */
struct	buf *swbuf;		/* swap I/O headers */
int	nswbuf;
struct	bufhd bufhash[BUFHSZ];	/* heads of hash lists */
struct	buf bfreelist[BQUEUES];	/* heads of available lists */
struct	buf bswlist;		/* head of free swap header list */
struct	buf *bclnlist;		/* head of cleaned page list */

static struct buf *getnewbuf(int);
extern	vm_map_t buffer_map, io_map;

/*
 * Internel update daemon, process 3
 *	The variable vfs_update_wakeup allows for internal syncs.
 */
int vfs_update_wakeup;

/*
 * Initialize buffer headers and related structures.
 */
void bufinit()
{
	struct bufhd *bh;
	struct buf *bp;

	/* first, make a null hash table */
	for(bh = bufhash; bh < bufhash + BUFHSZ; bh++) {
		bh->b_flags = 0;
		bh->b_forw = (struct buf *)bh;
		bh->b_back = (struct buf *)bh;
	}

	/* next, make a null set of free lists */
	for(bp = bfreelist; bp < bfreelist + BQUEUES; bp++) {
		bp->b_flags = 0;
		bp->av_forw = bp;
		bp->av_back = bp;
		bp->b_forw = bp;
		bp->b_back = bp;
	}

	/* finally, initialize each buffer header and stick on empty q */
	for(bp = buf; bp < buf + nbuf ; bp++) {
		bp->b_flags = B_HEAD | B_INVAL;	/* we're just an empty header */
		bp->b_dev = NODEV;
		bp->b_vp = 0;
		binstailfree(bp, bfreelist + BQ_EMPTY);
		binshash(bp, bfreelist + BQ_EMPTY);
	}
}

/*
 * Find the block in the buffer pool.
 * If the buffer is not present, allocate a new buffer and load
 * its contents according to the filesystem fill routine.
 */
int
bread(struct vnode *vp, daddr_t blkno, int size, struct ucred *cred,
	struct buf **bpp)
{
	struct buf *bp;
	int rv = 0;

	bp = getblk (vp, blkno, size);

	/* if not found in cache, do some I/O */
	if ((bp->b_flags & B_CACHE) == 0 || (bp->b_flags & B_INVAL) != 0) {
		if (curproc && curproc->p_stats)	/* count block I/O */
			curproc->p_stats->p_ru.ru_inblock++;
		bp->b_flags |= B_READ;
		bp->b_flags &= ~(B_DONE|B_ERROR|B_INVAL);
		if (cred != NOCRED) crhold(cred);		/* 25 Apr 92*/
		bp->b_rcred = cred;
		VOP_STRATEGY(bp);
		rv = biowait (bp);
	}
	*bpp = bp;
	
	return (rv);
}

/*
 * Operates like bread, but also starts I/O on the specified
 * read-ahead block. [See page 55 of Bach's Book]
 */
int
breada(struct vnode *vp, daddr_t blkno, int size, daddr_t rablkno, int rabsize,
	struct ucred *cred, struct buf **bpp)
{
	struct buf *bp, *rabp;
	int rv = 0, needwait = 0;

	bp = getblk (vp, blkno, size);

	/* if not found in cache, do some I/O */
	if ((bp->b_flags & B_CACHE) == 0 || (bp->b_flags & B_INVAL) != 0) {
		if (curproc && curproc->p_stats)	/* count block I/O */
			curproc->p_stats->p_ru.ru_inblock++;
		bp->b_flags |= B_READ;
		bp->b_flags &= ~(B_DONE|B_ERROR|B_INVAL);
		if (cred != NOCRED) crhold(cred);		/* 25 Apr 92*/
		bp->b_rcred = cred;
		VOP_STRATEGY(bp);
		needwait++;
	}
	
	rabp = getblk (vp, rablkno, rabsize);

	/* if not found in cache, do some I/O (overlapped with first) */
	if ((rabp->b_flags & B_CACHE) == 0 || (rabp->b_flags & B_INVAL) != 0) {
		if (curproc && curproc->p_stats)	/* count block I/O */
			curproc->p_stats->p_ru.ru_inblock++;
		rabp->b_flags |= B_READ | B_ASYNC;
		rabp->b_flags &= ~(B_DONE|B_ERROR|B_INVAL);
		if (cred != NOCRED) crhold(cred);		/* 25 Apr 92*/
		rabp->b_rcred = cred;
		VOP_STRATEGY(rabp);
	} else
		brelse(rabp);
	
	/* wait for original I/O */
	if (needwait)
		rv = biowait (bp);

	*bpp = bp;
	return (rv);
}

/*
 * Synchronous write.
 * Release buffer on completion.
 */
int
bwrite(register struct buf *bp)
{
	int rv;

	if(bp->b_flags & B_INVAL) {
		brelse(bp);
		return (0);
	} else {
		int wasdelayed;

		if(!(bp->b_flags & B_BUSY))
			panic("bwrite: not busy");

		wasdelayed = bp->b_flags & B_DELWRI;
		bp->b_flags &= ~(B_READ|B_DONE|B_ERROR|B_ASYNC|B_DELWRI);
		if(wasdelayed)
			reassignbuf(bp, bp->b_vp);

		if (curproc && curproc->p_stats)	/* count block I/O */
			curproc->p_stats->p_ru.ru_oublock++;
		bp->b_flags |= B_DIRTY;
		bp->b_vp->v_numoutput++;
		VOP_STRATEGY(bp);
		rv = biowait(bp);
		brelse(bp);
		return (rv);
	}
}

/*
 * Delayed write.
 *
 * The buffer is marked dirty, but is not queued for I/O.
 * This routine should be used when the buffer is expected
 * to be modified again soon, typically a small write that
 * partially fills a buffer.
 *
 * NB: magnetic tapes cannot be delayed; they must be
 * written in the order that the writes are requested.
 */
void
bdwrite(register struct buf *bp)
{

	if(!(bp->b_flags & B_BUSY))
		panic("bdwrite: not busy");

	if(bp->b_flags & B_INVAL) {
		brelse(bp);
		return;
	}
	if(bp->b_flags & B_TAPE) {
		bwrite(bp);
		return;
	}
	bp->b_flags &= ~(B_READ|B_DONE);
	bp->b_flags |= B_DIRTY|B_DELWRI;
	reassignbuf(bp, bp->b_vp);
	brelse(bp);
	return;
}

/*
 * Asynchronous write.
 * Start I/O on a buffer, but do not wait for it to complete.
 * The buffer is released when the I/O completes.
 */
void
bawrite(register struct buf *bp)
{

	if(!(bp->b_flags & B_BUSY))
		panic("bawrite: not busy");

	if(bp->b_flags & B_INVAL)
		brelse(bp);
	else {
		int wasdelayed;

		wasdelayed = bp->b_flags & B_DELWRI;
		bp->b_flags &= ~(B_READ|B_DONE|B_ERROR|B_DELWRI);
		if(wasdelayed)
			reassignbuf(bp, bp->b_vp);

		if (curproc && curproc->p_stats)	/* count block I/O */
			curproc->p_stats->p_ru.ru_oublock++;
		bp->b_flags |= B_DIRTY | B_ASYNC;
		bp->b_vp->v_numoutput++;
		VOP_STRATEGY(bp);
	}
}

/*
 * Release a buffer.
 * Even if the buffer is dirty, no I/O is started.
 */
void
brelse(register struct buf *bp)
{
	int x;

	/* anyone need a "free" block? */
	x=splbio();
	if ((bfreelist + BQ_AGE)->b_flags & B_WANTED) {
		(bfreelist + BQ_AGE) ->b_flags &= ~B_WANTED;
		wakeup((caddr_t)bfreelist);
	}
	/* anyone need this very block? */
	if (bp->b_flags & B_WANTED) {
		bp->b_flags &= ~B_WANTED;
		wakeup((caddr_t)bp);
	}

	if (bp->b_flags & (B_INVAL|B_ERROR)) {
		bp->b_flags |= B_INVAL;
		bp->b_flags &= ~(B_DELWRI|B_CACHE);
		if(bp->b_vp)
			brelvp(bp);
	}

	/* enqueue */
	/* just an empty buffer head ... */
	/*if(bp->b_flags & B_HEAD)
		binsheadfree(bp, bfreelist + BQ_EMPTY)*/
	/* buffers with junk contents */
	/*else*/ if(bp->b_flags & (B_ERROR|B_INVAL|B_NOCACHE))
		binsheadfree(bp, bfreelist + BQ_AGE)
	/* buffers with stale but valid contents */
	else if(bp->b_flags & B_AGE)
		binstailfree(bp, bfreelist + BQ_AGE)
	/* buffers with valid and quite potentially reuseable contents */
	else
		binstailfree(bp, bfreelist + BQ_LRU)

	/* unlock */
	bp->b_flags &= ~B_BUSY;
	splx(x);

}

int freebufspace;
int allocbufspace;

/*
 * Find a buffer which is available for use.
 * If free memory for buffer space and an empty header from the empty list,
 * use that. Otherwise, select something from a free list.
 * Preference is to AGE list, then LRU list.
 */
static struct buf *
getnewbuf(int sz)
{
	struct buf *bp;
	int x;

	x = splbio();
start:
	/* can we constitute a new buffer? */
	if (freebufspace > sz
		&& bfreelist[BQ_EMPTY].av_forw != (struct buf *)bfreelist+BQ_EMPTY) {
		caddr_t addr;

		if ((addr = malloc (sz, M_IOBUF, M_NOWAIT)) == 0)
			goto tryfree;
		freebufspace -= sz;
		allocbufspace += sz;

		bp = bfreelist[BQ_EMPTY].av_forw;
		bp->b_flags = B_BUSY | B_INVAL;
		bremfree(bp);
		bp->b_un.b_addr = addr;
		bp->b_bufsize = sz;	/* 20 Aug 92*/
		goto fillin;
	}

tryfree:
	if (bfreelist[BQ_AGE].av_forw != (struct buf *)bfreelist+BQ_AGE) {
		bp = bfreelist[BQ_AGE].av_forw;
		bremfree(bp);
	} else if (bfreelist[BQ_LRU].av_forw != (struct buf *)bfreelist+BQ_LRU) {
		bp = bfreelist[BQ_LRU].av_forw;
		bremfree(bp);
	} else	{
		/* wait for a free buffer of any kind */
		(bfreelist + BQ_AGE)->b_flags |= B_WANTED;
		tsleep((caddr_t)bfreelist, PRIBIO, "newbuf", 0);
		splx(x);
		return (0);
	}

	/* if we are a delayed write, convert to an async write! */
	if (bp->b_flags & B_DELWRI) {
		bp->b_flags |= B_BUSY;
		bawrite (bp);
		goto start;
	}


	if(bp->b_vp)
		brelvp(bp);

	/* we are not free, nor do we contain interesting data */
	if (bp->b_rcred != NOCRED) crfree(bp->b_rcred);		/* 25 Apr 92*/
	if (bp->b_wcred != NOCRED) crfree(bp->b_wcred);
	bp->b_flags = B_BUSY;
fillin:
	bremhash(bp);
	splx(x);
	bp->b_dev = NODEV;
	bp->b_vp = NULL;
	bp->b_blkno = bp->b_lblkno = 0;
	bp->b_iodone = 0;
	bp->b_error = 0;
	bp->b_resid = 0;
	bp->b_wcred = bp->b_rcred = NOCRED;
	if (bp->b_bufsize != sz)
		allocbuf(bp, sz);
	bp->b_bcount = bp->b_bufsize = sz;
	bp->b_dirtyoff = bp->b_dirtyend = 0;
	return (bp);
}

/*
 * Check to see if a block is currently memory resident.
 */
struct buf *
incore(struct vnode *vp, daddr_t blkno)
{
	struct buf *bh;
	struct buf *bp;

	bh = BUFHASH(vp, blkno);

	/* Search hash chain */
	bp = bh->b_forw;
	while (bp != (struct buf *) bh) {
		/* hit */
		if (bp->b_lblkno == blkno && bp->b_vp == vp
			&& (bp->b_flags & B_INVAL) == 0)
			return (bp);
		bp = bp->b_forw;
	}
	
	return(0);
}

/*
 * Get a block of requested size that is associated with
 * a given vnode and block offset. If it is found in the
 * block cache, mark it as having been found, make it busy
 * and return it. Otherwise, return an empty block of the
 * correct size. It is up to the caller to insure that the
 * cached blocks be of the correct size.
 */
struct buf *
getblk(register struct vnode *vp, daddr_t blkno, int size)
{
	struct buf *bp, *bh;
	int x;

	x = splbio();
loop:
	if (bp = incore(vp, blkno)) {
		if (bp->b_flags & B_BUSY) {
			bp->b_flags |= B_WANTED;
			tsleep ((caddr_t)bp, PRIBIO, "getblk", 0);
			goto loop;
		}
		bp->b_flags |= B_BUSY | B_CACHE;
		bremfree(bp);
		if (size > bp->b_bufsize)
			panic("now what do we do?");
		/* if (bp->b_bufsize != size) allocbuf(bp, size); */
	} else {

		if ((bp = getnewbuf(size)) == 0)
			goto loop;
		if ( incore(vp, blkno)) {
			bp->b_flags |= B_INVAL;
			brelse(bp);
			goto loop;
		}
			
		bp->b_blkno = bp->b_lblkno = blkno;
		bgetvp(vp, bp);
		bh = BUFHASH(vp, blkno);
		binshash(bp, bh);
		bp->b_flags = B_BUSY;
	}
	splx(x);
	return (bp);
}

/*
 * Get an empty, disassociated buffer of given size.
 */
struct buf *
geteblk(int size)
{
	struct buf *bp;
	int x;

	while ((bp = getnewbuf(size)) == 0)
		;
	x = splbio();
	binshash(bp, bfreelist + BQ_AGE);
	splx(x);

	return (bp);
}

/*
 * Exchange a buffer's underlying buffer storage for one of different
 * size, taking care to maintain contents appropriately. When buffer
 * increases in size, caller is responsible for filling out additional
 * contents. When buffer shrinks in size, data is lost, so caller must
 * first return it to backing store before shrinking the buffer, as
 * no implied I/O will be done.
 *
 * Expanded buffer is returned as value.
 */
void
allocbuf(register struct buf *bp, int size)
{
	caddr_t newcontents;

	/* get new memory buffer */
	newcontents = (caddr_t) malloc (size, M_IOBUF, M_WAITOK);

	/* copy the old into the new, up to the maximum that will fit */
	bcopy (bp->b_un.b_addr, newcontents, min(bp->b_bufsize, size));

	/* return old contents to free heap */
	free (bp->b_un.b_addr, M_IOBUF);

	/* adjust buffer cache's idea of memory allocated to buffer contents */
	freebufspace -= size - bp->b_bufsize;
	allocbufspace += size - bp->b_bufsize;

	/* update buffer header */
	bp->b_un.b_addr = newcontents;
	bp->b_bcount = bp->b_bufsize = size;
}

/*
 * Patiently await operations to complete on this buffer.
 * When they do, extract error value and return it.
 * Extract and return any errors associated with the I/O.
 * If an invalid block, force it off the lookup hash chains.
 */
int
biowait(register struct buf *bp)
{
	int x;

	x = splbio();
	while ((bp->b_flags & B_DONE) == 0)
		tsleep((caddr_t)bp, PRIBIO, "biowait", 0);
	if((bp->b_flags & B_ERROR) || bp->b_error) {
		if ((bp->b_flags & B_INVAL) == 0) {
			bp->b_flags |= B_INVAL;
			bremhash(bp);
			binshash(bp, bfreelist + BQ_AGE);
		}
		if (!bp->b_error)
			bp->b_error = EIO;
		else
			bp->b_flags |= B_ERROR;
		splx(x);
		return (bp->b_error);
	} else {
		splx(x);
		return (0);
	}
}

/*
 * Finish up operations on a buffer, calling an optional function
 *	(if requested), and releasing the buffer if marked asynchronous.
 *	Mark this buffer done so that others biowait()'ing for it will
 *	notice when they are woken up from sleep().
 */
void
biodone(register struct buf *bp)
{
	int s;
	s = splbio();
	if (bp->b_flags & B_CLUSTER) {
		struct buf *tbp;
		bp->b_resid = bp->b_bcount;
		while ( tbp = bp->b_clusterf) {
			bp->b_clusterf = tbp->av_forw;
			bp->b_resid -= tbp->b_bcount;
			tbp->b_resid = 0;
			if( bp->b_resid <= 0) {
				tbp->b_error = bp->b_error;
				tbp->b_flags |= (bp->b_flags & B_ERROR);
				tbp->b_resid = -bp->b_resid;
				bp->b_resid = 0;
			}
/*
			printf("rdc (%d,%d,%d) ", tbp->b_blkno, tbp->b_bcount, tbp->b_resid);
*/
			
			biodone(tbp);
		}
#ifndef NOBOUNCE
		vm_bounce_kva_free( bp->b_un.b_addr, bp->b_bufsize, 0);
#endif
		relpbuf(bp);
		splx(s);
		return;
	}
		
#ifndef NOBOUNCE
	if (bp->b_flags & B_BOUNCE)
		vm_bounce_free(bp);
#endif
	bp->b_flags |= B_DONE;

	if ((bp->b_flags & B_READ) == 0)  {
		vwakeup(bp);
	}

	/* call optional completion function if requested */
	if (bp->b_flags & B_CALL) {
		bp->b_flags &= ~B_CALL;
		(*bp->b_iodone)(bp);
		splx(s);
		return;
	}

/*
 * For asynchronous completions, release the buffer now. The brelse
 *	checks for B_WANTED and will do the wakeup there if necessary -
 *	so no need to do a wakeup here in the async case.
 */

	if (bp->b_flags & B_ASYNC) {
		brelse(bp);
	} else {
		bp->b_flags &= ~B_WANTED;
		wakeup((caddr_t) bp);
	}
	splx(s);
}

#ifndef UPDATE_INTERVAL
int vfs_update_interval = 30;
#else
int vfs_update_interval = UPDATE_INTERVAL;
#endif

void
vfs_update() {
	(void) spl0();
	while(1) {
		tsleep((caddr_t)&vfs_update_wakeup, PRIBIO, "update",
			hz * vfs_update_interval);
		vfs_update_wakeup = 0;
		sync(curproc, NULL, NULL);
	}
}

/*
 * Print out statistics on the current allocation of the buffer pool.
 * Can be enabled to print out on every ``sync'' by setting "syncprt"
 * in ufs/ufs_vfsops.c.
 */
void
bufstats()
{
	int s, i, j, count;
	register struct buf *bp, *dp;
	int counts[MAXBSIZE/CLBYTES+1];
	static char *bname[BQUEUES] = { "LOCKED", "LRU", "AGE", "EMPTY" };

	for (bp = bfreelist, i = 0; bp < &bfreelist[BQUEUES]; bp++, i++) {
		count = 0;
		for (j = 0; j <= MAXBSIZE/CLBYTES; j++)
			counts[j] = 0;
		s = splbio();
		for (dp = bp->av_forw; dp != bp; dp = dp->av_forw) {
			counts[dp->b_bufsize/CLBYTES]++;
			count++;
		}
		splx(s);
		printf("%s: total-%d", bname[i], count);
		for (j = 0; j <= MAXBSIZE/CLBYTES; j++)
			if (counts[j] != 0)
				printf(", %d-%d", j * CLBYTES, counts[j]);
		printf("\n");
	}
}
