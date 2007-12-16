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
#include <sys/bio.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/limits.h>
#include <geom/geom.h>
#include <geom/geom_int.h>

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

static g_taste_t g_dev_taste;
static g_orphan_t g_dev_orphan;
static g_init_t		g_dev_init;

static struct g_class g_dev_class	= {
	.name = "DEV",
	.version = G_VERSION,
	.taste = g_dev_taste,
	.orphan = g_dev_orphan,
	.init = g_dev_init,
};

static struct unrhdr *unithdr;	/* Locked by topology */

static void
g_dev_init(struct g_class *mp)
{

	unithdr = new_unrhdr(0, minor2unit(MAXMINOR), NULL);
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
	int error;
	struct cdev *dev;
	u_int unit;

	g_trace(G_T_TOPOLOGY, "dev_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	LIST_FOREACH(cp, &pp->consumers, consumers)
		if (cp->geom->class == mp)
			return (NULL);
	gp = g_new_geomf(mp, pp->name);
	cp = g_new_consumer(gp);
	error = g_attach(cp, pp);
	KASSERT(error == 0,
	    ("g_dev_taste(%s) failed to g_attach, err=%d", pp->name, error));
	unit = alloc_unr(unithdr);
	dev = make_dev(&g_dev_cdevsw, unit2minor(unit),
	    UID_ROOT, GID_OPERATOR, 0640, gp->name);
	if (pp->flags & G_PF_CANDELETE)
		dev->si_flags |= SI_CANDELETE;
	dev->si_iosize_max = MAXPHYS;
	gp->softc = dev;
	dev->si_drv1 = gp;
	dev->si_drv2 = cp;
	return (gp);
}

static int
g_dev_open(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	int error, r, w, e;

	gp = dev->si_drv1;
	cp = dev->si_drv2;
	if (gp == NULL || cp == NULL || gp->softc != dev)
		return(ENXIO);		/* g_dev_taste() not done yet */

	g_trace(G_T_ACCESS, "g_dev_open(%s, %d, %d, %p)",
	    gp->name, flags, fmt, td);

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
	if (dev->si_devsw == NULL)
		error = ENXIO;		/* We were orphaned */
	else
		error = g_access(cp, r, w, e);
	g_topology_unlock();
	return(error);
}

static int
g_dev_close(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	int error, r, w, e, i;

	gp = dev->si_drv1;
	cp = dev->si_drv2;
	if (gp == NULL || cp == NULL)
		return(ENXIO);
	g_trace(G_T_ACCESS, "g_dev_close(%s, %d, %d, %p)",
	    gp->name, flags, fmt, td);
	r = flags & FREAD ? -1 : 0;
	w = flags & FWRITE ? -1 : 0;
#ifdef notyet
	e = flags & O_EXCL ? -1 : 0;
#else
	e = 0;
#endif
	g_topology_lock();
	if (dev->si_devsw == NULL)
		error = ENXIO;		/* We were orphaned */
	else
		error = g_access(cp, r, w, e);
	for (i = 0; i < 10 * hz;) {
		if (cp->acr != 0 || cp->acw != 0)
			break;
 		if (cp->nstart == cp->nend)
			break;
		pause("gdevwclose", hz / 10);
		i += hz / 10;
	}
	if (cp->acr == 0 && cp->acw == 0 && cp->nstart != cp->nend) {
		printf("WARNING: Final close of geom_dev(%s) %s %s\n",
		    gp->name,
		    "still has outstanding I/O after 10 seconds.",
		    "Completing close anyway, panic may happen later.");
	}
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
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_kerneldump kd;
	off_t offset, length, chunk;
	int i, error;
	u_int u;

	gp = dev->si_drv1;
	cp = dev->si_drv2;

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
		u = *((u_int *)data);
		if (!u) {
			set_dumper(NULL);
			error = 0;
			break;
		}
		kd.offset = 0;
		kd.length = OFF_MAX;
		i = sizeof kd;
		error = g_io_getattr("GEOM::kerneldump", cp, &i, &kd);
		if (!error)
			dev->si_flags |= SI_DUMPDEV;
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
			if (chunk > 1024 * cp->provider->sectorsize)
				chunk = 1024 * cp->provider->sectorsize;
			error = g_delete_data(cp, offset, chunk);
			length -= chunk;
			offset += chunk;
			if (error)
				break;
			/*
			 * Since the request size is unbounded, the service
			 * time is likewise.  We make this ioctl interruptible
			 * by checking for signals for each bio.
			 */
			if (SIGPENDING(td))
				break;
		}
		break;
	case DIOCGIDENT:
		error = g_io_getattr("GEOM::ident", cp, &i, data);
		break;

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
	struct bio *bp;

	bp = bp2->bio_parent;
	bp->bio_error = bp2->bio_error;
	if (bp->bio_error != 0) {
		g_trace(G_T_BIO, "g_dev_done(%p) had error %d",
		    bp2, bp->bio_error);
		bp->bio_flags |= BIO_ERROR;
	} else {
		g_trace(G_T_BIO, "g_dev_done(%p/%p) resid %ld completed %jd",
		    bp2, bp, bp->bio_resid, (intmax_t)bp2->bio_completed);
	}
	bp->bio_resid = bp->bio_length - bp2->bio_completed;
	bp->bio_completed = bp2->bio_completed;
	g_destroy_bio(bp2);
	biodone(bp);
}

