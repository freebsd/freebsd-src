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
__FBSDID("$FreeBSD: src/sys/geom/vinum/geom_vinum_plex.c,v 1.17 2006/01/06 18:03:17 le Exp $");

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

static void gv_plex_completed_request(struct gv_plex *, struct bio *);
static void gv_plex_normal_request(struct gv_plex *, struct bio *);
static void gv_plex_worker(void *);
static int gv_check_parity(struct gv_plex *, struct bio *,
    struct gv_raid5_packet *);
static int gv_normal_parity(struct gv_plex *, struct bio *,
    struct gv_raid5_packet *);

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
		gv_kill_plex_thread(p);
		p->geom = NULL;
		p->provider = NULL;
		p->consumer = NULL;
	}
	gp->softc = NULL;
	g_wither_geom(gp, error);
}

void
gv_plex_done(struct bio *bp)
{
	struct gv_plex *p;

	p = bp->bio_from->geom->softc;
	bp->bio_cflags |= GV_BIO_DONE;
	mtx_lock(&p->bqueue_mtx);
	bioq_insert_tail(p->bqueue, bp);
	wakeup(p);
	mtx_unlock(&p->bqueue_mtx);
}

/* Find the correct subdisk to send the bio to and build a bio to send. */
static int
gv_plexbuffer(struct gv_plex *p, struct bio *bp, caddr_t addr, off_t boff, off_t bcount)
{
	struct g_geom *gp;
	struct gv_sd *s;
	struct bio *cbp, *pbp;
	int i, sdno;
	off_t len_left, real_len, real_off;
	off_t stripeend, stripeno, stripestart;

	if (p == NULL || LIST_EMPTY(&p->subdisks))
		return (ENXIO);

	s = NULL;
	gp = bp->bio_to->geom;

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
		if (!(bp->bio_cflags & GV_BIO_SYNCREQ))
			return (ENXIO);

		printf("GEOM_VINUM: sd %s is initializing\n", s->name);
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
	cbp->bio_done = g_std_done;
	cbp->bio_caller2 = s->consumer;
	if ((bp->bio_cflags & GV_BIO_SYNCREQ)) {
		cbp->bio_cflags |= GV_BIO_SYNCREQ;
		cbp->bio_done = gv_plex_done;
	}

	if (bp->bio_driver1 == NULL) {
		bp->bio_driver1 = cbp;
	} else {
		pbp = bp->bio_driver1;
		while (pbp->bio_caller1 != NULL)
			pbp = pbp->bio_caller1;
		pbp->bio_caller1 = cbp;
	}

	return (0);
}

static void
gv_plex_start(struct bio *bp)
{
	struct gv_plex *p;

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

	/*
	 * We cannot handle this request if too many of our subdisks are
	 * inaccessible.
	 */
	p = bp->bio_to->geom->softc;
	if ((p->state < GV_PLEX_DEGRADED) &&
	    !(bp->bio_cflags & GV_BIO_SYNCREQ)) {
		g_io_deliver(bp, ENXIO);
		return;
	}

	mtx_lock(&p->bqueue_mtx);
	bioq_disksort(p->bqueue, bp);
	wakeup(p);
	mtx_unlock(&p->bqueue_mtx);
}

static void
gv_plex_worker(void *arg)
{
	struct bio *bp;
	struct gv_plex *p;
	struct gv_sd *s;

	p = arg;
	KASSERT(p != NULL, ("NULL p"));

	mtx_lock(&p->bqueue_mtx);
	for (;;) {
		/* We were signaled to exit. */
		if (p->flags & GV_PLEX_THREAD_DIE)
			break;

		/* Take the first BIO from our queue. */
		bp = bioq_takefirst(p->bqueue);
		if (bp == NULL) {
			msleep(p, &p->bqueue_mtx, PRIBIO, "-", hz/10);
			continue;
		}
		mtx_unlock(&p->bqueue_mtx);

		/* A completed request. */
		if (bp->bio_cflags & GV_BIO_DONE) {
			if (bp->bio_cflags & GV_BIO_SYNCREQ ||
			    bp->bio_cflags & GV_BIO_REBUILD) {
				s = bp->bio_to->private;
				if (bp->bio_error == 0)
					s->initialized += bp->bio_length;
				if (s->initialized >= s->size) {
					g_topology_lock();
					gv_set_sd_state(s, GV_SD_UP,
					    GV_SETSTATE_CONFIG);
					g_topology_unlock();
					s->initialized = 0;
				}
			}

			if (bp->bio_cflags & GV_BIO_SYNCREQ)
				g_std_done(bp);
			else
				gv_plex_completed_request(p, bp);
		/*
		 * A sub-request that was hold back because it interfered with
		 * another sub-request.
		 */
		} else if (bp->bio_cflags & GV_BIO_ONHOLD) {
			/* Is it still locked out? */
			if (gv_stripe_active(p, bp)) {
				/* Park the bio on the waiting queue. */
				mtx_lock(&p->bqueue_mtx);
				bioq_disksort(p->wqueue, bp);
				mtx_unlock(&p->bqueue_mtx);
			} else {
				bp->bio_cflags &= ~GV_BIO_ONHOLD;
				g_io_request(bp, bp->bio_caller2);
			}

		/* A normal request to this plex. */
		} else
			gv_plex_normal_request(p, bp);

		mtx_lock(&p->bqueue_mtx);
	}
	mtx_unlock(&p->bqueue_mtx);
	p->flags |= GV_PLEX_THREAD_DEAD;
	wakeup(p);

	kthread_exit(ENXIO);
}

