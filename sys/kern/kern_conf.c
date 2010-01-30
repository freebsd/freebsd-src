/*-
 * Copyright (c) 1999-2002 Poul-Henning Kamp
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/bio.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <sys/queue.h>
#include <sys/poll.h>
#include <sys/sx.h>
#include <sys/ctype.h>
#include <sys/ucred.h>
#include <sys/taskqueue.h>
#include <machine/stdarg.h>

#include <fs/devfs/devfs_int.h>
#include <vm/vm.h>

static MALLOC_DEFINE(M_DEVT, "cdev", "cdev storage");

struct mtx devmtx;
static void destroy_devl(struct cdev *dev);
static int destroy_dev_sched_cbl(struct cdev *dev,
    void (*cb)(void *), void *arg);
static struct cdev *make_dev_credv(int flags,
    struct cdevsw *devsw, int unit,
    struct ucred *cr, uid_t uid, gid_t gid, int mode, const char *fmt,
    va_list ap);

static struct cdev_priv_list cdevp_free_list =
    TAILQ_HEAD_INITIALIZER(cdevp_free_list);
static SLIST_HEAD(free_cdevsw, cdevsw) cdevsw_gt_post_list =
    SLIST_HEAD_INITIALIZER(cdevsw_gt_post_list);

void
dev_lock(void)
{

	mtx_lock(&devmtx);
}

/*
 * Free all the memory collected while the cdev mutex was
 * locked. Since devmtx is after the system map mutex, free() cannot
 * be called immediately and is postponed until cdev mutex can be
 * dropped.
 */
static void
dev_unlock_and_free(void)
{
	struct cdev_priv_list cdp_free;
	struct free_cdevsw csw_free;
	struct cdev_priv *cdp;
	struct cdevsw *csw;

	mtx_assert(&devmtx, MA_OWNED);

	/*
	 * Make the local copy of the list heads while the dev_mtx is
	 * held. Free it later.
	 */
	TAILQ_INIT(&cdp_free);
	TAILQ_CONCAT(&cdp_free, &cdevp_free_list, cdp_list);
	csw_free = cdevsw_gt_post_list;
	SLIST_INIT(&cdevsw_gt_post_list);

	mtx_unlock(&devmtx);

	while ((cdp = TAILQ_FIRST(&cdp_free)) != NULL) {
		TAILQ_REMOVE(&cdp_free, cdp, cdp_list);
		devfs_free(&cdp->cdp_c);
	}
	while ((csw = SLIST_FIRST(&csw_free)) != NULL) {
		SLIST_REMOVE_HEAD(&csw_free, d_postfree_list);
		free(csw, M_DEVT);
	}
}

static void
dev_free_devlocked(struct cdev *cdev)
{
	struct cdev_priv *cdp;

	mtx_assert(&devmtx, MA_OWNED);
	cdp = cdev2priv(cdev);
	TAILQ_INSERT_HEAD(&cdevp_free_list, cdp, cdp_list);
}

static void
cdevsw_free_devlocked(struct cdevsw *csw)
{

	mtx_assert(&devmtx, MA_OWNED);
	SLIST_INSERT_HEAD(&cdevsw_gt_post_list, csw, d_postfree_list);
}

void
dev_unlock(void)
{

	mtx_unlock(&devmtx);
}

void
dev_ref(struct cdev *dev)
{

	mtx_assert(&devmtx, MA_NOTOWNED);
	mtx_lock(&devmtx);
	dev->si_refcount++;
	mtx_unlock(&devmtx);
}

void
dev_refl(struct cdev *dev)
{

	mtx_assert(&devmtx, MA_OWNED);
	dev->si_refcount++;
}

void
dev_rel(struct cdev *dev)
{
	int flag = 0;

	mtx_assert(&devmtx, MA_NOTOWNED);
	dev_lock();
	dev->si_refcount--;
	KASSERT(dev->si_refcount >= 0,
	    ("dev_rel(%s) gave negative count", devtoname(dev)));
#if 0
	if (dev->si_usecount == 0 &&
	    (dev->si_flags & SI_CHEAPCLONE) && (dev->si_flags & SI_NAMED))
		;
	else 
#endif
	if (dev->si_devsw == NULL && dev->si_refcount == 0) {
		LIST_REMOVE(dev, si_list);
		flag = 1;
	}
	dev_unlock();
	if (flag)
		devfs_free(dev);
}