static void
g_dev_strategy(struct bio *bp)
{
	struct g_consumer *cp;
	struct bio *bp2;
	struct cdev *dev;

	KASSERT(bp->bio_cmd == BIO_READ ||
	        bp->bio_cmd == BIO_WRITE ||
	        bp->bio_cmd == BIO_DELETE,
		("Wrong bio_cmd bio=%p cmd=%d", bp, bp->bio_cmd));
	dev = bp->bio_dev;
	cp = dev->si_drv2;
	KASSERT(cp->acr || cp->acw,
	    ("Consumer with zero access count in g_dev_strategy"));

	if ((bp->bio_offset % cp->provider->sectorsize) != 0 ||
	    (bp->bio_bcount % cp->provider->sectorsize) != 0) {
		bp->bio_resid = bp->bio_bcount;
		biofinish(bp, NULL, EINVAL);
		return;
	}

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
 * g_dev_orphan()
 *
 * Called from below when the provider orphaned us.
 * - Clear any dump settings.
 * - Destroy the struct cdev *to prevent any more request from coming in.  The
 *   provider is already marked with an error, so anything which comes in
 *   in the interrim will be returned immediately.
 * - Wait for any outstanding I/O to finish.
 * - Set our access counts to zero, whatever they were.
 * - Detach and self-destruct.
 */

static void
g_dev_orphan(struct g_consumer *cp)
{
	struct g_geom *gp;
	struct cdev *dev;
	u_int unit;

	g_topology_assert();
	gp = cp->geom;
	dev = gp->softc;
	g_trace(G_T_TOPOLOGY, "g_dev_orphan(%p(%s))", cp, gp->name);

	/* Reset any dump-area set on this device */
	if (dev->si_flags & SI_DUMPDEV)
		set_dumper(NULL);

	/* Destroy the struct cdev *so we get no more requests */
	unit = dev2unit(dev);
	destroy_dev(dev);
	free_unr(unithdr, unit);

	/* Wait for the cows to come home */
	while (cp->nstart != cp->nend)
		pause("gdevorphan", hz / 10);

	if (cp->acr > 0 || cp->acw > 0 || cp->ace > 0)
		g_access(cp, -cp->acr, -cp->acw, -cp->ace);

	g_detach(cp);
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
}

DECLARE_GEOM_CLASS(g_dev_class, g_dev);
