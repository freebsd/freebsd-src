/*-
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

/*-
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
#include <sys/sched.h>
#include <sys/sf_buf.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <geom/geom.h>

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

static g_init_t g_md_init;
static g_fini_t g_md_fini;
static g_start_t g_md_start;
static g_access_t g_md_access;

static int	mdunits;
static struct cdev *status_dev = 0;

static d_ioctl_t mdctlioctl;

static struct cdevsw mdctl_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_ioctl =	mdctlioctl,
	.d_name =	MD_NAME,
};

struct g_class g_md_class = {
	.name = "MD",
	.version = G_VERSION,
	.init = g_md_init,
	.fini = g_md_fini,
	.start = g_md_start,
	.access = g_md_access,
};

DECLARE_GEOM_CLASS(g_md_class, g_md);


static LIST_HEAD(, md_s) md_softc_list = LIST_HEAD_INITIALIZER(&md_softc_list);

#define NINDIR	(PAGE_SIZE / sizeof(uintptr_t))
#define NMASK	(NINDIR-1)
static int nshift;

struct indir {
	uintptr_t	*array;
	u_int		total;
	u_int		used;
	u_int		shift;
};

struct md_s {
	int unit;
	LIST_ENTRY(md_s) list;
	struct bio_queue_head bio_queue;
	struct mtx queue_mtx;
	struct cdev *dev;
	enum md_types type;
	off_t mediasize;
	unsigned sectorsize;
	unsigned opencount;
	unsigned fwheads;
	unsigned fwsectors;
	unsigned flags;
	char name[20];
	struct proc *procp;
	struct g_geom *gp;
	struct g_provider *pp;

	/* MD_MALLOC related fields */
	struct indir *indir;
	uma_zone_t uma;

	/* MD_PRELOAD related fields */
	u_char *pl_ptr;
	size_t pl_len;

	/* MD_VNODE related fields */
	struct vnode *vnode;
	char file[PATH_MAX];
	struct ucred *cred;

	/* MD_SWAP related fields */
	vm_object_t object;
};

static int mddestroy(struct md_s *sc, struct thread *td);

static struct indir *
new_indir(u_int shift)
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
	 * too much space for ip->array in here.
	 */
	ip = malloc(sizeof *ip, M_MD, M_WAITOK | M_ZERO);
	ip->array = malloc(sizeof(uintptr_t) * NINDIR,
	    M_MDSECT, M_WAITOK | M_ZERO);
	ip->total = NINDIR;
	ip->shift = layer * nshift;
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


static int
g_md_access(struct g_provider *pp, int r, int w, int e)
{
	struct md_s *sc;

	sc = pp->geom->softc;
	if (sc == NULL)
		return (ENXIO);
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
	mtx_lock(&sc->queue_mtx);
	bioq_disksort(&sc->bio_queue, bp);
	mtx_unlock(&sc->queue_mtx);
	wakeup(sc);
}



