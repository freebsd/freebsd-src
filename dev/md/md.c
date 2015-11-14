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
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
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
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/limits.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mdioctl.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/rwlock.h>
#include <sys/sbuf.h>
#include <sys/sched.h>
#include <sys/sf_buf.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <geom/geom.h>
#include <geom/geom_int.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/swap_pager.h>
#include <vm/uma.h>

#define MD_MODVER 1

#define MD_SHUTDOWN	0x10000		/* Tell worker thread to terminate. */
#define	MD_EXITING	0x20000		/* Worker thread is exiting. */

#ifndef MD_NSECT
#define MD_NSECT (10000 * 2)
#endif

static MALLOC_DEFINE(M_MD, "md_disk", "Memory Disk");
static MALLOC_DEFINE(M_MDSECT, "md_sectors", "Memory Disk Sectors");

static int md_debug;
SYSCTL_INT(_debug, OID_AUTO, mddebug, CTLFLAG_RW, &md_debug, 0,
    "Enable md(4) debug messages");
static int md_malloc_wait;
SYSCTL_INT(_vm, OID_AUTO, md_malloc_wait, CTLFLAG_RW, &md_malloc_wait, 0,
    "Allow malloc to wait for memory allocations");

#if defined(MD_ROOT) && !defined(MD_ROOT_FSTYPE)
#define	MD_ROOT_FSTYPE	"ufs"
#endif

#if defined(MD_ROOT)
/*
 * Preloaded image gets put here.
 */
#if defined(MD_ROOT_SIZE)
/*
 * Applications that patch the object with the image can determine
 * the size looking at the start and end markers (strings),
 * so we want them contiguous.
 */
static struct {
	u_char start[MD_ROOT_SIZE*1024];
	u_char end[128];
} mfs_root = {
	.start = "MFS Filesystem goes here",
	.end = "MFS Filesystem had better STOP here",
};
const int mfs_root_size = sizeof(mfs_root.start);
#else
extern volatile u_char __weak_symbol mfs_root;
extern volatile u_char __weak_symbol mfs_root_end;
__GLOBL(mfs_root);
__GLOBL(mfs_root_end);
#define mfs_root_size ((uintptr_t)(&mfs_root_end - &mfs_root))
#endif
#endif

static g_init_t g_md_init;
static g_fini_t g_md_fini;
static g_start_t g_md_start;
static g_access_t g_md_access;
static void g_md_dumpconf(struct sbuf *sb, const char *indent,
    struct g_geom *gp, struct g_consumer *cp __unused, struct g_provider *pp);

static struct cdev *status_dev = 0;
static struct sx md_sx;
static struct unrhdr *md_uh;

static d_ioctl_t mdctlioctl;

static struct cdevsw mdctl_cdevsw = {
	.d_version =	D_VERSION,
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
	.dumpconf = g_md_dumpconf,
};

DECLARE_GEOM_CLASS(g_md_class, g_md);


static LIST_HEAD(, md_s) md_softc_list = LIST_HEAD_INITIALIZER(md_softc_list);

#define NINDIR	(PAGE_SIZE / sizeof(uintptr_t))
#define NMASK	(NINDIR-1)
static int nshift;

static int md_vnode_pbuf_freecnt;

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
	struct mtx stat_mtx;
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
	int (*start)(struct md_s *sc, struct bio *bp);
	struct devstat *devstat;

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

