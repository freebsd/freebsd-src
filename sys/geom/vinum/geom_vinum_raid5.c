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
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include <geom/geom.h>
#include <geom/vinum/geom_vinum_var.h>
#include <geom/vinum/geom_vinum_raid5.h>
#include <geom/vinum/geom_vinum.h>

int	gv_raid5_parity(struct gv_raid5_packet *);
int	gv_stripe_active(struct gv_raid5_packet *, struct gv_plex *);

struct gv_raid5_bit *
gv_new_raid5_bit(void)
{
	struct gv_raid5_bit *r;
	r = g_malloc(sizeof(*r), M_NOWAIT | M_ZERO);
	KASSERT(r != NULL, ("gv_new_raid5_bit: NULL r"));
	return (r);
}

struct gv_raid5_packet *
gv_new_raid5_packet(void)
{
	struct gv_raid5_packet *wp;

	wp = g_malloc(sizeof(*wp), M_NOWAIT | M_ZERO);
	KASSERT(wp != NULL, ("gv_new_raid5_packet: NULL wp"));
	wp->state = SETUP;
	wp->type = JUNK;
	TAILQ_INIT(&wp->bits);

	return (wp);
}

void
gv_free_raid5_packet(struct gv_raid5_packet *wp)
{
	struct gv_raid5_bit *r, *r2;

	/* Remove all the bits from this work packet. */
	TAILQ_FOREACH_SAFE(r, &wp->bits, list, r2) {
		TAILQ_REMOVE(&wp->bits, r, list);
		if (r->malloc)
			g_free(r->buf);
		if (r->bio != NULL)
			g_destroy_bio(r->bio);
		g_free(r);
	}

	if (wp->bufmalloc == 1)
		g_free(wp->buf);
	g_free(wp);
}

/*
 * Check if the stripe that the work packet wants is already being used by
 * some other work packet.
 */
int
gv_stripe_active(struct gv_raid5_packet *wp, struct gv_plex *sc)
{
	struct gv_raid5_packet *wpa;

	TAILQ_FOREACH(wpa, &sc->worklist, list) {
		if (wpa->lockbase == wp->lockbase) {
			if (wpa == wp)
				return (0);
			return (1);
		}
	}
	return (0);
}

/*
 * The "worker" thread that runs through the worklist and fires off the
 * "subrequests" needed to fulfill a RAID5 read or write request.
 */
void
gv_raid5_worker(void *arg)
{
	struct bio *bp;
	struct g_geom *gp;
	struct gv_plex *p;
	struct gv_raid5_packet *wp, *wpt;
	struct gv_raid5_bit *rbp, *rbpt;
	int error, restart;

	gp = arg;
	p = gp->softc;

	mtx_lock(&p->worklist_mtx);
	for (;;) {
		restart = 0;
		TAILQ_FOREACH_SAFE(wp, &p->worklist, list, wpt) {
			/* This request packet is already being processed. */
			if (wp->state == IO)
				continue;
			/* This request packet is ready for processing. */
			if (wp->state == VALID) {
				/* Couldn't get the lock, try again. */
				if ((wp->lockbase != -1) &&
				    gv_stripe_active(wp, p))
					continue;

				wp->state = IO;
				mtx_unlock(&p->worklist_mtx);
				TAILQ_FOREACH_SAFE(rbp, &wp->bits, list, rbpt)
					g_io_request(rbp->bio, rbp->consumer);
				mtx_lock(&p->worklist_mtx);
				continue;
			}
			if (wp->state == FINISH) {
				bp = wp->bio;
				bp->bio_completed += wp->length;
				/*
				 * Deliver the original request if we have
				 * finished.
				 */
				if (bp->bio_completed == bp->bio_length) {
					mtx_unlock(&p->worklist_mtx);
					g_io_deliver(bp, 0);
					mtx_lock(&p->worklist_mtx);
				}
				TAILQ_REMOVE(&p->worklist, wp, list);
				gv_free_raid5_packet(wp);
				restart++;
				/*break;*/
			}
		}
		if (!restart) {
			/* Self-destruct. */
			if (p->flags & GV_PLEX_THREAD_DIE)
				break;
			error = msleep(p, &p->worklist_mtx, PRIBIO, "-",
			    hz/100);
		}
	}
	mtx_unlock(&p->worklist_mtx);

	g_trace(G_T_TOPOLOGY, "gv_raid5_worker die");

	/* Signal our plex that we are dead. */
	p->flags |= GV_PLEX_THREAD_DEAD;
	wakeup(p);
	kthread_exit(0);
}

