/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/ctype.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/limits.h>
#include <sys/sysctl.h>
#include <geom/geom.h>
#include <geom/geom_int.h>
#include <machine/stdarg.h>

struct g_dev_softc {
	struct mtx	 sc_mtx;
	struct cdev	*sc_dev;
	struct cdev	*sc_alias;
	int		 sc_open;
	int		 sc_active;
};

static d_open_t		g_dev_open;
static d_close_t	g_dev_close;
static d_strategy_t	g_dev_strategy;
static d_ioctl_t	g_dev_ioctl;

static struct cdevsw g_dev_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	g_dev_open,
	.d_close =	g_dev_close,
	.d_read =	physread,
	.d_write =	physwrite,
	.d_ioctl =	g_dev_ioctl,
	.d_strategy =	g_dev_strategy,
	.d_name =	"g_dev",
	.d_flags =	D_DISK | D_TRACKCLOSE,
};

static g_init_t g_dev_init;
static g_fini_t g_dev_fini;
static g_taste_t g_dev_taste;
static g_orphan_t g_dev_orphan;
static g_attrchanged_t g_dev_attrchanged;

static struct g_class g_dev_class	= {
	.name = "DEV",
	.version = G_VERSION,
	.init = g_dev_init,
	.fini = g_dev_fini,
	.taste = g_dev_taste,
	.orphan = g_dev_orphan,
	.attrchanged = g_dev_attrchanged
};

/*
 * We target 262144 (8 x 32768) sectors by default as this significantly
 * increases the throughput on commonly used SSD's with a marginal
 * increase in non-interruptible request latency.
 */
static uint64_t g_dev_del_max_sectors = 262144;
SYSCTL_DECL(_kern_geom);
SYSCTL_NODE(_kern_geom, OID_AUTO, dev, CTLFLAG_RW, 0, "GEOM_DEV stuff");
SYSCTL_QUAD(_kern_geom_dev, OID_AUTO, delete_max_sectors, CTLFLAG_RW,
    &g_dev_del_max_sectors, 0, "Maximum number of sectors in a single "
    "delete request sent to the provider. Larger requests are chunked "
    "so they can be interrupted. (0 = disable chunking)");

static char *dumpdev = NULL;
static void
g_dev_init(struct g_class *mp)
{

	dumpdev = kern_getenv("dumpdev");
}

static void
g_dev_fini(struct g_class *mp)
{

	freeenv(dumpdev);
}

static int
g_dev_setdumpdev(struct cdev *dev, struct thread *td)
{
	struct g_kerneldump kd;
	struct g_consumer *cp;
	int error, len;

	if (dev == NULL)
		return (set_dumper(NULL, NULL, td));

	cp = dev->si_drv2;
	len = sizeof(kd);
	kd.offset = 0;
	kd.length = OFF_MAX;
	error = g_io_getattr("GEOM::kerneldump", cp, &len, &kd);
	if (error == 0) {
		error = set_dumper(&kd.di, devtoname(dev), td);
		if (error == 0)
			dev->si_flags |= SI_DUMPDEV;
	}
	return (error);
}

static void
init_dumpdev(struct cdev *dev)
{

	if (dumpdev == NULL)
		return;
	if (strcmp(devtoname(dev), dumpdev) != 0)
		return;
	if (g_dev_setdumpdev(dev, curthread) == 0) {
		freeenv(dumpdev);
		dumpdev = NULL;
	}
}

static void
g_dev_destroy(void *arg, int flags __unused)
{
	struct g_consumer *cp;
	struct g_geom *gp;
	struct g_dev_softc *sc;
	char buf[SPECNAMELEN + 6];

	g_topology_assert();
	cp = arg;
	gp = cp->geom;
	sc = cp->private;
	g_trace(G_T_TOPOLOGY, "g_dev_destroy(%p(%s))", cp, gp->name);
	snprintf(buf, sizeof(buf), "cdev=%s", gp->name);
	devctl_notify_f("GEOM", "DEV", "DESTROY", buf, M_WAITOK);
	if (cp->acr > 0 || cp->acw > 0 || cp->ace > 0)
		g_access(cp, -cp->acr, -cp->acw, -cp->ace);
	g_detach(cp);
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	mtx_destroy(&sc->sc_mtx);
	g_free(sc);
}

void
g_dev_print(void)
{
	struct g_geom *gp;
	char const *p = "";

	LIST_FOREACH(gp, &g_dev_class.geom, geom) {
		printf("%s%s", p, gp->name);
		p = " ";
	}
	printf("\n");
}

