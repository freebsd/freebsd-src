/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
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
 *
 *	from: @(#)vm_swap.c	7.18 (Berkeley) 5/6/91
 *	$Id: vm_swap.c,v 1.4 1993/10/16 16:20:56 rgrimes Exp $
 */

#include "param.h"
#include "systm.h"
#include "buf.h"
#include "conf.h"
#include "proc.h"
#include "namei.h"
#include "dmap.h"		/* XXX */
#include "vnode.h"
#include "specdev.h"
#include "file.h"
#include "rlist.h"

/*
 * Indirect driver for multi-controller paging.
 */

int	nswap, nswdev;

/*
 * Set up swap devices.
 * Initialize linked list of free swap
 * headers. These do not actually point
 * to buffers, but rather to pages that
 * are being swapped in and out.
 */
swapinit()
{
	register int i;
	register struct buf *sp = swbuf;
	struct swdevt *swp;
	int error;

	/*
	 * Count swap devices, and adjust total swap space available.
	 * Some of this space will not be available until a swapon()
	 * system is issued, usually when the system goes multi-user.
	 */
	nswdev = 0;
	nswap = 0;
	for (swp = swdevt; swp->sw_dev; swp++) {
		nswdev++;
		if (swp->sw_nblks > nswap)
			nswap = swp->sw_nblks;
	}
	if (nswdev == 0)
		panic("swapinit");
	if (nswdev > 1)
		nswap = ((nswap + dmmax - 1) / dmmax) * dmmax;
	nswap *= nswdev;
	if (bdevvp(swdevt[0].sw_dev, &swdevt[0].sw_vp))
		panic("swapvp");
	if (error = swfree(&proc0, 0)) {
		printf("\nwarning: no swap space present (yet)\n");
		/* printf("(swfree (..., 0) -> %d)\n", error);	/* XXX */
		/*panic("swapinit swfree 0");*/
	}

	/*
	 * Now set up swap buffer headers.
	 */
	bswlist.av_forw = sp;
	for (i = 0; i < nswbuf - 1; i++, sp++)
		sp->av_forw = sp + 1;
	sp->av_forw = NULL;
}

swstrategy(bp)
	register struct buf *bp;
{
	int sz, off, seg, index;
	register struct swdevt *sp;
	struct vnode *vp;

#ifdef GENERIC
	/*
	 * A mini-root gets copied into the front of the swap
	 * and we run over top of the swap area just long
	 * enough for us to do a mkfs and restor of the real
	 * root (sure beats rewriting standalone restor).
	 */
#define	MINIROOTSIZE	4096
	if (rootdev == dumpdev)
		bp->b_blkno += MINIROOTSIZE;
#endif
	sz = howmany(bp->b_bcount, DEV_BSIZE);
	if (bp->b_blkno + sz > nswap) {
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}
	if (nswdev > 1) {
		off = bp->b_blkno % dmmax;
		if (off+sz > dmmax) {
			bp->b_flags |= B_ERROR;
			biodone(bp);
			return;
		}
		seg = bp->b_blkno / dmmax;
		index = seg % nswdev;
		seg /= nswdev;
		bp->b_blkno = seg*dmmax + off;
	} else
		index = 0;
	sp = &swdevt[index];
	if ((bp->b_dev = sp->sw_dev) == 0)
		panic("swstrategy");
	if (sp->sw_vp == NULL) {
		bp->b_error |= B_ERROR;
		biodone(bp);
		return;
	}
	VHOLD(sp->sw_vp);
	if ((bp->b_flags & B_READ) == 0) {
		if (vp = bp->b_vp) {
			vp->v_numoutput--;
			if ((vp->v_flag & VBWAIT) && vp->v_numoutput <= 0) {
				vp->v_flag &= ~VBWAIT;
				wakeup((caddr_t)&vp->v_numoutput);
			}
		}
		sp->sw_vp->v_numoutput++;
	}
	if (bp->b_vp != NULL)
		brelvp(bp);
	bp->b_vp = sp->sw_vp;
	VOP_STRATEGY(bp);
}

/*
 * System call swapon(name) enables swapping on device name,
 * which must be in the swdevsw.  Return EBUSY
 * if already swapping on this device.
 */

struct swapon_args {
	char	*name;
};

/* ARGSUSED */
swapon(p, uap, retval)
	struct proc *p;
	struct swapon_args *uap;
	int *retval;
{
	register struct vnode *vp;
	register struct swdevt *sp;
	register struct nameidata *ndp;
	dev_t dev;
	int error;
	struct nameidata nd;

	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | FOLLOW;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->name;
	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
	if (vp->v_type != VBLK) {
		vrele(vp);
		return (ENOTBLK);
	}
	dev = (dev_t)vp->v_rdev;
	if (major(dev) >= nblkdev) {
		vrele(vp);
		return (ENXIO);
	}
	for (sp = &swdevt[0]; sp->sw_dev; sp++)
		if (sp->sw_dev == dev) {
			if (sp->sw_freed) {
				vrele(vp);
				return (EBUSY);
			}
			sp->sw_vp = vp;
			if (error = swfree(p, sp - swdevt)) {
				printf("swap failed! (unchanged)\n");
				vrele(vp);
				return (error);
			}
			return (0);
		}
	vrele(vp);
	return (EINVAL);
}

/*
 * Swfree(index) frees the index'th portion of the swap map.
 * Each of the nswdev devices provides 1/nswdev'th of the swap
 * space, which is laid out with blocks of dmmax pages circularly
 * among the devices.
 */
swfree(p, index)
	struct proc *p;
	int index;
{
	register struct swdevt *sp;
	register swblk_t vsbase;
	register long blk;
	struct vnode *vp;
	register swblk_t dvbase;
	register int nblks;
	int error;

	sp = &swdevt[index];
	nblks = sp->sw_nblks;
	if (nblks <= 0)
		return(ENXIO);
	vp = sp->sw_vp;
	if (error = VOP_OPEN(vp, FREAD|FWRITE, p->p_ucred, p))
		return (error);
	sp->sw_freed = 1;

	/*printf("%d blocks from device %d/%d ",
		sp->sw_nblks, major(sp->sw_dev), minor(sp->sw_dev));*/
	for (dvbase = 0; dvbase < nblks; dvbase += dmmax) {
		blk = nblks - dvbase;
		if ((vsbase = index*dmmax + dvbase*nswdev) >= nswap)
			panic("swfree");
		if (blk > dmmax)
			blk = dmmax;
		rlist_free(&swapmap, vsbase, vsbase + blk - 1); 
	}
	return (0);
}
