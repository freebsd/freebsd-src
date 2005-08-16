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
#include <sys/kthread.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include <geom/geom.h>
#include <geom/vinum/geom_vinum_var.h>
#include <geom/vinum/geom_vinum.h>

static void gv_vol_completed_request(struct gv_volume *, struct bio *);
static void gv_vol_normal_request(struct gv_volume *, struct bio *);

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
	if (v != NULL) {
		gv_kill_vol_thread(v);
		v->geom = NULL;
	}
	gp->softc = NULL;
	g_wither_geom(gp, error);
}

/* We end up here after the requests to our plexes are done. */
static void
gv_volume_done(struct bio *bp)
{
	struct gv_volume *v;
	struct gv_bioq *bq;

	v = bp->bio_from->geom->softc;
	bp->bio_cflags |= GV_BIO_DONE;
	bq = g_malloc(sizeof(*bq), M_NOWAIT | M_ZERO);
	bq->bp = bp;
	mtx_lock(&v->bqueue_mtx);
	TAILQ_INSERT_TAIL(&v->bqueue, bq, queue);
	wakeup(v);
	mtx_unlock(&v->bqueue_mtx);
}

static void
gv_volume_start(struct bio *bp)
{
	struct gv_volume *v;
	struct gv_bioq *bq;

	switch(bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		break;
	case BIO_GETATTR:
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}

	v = bp->bio_to->geom->softc;
	if (v->state != GV_VOL_UP) {
		g_io_deliver(bp, ENXIO);
		return;
	}

	bq = g_malloc(sizeof(*bq), M_NOWAIT | M_ZERO);
	bq->bp = bp;
	mtx_lock(&v->bqueue_mtx);
	TAILQ_INSERT_TAIL(&v->bqueue, bq, queue);
	wakeup(v);
	mtx_unlock(&v->bqueue_mtx);
}

static void
gv_vol_worker(void *arg)
{
	struct bio *bp;
	struct gv_volume *v;
	struct gv_bioq *bq;

	v = arg;
	KASSERT(v != NULL, ("NULL v"));
	mtx_lock(&v->bqueue_mtx);
	for (;;) {
		/* We were signaled to exit. */
		if (v->flags & GV_VOL_THREAD_DIE)
			break;

		/* Take the first BIO from our queue. */
		bq = TAILQ_FIRST(&v->bqueue);
		if (bq == NULL) {
			msleep(v, &v->bqueue_mtx, PRIBIO, "-", hz/10);
			continue;
		}
		TAILQ_REMOVE(&v->bqueue, bq, queue);
		mtx_unlock(&v->bqueue_mtx);

		bp = bq->bp;
		g_free(bq);

		if (bp->bio_cflags & GV_BIO_DONE)
			gv_vol_completed_request(v, bp);
		else
			gv_vol_normal_request(v, bp);

		mtx_lock(&v->bqueue_mtx);
	}
	mtx_unlock(&v->bqueue_mtx);
	v->flags |= GV_VOL_THREAD_DEAD;
	wakeup(v);

	kthread_exit(ENXIO);
}

static void
gv_vol_completed_request(struct gv_volume *v, struct bio *bp)
{
	struct bio *pbp;
	struct g_geom *gp;
	struct g_consumer *cp, *cp2;
	struct gv_bioq *bq;

	pbp = bp->bio_parent;

	if (pbp->bio_error == 0)
		pbp->bio_error = bp->bio_error;

	switch (pbp->bio_cmd) {
	case BIO_READ:
		if (bp->bio_error == 0)
			break;

		if (pbp->bio_cflags & GV_BIO_RETRY)
			break;

		/* Check if we have another plex left. */
		cp = bp->bio_from;
		gp = cp->geom;
		cp2 = LIST_NEXT(cp, consumer);
		if (cp2 == NULL)
			break;

		if (LIST_NEXT(cp2, consumer) == NULL)
			pbp->bio_cflags |= GV_BIO_RETRY;

		g_destroy_bio(bp);
		pbp->bio_children--;
		bq = g_malloc(sizeof(*bq), M_WAITOK | M_ZERO);
		bq->bp = pbp;
		mtx_lock(&v->bqueue_mtx);
		TAILQ_INSERT_TAIL(&v->bqueue, bq, queue);
		mtx_unlock(&v->bqueue_mtx);
		return;

	case BIO_WRITE:
	case BIO_DELETE:
		/* Remember if this write request succeeded. */
		if (bp->bio_error == 0)
			pbp->bio_cflags |= GV_BIO_SUCCEED;
		break;
	}

	/* When the original request is finished, we deliver it. */
	pbp->bio_inbed++;
	if (pbp->bio_inbed == pbp->bio_children) {
		if (pbp->bio_cflags & GV_BIO_SUCCEED)
			pbp->bio_error = 0;
		pbp->bio_completed = bp->bio_length;
		g_io_deliver(pbp, pbp->bio_error);
	}

	g_destroy_bio(bp);
}

