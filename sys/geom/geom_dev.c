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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/bio.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <geom/geom.h>

#define CDEV_MAJOR	4

static d_open_t		g_dev_open;
static d_close_t	g_dev_close;
static d_strategy_t	g_dev_strategy;
static d_ioctl_t	g_dev_ioctl;
static d_psize_t	g_dev_psize;

static struct cdevsw g_dev_cdevsw = {
        /* open */      g_dev_open,
        /* close */     g_dev_close,
        /* read */      physread,
        /* write */     physwrite,
        /* ioctl */     g_dev_ioctl,
        /* poll */      nopoll,
        /* mmap */      nommap,
        /* strategy */  g_dev_strategy,
        /* name */      "g_dev",
        /* maj */       CDEV_MAJOR,
        /* dump */      nodump,
        /* psize */     g_dev_psize,
        /* flags */     D_DISK | D_CANFREE | D_TRACKCLOSE,
};

static g_taste_t g_dev_taste;
static g_orphan_t g_dev_orphan;

static struct g_method g_dev_method	= {
	"DEV-method",
	g_dev_taste,
	NULL,
	g_dev_orphan,
	NULL,
	G_METHOD_INITSTUFF
};

static void
g_dev_clone(void *arg, char *name, int namelen, dev_t *dev)
{
	struct g_geom *gp;

	if (*dev != NODEV)
		return;

	g_trace(G_T_TOPOLOGY, "g_dev_clone(%s)", name);
	g_rattle();

	/* XXX: can I drop Giant here ??? */
	/* g_topology_lock(); */
	LIST_FOREACH(gp, &g_dev_method.geom, geom) {
		if (strcmp(gp->name, name))
			continue;
		*dev = gp->softc;
		g_trace(G_T_TOPOLOGY, "g_dev_clone(%s) = %p", name, *dev);
		return;
	}
	/* g_topology_unlock(); */
	return;
}

static void
g_dev_register_cloner(void *foo __unused)
{
	static int once;

	if (!once) {
		if (!once)
			EVENTHANDLER_REGISTER(dev_clone, g_dev_clone, 0, 1000);
		once++;
	}
}

SYSINIT(geomdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE,g_dev_register_cloner,NULL);

static struct g_geom *
g_dev_taste(struct g_method *mp, struct g_provider *pp, struct thread *tp __unused, int insist __unused)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	static int unit;
	u_int secsize;
	off_t mediasize;
	int error, j;
	dev_t dev;

	g_trace(G_T_TOPOLOGY, "dev_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	LIST_FOREACH(cp, &pp->consumers, consumers)
		if (cp->geom->method == mp)
			return (NULL);
	gp = g_new_geomf(mp, pp->name);
	cp = g_new_consumer(gp);
	g_attach(cp, pp);
	error = g_access_rel(cp, 1, 0, 0);
	g_topology_unlock();
	if (!error) {
		j = sizeof secsize;
		error = g_io_getattr("GEOM::sectorsize", cp, &j, &secsize, tp);
		if (error) {
			secsize = 512;
			printf("g_bsd_taste: error %d Sectors are %d bytes\n",
			    error, secsize);
		}
		j = sizeof mediasize;
		error = g_io_getattr("GEOM::mediasize", cp, &j, &mediasize, tp);
		if (error) {
			mediasize = 0;
			printf("g_error %d Mediasize is %lld bytes\n",
			    error, mediasize);
		}
		g_topology_lock();
		g_access_rel(cp, -1, 0, 0);
		g_topology_unlock();
	} else {
		secsize = 512;
		mediasize = 0;
	}
	mtx_lock(&Giant);
	if (mediasize != 0)
		printf("GEOM: \"%s\" %lld bytes in %lld sectors of %u bytes\n",
		    pp->name, mediasize, mediasize / secsize, secsize);
	else
		printf("GEOM: \"%s\" (size unavailable)\n", pp->name);
	dev = make_dev(&g_dev_cdevsw, unit++,
	    UID_ROOT, GID_WHEEL, 0600, gp->name);
	gp->softc = dev;
	dev->si_drv1 = gp;
	dev->si_drv2 = cp;
	mtx_unlock(&Giant);
	g_topology_lock();
	return (gp);
}

static int
g_dev_open(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	int error, r, w, e;

	gp = dev->si_drv1;
	cp = dev->si_drv2;
	if (gp == NULL || cp == NULL)
		return(ENXIO);
	g_trace(G_T_ACCESS, "g_dev_open(%s, %d, %d, %p)",
	    gp->name, flags, fmt, td);
	mtx_unlock(&Giant);
	g_topology_lock();
	g_silence();
	r = flags & FREAD ? 1 : 0;
	w = flags & FWRITE ? 1 : 0;
	e = flags & O_EXCL ? 1 : 0;
	error = g_access_rel(cp, r, w, e);
	g_topology_unlock();
	mtx_lock(&Giant);
	g_rattle();
	return(error);
}

