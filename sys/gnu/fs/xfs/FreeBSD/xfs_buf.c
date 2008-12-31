/*
 * Copyright (c) 2001,2005 Russell Cattelan
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/gnu/fs/xfs/FreeBSD/xfs_buf.c,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#include "xfs.h"
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_dmapi.h"
#include "xfs_mount.h"
#include "xfs_clnt.h"
#include "xfs_mountops.h"

#include <geom/geom.h>
#include <geom/geom_vfs.h>

xfs_buf_t *
xfs_buf_read_flags(xfs_buftarg_t *target, xfs_daddr_t blkno, size_t len, int flags)
{
	struct buf *bp;
	KASSERT((target != NULL), ("got NULL buftarg_t"));

	if (bread(target->specvp, blkno, BBTOB(len), NOCRED, &bp)) {
		printf("bread failed specvp %p blkno %qd BBTOB(len) %ld\n",
		       target->specvp, blkno, (long)BBTOB(len));
		bp = NULL;
	}

	/* not really sure what B_MANAGED really does for us
	 * maybe we should drop this and just stick with a locked buf
	 */

	if (flags & B_MANAGED)
		bp->b_flags |= B_MANAGED;
	xfs_buf_set_target(bp, target);
	return (bp);
}

xfs_buf_t *
xfs_buf_get_flags(xfs_buftarg_t *target, xfs_daddr_t blkno, size_t len, int flags)
{
	struct buf *bp = NULL;
	KASSERT((target != NULL), ("got NULL buftarg_t"));
	bp = getblk(target->specvp, blkno, BBTOB(len), 0, 0, 0);
	if (bp != NULL)
		xfs_buf_set_target(bp, target);
	return (bp);
}

xfs_buf_t*
xfs_buf_get_empty(size_t size,  xfs_buftarg_t *target)
{
	struct buf *bp;

	bp = geteblk(0);
	if (bp != NULL) {
		bp->b_bufsize = size;
		bp->b_bcount = size;

		KASSERT(BUF_REFCNT(bp) == 1,
			("xfs_buf_get_empty: bp %p not locked",bp));

		xfs_buf_set_target(bp, target);
	}
	return (bp);
}

xfs_buf_t*
xfs_buf_get_noaddr(size_t len, xfs_buftarg_t *target)
{
	struct buf *bp;
	if (len >= MAXPHYS)
		return (NULL);

	bp = geteblk(len);
	if (bp != NULL) {
		KASSERT(BUF_REFCNT(bp) == 1,
			("xfs_buf_get_empty: bp %p not locked",bp));

		xfs_buf_set_target(bp, target);
	}
	return (bp);
}

void
xfs_buf_free(xfs_buf_t *bp)
{
	bp->b_flags |= B_INVAL;
	BUF_KERNPROC(bp);			 /* ugly hack #1 */
	if (bp->b_kvasize == 0) {
		bp->b_saveaddr = bp->b_kvabase;  /* ugly hack #2 */
		bp->b_data = bp->b_saveaddr;
		bp->b_bcount  = 0;
		bp->b_bufsize = 0;
	}
	brelse(bp);
}

void
xfs_buf_readahead(
		  xfs_buftarg_t		*target,
		  xfs_daddr_t		ioff,
		  size_t		isize,
		  xfs_buf_flags_t	flags)
{
	daddr_t rablkno;
	int rabsize;

	rablkno = ioff;
	rabsize = BBTOB(isize);
	breada(target->specvp, &rablkno, &rabsize, 1, NOCRED);
}

void
xfs_buf_set_target(xfs_buf_t *bp, xfs_buftarg_t *targ)
{
	bp->b_bufobj = &targ->specvp->v_bufobj;
	bp->b_caller1 = targ;
}

xfs_buftarg_t *
xfs_buf_get_target(xfs_buf_t *bp)
{
	return (xfs_buftarg_t *)bp->b_caller1;
}

int
XFS_bwrite(xfs_buf_t *bp)
{
	int error;
	if (bp->b_vp == NULL) {
		error = xfs_buf_iorequest(bp);

		if ((bp->b_flags & B_ASYNC) == 0) {
			error = bufwait(bp);
#if 0
			if (BUF_REFCNT(bp) > 1)
				BUF_UNLOCK(bp);
			else
				brelse(bp);
#endif
			brelse(bp);
		}
		return (error);
	}
	error = bwrite(bp);
	return (error);
}

