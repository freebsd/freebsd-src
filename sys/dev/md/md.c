/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

/*
 * The following functions are based in the vn(4) driver: mdstart_swap(),
 * mdstart_vnode(), mdcreate_swap(), mdcreate_vnode() and mddestroy(),
 * and as such under the following copyright:
 *
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah Hdr: vn.c 1.13 94/04/02
 *
 *	from: @(#)vn.c	8.6 (Berkeley) 4/1/94
 * From: src/sys/dev/vn/vn.c,v 1.122 2000/12/16 16:06:03
 */

#include "opt_geom.h"
#include "opt_md.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mdioctl.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/stdint.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#ifndef NO_GEOM
#include <geom/geom.h>
#endif

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/swap_pager.h>
#include <vm/uma.h>

#define MD_MODVER 1

#define MD_SHUTDOWN 0x10000	/* Tell worker thread to terminate. */

#ifndef MD_NSECT
#define MD_NSECT (10000 * 2)
#endif

static MALLOC_DEFINE(M_MD, "MD disk", "Memory Disk");
static MALLOC_DEFINE(M_MDSECT, "MD sectors", "Memory Disk Sectors");

static int md_debug;
SYSCTL_INT(_debug, OID_AUTO, mddebug, CTLFLAG_RW, &md_debug, 0, "");

#if defined(MD_ROOT) && defined(MD_ROOT_SIZE)
/* Image gets put here: */
static u_char mfs_root[MD_ROOT_SIZE*1024] = "MFS Filesystem goes here";
static u_char end_mfs_root[] __unused = "MFS Filesystem had better STOP here";
#endif

static int	mdrootready;
static int	mdunits;
static dev_t	status_dev = 0;

#define CDEV_MAJOR	95

static d_ioctl_t mdctlioctl;

static struct cdevsw mdctl_cdevsw = {
        /* open */      nullopen,
        /* close */     nullclose,
        /* read */      noread,
        /* write */     nowrite,
        /* ioctl */     mdctlioctl,
        /* poll */      nopoll,
        /* mmap */      nommap,
        /* strategy */  nostrategy,
        /* name */      MD_NAME,
        /* maj */       CDEV_MAJOR
};


static LIST_HEAD(, md_s) md_softc_list = LIST_HEAD_INITIALIZER(&md_softc_list);

#define NINDIR	(PAGE_SIZE / sizeof(uintptr_t))
#define NMASK	(NINDIR-1)
static int nshift;

struct indir {
	uintptr_t	*array;
	uint		total;
	uint		used;
	uint		shift;
};

struct md_s {
	int unit;
	LIST_ENTRY(md_s) list;
	struct devstat stats;
	struct bio_queue_head bio_queue;
	struct mtx queue_mtx;
	struct disk disk;
	dev_t dev;
	enum md_types type;
	unsigned nsect;
	unsigned opencount;
	unsigned secsize;
	unsigned flags;
	char name[20];
	struct proc *procp;
#ifndef NO_GEOM
	struct g_geom *gp;
	struct g_provider *pp;
#endif

	/* MD_MALLOC related fields */
	struct indir *indir;
	uma_zone_t uma;

	/* MD_PRELOAD related fields */
	u_char *pl_ptr;
	unsigned pl_len;

	/* MD_VNODE related fields */
	struct vnode *vnode;
	struct ucred *cred;

	/* MD_SWAP related fields */
	vm_object_t object;
};

static int mddestroy(struct md_s *sc, struct thread *td);

static struct indir *
new_indir(uint shift)
{
	struct indir *ip;

	ip = malloc(sizeof *ip, M_MD, M_NOWAIT | M_ZERO);
	if (ip == NULL)
		return (NULL);
	ip->array = malloc(sizeof(uintptr_t) * NINDIR,
	    M_MDSECT, M_NOWAIT | M_ZERO);
	if (ip->array == NULL) {
		free(ip, M_MD);
		return (NULL);
	}
	ip->total = NINDIR;
	ip->shift = shift;
	return (ip);
}

static void
del_indir(struct indir *ip)
{

	free(ip->array, M_MDSECT);
	free(ip, M_MD);
}

static void
destroy_indir(struct md_s *sc, struct indir *ip)
{
	int i;

	for (i = 0; i < NINDIR; i++) {
		if (!ip->array[i])
			continue;
		if (ip->shift)
			destroy_indir(sc, (struct indir*)(ip->array[i]));
		else if (ip->array[i] > 255)
			uma_zfree(sc->uma, (void *)(ip->array[i]));
	}
	del_indir(ip);
}