static void
g_dev_attrchanged(struct g_consumer *cp, const char *attr)
{
	struct g_dev_softc *sc;
	struct cdev *dev;
	char buf[SPECNAMELEN + 6];

	sc = cp->private;
	if (strcmp(attr, "GEOM::media") == 0) {
		dev = sc->sc_dev;
		snprintf(buf, sizeof(buf), "cdev=%s", dev->si_name);
		devctl_notify_f("DEVFS", "CDEV", "MEDIACHANGE", buf, M_WAITOK);
		devctl_notify_f("GEOM", "DEV", "MEDIACHANGE", buf, M_WAITOK);
		dev = sc->sc_alias;
		if (dev != NULL) {
			snprintf(buf, sizeof(buf), "cdev=%s", dev->si_name);
			devctl_notify_f("DEVFS", "CDEV", "MEDIACHANGE", buf,
			    M_WAITOK);
			devctl_notify_f("GEOM", "DEV", "MEDIACHANGE", buf,
			    M_WAITOK);
		}
		return;
	}

	if (strcmp(attr, "GEOM::physpath") != 0)
		return;

	if (g_access(cp, 1, 0, 0) == 0) {
		char *physpath;
		int error, physpath_len;

		physpath_len = MAXPATHLEN;
		physpath = g_malloc(physpath_len, M_WAITOK|M_ZERO);
		error =
		    g_io_getattr("GEOM::physpath", cp, &physpath_len, physpath);
		g_access(cp, -1, 0, 0);
		if (error == 0 && strlen(physpath) != 0) {
			struct cdev *old_alias_dev;
			struct cdev **alias_devp;

			dev = sc->sc_dev;
			old_alias_dev = sc->sc_alias;
			alias_devp = (struct cdev **)&sc->sc_alias;
			make_dev_physpath_alias(MAKEDEV_WAITOK, alias_devp,
			    dev, old_alias_dev, physpath);
		} else if (sc->sc_alias) {
			destroy_dev((struct cdev *)sc->sc_alias);
			sc->sc_alias = NULL;
		}
		g_free(physpath);
	}
}

struct g_provider *
g_dev_getprovider(struct cdev *dev)
{
	struct g_consumer *cp;

	g_topology_assert();
	if (dev == NULL)
		return (NULL);
	if (dev->si_devsw != &g_dev_cdevsw)
		return (NULL);
	cp = dev->si_drv2;
	return (cp->provider);
}

static struct g_geom *
g_dev_taste(struct g_class *mp, struct g_provider *pp, int insist __unused)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_dev_softc *sc;
	int error, len;
	struct cdev *dev, *adev;
	char buf[SPECNAMELEN + 6], *val;

	g_trace(G_T_TOPOLOGY, "dev_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	gp = g_new_geomf(mp, "%s", pp->name);
	sc = g_malloc(sizeof(*sc), M_WAITOK | M_ZERO);
	mtx_init(&sc->sc_mtx, "g_dev", NULL, MTX_DEF);
	cp = g_new_consumer(gp);
	cp->private = sc;
	cp->flags |= G_CF_DIRECT_SEND | G_CF_DIRECT_RECEIVE;
	error = g_attach(cp, pp);
	KASSERT(error == 0,
	    ("g_dev_taste(%s) failed to g_attach, err=%d", pp->name, error));
	error = make_dev_p(MAKEDEV_CHECKNAME | MAKEDEV_WAITOK, &dev,
	    &g_dev_cdevsw, NULL, UID_ROOT, GID_OPERATOR, 0640, "%s", gp->name);
	if (error != 0) {
		printf("%s: make_dev_p() failed (gp->name=%s, error=%d)\n",
		    __func__, gp->name, error);
		g_detach(cp);
		g_destroy_consumer(cp);
		g_destroy_geom(gp);
		mtx_destroy(&sc->sc_mtx);
		g_free(sc);
		return (NULL);
	}
	dev->si_flags |= SI_UNMAPPED;
	sc->sc_dev = dev;

	/* Search for device alias name and create it if found. */
	adev = NULL;
	for (len = MIN(strlen(gp->name), sizeof(buf) - 15); len > 0; len--) {
		snprintf(buf, sizeof(buf), "kern.devalias.%s", gp->name);
		buf[14 + len] = 0;
		val = kern_getenv(buf);
		if (val != NULL) {
			snprintf(buf, sizeof(buf), "%s%s",
			    val, gp->name + len);
			freeenv(val);
			if ((make_dev_alias_p(MAKEDEV_CHECKNAME|MAKEDEV_WAITOK,
			    &adev, dev, "%s", buf)) != 0)
				printf("Warning: unable to create device "
				    "alias %s\n", buf);
			break;
		}
	}

	dev->si_iosize_max = MAXPHYS;
	dev->si_drv2 = cp;
	init_dumpdev(dev);
	if (adev != NULL) {
		adev->si_iosize_max = MAXPHYS;
		adev->si_drv2 = cp;
		adev->si_flags |= SI_UNMAPPED;
		init_dumpdev(adev);
	}

	g_dev_attrchanged(cp, "GEOM::physpath");
	snprintf(buf, sizeof(buf), "cdev=%s", gp->name);
	devctl_notify_f("GEOM", "DEV", "CREATE", buf, M_WAITOK);

	return (gp);
}