static void
gv_vol_normal_request(struct gv_volume *v, struct bio *bp)
{
	struct bio_queue_head queue;
	struct g_geom *gp;
	struct gv_plex *p, *lp;
	struct bio *cbp;

	gp = v->geom;

	switch (bp->bio_cmd) {
	case BIO_READ:
		cbp = g_clone_bio(bp);
		if (cbp == NULL) {
			g_io_deliver(bp, ENOMEM);
			return;
		}
		cbp->bio_done = gv_volume_done;
		/*
		 * Try to find a good plex where we can send the request to.
		 * The plex either has to be up, or it's a degraded RAID5 plex.
		 */
		lp = v->last_read_plex;
		if (lp == NULL)
			lp = LIST_FIRST(&v->plexes);
		p = LIST_NEXT(lp, in_volume);
		do {
			if (p == NULL)
				p = LIST_FIRST(&v->plexes);
			if ((p->state > GV_PLEX_DEGRADED) ||
			    (p->state >= GV_PLEX_DEGRADED &&
			    p->org == GV_PLEX_RAID5))
				break;
			p = LIST_NEXT(p, in_volume);
		} while (p != lp);

		if (p == NULL ||
		    (p->org == GV_PLEX_RAID5 && p->state < GV_PLEX_DEGRADED) ||
		    (p->state <= GV_PLEX_DEGRADED)) {
			g_destroy_bio(cbp);
			bp->bio_children--;
			g_io_deliver(bp, ENXIO);
			return;
		}
		g_io_request(cbp, p->consumer);
		v->last_read_plex = p;

		break;

	case BIO_WRITE:
	case BIO_DELETE:
		bioq_init(&queue);
		LIST_FOREACH(p, &v->plexes, in_volume) {
			if (p->state < GV_PLEX_DEGRADED)
				continue;
			cbp = g_clone_bio(bp);
			if (cbp == NULL) {
				for (cbp = bioq_first(&queue); cbp != NULL;
				    cbp = bioq_first(&queue)) {
					bioq_remove(&queue, cbp);
					g_destroy_bio(cbp);
				}
				if (bp->bio_error == 0)
					bp->bio_error = ENOMEM;
				g_io_deliver(bp, bp->bio_error);
				return;
			}
			bioq_insert_tail(&queue, cbp);
			cbp->bio_done = gv_volume_done;
			cbp->bio_caller1 = p->consumer;
		}
		/* Fire off all sub-requests. */
		for (cbp = bioq_first(&queue); cbp != NULL;
		     cbp = bioq_first(&queue)) {
			bioq_remove(&queue, cbp);
			g_io_request(cbp, cbp->bio_caller1);
		}
		break;
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

	/* Let's see if the volume this plex wants is already configured. */
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
		TAILQ_INIT(&v->bqueue);
	} else
		gp = v->geom;

	/* Create bio queue mutex and worker thread, if necessary. */
	if (mtx_initialized(&v->bqueue_mtx) == 0)
		mtx_init(&v->bqueue_mtx, "gv_plex", NULL, MTX_DEF);

	if (!(v->flags & GV_VOL_THREAD_ACTIVE)) {
		kthread_create(gv_vol_worker, v, NULL, 0, 0, "gv_v %s",
		    v->name);
		v->flags |= GV_VOL_THREAD_ACTIVE;
	}

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
	struct gv_volume *v;

	g_trace(G_T_TOPOLOGY, "gv_volume_destroy_geom: %s", gp->name);
	g_topology_assert();

	v = gp->softc;
	gv_kill_vol_thread(v);
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
