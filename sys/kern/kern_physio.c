/*
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    John S. Dyson.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 * $Id: kern_physio.c,v 1.22 1997/09/02 20:05:40 bde Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>

static void	physwakeup __P((struct buf *bp));
static struct buf * phygetvpbuf(dev_t dev, int resid);

int
physio(strategy, bp, dev, rw, minp, uio)
	d_strategy_t *strategy;
	struct buf *bp;
	dev_t dev;
	int rw;
	u_int (*minp) __P((struct buf *bp));
	struct uio *uio;
{
	int i;
	int bufflags = rw?B_READ:0;
	int error;
	int spl;
	caddr_t sa;
	int bp_alloc = (bp == 0);
	struct buf *bpa;

/*
 * keep the process from being swapped
 */
	curproc->p_flag |= P_PHYSIO;

	/* create and build a buffer header for a transfer */
	bpa = (struct buf *)phygetvpbuf(dev, uio->uio_resid);
	if (!bp_alloc) {
		spl = splbio();
		while (bp->b_flags & B_BUSY) {
			bp->b_flags |= B_WANTED;
			tsleep((caddr_t)bp, PRIBIO, "physbw", 0);
		}
		bp->b_flags |= B_BUSY;
		splx(spl);
	} else {
		bp = bpa;
	}

	/*
	 * get a copy of the kva from the physical buffer
	 */
	sa = bpa->b_data;
	bp->b_proc = curproc;
	error = bp->b_error = 0;

	for(i=0;i<uio->uio_iovcnt;i++) {
		while( uio->uio_iov[i].iov_len) {

			bp->b_dev = dev;
			bp->b_bcount = uio->uio_iov[i].iov_len;
			bp->b_flags = B_BUSY | B_PHYS | B_CALL | bufflags;
			bp->b_iodone = physwakeup;
			bp->b_data = uio->uio_iov[i].iov_base;
			bp->b_bcount = minp( bp);
			if( minp != minphys)
				bp->b_bcount = minphys( bp);
			bp->b_bufsize = bp->b_bcount;
			/*
			 * pass in the kva from the physical buffer
			 * for the temporary kernel mapping.
			 */
			bp->b_saveaddr = sa;
			bp->b_blkno = btodb(uio->uio_offset);


			if (uio->uio_segflg == UIO_USERSPACE) {
				if (rw && !useracc(bp->b_data, bp->b_bufsize, B_WRITE)) {
					error = EFAULT;
					goto doerror;
				}
				if (!rw && !useracc(bp->b_data, bp->b_bufsize, B_READ)) {
					error = EFAULT;
					goto doerror;
				}

				/* bring buffer into kernel space */
				vmapbuf(bp);
			}

			/* perform transfer */
			(*strategy)(bp);

			spl = splbio();
			while ((bp->b_flags & B_DONE) == 0)
#if defined(NO_SCHEDULE_MODS)
				tsleep((caddr_t)bp, PRIBIO, "physstr", 0);
#else
				tsleep((caddr_t)bp, curproc->p_usrpri, "physstr", 0);
#endif
			splx(spl);

			/* release mapping into kernel space */
			if (uio->uio_segflg == UIO_USERSPACE)
				vunmapbuf(bp);

			/*
			 * update the uio data
			 */
			{
				int iolen = bp->b_bcount - bp->b_resid;

				if (iolen == 0 && !(bp->b_flags & B_ERROR))
					goto doerror;	/* EOF */
				uio->uio_iov[i].iov_len -= iolen;
				uio->uio_iov[i].iov_base += iolen;
				uio->uio_resid -= iolen;
				uio->uio_offset += iolen;
			}

			/*
			 * check for an error
			 */
			if( bp->b_flags & B_ERROR) {
				error = bp->b_error;
				goto doerror;
			}
		}
	}


doerror:
	relpbuf(bpa);
	if (!bp_alloc) {
		bp->b_flags &= ~(B_BUSY|B_PHYS);
		if( bp->b_flags & B_WANTED) {
			bp->b_flags &= ~B_WANTED;
			wakeup((caddr_t)bp);
		}
	}
/*
 * allow the process to be swapped
 */
	curproc->p_flag &= ~P_PHYSIO;

	return (error);
}

u_int
minphys(bp)
	struct buf *bp;
{
	u_int maxphys = DFLTPHYS;
	struct bdevsw *bdsw;
	int offset;

	bdsw = cdevsw[major(bp->b_dev)]->d_bdev;

	if (bdsw && bdsw->d_maxio) {
		maxphys = bdsw->d_maxio;
	}
	if (bp->b_kvasize < maxphys)
		maxphys = bp->b_kvasize;

	if(((vm_offset_t) bp->b_data) & PAGE_MASK) {
		maxphys -= PAGE_SIZE;
	}

	if( bp->b_bcount > maxphys) {
		bp->b_bcount = maxphys;
	}

	return bp->b_bcount;
}

struct buf *
phygetvpbuf(dev_t dev, int resid)
{
	struct bdevsw *bdsw;
	int maxio;

	bdsw = cdevsw[major(dev)]->d_bdev;
	if (bdsw == NULL)
		return getpbuf();

	maxio = bdsw->d_maxio;
	if (resid > maxio)
		resid = maxio;

	return getpbuf();
}

int
rawread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	return (physio(cdevsw[major(dev)]->d_strategy, (struct buf *)NULL,
	    dev, 1, minphys, uio));
}

int
rawwrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	return (physio(cdevsw[major(dev)]->d_strategy, (struct buf *)NULL,
	    dev, 0, minphys, uio));
}

static void
physwakeup(bp)
	struct buf *bp;
{
	wakeup((caddr_t) bp);
	bp->b_flags &= ~B_CALL;
}