struct cdevsw *
dev_refthread(struct cdev *dev)
{
	struct cdevsw *csw;
	struct cdev_priv *cdp;

	mtx_assert(&devmtx, MA_NOTOWNED);
	dev_lock();
	csw = dev->si_devsw;
	if (csw != NULL) {
		cdp = cdev2priv(dev);
		if ((cdp->cdp_flags & CDP_SCHED_DTR) == 0)
			dev->si_threadcount++;
		else
			csw = NULL;
	}
	dev_unlock();
	return (csw);
}

struct cdevsw *
devvn_refthread(struct vnode *vp, struct cdev **devp)
{
	struct cdevsw *csw;
	struct cdev_priv *cdp;

	mtx_assert(&devmtx, MA_NOTOWNED);
	csw = NULL;
	dev_lock();
	*devp = vp->v_rdev;
	if (*devp != NULL) {
		cdp = cdev2priv(*devp);
		if ((cdp->cdp_flags & CDP_SCHED_DTR) == 0) {
			csw = (*devp)->si_devsw;
			if (csw != NULL)
				(*devp)->si_threadcount++;
		}
	}
	dev_unlock();
	return (csw);
}

void	
dev_relthread(struct cdev *dev)
{

	mtx_assert(&devmtx, MA_NOTOWNED);
	dev_lock();
	KASSERT(dev->si_threadcount > 0,
	    ("%s threadcount is wrong", dev->si_name));
	dev->si_threadcount--;
	dev_unlock();
}

int
nullop(void)
{

	return (0);
}

int
eopnotsupp(void)
{

	return (EOPNOTSUPP);
}

static int
enxio(void)
{
	return (ENXIO);
}

static int
enodev(void)
{
	return (ENODEV);
}

/* Define a dead_cdevsw for use when devices leave unexpectedly. */

#define dead_open	(d_open_t *)enxio
#define dead_close	(d_close_t *)enxio
#define dead_read	(d_read_t *)enxio
#define dead_write	(d_write_t *)enxio
#define dead_ioctl	(d_ioctl_t *)enxio
#define dead_poll	(d_poll_t *)enodev
#define dead_mmap	(d_mmap_t *)enodev

static void
dead_strategy(struct bio *bp)
{

	biofinish(bp, NULL, ENXIO);
}

#define dead_dump	(dumper_t *)enxio
#define dead_kqfilter	(d_kqfilter_t *)enxio
#define dead_mmap_single (d_mmap_single_t *)enodev

static struct cdevsw dead_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT, /* XXX: does dead_strategy need this ? */
	.d_open =	dead_open,
	.d_close =	dead_close,
	.d_read =	dead_read,
	.d_write =	dead_write,
	.d_ioctl =	dead_ioctl,
	.d_poll =	dead_poll,
	.d_mmap =	dead_mmap,
	.d_strategy =	dead_strategy,
	.d_name =	"dead",
	.d_dump =	dead_dump,
	.d_kqfilter =	dead_kqfilter,
	.d_mmap_single = dead_mmap_single
};

/* Default methods if driver does not specify method */

#define null_open	(d_open_t *)nullop
#define null_close	(d_close_t *)nullop
#define no_read		(d_read_t *)enodev
#define no_write	(d_write_t *)enodev
#define no_ioctl	(d_ioctl_t *)enodev
#define no_mmap		(d_mmap2_t *)enodev
#define no_kqfilter	(d_kqfilter_t *)enodev
#define no_mmap_single	(d_mmap_single_t *)enodev

static void
no_strategy(struct bio *bp)
{

	biofinish(bp, NULL, ENODEV);
}

static int
no_poll(struct cdev *dev __unused, int events, struct thread *td __unused)
{

	return (poll_no_poll(events));
}

#define no_dump		(dumper_t *)enodev

static int
giant_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct cdevsw *dsw;
	int retval;

	dsw = dev_refthread(dev);
	if (dsw == NULL)
		return (ENXIO);
	mtx_lock(&Giant);
	retval = dsw->d_gianttrick->d_open(dev, oflags, devtype, td);
	mtx_unlock(&Giant);
	dev_relthread(dev);
	return (retval);
}