static struct indir *
new_indir(u_int shift)
{
	struct indir *ip;

	ip = malloc(sizeof *ip, M_MD, (md_malloc_wait ? M_WAITOK : M_NOWAIT)
	    | M_ZERO);
	if (ip == NULL)
		return (NULL);
	ip->array = malloc(sizeof(uintptr_t) * NINDIR,
	    M_MDSECT, (md_malloc_wait ? M_WAITOK : M_NOWAIT) | M_ZERO);
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
 * This function does the math and allocates the top level "indir" structure
 * for a device of "size" sectors.
 */

static struct indir *
dimension(off_t size)
{
	off_t rcnt;
	struct indir *ip;
	int layer;

	rcnt = size;
	layer = 0;
	while (rcnt > NINDIR) {
		rcnt /= NINDIR;
		layer++;
	}

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
	if (sc == NULL) {
		if (r <= 0 && w <= 0 && e <= 0)
			return (0);
		return (ENXIO);
	}
	r += pp->acr;
	w += pp->acw;
	e += pp->ace;
	if ((sc->flags & MD_READONLY) != 0 && w > 0)
		return (EROFS);
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
	if ((bp->bio_cmd == BIO_READ) || (bp->bio_cmd == BIO_WRITE)) {
		mtx_lock(&sc->stat_mtx);
		devstat_start_transaction_bio(sc->devstat, bp);
		mtx_unlock(&sc->stat_mtx);
	}
	mtx_lock(&sc->queue_mtx);
	bioq_disksort(&sc->bio_queue, bp);
	mtx_unlock(&sc->queue_mtx);
	wakeup(sc);
}

#define	MD_MALLOC_MOVE_ZERO	1
#define	MD_MALLOC_MOVE_FILL	2
#define	MD_MALLOC_MOVE_READ	3
#define	MD_MALLOC_MOVE_WRITE	4
#define	MD_MALLOC_MOVE_CMP	5

static int
md_malloc_move(vm_page_t **mp, int *ma_offs, unsigned sectorsize,
    void *ptr, u_char fill, int op)
{
	struct sf_buf *sf;
	vm_page_t m, *mp1;
	char *p, first;
	off_t *uc;
	unsigned n;
	int error, i, ma_offs1, sz, first_read;

	m = NULL;
	error = 0;
	sf = NULL;
	/* if (op == MD_MALLOC_MOVE_CMP) { gcc */
		first = 0;
		first_read = 0;
		uc = ptr;
		mp1 = *mp;
		ma_offs1 = *ma_offs;
	/* } */
	sched_pin();
	for (n = sectorsize; n != 0; n -= sz) {
		sz = imin(PAGE_SIZE - *ma_offs, n);
		if (m != **mp) {
			if (sf != NULL)
				sf_buf_free(sf);
			m = **mp;
			sf = sf_buf_alloc(m, SFB_CPUPRIVATE |
			    (md_malloc_wait ? 0 : SFB_NOWAIT));
			if (sf == NULL) {
				error = ENOMEM;
				break;
			}
		}
		p = (char *)sf_buf_kva(sf) + *ma_offs;
		switch (op) {
		case MD_MALLOC_MOVE_ZERO:
			bzero(p, sz);
			break;
		case MD_MALLOC_MOVE_FILL:
			memset(p, fill, sz);
			break;
		case MD_MALLOC_MOVE_READ:
			bcopy(ptr, p, sz);
			cpu_flush_dcache(p, sz);
			break;
		case MD_MALLOC_MOVE_WRITE:
			bcopy(p, ptr, sz);
			break;
		case MD_MALLOC_MOVE_CMP:
			for (i = 0; i < sz; i++, p++) {
				if (!first_read) {
					*uc = (u_char)*p;
					first = *p;
					first_read = 1;
				} else if (*p != first) {
					error = EDOOFUS;
					break;
				}
			}
			break;
		default:
			KASSERT(0, ("md_malloc_move unknown op %d\n", op));
			break;
		}
		if (error != 0)
			break;
		*ma_offs += sz;
		*ma_offs %= PAGE_SIZE;
		if (*ma_offs == 0)
			(*mp)++;
		ptr = (char *)ptr + sz;
	}

	if (sf != NULL)
		sf_buf_free(sf);
	sched_unpin();
	if (op == MD_MALLOC_MOVE_CMP && error != 0) {
		*mp = mp1;
		*ma_offs = ma_offs1;
	}
	return (error);
}

static int
mdstart_malloc(struct md_s *sc, struct bio *bp)
{
	u_char *dst;
	vm_page_t *m;
	int i, error, error1, ma_offs, notmapped;
	off_t secno, nsec, uc;
	uintptr_t sp, osp;

	switch (bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		break;
	default:
		return (EOPNOTSUPP);
	}

	notmapped = (bp->bio_flags & BIO_UNMAPPED) != 0;
	if (notmapped) {
		m = bp->bio_ma;
		ma_offs = bp->bio_ma_offset;
		dst = NULL;
	} else {
		dst = bp->bio_data;
	}

	nsec = bp->bio_length / sc->sectorsize;
	secno = bp->bio_offset / sc->sectorsize;
	error = 0;
	while (nsec--) {
		osp = s_read(sc->indir, secno);
		if (bp->bio_cmd == BIO_DELETE) {
			if (osp != 0)
				error = s_write(sc->indir, secno, 0);
		} else if (bp->bio_cmd == BIO_READ) {
			if (osp == 0) {
				if (notmapped) {
					error = md_malloc_move(&m, &ma_offs,
					    sc->sectorsize, NULL, 0,
					    MD_MALLOC_MOVE_ZERO);
				} else
					bzero(dst, sc->sectorsize);
			} else if (osp <= 255) {
				if (notmapped) {
					error = md_malloc_move(&m, &ma_offs,
					    sc->sectorsize, NULL, osp,
					    MD_MALLOC_MOVE_FILL);
				} else
					memset(dst, osp, sc->sectorsize);
			} else {
				if (notmapped) {
					error = md_malloc_move(&m, &ma_offs,
					    sc->sectorsize, (void *)osp, 0,
					    MD_MALLOC_MOVE_READ);
				} else {
					bcopy((void *)osp, dst, sc->sectorsize);
					cpu_flush_dcache(dst, sc->sectorsize);
				}
			}
			osp = 0;
		} else if (bp->bio_cmd == BIO_WRITE) {
			if (sc->flags & MD_COMPRESS) {
				if (notmapped) {
					error1 = md_malloc_move(&m, &ma_offs,
					    sc->sectorsize, &uc, 0,
					    MD_MALLOC_MOVE_CMP);
					i = error1 == 0 ? sc->sectorsize : 0;
				} else {
					uc = dst[0];
					for (i = 1; i < sc->sectorsize; i++) {
						if (dst[i] != uc)
							break;
					}
				}
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
					    md_malloc_wait ? M_WAITOK :
					    M_NOWAIT);
					if (sp == 0) {
						error = ENOSPC;
						break;
					}
					if (notmapped) {
						error = md_malloc_move(&m,
						    &ma_offs, sc->sectorsize,
						    (void *)sp, 0,
						    MD_MALLOC_MOVE_WRITE);
					} else {
						bcopy(dst, (void *)sp,
						    sc->sectorsize);
					}
					error = s_write(sc->indir, secno, sp);
				} else {
					if (notmapped) {
						error = md_malloc_move(&m,
						    &ma_offs, sc->sectorsize,
						    (void *)osp, 0,
						    MD_MALLOC_MOVE_WRITE);
					} else {
						bcopy(dst, (void *)osp,
						    sc->sectorsize);
					}
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
		if (!notmapped)
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
		cpu_flush_dcache(bp->bio_data, bp->bio_length);
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
	struct vnode *vp;
	struct buf *pb;
	struct thread *td;
	off_t end, zerosize;

	switch (bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
	case BIO_FLUSH:
		break;
	default:
		return (EOPNOTSUPP);
	}

	td = curthread;
	vp = sc->vnode;

	/*
	 * VNODE I/O
	 *
	 * If an error occurs, we set BIO_ERROR but we do not set
	 * B_INVAL because (for a write anyway), the buffer is
	 * still valid.
	 */

	if (bp->bio_cmd == BIO_FLUSH) {
		(void) vn_start_write(vp, &mp, V_WAIT);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		error = VOP_FSYNC(vp, MNT_WAIT, td);
		VOP_UNLOCK(vp, 0);
		vn_finished_write(mp);
		return (error);
	}

	bzero(&auio, sizeof(auio));

	/*
	 * Special case for BIO_DELETE.  On the surface, this is very
	 * similar to BIO_WRITE, except that we write from our own
	 * fixed-length buffer, so we have to loop.  The net result is
	 * that the two cases end up having very little in common.
	 */
	if (bp->bio_cmd == BIO_DELETE) {
		zerosize = ZERO_REGION_SIZE -
		    (ZERO_REGION_SIZE % sc->sectorsize);
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = (vm_ooffset_t)bp->bio_offset;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_rw = UIO_WRITE;
		auio.uio_td = td;
		end = bp->bio_offset + bp->bio_length;
		(void) vn_start_write(vp, &mp, V_WAIT);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		error = 0;
		while (auio.uio_offset < end) {
			aiov.iov_base = __DECONST(void *, zero_region);
			aiov.iov_len = end - auio.uio_offset;
			if (aiov.iov_len > zerosize)
				aiov.iov_len = zerosize;
			auio.uio_resid = aiov.iov_len;
			error = VOP_WRITE(vp, &auio,
			    sc->flags & MD_ASYNC ? 0 : IO_SYNC, sc->cred);
			if (error != 0)
				break;
		}
		VOP_UNLOCK(vp, 0);
		vn_finished_write(mp);
		bp->bio_resid = end - auio.uio_offset;
		return (error);
	}

	if ((bp->bio_flags & BIO_UNMAPPED) == 0) {
		pb = NULL;
		aiov.iov_base = bp->bio_data;
	} else {
		KASSERT(bp->bio_length <= MAXPHYS, ("bio_length %jd",
		    (uintmax_t)bp->bio_length));
		pb = getpbuf(&md_vnode_pbuf_freecnt);
		pmap_qenter((vm_offset_t)pb->b_data, bp->bio_ma, bp->bio_ma_n);
		aiov.iov_base = (void *)((vm_offset_t)pb->b_data +
		    bp->bio_ma_offset);
	}
	aiov.iov_len = bp->bio_length;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = (vm_ooffset_t)bp->bio_offset;
	auio.uio_segflg = UIO_SYSSPACE;
	if (bp->bio_cmd == BIO_READ)
		auio.uio_rw = UIO_READ;
	else if (bp->bio_cmd == BIO_WRITE)
		auio.uio_rw = UIO_WRITE;
	else
		panic("wrong BIO_OP in mdstart_vnode");
	auio.uio_resid = bp->bio_length;
	auio.uio_td = td;
	/*
	 * When reading set IO_DIRECT to try to avoid double-caching
	 * the data.  When writing IO_DIRECT is not optimal.
	 */
	if (bp->bio_cmd == BIO_READ) {
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		error = VOP_READ(vp, &auio, IO_DIRECT, sc->cred);
		VOP_UNLOCK(vp, 0);
	} else {
		(void) vn_start_write(vp, &mp, V_WAIT);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		error = VOP_WRITE(vp, &auio, sc->flags & MD_ASYNC ? 0 : IO_SYNC,
		    sc->cred);
		VOP_UNLOCK(vp, 0);
		vn_finished_write(mp);
	}
	if ((bp->bio_flags & BIO_UNMAPPED) != 0) {
		pmap_qremove((vm_offset_t)pb->b_data, bp->bio_ma_n);
		relpbuf(pb, &md_vnode_pbuf_freecnt);
	}
	bp->bio_resid = auio.uio_resid;
	return (error);
}

static int
mdstart_swap(struct md_s *sc, struct bio *bp)
{
	vm_page_t m;
	u_char *p;
	vm_pindex_t i, lastp;
	int rv, ma_offs, offs, len, lastend;

	switch (bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		break;
	default:
		return (EOPNOTSUPP);
	}

	p = bp->bio_data;
	ma_offs = (bp->bio_flags & BIO_UNMAPPED) == 0 ? 0 : bp->bio_ma_offset;

	/*
	 * offs is the offset at which to start operating on the
	 * next (ie, first) page.  lastp is the last page on
	 * which we're going to operate.  lastend is the ending
	 * position within that last page (ie, PAGE_SIZE if
	 * we're operating on complete aligned pages).
	 */
	offs = bp->bio_offset % PAGE_SIZE;
	lastp = (bp->bio_offset + bp->bio_length - 1) / PAGE_SIZE;
	lastend = (bp->bio_offset + bp->bio_length - 1) % PAGE_SIZE + 1;

	rv = VM_PAGER_OK;
	VM_OBJECT_WLOCK(sc->object);
	vm_object_pip_add(sc->object, 1);
	for (i = bp->bio_offset / PAGE_SIZE; i <= lastp; i++) {
		len = ((i == lastp) ? lastend : PAGE_SIZE) - offs;
		m = vm_page_grab(sc->object, i, VM_ALLOC_SYSTEM);
		if (bp->bio_cmd == BIO_READ) {
			if (m->valid == VM_PAGE_BITS_ALL)
				rv = VM_PAGER_OK;
			else
				rv = vm_pager_get_pages(sc->object, &m, 1, 0);
			if (rv == VM_PAGER_ERROR) {
				vm_page_xunbusy(m);
				break;
			} else if (rv == VM_PAGER_FAIL) {
				/*
				 * Pager does not have the page.  Zero
				 * the allocated page, and mark it as
				 * valid. Do not set dirty, the page
				 * can be recreated if thrown out.
				 */
				pmap_zero_page(m);
				m->valid = VM_PAGE_BITS_ALL;
			}
			if ((bp->bio_flags & BIO_UNMAPPED) != 0) {
				pmap_copy_pages(&m, offs, bp->bio_ma,
				    ma_offs, len);
			} else {
				physcopyout(VM_PAGE_TO_PHYS(m) + offs, p, len);
				cpu_flush_dcache(p, len);
			}
		} else if (bp->bio_cmd == BIO_WRITE) {
			if (len != PAGE_SIZE && m->valid != VM_PAGE_BITS_ALL)
				rv = vm_pager_get_pages(sc->object, &m, 1, 0);
			else
				rv = VM_PAGER_OK;
			if (rv == VM_PAGER_ERROR) {
				vm_page_xunbusy(m);
				break;
			}
			if ((bp->bio_flags & BIO_UNMAPPED) != 0) {
				pmap_copy_pages(bp->bio_ma, ma_offs, &m,
				    offs, len);
			} else {
				physcopyin(p, VM_PAGE_TO_PHYS(m) + offs, len);
			}
			m->valid = VM_PAGE_BITS_ALL;
		} else if (bp->bio_cmd == BIO_DELETE) {
			if (len != PAGE_SIZE && m->valid != VM_PAGE_BITS_ALL)
				rv = vm_pager_get_pages(sc->object, &m, 1, 0);
			else
				rv = VM_PAGER_OK;
			if (rv == VM_PAGER_ERROR) {
				vm_page_xunbusy(m);
				break;
			}
			if (len != PAGE_SIZE) {
				pmap_zero_page_area(m, offs, len);
				vm_page_clear_dirty(m, offs, len);
				m->valid = VM_PAGE_BITS_ALL;
			} else
				vm_pager_page_unswapped(m);
		}
		vm_page_xunbusy(m);
		vm_page_lock(m);
		if (bp->bio_cmd == BIO_DELETE && len == PAGE_SIZE)
			vm_page_free(m);
		else
			vm_page_activate(m);
		vm_page_unlock(m);
		if (bp->bio_cmd == BIO_WRITE) {
			vm_page_dirty(m);
			vm_pager_page_unswapped(m);
		}

		/* Actions on further pages start at offset 0 */
		p += PAGE_SIZE - offs;
		offs = 0;
		ma_offs += len;
	}
	vm_object_pip_wakeup(sc->object);
	VM_OBJECT_WUNLOCK(sc->object);
	return (rv != VM_PAGER_ERROR ? 0 : ENOSPC);
}

static int
mdstart_null(struct md_s *sc, struct bio *bp)
{

	switch (bp->bio_cmd) {
	case BIO_READ:
		bzero(bp->bio_data, bp->bio_length);
		cpu_flush_dcache(bp->bio_data, bp->bio_length);
		break;
	case BIO_WRITE:
		break;
	}
	bp->bio_resid = 0;
	return (0);
}

static void
md_kthread(void *arg)
{
	struct md_s *sc;
	struct bio *bp;
	int error;

	sc = arg;
	thread_lock(curthread);
	sched_prio(curthread, PRIBIO);
	thread_unlock(curthread);
	if (sc->type == MD_VNODE)
		curthread->td_pflags |= TDP_NORUNNINGBUF;

	for (;;) {
		mtx_lock(&sc->queue_mtx);
		if (sc->flags & MD_SHUTDOWN) {
			sc->flags |= MD_EXITING;
			mtx_unlock(&sc->queue_mtx);
			kproc_exit(0);
		}
		bp = bioq_takefirst(&sc->bio_queue);
		if (!bp) {
			msleep(sc, &sc->queue_mtx, PRIBIO | PDROP, "mdwait", 0);
			continue;
		}
		mtx_unlock(&sc->queue_mtx);
		if (bp->bio_cmd == BIO_GETATTR) {
			if ((sc->fwsectors && sc->fwheads &&
			    (g_handleattr_int(bp, "GEOM::fwsectors",
			    sc->fwsectors) ||
			    g_handleattr_int(bp, "GEOM::fwheads",
			    sc->fwheads))) ||
			    g_handleattr_int(bp, "GEOM::candelete", 1))
				error = -1;
			else
				error = EOPNOTSUPP;
		} else {
			error = sc->start(sc, bp);
		}

		if (error != -1) {
			bp->bio_completed = bp->bio_length;
			if ((bp->bio_cmd == BIO_READ) || (bp->bio_cmd == BIO_WRITE))
				devstat_end_transaction_bio(sc->devstat, bp);
			g_io_deliver(bp, error);
		}
	}
}

static struct md_s *
mdfind(int unit)
{
	struct md_s *sc;

	LIST_FOREACH(sc, &md_softc_list, list) {
		if (sc->unit == unit)
			break;
	}
	return (sc);
}

static struct md_s *
mdnew(int unit, int *errp, enum md_types type)
{
	struct md_s *sc;
	int error;

	*errp = 0;
	if (unit == -1)
		unit = alloc_unr(md_uh);
	else
		unit = alloc_unr_specific(md_uh, unit);

	if (unit == -1) {
		*errp = EBUSY;
		return (NULL);
	}

	sc = (struct md_s *)malloc(sizeof *sc, M_MD, M_WAITOK | M_ZERO);
	sc->type = type;
	bioq_init(&sc->bio_queue);
	mtx_init(&sc->queue_mtx, "md bio queue", NULL, MTX_DEF);
	mtx_init(&sc->stat_mtx, "md stat", NULL, MTX_DEF);
	sc->unit = unit;
	sprintf(sc->name, "md%d", unit);
	LIST_INSERT_HEAD(&md_softc_list, sc, list);
	error = kproc_create(md_kthread, sc, &sc->procp, 0, 0,"%s", sc->name);
	if (error == 0)
		return (sc);
	LIST_REMOVE(sc, list);
	mtx_destroy(&sc->stat_mtx);
	mtx_destroy(&sc->queue_mtx);
	free_unr(md_uh, sc->unit);
	free(sc, M_MD);
	*errp = error;
	return (NULL);
}

static void
mdinit(struct md_s *sc)
{
	struct g_geom *gp;
	struct g_provider *pp;

	g_topology_lock();
	gp = g_new_geomf(&g_md_class, "md%d", sc->unit);
	gp->softc = sc;
	pp = g_new_providerf(gp, "md%d", sc->unit);
	pp->flags |= G_PF_DIRECT_SEND | G_PF_DIRECT_RECEIVE;
	pp->mediasize = sc->mediasize;
	pp->sectorsize = sc->sectorsize;
	switch (sc->type) {
	case MD_MALLOC:
	case MD_VNODE:
	case MD_SWAP:
		pp->flags |= G_PF_ACCEPT_UNMAPPED;
		break;
	case MD_PRELOAD:
	case MD_NULL:
		break;
	}
	sc->gp = gp;
	sc->pp = pp;
	g_error_provider(pp, 0);
	g_topology_unlock();
	sc->devstat = devstat_new_entry("md", sc->unit, sc->sectorsize,
	    DEVSTAT_ALL_SUPPORTED, DEVSTAT_TYPE_DIRECT, DEVSTAT_PRIORITY_MAX);
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
			sp = (uintptr_t)uma_zalloc(sc->uma, (md_malloc_wait ?
			    M_WAITOK : M_NOWAIT) | M_ZERO);
			if (sp != 0)
				error = s_write(sc->indir, u, sp);
			else
				error = ENOMEM;
			if (error != 0)
				break;
		}
	}
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
		vn_lock(sc->vnode, LK_EXCLUSIVE | LK_RETRY);
		error = VOP_READ(sc->vnode, &auio, 0, sc->cred);
		VOP_UNLOCK(sc->vnode, 0);
		free(tmpbuf, M_TEMP);
	}
	return (error);
}