/*
 * This function does the math and alloctes the top level "indir" structure
 * for a device of "size" sectors.
 */

static struct indir *
dimension(off_t size)
{
	off_t rcnt;
	struct indir *ip;
	int i, layer;

	rcnt = size;
	layer = 0;
	while (rcnt > NINDIR) {
		rcnt /= NINDIR;
		layer++;
	}
	/* figure out log2(NINDIR) */
	for (i = NINDIR, nshift = -1; i; nshift++)
		i >>= 1;

	/*
	 * XXX: the top layer is probably not fully populated, so we allocate
	 * too much space for ip->array in new_indir() here.
	 */
	ip = new_indir(layer * nshift);
	return (ip);
}

/*
 * Read a given sector
 */

static uintptr_t
s_read(struct indir *ip, off_t offset)
{
	struct indir *cip;
	int idx;
	uintptr_t up;

	if (md_debug > 1)
		printf("s_read(%jd)\n", (intmax_t)offset);
	up = 0;
	for (cip = ip; cip != NULL;) {
		if (cip->shift) {
			idx = (offset >> cip->shift) & NMASK;
			up = cip->array[idx];
			cip = (struct indir *)up;
			continue;
		}
		idx = offset & NMASK;
		return (cip->array[idx]);
	}
	return (0);
}

/*
 * Write a given sector, prune the tree if the value is 0
 */

static int
s_write(struct indir *ip, off_t offset, uintptr_t ptr)
{
	struct indir *cip, *lip[10];
	int idx, li;
	uintptr_t up;

	if (md_debug > 1)
		printf("s_write(%jd, %p)\n", (intmax_t)offset, (void *)ptr);
	up = 0;
	li = 0;
	cip = ip;
	for (;;) {
		lip[li++] = cip;
		if (cip->shift) {
			idx = (offset >> cip->shift) & NMASK;
			up = cip->array[idx];
			if (up != 0) {
				cip = (struct indir *)up;
				continue;
			}
			/* Allocate branch */
			cip->array[idx] =
			    (uintptr_t)new_indir(cip->shift - nshift);
			if (cip->array[idx] == 0)
				return (ENOSPC);
			cip->used++;
			up = cip->array[idx];
			cip = (struct indir *)up;
			continue;
		}
		/* leafnode */
		idx = offset & NMASK;
		up = cip->array[idx];
		if (up != 0)
			cip->used--;
		cip->array[idx] = ptr;
		if (ptr != 0)
			cip->used++;
		break;
	}
	if (cip->used != 0 || li == 1)
		return (0);
	li--;
	while (cip->used == 0 && cip != ip) {
		li--;
		idx = (offset >> lip[li]->shift) & NMASK;
		up = lip[li]->array[idx];
		KASSERT(up == (uintptr_t)cip, ("md screwed up"));
		del_indir(cip);
		lip[li]->array[idx] = 0;
		lip[li]->used--;
		cip = lip[li];
	}
	return (0);
}

#ifndef NO_GEOM

struct g_class g_md_class = {
	"MD",
	NULL,
	NULL,
	G_CLASS_INITIALIZER

};

static int
g_md_access(struct g_provider *pp, int r, int w, int e)
{
	struct md_s *sc;

	sc = pp->geom->softc;
	r += pp->acr;
	w += pp->acw;
	e += pp->ace;
	if ((pp->acr + pp->acw + pp->ace) == 0 && (r + w + e) > 0) {
		sc->opencount = 1;
	} else if ((pp->acr + pp->acw + pp->ace) > 0 && (r + w + e) == 0) {
		sc->opencount = 0;
	}
	return (0);
}

static void
g_md_start(struct bio *bp)
{
	struct md_s *sc;

	sc = bp->bio_to->geom->softc;

	switch(bp->bio_cmd) {
	case BIO_GETATTR:
	case BIO_SETATTR:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}
	bp->bio_blkno = bp->bio_offset >> DEV_BSHIFT;
	bp->bio_pblkno = bp->bio_offset / sc->secsize;
	bp->bio_bcount = bp->bio_length;
	mtx_lock(&sc->queue_mtx);
	bioqdisksort(&sc->bio_queue, bp);
	mtx_unlock(&sc->queue_mtx);

	wakeup(sc);
}

DECLARE_GEOM_CLASS(g_md_class, g_md);
#endif

#ifdef NO_GEOM

static d_strategy_t mdstrategy;
static d_open_t mdopen;
static d_close_t mdclose;
static d_ioctl_t mdioctl;