static int
g_dev_open(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct g_consumer *cp;
	struct g_dev_softc *sc;
	int error, r, w, e;

	cp = dev->si_drv2;
	if (cp == NULL)
		return (ENXIO);		/* g_dev_taste() not done yet */
	g_trace(G_T_ACCESS, "g_dev_open(%s, %d, %d, %p)",
	    cp->geom->name, flags, fmt, td);

	r = flags & FREAD ? 1 : 0;
	w = flags & FWRITE ? 1 : 0;
#ifdef notyet
	e = flags & O_EXCL ? 1 : 0;
#else
	e = 0;
#endif
	if (w) {
		/*
		 * When running in very secure mode, do not allow
		 * opens for writing of any disks.
		 */
		error = securelevel_ge(td->td_ucred, 2);
		if (error)
			return (error);
	}
	g_topology_lock();
	error = g_access(cp, r, w, e);
	g_topology_unlock();
	if (error == 0) {
		sc = cp->private;
		mtx_lock(&sc->sc_mtx);
		if (sc->sc_open == 0 && sc->sc_active != 0)
			wakeup(&sc->sc_active);
		sc->sc_open += r + w + e;
		mtx_unlock(&sc->sc_mtx);
	}
	return (error);
}

static int
g_dev_close(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct g_consumer *cp;
	struct g_dev_softc *sc;
	int error, r, w, e;

	cp = dev->si_drv2;
	if (cp == NULL)
		return (ENXIO);
	g_trace(G_T_ACCESS, "g_dev_close(%s, %d, %d, %p)",
	    cp->geom->name, flags, fmt, td);

	r = flags & FREAD ? -1 : 0;
	w = flags & FWRITE ? -1 : 0;
#ifdef notyet
	e = flags & O_EXCL ? -1 : 0;
#else
	e = 0;
#endif

	/*
	 * The vgonel(9) - caused by eg. forced unmount of devfs - calls
	 * VOP_CLOSE(9) on devfs vnode without any FREAD or FWRITE flags,
	 * which would result in zero deltas, which in turn would cause
	 * panic in g_access(9).
	 *
	 * Note that we cannot zero the counters (ie. do "r = cp->acr"
	 * etc) instead, because the consumer might be opened in another
	 * devfs instance.
	 */
	if (r + w + e == 0)
		return (EINVAL);

	sc = cp->private;
	mtx_lock(&sc->sc_mtx);
	sc->sc_open += r + w + e;
	while (sc->sc_open == 0 && sc->sc_active != 0)
		msleep(&sc->sc_active, &sc->sc_mtx, 0, "PRIBIO", 0);
	mtx_unlock(&sc->sc_mtx);
	g_topology_lock();
	error = g_access(cp, r, w, e);
	g_topology_unlock();
	return (error);
}

/*
 * XXX: Until we have unmessed the ioctl situation, there is a race against
 * XXX: a concurrent orphanization.  We cannot close it by holding topology
 * XXX: since that would prevent us from doing our job, and stalling events
 * XXX: will break (actually: stall) the BSD disklabel hacks.
 */
