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

#include "opt_mac.h"
#include "opt_swap.h"
#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/blist.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/mac.h>
#include <sys/mount.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/vm_pageout.h>
#include <vm/swap_pager.h>
#include <vm/uma.h>

/*
 * Indirect driver for multi-controller paging.
 */

typedef	int32_t	swblk_t;	/* swap offset */
extern int dmmax;

#ifndef NSWAPDEV
#define NSWAPDEV	4
#endif
static struct swdevt should_be_malloced[NSWAPDEV];
struct swdevt *swdevt = should_be_malloced;
static int nswap;		/* first block after the interleaved devs */
int nswdev = NSWAPDEV;
int vm_swap_size;
static int swdev_syscall_active = 0; /* serialize swap(on|off) */


static int swapdev_strategy(struct vop_strategy_args *ap);
struct vnode *swapdev_vp;

/*
 *	swapdev_strategy:
 *
 *	VOP_STRATEGY() for swapdev_vp.
 *	Perform swap strategy interleave device selection.
 *
 *	The bp is expected to be locked and *not* B_DONE on call.
 */
static int
swapdev_strategy(ap)
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap;
{
	int s, sz, off, seg, index;
	struct swdevt *sp;
	struct vnode *vp;
	struct buf *bp;

	bp = ap->a_bp;
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
			bp->b_ioflags |= BIO_ERROR;
			bufdone(bp);
			return 0;
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
		bp->b_ioflags |= BIO_ERROR;
		bufdone(bp);
		return 0;
	}
	bp->b_dev = sp->sw_device;
	if (sp->sw_vp == NULL) {
		bp->b_error = ENODEV;
		bp->b_ioflags |= BIO_ERROR;
		bufdone(bp);
		return 0;
	}

	/*
	 * Convert from PAGE_SIZE'd to DEV_BSIZE'd chunks for the actual I/O
	 */
	bp->b_blkno = ctodb(bp->b_blkno);

	vhold(sp->sw_vp);
	s = splvm();
	if (bp->b_iocmd == BIO_WRITE) {
		vp = bp->b_vp;
		if (vp) {
			VI_LOCK(vp);
			vp->v_numoutput--;
			if ((vp->v_iflag & VI_BWAIT) && vp->v_numoutput <= 0) {
				vp->v_iflag &= ~VI_BWAIT;
				wakeup(&vp->v_numoutput);
			}
			VI_UNLOCK(vp);
		}
		VI_LOCK(sp->sw_vp);
		sp->sw_vp->v_numoutput++;
		VI_UNLOCK(sp->sw_vp);
	}
	bp->b_vp = sp->sw_vp;
	splx(s);
	VOP_STRATEGY(bp->b_vp, bp);
	return 0;
}

/*
 * Create a special vnode op vector for swapdev_vp - we only use
 * VOP_STRATEGY() and reclaim; everything else returns an error.
 */
vop_t **swapdev_vnodeop_p;
static struct vnodeopv_entry_desc swapdev_vnodeop_entries[] = {  
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_reclaim_desc,		(vop_t *) vop_null },
	{ &vop_strategy_desc,		(vop_t *) swapdev_strategy },
	{ NULL, NULL }
};
static struct vnodeopv_desc swapdev_vnodeop_opv_desc =
	{ &swapdev_vnodeop_p, swapdev_vnodeop_entries };

VNODEOP_SET(swapdev_vnodeop_opv_desc);

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

/* 
 * MPSAFE
 */
/* ARGSUSED */
int
swapon(td, uap)
	struct thread *td;
	struct swapon_args *uap;
{
	struct vattr attr;
	struct vnode *vp;
	struct nameidata nd;
	int error;

	mtx_lock(&Giant);
	error = suser(td);
	if (error)
		goto done2;

	while (swdev_syscall_active)
	    tsleep(&swdev_syscall_active, PUSER - 1, "swpon", 0);
	swdev_syscall_active = 1;

