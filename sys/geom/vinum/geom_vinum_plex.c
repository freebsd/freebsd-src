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
#include <geom/vinum/geom_vinum_raid5.h>
#include <geom/vinum/geom_vinum.h>

/* XXX: is this the place to catch dying subdisks? */
static void
gv_plex_orphan(struct g_consumer *cp)
{
	struct g_geom *gp;
	struct gv_plex *p;
	int error;

	g_topology_assert();
	gp = cp->geom;
	g_trace(G_T_TOPOLOGY, "gv_plex_orphan(%s)", gp->name);

	if (cp->acr != 0 || cp->acw != 0 || cp->ace != 0)
		g_access(cp, -cp->acr, -cp->acw, -cp->ace);
	error = cp->provider->error;
	if (error == 0)
		error = ENXIO;
	g_detach(cp);
	g_destroy_consumer(cp);	
	if (!LIST_EMPTY(&gp->consumer))
		return;

	p = gp->softc;
	if (p != NULL) {
		gv_kill_thread(p);
		p->geom = NULL;
		p->provider = NULL;
		p->consumer = NULL;
	}
	gp->softc = NULL;
	g_wither_geom(gp, error);
}

static void
gv_plex_done(struct bio *bp)
{
	struct g_geom *gp;
	struct gv_sd *s;
	
	gp = bp->bio_to->geom;

	s = bp->bio_caller1;
	KASSERT(s != NULL, ("gv_plex_done: NULL s"));

	if (bp->bio_error == 0)
		s->initialized += bp->bio_length;
	
	if (s->initialized >= s->size) {
		gv_set_sd_state(s, GV_SD_UP, 0);
		s->initialized = 0;
	}

	g_std_done(bp);
}

/* Find the correct subdisk to send the bio to and build a bio to send. */
static int
gv_plexbuffer(struct bio *bp, struct bio **bp2, struct g_consumer **cp,
    caddr_t addr, long bcount, off_t boff)
{
	struct g_geom *gp;
	struct gv_plex *p;
	struct gv_sd *s;
	struct bio *cbp;
	int i, sdno;
	off_t len_left, real_len, real_off, stripeend, stripeno, stripestart;

	s = NULL;

	gp = bp->bio_to->geom;
	p = gp->softc;

	if (p == NULL || LIST_EMPTY(&p->subdisks))
		return (ENXIO);

	/*
	 * We only handle concatenated and striped plexes here.  RAID5 plexes
	 * are handled in build_raid5_request().
	 */
	switch (p->org) {
	case GV_PLEX_CONCAT:
		/*
		 * Find the subdisk where this request starts.  The subdisks in
		 * this list must be ordered by plex_offset.
		 */
		LIST_FOREACH(s, &p->subdisks, in_plex) {
			if (s->plex_offset <= boff &&
			    s->plex_offset + s->size > boff)
				break;
		}
		/* Subdisk not found. */
		if (s == NULL)
			return (ENXIO);

		/* Calculate corresponding offsets on disk. */
		real_off = boff - s->plex_offset;
		len_left = s->size - real_off;
		real_len = (bcount > len_left) ? len_left : bcount;
		break;

	case GV_PLEX_STRIPED:
		/* The number of the stripe where the request starts. */
		stripeno = boff / p->stripesize;

		/* The number of the subdisk where the stripe resides. */
		sdno = stripeno % p->sdcount;

		/* Find the right subdisk. */
		i = 0;
		LIST_FOREACH(s, &p->subdisks, in_plex) {
			if (i == sdno)
				break;
			i++;
		}

		/* Subdisk not found. */
		if (s == NULL)
			return (ENXIO);

		/* The offset of the stripe from the start of the subdisk. */ 
		stripestart = (stripeno / p->sdcount) *
		    p->stripesize;

		/* The offset at the end of the stripe. */
		stripeend = stripestart + p->stripesize;

		/* The offset of the request on this subdisk. */
		real_off = boff - (stripeno * p->stripesize) +
		    stripestart;

		/* The length left in this stripe. */
		len_left = stripeend - real_off;

		real_len = (bcount <= len_left) ? bcount : len_left;
		break;

	default:
		return (EINVAL);
	}

	/* Now check if we can handle the request on this subdisk. */
	switch (s->state) {
	case GV_SD_UP:
		/* If the subdisk is up, just continue. */
		break;

	case GV_SD_STALE:
		if (bp->bio_caller1 != p)
			return (ENXIO);

		printf("FOO: setting sd %s to GV_SD_INITIALIZING\n", s->name);
		gv_set_sd_state(s, GV_SD_INITIALIZING, GV_SETSTATE_FORCE);
		break;

	case GV_SD_INITIALIZING:
		if (bp->bio_cmd == BIO_READ)
			return (ENXIO);
		break;

	default:
		/* All other subdisk states mean it's not accessible. */
		return (ENXIO);
	}

	/* Clone the bio and adjust the offsets and sizes. */
	cbp = g_clone_bio(bp);
	if (cbp == NULL)
		return (ENOMEM);
	cbp->bio_offset = real_off;
	cbp->bio_length = real_len;
	cbp->bio_data = addr;
	if (bp->bio_caller1 == p) {
		cbp->bio_caller1 = s;
		cbp->bio_done = gv_plex_done;
	} else
		cbp->bio_done = g_std_done;
	*bp2 = cbp;
	*cp = s->consumer;
	return (0);
}

