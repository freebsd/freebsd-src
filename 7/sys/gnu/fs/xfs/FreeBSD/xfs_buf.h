/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.	 Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */
#ifndef __XFS_BUF_H__
#define __XFS_BUF_H__

#include <sys/bio.h>
#include <sys/buf.h>

struct xfs_buf;
struct xfs_mount;
struct vnode;
typedef struct buf xfs_buf_t;
typedef uint32_t xfs_buf_flags_t;
#define xfs_buf buf

extern struct buf_ops xfs_bo_ops;

typedef enum {
	XBRW_READ = 1,			/* transfer into target memory */
	XBRW_WRITE = 2,			/* transfer from target memory */
	XBRW_ZERO = 3,			/* Zero target memory */
} xfs_buf_rw_t;

/* Buffer Read and Write Routines */
extern void xfs_buf_ioend(xfs_buf_t *,	int);
extern void xfs_buf_ioerror(xfs_buf_t *, int);
extern int xfs_buf_iostart(xfs_buf_t *, xfs_buf_flags_t);
extern int xfs_buf_iorequest(xfs_buf_t *);
extern int xfs_buf_iowait(xfs_buf_t *);
extern void xfs_buf_iomove(xfs_buf_t *, size_t, size_t, xfs_caddr_t, xfs_buf_rw_t);

/* Pinning Buffer Storage in Memory */
extern void xfs_buf_pin(xfs_buf_t *);
extern void xfs_buf_unpin(xfs_buf_t *);
extern int xfs_buf_ispin(xfs_buf_t *);


typedef void (*xfs_buf_iodone_t)(struct xfs_buf *); /* call-back function on I/O completion */
typedef void (*xfs_buf_relse_t)(struct xfs_buf *); /* call-back function on I/O completion */
typedef int (*xfs_buf_bdstrat_t)(struct xfs_buf *);

typedef struct xfs_buftarg {
	/* this probaby redundant info, but stick with linux conventions for now */
	unsigned int	bt_bsize;
	unsigned int	bt_sshift;
	size_t		bt_smask;
	struct cdev	*dev;
	struct vnode	*specvp;
} xfs_buftarg_t;


/* Finding and Reading Buffers */
extern void xfs_buf_readahead(xfs_buftarg_t *, xfs_off_t, size_t, xfs_buf_flags_t);
/* Misc buffer rountines */
extern int xfs_readonly_buftarg(xfs_buftarg_t *);

/* These are just for xfs_syncsub... it sets an internal variable
 * then passes it to VOP_FLUSH_PAGES or adds the flags to a newly gotten buf_t
 */
#define XBF_DONT_BLOCK		0

#define	XFS_B_ASYNC		B_ASYNC
#define	XFS_B_DELWRI		B_DELWRI
#define	XFS_B_READ		BIO_READ
#define	XFS_B_WRITE		BIO_WRITE

#define	XFS_B_STALE		B_INVAL
#define	XFS_BUF_LOCK		0
#define	XFS_BUF_TRYLOCK		0
#define	XFS_BUF_MAPPED		0
#define	BUF_BUSY		0

#define	XBF_ORDERED             0

				/* debugging routines might need this */
#define XFS_BUF_BFLAGS(x)	((x)->b_flags)
#define XFS_BUF_ZEROFLAGS(x)	((x)->b_flags = 0)
#define XFS_BUF_STALE(x)	((x)->b_flags |= (XFS_B_STALE|B_NOCACHE))
#define XFS_BUF_UNSTALE(x)	((x)->b_flags &= ~(XFS_B_STALE|B_NOCACHE))
#define XFS_BUF_ISSTALE(x)	((x)->b_flags & (XFS_B_STALE|B_NOCACHE))
#define XFS_BUF_SUPER_STALE(x)	{(x)->b_flags |= (XFS_B_STALE|B_NOCACHE); \
				(x)->b_flags &= ~(XFS_B_DELWRI|B_CACHE);}

#define XFS_BUF_MANAGE		B_MANAGED
#define XFS_BUF_UNMANAGE(x)	((x)->b_flags &= ~B_MANAGED)

