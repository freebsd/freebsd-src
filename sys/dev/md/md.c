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

#include "opt_mfs.h"		/* We have adopted some tasks from MFS */
#include "opt_md.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mdioctl.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <machine/atomic.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_zone.h>
#include <vm/swap_pager.h>

#define MD_MODVER 1

#ifndef MD_NSECT
#define MD_NSECT (10000 * 2)
#endif

MALLOC_DEFINE(M_MD, "MD disk", "Memory Disk");
MALLOC_DEFINE(M_MDSECT, "MD sectors", "Memory Disk Sectors");

static int md_debug;
SYSCTL_INT(_debug, OID_AUTO, mddebug, CTLFLAG_RW, &md_debug, 0, "");

#if defined(MFS_ROOT) && !defined(MD_ROOT)
#define MD_ROOT MFS_ROOT
#warning "option MFS_ROOT has been superceeded by MD_ROOT"
#endif

#if defined(MFS_ROOT_SIZE) && !defined(MD_ROOT_SIZE)
#define MD_ROOT_SIZE MFS_ROOT_SIZE
#warning "option MFS_ROOT_SIZE has been superceeded by MD_ROOT_SIZE"
#endif

#if defined(MD_ROOT) && defined(MD_ROOT_SIZE)
/* Image gets put here: */
static u_char mfs_root[MD_ROOT_SIZE*1024] = "MFS Filesystem goes here";
static u_char end_mfs_root[] __unused = "MFS Filesystem had better STOP here";
#endif

static int	mdrootready;
static int	mdunits;
static dev_t	status_dev = 0;


#define CDEV_MAJOR	95

static d_strategy_t mdstrategy;
static d_open_t mdopen;
static d_ioctl_t mdioctl, mdctlioctl;

static struct cdevsw md_cdevsw = {
        /* open */      mdopen,
        /* close */     nullclose,
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

static struct cdevsw mddisk_cdevsw;

static LIST_HEAD(, md_s) md_softc_list = LIST_HEAD_INITIALIZER(&md_softc_list);

struct md_s {
	int unit;
	LIST_ENTRY(md_s) list;
	struct devstat stats;
	struct bio_queue_head bio_queue;
	struct disk disk;
	dev_t dev;
	int busy;
	enum md_types type;
	unsigned nsect;
	unsigned secsize;
	unsigned flags;

	/* MD_MALLOC related fields */
	u_char **secp;

	/* MD_PRELOAD related fields */
	u_char *pl_ptr;
	unsigned pl_len;

	/* MD_VNODE related fields */
	struct vnode *vnode;
	struct ucred *cred;

	/* MD_OBJET related fields */
	vm_object_t object;
};

static int
mdopen(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct md_s *sc;
	struct disklabel *dl;

	if (md_debug)
		printf("mdopen(%s %x %x %p)\n",
			devtoname(dev), flag, fmt, p);

	sc = dev->si_drv1;

	dl = &sc->disk.d_label;
	bzero(dl, sizeof(*dl));
	dl->d_secsize = sc->secsize;
	dl->d_nsectors = sc->nsect > 63 ? 63 : sc->nsect;
	dl->d_ntracks = 1;
	dl->d_secpercyl = dl->d_nsectors * dl->d_ntracks;
	dl->d_secperunit = sc->nsect;
	dl->d_ncylinders = dl->d_secperunit / dl->d_secpercyl;
	return (0);
}

static int
mdioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct proc *p)
{

	if (md_debug)
		printf("mdioctl(%s %lx %p %x %p)\n",
			devtoname(dev), cmd, addr, flags, p);

	return (ENOIOCTL);
}