static int
mdcreate_vnode(struct md_s *sc, struct md_ioctl *mdio, struct thread *td)
{
	struct vattr vattr;
	struct nameidata nd;
	char *fname;
	int error, flags;

	/*
	 * Kernel-originated requests must have the filename appended
	 * to the mdio structure to protect against malicious software.
	 */
	fname = mdio->md_file;
	if ((void *)fname != (void *)(mdio + 1)) {
		error = copyinstr(fname, sc->file, sizeof(sc->file), NULL);
		if (error != 0)
			return (error);
	} else
		strlcpy(sc->file, fname, sizeof(sc->file));

	/*
	 * If the user specified that this is a read only device, don't
	 * set the FWRITE mask before trying to open the backing store.
	 */
	flags = FREAD | ((mdio->md_options & MD_READONLY) ? 0 : FWRITE);
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, sc->file, td);
	error = vn_open(&nd, &flags, 0, NULL);
	if (error != 0)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (nd.ni_vp->v_type != VREG) {
		error = EINVAL;
		goto bad;
	}
	error = VOP_GETATTR(nd.ni_vp, &vattr, td->td_ucred);
	if (error != 0)
		goto bad;
	if (VOP_ISLOCKED(nd.ni_vp) != LK_EXCLUSIVE) {
		vn_lock(nd.ni_vp, LK_UPGRADE | LK_RETRY);
		if (nd.ni_vp->v_iflag & VI_DOOMED) {
			/* Forced unmount. */
			error = EBADF;
			goto bad;
		}
	}
	nd.ni_vp->v_vflag |= VV_MD;
	VOP_UNLOCK(nd.ni_vp, 0);

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
		sc->vnode = NULL;
		vn_lock(nd.ni_vp, LK_EXCLUSIVE | LK_RETRY);
		nd.ni_vp->v_vflag &= ~VV_MD;
		goto bad;
	}
	return (0);