static struct cdevsw md_cdevsw = {
        /* open */      mdopen,
        /* close */     mdclose,
        /* read */      physread,
        /* write */     physwrite,
        /* ioctl */     mdioctl,
        /* poll */      nopoll,
        /* mmap */      nommap,
        /* strategy */  mdstrategy,
        /* name */      MD_NAME,
        /* maj */       CDEV_MAJOR,
        /* dump */      nodump,
        /* psize */     nopsize,
        /* flags */     D_DISK | D_CANFREE | D_MEMDISK,
};

static struct cdevsw mddisk_cdevsw;

static int
mdopen(dev_t dev, int flag, int fmt, struct thread *td)
{
	struct md_s *sc;

	if (md_debug)
		printf("mdopen(%s %x %x %p)\n",
			devtoname(dev), flag, fmt, td);

	sc = dev->si_drv1;

	sc->disk.d_sectorsize = sc->secsize;
	sc->disk.d_mediasize = (off_t)sc->nsect * sc->secsize;
	sc->disk.d_fwsectors = sc->nsect > 63 ? 63 : sc->nsect;
	sc->disk.d_fwheads = 1;
	sc->opencount++;
	return (0);
}

static int
mdclose(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct md_s *sc = dev->si_drv1;

	sc->opencount--;
	return (0);
}

static int
mdioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct thread *td)
{

	if (md_debug)
		printf("mdioctl(%s %lx %p %x %p)\n",
			devtoname(dev), cmd, addr, flags, td);

	return (ENOIOCTL);
}

static void
mdstrategy(struct bio *bp)
{
	struct md_s *sc;

	if (md_debug > 1)
		printf("mdstrategy(%p) %s %x, %jd, %jd %ld, %p)\n",
		    (void *)bp, devtoname(bp->bio_dev), bp->bio_flags,
		    (intmax_t)bp->bio_blkno,
		    (intmax_t)bp->bio_pblkno,
		    bp->bio_bcount / DEV_BSIZE,
		    (void *)bp->bio_data);

	sc = bp->bio_dev->si_drv1;

	mtx_lock(&sc->queue_mtx);
	bioqdisksort(&sc->bio_queue, bp);
	mtx_unlock(&sc->queue_mtx);

	wakeup(sc);
}

#endif /* NO_GEOM */

static int
mdstart_malloc(struct md_s *sc, struct bio *bp)
{
	int i, error;
	u_char *dst;
	unsigned secno, nsec, uc;
	uintptr_t sp, osp;

	nsec = bp->bio_bcount / sc->secsize;
	secno = bp->bio_pblkno;
	dst = bp->bio_data;
	error = 0;
	while (nsec--) {
		osp = s_read(sc->indir, secno);
		if (bp->bio_cmd == BIO_DELETE) {
			if (osp != 0)
				error = s_write(sc->indir, secno, 0);
		} else if (bp->bio_cmd == BIO_READ) {
			if (osp == 0)
				bzero(dst, sc->secsize);
			else if (osp <= 255)
				for (i = 0; i < sc->secsize; i++)
					dst[i] = osp;
			else
				bcopy((void *)osp, dst, sc->secsize);
			osp = 0;
		} else if (bp->bio_cmd == BIO_WRITE) {
			if (sc->flags & MD_COMPRESS) {
				uc = dst[0];
				for (i = 1; i < sc->secsize; i++)
					if (dst[i] != uc)
						break;
			} else {
				i = 0;
				uc = 0;
			}
			if (i == sc->secsize) {
				if (osp != uc)
					error = s_write(sc->indir, secno, uc);
			} else {
				if (osp <= 255) {
					sp = (uintptr_t) uma_zalloc(
					    sc->uma, M_NOWAIT);
					if (sp == 0) {
						error = ENOSPC;
						break;
					}
					bcopy(dst, (void *)sp, sc->secsize);
					error = s_write(sc->indir, secno, sp);
				} else {
					bcopy(dst, (void *)osp, sc->secsize);
					osp = 0;
				}
			}
		} else {
			error = EOPNOTSUPP;
		}
		if (osp > 255)
			uma_zfree(sc->uma, (void*)osp);
		if (error)
			break;
		secno++;
		dst += sc->secsize;
	}
	bp->bio_resid = 0;
	return (error);
}

static int
mdstart_preload(struct md_s *sc, struct bio *bp)
{

	if (bp->bio_cmd == BIO_DELETE) {
	} else if (bp->bio_cmd == BIO_READ) {
		bcopy(sc->pl_ptr + (bp->bio_pblkno << DEV_BSHIFT), bp->bio_data, bp->bio_bcount);
	} else {
		bcopy(bp->bio_data, sc->pl_ptr + (bp->bio_pblkno << DEV_BSHIFT), bp->bio_bcount);
	}
	bp->bio_resid = 0;
	return (0);
}