static int
giant_fdopen(struct cdev *dev, int oflags, struct thread *td, struct file *fp)
{
	struct cdevsw *dsw;
	int retval;

	dsw = dev_refthread(dev);
	if (dsw == NULL)
		return (ENXIO);
	mtx_lock(&Giant);
	retval = dsw->d_gianttrick->d_fdopen(dev, oflags, td, fp);
	mtx_unlock(&Giant);
	dev_relthread(dev);
	return (retval);
}

static int
giant_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct cdevsw *dsw;
	int retval;

	dsw = dev_refthread(dev);
	if (dsw == NULL)
		return (ENXIO);
	mtx_lock(&Giant);
	retval = dsw->d_gianttrick->d_close(dev, fflag, devtype, td);
	mtx_unlock(&Giant);
	dev_relthread(dev);
	return (retval);
}

static void
giant_strategy(struct bio *bp)
{
	struct cdevsw *dsw;
	struct cdev *dev;

	dev = bp->bio_dev;
	dsw = dev_refthread(dev);
	if (dsw == NULL) {
		biofinish(bp, NULL, ENXIO);
		return;
	}
	mtx_lock(&Giant);
	dsw->d_gianttrick->d_strategy(bp);
	mtx_unlock(&Giant);
	dev_relthread(dev);
}

static int
giant_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	struct cdevsw *dsw;
	int retval;

	dsw = dev_refthread(dev);
	if (dsw == NULL)
		return (ENXIO);
	mtx_lock(&Giant);
	retval = dsw->d_gianttrick->d_ioctl(dev, cmd, data, fflag, td);
	mtx_unlock(&Giant);
	dev_relthread(dev);
	return (retval);
}
  
static int
giant_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct cdevsw *dsw;
	int retval;

	dsw = dev_refthread(dev);
	if (dsw == NULL)
		return (ENXIO);
	mtx_lock(&Giant);
	retval = dsw->d_gianttrick->d_read(dev, uio, ioflag);
	mtx_unlock(&Giant);
	dev_relthread(dev);
	return (retval);
}

static int
giant_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct cdevsw *dsw;
	int retval;

	dsw = dev_refthread(dev);
	if (dsw == NULL)
		return (ENXIO);
	mtx_lock(&Giant);
	retval = dsw->d_gianttrick->d_write(dev, uio, ioflag);
	mtx_unlock(&Giant);
	dev_relthread(dev);
	return (retval);
}

static int
giant_poll(struct cdev *dev, int events, struct thread *td)
{
	struct cdevsw *dsw;
	int retval;

	dsw = dev_refthread(dev);
	if (dsw == NULL)
		return (ENXIO);
	mtx_lock(&Giant);
	retval = dsw->d_gianttrick->d_poll(dev, events, td);
	mtx_unlock(&Giant);
	dev_relthread(dev);
	return (retval);
}

static int
giant_kqfilter(struct cdev *dev, struct knote *kn)
{
	struct cdevsw *dsw;
	int retval;

	dsw = dev_refthread(dev);
	if (dsw == NULL)
		return (ENXIO);
	mtx_lock(&Giant);
	retval = dsw->d_gianttrick->d_kqfilter(dev, kn);
	mtx_unlock(&Giant);
	dev_relthread(dev);
	return (retval);
}

static int
giant_mmap(struct cdev *dev, vm_offset_t offset, vm_paddr_t *paddr, int nprot,
    vm_memattr_t *memattr)
{
	struct cdevsw *dsw;
	int retval;

	dsw = dev_refthread(dev);
	if (dsw == NULL)
		return (ENXIO);
	mtx_lock(&Giant);
	if (dsw->d_gianttrick->d_flags & D_MMAP2)
		retval = dsw->d_gianttrick->d_mmap2(dev, offset, paddr, nprot,
		    memattr);
	else
		retval = dsw->d_gianttrick->d_mmap(dev, offset, paddr, nprot);
	mtx_unlock(&Giant);
	dev_relthread(dev);
	return (retval);
}

static int
giant_mmap_single(struct cdev *dev, vm_ooffset_t *offset, vm_size_t size,
    vm_object_t *object, int nprot)
{
	struct cdevsw *dsw;
	int retval;

	dsw = dev_refthread(dev);
	if (dsw == NULL)
		return (ENXIO);
	mtx_lock(&Giant);
	retval = dsw->d_gianttrick->d_mmap_single(dev, offset, size, object,
	    nprot);
	mtx_unlock(&Giant);
	dev_relthread(dev);
	return (retval);
}