bad:
	VOP_UNLOCK(nd.ni_vp, 0);
	(void)vn_close(nd.ni_vp, flags, td->td_ucred, td);
	return (error);
}

static int
mddestroy(struct md_s *sc, struct thread *td)
{

	if (sc->gp) {
		sc->gp->softc = NULL;
		g_topology_lock();
		g_wither_geom(sc->gp, ENXIO);
		g_topology_unlock();
		sc->gp = NULL;
		sc->pp = NULL;
	}
	if (sc->devstat) {
		devstat_remove_entry(sc->devstat);
		sc->devstat = NULL;
	}
	mtx_lock(&sc->queue_mtx);
	sc->flags |= MD_SHUTDOWN;
	wakeup(sc);
	while (!(sc->flags & MD_EXITING))
		msleep(sc->procp, &sc->queue_mtx, PRIBIO, "mddestroy", hz / 10);
	mtx_unlock(&sc->queue_mtx);
	mtx_destroy(&sc->stat_mtx);
	mtx_destroy(&sc->queue_mtx);
	if (sc->vnode != NULL) {
		vn_lock(sc->vnode, LK_EXCLUSIVE | LK_RETRY);
		sc->vnode->v_vflag &= ~VV_MD;
		VOP_UNLOCK(sc->vnode, 0);
		(void)vn_close(sc->vnode, sc->flags & MD_READONLY ?
		    FREAD : (FREAD|FWRITE), sc->cred, td);
	}
	if (sc->cred != NULL)
		crfree(sc->cred);
	if (sc->object != NULL)
		vm_object_deallocate(sc->object);
	if (sc->indir)
		destroy_indir(sc, sc->indir);
	if (sc->uma)
		uma_zdestroy(sc->uma);

	LIST_REMOVE(sc, list);
	free_unr(md_uh, sc->unit);
	free(sc, M_MD);
	return (0);
}