static int
mdstart_vnode(struct md_s *sc, struct bio *bp)
{
	int error;
	struct uio auio;
	struct iovec aiov;
	struct mount *mp;

	/*
	 * VNODE I/O
	 *
	 * If an error occurs, we set BIO_ERROR but we do not set
	 * B_INVAL because (for a write anyway), the buffer is
	 * still valid.
	 */

	bzero(&auio, sizeof(auio));

	aiov.iov_base = bp->bio_data;
	aiov.iov_len = bp->bio_bcount;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = (vm_ooffset_t)bp->bio_pblkno * sc->secsize;
	auio.uio_segflg = UIO_SYSSPACE;
	if(bp->bio_cmd == BIO_READ)
		auio.uio_rw = UIO_READ;
	else
		auio.uio_rw = UIO_WRITE;
	auio.uio_resid = bp->bio_bcount;
	auio.uio_td = curthread;
	/*
	 * When reading set IO_DIRECT to try to avoid double-caching
	 * the data.  When writing IO_DIRECT is not optimal, but we
	 * must set IO_NOWDRAIN to avoid a wdrain deadlock.
	 */
	if (bp->bio_cmd == BIO_READ) {
		vn_lock(sc->vnode, LK_EXCLUSIVE | LK_RETRY, curthread);
		error = VOP_READ(sc->vnode, &auio, IO_DIRECT, sc->cred);
	} else {
		(void) vn_start_write(sc->vnode, &mp, V_WAIT);
		vn_lock(sc->vnode, LK_EXCLUSIVE | LK_RETRY, curthread);
		error = VOP_WRITE(sc->vnode, &auio, IO_NOWDRAIN, sc->cred);
		vn_finished_write(mp);
	}
	VOP_UNLOCK(sc->vnode, 0, curthread);
	bp->bio_resid = auio.uio_resid;
	return (error);
}

#ifndef NO_GEOM
static void
mddone_swap(struct bio *bp)
{

	bp->bio_completed = bp->bio_length - bp->bio_resid;
	g_std_done(bp);
}
#endif

static int
mdstart_swap(struct md_s *sc, struct bio *bp)
{
#ifndef NO_GEOM
	{
	struct bio *bp2;

	bp2 = g_clone_bio(bp);
	bp2->bio_done = mddone_swap;
	bp2->bio_blkno = bp2->bio_offset >> DEV_BSHIFT;
	bp2->bio_pblkno = bp2->bio_offset / sc->secsize;
	bp2->bio_bcount = bp2->bio_length;
	bp = bp2;
	}
#endif

	bp->bio_resid = 0;
	if ((bp->bio_cmd == BIO_DELETE) && (sc->flags & MD_RESERVE))
		biodone(bp);
	else
		vm_pager_strategy(sc->object, bp);
	return (-1);
}

static void
md_kthread(void *arg)
{
	struct md_s *sc;
	struct bio *bp;
	int error, hasgiant;

	sc = arg;
	curthread->td_base_pri = PRIBIO;

	switch (sc->type) {
	case MD_SWAP:
	case MD_VNODE:
		mtx_lock(&Giant);
		hasgiant = 1;
		break;
	case MD_MALLOC:
	case MD_PRELOAD:
	default:
		hasgiant = 0;
		break;
	}
	
	for (;;) {
		mtx_lock(&sc->queue_mtx);
		bp = bioq_first(&sc->bio_queue);
		if (bp)
			bioq_remove(&sc->bio_queue, bp);
		if (!bp) {
			if (sc->flags & MD_SHUTDOWN) {
				mtx_unlock(&sc->queue_mtx);
				sc->procp = NULL;
				wakeup(&sc->procp);
				if (!hasgiant)
					mtx_lock(&Giant);
				kthread_exit(0);
			}
			msleep(sc, &sc->queue_mtx, PRIBIO | PDROP, "mdwait", 0);
			continue;
		}
		mtx_unlock(&sc->queue_mtx);

		switch (sc->type) {
		case MD_MALLOC:
			devstat_start_transaction(&sc->stats);
			error = mdstart_malloc(sc, bp);
			break;
		case MD_PRELOAD:
			devstat_start_transaction(&sc->stats);
			error = mdstart_preload(sc, bp);
			break;
		case MD_VNODE:
			devstat_start_transaction(&sc->stats);
			error = mdstart_vnode(sc, bp);
			break;
		case MD_SWAP:
			error = mdstart_swap(sc, bp);
			break;
		default:
			panic("Impossible md(type)");
			break;
		}

		if (error != -1) {
#ifdef NO_GEOM
			biofinish(bp, &sc->stats, error);
#else /* !NO_GEOM */
			bp->bio_completed = bp->bio_length;
			g_io_deliver(bp, error);
#endif
		}
	}
}

