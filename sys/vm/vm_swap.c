/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *
 *	@(#)vm_swap.c	8.5 (Berkeley) 2/17/94
 * $FreeBSD$
 */

#include "opt_swap.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/dmap.h>		/* XXX */
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/blist.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/conf.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/swap_pager.h>

/*
 * "sw" is a fake device implemented
 * in vm_swap.c and used only internally to get to swstrategy.
 * It cannot be provided to the users, because the
 * swstrategy routine munches the b_dev and b_blkno entries
 * before calling the appropriate driver.  This would horribly
 * confuse, e.g. the hashing routines. Instead, /dev/drum is
 * provided as a character (raw) device.
 */

static	d_strategy_t    swstrategy;

#define CDEV_MAJOR 4
#define BDEV_MAJOR 26

static struct cdevsw sw_cdevsw = {
	/* open */	nullopen,
	/* close */	nullclose,
	/* read */	physread,
	/* write */	physwrite,
	/* ioctl */	noioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	swstrategy,
	/* name */	"sw",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_DISK,
	/* bmaj */	BDEV_MAJOR
};


/*
 * Indirect driver for multi-controller paging.
 */

#ifndef NSWAPDEV
#define NSWAPDEV	4
#endif
static struct swdevt should_be_malloced[NSWAPDEV];
static struct swdevt *swdevt = should_be_malloced;
struct vnode *swapdev_vp;
static int nswap;		/* first block after the interleaved devs */
static int nswdev = NSWAPDEV;
int vm_swap_size;

/*
 *	swstrategy:
 *
 *	Perform swap strategy interleave device selection
 *
 *	The bp is expected to be locked and *not* B_DONE on call.
 */

static void
swstrategy(bp)
	register struct buf *bp;
{
	int s, sz, off, seg, index;
	register struct swdevt *sp;
	struct vnode *vp;

	sz = howmany(bp->b_bcount, PAGE_SIZE);
	/*
	 * Convert interleaved swap into per-device swap.  Note that
	 * the block size is left in PAGE_SIZE'd chunks (for the newswap)
	 * here.
	 */

	if (nswdev > 1) {
		off = bp->b_blkno % dmmax;
		if (off + sz > dmmax) {
			bp->b_error = EINVAL;
			bp->b_flags |= B_ERROR;
			biodone(bp);
			return;
		}
		seg = bp->b_blkno / dmmax;
		index = seg % nswdev;
		seg /= nswdev;
		bp->b_blkno = seg * dmmax + off;
	} else {
		index = 0;
	}
	sp = &swdevt[index];
	if (bp->b_blkno + sz > sp->sw_nblks) {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}
	bp->b_dev = sp->sw_device;
	if (sp->sw_vp == NULL) {
		bp->b_error = ENODEV;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}

	/*
	 * Convert from PAGE_SIZE'd to DEV_BSIZE'd chunks for the actual I/O
	 */
	bp->b_blkno = ctodb(bp->b_blkno);

	vhold(sp->sw_vp);
	s = splvm();
	if ((bp->b_flags & B_READ) == 0) {
		vp = bp->b_vp;
		if (vp) {
			vp->v_numoutput--;
			if ((vp->v_flag & VBWAIT) && vp->v_numoutput <= 0) {
				vp->v_flag &= ~VBWAIT;
				wakeup(&vp->v_numoutput);
			}
		}
		sp->sw_vp->v_numoutput++;
	}
	pbreassignbuf(bp, sp->sw_vp);
	splx(s);
	VOP_STRATEGY(bp->b_vp, bp);
}

/*
 * System call swapon(name) enables swapping on device name,
 * which must be in the swdevsw.  Return EBUSY
 * if already swapping on this device.
 */
#ifndef _SYS_SYSPROTO_H_
struct swapon_args {
	char *name;
};
#endif

/* ARGSUSED */
int
swapon(p, uap)
	struct proc *p;
	struct swapon_args *uap;
{
	register struct vnode *vp;
	dev_t dev;
	struct nameidata nd;
	int error;
	static int once;

	if (!once) {
		cdevsw_add(&sw_cdevsw);
		make_dev(&sw_cdevsw, 0, UID_ROOT, GID_KMEM, 0640, "drum");
		once++;
	}
	error = suser(p);
	if (error)
		return (error);

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->name, p);
	error = namei(&nd);
	if (error)
		return (error);

	vp = nd.ni_vp;

	switch (vp->v_type) {
	case VBLK:
		dev = vp->v_rdev;
		if (devsw(dev) == NULL) {
			error = ENXIO;
			break;
		}
		error = swaponvp(p, vp, dev, 0);
		break;
	case VCHR:
		/*
		 * For now, we disallow swapping to regular files.
		 * It requires logical->physcal block translation
		 * support in the swap pager before it will work.
		 */
		error = ENOTBLK;
		break;
#if 0
		error = VOP_GETATTR(vp, &attr, p->p_ucred, p);
		if (!error)
			error = swaponvp(p, vp, NODEV, attr.va_size / DEV_BSIZE);
		break;
#endif
	default:
		error = EINVAL;
		break;
	}

	if (error)
		vrele(vp);

	return (error);
}