static int
mdresize(struct md_s *sc, struct md_ioctl *mdio)
{
	int error, res;
	vm_pindex_t oldpages, newpages;

	switch (sc->type) {
	case MD_VNODE:
	case MD_NULL:
		break;
	case MD_SWAP:
		if (mdio->md_mediasize <= 0 ||
		    (mdio->md_mediasize % PAGE_SIZE) != 0)
			return (EDOM);
		oldpages = OFF_TO_IDX(round_page(sc->mediasize));
		newpages = OFF_TO_IDX(round_page(mdio->md_mediasize));
		if (newpages < oldpages) {
			VM_OBJECT_WLOCK(sc->object);
			vm_object_page_remove(sc->object, newpages, 0, 0);
			swap_pager_freespace(sc->object, newpages,
			    oldpages - newpages);
			swap_release_by_cred(IDX_TO_OFF(oldpages -
			    newpages), sc->cred);
			sc->object->charge = IDX_TO_OFF(newpages);
			sc->object->size = newpages;
			VM_OBJECT_WUNLOCK(sc->object);
		} else if (newpages > oldpages) {
			res = swap_reserve_by_cred(IDX_TO_OFF(newpages -
			    oldpages), sc->cred);
			if (!res)
				return (ENOMEM);
			if ((mdio->md_options & MD_RESERVE) ||
			    (sc->flags & MD_RESERVE)) {
				error = swap_pager_reserve(sc->object,
				    oldpages, newpages - oldpages);
				if (error < 0) {
					swap_release_by_cred(
					    IDX_TO_OFF(newpages - oldpages),
					    sc->cred);
					return (EDOM);
				}
			}
			VM_OBJECT_WLOCK(sc->object);
			sc->object->charge = IDX_TO_OFF(newpages);
			sc->object->size = newpages;
			VM_OBJECT_WUNLOCK(sc->object);
		}
		break;
	default:
		return (EOPNOTSUPP);
	}

	sc->mediasize = mdio->md_mediasize;
	g_topology_lock();
	g_resize_provider(sc->pp, sc->mediasize);
	g_topology_unlock();
	return (0);
}