#define XFS_BUF_DELAYWRITE(x)		((x)->b_flags |= XFS_B_DELWRI)
#define XFS_BUF_UNDELAYWRITE(x)	((x)->b_flags &= ~XFS_B_DELWRI)
#define XFS_BUF_ISDELAYWRITE(x)	((x)->b_flags & XFS_B_DELWRI)

#define XFS_BUF_ERROR(x,no)	xfs_buf_set_error((x), (no))
#define XFS_BUF_GETERROR(x)	xfs_buf_get_error(x)
#define XFS_BUF_ISERROR(x)	(((x)->b_ioflags & BIO_ERROR) != 0)

void static __inline__
xfs_buf_set_error(struct buf *bp, int err)
{
	bp->b_ioflags |= BIO_ERROR;
	bp->b_error = err;
}

int static __inline__
xfs_buf_get_error(struct buf *bp)
{
	return XFS_BUF_ISERROR(bp) ? (bp->b_error ? bp->b_error : EIO) : 0;
}

#define XFS_BUF_DONE(x)		((x)->b_flags |= B_CACHE)
#define XFS_BUF_UNDONE(x)	((x)->b_flags &= ~B_CACHE)
#define XFS_BUF_ISDONE(x)	((x)->b_flags & B_CACHE)

#define XFS_BUF_BUSY(x)		((x)->b_flags |= BUF_BUSY)
#define XFS_BUF_UNBUSY(x)	((x)->b_flags &= ~BUF_BUSY)
#define XFS_BUF_ISBUSY(x)	(1)

#define XFS_BUF_ASYNC(x)	((x)->b_flags |=  B_ASYNC)
#define XFS_BUF_UNASYNC(x)	((x)->b_flags &= ~B_ASYNC)
#define XFS_BUF_ISASYNC(x)	((x)->b_flags &   B_ASYNC)

#define XFS_BUF_ORDERED(bp)	((bp)->b_flags |= XBF_ORDERED)
#define XFS_BUF_UNORDERED(bp)	((bp)->b_flags &= ~XBF_ORDERED)
#define XFS_BUF_ISORDERED(bp)	((bp)->b_flags & XBF_ORDERED)

#define XFS_BUF_FLUSH(x)	((x)->b_flags |=  B_00800000)
#define XFS_BUF_UNFLUSH(x)	((x)->b_flags &= ~B_00800000)
#define XFS_BUF_ISFLUSH(x) 	((x)->b_flags &   B_00800000)

#define XFS_BUF_SHUT(x)		printf("XFS_BUF_SHUT not implemented yet\n")
#define XFS_BUF_UNSHUT(x)	printf("XFS_BUF_UNSHUT not implemented yet\n")
#define XFS_BUF_ISSHUT(x)	(0)

#define XFS_BUF_HOLD(x)		((void)0)
#define XFS_BUF_UNHOLD(x)	((void)0)
#define XFS_BUF_ISHOLD(x)	BUF_REFCNT(x)

#define XFS_BUF_READ(x)		((x)->b_iocmd = BIO_READ)
#define XFS_BUF_UNREAD(x)	((x)->b_iocmd = 0)
#define XFS_BUF_ISREAD(x)	((x)->b_iocmd == BIO_READ)

#define XFS_BUF_WRITE(x)	((x)->b_iocmd = BIO_WRITE)
#define XFS_BUF_UNWRITE(x)	((x)->b_iocmd = 0)
#define XFS_BUF_ISWRITE(x)	((x)->b_iocmd == BIO_WRITE)

#define XFS_BUF_ISUNINITIAL(x)	(0)
#define XFS_BUF_UNUNINITIAL(x)	(0)

#define XFS_BUF_IODONE_FUNC(x)		(x)->b_iodone
#define XFS_BUF_SET_IODONE_FUNC(x, f)	(x)->b_iodone = (f)
#define XFS_BUF_CLR_IODONE_FUNC(x)	(x)->b_iodone = NULL

#define XFS_BUF_SET_BDSTRAT_FUNC(x, f)	do { if(f != NULL) {} } while(0)
#define XFS_BUF_CLR_BDSTRAT_FUNC(x)	((void)0)

#define XFS_BUF_BP_ISMAPPED(bp)		(1)

