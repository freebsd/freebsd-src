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
 * $Id: vm_swap.c,v 1.47 1997/09/14 03:19:42 peter Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/buf.h>
#include <sys/conf.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/dmap.h>		/* XXX */
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/rlist.h>
#include <sys/kernel.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/swap_pager.h>

#include <miscfs/specfs/specdev.h>

/*
 * "sw" is a fake device implemented
 * in vm_swap.c and used only internally to get to swstrategy.
 * It cannot be provided to the users, because the
 * swstrategy routine munches the b_dev and b_blkno entries
 * before calling the appropriate driver.  This would horribly
 * confuse, e.g. the hashing routines. Instead, /dev/drum is
 * provided as a character (raw) device.
 */

static void swstrategy __P((struct buf *));

#define CDEV_MAJOR 4
#define BDEV_MAJOR 1
extern struct cdevsw sw_cdevsw ;

static struct bdevsw sw_bdevsw = 
	{ noopen,	noclose,	swstrategy,	noioc,		/*1*/
	  nodump,	nopsize,	0,	"sw",	&sw_cdevsw,	-1 };

static struct cdevsw sw_cdevsw = 
	{ nullopen,	nullclose,	rawread,	rawwrite,	/*4*/
	  noioc,	nostop,		noreset,	nodevtotty,/* swap */
	  seltrue,	nommap,		swstrategy,	"sw",
	  &sw_bdevsw,	-1 };

static dev_t	swapdev = makedev(BDEV_MAJOR, 0);

/*
 * Indirect driver for multi-controller paging.
 */

#ifndef NSWAPDEV
#define NSWAPDEV	4
#endif
static struct swdevt should_be_malloced[NSWAPDEV];
static struct swdevt *swdevt = should_be_malloced;
struct vnode *swapdev_vp;
/* XXX swapinfo(8) needs this one I belive */
int nswap;			/* first block after the interleaved devs */
static int nswdev = NSWAPDEV;
int vm_swap_size;

static void
swstrategy(bp)
	register struct buf *bp;
{
	int sz, off, seg, index;
	register struct swdevt *sp;
	struct vnode *vp;

	sz = howmany(bp->b_bcount, DEV_BSIZE);
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
	} else
		index = 0;
	sp = &swdevt[index];
	if (bp->b_blkno + sz > sp->sw_nblks) {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}
	bp->b_dev = sp->sw_dev;
	if (sp->sw_vp == NULL) {
		bp->b_error = ENODEV;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}
	vhold(sp->sw_vp);
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
	if (bp->b_vp != NULL)
		pbrelvp(bp);
	bp->b_vp = sp->sw_vp;
	VOP_STRATEGY(bp);
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

	error = suser(p->p_ucred, &p->p_acflag);
	if (error)
		return (error);

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->name, p);
	error = namei(&nd);
	if (error)
		return (error);

	vp = nd.ni_vp;

	switch (vp->v_type) {
	case VBLK:
		dev = (dev_t) vp->v_rdev;
		if (major(dev) >= nblkdev) {
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

	for (sp = swdevt, index = 0 ; index < nswdev; index++, sp++) {
		if (sp->sw_vp == vp)
			return EBUSY;
		if (!sp->sw_vp)
			goto found;

	}
	return EINVAL;
    found:
	if (dev != NODEV && (major(dev) >= nblkdev))
		return (ENXIO);

	error = VOP_OPEN(vp, FREAD | FWRITE, p->p_ucred, p);
	if (error)
		return (error);

	if (nblks == 0 && (bdevsw[major(dev)]->d_psize == 0 ||
	    (nblks = (*bdevsw[major(dev)]->d_psize) (dev)) == -1)) {
		(void) VOP_CLOSE(vp, FREAD | FWRITE, p->p_ucred, p);
		return (ENXIO);
	}
	if (nblks == 0) {
		(void) VOP_CLOSE(vp, FREAD | FWRITE, p->p_ucred, p);
		return (ENXIO);
	}
	sp->sw_vp = vp;
	sp->sw_dev = dev;
	sp->sw_flags |= SW_FREED;
	sp->sw_nblks = nblks;

	if (nblks * nswdev > nswap)
		nswap = (nblks+1) * nswdev;

	for (dvbase = dmmax; dvbase < nblks; dvbase += dmmax) {
		blk = min(nblks - dvbase,dmmax);
		vsbase = index * dmmax + dvbase * nswdev;
		rlist_free(&swaplist, vsbase, vsbase + blk - 1);
		vm_swap_size += blk;
	}

	if (!swapdev_vp) {
		struct vnode *vp;
		struct vnode *nvp;

		error = getnewvnode(VT_NON, (struct mount *) 0,
		    spec_vnodeop_p, &nvp);
		if (error)
			panic("Cannot get vnode for swapdev");
		vp = nvp;
		vp->v_type = VBLK;
		if ((nvp = checkalias(vp, swapdev,
		    (struct mount *) 0))) {
			vput(vp);
			vp = nvp;
		}
		swapdev_vp = vp;
	}
	return (0);
}

static sw_devsw_installed = 0;
#ifdef DEVFS
static void *drum_devfs_token;
#endif

static void 	sw_drvinit(void *unused)
{
	dev_t dev;

	if( ! sw_devsw_installed ) {
		dev = makedev(CDEV_MAJOR,0);
		cdevsw_add(&dev,&sw_cdevsw,NULL);
		dev = makedev(BDEV_MAJOR,0);
		bdevsw_add(&dev,&sw_bdevsw,NULL);
		sw_devsw_installed = 1;
#ifdef DEVFS
		drum_devfs_token = devfs_add_devswf(&sw_cdevsw, 0, DV_CHR,
						    UID_ROOT, GID_KMEM, 0640,
						    "drum");
#endif
    	}
}

SYSINIT(swdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,sw_drvinit,NULL)