static int
mdcreate_swap(struct md_s *sc, struct md_ioctl *mdio, struct thread *td)
{
	vm_ooffset_t npage;
	int error;

	/*
	 * Range check.  Disallow negative sizes and sizes not being
	 * multiple of page size.
	 */
	if (sc->mediasize <= 0 || (sc->mediasize % PAGE_SIZE) != 0)
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
	    VM_PROT_DEFAULT, 0, td->td_ucred);
	if (sc->object == NULL)
		return (ENOMEM);
	sc->flags = mdio->md_options & (MD_FORCE | MD_RESERVE);
	if (mdio->md_options & MD_RESERVE) {
		if (swap_pager_reserve(sc->object, 0, npage) < 0) {
			error = EDOM;
			goto finish;
		}
	}
	error = mdsetcred(sc, td->td_ucred);
 finish:
	if (error != 0) {
		vm_object_deallocate(sc->object);
		sc->object = NULL;
	}
	return (error);
}

static int
mdcreate_null(struct md_s *sc, struct md_ioctl *mdio, struct thread *td)
{

	/*
	 * Range check.  Disallow negative sizes and sizes not being
	 * multiple of page size.
	 */
	if (sc->mediasize <= 0 || (sc->mediasize % PAGE_SIZE) != 0)
		return (EDOM);

	return (0);
}