#define XFS_BUF_FSPRIVATE(buf, type) \
			((type)(buf)->b_fsprivate1)
#define XFS_BUF_SET_FSPRIVATE(buf, value) \
			(buf)->b_fsprivate1 = (void *)(value)
#define XFS_BUF_FSPRIVATE2(buf, type) \
			((type)(buf)->b_fsprivate2)
#define XFS_BUF_SET_FSPRIVATE2(buf, value) \
			(buf)->b_fsprivate2 = (void *)(value)
#define XFS_BUF_FSPRIVATE3(buf, type) \
			((type)(buf)->b_fsprivate3)
#define XFS_BUF_SET_FSPRIVATE3(buf, value) \
			(buf)->b_fsprivate3 = (void *)(value)
#define XFS_BUF_SET_START(buf) \
		printf("XFS_BUF_SET_START: %s:%d\n", __FILE__, __LINE__)

#define XFS_BUF_SET_BRELSE_FUNC(buf, value) \
	do { \
		printf("XFS_BUF_SET_BRELSE_FUNC: %s:%d\n", \
		        __FILE__, __LINE__); \
		if (value != NULL ) {} \
	} while(0)

#define XFS_BUF_PTR(bp)		(xfs_caddr_t)((bp)->b_data)

static __inline xfs_caddr_t
xfs_buf_offset(xfs_buf_t *bp, size_t offset)
{
	return XFS_BUF_PTR(bp) + offset;
}

#define XFS_BUF_SET_PTR(bp, val, count)	\
				do { \
					(bp)->b_data = (val); \
					(bp)->b_bcount = (count); \
				} while(0)

#define XFS_BUF_ADDR(bp)	((bp)->b_blkno)
#define XFS_BUF_SET_ADDR(bp, blk) \
				((bp)->b_blkno = blk)
#define XFS_BUF_OFFSET(bp)	((bp)->b_offset)
#define XFS_BUF_SET_OFFSET(bp, off) \
				((bp)->b_offset = off)
#define XFS_BUF_COUNT(bp)	((bp)->b_bcount)
#define XFS_BUF_SET_COUNT(bp, cnt) \
				((bp)->b_bcount = cnt)
#define XFS_BUF_SIZE(bp)	((bp)->b_bufsize)
#define XFS_BUF_SET_SIZE(bp, cnt) \
			        ((bp)->b_bufsize = cnt)
#define	XFS_BUF_SET_VTYPE_REF(bp, type, ref)
#define	XFS_BUF_SET_VTYPE(bp, type)
#define	XFS_BUF_SET_REF(bp, ref)

#define	XFS_BUF_VALUSEMA(bp)	(BUF_REFCNT(bp)? 0 : 1)
#define	XFS_BUF_CPSEMA(bp) \
	(BUF_LOCK(bp, LK_EXCLUSIVE|LK_CANRECURSE | LK_SLEEPFAIL, NULL) == 0)

#define	XFS_BUF_PSEMA(bp,x)	BUF_LOCK(bp, LK_EXCLUSIVE|LK_CANRECURSE, NULL)
#define	XFS_BUF_VSEMA(bp)	BUF_UNLOCK(bp)

#define	XFS_BUF_V_IODONESEMA(bp) bdone(bp)

/* setup the buffer target from a buftarg structure */
#define XFS_BUF_SET_TARGET(bp, target) \
	xfs_buf_set_target(bp, target)

void xfs_buf_set_target(xfs_buf_t *, xfs_buftarg_t *);
xfs_buftarg_t *xfs_buf_get_target(xfs_buf_t *);

/* return the dev_t being used */
#define XFS_BUF_TARGET(bp)	xfs_buf_get_target(bp)
#define	XFS_BUFTARG_NAME(targp)	devtoname((targp)->dev)

#define XFS_BUF_SET_VTYPE_REF(bp, type, ref)
#define XFS_BUF_SET_VTYPE(bp, type)
#define XFS_BUF_SET_REF(bp, ref)

#define XFS_BUF_ISPINNED(bp)	xfs_buf_ispin(bp)

xfs_buf_t *
xfs_buf_read_flags(xfs_buftarg_t *, xfs_daddr_t, size_t, int);