static void
mdstart_malloc(struct md_s *sc)
{
	int i;
	struct bio *bp;
	devstat_trans_flags dop;
	u_char *secp, **secpp, *dst;
	unsigned secno, nsec, secval, uc;

	for (;;) {
		/* XXX: LOCK(unique unit numbers) */
		bp = bioq_first(&sc->bio_queue);
		if (bp)
			bioq_remove(&sc->bio_queue, bp);
		/* XXX: UNLOCK(unique unit numbers) */
		if (!bp)
			break;

		devstat_start_transaction(&sc->stats);

		if (bp->bio_cmd == BIO_DELETE)
			dop = DEVSTAT_NO_DATA;
		else if (bp->bio_cmd == BIO_READ)
			dop = DEVSTAT_READ;
		else
			dop = DEVSTAT_WRITE;

		nsec = bp->bio_bcount / sc->secsize;
		secno = bp->bio_pblkno;
		dst = bp->bio_data;
		while (nsec--) {
			secpp = &sc->secp[secno];
			if ((uintptr_t)*secpp > 255) {
				secp = *secpp;
				secval = 0;
			} else {
				secp = NULL;
				secval = (uintptr_t) *secpp;
			}

			if (md_debug > 2)
				printf("%x %p %p %d\n", 
				    bp->bio_flags, secpp, secp, secval);

			if (bp->bio_cmd == BIO_DELETE) {
				if (!(sc->flags & MD_RESERVE) && secp != NULL) {
					FREE(secp, M_MDSECT);
					*secpp = 0;
				}
			} else if (bp->bio_cmd == BIO_READ) {
				if (secp != NULL) {
					bcopy(secp, dst, sc->secsize);
				} else if (secval) {
					for (i = 0; i < sc->secsize; i++)
						dst[i] = secval;
				} else {
					bzero(dst, sc->secsize);
				}
			} else {
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
					if (secp)
						FREE(secp, M_MDSECT);
					*secpp = (u_char *)(uintptr_t)uc;
				} else {
					if (secp == NULL) 
						MALLOC(secp, u_char *, sc->secsize, M_MDSECT, M_WAITOK);
					bcopy(dst, secp, sc->secsize);
					*secpp = secp;
				}
			}
			secno++;
			dst += sc->secsize;
		}
		bp->bio_resid = 0;
		biofinish(bp, &sc->stats, 0);
	}
	return;
}


static void
mdstart_preload(struct md_s *sc)
{
	struct bio *bp;
	devstat_trans_flags dop;

	for (;;) {
		/* XXX: LOCK(unique unit numbers) */
		bp = bioq_first(&sc->bio_queue);
		if (bp)
			bioq_remove(&sc->bio_queue, bp);
		/* XXX: UNLOCK(unique unit numbers) */
		if (!bp)
			break;

		devstat_start_transaction(&sc->stats);

		if (bp->bio_cmd == BIO_DELETE) {
			dop = DEVSTAT_NO_DATA;
		} else if (bp->bio_cmd == BIO_READ) {
			dop = DEVSTAT_READ;
			bcopy(sc->pl_ptr + (bp->bio_pblkno << DEV_BSHIFT), bp->bio_data, bp->bio_bcount);
		} else {
			dop = DEVSTAT_WRITE;
			bcopy(bp->bio_data, sc->pl_ptr + (bp->bio_pblkno << DEV_BSHIFT), bp->bio_bcount);
		}
		bp->bio_resid = 0;
		biofinish(bp, &sc->stats, 0);
	}
	return;
}

static void
mdstart_vnode(struct md_s *sc)
{
	int error;
	struct bio *bp;
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

	for (;;) {
		/* XXX: LOCK(unique unit numbers) */
		bp = bioq_first(&sc->bio_queue);
		if (bp)
			bioq_remove(&sc->bio_queue, bp);
		/* XXX: UNLOCK(unique unit numbers) */
		if (!bp)
			break;

		devstat_start_transaction(&sc->stats);

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
		auio.uio_procp = curproc;
		if (VOP_ISLOCKED(sc->vnode, NULL))
			vprint("unexpected md driver lock", sc->vnode);
		if (bp->bio_cmd == BIO_READ) {
			vn_lock(sc->vnode, LK_EXCLUSIVE | LK_RETRY, curproc);
			error = VOP_READ(sc->vnode, &auio, 0, sc->cred);
		} else {
			(void) vn_start_write(sc->vnode, &mp, V_WAIT);
			vn_lock(sc->vnode, LK_EXCLUSIVE | LK_RETRY, curproc);
			error = VOP_WRITE(sc->vnode, &auio, 0, sc->cred);
			vn_finished_write(mp);
		}
		VOP_UNLOCK(sc->vnode, 0, curproc);
		bp->bio_resid = auio.uio_resid;
		biofinish(bp, &sc->stats, error);
	}
	return;
}

static void
mdstart_swap(struct md_s *sc)
{
	struct bio *bp;

	for (;;) {
		/* XXX: LOCK(unique unit numbers) */
		bp = bioq_first(&sc->bio_queue);
		if (bp)
			bioq_remove(&sc->bio_queue, bp);
		/* XXX: UNLOCK(unique unit numbers) */
		if (!bp)
			break;

#if 0
		devstat_start_transaction(&sc->stats);
#endif

		if ((bp->bio_cmd == BIO_DELETE) && (sc->flags & MD_RESERVE)) 
			biodone(bp);
		else
			vm_pager_strategy(sc->object, bp);

#if 0
		devstat_end_transaction_bio(&sc->stats, bp);
#endif
	}
	return;
}