static struct md_s *
mdfind(int unit)
{
	struct md_s *sc;

	/* XXX: LOCK(unique unit numbers) */
	LIST_FOREACH(sc, &md_softc_list, list) {
		if (sc->unit == unit)
			break;
	}
	/* XXX: UNLOCK(unique unit numbers) */
	return (sc);
}

static struct md_s *
mdnew(int unit)
{
	struct md_s *sc;
	int error, max = -1;

	/* XXX: LOCK(unique unit numbers) */
	LIST_FOREACH(sc, &md_softc_list, list) {
		if (sc->unit == unit) {
			/* XXX: UNLOCK(unique unit numbers) */
			return (NULL);
		}
		if (sc->unit > max)
			max = sc->unit;
	}
	if (unit == -1)
		unit = max + 1;
	if (unit > 255)
		return (NULL);
	sc = (struct md_s *)malloc(sizeof *sc, M_MD, M_ZERO);
	sc->unit = unit;
	bioq_init(&sc->bio_queue);
	mtx_init(&sc->queue_mtx, "md bio queue", NULL, MTX_DEF);
	sprintf(sc->name, "md%d", unit);
	error = kthread_create(md_kthread, sc, &sc->procp, 0, 0,"%s", sc->name);
	if (error) {
		free(sc, M_MD);
		return (NULL);
	}
	LIST_INSERT_HEAD(&md_softc_list, sc, list);
	/* XXX: UNLOCK(unique unit numbers) */
	return (sc);
}

static void
mdinit(struct md_s *sc)
{

	devstat_add_entry(&sc->stats, MD_NAME, sc->unit, sc->secsize,
		DEVSTAT_NO_ORDERED_TAGS,
		DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_OTHER,
		DEVSTAT_PRIORITY_OTHER);
#ifdef NO_GEOM
	sc->dev = disk_create(sc->unit, &sc->disk, 0, &md_cdevsw, &mddisk_cdevsw);
	sc->dev->si_drv1 = sc;
#else /* !NO_GEOM */
	{
	struct g_geom *gp;
	struct g_provider *pp;

	DROP_GIANT();
	g_topology_lock();
	gp = g_new_geomf(&g_md_class, "md%d", sc->unit);
	gp->start = g_md_start;
	gp->access = g_md_access;
	gp->softc = sc;
	pp = g_new_providerf(gp, "md%d", sc->unit);
	pp->mediasize = (off_t)sc->nsect * sc->secsize;
	pp->sectorsize = sc->secsize;
	sc->gp = gp;
	sc->pp = pp;
	g_error_provider(pp, 0);
	g_topology_unlock();
	PICKUP_GIANT();
	}
#endif /* NO_GEOM */
}

/*
 * XXX: we should check that the range they feed us is mapped.
 * XXX: we should implement read-only.
 */

static int
mdcreate_preload(struct md_ioctl *mdio)
{
	struct md_s *sc;

	if (mdio->md_size == 0)
		return (EINVAL);
	if (mdio->md_options & ~(MD_AUTOUNIT))
		return (EINVAL);
	if (mdio->md_options & MD_AUTOUNIT) {
		sc = mdnew(-1);
		if (sc == NULL)
			return (ENOMEM);
		mdio->md_unit = sc->unit;
	} else {
		sc = mdnew(mdio->md_unit);
		if (sc == NULL)
			return (EBUSY);
	}
	sc->type = MD_PRELOAD;
	sc->secsize = DEV_BSIZE;
	sc->nsect = mdio->md_size;
	sc->flags = mdio->md_options & MD_FORCE;
	/* Cast to pointer size, then to pointer to avoid warning */
	sc->pl_ptr = (u_char *)(uintptr_t)mdio->md_base;
	sc->pl_len = (mdio->md_size << DEV_BSHIFT);
	mdinit(sc);
	return (0);
}