static int
mdstart_malloc(struct md_s *sc, struct bio *bp)
{
	int i, error;
	u_char *dst;
	off_t secno, nsec, uc;
	uintptr_t sp, osp;

	nsec = bp->bio_length / sc->sectorsize;
	secno = bp->bio_offset / sc->sectorsize;
	dst = bp->bio_data;
	error = 0;
	while (nsec--) {
		osp = s_read(sc->indir, secno);
		if (bp->bio_cmd == BIO_DELETE) {
			if (osp != 0)
				error = s_write(sc->indir, secno, 0);
		} else if (bp->bio_cmd == BIO_READ) {
			if (osp == 0)
				bzero(dst, sc->sectorsize);
			else if (osp <= 255)
				for (i = 0; i < sc->sectorsize; i++)
					dst[i] = osp;
			else
				bcopy((void *)osp, dst, sc->sectorsize);
			osp = 0;
		} else if (bp->bio_cmd == BIO_WRITE) {
			if (sc->flags & MD_COMPRESS) {
				uc = dst[0];
				for (i = 1; i < sc->sectorsize; i++)
					if (dst[i] != uc)
						break;
			} else {
				i = 0;
				uc = 0;
			}
			if (i == sc->sectorsize) {
				if (osp != uc)
					error = s_write(sc->indir, secno, uc);
			} else {
				if (osp <= 255) {
					sp = (uintptr_t)uma_zalloc(sc->uma,
					    M_NOWAIT);
					if (sp == 0) {
						error = ENOSPC;
						break;
					}
					bcopy(dst, (void *)sp, sc->sectorsize);
					error = s_write(sc->indir, secno, sp);
				} else {
					bcopy(dst, (void *)osp, sc->sectorsize);
					osp = 0;
				}
			}
		} else {
			error = EOPNOTSUPP;
		}
		if (osp > 255)
			uma_zfree(sc->uma, (void*)osp);
		if (error != 0)
			break;
		secno++;
		dst += sc->sectorsize;
	}
	bp->bio_resid = 0;
	return (error);
}