static void
notify(struct cdev *dev, const char *ev)
{
	static const char prefix[] = "cdev=";
	char *data;
	int namelen;

	if (cold)
		return;
	namelen = strlen(dev->si_name);
	data = malloc(namelen + sizeof(prefix), M_TEMP, M_NOWAIT);
	if (data == NULL)
		return;
	memcpy(data, prefix, sizeof(prefix) - 1);
	memcpy(data + sizeof(prefix) - 1, dev->si_name, namelen + 1);
	devctl_notify("DEVFS", "CDEV", ev, data);
	free(data, M_TEMP);
}

static void
notify_create(struct cdev *dev)
{

	notify(dev, "CREATE");
}

static void
notify_destroy(struct cdev *dev)
{

	notify(dev, "DESTROY");
}

static struct cdev *
newdev(struct cdevsw *csw, int unit, struct cdev *si)
{
	struct cdev *si2;

	mtx_assert(&devmtx, MA_OWNED);
	if (csw->d_flags & D_NEEDMINOR) {
		/* We may want to return an existing device */
		LIST_FOREACH(si2, &csw->d_devs, si_list) {
			if (dev2unit(si2) == unit) {
				dev_free_devlocked(si);
				return (si2);
			}
		}
	}
	si->si_drv0 = unit;
	si->si_devsw = csw;
	LIST_INSERT_HEAD(&csw->d_devs, si, si_list);
	return (si);
}

static void
fini_cdevsw(struct cdevsw *devsw)
{
	struct cdevsw *gt;

	if (devsw->d_gianttrick != NULL) {
		gt = devsw->d_gianttrick;
		memcpy(devsw, gt, sizeof *devsw);
		cdevsw_free_devlocked(gt);
		devsw->d_gianttrick = NULL;
	}
	devsw->d_flags &= ~D_INIT;
}

static void
prep_cdevsw(struct cdevsw *devsw)
{
	struct cdevsw *dsw2;

	mtx_assert(&devmtx, MA_OWNED);
	if (devsw->d_flags & D_INIT)
		return;
	if (devsw->d_flags & D_NEEDGIANT) {
		dev_unlock();
		dsw2 = malloc(sizeof *dsw2, M_DEVT, M_WAITOK);
		dev_lock();
	} else
		dsw2 = NULL;
	if (devsw->d_flags & D_INIT) {
		if (dsw2 != NULL)
			cdevsw_free_devlocked(dsw2);
		return;
	}

	if (devsw->d_version != D_VERSION_01 &&
	    devsw->d_version != D_VERSION_02) {
		printf(
		    "WARNING: Device driver \"%s\" has wrong version %s\n",
		    devsw->d_name == NULL ? "???" : devsw->d_name,
		    "and is disabled.  Recompile KLD module.");
		devsw->d_open = dead_open;
		devsw->d_close = dead_close;
		devsw->d_read = dead_read;
		devsw->d_write = dead_write;
		devsw->d_ioctl = dead_ioctl;
		devsw->d_poll = dead_poll;
		devsw->d_mmap = dead_mmap;
		devsw->d_strategy = dead_strategy;
		devsw->d_dump = dead_dump;
		devsw->d_kqfilter = dead_kqfilter;
	}
	if (devsw->d_version == D_VERSION_01)
		devsw->d_mmap_single = NULL;
	
	if (devsw->d_flags & D_NEEDGIANT) {
		if (devsw->d_gianttrick == NULL) {
			memcpy(dsw2, devsw, sizeof *dsw2);
			devsw->d_gianttrick = dsw2;
			devsw->d_flags |= D_MMAP2;
			dsw2 = NULL;
		}
	}

#define FIXUP(member, noop, giant) 				\
	do {							\
		if (devsw->member == NULL) {			\
			devsw->member = noop;			\
		} else if (devsw->d_flags & D_NEEDGIANT)	\
			devsw->member = giant;			\
		}						\
	while (0)

	FIXUP(d_open,		null_open,	giant_open);
	FIXUP(d_fdopen,		NULL,		giant_fdopen);
	FIXUP(d_close,		null_close,	giant_close);
	FIXUP(d_read,		no_read,	giant_read);
	FIXUP(d_write,		no_write,	giant_write);
	FIXUP(d_ioctl,		no_ioctl,	giant_ioctl);
	FIXUP(d_poll,		no_poll,	giant_poll);
	FIXUP(d_mmap2,		no_mmap,	giant_mmap);
	FIXUP(d_strategy,	no_strategy,	giant_strategy);
	FIXUP(d_kqfilter,	no_kqfilter,	giant_kqfilter);
	FIXUP(d_mmap_single,	no_mmap_single,	giant_mmap_single);

	if (devsw->d_dump == NULL)	devsw->d_dump = no_dump;

	LIST_INIT(&devsw->d_devs);

	devsw->d_flags |= D_INIT;

	if (dsw2 != NULL)
		cdevsw_free_devlocked(dsw2);
}