static int
mdcreate_malloc(struct md_ioctl *mdio)
{
	struct md_s *sc;
	off_t u;
	uintptr_t sp;
	int error;

	error = 0;
	if (mdio->md_size == 0)
		return (EINVAL);
	if (mdio->md_options & ~(MD_AUTOUNIT | MD_COMPRESS | MD_RESERVE))
		return (EINVAL);
	/* Compression doesn't make sense if we have reserved space */
	if (mdio->md_options & MD_RESERVE)
		mdio->md_options &= ~MD_COMPRESS;
	if (mdio->md_options & MD_AUTOUNIT) {
		sc = mdnew(-1);
		if (sc == NULL)
			return (ENOMEM);
		mdio->md_unit = sc->unit;
	} else {
		sc = mdnew(mdio->md_unit);
		if (sc == NULL)
			return (EBUSY);
	}
	sc->type = MD_MALLOC;
	sc->secsize = DEV_BSIZE;
	sc->nsect = mdio->md_size;
	sc->flags = mdio->md_options & (MD_COMPRESS | MD_FORCE);
	sc->indir = dimension(sc->nsect);
	sc->uma = uma_zcreate(sc->name, sc->secsize,
	    NULL, NULL, NULL, NULL, 0x1ff, 0);
	if (mdio->md_options & MD_RESERVE) {
		for (u = 0; u < sc->nsect; u++) {
			sp = (uintptr_t) uma_zalloc(sc->uma, M_NOWAIT | M_ZERO);
			if (sp != 0)
				error = s_write(sc->indir, u, sp);
			else
				error = ENOMEM;
			if (error)
				break;
		}
	}
	if (!error)  {
		mdinit(sc);
	} else
		mddestroy(sc, NULL);
	return (error);
}


static int
mdsetcred(struct md_s *sc, struct ucred *cred)
{
	char *tmpbuf;
	int error = 0;

	/*
	 * Set credits in our softc
	 */

	if (sc->cred)
		crfree(sc->cred);
	sc->cred = crhold(cred);

	/*
	 * Horrible kludge to establish credentials for NFS  XXX.
	 */

	if (sc->vnode) {
		struct uio auio;
		struct iovec aiov;

		tmpbuf = malloc(sc->secsize, M_TEMP, 0);
		bzero(&auio, sizeof(auio));

		aiov.iov_base = tmpbuf;
		aiov.iov_len = sc->secsize;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		auio.uio_rw = UIO_READ;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_resid = aiov.iov_len;
		vn_lock(sc->vnode, LK_EXCLUSIVE | LK_RETRY, curthread);
		error = VOP_READ(sc->vnode, &auio, 0, sc->cred);
		VOP_UNLOCK(sc->vnode, 0, curthread);
		free(tmpbuf, M_TEMP);
	}
	return (error);
}

static int
mdcreate_vnode(struct md_ioctl *mdio, struct thread *td)
{
	struct md_s *sc;
	struct vattr vattr;
	struct nameidata nd;
	int error, flags;

	flags = FREAD|FWRITE;
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, mdio->md_file, td);
	error = vn_open(&nd, &flags, 0);
	if (error) {
		if (error != EACCES && error != EPERM && error != EROFS)
			return (error);
		flags &= ~FWRITE;
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, mdio->md_file, td);
		error = vn_open(&nd, &flags, 0);
		if (error)
			return (error);
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (nd.ni_vp->v_type != VREG ||
	    (error = VOP_GETATTR(nd.ni_vp, &vattr, td->td_ucred, td))) {
		VOP_UNLOCK(nd.ni_vp, 0, td);
		(void) vn_close(nd.ni_vp, flags, td->td_ucred, td);
		return (error ? error : EINVAL);
	}
	VOP_UNLOCK(nd.ni_vp, 0, td);

	if (mdio->md_options & MD_AUTOUNIT) {
		sc = mdnew(-1);
		mdio->md_unit = sc->unit;
	} else {
		sc = mdnew(mdio->md_unit);
	}
	if (sc == NULL) {
		(void) vn_close(nd.ni_vp, flags, td->td_ucred, td);
		return (EBUSY);
	}

	sc->type = MD_VNODE;
	sc->flags = mdio->md_options & MD_FORCE;
	if (!(flags & FWRITE))
		sc->flags |= MD_READONLY;
	sc->secsize = DEV_BSIZE;
	sc->vnode = nd.ni_vp;

	/*
	 * If the size is specified, override the file attributes.
	 */
	if (mdio->md_size)
		sc->nsect = mdio->md_size;
	else
		sc->nsect = vattr.va_size / sc->secsize; /* XXX: round up ? */
	if (sc->nsect == 0) {
		mddestroy(sc, td);
		return (EINVAL);
	}
	error = mdsetcred(sc, td->td_ucred);
	if (error) {
		mddestroy(sc, td);
		return (error);
	}
	mdinit(sc);
	return (0);
}