	/*
	 * Swap metadata may not fit in the KVM if we have physical
	 * memory of >1GB.
	 */
	if (swap_zone == NULL) {
		error = ENOMEM;
		goto done;
	}

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->name, td);
	error = namei(&nd);
	if (error)
		goto done;

	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;

	if (vn_isdisk(vp, &error))
		error = swaponvp(td, vp, vp->v_rdev, 0);
	else if (vp->v_type == VREG &&
	    (vp->v_mount->mnt_vfc->vfc_flags & VFCF_NETWORK) != 0 &&
	    (error = VOP_GETATTR(vp, &attr, td->td_ucred, td)) == 0) {
		/*
		 * Allow direct swapping to NFS regular files in the same
		 * way that nfs_mountroot() sets up diskless swapping.
		 */
		error = swaponvp(td, vp, NODEV, attr.va_size / DEV_BSIZE);
	}

	if (error)
		vrele(vp);
done:
	swdev_syscall_active = 0;
	wakeup_one(&swdev_syscall_active);
done2:
	mtx_unlock(&Giant);
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
 */
int
swaponvp(td, vp, dev, nblks)
	struct thread *td;
	struct vnode *vp;
	dev_t dev;
	u_long nblks;
{
	int index;
	struct swdevt *sp;
	swblk_t vsbase;
	long blk;
	swblk_t dvbase;
	int error;
	u_long aligned_nblks;

	if (!swapdev_vp) {
		error = getnewvnode("none", NULL, swapdev_vnodeop_p,
		    &swapdev_vp);
		if (error)
			panic("Cannot get vnode for swapdev");
		swapdev_vp->v_type = VNON;	/* Untyped */
	}

	ASSERT_VOP_UNLOCKED(vp, "swaponvp");
	for (sp = swdevt, index = 0 ; index < nswdev; index++, sp++) {
		if (sp->sw_vp == vp)
			return EBUSY;
		if (!sp->sw_vp)
			goto found;

	}
	return EINVAL;
    found:
	(void) vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
#ifdef MAC
	error = mac_check_system_swapon(td->td_ucred, vp);
	if (error == 0)
#endif
		error = VOP_OPEN(vp, FREAD | FWRITE, td->td_ucred, td);
	(void) VOP_UNLOCK(vp, 0, td);
	if (error)
		return (error);

	if (nblks == 0 && dev != NODEV && (devsw(dev)->d_psize == 0 ||
	    (nblks = (*devsw(dev)->d_psize) (dev)) == -1)) {
		(void) VOP_CLOSE(vp, FREAD | FWRITE, td->td_ucred, td);
		return (ENXIO);
	}
	if (nblks == 0) {
		(void) VOP_CLOSE(vp, FREAD | FWRITE, td->td_ucred, td);
		return (ENXIO);
	}

	/*
	 * If we go beyond this, we get overflows in the radix
	 * tree bitmap code.
	 */
	if (nblks > 0x40000000 / BLIST_META_RADIX / nswdev) {
		printf("exceeded maximum of %d blocks per swap unit\n",
			0x40000000 / BLIST_META_RADIX / nswdev);
		(void) VOP_CLOSE(vp, FREAD | FWRITE, td->td_ucred, td);
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
	sp->sw_dev = dev2udev(dev);
	sp->sw_device = dev;
	sp->sw_flags = SW_FREED;
	sp->sw_nblks = nblks;
	sp->sw_used = 0;

	/*
	 * nblks, nswap, and dmmax are PAGE_SIZE'd parameters now, not
	 * DEV_BSIZE'd.   aligned_nblks is used to calculate the
	 * size of the swap bitmap, taking into account the stripe size.
	 */
	aligned_nblks = (nblks + (dmmax - 1)) & ~(u_long)(dmmax - 1);

	if (aligned_nblks * nswdev > nswap)
		nswap = aligned_nblks * nswdev;

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

	swap_pager_full = 0;

	return (0);
}

/*
 * SYSCALL: swapoff(devname)
 *
 * Disable swapping on the given device.
 */
#ifndef _SYS_SYSPROTO_H_
struct swapoff_args {
	char *name;
};
#endif

/*
 * MPSAFE
 */
/* ARGSUSED */
int
swapoff(td, uap)
	struct thread *td;
	struct swapoff_args *uap;
{
	struct vnode *vp;
	struct nameidata nd;
	struct swdevt *sp;
	swblk_t dvbase, vsbase;
	u_long nblks, aligned_nblks, blk;
	int error, index;

	mtx_lock(&Giant);

	error = suser(td);
	if (error)
		goto done2;

	while (swdev_syscall_active)
	    tsleep(&swdev_syscall_active, PUSER - 1, "swpoff", 0);
	swdev_syscall_active = 1;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->name, td);
	error = namei(&nd);
	if (error)
		goto done;
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;

	for (sp = swdevt, index = 0 ; index < nswdev; index++, sp++) {
		if (sp->sw_vp == vp)
			goto found;
	}
	error = EINVAL;
	goto done;
found:
	nblks = sp->sw_nblks;

	/*
	 * We can turn off this swap device safely only if the
	 * available virtual memory in the system will fit the amount
	 * of data we will have to page back in, plus an epsilon so
	 * the system doesn't become critically low on swap space.
	 */
	if (cnt.v_free_count + cnt.v_cache_count + vm_swap_size <
	    nblks + nswap_lowat) {
		error = ENOMEM;
		goto done;
	}

	/*
	 * Prevent further allocations on this device.
	 */
	sp->sw_flags |= SW_CLOSING;
	for (dvbase = dmmax; dvbase < nblks; dvbase += dmmax) {
		blk = min(nblks - dvbase, dmmax);
		vsbase = index * dmmax + dvbase * nswdev;
		vm_swap_size -= blist_fill(swapblist, vsbase, blk);
	}

	/*
	 * Page in the contents of the device and close it.
	 */
#ifndef NO_SWAPPING
       	vm_proc_swapin_all(index);
#endif /* !NO_SWAPPING */
	swap_pager_swapoff(index, &sp->sw_used);

	VOP_CLOSE(vp, FREAD | FWRITE, td->td_ucred, td);
	vrele(vp);
	sp->sw_vp = NULL;

	/*
	 * Resize the bitmap based on the new largest swap device,
	 * or free the bitmap if there are no more devices.
	 */
	for (sp = swdevt, nblks = 0; sp < swdevt + nswdev; sp++) {
		if (sp->sw_vp == NULL)
			continue;
		nblks = max(nblks, sp->sw_nblks);
	}

	aligned_nblks = (nblks + (dmmax - 1)) & ~(u_long)(dmmax - 1);
	nswap = aligned_nblks * nswdev;

	if (nswap == 0) {
		blist_destroy(swapblist);
		swapblist = NULL;
		vrele(swapdev_vp);
		swapdev_vp = NULL;
	} else
		blist_resize(&swapblist, nswap, 0);

done:
	swdev_syscall_active = 0;
	wakeup_one(&swdev_syscall_active);
done2:
	mtx_unlock(&Giant);
	return (error);
}

static int
sysctl_vm_swap_info(SYSCTL_HANDLER_ARGS)
{
	int	*name = (int *)arg1;
	int	error, i, n;
	struct xswdev xs;
	struct swdevt *sp;

	if (arg2 != 1) /* name length */
		return (EINVAL);

	for (sp = swdevt, i = 0, n = 0 ; i < nswdev; i++, sp++) {
		if (sp->sw_vp) {
			if (n == *name) {
				xs.xsw_version = XSWDEV_VERSION;
				xs.xsw_dev = sp->sw_dev;
				xs.xsw_flags = sp->sw_flags;
				xs.xsw_nblks = sp->sw_nblks;
				xs.xsw_used = sp->sw_used;

				error = SYSCTL_OUT(req, &xs, sizeof(xs));
				return (error);
			}
			n++;
		}

	}
	return (ENOENT);
}

SYSCTL_INT(_vm, OID_AUTO, nswapdev, CTLFLAG_RD, &nswdev, 0,
    "Number of swap devices");
SYSCTL_NODE(_vm, OID_AUTO, swap_info, CTLFLAG_RD, sysctl_vm_swap_info,
    "Swap statistics by device");