static int
xmdctlioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flags, struct thread *td)
{
	struct md_ioctl *mdio;
	struct md_s *sc;
	int error, i;
	unsigned sectsize;

	if (md_debug)
		printf("mdctlioctl(%s %lx %p %x %p)\n",
			devtoname(dev), cmd, addr, flags, td);

	mdio = (struct md_ioctl *)addr;
	if (mdio->md_version != MDIOVERSION)
		return (EINVAL);

	/*
	 * We assert the version number in the individual ioctl
	 * handlers instead of out here because (a) it is possible we
	 * may add another ioctl in the future which doesn't read an
	 * mdio, and (b) the correct return value for an unknown ioctl
	 * is ENOIOCTL, not EINVAL.
	 */
	error = 0;
	switch (cmd) {
	case MDIOCATTACH:
		switch (mdio->md_type) {
		case MD_MALLOC:
		case MD_PRELOAD:
		case MD_VNODE:
		case MD_SWAP:
		case MD_NULL:
			break;
		default:
			return (EINVAL);
		}
		if (mdio->md_sectorsize == 0)
			sectsize = DEV_BSIZE;
		else
			sectsize = mdio->md_sectorsize;
		if (sectsize > MAXPHYS || mdio->md_mediasize < sectsize)
			return (EINVAL);
		if (mdio->md_options & MD_AUTOUNIT)
			sc = mdnew(-1, &error, mdio->md_type);
		else {
			if (mdio->md_unit > INT_MAX)
				return (EINVAL);
			sc = mdnew(mdio->md_unit, &error, mdio->md_type);
		}
		if (sc == NULL)
			return (error);
		if (mdio->md_options & MD_AUTOUNIT)
			mdio->md_unit = sc->unit;
		sc->mediasize = mdio->md_mediasize;
		sc->sectorsize = sectsize;
		error = EDOOFUS;
		switch (sc->type) {
		case MD_MALLOC:
			sc->start = mdstart_malloc;
			error = mdcreate_malloc(sc, mdio);
			break;
		case MD_PRELOAD:
			/*
			 * We disallow attaching preloaded memory disks via
			 * ioctl. Preloaded memory disks are automatically
			 * attached in g_md_init().
			 */
			error = EOPNOTSUPP;
			break;
		case MD_VNODE:
			sc->start = mdstart_vnode;
			error = mdcreate_vnode(sc, mdio, td);
			break;
		case MD_SWAP:
			sc->start = mdstart_swap;
			error = mdcreate_swap(sc, mdio, td);
			break;
		case MD_NULL:
			sc->start = mdstart_null;
			error = mdcreate_null(sc, mdio, td);
			break;
		}
		if (error != 0) {
			mddestroy(sc, td);
			return (error);
		}

		/* Prune off any residual fractional sector */
		i = sc->mediasize % sc->sectorsize;
		sc->mediasize -= i;

		mdinit(sc);
		return (0);
	case MDIOCDETACH:
		if (mdio->md_mediasize != 0 ||
		    (mdio->md_options & ~MD_FORCE) != 0)
			return (EINVAL);

		sc = mdfind(mdio->md_unit);
		if (sc == NULL)
			return (ENOENT);
		if (sc->opencount != 0 && !(sc->flags & MD_FORCE) &&
		    !(mdio->md_options & MD_FORCE))
			return (EBUSY);
		return (mddestroy(sc, td));
	case MDIOCRESIZE:
		if ((mdio->md_options & ~(MD_FORCE | MD_RESERVE)) != 0)
			return (EINVAL);

		sc = mdfind(mdio->md_unit);
		if (sc == NULL)
			return (ENOENT);
		if (mdio->md_mediasize < sc->sectorsize)
			return (EINVAL);
		if (mdio->md_mediasize < sc->mediasize &&
		    !(sc->flags & MD_FORCE) &&
		    !(mdio->md_options & MD_FORCE))
			return (EBUSY);
		return (mdresize(sc, mdio));
	case MDIOCQUERY:
		sc = mdfind(mdio->md_unit);
		if (sc == NULL)
			return (ENOENT);
		mdio->md_type = sc->type;
		mdio->md_options = sc->flags;
		mdio->md_mediasize = sc->mediasize;
		mdio->md_sectorsize = sc->sectorsize;
		if (sc->type == MD_VNODE)
			error = copyout(sc->file, mdio->md_file,
			    strlen(sc->file) + 1);
		return (error);
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
}

static int
mdctlioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flags, struct thread *td)
{
	int error;

	sx_xlock(&md_sx);
	error = xmdctlioctl(dev, cmd, addr, flags, td);
	sx_xunlock(&md_sx);
	return (error);
}