static void
gv_plex_start(struct bio *bp)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct gv_plex *p;
	struct gv_raid5_packet *wp;
	struct bio *bp2;
	caddr_t addr;
	off_t boff;
	long bcount, rcount;
	int err;

	gp = bp->bio_to->geom;
	p = gp->softc;

	/*
	 * We cannot handle this request if too many of our subdisks are
	 * inaccessible.
	 */
	if ((p->state < GV_PLEX_DEGRADED) && (bp->bio_caller1 != p)) {
		g_io_deliver(bp, ENXIO);  /* XXX: correct way? */
		return;
	}

	switch(bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		/*
		 * We split up the request in smaller packets and hand them
		 * down to our subdisks.
		 */
		wp = NULL;
		addr = bp->bio_data;
		boff = bp->bio_offset;
		for (bcount = bp->bio_length; bcount > 0; bcount -= rcount) {
			/*
			 * RAID5 requests usually need to be split up in
			 * several subrequests.
			 */
			if (p->org == GV_PLEX_RAID5) {
				wp = gv_new_raid5_packet();
				wp->bio = bp;
				err = gv_build_raid5_req(wp, bp, addr, bcount,
				    boff);
			} else
				err = gv_plexbuffer(bp, &bp2, &cp, addr, bcount,
				    boff);

			if (err) {
				if (p->org == GV_PLEX_RAID5)
					gv_free_raid5_packet(wp);
				bp->bio_completed += bcount;
				if (bp->bio_error == 0)
					bp->bio_error = err;
				if (bp->bio_completed == bp->bio_length)
					g_io_deliver(bp, bp->bio_error);
				return;
			}
		
			if (p->org != GV_PLEX_RAID5) {
				rcount = bp2->bio_length;
				g_io_request(bp2, cp);

			/*
			 * RAID5 subrequests are queued on a worklist
			 * and picked up from the worker thread.  This
			 * ensures correct order.
			 */
			} else {
				mtx_lock(&p->worklist_mtx);
				TAILQ_INSERT_TAIL(&p->worklist, wp,
				    list);
				mtx_unlock(&p->worklist_mtx);
				wakeup(&p);
				rcount = wp->length;
			}

			boff += rcount;
			addr += rcount;
		}
		return;

	default:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}
}