#define xfs_buf_read(target, blkno, len, flags) \
                xfs_buf_read_flags(target, blkno, len, \
                       XFS_BUF_LOCK | XFS_BUF_MAPPED)

xfs_buf_t *
xfs_buf_get_flags(xfs_buftarg_t *, xfs_daddr_t, size_t, int);

#define xfs_buf_get(target, blkno, len, flags) \
                xfs_buf_get_flags(target, blkno, len, \
                       XFS_BUF_LOCK | XFS_BUF_MAPPED)

/* the return value is never used ... why does linux define this functions this way? */
static inline int xfs_bawrite(void *mp, xfs_buf_t *bp)
{
	/* Ditto for xfs_bawrite
	bp->b_fspriv3 = mp;
	bp->b_strat = xfs_bdstrat_cb;
	xfs_buf_delwri_dequeue(bp);
	return xfs_buf_iostart(bp, XBF_WRITE | XBF_ASYNC | _XBF_RUN_QUEUES);
	*/
	bawrite(bp);
	return 0;
}

static inline int xfs_bdwrite(void *mp, xfs_buf_t *bp)
{
  /* this is for io shutdown checking need to do this at some point RMC */
  /* probably should just change xfs to call a buf write function */
#if 0 /* RMC */
	bp->b_strat = xfs_bdstrat_cb;
	bp->b_fspriv3 = mp;
	return xfs_buf_iostart(bp, XBF_DELWRI | XBF_ASYNC);
#endif
	bdwrite(bp);
	return 0;
}

#define xfs_bpin(bp)		xfs_buf_pin(bp)
#define xfs_bunpin(bp)		xfs_buf_unpin(bp)

#define xfs_buf_relse(bp)            brelse(bp)
#define xfs_bp_mapin(bp)             bp_mapin(bp)
#define xfs_xfsd_list_evict(x)       _xfs_xfsd_list_evict(x)
#define xfs_buftrace(x,y)            CTR2(KTR_BUF, "%s bp %p flags %X", bp, bp->b_flags)
#define xfs_biodone(bp)              bufdone_finish(bp)

#define xfs_incore(xfs_buftarg,blkno,len,lockit)  \
			  incore(&xfs_buftarg->specvp->v_bufobj, blkno);

#define xfs_biomove(bp, off, len, data, rw) \
	xfs_buf_iomove((bp), (off), (len), (data),			\
		       ((rw) == XFS_B_WRITE) ? XBRW_WRITE : XBRW_READ)

#define xfs_biozero(bp, off, len) \
	xfs_buf_iomove((bp), (off), (len), NULL, XBRW_ZERO)

/* already a function xfs_bwrite... fix this */
#define XFS_bdwrite(bp)			bdwrite(bp)
#define xfs_iowait(bp)			bufwait(bp)

#define XFS_bdstrat(bp)			xfs_buf_iorequest(bp)

#define xfs_baread(target, rablkno, ralen)  \
	xfs_buf_readahead((target), (rablkno), (ralen), XBF_DONT_BLOCK)

struct xfs_mount;

int XFS_bwrite(xfs_buf_t *bp);
xfs_buf_t* xfs_buf_get_empty(size_t, xfs_buftarg_t *targ);
xfs_buf_t* xfs_buf_get_noaddr(size_t, xfs_buftarg_t *targ);
void xfs_buf_free(xfs_buf_t *);

extern void xfs_bwait_unpin(xfs_buf_t *bp);
extern xfs_buftarg_t *xfs_alloc_buftarg(struct vnode *, int);
extern void xfs_free_buftarg(xfs_buftarg_t *, int);
extern void xfs_wait_buftarg(xfs_buftarg_t *);
extern int xfs_setsize_buftarg(xfs_buftarg_t *, unsigned int, unsigned int);
extern unsigned int xfs_getsize_buftarg(struct xfs_buftarg *);
extern int xfs_flush_buftarg(xfs_buftarg_t *, int);

#define xfs_binval(buftarg)		xfs_flush_buftarg(buftarg, 1)
#define XFS_bflush(buftarg)		xfs_flush_buftarg(buftarg, 1)

#endif