static int
g_dev_close(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	int error, r, w, e;

	gp = dev->si_drv1;
	cp = dev->si_drv2;
	if (gp == NULL || cp == NULL)
		return(ENXIO);
	g_trace(G_T_ACCESS, "g_dev_close(%s, %d, %d, %p)",
	    gp->name, flags, fmt, td);
	mtx_unlock(&Giant);
	g_topology_lock();
	g_silence();
	r = flags & FREAD ? -1 : 0;
	w = flags & FWRITE ? -1 : 0;
	e = flags & O_EXCL ? -1 : 0;
	error = g_access_rel(cp, r, w, e);
	g_topology_unlock();
	mtx_lock(&Giant);
	g_rattle();
	return (error);
}

static int
g_dev_ioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	int i, error;
	struct g_ioctl *gio;

	gp = dev->si_drv1;
	cp = dev->si_drv2;

	error = 0;
	mtx_unlock(&Giant);

	gio = g_malloc(sizeof *gio, M_WAITOK);
	gio->cmd = cmd;
	gio->data = data;
	gio->fflag = fflag;
	gio->td = td;
	i = sizeof *gio;
	if (cmd & IOC_IN)
		error = g_io_setattr("GEOM::ioctl", cp, i, gio, td);
	else
		error = g_io_getattr("GEOM::ioctl", cp, &i, gio, td);
	g_free(gio);

	if (error != 0 && cmd == DIOCGDVIRGIN) {
		g_topology_lock();
		gp = g_create_geomf("BSD-method", cp->provider, NULL);
		g_topology_unlock();
	}
	mtx_lock(&Giant);
	g_rattle();
	if (error == ENOIOCTL) {
		i = IOCGROUP(cmd);
		printf("IOCTL(0x%lx) \"%s\"", cmd, gp->name);
		if (i > ' ' && i <= '~')
			printf(" '%c'", (int)IOCGROUP(cmd));
		else
			printf(" 0x%lx", IOCGROUP(cmd));
		printf("/%ld ", cmd & 0xff);
		if (cmd & IOC_IN)
			printf("I");
		if (cmd & IOC_OUT)
			printf("O");
		printf("(%ld) = ENOIOCTL\n", IOCPARM_LEN(cmd));
		error = ENOTTY;
	}
	return (error);
}

static int
g_dev_psize(dev_t dev)
{
	struct g_consumer *cp;
	int i, error;
	off_t mediasize;

	cp = dev->si_drv2;

	i = sizeof mediasize;
	error = g_io_getattr("GEOM::mediasize", cp, &i, &mediasize, NULL);
	if (error)
		return (-1);
	return (mediasize >> DEV_BSHIFT);
}

static void
g_dev_done(struct bio *bp2)
{
	struct bio *bp;

	bp = bp2->bio_linkage;
	bp->bio_error = bp2->bio_error;
	if (bp->bio_error != 0) {
		g_trace(G_T_BIO, "g_dev_done(%p) had error %d",
		    bp2, bp->bio_error);
		bp->bio_flags |= BIO_ERROR;
	} else {
		g_trace(G_T_BIO, "g_dev_done(%p/%p) resid %ld completed %lld",
		    bp2, bp, bp->bio_resid, bp2->bio_completed);
	}
	bp->bio_resid = bp->bio_bcount - bp2->bio_completed;
	g_destroy_bio(bp2);
	mtx_lock(&Giant);
	biodone(bp);
	mtx_unlock(&Giant);
}

static void
g_dev_strategy(struct bio *bp)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct bio *bp2;
	dev_t dev;

	mtx_unlock(&Giant);
	dev = bp->bio_dev;
	gp = dev->si_drv1;
	cp = dev->si_drv2;
	bp2 = g_clone_bio(bp);
	bp2->bio_offset = (off_t)bp->bio_blkno << DEV_BSHIFT;
	bp2->bio_length = (off_t)bp->bio_bcount;
	bp2->bio_done = g_dev_done;
	g_trace(G_T_BIO,
	    "g_dev_strategy(%p/%p) offset %lld length %lld data %p cmd %d",
	    bp, bp2, bp->bio_offset, bp2->bio_length, bp2->bio_data,
	    bp2->bio_cmd);
	g_io_request(bp2, cp);
	mtx_lock(&Giant);
}


static void
g_dev_orphan(struct g_consumer *cp, struct thread *tp)
{
	struct g_geom *gp;
	dev_t dev;

	gp = cp->geom;
	g_trace(G_T_TOPOLOGY, "g_dev_orphan(%p(%s))", cp, gp->name);
	g_topology_assert();
	if (cp->biocount > 0)
		return;
	dev = gp->softc;
	destroy_dev(dev);
	if (cp->acr > 0 || cp->acw > 0 || cp->ace > 0)
		g_access_rel(cp, -cp->acr, -cp->acw, -cp->ace);
	g_dettach(cp);
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
}

DECLARE_GEOM_METHOD(g_dev_method, g_dev)