static void
mdstrategy(struct bio *bp)
{
	struct md_s *sc;

	if (md_debug > 1)
		printf("mdstrategy(%p) %s %x, %d, %ld, %p)\n",
		    bp, devtoname(bp->bio_dev), bp->bio_flags, bp->bio_blkno, 
		    bp->bio_bcount / DEV_BSIZE, bp->bio_data);

	sc = bp->bio_dev->si_drv1;

	/* XXX: LOCK(sc->lock) */
	bioqdisksort(&sc->bio_queue, bp);
	/* XXX: UNLOCK(sc->lock) */

	if (atomic_cmpset_int(&sc->busy, 0, 1) == 0)
		return;

	switch (sc->type) {
	case MD_MALLOC:
		mdstart_malloc(sc);
		break;
	case MD_PRELOAD:
		mdstart_preload(sc);
		break;
	case MD_VNODE:
		mdstart_vnode(sc);
		break;
	case MD_SWAP:
		mdstart_swap(sc);
		break;
	default:
		panic("Impossible md(type)");
		break;
	}
	sc->busy = 0;
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
	int max = -1;

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
	if (unit > DKMAXUNIT)
		return (NULL);
	MALLOC(sc, struct md_s *,sizeof(*sc), M_MD, M_WAITOK | M_ZERO);
	sc->unit = unit;
	LIST_INSERT_HEAD(&md_softc_list, sc, list);
	/* XXX: UNLOCK(unique unit numbers) */
	return (sc);
}

static void
mdinit(struct md_s *sc)
{

	bioq_init(&sc->bio_queue);
	devstat_add_entry(&sc->stats, MD_NAME, sc->unit, sc->secsize,
		DEVSTAT_NO_ORDERED_TAGS, 
		DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_OTHER,
		DEVSTAT_PRIORITY_OTHER);
	sc->dev = disk_create(sc->unit, &sc->disk, 0, &md_cdevsw, &mddisk_cdevsw);
	sc->dev->si_drv1 = sc;
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
		return(EINVAL);
	if (mdio->md_options & ~(MD_AUTOUNIT))
		return(EINVAL);
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
	unsigned u;

	if (mdio->md_size == 0)
		return(EINVAL);
	if (mdio->md_options & ~(MD_AUTOUNIT | MD_COMPRESS | MD_RESERVE))
		return(EINVAL);
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
	sc->flags = mdio->md_options & MD_COMPRESS;
	MALLOC(sc->secp, u_char **, sc->nsect * sizeof(u_char *), M_MD, M_WAITOK | M_ZERO);
	if (mdio->md_options & MD_RESERVE) {
		for (u = 0; u < sc->nsect; u++)
			MALLOC(sc->secp[u], u_char *, DEV_BSIZE, M_MDSECT, M_WAITOK | M_ZERO);
	}
	printf("%s%d: Malloc disk\n", MD_NAME, sc->unit);
	mdinit(sc);
	return (0);
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
	sc->cred = crdup(cred);

	/*
	 * Horrible kludge to establish credentials for NFS  XXX.
	 */

	if (sc->vnode) {
		struct uio auio;
		struct iovec aiov;

		tmpbuf = malloc(sc->secsize, M_TEMP, M_WAITOK);
		bzero(&auio, sizeof(auio));

		aiov.iov_base = tmpbuf;
		aiov.iov_len = sc->secsize;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		auio.uio_rw = UIO_READ;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_resid = aiov.iov_len;
		vn_lock(sc->vnode, LK_EXCLUSIVE | LK_RETRY, curproc);
		error = VOP_READ(sc->vnode, &auio, 0, sc->cred);
		VOP_UNLOCK(sc->vnode, 0, curproc);
		free(tmpbuf, M_TEMP);
	}
	return (error);
}

static int
mdcreate_vnode(struct md_ioctl *mdio, struct proc *p)
{
	struct md_s *sc;
	struct vattr vattr;
	struct nameidata nd;
	int error, flags;

	if (mdio->md_options & MD_AUTOUNIT) {
		sc = mdnew(-1); 
		mdio->md_unit = sc->unit;
	} else {
		sc = mdnew(mdio->md_unit);
	}
	if (sc == NULL)
		return (EBUSY);

	sc->type = MD_VNODE;

	flags = FREAD|FWRITE;
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, mdio->md_file, p);
	error = vn_open(&nd, &flags, 0);
	if (error) {
		if (error != EACCES && error != EPERM && error != EROFS)
			return (error);
		flags &= ~FWRITE;
		sc->flags |= MD_READONLY;
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, mdio->md_file, p);
		error = vn_open(&nd, &flags, 0);
		if (error)
			return (error);
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (nd.ni_vp->v_type != VREG ||
	    (error = VOP_GETATTR(nd.ni_vp, &vattr, p->p_ucred, p))) {
		VOP_UNLOCK(nd.ni_vp, 0, p);
		(void) vn_close(nd.ni_vp, flags, p->p_ucred, p);
		return (error ? error : EINVAL);
	}
	VOP_UNLOCK(nd.ni_vp, 0, p);
	sc->secsize = DEV_BSIZE;
	sc->vnode = nd.ni_vp;

	/*
	 * If the size is specified, override the file attributes.
	 */
	if (mdio->md_size)
		sc->nsect = mdio->md_size;
	else
		sc->nsect = vattr.va_size / sc->secsize; /* XXX: round up ? */
	error = mdsetcred(sc, p->p_ucred);
	if (error) {
		(void) vn_close(nd.ni_vp, flags, p->p_ucred, p);
		return(error);
	}
	mdinit(sc);
	return (0);
}