struct cdev *
make_dev_credv(int flags, struct cdevsw *devsw, int unit,
    struct ucred *cr, uid_t uid,
    gid_t gid, int mode, const char *fmt, va_list ap)
{
	struct cdev *dev;
	int i;

	dev = devfs_alloc();
	dev_lock();
	prep_cdevsw(devsw);
	dev = newdev(devsw, unit, dev);
	if (flags & MAKEDEV_REF)
		dev_refl(dev);
	if (dev->si_flags & SI_CHEAPCLONE &&
	    dev->si_flags & SI_NAMED) {
		/*
		 * This is allowed as it removes races and generally
		 * simplifies cloning devices.
		 * XXX: still ??
		 */
		dev_unlock_and_free();
		return (dev);
	}
	KASSERT(!(dev->si_flags & SI_NAMED),
	    ("make_dev() by driver %s on pre-existing device (min=%x, name=%s)",
	    devsw->d_name, dev2unit(dev), devtoname(dev)));

	i = vsnrprintf(dev->__si_namebuf, sizeof dev->__si_namebuf, 32, fmt, ap);
	if (i > (sizeof dev->__si_namebuf - 1)) {
		printf("WARNING: Device name truncated! (%s)\n", 
		    dev->__si_namebuf);
	}
		
	dev->si_flags |= SI_NAMED;
	if (cr != NULL)
		dev->si_cred = crhold(cr);
	else
		dev->si_cred = NULL;
	dev->si_uid = uid;
	dev->si_gid = gid;
	dev->si_mode = mode;

	devfs_create(dev);
	clean_unrhdrl(devfs_inos);
	dev_unlock_and_free();

	notify_create(dev);

	return (dev);
}

struct cdev *
make_dev(struct cdevsw *devsw, int unit, uid_t uid, gid_t gid, int mode,
    const char *fmt, ...)
{
	struct cdev *dev;
	va_list ap;

	va_start(ap, fmt);
	dev = make_dev_credv(0, devsw, unit, NULL, uid, gid, mode, fmt, ap);
	va_end(ap);
	return (dev);
}

struct cdev *
make_dev_cred(struct cdevsw *devsw, int unit, struct ucred *cr, uid_t uid,
    gid_t gid, int mode, const char *fmt, ...)
{
	struct cdev *dev;
	va_list ap;

	va_start(ap, fmt);
	dev = make_dev_credv(0, devsw, unit, cr, uid, gid, mode, fmt, ap);
	va_end(ap);

	return (dev);
}

struct cdev *
make_dev_credf(int flags, struct cdevsw *devsw, int unit,
    struct ucred *cr, uid_t uid,
    gid_t gid, int mode, const char *fmt, ...)
{
	struct cdev *dev;
	va_list ap;

	va_start(ap, fmt);
	dev = make_dev_credv(flags, devsw, unit, cr, uid, gid, mode,
	    fmt, ap);
	va_end(ap);

	return (dev);
}

static void
dev_dependsl(struct cdev *pdev, struct cdev *cdev)
{

	cdev->si_parent = pdev;
	cdev->si_flags |= SI_CHILD;
	LIST_INSERT_HEAD(&pdev->si_children, cdev, si_siblings);
}


void
dev_depends(struct cdev *pdev, struct cdev *cdev)
{

	dev_lock();
	dev_dependsl(pdev, cdev);
	dev_unlock();
}