static int
gv_normal_parity(struct gv_plex *p, struct bio *bp, struct gv_raid5_packet *wp)
{
	struct bio *cbp, *pbp;
	int finished, i;

	finished = 1;

	if (wp->waiting != NULL) {
		pbp = wp->waiting;
		wp->waiting = NULL;
		cbp = wp->parity;
		for (i = 0; i < wp->length; i++)
			cbp->bio_data[i] ^= pbp->bio_data[i];
		g_io_request(pbp, pbp->bio_caller2);
		finished = 0;

	} else if (wp->parity != NULL) {
		cbp = wp->parity;
		wp->parity = NULL;
		g_io_request(cbp, cbp->bio_caller2);
		finished = 0;
	}

	return (finished);
}

static int
gv_check_parity(struct gv_plex *p, struct bio *bp, struct gv_raid5_packet *wp)
{
	struct bio *pbp;
	int err, finished, i;

	err = 0;
	finished = 1;

	if (wp->waiting != NULL) {
		pbp = wp->waiting;
		wp->waiting = NULL;
		g_io_request(pbp, pbp->bio_caller2);
		finished = 0;

	} else if (wp->parity != NULL) {
		pbp = wp->parity;
		wp->parity = NULL;

		/* Check if the parity is correct. */
		for (i = 0; i < wp->length; i++) {
			if (bp->bio_data[i] != pbp->bio_data[i]) {
				err = 1;
				break;
			}
		}

		/* The parity is not correct... */
		if (err) {
			bp->bio_parent->bio_error = EAGAIN;

			/* ... but we rebuild it. */
			if (bp->bio_parent->bio_cflags & GV_BIO_PARITY) {
				g_io_request(pbp, pbp->bio_caller2);
				finished = 0;
			}
		}

		/*
		 * Clean up the BIO we would have used for rebuilding the
		 * parity.
		 */
		if (finished) {
			bp->bio_parent->bio_inbed++;
			g_destroy_bio(pbp);
		}

	}

	return (finished);
}

void
gv_plex_completed_request(struct gv_plex *p, struct bio *bp)
{
	struct bio *cbp, *pbp;
	struct gv_bioq *bq, *bq2;
	struct gv_raid5_packet *wp;
	int i;

	wp = bp->bio_driver1;

	switch (bp->bio_parent->bio_cmd) {
	case BIO_READ:
		if (wp == NULL)
			break;

		TAILQ_FOREACH_SAFE(bq, &wp->bits, queue, bq2) {
			if (bq->bp == bp) {
				TAILQ_REMOVE(&wp->bits, bq, queue);
				g_free(bq);
				for (i = 0; i < wp->length; i++)
					wp->data[i] ^= bp->bio_data[i];
				break;
			}
		}
		if (TAILQ_EMPTY(&wp->bits)) {
			bp->bio_parent->bio_completed += wp->length;
			if (wp->lockbase != -1) {
				TAILQ_REMOVE(&p->packets, wp, list);
				/* Bring the waiting bios back into the game. */
				mtx_lock(&p->bqueue_mtx);
				pbp = bioq_takefirst(p->wqueue);
				while (pbp != NULL) {
					bioq_disksort(p->bqueue, pbp);
					pbp = bioq_takefirst(p->wqueue);
				}
				mtx_unlock(&p->bqueue_mtx);
			}
			g_free(wp);
		}

		break;

 	case BIO_WRITE:
		if (wp == NULL)
			break;

		/* Check if we need to handle parity data. */
		TAILQ_FOREACH_SAFE(bq, &wp->bits, queue, bq2) {
			if (bq->bp == bp) {
				TAILQ_REMOVE(&wp->bits, bq, queue);
				g_free(bq);
				cbp = wp->parity;
				if (cbp != NULL) {
					for (i = 0; i < wp->length; i++)
						cbp->bio_data[i] ^=
						    bp->bio_data[i];
				}
				break;
			}
		}

		/* Handle parity data. */
		if (TAILQ_EMPTY(&wp->bits)) {
			if (bp->bio_parent->bio_cflags & GV_BIO_CHECK)
				i = gv_check_parity(p, bp, wp);
			else
				i = gv_normal_parity(p, bp, wp);

			/* All of our sub-requests have finished. */
			if (i) {
				bp->bio_parent->bio_completed += wp->length;
				TAILQ_REMOVE(&p->packets, wp, list);
				/* Bring the waiting bios back into the game. */
				mtx_lock(&p->bqueue_mtx);
				pbp = bioq_takefirst(p->wqueue);
				while (pbp != NULL) {
					bioq_disksort(p->bqueue, pbp);
					pbp = bioq_takefirst(p->wqueue);
				}
				mtx_unlock(&p->bqueue_mtx);
				g_free(wp);
			}
		}

		break;
	}

	pbp = bp->bio_parent;
	if (pbp->bio_error == 0)
		pbp->bio_error = bp->bio_error;

	/* When the original request is finished, we deliver it. */
	pbp->bio_inbed++;
	if (pbp->bio_inbed == pbp->bio_children)
		g_io_deliver(pbp, pbp->bio_error);

	/* Clean up what we allocated. */
	if (bp->bio_cflags & GV_BIO_MALLOC)
		g_free(bp->bio_data);
	g_destroy_bio(bp);
}