static void
md_preloaded(u_char *image, size_t length, const char *name)
{
	struct md_s *sc;
	int error;

	sc = mdnew(-1, &error, MD_PRELOAD);
	if (sc == NULL)
		return;
	sc->mediasize = length;
	sc->sectorsize = DEV_BSIZE;
	sc->pl_ptr = image;
	sc->pl_len = length;
	sc->start = mdstart_preload;
#ifdef MD_ROOT
	if (sc->unit == 0)
		rootdevnames[0] = MD_ROOT_FSTYPE ":/dev/md0";
#endif
	mdinit(sc);
	if (name != NULL) {
		printf("%s%d: Preloaded image <%s> %zd bytes at %p\n",
		    MD_NAME, sc->unit, name, length, image);
	} else {
		printf("%s%d: Embedded image %zd bytes at %p\n",
		    MD_NAME, sc->unit, length, image);
	}
}

static void
g_md_init(struct g_class *mp __unused)
{
	caddr_t mod;
	u_char *ptr, *name, *type;
	unsigned len;
	int i;

	/* figure out log2(NINDIR) */
	for (i = NINDIR, nshift = -1; i; nshift++)
		i >>= 1;

	mod = NULL;
	sx_init(&md_sx, "MD config lock");
	g_topology_unlock();
	md_uh = new_unrhdr(0, INT_MAX, NULL);
#ifdef MD_ROOT
	if (mfs_root_size != 0) {
		sx_xlock(&md_sx);
		md_preloaded(__DEVOLATILE(u_char *, &mfs_root), mfs_root_size,
		    NULL);
		sx_xunlock(&md_sx);
	}
#endif
	/* XXX: are preload_* static or do they need Giant ? */
	while ((mod = preload_search_next_name(mod)) != NULL) {
		name = (char *)preload_search_info(mod, MODINFO_NAME);
		if (name == NULL)
			continue;
		type = (char *)preload_search_info(mod, MODINFO_TYPE);
		if (type == NULL)
			continue;
		if (strcmp(type, "md_image") && strcmp(type, "mfs_root"))
			continue;
		ptr = preload_fetch_addr(mod);
		len = preload_fetch_size(mod);
		if (ptr != NULL && len != 0) {
			sx_xlock(&md_sx);
			md_preloaded(ptr, len, name);
			sx_xunlock(&md_sx);
		}
	}
	md_vnode_pbuf_freecnt = nswbuf / 10;
	status_dev = make_dev(&mdctl_cdevsw, INT_MAX, UID_ROOT, GID_WHEEL,
	    0600, MDCTL_NAME);
	g_topology_lock();
}

static void
g_md_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
    struct g_consumer *cp __unused, struct g_provider *pp)
{
	struct md_s *mp;
	char *type;

	mp = gp->softc;
	if (mp == NULL)
		return;

	switch (mp->type) {
	case MD_MALLOC:
		type = "malloc";
		break;
	case MD_PRELOAD:
		type = "preload";
		break;
	case MD_VNODE:
		type = "vnode";
		break;
	case MD_SWAP:
		type = "swap";
		break;
	case MD_NULL:
		type = "null";
		break;
	default:
		type = "unknown";
		break;
	}

	if (pp != NULL) {
		if (indent == NULL) {
			sbuf_printf(sb, " u %d", mp->unit);
			sbuf_printf(sb, " s %ju", (uintmax_t) mp->sectorsize);
			sbuf_printf(sb, " f %ju", (uintmax_t) mp->fwheads);
			sbuf_printf(sb, " fs %ju", (uintmax_t) mp->fwsectors);
			sbuf_printf(sb, " l %ju", (uintmax_t) mp->mediasize);
			sbuf_printf(sb, " t %s", type);
			if (mp->type == MD_VNODE && mp->vnode != NULL)
				sbuf_printf(sb, " file %s", mp->file);
		} else {
			sbuf_printf(sb, "%s<unit>%d</unit>\n", indent,
			    mp->unit);
			sbuf_printf(sb, "%s<sectorsize>%ju</sectorsize>\n",
			    indent, (uintmax_t) mp->sectorsize);
			sbuf_printf(sb, "%s<fwheads>%ju</fwheads>\n",
			    indent, (uintmax_t) mp->fwheads);
			sbuf_printf(sb, "%s<fwsectors>%ju</fwsectors>\n",
			    indent, (uintmax_t) mp->fwsectors);
			sbuf_printf(sb, "%s<length>%ju</length>\n",
			    indent, (uintmax_t) mp->mediasize);
			sbuf_printf(sb, "%s<compression>%s</compression>\n", indent,
			    (mp->flags & MD_COMPRESS) == 0 ? "off": "on");
			sbuf_printf(sb, "%s<access>%s</access>\n", indent,
			    (mp->flags & MD_READONLY) == 0 ? "read-write":
			    "read-only");
			sbuf_printf(sb, "%s<type>%s</type>\n", indent,
			    type);
			if (mp->type == MD_VNODE && mp->vnode != NULL) {
				sbuf_printf(sb, "%s<file>", indent);
				g_conf_printf_escaped(sb, "%s", mp->file);
				sbuf_printf(sb, "</file>\n");
			}
		}
	}
}

static void
g_md_fini(struct g_class *mp __unused)
{

	sx_destroy(&md_sx);
	if (status_dev != NULL)
		destroy_dev(status_dev);
	delete_unrhdr(md_uh);
}
