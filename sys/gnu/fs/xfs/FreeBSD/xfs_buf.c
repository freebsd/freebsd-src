/*
 *
 *
 */
#include "xfs.h"
#include "xfs_macros.h"
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

xfs_buf_t *
xfs_buf_read_flags(xfs_buftarg_t *target, xfs_daddr_t blkno, size_t len, int flags)
{
	struct buf *bp;
	struct g_consumer *cp;

	KASSERT((target != NULL), ("got NULL buftarg_t"));

	cp = target->specvp->v_bufobj.bo_private;
	if (cp == NULL) {
		bp = NULL;
		goto done;
	}

	/* This restriction is in GEOM's g_io_request() */
	if ((BBTOB(len) % cp->provider->sectorsize) != 0) {
		printf("Read request %ld does not align with sector size: %d\n",
		    (long)BBTOB(len), cp->provider->sectorsize);
		bp = NULL;
		goto done;
	}

	if (bread(target->specvp, blkno, BBTOB(len), NOCRED, &bp)) {
		printf("bread failed specvp %p blkno %qd BBTOB(len) %ld\n",
		       target->specvp, blkno, (long)BBTOB(len));
		bp = NULL;
		goto done;
	}
	if (flags & B_MANAGED)
		bp->b_flags |= B_MANAGED;
	xfs_buf_set_target(bp, target);

done:
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
xfs_baread(xfs_buftarg_t *targp, xfs_daddr_t ioff, size_t isize)
{
	daddr_t rablkno;
	int rabsize;

	rablkno = ioff;
	rabsize = BBTOB(isize);
	breada(targp->specvp, &rablkno, &rabsize, 1, NOCRED);
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
			if (BUF_REFCNT(bp) > 1)
				BUF_UNLOCK(bp);
			else
				brelse(bp);
		}
		return (error);
	}
	error = bwrite(bp);
	return (error);
}

void
xfs_bpin(xfs_buf_t *bp)
{
	printf("xfs_bpin(%p)\n", bp);
	bpin(bp);
}

void
xfs_bunpin(xfs_buf_t *bp)
{
	printf("xfs_bunpin(%p)\n", bp);
	bunpin(bp);
}

int
xfs_ispin(xfs_buf_t *bp)
{
	printf("xfs_ispin(%p)\n", bp);
	return bp->b_pin_count;
}

void
xfs_bwait_unpin(xfs_buf_t *bp)
{
	printf("xfs_bwait_unpin(%p)\n", bp);
	bunpin_wait(bp);
}