static int
mdstart_preload(struct md_s *sc, struct bio *bp)
{

	switch (bp->bio_cmd) {
	case BIO_READ:
		bcopy(sc->pl_ptr + bp->bio_offset, bp->bio_data,
		    bp->bio_length);
		break;
	case BIO_WRITE:
		bcopy(bp->bio_data, sc->pl_ptr + bp->bio_offset,
		    bp->bio_length);
		break;
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
	aiov.iov_len = bp->bio_length;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = (vm_ooffset_t)bp->bio_offset;
	auio.uio_segflg = UIO_SYSSPACE;
	if(bp->bio_cmd == BIO_READ)
		auio.uio_rw = UIO_READ;
	else if(bp->bio_cmd == BIO_WRITE)
		auio.uio_rw = UIO_WRITE;
	else
		panic("wrong BIO_OP in mdstart_vnode");
	auio.uio_resid = bp->bio_length;
	auio.uio_td = curthread;
	/*
	 * When reading set IO_DIRECT to try to avoid double-caching
	 * the data.  When writing IO_DIRECT is not optimal.
	 */
	if (bp->bio_cmd == BIO_READ) {
		vn_lock(sc->vnode, LK_EXCLUSIVE | LK_RETRY, curthread);
		error = VOP_READ(sc->vnode, &auio, IO_DIRECT, sc->cred);
		VOP_UNLOCK(sc->vnode, 0, curthread);
	} else {
		(void) vn_start_write(sc->vnode, &mp, V_WAIT);
		vn_lock(sc->vnode, LK_EXCLUSIVE | LK_RETRY, curthread);
		error = VOP_WRITE(sc->vnode, &auio,
		    sc->flags & MD_ASYNC ? 0 : IO_SYNC, sc->cred);
		VOP_UNLOCK(sc->vnode, 0, curthread);
		vn_finished_write(mp);
	}
	bp->bio_resid = auio.uio_resid;
	return (error);
}

static int
mdstart_swap(struct md_s *sc, struct bio *bp)
{
	struct sf_buf *sf;
	int rv, offs, len, lastend;
	vm_pindex_t i, lastp;
	vm_page_t m;
	u_char *p;

	p = bp->bio_data;

	/*
	 * offs is the ofset at whih to start operating on the
	 * next (ie, first) page.  lastp is the last page on
	 * which we're going to operate.  lastend is the ending
	 * position within that last page (ie, PAGE_SIZE if
	 * we're operating on complete aligned pages).
	 */
	offs = bp->bio_offset % PAGE_SIZE;
	lastp = (bp->bio_offset + bp->bio_length - 1) / PAGE_SIZE;
	lastend = (bp->bio_offset + bp->bio_length - 1) % PAGE_SIZE + 1;

	rv = VM_PAGER_OK;
	VM_OBJECT_LOCK(sc->object);
	vm_object_pip_add(sc->object, 1);
	for (i = bp->bio_offset / PAGE_SIZE; i <= lastp; i++) {
		len = ((i == lastp) ? lastend : PAGE_SIZE) - offs;

		m = vm_page_grab(sc->object, i,
		    VM_ALLOC_NORMAL|VM_ALLOC_RETRY);
		VM_OBJECT_UNLOCK(sc->object);
		sf = sf_buf_alloc(m, 0);
		VM_OBJECT_LOCK(sc->object);
		if (bp->bio_cmd == BIO_READ) {
			if (m->valid != VM_PAGE_BITS_ALL)
				rv = vm_pager_get_pages(sc->object, &m, 1, 0);
			if (rv == VM_PAGER_ERROR) {
				sf_buf_free(sf);
				vm_page_lock_queues();
				vm_page_wakeup(m);
				vm_page_unlock_queues();
				break;
			}
			bcopy((void *)(sf_buf_kva(sf) + offs), p, len);
		} else if (bp->bio_cmd == BIO_WRITE) {
			if (len != PAGE_SIZE && m->valid != VM_PAGE_BITS_ALL)
				rv = vm_pager_get_pages(sc->object, &m, 1, 0);
			if (rv == VM_PAGER_ERROR) {
				sf_buf_free(sf);
				vm_page_lock_queues();
				vm_page_wakeup(m);
				vm_page_unlock_queues();
				break;
			}
			bcopy(p, (void *)(sf_buf_kva(sf) + offs), len);
			m->valid = VM_PAGE_BITS_ALL;
#if 0
		} else if (bp->bio_cmd == BIO_DELETE) {
			if (len != PAGE_SIZE && m->valid != VM_PAGE_BITS_ALL)
				rv = vm_pager_get_pages(sc->object, &m, 1, 0);
			if (rv == VM_PAGER_ERROR) {
				sf_buf_free(sf);
				vm_page_lock_queues();
				vm_page_wakeup(m);
				vm_page_unlock_queues();
				break;
			}
			bzero((void *)(sf_buf_kva(sf) + offs), len);
			vm_page_dirty(m);
			m->valid = VM_PAGE_BITS_ALL;
#endif
		}
		sf_buf_free(sf);
		vm_page_lock_queues();
		vm_page_wakeup(m);
		vm_page_activate(m);
		if (bp->bio_cmd == BIO_WRITE)
			vm_page_dirty(m);
		vm_page_unlock_queues();

		/* Actions on further pages start at offset 0 */
		p += PAGE_SIZE - offs;
		offs = 0;
#if 0
if (bootverbose || bp->bio_offset / PAGE_SIZE < 17)
printf("wire_count %d busy %d flags %x hold_count %d act_count %d queue %d valid %d dirty %d @ %d\n",
    m->wire_count, m->busy, 
    m->flags, m->hold_count, m->act_count, m->queue, m->valid, m->dirty, i);
#endif
	}
	vm_object_pip_subtract(sc->object, 1);
	vm_object_set_writeable_dirty(sc->object);
	VM_OBJECT_UNLOCK(sc->object);
	return (rv != VM_PAGER_ERROR ? 0 : ENOSPC);
}

static void
md_kthread(void *arg)
{
	struct md_s *sc;
	struct bio *bp;
	int error, hasgiant;

	sc = arg;
	mtx_lock_spin(&sched_lock);
	sched_prio(curthread, PRIBIO);
	mtx_unlock_spin(&sched_lock);

	switch (sc->type) {
	case MD_VNODE:
		mtx_lock(&Giant);
		hasgiant = 1;
		break;
	case MD_MALLOC:
	case MD_PRELOAD:
	case MD_SWAP:
	default:
		hasgiant = 0;
		break;
	}
	
	for (;;) {
		mtx_lock(&sc->queue_mtx);
		bp = bioq_takefirst(&sc->bio_queue);
		if (!bp) {
			if (sc->flags & MD_SHUTDOWN) {
				mtx_unlock(&sc->queue_mtx);
				sc->procp = NULL;
				wakeup(&sc->procp);
				if (hasgiant)
					mtx_unlock(&Giant);
				kthread_exit(0);
			}
			msleep(sc, &sc->queue_mtx, PRIBIO | PDROP, "mdwait", 0);
			continue;
		}
		mtx_unlock(&sc->queue_mtx);
		if (bp->bio_cmd == BIO_GETATTR) {
			if (sc->fwsectors && sc->fwheads &&
			    (g_handleattr_int(bp, "GEOM::fwsectors",
			    sc->fwsectors) ||
			    g_handleattr_int(bp, "GEOM::fwheads",
			    sc->fwheads)))
				error = -1;
			else
				error = EOPNOTSUPP;
		} else {
			switch (sc->type) {
			case MD_MALLOC:
				error = mdstart_malloc(sc, bp);
				break;
			case MD_PRELOAD:
				error = mdstart_preload(sc, bp);
				break;
			case MD_VNODE:
				mtx_assert(&Giant, MA_OWNED);
				error = mdstart_vnode(sc, bp);
				mtx_assert(&Giant, MA_OWNED);
				break;
			case MD_SWAP:
				error = mdstart_swap(sc, bp);
				break;
			default:
				panic("Impossible md(type)");
				break;
			}
		}

		if (error != -1) {
			bp->bio_completed = bp->bio_length;
			g_io_deliver(bp, error);
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
	sc = (struct md_s *)malloc(sizeof *sc, M_MD, M_WAITOK | M_ZERO);
	sc->unit = unit;
	bioq_init(&sc->bio_queue);
	mtx_init(&sc->queue_mtx, "md bio queue", NULL, MTX_DEF);
	sprintf(sc->name, "md%d", unit);
	error = kthread_create(md_kthread, sc, &sc->procp, 0, 0,"%s", sc->name);
	if (error != 0) {
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

	struct g_geom *gp;
	struct g_provider *pp;

	DROP_GIANT();
	g_topology_lock();
	gp = g_new_geomf(&g_md_class, "md%d", sc->unit);
	gp->softc = sc;
	pp = g_new_providerf(gp, "md%d", sc->unit);
	pp->mediasize = sc->mediasize;
	pp->sectorsize = sc->sectorsize;
	sc->gp = gp;
	sc->pp = pp;
	g_error_provider(pp, 0);
	g_topology_unlock();
	PICKUP_GIANT();
}

/*
 * XXX: we should check that the range they feed us is mapped.
 * XXX: we should implement read-only.
 */

static int
mdcreate_preload(struct md_s *sc, struct md_ioctl *mdio)
{

	if (mdio->md_options & ~(MD_AUTOUNIT | MD_FORCE))
		return (EINVAL);
	sc->flags = mdio->md_options & MD_FORCE;
	/* Cast to pointer size, then to pointer to avoid warning */
	sc->pl_ptr = (u_char *)(uintptr_t)mdio->md_base;
	sc->pl_len = (size_t)sc->mediasize;
	return (0);
}


static int
mdcreate_malloc(struct md_s *sc, struct md_ioctl *mdio)
{
	uintptr_t sp;
	int error;
	off_t u;

	error = 0;
	if (mdio->md_options & ~(MD_AUTOUNIT | MD_COMPRESS | MD_RESERVE))
		return (EINVAL);
	if (mdio->md_sectorsize != 0 && !powerof2(mdio->md_sectorsize))
		return (EINVAL);
	/* Compression doesn't make sense if we have reserved space */
	if (mdio->md_options & MD_RESERVE)
		mdio->md_options &= ~MD_COMPRESS;
	if (mdio->md_fwsectors != 0)
		sc->fwsectors = mdio->md_fwsectors;
	if (mdio->md_fwheads != 0)
		sc->fwheads = mdio->md_fwheads;
	sc->flags = mdio->md_options & (MD_COMPRESS | MD_FORCE);
	sc->indir = dimension(sc->mediasize / sc->sectorsize);
	sc->uma = uma_zcreate(sc->name, sc->sectorsize, NULL, NULL, NULL, NULL,
	    0x1ff, 0);
	if (mdio->md_options & MD_RESERVE) {
		off_t nsectors;

		nsectors = sc->mediasize / sc->sectorsize;
		for (u = 0; u < nsectors; u++) {
			sp = (uintptr_t)uma_zalloc(sc->uma, M_NOWAIT | M_ZERO);
			if (sp != 0)
				error = s_write(sc->indir, u, sp);
			else
				error = ENOMEM;
			if (error != 0)
				break;
		}
	}
	if (error != 0)
		uma_zdestroy(sc->uma);
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

		tmpbuf = malloc(sc->sectorsize, M_TEMP, M_WAITOK);
		bzero(&auio, sizeof(auio));

		aiov.iov_base = tmpbuf;
		aiov.iov_len = sc->sectorsize;
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
mdcreate_vnode(struct md_s *sc, struct md_ioctl *mdio, struct thread *td)
{
	struct vattr vattr;
	struct nameidata nd;
	int error, flags;

	error = copyinstr(mdio->md_file, sc->file, sizeof(sc->file), NULL);
	if (error != 0)
		return (error);
	flags = FREAD|FWRITE;
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, sc->file, td);
	error = vn_open(&nd, &flags, 0, -1);
	if (error != 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		if (error != EACCES && error != EPERM && error != EROFS)
			return (error);
		flags &= ~FWRITE;
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, sc->file, td);
		error = vn_open(&nd, &flags, 0, -1);
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (error != 0)
		return (error);
	if (nd.ni_vp->v_type != VREG ||
	    (error = VOP_GETATTR(nd.ni_vp, &vattr, td->td_ucred, td))) {
		VOP_UNLOCK(nd.ni_vp, 0, td);
		(void)vn_close(nd.ni_vp, flags, td->td_ucred, td);
		return (error ? error : EINVAL);
	}
	VOP_UNLOCK(nd.ni_vp, 0, td);

	if (mdio->md_fwsectors != 0)
		sc->fwsectors = mdio->md_fwsectors;
	if (mdio->md_fwheads != 0)
		sc->fwheads = mdio->md_fwheads;
	sc->flags = mdio->md_options & (MD_FORCE | MD_ASYNC);
	if (!(flags & FWRITE))
		sc->flags |= MD_READONLY;
	sc->vnode = nd.ni_vp;

	error = mdsetcred(sc, td->td_ucred);
	if (error != 0) {
		(void)vn_close(nd.ni_vp, flags, td->td_ucred, td);
		return (error);
	}
	return (0);
}

static void
md_zapit(void *p, int cancel)
{
	if (cancel)
		return;
	g_wither_geom(p, ENXIO);
}

static int
mddestroy(struct md_s *sc, struct thread *td)
{

	GIANT_REQUIRED;

	mtx_destroy(&sc->queue_mtx);
	if (sc->gp) {
		sc->gp->softc = NULL;
		g_waitfor_event(md_zapit, sc->gp, M_WAITOK, sc->gp, NULL);
		sc->gp = NULL;
		sc->pp = NULL;
	}
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
		vm_object_deallocate(sc->object);
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
mdcreate_swap(struct md_s *sc, struct md_ioctl *mdio, struct thread *td)
{
	vm_ooffset_t npage;
	int error;

	GIANT_REQUIRED;

	/*
	 * Range check.  Disallow negative sizes or any size less then the
	 * size of a page.  Then round to a page.
	 */
	if (sc->mediasize == 0 || (sc->mediasize % PAGE_SIZE) != 0)
		return (EDOM);

	/*
	 * Allocate an OBJT_SWAP object.
	 *
	 * Note the truncation.
	 */

	npage = mdio->md_mediasize / PAGE_SIZE;
	if (mdio->md_fwsectors != 0)
		sc->fwsectors = mdio->md_fwsectors;
	if (mdio->md_fwheads != 0)
		sc->fwheads = mdio->md_fwheads;
	sc->object = vm_pager_allocate(OBJT_SWAP, NULL, PAGE_SIZE * npage,
	    VM_PROT_DEFAULT, 0);
	if (sc->object == NULL)
		return (ENOMEM);
	sc->flags = mdio->md_options & MD_FORCE;
	if (mdio->md_options & MD_RESERVE) {
		if (swap_pager_reserve(sc->object, 0, npage) < 0) {
			vm_object_deallocate(sc->object);
			sc->object = NULL;
			return (EDOM);
		}
	}
	error = mdsetcred(sc, td->td_ucred);
	if (error != 0) {
		vm_object_deallocate(sc->object);
		sc->object = NULL;
	}
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
mdctlioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flags, struct thread *td)
{
	struct md_ioctl *mdio;
	struct md_s *sc;
	int error, i;

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
		case MD_PRELOAD:
		case MD_VNODE:
		case MD_SWAP:
			break;
		default:
			return (EINVAL);
		}
		if (mdio->md_options & MD_AUTOUNIT) {
			sc = mdnew(-1);
			mdio->md_unit = sc->unit;
		} else {
			sc = mdnew(mdio->md_unit);
			if (sc == NULL)
				return (EBUSY);
		}
		sc->type = mdio->md_type;
		sc->mediasize = mdio->md_mediasize;
		if (mdio->md_sectorsize == 0)
			sc->sectorsize = DEV_BSIZE;
		else
			sc->sectorsize = mdio->md_sectorsize;
		error = EDOOFUS;
		switch (sc->type) {
		case MD_MALLOC:
			error = mdcreate_malloc(sc, mdio);
			break;
		case MD_PRELOAD:
			error = mdcreate_preload(sc, mdio);
			break;
		case MD_VNODE:
			error = mdcreate_vnode(sc, mdio, td);
			break;
		case MD_SWAP:
			error = mdcreate_swap(sc, mdio, td);
			break;
		}
		if (error != 0) {
			mddestroy(sc, td);
			return (error);
		}
		mdinit(sc);
		return (0);
	case MDIOCDETACH:
		if (mdio->md_version != MDIOVERSION)
			return (EINVAL);
		if (mdio->md_mediasize != 0 || mdio->md_options != 0)
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
		mdio->md_mediasize = sc->mediasize;
		mdio->md_sectorsize = sc->sectorsize;
		if (sc->type == MD_VNODE) {
			error = copyout(sc->file, mdio->md_file,
			    strlen(sc->file) + 1);
			if (error != 0)
				return (error);
		}
		return (0);
	case MDIOCLIST:
		if (mdio->md_version != MDIOVERSION)
			return (EINVAL);
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
md_preloaded(u_char *image, size_t length)
{
	struct md_s *sc;

	sc = mdnew(-1);
	if (sc == NULL)
		return;
	sc->type = MD_PRELOAD;
	sc->mediasize = length;
	sc->sectorsize = DEV_BSIZE;
	sc->pl_ptr = image;
	sc->pl_len = length;
#ifdef MD_ROOT
	if (sc->unit == 0)
		rootdevnames[0] = "ufs:/dev/md0";
#endif
	mdinit(sc);
}

static void
g_md_init(struct g_class *mp __unused)
{

	caddr_t mod;
	caddr_t c;
	u_char *ptr, *name, *type;
	unsigned len;

	mod = NULL;
	g_topology_unlock();
#ifdef MD_ROOT_SIZE
	md_preloaded(mfs_root, MD_ROOT_SIZE*1024);
#endif
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
	g_topology_lock();
}

static void
g_md_fini(struct g_class *mp __unused)
{

	if (status_dev != NULL)
		destroy_dev(status_dev);
}