struct cdev *
make_dev_alias(struct cdev *pdev, const char *fmt, ...)
{
	struct cdev *dev;
	va_list ap;
	int i;

	KASSERT(pdev != NULL, ("NULL pdev"));
	dev = devfs_alloc();
	dev_lock();
	dev->si_flags |= SI_ALIAS;
	dev->si_flags |= SI_NAMED;
	va_start(ap, fmt);
	i = vsnrprintf(dev->__si_namebuf, sizeof dev->__si_namebuf, 32, fmt, ap);
	if (i > (sizeof dev->__si_namebuf - 1)) {
		printf("WARNING: Device name truncated! (%s)\n", 
		    dev->__si_namebuf);
	}
	va_end(ap);

	devfs_create(dev);
	dev_dependsl(pdev, dev);
	clean_unrhdrl(devfs_inos);
	dev_unlock();

	notify_create(dev);

	return (dev);
}

static void
destroy_devl(struct cdev *dev)
{
	struct cdevsw *csw;
	struct cdev_privdata *p, *p1;

	mtx_assert(&devmtx, MA_OWNED);
	KASSERT(dev->si_flags & SI_NAMED,
	    ("WARNING: Driver mistake: destroy_dev on %d\n", dev2unit(dev)));

	devfs_destroy(dev);

	/* Remove name marking */
	dev->si_flags &= ~SI_NAMED;

	/* If we are a child, remove us from the parents list */
	if (dev->si_flags & SI_CHILD) {
		LIST_REMOVE(dev, si_siblings);
		dev->si_flags &= ~SI_CHILD;
	}

	/* Kill our children */
	while (!LIST_EMPTY(&dev->si_children))
		destroy_devl(LIST_FIRST(&dev->si_children));

	/* Remove from clone list */
	if (dev->si_flags & SI_CLONELIST) {
		LIST_REMOVE(dev, si_clone);
		dev->si_flags &= ~SI_CLONELIST;
	}

	dev->si_refcount++;	/* Avoid race with dev_rel() */
	csw = dev->si_devsw;
	dev->si_devsw = NULL;	/* already NULL for SI_ALIAS */
	while (csw != NULL && csw->d_purge != NULL && dev->si_threadcount) {
		csw->d_purge(dev);
		msleep(csw, &devmtx, PRIBIO, "devprg", hz/10);
		if (dev->si_threadcount)
			printf("Still %lu threads in %s\n",
			    dev->si_threadcount, devtoname(dev));
	}
	while (dev->si_threadcount != 0) {
		/* Use unique dummy wait ident */
		msleep(&csw, &devmtx, PRIBIO, "devdrn", hz / 10);
	}

	dev_unlock();
	notify_destroy(dev);
	mtx_lock(&cdevpriv_mtx);
	LIST_FOREACH_SAFE(p, &cdev2priv(dev)->cdp_fdpriv, cdpd_list, p1) {
		devfs_destroy_cdevpriv(p);
		mtx_lock(&cdevpriv_mtx);
	}
	mtx_unlock(&cdevpriv_mtx);
	dev_lock();

	dev->si_drv1 = 0;
	dev->si_drv2 = 0;
	bzero(&dev->__si_u, sizeof(dev->__si_u));

	if (!(dev->si_flags & SI_ALIAS)) {
		/* Remove from cdevsw list */
		LIST_REMOVE(dev, si_list);

		/* If cdevsw has no more struct cdev *'s, clean it */
		if (LIST_EMPTY(&csw->d_devs)) {
			fini_cdevsw(csw);
			wakeup(&csw->d_devs);
		}
	}
	dev->si_flags &= ~SI_ALIAS;
	dev->si_refcount--;	/* Avoid race with dev_rel() */

	if (dev->si_refcount > 0) {
		LIST_INSERT_HEAD(&dead_cdevsw.d_devs, dev, si_list);
	} else {
		dev_free_devlocked(dev);
	}
}

void
destroy_dev(struct cdev *dev)
{

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL, "destroy_dev");
	dev_lock();
	destroy_devl(dev);
	dev_unlock_and_free();
}

const char *
devtoname(struct cdev *dev)
{

	return (dev->si_name);
}