void
gv_plex_normal_request(struct gv_plex *p, struct bio *bp)
{
	struct bio *cbp, *pbp;
	struct gv_bioq *bq, *bq2;
	struct gv_raid5_packet *wp, *wp2;
	caddr_t addr;
	off_t bcount, boff;
	int err;

	bcount = bp->bio_length;
	addr = bp->bio_data;
	boff = bp->bio_offset;

	/* Walk over the whole length of the request, we might split it up. */
	while (bcount > 0) {
		wp = NULL;

 		/*
		 * RAID5 plexes need special treatment, as a single write
		 * request involves several read/write sub-requests.
 		 */
		if (p->org == GV_PLEX_RAID5) {
			wp = g_malloc(sizeof(*wp), M_WAITOK | M_ZERO);
			wp->bio = bp;
			TAILQ_INIT(&wp->bits);

			if (bp->bio_cflags & GV_BIO_REBUILD)
				err = gv_rebuild_raid5(p, wp, bp, addr,
				    boff, bcount);
			else if (bp->bio_cflags & GV_BIO_CHECK)
				err = gv_check_raid5(p, wp, bp, addr,
				    boff, bcount);
			else
				err = gv_build_raid5_req(p, wp, bp, addr,
				    boff, bcount);

 			/*
			 * Building the sub-request failed, we probably need to
			 * clean up a lot.
 			 */
 			if (err) {
				printf("GEOM_VINUM: plex request failed for ");
				g_print_bio(bp);
				printf("\n");
				TAILQ_FOREACH_SAFE(bq, &wp->bits, queue, bq2) {
					TAILQ_REMOVE(&wp->bits, bq, queue);
					g_free(bq);
				}
				if (wp->waiting != NULL) {
					if (wp->waiting->bio_cflags &
					    GV_BIO_MALLOC)
						g_free(wp->waiting->bio_data);
					g_destroy_bio(wp->waiting);
				}
				if (wp->parity != NULL) {
					if (wp->parity->bio_cflags &
					    GV_BIO_MALLOC)
						g_free(wp->parity->bio_data);
					g_destroy_bio(wp->parity);
				}
				g_free(wp);

				TAILQ_FOREACH_SAFE(wp, &p->packets, list, wp2) {
					if (wp->bio == bp) {
						TAILQ_REMOVE(&p->packets, wp,
						    list);
						TAILQ_FOREACH_SAFE(bq,
						    &wp->bits, queue, bq2) {
							TAILQ_REMOVE(&wp->bits,
							    bq, queue);
							g_free(bq);
						}
						g_free(wp);
					}
				}

				cbp = bp->bio_driver1;
				while (cbp != NULL) {
					pbp = cbp->bio_caller1;
					if (cbp->bio_cflags & GV_BIO_MALLOC)
						g_free(cbp->bio_data);
					g_destroy_bio(cbp);
					cbp = pbp;
				}

				g_io_deliver(bp, err);
 				return;
 			}
 
			if (TAILQ_EMPTY(&wp->bits))
				g_free(wp);
			else if (wp->lockbase != -1)
				TAILQ_INSERT_TAIL(&p->packets, wp, list);

		/*
		 * Requests to concatenated and striped plexes go straight
		 * through.
		 */
		} else {
			err = gv_plexbuffer(p, bp, addr, boff, bcount);

			/* Building the sub-request failed. */
			if (err) {
				printf("GEOM_VINUM: plex request failed for ");
				g_print_bio(bp);
				printf("\n");
				cbp = bp->bio_driver1;
				while (cbp != NULL) {
					pbp = cbp->bio_caller1;
					g_destroy_bio(cbp);
					cbp = pbp;
				}
				g_io_deliver(bp, err);
				return;
			}
		}
 
		/* Abuse bio_caller1 as linked list. */
		pbp = bp->bio_driver1;
		while (pbp->bio_caller1 != NULL)
			pbp = pbp->bio_caller1;
		bcount -= pbp->bio_length;
		addr += pbp->bio_length;
		boff += pbp->bio_length;
	}

	/* Fire off all sub-requests. */
	pbp = bp->bio_driver1;
	while (pbp != NULL) {
		/*
		 * RAID5 sub-requests need to come in correct order, otherwise
		 * we trip over the parity, as it might be overwritten by
		 * another sub-request.
		 */
		if (pbp->bio_driver1 != NULL &&
		    gv_stripe_active(p, pbp)) {
			/* Park the bio on the waiting queue. */
			pbp->bio_cflags |= GV_BIO_ONHOLD;
			mtx_lock(&p->bqueue_mtx);
			bioq_disksort(p->wqueue, pbp);
			mtx_unlock(&p->bqueue_mtx);
		} else
			g_io_request(pbp, pbp->bio_caller2);
		pbp = pbp->bio_caller1;
	}
}