static int
g_dev_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	struct g_consumer *cp;
	struct g_provider *pp;
	off_t offset, length, chunk;
	int i, error;

	cp = dev->si_drv2;
	pp = cp->provider;

	error = 0;
	KASSERT(cp->acr || cp->acw,
	    ("Consumer with zero access count in g_dev_ioctl"));

	i = IOCPARM_LEN(cmd);
	switch (cmd) {
	case DIOCGSECTORSIZE:
		*(u_int *)data = cp->provider->sectorsize;
		if (*(u_int *)data == 0)
			error = ENOENT;
		break;
	case DIOCGMEDIASIZE:
		*(off_t *)data = cp->provider->mediasize;
		if (*(off_t *)data == 0)
			error = ENOENT;
		break;
	case DIOCGFWSECTORS:
		error = g_io_getattr("GEOM::fwsectors", cp, &i, data);
		if (error == 0 && *(u_int *)data == 0)
			error = ENOENT;
		break;
	case DIOCGFWHEADS:
		error = g_io_getattr("GEOM::fwheads", cp, &i, data);
		if (error == 0 && *(u_int *)data == 0)
			error = ENOENT;
		break;
	case DIOCGFRONTSTUFF:
		error = g_io_getattr("GEOM::frontstuff", cp, &i, data);
		break;
	case DIOCSKERNELDUMP:
		if (*(u_int *)data == 0)
			error = g_dev_setdumpdev(NULL, td);
		else
			error = g_dev_setdumpdev(dev, td);
		break;
	case DIOCGFLUSH:
		error = g_io_flush(cp);
		break;
	case DIOCGDELETE:
		offset = ((off_t *)data)[0];
		length = ((off_t *)data)[1];
		if ((offset % cp->provider->sectorsize) != 0 ||
		    (length % cp->provider->sectorsize) != 0 || length <= 0) {
			printf("%s: offset=%jd length=%jd\n", __func__, offset,
			    length);
			error = EINVAL;
			break;
		}
		while (length > 0) {
			chunk = length;
			if (g_dev_del_max_sectors != 0 && chunk >
			    g_dev_del_max_sectors * cp->provider->sectorsize) {
				chunk = g_dev_del_max_sectors *
				    cp->provider->sectorsize;
			}
			error = g_delete_data(cp, offset, chunk);
			length -= chunk;
			offset += chunk;
			if (error)
				break;
			/*
			 * Since the request size can be large, the service
			 * time can be is likewise.  We make this ioctl
			 * interruptible by checking for signals for each bio.
			 */
			if (SIGPENDING(td))
				break;
		}
		break;
	case DIOCGIDENT:
		error = g_io_getattr("GEOM::ident", cp, &i, data);
		break;
	case DIOCGPROVIDERNAME:
		if (pp == NULL)
			return (ENOENT);
		strlcpy(data, pp->name, i);
		break;
	case DIOCGSTRIPESIZE:
		*(off_t *)data = cp->provider->stripesize;
		break;
	case DIOCGSTRIPEOFFSET:
		*(off_t *)data = cp->provider->stripeoffset;
		break;
	case DIOCGPHYSPATH:
		error = g_io_getattr("GEOM::physpath", cp, &i, data);
		if (error == 0 && *(char *)data == '\0')
			error = ENOENT;
		break;
	case DIOCGATTR: {
		struct diocgattr_arg *arg = (struct diocgattr_arg *)data;

		if (arg->len > sizeof(arg->value)) {
			error = EINVAL;
			break;
		}
		error = g_io_getattr(arg->name, cp, &arg->len, &arg->value);
		break;
	}
	default:
		if (cp->provider->geom->ioctl != NULL) {
			error = cp->provider->geom->ioctl(cp->provider, cmd, data, fflag, td);
		} else {
			error = ENOIOCTL;
		}
	}

	return (error);
}

static void
g_dev_done(struct bio *bp2)
{
	struct g_consumer *cp;
	struct g_dev_softc *sc;
	struct bio *bp;
	int destroy;

	cp = bp2->bio_from;
	sc = cp->private;
	bp = bp2->bio_parent;
	bp->bio_error = bp2->bio_error;
	bp->bio_completed = bp2->bio_completed;
	bp->bio_resid = bp->bio_length - bp2->bio_completed;
	if (bp2->bio_error != 0) {
		g_trace(G_T_BIO, "g_dev_done(%p) had error %d",
		    bp2, bp2->bio_error);
		bp->bio_flags |= BIO_ERROR;
	} else {
		g_trace(G_T_BIO, "g_dev_done(%p/%p) resid %ld completed %jd",
		    bp2, bp, bp2->bio_resid, (intmax_t)bp2->bio_completed);
	}
	g_destroy_bio(bp2);
	destroy = 0;
	mtx_lock(&sc->sc_mtx);
	if ((--sc->sc_active) == 0) {
		if (sc->sc_open == 0)
			wakeup(&sc->sc_active);
		if (sc->sc_dev == NULL)
			destroy = 1;
	}
	mtx_unlock(&sc->sc_mtx);
	if (destroy)
		g_post_event(g_dev_destroy, cp, M_NOWAIT, NULL);
	biodone(bp);
}