int
dev_stdclone(char *name, char **namep, const char *stem, int *unit)
{
	int u, i;

	i = strlen(stem);
	if (bcmp(stem, name, i) != 0)
		return (0);
	if (!isdigit(name[i]))
		return (0);
	u = 0;
	if (name[i] == '0' && isdigit(name[i+1]))
		return (0);
	while (isdigit(name[i])) {
		u *= 10;
		u += name[i++] - '0';
	}
	if (u > 0xffffff)
		return (0);
	*unit = u;
	if (namep)
		*namep = &name[i];
	if (name[i]) 
		return (2);
	return (1);
}

/*
 * Helper functions for cloning device drivers.
 *
 * The objective here is to make it unnecessary for the device drivers to
 * use rman or similar to manage their unit number space.  Due to the way
 * we do "on-demand" devices, using rman or other "private" methods 
 * will be very tricky to lock down properly once we lock down this file.
 *
 * Instead we give the drivers these routines which puts the struct cdev *'s
 * that are to be managed on their own list, and gives the driver the ability
 * to ask for the first free unit number or a given specified unit number.
 *
 * In addition these routines support paired devices (pty, nmdm and similar)
 * by respecting a number of "flag" bits in the minor number.
 *
 */

struct clonedevs {
	LIST_HEAD(,cdev)	head;
};

void
clone_setup(struct clonedevs **cdp)
{

	*cdp = malloc(sizeof **cdp, M_DEVBUF, M_WAITOK | M_ZERO);
	LIST_INIT(&(*cdp)->head);
}

int
clone_create(struct clonedevs **cdp, struct cdevsw *csw, int *up, struct cdev **dp, int extra)
{
	struct clonedevs *cd;
	struct cdev *dev, *ndev, *dl, *de;
	int unit, low, u;

	KASSERT(*cdp != NULL,
	    ("clone_setup() not called in driver \"%s\"", csw->d_name));
	KASSERT(!(extra & CLONE_UNITMASK),
	    ("Illegal extra bits (0x%x) in clone_create", extra));
	KASSERT(*up <= CLONE_UNITMASK,
	    ("Too high unit (0x%x) in clone_create", *up));
	KASSERT(csw->d_flags & D_NEEDMINOR,
	    ("clone_create() on cdevsw without minor numbers"));


	/*
	 * Search the list for a lot of things in one go:
	 *   A preexisting match is returned immediately.
	 *   The lowest free unit number if we are passed -1, and the place
	 *	 in the list where we should insert that new element.
	 *   The place to insert a specified unit number, if applicable
	 *       the end of the list.
	 */
	unit = *up;
	ndev = devfs_alloc();
	dev_lock();
	prep_cdevsw(csw);
	low = extra;
	de = dl = NULL;
	cd = *cdp;
	LIST_FOREACH(dev, &cd->head, si_clone) {
		KASSERT(dev->si_flags & SI_CLONELIST,
		    ("Dev %p(%s) should be on clonelist", dev, dev->si_name));
		u = dev2unit(dev);
		if (u == (unit | extra)) {
			*dp = dev;
			dev_unlock();
			devfs_free(ndev);
			return (0);
		}
		if (unit == -1 && u == low) {
			low++;
			de = dev;
			continue;
		} else if (u < (unit | extra)) {
			de = dev;
			continue;
		} else if (u > (unit | extra)) {
			dl = dev;
			break;
		}
	}
	if (unit == -1)
		unit = low & CLONE_UNITMASK;
	dev = newdev(csw, unit | extra, ndev);
	if (dev->si_flags & SI_CLONELIST) {
		printf("dev %p (%s) is on clonelist\n", dev, dev->si_name);
		printf("unit=%d, low=%d, extra=0x%x\n", unit, low, extra);
		LIST_FOREACH(dev, &cd->head, si_clone) {
			printf("\t%p %s\n", dev, dev->si_name);
		}
		panic("foo");
	}
	KASSERT(!(dev->si_flags & SI_CLONELIST),
	    ("Dev %p(%s) should not be on clonelist", dev, dev->si_name));
	if (dl != NULL)
		LIST_INSERT_BEFORE(dl, dev, si_clone);
	else if (de != NULL)
		LIST_INSERT_AFTER(de, dev, si_clone);
	else
		LIST_INSERT_HEAD(&cd->head, dev, si_clone);
	dev->si_flags |= SI_CLONELIST;
	*up = unit;
	dev_unlock_and_free();
	return (1);
}