static int
mddestroy(struct md_s *sc, struct thread *td)
{

	GIANT_REQUIRED;

	mtx_destroy(&sc->queue_mtx);
	devstat_remove_entry(&sc->stats);
#ifdef NO_GEOM
	if (sc->dev != NULL)
		disk_destroy(sc->dev);
#else /* !NO_GEOM */
	{
	if (sc->gp) {
		sc->gp->flags |= G_GEOM_WITHER;
		sc->gp->softc = NULL;
	}
	if (sc->pp)
		g_orphan_provider(sc->pp, ENXIO);
	}
#endif
	sc->flags |= MD_SHUTDOWN;
	wakeup(sc);
	while (sc->procp != NULL)
		tsleep(&sc->procp, PRIBIO, "mddestroy", hz / 10);
	if (sc->vnode != NULL)
		(void)vn_close(sc->vnode, sc->flags & MD_READONLY ?
		    FREAD : (FREAD|FWRITE), sc->cred, td);
	if (sc->cred != NULL)
		crfree(sc->cred);
	if (sc->object != NULL) {
		vm_pager_deallocate(sc->object);
	}
	if (sc->indir)
		destroy_indir(sc, sc->indir);
	if (sc->uma)
		uma_zdestroy(sc->uma);

	/* XXX: LOCK(unique unit numbers) */
	LIST_REMOVE(sc, list);
	/* XXX: UNLOCK(unique unit numbers) */
	free(sc, M_MD);
	return (0);
}

static int
mdcreate_swap(struct md_ioctl *mdio, struct thread *td)
{
	int error;
	struct md_s *sc;

	GIANT_REQUIRED;

	if (mdio->md_options & MD_AUTOUNIT) {
		sc = mdnew(-1);
		mdio->md_unit = sc->unit;
	} else {
		sc = mdnew(mdio->md_unit);
	}
	if (sc == NULL)
		return (EBUSY);

	sc->type = MD_SWAP;

	/*
	 * Range check.  Disallow negative sizes or any size less then the
	 * size of a page.  Then round to a page.
	 */

	if (mdio->md_size == 0) {
		mddestroy(sc, td);
		return (EDOM);
	}

	/*
	 * Allocate an OBJT_SWAP object.
	 *
	 * sc_secsize is PAGE_SIZE'd
	 *
	 * mdio->size is in DEV_BSIZE'd chunks.
	 * Note the truncation.
	 */

	sc->secsize = PAGE_SIZE;
	sc->nsect = mdio->md_size / (PAGE_SIZE / DEV_BSIZE);
	sc->object = vm_pager_allocate(OBJT_SWAP, NULL, sc->secsize * (vm_offset_t)sc->nsect, VM_PROT_DEFAULT, 0);
	sc->flags = mdio->md_options & MD_FORCE;
	if (mdio->md_options & MD_RESERVE) {
		if (swap_pager_reserve(sc->object, 0, sc->nsect) < 0) {
			vm_pager_deallocate(sc->object);
			sc->object = NULL;
			mddestroy(sc, td);
			return (EDOM);
		}
	}
	error = mdsetcred(sc, td->td_ucred);
	if (error)
		mddestroy(sc, td);
	else
		mdinit(sc);
	return (error);
}

static int
mddetach(int unit, struct thread *td)
{
	struct md_s *sc;

	sc = mdfind(unit);
	if (sc == NULL)
		return (ENOENT);
	if (sc->opencount != 0 && !(sc->flags & MD_FORCE))
		return (EBUSY);
	switch(sc->type) {
	case MD_VNODE:
	case MD_SWAP:
	case MD_MALLOC:
	case MD_PRELOAD:
		return (mddestroy(sc, td));
	default:
		return (EOPNOTSUPP);
	}
}