static int
mddestroy(struct md_s *sc, struct md_ioctl *mdio, struct proc *p)
{
	unsigned u;

	if (sc->dev != NULL) {
		devstat_remove_entry(&sc->stats);
		disk_destroy(sc->dev);
	}
	if (sc->vnode != NULL)
		(void)vn_close(sc->vnode, sc->flags & MD_READONLY ?  FREAD : (FREAD|FWRITE), sc->cred, p);
	if (sc->cred != NULL)
		crfree(sc->cred);
	if (sc->object != NULL)
		vm_pager_deallocate(sc->object);
	if (sc->secp != NULL) {
		for (u = 0; u < sc->nsect; u++) 
			if ((uintptr_t)sc->secp[u] > 255)
				FREE(sc->secp[u], M_MDSECT);
		FREE(sc->secp, M_MD);
	}

	/* XXX: LOCK(unique unit numbers) */
	LIST_REMOVE(sc, list);
	/* XXX: UNLOCK(unique unit numbers) */
	FREE(sc, M_MD);
	return (0);
}

static int
mdcreate_swap(struct md_ioctl *mdio, struct proc *p)
{
	int error;
	struct md_s *sc;

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
		mddestroy(sc, mdio, p);
		return(EDOM);
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
	if (mdio->md_options & MD_RESERVE) {
		if (swap_pager_reserve(sc->object, 0, sc->nsect) < 0) {
			vm_pager_deallocate(sc->object);
			sc->object = NULL;
			mddestroy(sc, mdio, p);
			return(EDOM);
		}
	}
	error = mdsetcred(sc, p->p_ucred);
	if (error)
		mddestroy(sc, mdio, p);
	else
		mdinit(sc);
	return(error);
}

static int
mdctlioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct proc *p)
{
	struct md_ioctl *mdio;
	struct md_s *sc;

	if (md_debug)
		printf("mdctlioctl(%s %lx %p %x %p)\n",
			devtoname(dev), cmd, addr, flags, p);

	mdio = (struct md_ioctl *)addr;
	switch (cmd) {
	case MDIOCATTACH:
		switch (mdio->md_type) {
		case MD_MALLOC:
			return(mdcreate_malloc(mdio));
		case MD_PRELOAD:
			return(mdcreate_preload(mdio));
		case MD_VNODE:
			return(mdcreate_vnode(mdio, p));
		case MD_SWAP:
			return(mdcreate_swap(mdio, p));
		default:
			return (EINVAL);
		}
	case MDIOCDETACH:
		if (mdio->md_file != NULL)
			return(EINVAL);
		if (mdio->md_size != 0)
			return(EINVAL);
		if (mdio->md_options != 0)
			return(EINVAL);
		sc = mdfind(mdio->md_unit);
		if (sc == NULL)
			return (ENOENT);
		switch(sc->type) {
		case MD_VNODE:
		case MD_SWAP:
		case MD_MALLOC:
		case MD_PRELOAD:
			return(mddestroy(sc, mdio, p));
		default:
			return (EOPNOTSUPP);
		}
	case MDIOCQUERY:
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
			(u_char *)(uintptr_t)mdio->md_base = sc->pl_ptr;
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
		len = *(unsigned *)c;
		printf("md%d: Preloaded image <%s> %d bytes at %p\n",
		   mdunits, name, len, ptr);
		md_preloaded(ptr, len);
	} 
	status_dev = make_dev(&mdctl_cdevsw, 0xffff00ff, UID_ROOT, GID_WHEEL, 0600, "mdctl");
}

static int
md_modevent(module_t mod, int type, void *data)
{
        switch (type) {
        case MOD_LOAD:
		md_drvinit(NULL);
                break;
        case MOD_UNLOAD:
		if (!LIST_EMPTY(&md_softc_list))
			return EBUSY;
                if (status_dev)
                        destroy_dev(status_dev);
                status_dev = 0;
                break;
        default:
                break;
        }
        return 0;
}

static moduledata_t md_mod = {
        "md",
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
		rootdevnames[0] = "ufs:/dev/md0c";
}

SYSINIT(md_root, SI_SUB_MOUNT_ROOT, SI_ORDER_FIRST, md_takeroot, NULL);
#endif