/*
 * Kill everything still on the list.  The driver should already have
 * disposed of any softc hung of the struct cdev *'s at this time.
 */
void
clone_cleanup(struct clonedevs **cdp)
{
	struct cdev *dev;
	struct cdev_priv *cp;
	struct clonedevs *cd;
	
	cd = *cdp;
	if (cd == NULL)
		return;
	dev_lock();
	while (!LIST_EMPTY(&cd->head)) {
		dev = LIST_FIRST(&cd->head);
		LIST_REMOVE(dev, si_clone);
		KASSERT(dev->si_flags & SI_CLONELIST,
		    ("Dev %p(%s) should be on clonelist", dev, dev->si_name));
		dev->si_flags &= ~SI_CLONELIST;
		cp = cdev2priv(dev);
		if (!(cp->cdp_flags & CDP_SCHED_DTR)) {
			cp->cdp_flags |= CDP_SCHED_DTR;
			KASSERT(dev->si_flags & SI_NAMED,
				("Driver has goofed in cloning underways udev %x unit %x", dev2udev(dev), dev2unit(dev)));
			destroy_devl(dev);
		}
	}
	dev_unlock_and_free();
	free(cd, M_DEVBUF);
	*cdp = NULL;
}

static TAILQ_HEAD(, cdev_priv) dev_ddtr =
	TAILQ_HEAD_INITIALIZER(dev_ddtr);
static struct task dev_dtr_task;

static void
destroy_dev_tq(void *ctx, int pending)
{
	struct cdev_priv *cp;
	struct cdev *dev;
	void (*cb)(void *);
	void *cb_arg;

	dev_lock();
	while (!TAILQ_EMPTY(&dev_ddtr)) {
		cp = TAILQ_FIRST(&dev_ddtr);
		dev = &cp->cdp_c;
		KASSERT(cp->cdp_flags & CDP_SCHED_DTR,
		    ("cdev %p in dev_destroy_tq without CDP_SCHED_DTR", cp));
		TAILQ_REMOVE(&dev_ddtr, cp, cdp_dtr_list);
		cb = cp->cdp_dtr_cb;
		cb_arg = cp->cdp_dtr_cb_arg;
		destroy_devl(dev);
		dev_unlock_and_free();
		dev_rel(dev);
		if (cb != NULL)
			cb(cb_arg);
		dev_lock();
	}
	dev_unlock();
}

/*
 * devmtx shall be locked on entry. devmtx will be unlocked after
 * function return.
 */
static int
destroy_dev_sched_cbl(struct cdev *dev, void (*cb)(void *), void *arg)
{
	struct cdev_priv *cp;

	mtx_assert(&devmtx, MA_OWNED);
	cp = cdev2priv(dev);
	if (cp->cdp_flags & CDP_SCHED_DTR) {
		dev_unlock();
		return (0);
	}
	dev_refl(dev);
	cp->cdp_flags |= CDP_SCHED_DTR;
	cp->cdp_dtr_cb = cb;
	cp->cdp_dtr_cb_arg = arg;
	TAILQ_INSERT_TAIL(&dev_ddtr, cp, cdp_dtr_list);
	dev_unlock();
	taskqueue_enqueue(taskqueue_swi_giant, &dev_dtr_task);
	return (1);
}

int
destroy_dev_sched_cb(struct cdev *dev, void (*cb)(void *), void *arg)
{
	dev_lock();
	return (destroy_dev_sched_cbl(dev, cb, arg));
}

int
destroy_dev_sched(struct cdev *dev)
{
	return (destroy_dev_sched_cb(dev, NULL, NULL));
}

void
destroy_dev_drain(struct cdevsw *csw)
{

	dev_lock();
	while (!LIST_EMPTY(&csw->d_devs)) {
		msleep(&csw->d_devs, &devmtx, PRIBIO, "devscd", hz/10);
	}
	dev_unlock();
}

void
drain_dev_clone_events(void)
{

	sx_xlock(&clone_drain_lock);
	sx_xunlock(&clone_drain_lock);
}

static void
devdtr_init(void *dummy __unused)
{

	TASK_INIT(&dev_dtr_task, 0, destroy_dev_tq, NULL);
}

SYSINIT(devdtr, SI_SUB_DEVFS, SI_ORDER_SECOND, devdtr_init, NULL);