/* Final bio transaction to write out the parity data. */
int
gv_raid5_parity(struct gv_raid5_packet *wp)
{
	struct bio *bp;

	bp = g_new_bio();
	if (bp == NULL)
		return (ENOMEM);

	wp->type = ISPARITY;
	bp->bio_cmd = BIO_WRITE;
	bp->bio_data = wp->buf;
	bp->bio_offset = wp->offset;
	bp->bio_length = wp->length;
	bp->bio_done = gv_raid5_done;
	bp->bio_caller1 = wp;
	bp->bio_caller2 = NULL;
	g_io_request(bp, wp->parity);

	return (0);
}

/* We end up here after each subrequest. */
void
gv_raid5_done(struct bio *bp)
{
	struct bio *obp;
	struct g_geom *gp;
	struct gv_plex *p;
	struct gv_raid5_packet *wp;
	struct gv_raid5_bit *rbp;
	off_t i;
	int error;

	wp = bp->bio_caller1;
	rbp = bp->bio_caller2;
	obp = wp->bio;
	gp = bp->bio_from->geom;
	p = gp->softc;

	/* One less active subrequest. */
	wp->active--;

	switch (obp->bio_cmd) {
	case BIO_READ:
		/* Degraded reads need to handle parity data. */
		if (wp->type == DEGRADED) {
			for (i = 0; i < wp->length; i++)
				wp->buf[i] ^= bp->bio_data[i];

			/* When we're finished copy back the data we want. */
			if (wp->active == 0)
				bcopy(wp->buf, wp->data, wp->length);
		}

		break;

	case BIO_WRITE:
		/* Handle the parity data, if needed. */
		if ((wp->type != NOPARITY) && (wp->type != ISPARITY)) {
			for (i = 0; i < wp->length; i++)
				wp->buf[i] ^= bp->bio_data[i];

			/* Write out the parity data we calculated. */
			if (wp->active == 0) {
				wp->active++;
				error = gv_raid5_parity(wp);
			}
		}
		break;
	}

	/* This request group is done. */
	if (wp->active == 0)
		wp->state = FINISH;
}

