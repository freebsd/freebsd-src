/*-
 * Copyright (c) 2004 Lukas Ertl
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
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include <geom/geom.h>
#include <geom/vinum/geom_vinum_var.h>
#include <geom/vinum/geom_vinum.h>

static void
gv_volume_orphan(struct g_consumer *cp)
{
	struct g_geom *gp;
	struct gv_volume *v;
	int error;

	g_topology_assert();
	gp = cp->geom;
	g_trace(G_T_TOPOLOGY, "gv_volume_orphan(%s)", gp->name);
	if (cp->acr != 0 || cp->acw != 0 || cp->ace != 0)
		g_access(cp, -cp->acr, -cp->acw, -cp->ace);
	error = cp->provider->error;
	if (error == 0)
		error = ENXIO;
	g_detach(cp);
	g_destroy_consumer(cp);	
	if (!LIST_EMPTY(&gp->consumer))
		return;
	v = gp->softc;
	if (v != NULL)
		v->geom = NULL;
	gp->softc = NULL;
	g_wither_geom(gp, error);
}

/* We end up here after the requests to our plexes are done. */
static void
gv_volume_done(struct bio *bp)
{
	struct g_consumer *cp;
	
	/* The next plex in this volume. */
	cp = LIST_NEXT(bp->bio_from, consumer);

	switch (bp->bio_cmd) {
	case BIO_READ:
		/*
		 * If no error occured on this request, or if we have no plex
		 * left, finish here...
		 */
		if ((bp->bio_error == 0) || (cp == NULL)) {
			g_std_done(bp);
			return;
		}

		/* ... or try to read from the next plex. */
		g_io_request(bp, cp);
		return;

	case BIO_WRITE:
	case BIO_DELETE:
		/* No more plexes left. */
		if (cp == NULL) {
			/*
			 * Clear any errors if one of the previous writes
			 * succeeded.
			 */
			if (bp->bio_caller1 == (int *)1)
				bp->bio_error = 0;
			g_std_done(bp);
			return;
		}

		/* If this write request had no errors, remember that fact... */
		if (bp->bio_error == 0)
			bp->bio_caller1 = (int *)1;

		/* ... and write to the next plex. */
		g_io_request(bp, cp);
		return;
	}
}

static void
gv_volume_start(struct bio *bp)
{
	struct g_geom *gp;
	struct bio *bp2;
	struct gv_volume *v;

	gp = bp->bio_to->geom;
	v = gp->softc;
	if (v->state != GV_VOL_UP) {
		g_io_deliver(bp, ENXIO);
		return;
	}
	switch(bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		bp2 = g_clone_bio(bp);
		if (bp2 == NULL) {
			g_io_deliver(bp, ENOMEM);
			return;
		}
		bp2->bio_done = gv_volume_done;
		g_io_request(bp2, LIST_FIRST(&gp->consumer));
		return;
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}
}

static int
gv_volume_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_geom *gp;
	struct g_consumer *cp, *cp2;
	int error;

	gp = pp->geom;

	error = ENXIO;
	LIST_FOREACH(cp, &gp->consumer, consumer) {
		error = g_access(cp, dr, dw, de);
		if (error) {
			LIST_FOREACH(cp2, &gp->consumer, consumer) {
				if (cp == cp2)
					break;
				g_access(cp2, -dr, -dw, -de);
			}
			return (error);
		}
	}
	return (error);
}

static struct g_geom *
gv_volume_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
	struct g_geom *gp;
	struct g_provider *pp2;
	struct g_consumer *cp, *ocp;
	struct gv_softc *sc;
	struct gv_volume *v;
	struct gv_plex *p;
	int error, first;

	g_trace(G_T_TOPOLOGY, "gv_volume_taste(%s, %s)", mp->name, pp->name);
	g_topology_assert();

	/* First, find the VINUM class and its associated geom. */
	gp = find_vinum_geom();
	if (gp == NULL)
		return (NULL);

	sc = gp->softc;
	KASSERT(sc != NULL, ("gv_volume_taste: NULL sc"));

	gp = pp->geom;

	/* We only want to attach to plexes. */
	if (strcmp(gp->class->name, "VINUMPLEX"))
		return (NULL);

	first = 0;
	p = gp->softc;
	v = gv_find_vol(sc, p->volume);
	if (v == NULL)
		return (NULL);
	if (v->geom == NULL) {
		gp = g_new_geomf(mp, "%s", p->volume);
		gp->start = gv_volume_start;
		gp->orphan = gv_volume_orphan;
		gp->access = gv_volume_access;
		gp->softc = v;
		first++;
	} else
		gp = v->geom;

	/*
	 * Create a new consumer and attach it to the plex geom.  Since this
	 * volume might already have a plex attached, we need to adjust the
	 * access counts of the new consumer.
	 */
	ocp = LIST_FIRST(&gp->consumer);
	cp = g_new_consumer(gp);
	g_attach(cp, pp);
	if ((ocp != NULL) && (ocp->acr > 0 || ocp->acw > 0 || ocp->ace > 0)) {
		error = g_access(cp, ocp->acr, ocp->acw, ocp->ace);
		if (error) {
			printf("GEOM_VINUM: failed g_access %s -> %s; "
			    "errno %d\n", v->name, p->name, error);
			g_detach(cp);
			g_destroy_consumer(cp);
			if (first)
				g_destroy_geom(gp);
			return (NULL);
		}
	}

	p->consumer = cp;

	if (p->vol_sc != v) {
		p->vol_sc = v;
		v->plexcount++;
		LIST_INSERT_HEAD(&v->plexes, p, in_volume);
	}

	/* We need to setup a new VINUMVOLUME geom. */
	if (first) {
		pp2 = g_new_providerf(gp, "gvinum/%s", v->name);
		pp2->mediasize = pp->mediasize;
		pp2->sectorsize = pp->sectorsize;
		g_error_provider(pp2, 0);
		v->size = pp2->mediasize;
		v->geom = gp;
		return (gp);
	}

	return (NULL);
}

static int
gv_volume_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp)
{
	g_trace(G_T_TOPOLOGY, "gv_volume_destroy_geom: %s", gp->name);
	g_topology_assert();

	g_wither_geom(gp, ENXIO);
	return (0);
}

#define	VINUMVOLUME_CLASS_NAME "VINUMVOLUME"

static struct g_class g_vinum_volume_class = {
	.name = VINUMVOLUME_CLASS_NAME,
	.version = G_VERSION,
	.taste = gv_volume_taste,
	.destroy_geom = gv_volume_destroy_geom,
};

DECLARE_GEOM_CLASS(g_vinum_volume_class, g_vinum_volume);