/*
 * Swfree(index) frees the index'th portion of the swap map.
 * Each of the nswdev devices provides 1/nswdev'th of the swap
 * space, which is laid out with blocks of dmmax pages circularly
 * among the devices.
 *
 * The new swap code uses page-sized blocks.  The old swap code used
 * DEV_BSIZE'd chunks.
 *
 * XXX locking when multiple swapon's run in parallel
 */
int
swaponvp(p, vp, dev, nblks)
	struct proc *p;
	struct vnode *vp;
	dev_t dev;
	u_long nblks;
{
	int index;
	register struct swdevt *sp;
	register swblk_t vsbase;
	register long blk;
	swblk_t dvbase;
	int error;

	ASSERT_VOP_UNLOCKED(vp, "swaponvp");
	for (sp = swdevt, index = 0 ; index < nswdev; index++, sp++) {
		if (sp->sw_vp == vp)
			return EBUSY;
		if (!sp->sw_vp)
			goto found;

	}
	return EINVAL;
    found:
	(void) vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	error = VOP_OPEN(vp, FREAD | FWRITE, p->p_ucred, p);
	(void) VOP_UNLOCK(vp, 0, p);
	if (error)
		return (error);

	if (nblks == 0 && dev != NODEV && (devsw(dev)->d_psize == 0 ||
	    (nblks = (*devsw(dev)->d_psize) (dev)) == -1)) {
		(void) VOP_CLOSE(vp, FREAD | FWRITE, p->p_ucred, p);
		return (ENXIO);
	}
	if (nblks == 0) {
		(void) VOP_CLOSE(vp, FREAD | FWRITE, p->p_ucred, p);
		return (ENXIO);
	}
	/*
	 * nblks is in DEV_BSIZE'd chunks, convert to PAGE_SIZE'd chunks.
	 * First chop nblks off to page-align it, then convert.
	 * 
	 * sw->sw_nblks is in page-sized chunks now too.
	 */
	nblks &= ~(ctodb(1) - 1);
	nblks = dbtoc(nblks);

	sp->sw_vp = vp;
	sp->sw_dev = dev2budev(dev);
	sp->sw_device = dev;
	sp->sw_flags |= SW_FREED;
	sp->sw_nblks = nblks;

	/*
	 * nblks, nswap, and dmmax are PAGE_SIZE'd parameters now, not
	 * DEV_BSIZE'd. 
	 */

	if (nblks * nswdev > nswap)
		nswap = (nblks+1) * nswdev;

	if (swapblist == NULL)
		swapblist = blist_create(nswap);
	else
		blist_resize(&swapblist, nswap, 0);

	for (dvbase = dmmax; dvbase < nblks; dvbase += dmmax) {
		blk = min(nblks - dvbase, dmmax);
		vsbase = index * dmmax + dvbase * nswdev;
		blist_free(swapblist, vsbase, blk);
		vm_swap_size += blk;
	}

	if (!swapdev_vp) {
		struct vnode *vp1;
		struct vnode *nvp;

		error = getnewvnode(VT_NON, (struct mount *) 0,
		    spec_vnodeop_p, &nvp);
		if (error)
			panic("Cannot get vnode for swapdev");
		vp1 = nvp;
		vp1->v_type = VBLK;
		addaliasu(vp1, makeudev(BDEV_MAJOR, 0));
		swapdev_vp = vp1;
	}
	return (0);
}