/* Build a request group to perform (part of) a RAID5 request. */
int
gv_build_raid5_req(struct gv_raid5_packet *wp, struct bio *bp, caddr_t addr,
    long bcount, off_t boff)
{
	struct g_geom *gp;
	struct gv_plex *p;
	struct gv_raid5_bit *rbp;
	struct gv_sd *broken, *original, *parity, *s;
	int i, psdno, sdno;
	off_t len_left, real_off, stripeend, stripeoff, stripestart;

	gp = bp->bio_to->geom;
	p = gp->softc;	

	if (p == NULL || LIST_EMPTY(&p->subdisks))
		return (ENXIO);

	/* We are optimistic and assume that this request will be OK. */
	wp->type = NORMAL;
	original = parity = broken = NULL;

	/* The number of the subdisk containing the parity stripe. */
	psdno = p->sdcount - 1 - ( boff / (p->stripesize * (p->sdcount - 1))) %
	    p->sdcount;
	KASSERT(psdno >= 0, ("gv_build_raid5_request: psdno < 0"));

	/* Offset of the start address from the start of the stripe. */
	stripeoff = boff % (p->stripesize * (p->sdcount - 1));
	KASSERT(stripeoff >= 0, ("gv_build_raid5_request: stripeoff < 0"));

	/* The number of the subdisk where the stripe resides. */
	sdno = stripeoff / p->stripesize;
	KASSERT(sdno >= 0, ("gv_build_raid5_request: sdno < 0"));

	/* At or past parity subdisk. */
	if (sdno >= psdno)
		sdno++;

	/* The offset of the stripe on this subdisk. */
	stripestart = (boff - stripeoff) / (p->sdcount - 1);
	KASSERT(stripestart >= 0, ("gv_build_raid5_request: stripestart < 0"));

	stripeoff %= p->stripesize;

	/* The offset of the request on this subdisk. */
	real_off = stripestart + stripeoff;

	stripeend = stripestart + p->stripesize;
	len_left = stripeend - real_off;
	KASSERT(len_left >= 0, ("gv_build_raid5_request: len_left < 0"));

	/* Find the right subdisks. */
	i = 0;
	LIST_FOREACH(s, &p->subdisks, in_plex) {
		if (i == sdno)
			original = s;
		if (i == psdno)
			parity = s;
		if (s->state != GV_SD_UP)
			broken = s;
		i++;
	}

	if ((original == NULL) || (parity == NULL))
		return (ENXIO);

	/* Our data stripe is missing. */
	if (original->state != GV_SD_UP)
		wp->type = DEGRADED;
	/* Our parity stripe is missing. */
	if (parity->state != GV_SD_UP) {
		/* We cannot take another failure if we're already degraded. */
		if (wp->type != NORMAL)
			return (ENXIO);
		else
			wp->type = NOPARITY;
	}

	/*
	 * A combined write is necessary when the original data subdisk and the
	 * parity subdisk are both up, but one of the other subdisks isn't.
	 */
	if ((broken != NULL) && (broken != parity) && (broken != original))
		wp->type = COMBINED;

	wp->offset = real_off;
	wp->length = (bcount <= len_left) ? bcount : len_left;
	wp->data = addr;
	wp->original = original->consumer;
	wp->parity = parity->consumer;
	wp->lockbase = stripestart;

	KASSERT(wp->length >= 0, ("gv_build_raid5_request: wp->length < 0"));

	switch (bp->bio_cmd) {
	case BIO_READ:
		/*
		 * For a degraded read we need to read in all stripes except
		 * the broken one plus the parity stripe and then recalculate
		 * the desired data.
		 */
		if (wp->type == DEGRADED) {
			wp->buf = g_malloc(wp->length, M_NOWAIT | M_ZERO);
			if (wp->buf == NULL)
				return (ENOMEM);
			wp->bufmalloc = 1;
			LIST_FOREACH(s, &p->subdisks, in_plex) {
				/* Skip the broken subdisk. */
				if (s == broken)
					continue;
				rbp = gv_new_raid5_bit();
				rbp->consumer = s->consumer;
				rbp->bio = g_new_bio();
				if (rbp->bio == NULL)
					return (ENOMEM);
				rbp->buf = g_malloc(wp->length,
					M_NOWAIT | M_ZERO);
				if (rbp->buf == NULL)
					return (ENOMEM);
				rbp->malloc = 1;
				rbp->bio->bio_cmd = BIO_READ;
				rbp->bio->bio_offset = wp->offset;
				rbp->bio->bio_length = wp->length;
				rbp->bio->bio_data = rbp->buf;
				rbp->bio->bio_done = gv_raid5_done;
				rbp->bio->bio_caller1 = wp;
				rbp->bio->bio_caller2 = rbp;
				TAILQ_INSERT_HEAD(&wp->bits, rbp, list);
				wp->active++;
				wp->rqcount++;
			}

		/* A normal read can be fulfilled with the original subdisk. */
		} else {
			rbp = gv_new_raid5_bit();
			rbp->consumer = wp->original;
			rbp->bio = g_new_bio();
			if (rbp->bio == NULL)
				return (ENOMEM);
			rbp->bio->bio_cmd = BIO_READ;
			rbp->bio->bio_offset = wp->offset;
			rbp->bio->bio_length = wp->length;
			rbp->buf = addr;
			rbp->bio->bio_data = rbp->buf;
			rbp->bio->bio_done = gv_raid5_done;
			rbp->bio->bio_caller1 = wp;
			rbp->bio->bio_caller2 = rbp;
			TAILQ_INSERT_HEAD(&wp->bits, rbp, list);
			wp->active++;
			wp->rqcount++;
		}
		if (wp->type != COMBINED)
			wp->lockbase = -1;
		break;

	case BIO_WRITE:
		/*
		 * A degraded write means we cannot write to the original data
		 * subdisk.  Thus we need to read in all valid stripes,
		 * recalculate the parity from the original data, and then
		 * write the parity stripe back out.
		 */
		if (wp->type == DEGRADED) {
			wp->buf = g_malloc(wp->length, M_NOWAIT | M_ZERO);
			if (wp->buf == NULL)
				return (ENOMEM);
			wp->bufmalloc = 1;

			/* Copy the original data. */
			bcopy(wp->data, wp->buf, wp->length);

			LIST_FOREACH(s, &p->subdisks, in_plex) {
				/* Skip the broken and the parity subdisk. */
				if ((s == broken) ||
				    (s->consumer == wp->parity))
					continue;

				rbp = gv_new_raid5_bit();
				rbp->consumer = s->consumer;
				rbp->bio = g_new_bio();
				if (rbp->bio == NULL)
					return (ENOMEM);
				rbp->buf = g_malloc(wp->length,
				    M_NOWAIT | M_ZERO);
				if (rbp->buf == NULL)
					return (ENOMEM);
				rbp->malloc = 1;
				rbp->bio->bio_cmd = BIO_READ;
				rbp->bio->bio_data = rbp->buf;
				rbp->bio->bio_offset = wp->offset;
				rbp->bio->bio_length = wp->length;
				rbp->bio->bio_done = gv_raid5_done;
				rbp->bio->bio_caller1 = wp;
				rbp->bio->bio_caller2 = rbp;
				TAILQ_INSERT_HEAD(&wp->bits, rbp, list);
				wp->active++;
				wp->rqcount++;
			}

		/*
		 * When we don't have the parity stripe we just write out the
		 * data.
		 */
		} else if (wp->type == NOPARITY) {
			rbp = gv_new_raid5_bit();
			rbp->consumer = wp->original;
			rbp->bio = g_new_bio();
			if (rbp->bio == NULL)
				return (ENOMEM);
			rbp->bio->bio_cmd = BIO_WRITE;
			rbp->bio->bio_offset = wp->offset;
			rbp->bio->bio_length = wp->length;
			rbp->bio->bio_data = addr;
			rbp->bio->bio_done = gv_raid5_done;
			rbp->bio->bio_caller1 = wp;
			rbp->bio->bio_caller2 = rbp;
			TAILQ_INSERT_HEAD(&wp->bits, rbp, list);
			wp->active++;
			wp->rqcount++;

		/*
		 * A combined write means that our data subdisk and the parity
		 * subdisks are both up, but another subdisk isn't.  We need to
		 * read all valid stripes including the parity to recalculate
		 * the data of the stripe that is missing.  Then we write our
		 * original data, and together with the other data stripes
		 * recalculate the parity again.
		 */
		} else if (wp->type == COMBINED) {
			wp->buf = g_malloc(wp->length, M_NOWAIT | M_ZERO);
			if (wp->buf == NULL)
				return (ENOMEM);
			wp->bufmalloc = 1;

			/* Get the data from all subdisks. */
			LIST_FOREACH(s, &p->subdisks, in_plex) {
				/* Skip the broken subdisk. */
				if (s == broken)
					continue;

				rbp = gv_new_raid5_bit();
				rbp->consumer = s->consumer;
				rbp->bio = g_new_bio();
				if (rbp->bio == NULL)
					return (ENOMEM);
				rbp->bio->bio_cmd = BIO_READ;
				rbp->buf = g_malloc(wp->length,
				    M_NOWAIT | M_ZERO);
				if (rbp->buf == NULL)
					return (ENOMEM);
				rbp->malloc = 1;
				rbp->bio->bio_data = rbp->buf;
				rbp->bio->bio_offset = wp->offset;
				rbp->bio->bio_length = wp->length;
				rbp->bio->bio_done = gv_raid5_done;
				rbp->bio->bio_caller1 = wp;
				rbp->bio->bio_caller2 = rbp;
				TAILQ_INSERT_HEAD(&wp->bits, rbp, list);
				wp->active++;
				wp->rqcount++;
			}

			/* Write the original data. */
			rbp = gv_new_raid5_bit();
			rbp->consumer = wp->original;
			rbp->buf = addr;
			rbp->bio = g_new_bio();
			if (rbp->bio == NULL)
				return (ENOMEM);
			rbp->bio->bio_cmd = BIO_WRITE;
			rbp->bio->bio_data = rbp->buf;
			rbp->bio->bio_offset = wp->offset;
			rbp->bio->bio_length = wp->length;
			rbp->bio->bio_done = gv_raid5_done;
			rbp->bio->bio_caller1 = wp;
			rbp->bio->bio_caller2 = rbp;
			/*
			 * Insert at the tail, because we want to read the old
			 * data first.
			 */
			TAILQ_INSERT_TAIL(&wp->bits, rbp, list);
			wp->active++;
			wp->rqcount++;

			/* Get the rest of the data again. */
			LIST_FOREACH(s, &p->subdisks, in_plex) {
				/*
				 * Skip the broken subdisk, the parity, and the
				 * one we just wrote.
				 */
				if ((s == broken) ||
				    (s->consumer == wp->parity) ||
				    (s->consumer == wp->original))
					continue;
				rbp = gv_new_raid5_bit();
				rbp->consumer = s->consumer;
				rbp->bio = g_new_bio();
				if (rbp->bio == NULL)
					return (ENOMEM);
				rbp->bio->bio_cmd = BIO_READ;
				rbp->buf = g_malloc(wp->length,
				    M_NOWAIT | M_ZERO);
				if (rbp->buf == NULL)
					return (ENOMEM);
				rbp->malloc = 1;
				rbp->bio->bio_data = rbp->buf;
				rbp->bio->bio_offset = wp->offset;
				rbp->bio->bio_length = wp->length;
				rbp->bio->bio_done = gv_raid5_done;
				rbp->bio->bio_caller1 = wp;
				rbp->bio->bio_caller2 = rbp;
				/*
				 * Again, insert at the tail to keep correct
				 * order.
				 */
				TAILQ_INSERT_TAIL(&wp->bits, rbp, list);
				wp->active++;
				wp->rqcount++;
			}
			

		/*
		 * A normal write request goes to the original subdisk, then we
		 * read in all other stripes, recalculate the parity and write
		 * out the parity again.
		 */
		} else {
			wp->buf = g_malloc(wp->length, M_NOWAIT | M_ZERO);
			if (wp->buf == NULL)
				return (ENOMEM);
			wp->bufmalloc = 1;
			LIST_FOREACH(s, &p->subdisks, in_plex) {
				/* Skip the parity stripe. */
				if (s->consumer == wp->parity)
					continue;

				rbp = gv_new_raid5_bit();
				rbp->consumer = s->consumer;
				rbp->bio = g_new_bio();
				if (rbp->bio == NULL)
					return (ENOMEM);
				/*
				 * The data for the original stripe is written,
				 * the others need to be read in for the parity
				 * calculation.
				 */
				if (s->consumer == wp->original) {
					rbp->bio->bio_cmd = BIO_WRITE;
					rbp->buf = addr;
				} else {
					rbp->bio->bio_cmd = BIO_READ;
					rbp->buf = g_malloc(wp->length,
					    M_NOWAIT | M_ZERO);
					if (rbp->buf == NULL)
						return (ENOMEM);
					rbp->malloc = 1;
				}
				rbp->bio->bio_data = rbp->buf;
				rbp->bio->bio_offset = wp->offset;
				rbp->bio->bio_length = wp->length;
				rbp->bio->bio_done = gv_raid5_done;
				rbp->bio->bio_caller1 = wp;
				rbp->bio->bio_caller2 = rbp;
				TAILQ_INSERT_HEAD(&wp->bits, rbp, list);
				wp->active++;
				wp->rqcount++;
			}
		}
		break;
	default:
		return (EINVAL);
	}

	wp->state = VALID;
	return (0);
}