static int
gv_plex_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_geom *gp;
	struct g_consumer *cp, *cp2;
	int error;

	gp = pp->geom;

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
	return (0);
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
	if (p == NULL) {
		printf("gv_plex_taste: NULL p for '%s'\n", s->name);
		return (NULL);
	}

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

		/* Update the size of the volume this plex is attached to. */
		if (p->vol_sc != NULL)
			gv_update_vol_size(p->vol_sc, p->size);

		/*
		 * If necessary, create bio queues, queue mutex and a worker
		 * thread.
		 */
		if (p->bqueue == NULL) {
			p->bqueue = g_malloc(sizeof(struct bio_queue_head),
			    M_WAITOK | M_ZERO);
			bioq_init(p->bqueue);
		}
		if (p->wqueue == NULL) {
			p->wqueue = g_malloc(sizeof(struct bio_queue_head),
			    M_WAITOK | M_ZERO);
			bioq_init(p->wqueue);
		}
		if (mtx_initialized(&p->bqueue_mtx) == 0)
			mtx_init(&p->bqueue_mtx, "gv_plex", NULL, MTX_DEF);
		if (!(p->flags & GV_PLEX_THREAD_ACTIVE)) {
			kthread_create(gv_plex_worker, p, NULL, 0, 0, "gv_p %s",
			    p->name);
			p->flags |= GV_PLEX_THREAD_ACTIVE;
		}

		return (NULL);

	/* We need to create a new geom. */
	} else {
		gp = g_new_geomf(mp, "%s", p->name);
		gp->start = gv_plex_start;
		gp->orphan = gv_plex_orphan;
		gp->access = gv_plex_access;
		gp->softc = p;
		p->geom = gp;

		TAILQ_INIT(&p->packets);
		p->bqueue = g_malloc(sizeof(struct bio_queue_head),
		    M_WAITOK | M_ZERO);
		bioq_init(p->bqueue);
		p->wqueue = g_malloc(sizeof(struct bio_queue_head),
		    M_WAITOK | M_ZERO);
		bioq_init(p->wqueue);
		mtx_init(&p->bqueue_mtx, "gv_plex", NULL, MTX_DEF);
		kthread_create(gv_plex_worker, p, NULL, 0, 0, "gv_p %s",
		    p->name);
		p->flags |= GV_PLEX_THREAD_ACTIVE;

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
	gv_kill_plex_thread(p);
	/* g_free(sc); */
	g_wither_geom(gp, ENXIO);
	return (0);
}

#define	VINUMPLEX_CLASS_NAME "VINUMPLEX"

static struct g_class g_vinum_plex_class = {
	.name = VINUMPLEX_CLASS_NAME,
	.version = G_VERSION,
	.taste = gv_plex_taste,
	.destroy_geom = gv_plex_destroy_geom,
};

DECLARE_GEOM_CLASS(g_vinum_plex_class, g_vinum_plex);