static int
gv_plex_access(struct g_provider *pp, int dr, int dw, int de)
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
gv_plex_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
	struct g_geom *gp;
	struct g_consumer *cp, *cp2;
	struct g_provider *pp2;
	struct gv_plex *p;
	struct gv_sd *s;
	struct gv_softc *sc;
	int error;

	g_trace(G_T_TOPOLOGY, "gv_plex_taste(%s, %s)", mp->name, pp->name);
	g_topology_assert();

	/* We only want to attach to subdisks. */
	if (strcmp(pp->geom->class->name, "VINUMDRIVE"))
		return (NULL);

	/* Find the VINUM class and its associated geom. */
	gp = find_vinum_geom();
	if (gp == NULL)
		return (NULL);
	sc = gp->softc;
	KASSERT(sc != NULL, ("gv_plex_taste: NULL sc"));

	/* Find out which subdisk the offered provider corresponds to. */
	s = pp->private;
	KASSERT(s != NULL, ("gv_plex_taste: NULL s"));

	/* Now find the correct plex where this subdisk belongs to. */
	p = gv_find_plex(sc, s->plex);
	KASSERT(p != NULL, ("gv_plex_taste: NULL p"));

	/*
	 * Add this subdisk to this plex.  Since we trust the on-disk
	 * configuration, we don't check the given value (should we?).
	 * XXX: shouldn't be done here
	 */
	gv_sd_to_plex(p, s, 0);

	/* Now check if there's already a geom for this plex. */
	gp = p->geom;

	/* Yes, there is already a geom, so we just add the consumer. */
	if (gp != NULL) {
		cp2 = LIST_FIRST(&gp->consumer);
		/* Need to attach a new consumer to this subdisk. */
		cp = g_new_consumer(gp);
		error = g_attach(cp, pp);
		if (error) {
			printf("geom_vinum: couldn't attach consumer to %s\n",
			    pp->name);
			g_destroy_consumer(cp);
			return (NULL);
		}
		/* Adjust the access counts of the new consumer. */
		if ((cp2 != NULL) && (cp2->acr || cp2->acw || cp2->ace)) {
			error = g_access(cp, cp2->acr, cp2->acw, cp2->ace);
			if (error) {
				printf("geom_vinum: couldn't set access counts"
				    " for consumer on %s\n", pp->name);
				g_detach(cp);
				g_destroy_consumer(cp);
				return (NULL);
			}
		}
		s->consumer = cp;

		/* Adjust the size of the providers this plex has. */
		LIST_FOREACH(pp2, &gp->provider, provider)
			pp2->mediasize = p->size;

		return (NULL);

	/* We need to create a new geom. */
	} else {
		gp = g_new_geomf(mp, "%s", p->name);
		gp->start = gv_plex_start;
		gp->orphan = gv_plex_orphan;
		gp->access = gv_plex_access;
		gp->softc = p;
		p->geom = gp;

		/* RAID5 plexes need a 'worker' thread, where IO is handled. */
		if (p->org == GV_PLEX_RAID5) {
			TAILQ_INIT(&p->worklist);
			mtx_init(&p->worklist_mtx, "gvinum_worklist", NULL,
			    MTX_DEF);
			p->flags &= ~GV_PLEX_THREAD_DIE;
			kthread_create(gv_raid5_worker, gp, NULL, 0, 0,
			    "gv_raid5");
			p->flags |= GV_PLEX_THREAD_ACTIVE;
		}

		/* Attach a consumer to this provider. */
		cp = g_new_consumer(gp);
		g_attach(cp, pp);
		s->consumer = cp;

		/* Create a provider for the outside world. */
		pp2 = g_new_providerf(gp, "gvinum/plex/%s", p->name);
		pp2->mediasize = p->size;
		pp2->sectorsize = pp->sectorsize;
		p->provider = pp2;
		g_error_provider(pp2, 0);
		return (gp);
	}
}

static int
gv_plex_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp)
{
	struct gv_plex *p;

	g_trace(G_T_TOPOLOGY, "gv_plex_destroy_geom: %s", gp->name);
	g_topology_assert();

	p = gp->softc;

	KASSERT(p != NULL, ("gv_plex_destroy_geom: null p of '%s'", gp->name));

	/*
	 * If this is a RAID5 plex, check if its worker thread is still active
	 * and signal it to self destruct.
	 */
	gv_kill_thread(p);
	mtx_destroy(&p->worklist_mtx);
	/* g_free(sc); */
	g_wither_geom(gp, ENXIO);
	return (0);
}

#define	VINUMPLEX_CLASS_NAME "VINUMPLEX"

static struct g_class g_vinum_plex_class = {
	.name = VINUMPLEX_CLASS_NAME,
	.taste = gv_plex_taste,
	.destroy_geom = gv_plex_destroy_geom,
};

DECLARE_GEOM_CLASS(g_vinum_plex_class, g_vinum_plex);