static void
g_dev_strategy(struct bio *bp)
{
	struct g_consumer *cp;
	struct bio *bp2;
	struct cdev *dev;
	struct g_dev_softc *sc;

	KASSERT(bp->bio_cmd == BIO_READ ||
	        bp->bio_cmd == BIO_WRITE ||
	        bp->bio_cmd == BIO_DELETE ||
		bp->bio_cmd == BIO_FLUSH,
		("Wrong bio_cmd bio=%p cmd=%d", bp, bp->bio_cmd));
	dev = bp->bio_dev;
	cp = dev->si_drv2;
	sc = cp->private;
	KASSERT(cp->acr || cp->acw,
	    ("Consumer with zero access count in g_dev_strategy"));
#ifdef INVARIANTS
	if ((bp->bio_offset % cp->provider->sectorsize) != 0 ||
	    (bp->bio_bcount % cp->provider->sectorsize) != 0) {
		bp->bio_resid = bp->bio_bcount;
		biofinish(bp, NULL, EINVAL);
		return;
	}
#endif
	mtx_lock(&sc->sc_mtx);
	KASSERT(sc->sc_open > 0, ("Closed device in g_dev_strategy"));
	sc->sc_active++;
	mtx_unlock(&sc->sc_mtx);

	for (;;) {
		/*
		 * XXX: This is not an ideal solution, but I belive it to
		 * XXX: deadlock safe, all things considered.
		 */
		bp2 = g_clone_bio(bp);
		if (bp2 != NULL)
			break;
		pause("gdstrat", hz / 10);
	}
	KASSERT(bp2 != NULL, ("XXX: ENOMEM in a bad place"));
	bp2->bio_done = g_dev_done;
	g_trace(G_T_BIO,
	    "g_dev_strategy(%p/%p) offset %jd length %jd data %p cmd %d",
	    bp, bp2, (intmax_t)bp->bio_offset, (intmax_t)bp2->bio_length,
	    bp2->bio_data, bp2->bio_cmd);
	g_io_request(bp2, cp);
	KASSERT(cp->acr || cp->acw,
	    ("g_dev_strategy raced with g_dev_close and lost"));

}

/*
 * g_dev_callback()
 *
 * Called by devfs when asynchronous device destruction is completed.
 * - Mark that we have no attached device any more.
 * - If there are no outstanding requests, schedule geom destruction.
 *   Otherwise destruction will be scheduled later by g_dev_done().
 */

static void
g_dev_callback(void *arg)
{
	struct g_consumer *cp;
	struct g_dev_softc *sc;
	int destroy;

	cp = arg;
	sc = cp->private;
	g_trace(G_T_TOPOLOGY, "g_dev_callback(%p(%s))", cp, cp->geom->name);

	mtx_lock(&sc->sc_mtx);
	sc->sc_dev = NULL;
	sc->sc_alias = NULL;
	destroy = (sc->sc_active == 0);
	mtx_unlock(&sc->sc_mtx);
	if (destroy)
		g_post_event(g_dev_destroy, cp, M_WAITOK, NULL);
}

/*
 * g_dev_orphan()
 *
 * Called from below when the provider orphaned us.
 * - Clear any dump settings.
 * - Request asynchronous device destruction to prevent any more requests
 *   from coming in.  The provider is already marked with an error, so
 *   anything which comes in in the interrim will be returned immediately.
 */

static void
g_dev_orphan(struct g_consumer *cp)
{
	struct cdev *dev;
	struct g_dev_softc *sc;

	g_topology_assert();
	sc = cp->private;
	dev = sc->sc_dev;
	g_trace(G_T_TOPOLOGY, "g_dev_orphan(%p(%s))", cp, cp->geom->name);

	/* Reset any dump-area set on this device */
	if (dev->si_flags & SI_DUMPDEV)
		(void)set_dumper(NULL, NULL, curthread);

	/* Destroy the struct cdev *so we get no more requests */
	destroy_dev_sched_cb(dev, g_dev_callback, cp);
}

DECLARE_GEOM_CLASS(g_dev_class, g_dev);