void
xfs_buf_pin(xfs_buf_t *bp)
{
	bpin(bp);
}

void
xfs_buf_unpin(xfs_buf_t *bp)
{
	bunpin(bp);
}

int
xfs_buf_ispin(xfs_buf_t *bp)
{
	return bp->b_pin_count;
}

#if 0
void
xfs_buf_wait_unpin(
	xfs_buf_t *bp)
{
	bunpin_wait(bp);
}
#endif

/*
 *	Move data into or out of a buffer.
 */
void
xfs_buf_iomove(
	xfs_buf_t		*bp,	/* buffer to process		*/
	size_t			boff,	/* starting buffer offset	*/
	size_t			bsize,	/* length to copy		*/
	caddr_t			data,	/* data address			*/
	xfs_buf_rw_t		mode)	/* read/write/zero flag		*/
{

  printf("xfs_buf_iomove NI\n");
#ifdef RMC
	size_t			bend, cpoff, csize;
	struct page		*page;

	bend = boff + bsize;
	while (boff < bend) {
		page = bp->b_pages[xfs_buf_btoct(boff + bp->b_offset)];
		cpoff = xfs_buf_poff(boff + bp->b_offset);
		csize = min_t(size_t,
			      PAGE_CACHE_SIZE-cpoff, bp->b_count_desired-boff);

		ASSERT(((csize + cpoff) <= PAGE_CACHE_SIZE));

		switch (mode) {
		case XBRW_ZERO:
			memset(page_address(page) + cpoff, 0, csize);
			break;
		case XBRW_READ:
			memcpy(data, page_address(page) + cpoff, csize);
			break;
		case XBRW_WRITE:
			memcpy(page_address(page) + cpoff, data, csize);
		}

		boff += csize;
		data += csize;
	}
#endif
}

/*
 *	Handling of buffer targets (buftargs).
 */

/*
 *	Wait for any bufs with callbacks that have been submitted but
 *	have not yet returned... walk the hash list for the target.
 */
void
xfs_wait_buftarg(
		 xfs_buftarg_t *bp)
{
	printf("xfs_wait_buftarg(%p) NI\n", bp);
}

int
xfs_flush_buftarg(
	xfs_buftarg_t		*btp,
	int wait)
{
	int error = 0;

	error = vinvalbuf(btp->specvp, V_SAVE|V_NORMAL, curthread, 0, 0);
	return error;
}

void
xfs_free_buftarg(
	xfs_buftarg_t		*btp,
	int			external)
{
	xfs_flush_buftarg(btp, /* wait */ 0);
	kmem_free(btp, sizeof(*btp));
}

int
xfs_readonly_buftarg(
	xfs_buftarg_t		*btp)
{
	struct g_consumer *cp;

	KASSERT(btp->specvp->v_bufobj.bo_ops == &xfs_bo_ops,
	   ("Bogus xfs_buftarg_t pointer"));
	cp = btp->specvp->v_bufobj.bo_private;
	return (cp->acw == 0);
}

#if 0
void
xfs_relse_buftarg(
	xfs_buftarg_t		*btp)
{
	printf("xfs_relse_buftargNI %p\n",btp);
}
#endif

unsigned int
xfs_getsize_buftarg(
	xfs_buftarg_t		*btp)
{
	struct g_consumer       *cp;
	cp = btp->specvp->v_bufobj.bo_private;
	return (cp->provider->sectorsize);
}

int
xfs_setsize_buftarg(
	xfs_buftarg_t		*btp,
	unsigned int		blocksize,
	unsigned int		sectorsize)
{
	printf("xfs_setsize_buftarg NI %p\n",btp);
	return 0;
}

xfs_buftarg_t *
xfs_alloc_buftarg(
		  struct vnode	*bdev,
		  int		external)
{
	xfs_buftarg_t		*btp;

	btp = kmem_zalloc(sizeof(*btp), KM_SLEEP);

	btp->dev    = bdev->v_rdev;
	btp->specvp = bdev;
	return btp;
}