static int
mdctlioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct thread *td)
{
	struct md_ioctl *mdio;
	struct md_s *sc;
	int i;

	if (md_debug)
		printf("mdctlioctl(%s %lx %p %x %p)\n",
			devtoname(dev), cmd, addr, flags, td);

	/*
	 * We assert the version number in the individual ioctl
	 * handlers instead of out here because (a) it is possible we
	 * may add another ioctl in the future which doesn't read an
	 * mdio, and (b) the correct return value for an unknown ioctl
	 * is ENOIOCTL, not EINVAL.
	 */
	mdio = (struct md_ioctl *)addr;
	switch (cmd) {
	case MDIOCATTACH:
		if (mdio->md_version != MDIOVERSION)
			return (EINVAL);
		switch (mdio->md_type) {
		case MD_MALLOC:
			return (mdcreate_malloc(mdio));
		case MD_PRELOAD:
			return (mdcreate_preload(mdio));
		case MD_VNODE:
			return (mdcreate_vnode(mdio, td));
		case MD_SWAP:
			return (mdcreate_swap(mdio, td));
		default:
			return (EINVAL);
		}
	case MDIOCDETACH:
		if (mdio->md_version != MDIOVERSION)
			return (EINVAL);
		if (mdio->md_file != NULL || mdio->md_size != 0 ||
		    mdio->md_options != 0)
			return (EINVAL);
		return (mddetach(mdio->md_unit, td));
	case MDIOCQUERY:
		if (mdio->md_version != MDIOVERSION)
			return (EINVAL);
		sc = mdfind(mdio->md_unit);
		if (sc == NULL)
			return (ENOENT);
		mdio->md_type = sc->type;
		mdio->md_options = sc->flags;
		switch (sc->type) {
		case MD_MALLOC:
			mdio->md_size = sc->nsect;
			break;
		case MD_PRELOAD:
			mdio->md_size = sc->nsect;
			mdio->md_base = (uint64_t)(intptr_t)sc->pl_ptr;
			break;
		case MD_SWAP:
			mdio->md_size = sc->nsect * (PAGE_SIZE / DEV_BSIZE);
			break;
		case MD_VNODE:
			mdio->md_size = sc->nsect;
			/* XXX fill this in */
			mdio->md_file = NULL;
			break;
		}
		return (0);
	case MDIOCLIST:
		i = 1;
		LIST_FOREACH(sc, &md_softc_list, list) {
			if (i == MDNPAD - 1)
				mdio->md_pad[i] = -1;
			else
				mdio->md_pad[i++] = sc->unit;
		}
		mdio->md_pad[0] = i - 1;
		return (0);
	default:
		return (ENOIOCTL);
	};
	return (ENOIOCTL);
}

static void
md_preloaded(u_char *image, unsigned length)
{
	struct md_s *sc;

	sc = mdnew(-1);
	if (sc == NULL)
		return;
	sc->type = MD_PRELOAD;
	sc->secsize = DEV_BSIZE;
	sc->nsect = length / DEV_BSIZE;
	sc->pl_ptr = image;
	sc->pl_len = length;
	if (sc->unit == 0)
		mdrootready = 1;
	mdinit(sc);
}

static void
md_drvinit(void *unused)
{

	caddr_t mod;
	caddr_t c;
	u_char *ptr, *name, *type;
	unsigned len;

#ifdef MD_ROOT_SIZE
	md_preloaded(mfs_root, MD_ROOT_SIZE*1024);
#endif
	mod = NULL;
	while ((mod = preload_search_next_name(mod)) != NULL) {
		name = (char *)preload_search_info(mod, MODINFO_NAME);
		type = (char *)preload_search_info(mod, MODINFO_TYPE);
		if (name == NULL)
			continue;
		if (type == NULL)
			continue;
		if (strcmp(type, "md_image") && strcmp(type, "mfs_root"))
			continue;
		c = preload_search_info(mod, MODINFO_ADDR);
		ptr = *(u_char **)c;
		c = preload_search_info(mod, MODINFO_SIZE);
		len = *(size_t *)c;
		printf("%s%d: Preloaded image <%s> %d bytes at %p\n",
		    MD_NAME, mdunits, name, len, ptr);
		md_preloaded(ptr, len);
	}
	status_dev = make_dev(&mdctl_cdevsw, 0xffff00ff, UID_ROOT, GID_WHEEL,
	    0600, MDCTL_NAME);
}

static int
md_modevent(module_t mod, int type, void *data)
{
	int error;
	struct md_s *sc;

	switch (type) {
	case MOD_LOAD:
		md_drvinit(NULL);
		break;
	case MOD_UNLOAD:
		LIST_FOREACH(sc, &md_softc_list, list) {
			error = mddetach(sc->unit, curthread);
			if (error != 0)
				return (error);
		}
		if (status_dev)
			destroy_dev(status_dev);
		status_dev = 0;
		break;
	default:
		break;
	}
	return (0);
}

static moduledata_t md_mod = {
	MD_NAME,
	md_modevent,
	NULL
};
DECLARE_MODULE(md, md_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE+CDEV_MAJOR);
MODULE_VERSION(md, MD_MODVER);


#ifdef MD_ROOT
static void
md_takeroot(void *junk)
{
	if (mdrootready)
		rootdevnames[0] = "ufs:/dev/md0";
}

SYSINIT(md_root, SI_SUB_MOUNT_ROOT, SI_ORDER_FIRST, md_takeroot, NULL);
#endif /* MD_ROOT */
